#include "wx/audio/internal/soundmanager.h"

#if defined(__WXRETRO68__)

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>

#include <wx/log.h>

#include <Events.h>
#include <Gestalt.h>
#include <MacTypes.h>
#include <Sound.h>

#include "core/base/sound_driver.h"
#include "core/gba/gbaSound.h"
#include "wx/audio/internal/stall-detector.h"

namespace audio {
namespace internal {

namespace {

// A native classic Mac OS sound driver on the Sound Manager (2.0 and later,
// so every System 7). It plays a double buffer -- SndPlayDoubleBuffer() -- on
// a sampled-sound channel, and the Sound Manager calls back at interrupt time
// for each buffer that has finished playing; the callback refills it from a
// ring that write() fills on the emulator's thread.
//
// There are no threads, only that interrupt, so the ring is a single-producer,
// single-consumer queue whose indices are aligned 32-bit words, each written by
// one side only: a plain load or store of one is atomic on 68k and PowerPC, and
// a uniprocessor sees its own stores in program order once the compiler is kept
// from reordering them (Barrier()). No read-modify-write is needed -- a flush is
// a request counter the producer bumps and the consumer acknowledges -- which
// also keeps libatomic out of the 68k link.
//
// The callback runs at interrupt time: it allocates nothing, makes no Toolbox
// call that can move memory, logs nothing and uses no floating point (the 68k
// Sound Manager does not save the FPU; this build is soft-float there anyway,
// but the PowerPC one is not).

// Multiversal Interfaces leave these out.
constexpr LONGINT kGestaltStereoCapability = 0;
constexpr LONGINT kGestalt16BitSoundIO = 7;
constexpr LONGINT kGestalt16BitAudioSupport = 12;
constexpr LONGINT kInitMono = 0x0080;
constexpr LONGINT kInitStereo = 0x00C0;

// The core always produces interleaved 16-bit stereo; the ring holds that and
// counts in samples (frames * 2).
constexpr uint32_t kCoreChannels = 2;

// Every Toolbox call in this file goes through Toolbox(). Retro68's PowerPC
// linker builds the import glue for this object (an @FIX0 csect) against a
// stale TOC anchor -- 0x4A2C below the real one -- so a direct call such as
// Gestalt() loaded a garbage transition vector and jumped into nowhere, which
// crashed the PowerPC build as soon as a ROM loaded and opened this driver.
// Taking the function's address is an ordinary TOC load, which the linker gets
// right, and a call through a pointer it cannot see through is made inline
// (load the transition vector, set r2, bctrl) instead of through that glue.
// On 68k there is no glue and the call stays direct.
template <typename F>
F* Toolbox(F* fn) {
#if defined(__m68k__)
    return fn;
#else
    F* volatile indirect = fn;
    return indirect;
#endif
}

inline void Barrier() {
    __asm__ __volatile__("" ::: "memory");
}

uint32_t RoundUpPow2(uint32_t value) {
    uint32_t result = 1;
    while (result < value) {
        result <<= 1;
    }
    return result;
}

class SoundManagerDriver final : public SoundDriver {
public:
    SoundManagerDriver() = default;
    ~SoundManagerDriver() override { Close(); }

    SoundManagerDriver(const SoundManagerDriver&) = delete;
    SoundManagerDriver& operator=(const SoundManagerDriver&) = delete;

    bool init(long sampleRate) override;
    void pause() override;
    void reset() override;
    void resume() override;
    void write(uint16_t* finalWave, int length) override;
    void setThrottle(unsigned short throttle) override;

    // Consumer side, interrupt time.
    void FillBuffer(SndDoubleBufferPtr buffer);

private:
    bool Open();
    void Close();

    // Producer side, emulator thread.
    void PushSamples(const int16_t* in, uint32_t samples, bool pace);

    uint32_t Queued() const {
        const uint32_t w = write_;
        const uint32_t r = read_;
        return w - r;
    }

