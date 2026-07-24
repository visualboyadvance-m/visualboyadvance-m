#include "wx/audio/internal/aaudio.h"

#if defined(__ANDROID__)

#include <aaudio/AAudio.h>
#include <android/log.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <vector>

#include "core/base/sound_driver.h"
#include "core/gba/gbaSound.h"

namespace audio {
namespace internal {

namespace {

// A native Android sound driver built on AAudio (NDK, API 26+). It runs a
// callback-driven output stream and feeds it from an interleaved-stereo ring
// buffer that the emulator thread fills via write(). The emulator is paced by
// blocking write() while the buffer is full at normal speed; at turbo/fast
// speeds it drops the oldest samples instead so emulation is never stalled.
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
    void FillBuffer(int16_t* out, int32_t num_frames);
    void CloseStream();

    AAudioStream* stream_ = nullptr;
    int channels_ = 2;
    long sample_rate_ = 44100;

    std::mutex mutex_;
    std::condition_variable space_cv_;
    std::vector<int16_t> ring_;  // interleaved stereo samples
    size_t head_ = 0;
    size_t tail_ = 0;
    size_t count_ = 0;      // queued samples
    size_t capacity_ = 0;   // ring capacity in samples
    size_t target_ = 0;     // pacing high-water mark in samples

    std::atomic<bool> initialized_{false};
    std::atomic<unsigned short> throttle_{100};
};

aaudio_data_callback_result_t AAudioDriver::DataCallback(AAudioStream* /*stream*/, void* user_data,
                                                         void* audio_data, int32_t num_frames) {
    static_cast<AAudioDriver*>(user_data)->FillBuffer(static_cast<int16_t*>(audio_data), num_frames);
    return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

void AAudioDriver::FillBuffer(int16_t* out, int32_t num_frames) {
    const size_t wanted = static_cast<size_t>(num_frames) * channels_;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const size_t avail = std::min(count_, wanted);
        for (size_t i = 0; i < avail; ++i) {
            out[i] = ring_[head_];
            head_ = (head_ + 1) % capacity_;
        }
        count_ -= avail;
        // Pad any shortfall with silence (underrun).
        if (avail < wanted) {
            std::memset(out + avail, 0, (wanted - avail) * sizeof(int16_t));
        }
    }
    space_cv_.notify_one();
}

bool AAudioDriver::init(long sampleRate) {
    CloseStream();

    sample_rate_ = sampleRate > 0 ? sampleRate : 44100;
    channels_ = 2;

    // Ring holds ~500ms; pace the emulator once ~80ms is buffered.
    capacity_ = static_cast<size_t>(sample_rate_) * channels_ / 2;
    target_ = static_cast<size_t>(sample_rate_) * channels_ * 80 / 1000;
    if (capacity_ < 8192) {
        capacity_ = 8192;
    }
    ring_.assign(capacity_, 0);
    head_ = tail_ = count_ = 0;

    AAudioStreamBuilder* builder = nullptr;
    if (AAudio_createStreamBuilder(&builder) != AAUDIO_OK || builder == nullptr) {
        __android_log_print(ANDROID_LOG_ERROR, "VBAM", "AAudio: createStreamBuilder failed");
        return false;
    }

    AAudioStreamBuilder_setDirection(builder, AAUDIO_DIRECTION_OUTPUT);
    AAudioStreamBuilder_setSampleRate(builder, static_cast<int32_t>(sample_rate_));
    AAudioStreamBuilder_setChannelCount(builder, channels_);
    AAudioStreamBuilder_setFormat(builder, AAUDIO_FORMAT_PCM_I16);
    AAudioStreamBuilder_setSharingMode(builder, AAUDIO_SHARING_MODE_SHARED);
    AAudioStreamBuilder_setPerformanceMode(builder, AAUDIO_PERFORMANCE_MODE_NONE);
    AAudioStreamBuilder_setDataCallback(builder, &AAudioDriver::DataCallback, this);

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
        AAudioStream_getChannelCount(stream_) != channels_) {
        __android_log_print(ANDROID_LOG_ERROR, "VBAM",
                            "AAudio: unexpected stream format/channels; falling back");
        CloseStream();
        return false;
    }

    // The framework resamples to the device rate, so a mismatch here is fine;
    // the callback is fed at our requested rate.
    initialized_ = true;
    AAudioStream_requestStart(stream_);
    return true;
}

void AAudioDriver::CloseStream() {
    initialized_ = false;
    space_cv_.notify_all();
    if (stream_ != nullptr) {
        AAudioStream_requestStop(stream_);
        AAudioStream_close(stream_);
        stream_ = nullptr;
    }
}

void AAudioDriver::write(uint16_t* finalWave, int length) {
    if (!initialized_ || stream_ == nullptr || finalWave == nullptr || length <= 0) {
        return;
    }

    const size_t samples = static_cast<size_t>(length) / sizeof(int16_t);
    const int16_t* in = reinterpret_cast<const int16_t*>(finalWave);
    const bool pace = throttle_.load() == 100;

    std::unique_lock<std::mutex> lock(mutex_);
    size_t i = 0;
    while (i < samples) {
        if (count_ == capacity_) {
            if (pace) {
                // At normal speed, block until the callback drains space so the
                // emulator is paced by audio instead of overrunning the buffer.
                space_cv_.wait_for(lock, std::chrono::milliseconds(10),
                                   [this] { return !initialized_ || count_ < capacity_; });
                if (!initialized_) {
                    return;
                }
                if (count_ == capacity_) {
                    continue;  // timed out still full: retry
                }
            } else {
                // Turbo/fast-forward: never stall emulation; drop the oldest.
                head_ = (head_ + 1) % capacity_;
                --count_;
            }
        }
        ring_[tail_] = in[i];
        tail_ = (tail_ + 1) % capacity_;
        ++count_;
        ++i;
    }

    // Keep latency bounded at normal speed: if we've buffered past the pacing
    // mark, wait for the callback to catch up.
    if (pace) {
        space_cv_.wait_for(lock, std::chrono::milliseconds(10),
                           [this] { return !initialized_ || count_ <= target_; });
    }
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
        AAudioStream_requestStart(stream_);
    }
}

void AAudioDriver::reset() {
    init(soundGetSampleRate());
}

}  // namespace

std::unique_ptr<SoundDriver> CreateAAudioDriver() {
    return std::make_unique<AAudioDriver>();
}

}  // namespace internal
}  // namespace audio

#endif  // defined(__ANDROID__)
