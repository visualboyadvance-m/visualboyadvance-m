#include "wx/audio/internal/aaudio.h"

#if defined(VBAM_ENABLE_AAUDIO)

#include <aaudio/AAudio.h>
#include <android/log.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <thread>
#include <vector>

#include "core/base/sound_driver.h"
#include "core/gba/gbaSound.h"

namespace audio {
namespace internal {

namespace {

constexpr int kChannels = 2;

// Interleaved stereo, so everything below counts in samples (frames * 2).
size_t RoundUpPow2(size_t value) {
    size_t result = 1;
    while (result < value) {
        result <<= 1;
    }
    return result;
}

// A native Android sound driver built on AAudio (NDK, API 26+). It runs a
// callback-driven output stream fed from a ring buffer that the emulator thread
// fills via write().
//
// The ring is a lock-free single-producer/single-consumer queue: the emulator
// thread is the only writer, the AAudio callback the only reader. That is not
// incidental -- an AAudio data callback runs on a real-time thread with a hard
// deadline, and taking a lock it shares with the emulator thread (which the
// scheduler can and does preempt) inverts priorities and drops frames. For the
// same reason the callback never allocates, never signals a condition variable
// and never calls back into AAudio.
//
// The emulator is paced by the queue depth at normal speed; at turbo it is never
// stalled and the queue is dropped instead.
class AAudioDriver final : public SoundDriver {
public:
    AAudioDriver() = default;
    ~AAudioDriver() override { CloseStream(); }

    AAudioDriver(const AAudioDriver&) = delete;
    AAudioDriver& operator=(const AAudioDriver&) = delete;

    bool init(long sampleRate) override;
    void pause() override;
    void reset() override;
    void resume() override;
    void write(uint16_t* finalWave, int length) override;
    void setThrottle(unsigned short throttle) override;

private:
    static aaudio_data_callback_result_t DataCallback(AAudioStream* stream, void* user_data,
                                                      void* audio_data, int32_t num_frames);
    static void ErrorCallback(AAudioStream* stream, void* user_data, aaudio_result_t error);

    // Consumer side, real-time thread.
    void FillBuffer(int16_t* out, int32_t num_frames);

    bool OpenStream();
    void CloseStream();

    // Producer side, emulator thread.
    void PushSamples(const int16_t* in, size_t samples, bool pace);
    void AdaptToXRuns();
    void ReopenIfDisconnected();
    void LogHealth();

    size_t Queued() const {
        return write_.load(std::memory_order_relaxed) - read_.load(std::memory_order_acquire);
    }

    AAudioStream* stream_ = nullptr;
    // Rate the emulator produces at, and the rate the stream consumes at. They
    // are normally the same; see the resampler below for when they are not.
    long core_rate_ = 44100;
    long stream_rate_ = 44100;
    int32_t burst_ = 0;

    std::vector<int16_t> ring_;
    size_t mask_ = 0;
    std::atomic<size_t> read_{0};   // consumer-owned, monotonic
    std::atomic<size_t> write_{0};  // producer-owned, monotonic

    // Producer -> consumer request to drop everything queued. The consumer owns
    // read_, so the producer must never move it itself.
    std::atomic<bool> flush_{false};
    // Consumer is waiting for the queue to refill after running dry.
    std::atomic<bool> priming_{true};

    size_t target_ = 0;  // pacing high-water mark, in samples
    size_t prime_ = 0;   // refill level after an underrun, in samples
    size_t target_max_ = 0;

    // Producer-side resampler, used only when AAudio would not give us a stream
    // at the core's rate. Resampling here rather than in the callback keeps the
    // real-time path a plain memcpy.
    double resample_ratio_ = 1.0;  // input frames per output frame
    double resample_pos_ = 0.0;
    int16_t prev_frame_[kChannels] = {0, 0};
    bool have_prev_frame_ = false;

    std::atomic<bool> initialized_{false};
    std::atomic<bool> disconnected_{false};
    std::atomic<unsigned short> throttle_{100};