    long rate_ = 44100;
    bool sixteen_bit_ = true;
    uint32_t out_channels_ = 2;
    uint32_t buffer_frames_ = 0;

    SndChannelPtr channel_ = nullptr;
    SndDoubleBackUPP double_back_ = nullptr;
    SndDoubleBufferHeader header_{};
    // LONGINT storage keeps the buffers long-aligned.
    std::unique_ptr<LONGINT[]> buffer_storage_[2];

    std::unique_ptr<int16_t[]> ring_;
    uint32_t mask_ = 0;
    volatile uint32_t read_ = 0;   // written by the consumer only, monotonic
    volatile uint32_t write_ = 0;  // written by the producer only, monotonic

    // Producer -> consumer request to drop everything queued: the consumer owns
    // read_, so the producer must never move it itself. Pending while the two
    // differ.
    volatile uint32_t flush_requested_ = 0;  // producer only
    volatile uint32_t flush_done_ = 0;       // consumer only

    // Consumer only: waiting for the queue to refill after running dry.
    volatile bool priming_ = true;
    volatile bool paused_ = false;      // producer only
    volatile uint32_t underruns_ = 0;   // consumer only

    uint32_t target_ = 0;  // pacing high-water mark, in samples
    uint32_t prime_ = 0;   // refill level after an underrun, in samples

    std::unique_ptr<int16_t[]> silence_;
    uint32_t silence_len_ = 0;
    StallDetector stall_;
    bool stall_pending_ = false;

    bool initialized_ = false;
    unsigned short throttle_ = 100;
};

// The Sound Manager's double-buffer callback. A free function, because GCC
// has no pascal member functions.
static pascal void VbamSndMgrDoubleBack(SndChannelPtr /*channel*/, SndDoubleBufferPtr buffer) {
#if defined(__m68k__)
    // Interrupt time: A5 is whatever the interrupted code had.
    const LONGINT old_a5 = SetA5(buffer->dbUserInfo[1]);
#endif
    reinterpret_cast<SoundManagerDriver*>(buffer->dbUserInfo[0])->FillBuffer(buffer);
#if defined(__m68k__)
    SetA5(old_a5);
#endif
}

void SoundManagerDriver::FillBuffer(SndDoubleBufferPtr buffer) {
    const uint32_t frames = buffer_frames_;
    const uint32_t wanted = frames * kCoreChannels;
    const uint32_t capacity = mask_ + 1;

    // Always hand back a full, ready buffer: one that is not marked ready stops
    // the double buffer for good.
    buffer->dbNumFrames = static_cast<LONGINT>(frames);
    buffer->dbFlags |= dbBufferReady;

    uint32_t r = read_;
    const uint32_t w = write_;
    Barrier();

    const uint32_t flush = flush_requested_;
    if (flush != flush_done_) {
        flush_done_ = flush;
        r = w;
        priming_ = true;
    }

    uint32_t avail = w - r;
    if (avail > capacity) {
        // The producer lapped us; resynchronize on the newest data.
        r = w;
        avail = 0;
    }

    uint32_t take = 0;
    if (!paused_) {
        // After running dry, stay silent until the queue has refilled to
        // prime_: draining a trickle one buffer at a time is what turns a single
        // glitch into continuous crackle.
        if (priming_ && avail >= prime_) {
            priming_ = false;
        }
        if (!priming_) {
            take = std::min(avail, wanted);
        }
    }

    const int16_t* const ring = ring_.get();
    const uint32_t mask = mask_;
    const uint32_t take_frames = take / kCoreChannels;
    uint32_t pos = r;

    if (sixteen_bit_ && out_channels_ == 2) {
        int16_t* out = reinterpret_cast<int16_t*>(buffer->dbSoundData);
        const uint32_t start = pos & mask;
        const uint32_t first = std::min(take, capacity - start);
        std::memcpy(out, ring + start, first * sizeof(int16_t));
        std::memcpy(out + first, ring, (take - first) * sizeof(int16_t));
        std::memset(out + take, 0, (wanted - take) * sizeof(int16_t));
    } else if (sixteen_bit_) {
        int16_t* out = reinterpret_cast<int16_t*>(buffer->dbSoundData);
        for (uint32_t f = 0; f < take_frames; ++f, pos += 2) {
            out[f] = static_cast<int16_t>(
                (static_cast<int32_t>(ring[pos & mask]) + ring[(pos + 1) & mask]) >> 1);
        }
        std::memset(out + take_frames, 0, (frames - take_frames) * sizeof(int16_t));
    } else {
        // 8-bit samples are unsigned, offset binary; silence is 0x80.
        uint8_t* out = buffer->dbSoundData;
        if (out_channels_ == 2) {
            for (uint32_t s = 0; s < take; ++s, ++pos) {
                out[s] = static_cast<uint8_t>((ring[pos & mask] >> 8) + 0x80);
            }
            std::memset(out + take, 0x80, wanted - take);
        } else {
            for (uint32_t f = 0; f < take_frames; ++f, pos += 2) {
                const int32_t mixed =
                    (static_cast<int32_t>(ring[pos & mask]) + ring[(pos + 1) & mask]) >> 1;
                out[f] = static_cast<uint8_t>((mixed >> 8) + 0x80);
            }
            std::memset(out + take_frames, 0x80, frames - take_frames);
        }
    }

    Barrier();
    read_ = r + take;

    if (!paused_ && !priming_ && take < wanted) {
        underruns_ = underruns_ + 1;
        priming_ = true;
    }
}

bool SoundManagerDriver::init(long sampleRate) {
    Close();
    rate_ = sampleRate > 0 ? sampleRate : 44100;
    return Open();
}

bool SoundManagerDriver::Open() {
    // The Sound Manager mixes stereo down itself where the hardware is mono,
    // except on the old 8-bit-only hardware, which gets mono 8-bit. 16-bit
    // samples are signed big-endian, native on 68k and PowerPC alike.
    LONGINT attrs = 0;
    bool stereo = false;
    sixteen_bit_ = false;
    if (Toolbox(&Gestalt)(gestaltSoundAttr, &attrs) == noErr) {
        sixteen_bit_ =
            (attrs & ((1L << kGestalt16BitSoundIO) | (1L << kGestalt16BitAudioSupport))) != 0;
        stereo = (attrs & (1L << kGestaltStereoCapability)) != 0;
    }
    out_channels_ = (stereo || sixteen_bit_) ? 2 : 1;

    // Unsigned 16.16 fixed point.
    if (rate_ > 65535) {
        wxLogDebug(wxT("Sound Manager: %ld Hz is above the Sound Manager's limit"), rate_);
        return false;
    }

    // Each half of the double buffer is a 40th of a second: few enough
    // interrupts for a 68k Mac, and 50 ms of latency in the pair.
    buffer_frames_ = std::max<uint32_t>((static_cast<uint32_t>(rate_) / 40 + 31) & ~31u, 256);
    const uint32_t buffer_samples = buffer_frames_ * kCoreChannels;
    const uint32_t samples_per_ms = static_cast<uint32_t>(rate_) * kCoreChannels / 1000;

    // A quarter second of slack. The pacing mark is the double buffer's worth
    // plus a margin, since the emulator's output comes in frame-sized bursts.
    const uint32_t capacity = RoundUpPow2(std::max(samples_per_ms * 250, buffer_samples * 8));
    ring_.reset(new int16_t[capacity]());
    mask_ = capacity - 1;
    read_ = 0;
    write_ = 0;
    flush_requested_ = 0;
    flush_done_ = 0;
    priming_ = true;
    paused_ = false;
    underruns_ = 0;

    target_ = std::min(buffer_samples * 2 + samples_per_ms * 30, capacity / 2);
    prime_ = buffer_samples;

    if (double_back_ == nullptr) {
#if defined(__m68k__)
        double_back_ = NewSndDoubleBackUPP(&VbamSndMgrDoubleBack);
#else
        // NewSndDoubleBackUPP(), spelled out to keep the call off the glue.
        double_back_ = reinterpret_cast<SndDoubleBackUPP>(Toolbox(&NewRoutineDescriptor)(
            reinterpret_cast<ProcPtr>(&VbamSndMgrDoubleBack), uppSndDoubleBackProcInfo,
            GetCurrentArchitecture()));
#endif
        if (double_back_ == nullptr) {
            wxLogDebug(wxT("Sound Manager: no memory for the double-buffer callback"));
            return false;
        }
    }

    const uint32_t bytes_per_frame = out_channels_ * (sixteen_bit_ ? 2 : 1);
    const size_t storage_len = (offsetof(SndDoubleBuffer, dbSoundData) +
                                buffer_frames_ * bytes_per_frame + sizeof(LONGINT) - 1) /
                               sizeof(LONGINT);
    for (std::unique_ptr<LONGINT[]>& storage : buffer_storage_) {
        storage.reset(new LONGINT[storage_len]());
        SndDoubleBufferPtr buffer = reinterpret_cast<SndDoubleBufferPtr>(storage.get());
        buffer->dbUserInfo[0] = reinterpret_cast<LONGINT>(this);
#if defined(__m68k__)
        buffer->dbUserInfo[1] = SetCurrentA5();
#endif
    }

    channel_ = nullptr;
    const OSErr err =
        Toolbox(&SndNewChannel)(&channel_, sampledSynth,
                                out_channels_ == 2 ? kInitStereo : kInitMono, nullptr);
    if (err != noErr) {
        wxLogDebug(wxT("Sound Manager: SndNewChannel failed (%d)"), static_cast<int>(err));
        channel_ = nullptr;
        return false;
    }

    header_ = SndDoubleBufferHeader{};
    header_.dbhNumChannels = static_cast<INTEGER>(out_channels_);
    header_.dbhSampleSize = static_cast<INTEGER>(sixteen_bit_ ? 16 : 8);
    header_.dbhCompressionID = 0;
    header_.dbhPacketSize = 0;
    header_.dbhSampleRate = static_cast<Fixed>(static_cast<unsigned long>(rate_) << 16);
    header_.dbhBufferPtr[0] = reinterpret_cast<SndDoubleBufferPtr>(buffer_storage_[0].get());
    header_.dbhBufferPtr[1] = reinterpret_cast<SndDoubleBufferPtr>(buffer_storage_[1].get());
    header_.dbhDoubleBack = double_back_;

    // Both buffers go in ready (silent, since the queue is still priming).
    FillBuffer(header_.dbhBufferPtr[0]);
    FillBuffer(header_.dbhBufferPtr[1]);

    initialized_ = true;
    const OSErr play = Toolbox(&SndPlayDoubleBuffer)(channel_, &header_);
    if (play != noErr) {
        wxLogDebug(wxT("Sound Manager: SndPlayDoubleBuffer failed (%d)"), static_cast<int>(play));
        Close();
        return false;
    }

    wxLogDebug(wxT("Sound Manager: %ld Hz, %u-bit %s, %u frames per buffer, target %u ms"), rate_,
               sixteen_bit_ ? 16u : 8u, out_channels_ == 2 ? wxT("stereo") : wxT("mono"),
               static_cast<unsigned>(buffer_frames_),
               static_cast<unsigned>(samples_per_ms ? target_ / samples_per_ms : 0));
    return true;
}

void SoundManagerDriver::Close() {
    initialized_ = false;
    if (channel_ != nullptr) {
        // quitNow: stops at once, and the callback is not called again once
        // this returns, so the buffers and the ring are ours.
        Toolbox(&SndDisposeChannel)(channel_, true);
        channel_ = nullptr;
    }
    buffer_storage_[0].reset();
    buffer_storage_[1].reset();
    if (double_back_ != nullptr) {
#if defined(__m68k__)
        DisposeSndDoubleBackUPP(double_back_);
#else
        // DisposeSndDoubleBackUPP(), spelled out to keep the call off the glue.
        Toolbox(&DisposeRoutineDescriptor)(reinterpret_cast<UniversalProcPtr>(double_back_));
#endif
        double_back_ = nullptr;
    }
}

void SoundManagerDriver::PushSamples(const int16_t* in, uint32_t samples, bool pace) {
    const uint32_t capacity = mask_ + 1;
    samples -= samples % kCoreChannels;

    uint32_t w = write_;
    uint32_t space = capacity - (w - read_);
    if (samples > space) {
        if (!pace) {
            // Turbo: never stall the emulator. Ask the consumer to drop what it
            // has, so the audio that does play stays close to the present.
            flush_requested_ = flush_requested_ + 1;
            return;
        }
        // Paced writes already waited; drop the tail.
        samples = space - space % kCoreChannels;
    }

    const uint32_t start = w & mask_;
    const uint32_t first = std::min(samples, capacity - start);
    std::memcpy(&ring_[start], in, first * sizeof(int16_t));
    std::memcpy(&ring_[0], in + first, (samples - first) * sizeof(int16_t));
    w += samples;

    Barrier();
    write_ = w;
}

void SoundManagerDriver::write(uint16_t* finalWave, int length) {
    if (!initialized_ || finalWave == nullptr || length <= 0) {
        return;
    }

    const bool pace = throttle_ == 100;
    if (pace) {
        // Pace the emulator on the queue depth. With no threads this is a spin,
        // and the interrupt that drains the queue is what ends it; the deadline
        // (four ticks, 67 ms) keeps a stopped channel from wedging emulation.
        const unsigned long deadline = Toolbox(&TickCount)() + 4;
        while (Queued() > target_ && Toolbox(&TickCount)() < deadline) {
        }
    }

    // After the emulator has not run for a while -- a menu held open, a modal
    // dialog, another application in front -- the queue has played dry, and
    // refilling it from the game would run the game fast until it is past
    // target_ again. The gap is in the sound already, so fill it with silence
    // instead. Not while a flush is pending: the consumer would drop the
    // silence with the rest. See StallDetector.
    stall_pending_ = pace && (stall_.Stalled() || stall_pending_);
    if (stall_pending_ && flush_requested_ == flush_done_) {
        stall_pending_ = false;
        if (Queued() == 0) {
            if (silence_len_ < target_) {
                silence_.reset(new int16_t[target_]());
                silence_len_ = target_;
            }
            PushSamples(silence_.get(), target_, pace);
        }
    }

    PushSamples(reinterpret_cast<const int16_t*>(finalWave),
                static_cast<uint32_t>(length) / sizeof(int16_t), pace);
}

void SoundManagerDriver::setThrottle(unsigned short throttle) {
    throttle_ = throttle;
}

void SoundManagerDriver::pause() {
    // The double buffer keeps running and the callback plays silence without
    // consuming: a double buffer the Sound Manager has stopped cannot be
    // restarted on the same channel, and pauseCmd on one is unreliable before
    // Sound Manager 3.
    paused_ = true;
}

void SoundManagerDriver::resume() {
    // Drop whatever was queued when we paused: it is stale by now.
    flush_requested_ = flush_requested_ + 1;
    paused_ = false;
}

void SoundManagerDriver::reset() {
    const long rate = soundGetSampleRate();
    if (!initialized_ || rate != rate_) {
        init(rate);
        return;
    }
    flush_requested_ = flush_requested_ + 1;
}

}  // namespace

std::unique_ptr<SoundDriver> CreateSoundManagerDriver() {
    return std::make_unique<SoundManagerDriver>();
}

}  // namespace internal
}  // namespace audio

#endif  // defined(__WXRETRO68__)