    // Health counters. underruns_ is written by the callback, the rest is
    // producer-only.
    std::atomic<uint32_t> underruns_{0};
    uint32_t logged_underruns_ = 0;
    int32_t last_xruns_ = 0;
    std::chrono::steady_clock::time_point last_log_;
};

aaudio_data_callback_result_t AAudioDriver::DataCallback(AAudioStream* /*stream*/, void* user_data,
                                                         void* audio_data, int32_t num_frames) {
    static_cast<AAudioDriver*>(user_data)->FillBuffer(static_cast<int16_t*>(audio_data), num_frames);
    return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

void AAudioDriver::ErrorCallback(AAudioStream* /*stream*/, void* user_data,
                                 aaudio_result_t error) {
    // Called when the stream is torn down under us, typically because the output
    // device changed (headphones, Bluetooth). The stream must not be closed from
    // its own callback, so just flag it and let write() reopen.
    AAudioDriver* const self = static_cast<AAudioDriver*>(user_data);
    if (error == AAUDIO_ERROR_DISCONNECTED) {
        self->disconnected_.store(true, std::memory_order_relaxed);
    }
}

void AAudioDriver::FillBuffer(int16_t* out, int32_t num_frames) {
    const size_t wanted = static_cast<size_t>(num_frames) * kChannels;
    const size_t capacity = mask_ + 1;

    size_t r = read_.load(std::memory_order_relaxed);
    const size_t w = write_.load(std::memory_order_acquire);

    if (flush_.exchange(false, std::memory_order_acq_rel)) {
        r = w;
        priming_.store(true, std::memory_order_relaxed);
    }

    size_t avail = w - r;
    if (avail > capacity) {
        // Producer lapped us (only possible if it ignored the queue depth).
        // Resynchronize on the newest data rather than play garbage.
        r = w;
        avail = 0;
    }

    // After running dry, stay silent until the queue has refilled to prime_.
    // Draining a trickle one callback at a time is what turns a single glitch
    // into continuous crackle.
    if (priming_.load(std::memory_order_relaxed)) {
        if (avail < prime_) {
            std::memset(out, 0, wanted * sizeof(int16_t));
            read_.store(r, std::memory_order_release);
            return;
        }
        priming_.store(false, std::memory_order_relaxed);
    }

    const size_t take = std::min(avail, wanted);
    const size_t start = r & mask_;
    const size_t first = std::min(take, capacity - start);
    std::memcpy(out, &ring_[start], first * sizeof(int16_t));
    if (take > first) {
        std::memcpy(out + first, &ring_[0], (take - first) * sizeof(int16_t));
    }
    read_.store(r + take, std::memory_order_release);

    if (take < wanted) {
        std::memset(out + take, 0, (wanted - take) * sizeof(int16_t));
        underruns_.fetch_add(1, std::memory_order_relaxed);
        priming_.store(true, std::memory_order_relaxed);
    }
}

bool AAudioDriver::init(long sampleRate) {
    CloseStream();
    core_rate_ = sampleRate > 0 ? sampleRate : 44100;
    return OpenStream();
}

bool AAudioDriver::OpenStream() {
    AAudioStreamBuilder* builder = nullptr;
    if (AAudio_createStreamBuilder(&builder) != AAUDIO_OK || builder == nullptr) {
        __android_log_print(ANDROID_LOG_ERROR, "VBAM", "AAudio: createStreamBuilder failed");
        return false;
    }

    AAudioStreamBuilder_setDirection(builder, AAUDIO_DIRECTION_OUTPUT);
    AAudioStreamBuilder_setSampleRate(builder, static_cast<int32_t>(core_rate_));
    AAudioStreamBuilder_setChannelCount(builder, kChannels);
    AAudioStreamBuilder_setFormat(builder, AAUDIO_FORMAT_PCM_I16);
    AAudioStreamBuilder_setSharingMode(builder, AAUDIO_SHARING_MODE_SHARED);
    // The low-latency path is the one served by the FAST mixer thread. It is not
    // about latency here so much as scheduling: the normal path's mixer runs on
    // a longer, less predictable period, which shows up as audible unevenness
    // even when the queue never runs dry.
    AAudioStreamBuilder_setPerformanceMode(builder, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
#if __ANDROID_API__ >= 28
    AAudioStreamBuilder_setUsage(builder, AAUDIO_USAGE_GAME);
#endif
    AAudioStreamBuilder_setDataCallback(builder, &AAudioDriver::DataCallback, this);
    AAudioStreamBuilder_setErrorCallback(builder, &AAudioDriver::ErrorCallback, this);

    const aaudio_result_t result = AAudioStreamBuilder_openStream(builder, &stream_);
    AAudioStreamBuilder_delete(builder);

    if (result != AAUDIO_OK || stream_ == nullptr) {
        __android_log_print(ANDROID_LOG_ERROR, "VBAM", "AAudio: openStream failed: %s",
                            AAudio_convertResultToText(result));
        stream_ = nullptr;
        return false;
    }

    // The device must actually deliver the format we assume in the callback.
    if (AAudioStream_getFormat(stream_) != AAUDIO_FORMAT_PCM_I16 ||
        AAudioStream_getChannelCount(stream_) != kChannels) {
        __android_log_print(ANDROID_LOG_ERROR, "VBAM",
                            "AAudio: unexpected stream format/channels; falling back");
        CloseStream();
        return false;
    }

    // The rate we asked for is not necessarily the rate we got: an MMAP-backed
    // stream runs at the device rate and does no conversion. Feeding it at the
    // core's rate then drains the queue faster than it fills, which is heard as
    // a permanent stutter rather than as the pitch error it also is. Resample in
    // write() when the rates differ.
    stream_rate_ = AAudioStream_getSampleRate(stream_);
    if (stream_rate_ <= 0) {
        stream_rate_ = core_rate_;
    }
    resample_ratio_ = static_cast<double>(core_rate_) / static_cast<double>(stream_rate_);
    resample_pos_ = 0.0;
    have_prev_frame_ = false;

    burst_ = AAudioStream_getFramesPerBurst(stream_);
    if (burst_ <= 0) {
        burst_ = 192;
    }

    // Queue: half a second of slack, with the pacing mark far enough above the
    // callback period that a late frame does not empty it. The emulator runs on
    // the thread that also serves the UI, so its output arrives in bursts.
    const size_t samples_per_ms = static_cast<size_t>(stream_rate_) * kChannels / 1000;
    const size_t burst_samples = static_cast<size_t>(burst_) * kChannels;
    const size_t capacity = RoundUpPow2(std::max<size_t>(samples_per_ms * 500, 8192));
    ring_.assign(capacity, 0);
    mask_ = capacity - 1;
    read_.store(0, std::memory_order_relaxed);
    write_.store(0, std::memory_order_relaxed);
    flush_.store(false, std::memory_order_relaxed);
    priming_.store(true, std::memory_order_relaxed);

    target_ = std::max(samples_per_ms * 80, burst_samples * 4);
    prime_ = std::max(samples_per_ms * 30, burst_samples * 2);
    target_max_ = std::min(samples_per_ms * 200, capacity / 2);
    if (target_ > target_max_) {
        target_ = target_max_;
    }

    // Start at two bursts and let AdaptToXRuns() grow it if the device turns
    // out to need more.
    AAudioStream_setBufferSizeInFrames(stream_, burst_ * 2);

    last_xruns_ = 0;
    underruns_.store(0, std::memory_order_relaxed);
    logged_underruns_ = 0;
    last_log_ = std::chrono::steady_clock::now();
    disconnected_.store(false, std::memory_order_relaxed);
    initialized_ = true;

    const aaudio_result_t start = AAudioStream_requestStart(stream_);
    if (start != AAUDIO_OK) {
        __android_log_print(ANDROID_LOG_ERROR, "VBAM", "AAudio: requestStart failed: %s",
                            AAudio_convertResultToText(start));
        CloseStream();
        return false;
    }

    __android_log_print(ANDROID_LOG_INFO, "VBAM",
                        "AAudio: core %ld Hz, stream %ld Hz%s, burst %d, buffer %d/%d frames, "
                        "queue target %zu ms",
                        core_rate_, stream_rate_,
                        resample_ratio_ != 1.0 ? " (resampling)" : "", burst_,
                        AAudioStream_getBufferSizeInFrames(stream_),
                        AAudioStream_getBufferCapacityInFrames(stream_),
                        samples_per_ms > 0 ? target_ / samples_per_ms : 0);
    return true;
}

void AAudioDriver::CloseStream() {
    initialized_ = false;
    if (stream_ != nullptr) {
        // Returns once the data callback has stopped, so the ring is ours again.
        AAudioStream_requestStop(stream_);
        AAudioStream_close(stream_);
        stream_ = nullptr;
    }
}

void AAudioDriver::PushSamples(const int16_t* in, size_t samples, bool pace) {
    const size_t capacity = mask_ + 1;
    const size_t frames = samples / kChannels;
    if (frames == 0) {
        return;
    }

    size_t w = write_.load(std::memory_order_relaxed);

    const auto space = [&]() -> size_t {
        return capacity - (w - read_.load(std::memory_order_acquire));
    };

    const auto push_frame = [&](const int16_t* frame) {
        for (int c = 0; c < kChannels; ++c) {
            ring_[(w + c) & mask_] = frame[c];
        }
        w += kChannels;
    };

    if (resample_ratio_ == 1.0) {
        for (size_t f = 0; f < frames; ++f) {
            if (space() < kChannels) {
                if (pace) {
                    break;  // paced writes already waited; drop the tail
                }
                // Turbo: never stall the emulator. Ask the consumer to drop what
                // it has so the audio that does play stays close to the present.
                flush_.store(true, std::memory_order_release);
                write_.store(w, std::memory_order_release);
                return;
            }
            push_frame(in + f * kChannels);
        }
        write_.store(w, std::memory_order_release);
        return;
    }

    // Linear resampling from the core's rate to the stream's rate.
    //
    // The input is treated as the frame carried over from the previous call
    // followed by this call's frames, so positions run 0..count-1 over that
    // virtual buffer. resample_pos_ carries the leftover fractional position
    // between calls, measured from the carried frame, which is what lets
    // successive writes join without a discontinuity at the seam.
    const bool had_prev = have_prev_frame_;
    const size_t count = frames + (had_prev ? 1 : 0);
    const auto virtual_frame = [&](size_t index) -> const int16_t* {
        if (had_prev) {
            return index == 0 ? prev_frame_ : in + (index - 1) * kChannels;
        }
        return in + index * kChannels;
    };

    double pos = resample_pos_;
    while (static_cast<size_t>(pos) + 1 < count) {
        if (space() < kChannels) {
            if (pace) {
                break;  // paced writes already waited; drop the tail
            }
            // Turbo: never stall the emulator. Ask the consumer to drop what it
            // has so the audio that does play stays close to the present.
            flush_.store(true, std::memory_order_release);
            break;
        }

        const size_t base = static_cast<size_t>(pos);
        const double frac = pos - static_cast<double>(base);
        const int16_t* const a = virtual_frame(base);
        const int16_t* const b = virtual_frame(base + 1);
        int16_t frame[kChannels];
        for (int c = 0; c < kChannels; ++c) {
            frame[c] = static_cast<int16_t>(a[c] + (b[c] - a[c]) * frac);
        }
        push_frame(frame);
        pos += resample_ratio_;
    }

    // Carry the last input frame and the leftover fraction, now measured from
    // that frame (virtual index count - 1).
    std::memcpy(prev_frame_, in + (frames - 1) * kChannels, sizeof(prev_frame_));
    have_prev_frame_ = true;
    resample_pos_ = pos - static_cast<double>(count - 1);
    if (resample_pos_ < 0.0) {
        resample_pos_ = 0.0;
    }
    write_.store(w, std::memory_order_release);
}

void AAudioDriver::AdaptToXRuns() {
    if (stream_ == nullptr) {
        return;
    }
    const int32_t xruns = AAudioStream_getXRunCount(stream_);
    if (xruns <= last_xruns_) {
        return;
    }
    last_xruns_ = xruns;

    // The device could not keep up with the buffer size we chose. Give it one
    // more burst, which is the adaptation AAudio expects callers to do, and
    // carry a little more in our own queue as well.
    const int32_t capacity = AAudioStream_getBufferCapacityInFrames(stream_);
    const int32_t size = AAudioStream_getBufferSizeInFrames(stream_);
    if (size > 0 && capacity > 0 && size + burst_ <= capacity) {
        AAudioStream_setBufferSizeInFrames(stream_, size + burst_);
    }
    const size_t samples_per_ms = static_cast<size_t>(stream_rate_) * kChannels / 1000;
    if (target_ + samples_per_ms * 10 <= target_max_) {
        target_ += samples_per_ms * 10;
    }
}

void AAudioDriver::ReopenIfDisconnected() {
    if (!disconnected_.load(std::memory_order_relaxed)) {
        return;
    }
    __android_log_print(ANDROID_LOG_INFO, "VBAM", "AAudio: stream disconnected, reopening");
    CloseStream();
    OpenStream();
}

void AAudioDriver::LogHealth() {
    const auto now = std::chrono::steady_clock::now();
    if (now - last_log_ < std::chrono::seconds(10)) {
        return;
    }
    last_log_ = now;
    const uint32_t underruns = underruns_.load(std::memory_order_relaxed);
    if (underruns == logged_underruns_) {
        return;
    }
    const size_t samples_per_ms = static_cast<size_t>(stream_rate_) * kChannels / 1000;
    __android_log_print(ANDROID_LOG_WARN, "VBAM",
                        "AAudio: %u queue underruns (+%u), %d device xruns, queued %zu ms, "
                        "target %zu ms",
                        underruns, underruns - logged_underruns_, last_xruns_,
                        samples_per_ms > 0 ? Queued() / samples_per_ms : 0,
                        samples_per_ms > 0 ? target_ / samples_per_ms : 0);
    logged_underruns_ = underruns;
}

void AAudioDriver::write(uint16_t* finalWave, int length) {
    if (!initialized_ || finalWave == nullptr || length <= 0) {
        return;
    }
    ReopenIfDisconnected();
    if (stream_ == nullptr) {
        return;
    }

    const bool pace = throttle_.load(std::memory_order_relaxed) == 100;
    if (pace) {
        // Pace the emulator on the queue depth, in short sleeps. This runs on the
        // thread that also drives the frame loop and the UI, so a long block here
        // is both video judder and -- because it makes this producer bursty --
        // a cause of the very underruns it is meant to prevent. The deadline
        // keeps a stalled or stopped stream from wedging emulation.
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(50);
        while (Queued() > target_) {
            if (std::chrono::steady_clock::now() >= deadline) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::microseconds(500));
        }
    }

    PushSamples(reinterpret_cast<const int16_t*>(finalWave),
                static_cast<size_t>(length) / sizeof(int16_t), pace);
    AdaptToXRuns();
    LogHealth();
}

void AAudioDriver::setThrottle(unsigned short throttle) {
    throttle_ = throttle;
}

void AAudioDriver::pause() {
    if (initialized_ && stream_ != nullptr) {
        AAudioStream_requestPause(stream_);
    }
}

void AAudioDriver::resume() {
    if (initialized_ && stream_ != nullptr) {
        // Drop whatever was queued when we paused: it is stale by now, and
        // playing it back before the fresh audio is an audible hiccup.
        flush_.store(true, std::memory_order_release);
        AAudioStream_requestStart(stream_);
    }
}

void AAudioDriver::reset() {
    const long rate = soundGetSampleRate();
    if (!initialized_ || stream_ == nullptr || rate != core_rate_) {
        // Only a rate change needs a new stream; tearing one down and building
        // it back up takes tens of milliseconds and is heard as a dropout.
        init(rate);
        return;
    }
    flush_.store(true, std::memory_order_release);
}

}  // namespace

std::unique_ptr<SoundDriver> CreateAAudioDriver() {
    return std::make_unique<AAudioDriver>();
}

}  // namespace internal
}  // namespace audio

#endif  // defined(VBAM_ENABLE_AAUDIO)
