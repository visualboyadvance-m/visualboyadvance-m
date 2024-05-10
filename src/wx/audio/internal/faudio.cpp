#include <wx/string.h>
#if !defined(VBAM_ENABLE_FAUDIO)
#error "This file should only be compiled if FAudio is enabled"
#endif

#include "wx/audio/internal/faudio.h"

#include <condition_variable>
#include <mutex>
#include <vector>

// FAudio
#include <FAudio.h>

#include <wx/arrstr.h>
#include <wx/log.h>
#include <wx/translation.h>

#include "core/base/check.h"
#include "core/base/system.h"
#include "core/gba/gbaGlobals.h"
#include "wx/config/option-proxy.h"

namespace audio {
namespace internal {

namespace {

int FAGetDev(FAudio* fa) {
    const wxString& audio_device = OPTION(kSoundAudioDevice);
    if (audio_device.empty()) {
        // Just use the default device.
        return 0;
    }

    uint32_t hr;
    uint32_t dev_count = 0;
    hr = FAudio_GetDeviceCount(fa, &dev_count);
    if (hr != 0) {
        wxLogError(_("FAudio: Enumerating devices failed!"));
        return 0;
    }

    FAudioDeviceDetails dd;
    for (uint32_t i = 0; i < dev_count; i++) {
        hr = FAudio_GetDeviceDetails(fa, i, &dd);
        if (hr != 0) {
            continue;
        }
        const wxString device_id(reinterpret_cast<wchar_t*>(dd.DeviceID));
        if (audio_device == device_id) {
            return i;
        }
    }

    return 0;
}

class FAudio_BufferNotify : public FAudioVoiceCallback {
public:
    // Waits for the buffer end event to be signaled for 10 seconds.
    // Returns true if the buffer end event was signaled, false if the wait timed out.
    bool WaitForSignal() {
        std::unique_lock<std::mutex> lock(mutex_);
        waiting_for_buffer_end_ = true;
        const bool was_signaled =
            buffer_end_cv_.wait_for(lock, std::chrono::seconds(10), [this] { return signaled_; });
        waiting_for_buffer_end_ = false;
        signaled_ = false;
        return was_signaled;
    }

    FAudio_BufferNotify() {
        OnBufferEnd = &FAudio_BufferNotify::StaticOnBufferEnd;
        OnVoiceProcessingPassStart = &FAudio_BufferNotify::StaticOnVoiceProcessingPassStart;
        OnVoiceProcessingPassEnd = &FAudio_BufferNotify::StaticOnVoiceProcessingPassEnd;
        OnStreamEnd = &FAudio_BufferNotify::StaticOnStreamEnd;
        OnBufferStart = &FAudio_BufferNotify::StaticOnBufferStart;
        OnLoopEnd = &FAudio_BufferNotify::StaticOnLoopEnd;
        OnVoiceError = &FAudio_BufferNotify::StaticOnVoiceError;
    }
    ~FAudio_BufferNotify() = default;

private:
    // Signals that the buffer end event has occurred.
    void SignalBufferEnd() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!waiting_for_buffer_end_) {
            return;
        }
        signaled_ = true;
        buffer_end_cv_.notify_one();
    }

    // Protects `waiting_for_buffer_end_` and `signaled_`.
    std::mutex mutex_;
    // Used to wait for the buffer end event.
    std::condition_variable buffer_end_cv_;
    // Set to true when we are waiting for the buffer end event.
    // Must be protected by `mutex_`.
    bool waiting_for_buffer_end_ = false;
    // Set to true when the buffer end event has been signaled.
    // Must be protected by `mutex_`.
    bool signaled_ = false;

    static void StaticOnBufferEnd(FAudioVoiceCallback* callback, void*) {
        static_cast<FAudio_BufferNotify*>(callback)->SignalBufferEnd();
    }
    static void StaticOnVoiceProcessingPassStart(FAudioVoiceCallback*, uint32_t) {}
    static void StaticOnVoiceProcessingPassEnd(FAudioVoiceCallback*) {}
    static void StaticOnStreamEnd(FAudioVoiceCallback*) {}
    static void StaticOnBufferStart(FAudioVoiceCallback*, void*) {}
    static void StaticOnLoopEnd(FAudioVoiceCallback*, void*) {}
    static void StaticOnVoiceError(FAudioVoiceCallback*, void*, uint32_t) {}
};

class FAudio_Output : public SoundDriver {
public:
    FAudio_Output();
    ~FAudio_Output();

    void device_change();

private:
    void close();

    // SoundDriver implementation.
    bool init(long sampleRate) override;
    void pause() override;
    void reset() override;
    void resume() override;
    void write(uint16_t* finalWave, int length) override;
    void setThrottle(unsigned short throttle_) override;

    bool failed;
    bool initialized;
    bool playing;
    uint32_t freq_;
    const uint32_t buffer_count_;
    std::vector<uint8_t> buffers_;
    int currentBuffer;
    int sound_buffer_len_;

    volatile bool device_changed;

    FAudio* faud;
    FAudioMasteringVoice* mVoice;  // listener
    FAudioSourceVoice* sVoice;     // sound source
    FAudioBuffer buf;
    FAudioVoiceState vState;
    FAudio_BufferNotify notify;  // buffer end notification
};

// Class Implementation
FAudio_Output::FAudio_Output() : buffer_count_(OPTION(kSoundBuffers)) {
    failed = false;
    initialized = false;
    playing = false;
    freq_ = 0;
    currentBuffer = 0;
    device_changed = false;
    faud = nullptr;
    mVoice = nullptr;
    sVoice = nullptr;
    memset(&buf, 0, sizeof(buf));
    memset(&vState, 0, sizeof(vState));
}

FAudio_Output::~FAudio_Output() {
    close();
}

void FAudio_Output::close() {
    initialized = false;

    if (sVoice) {
        if (playing) {
            VBAM_CHECK(FAudioSourceVoice_Stop(sVoice, 0, FAUDIO_COMMIT_NOW) == 0);
        }

        FAudioVoice_DestroyVoice(sVoice);
        sVoice = nullptr;
    }

    if (mVoice) {
        FAudioVoice_DestroyVoice(mVoice);
        mVoice = nullptr;
    }

    if (faud) {
        FAudio_Release(faud);
        faud = nullptr;
    }
}

void FAudio_Output::device_change() {
    device_changed = true;
}

bool FAudio_Output::init(long sampleRate) {
    if (failed || initialized)
        return false;

    uint32_t hr;
    // Initialize FAudio
    uint32_t flags = 0;
    // #ifdef _DEBUG
    //	flags = FAUDIO_DEBUG_ENGINE;
    // #endif
    hr = FAudioCreate(&faud, flags, FAUDIO_DEFAULT_PROCESSOR);

    if (hr != 0) {
        wxLogError(_("The FAudio interface failed to initialize!"));
        failed = true;
        return false;
    }

    freq_ = sampleRate;
    // calculate the number of samples per frame first
    // then multiply it with the size of a sample frame (16 bit * stereo)
    sound_buffer_len_ = (freq_ / 60) * 4;
    // create own buffers to store sound data because it must not be
    // manipulated while the voice plays from it.
    // +1 because we need one temporary buffer when all others are in use.
    buffers_.resize((buffer_count_ + 1) * sound_buffer_len_);
    static const uint16_t kNumChannels = 2;
    static const uint16_t kBitsPerSample = 16;
    static const uint16_t kBlockAlign = kNumChannels * (kBitsPerSample / 8);
    FAudioWaveFormatEx wfx{
        /*.wFormatTag=*/FAUDIO_FORMAT_PCM,
        /*.nChannels=*/kNumChannels,
        /*.nSamplesPerSec=*/freq_,
        /*.nAvgBytesPerSec=*/freq_ * kBlockAlign,
        /*.nBlockAlign=*/kNumChannels * (kBitsPerSample / 8),
        /*.wBitsPerSample=*/kBitsPerSample,
        /*.cbSize=*/0,
    };

    // create sound receiver
    hr = FAudio_CreateMasteringVoice(faud, &mVoice, FAUDIO_DEFAULT_CHANNELS,
                                     FAUDIO_DEFAULT_SAMPLERATE, 0, FAGetDev(faud), nullptr);

    if (hr != 0) {
        wxLogError(_("FAudio: Creating mastering voice failed!"));
        failed = true;
        return false;
    }

    // create sound emitter
    hr = FAudio_CreateSourceVoice(faud, &sVoice, &wfx, 0, 4.0f, &notify, nullptr, nullptr);

    if (hr != 0) {
        wxLogError(_("FAudio: Creating source voice failed!"));
        failed = true;
        return false;
    }

    if (OPTION(kSoundUpmix)) {
        // set up stereo upmixing
        FAudioDeviceDetails dd{};
        VBAM_CHECK(FAudio_GetDeviceDetails(faud, 0, &dd) == 0);
        std::vector<float> matrix(sizeof(float) * 2 * dd.OutputFormat.Format.nChannels);

        bool matrixAvailable = true;

        switch (dd.OutputFormat.Format.nChannels) {
            case 4:  // 4.0
                     // Speaker \ Left Source           Right Source
                /*Front L*/ matrix[0] = 1.0000f;
                matrix[1] = 0.0000f;
                /*Front R*/ matrix[2] = 0.0000f;
                matrix[3] = 1.0000f;
                /*Back  L*/ matrix[4] = 1.0000f;
                matrix[5] = 0.0000f;
                /*Back  R*/ matrix[6] = 0.0000f;
                matrix[7] = 1.0000f;
                break;

            case 5:  // 5.0
                     // Speaker \ Left Source           Right Source
                /*Front L*/ matrix[0] = 1.0000f;
                matrix[1] = 0.0000f;
                /*Front R*/ matrix[2] = 0.0000f;
                matrix[3] = 1.0000f;
                /*Front C*/ matrix[4] = 0.7071f;
                matrix[5] = 0.7071f;
                /*Side  L*/ matrix[6] = 1.0000f;
                matrix[7] = 0.0000f;
                /*Side  R*/ matrix[8] = 0.0000f;
                matrix[9] = 1.0000f;
                break;

            case 6:  // 5.1
                     // Speaker \ Left Source           Right Source
                /*Front L*/ matrix[0] = 1.0000f;
                matrix[1] = 0.0000f;
                /*Front R*/ matrix[2] = 0.0000f;
                matrix[3] = 1.0000f;
                /*Front C*/ matrix[4] = 0.7071f;
                matrix[5] = 0.7071f;
                /*LFE    */ matrix[6] = 0.0000f;
                matrix[7] = 0.0000f;
                /*Side  L*/ matrix[8] = 1.0000f;
                matrix[9] = 0.0000f;
                /*Side  R*/ matrix[10] = 0.0000f;
                matrix[11] = 1.0000f;
                break;

            case 7:  // 6.1
                     // Speaker \ Left Source           Right Source
                /*Front L*/ matrix[0] = 1.0000f;
                matrix[1] = 0.0000f;
                /*Front R*/ matrix[2] = 0.0000f;
                matrix[3] = 1.0000f;
                /*Front C*/ matrix[4] = 0.7071f;
                matrix[5] = 0.7071f;
                /*LFE    */ matrix[6] = 0.0000f;
                matrix[7] = 0.0000f;
                /*Side  L*/ matrix[8] = 1.0000f;
                matrix[9] = 0.0000f;
                /*Side  R*/ matrix[10] = 0.0000f;
                matrix[11] = 1.0000f;
                /*Back  C*/ matrix[12] = 0.7071f;
                matrix[13] = 0.7071f;
                break;

            case 8:  // 7.1
                     // Speaker \ Left Source           Right Source
                /*Front L*/ matrix[0] = 1.0000f;
                matrix[1] = 0.0000f;
                /*Front R*/ matrix[2] = 0.0000f;
                matrix[3] = 1.0000f;
                /*Front C*/ matrix[4] = 0.7071f;
                matrix[5] = 0.7071f;
                /*LFE    */ matrix[6] = 0.0000f;
                matrix[7] = 0.0000f;
                /*Back  L*/ matrix[8] = 1.0000f;
                matrix[9] = 0.0000f;
                /*Back  R*/ matrix[10] = 0.0000f;
                matrix[11] = 1.0000f;
                /*Side  L*/ matrix[12] = 1.0000f;
                matrix[13] = 0.0000f;
                /*Side  R*/ matrix[14] = 0.0000f;
                matrix[15] = 1.0000f;
                break;

            default:
                matrixAvailable = false;
                break;
        }

        if (matrixAvailable) {
            hr = FAudioVoice_SetOutputMatrix(sVoice, nullptr, 2, dd.OutputFormat.Format.nChannels,
                                             matrix.data(), FAUDIO_DEFAULT_CHANNELS);
            VBAM_CHECK(hr == 0);
        }
    }

    hr = FAudioSourceVoice_Start(sVoice, 0, FAUDIO_COMMIT_NOW);
    VBAM_CHECK(hr == 0);
    playing = true;
    currentBuffer = 0;
    device_changed = false;
    initialized = true;
    return true;
}

void FAudio_Output::write(uint16_t* finalWave, int) {
    uint32_t flags = 0;
    if (!initialized || failed)
        return;

    while (true) {
        if (device_changed) {
            close();

            if (!init(freq_))
                return;
        }

        FAudioSourceVoice_GetState(sVoice, &vState, flags);
        VBAM_CHECK(vState.BuffersQueued <= buffer_count_);

        if (vState.BuffersQueued < buffer_count_) {
            if (vState.BuffersQueued == 0) {
                // buffers ran dry
                if (systemVerbose & VERBOSE_SOUNDOUTPUT) {
                    static unsigned int i = 0;
                    log("FAudio: Buffers were not refilled fast enough (i=%i)\n", i++);
                }
            }

            // there is at least one free buffer
            break;
        } else {
            // the maximum number of buffers is currently queued
            if (!coreOptions.speedup && coreOptions.throttle && !gba_joybus_active) {
                // wait for one buffer to finish playing
                if (notify.WaitForSignal()) {
                    device_changed = true;
                }
            } else {
                // drop current audio frame
                return;
            }
        }
    }

    // copy & protect the audio data in own memory area while playing it
    memcpy(&buffers_[currentBuffer * sound_buffer_len_], finalWave, sound_buffer_len_);
    buf.AudioBytes = sound_buffer_len_;
    buf.pAudioData = &buffers_[currentBuffer * sound_buffer_len_];
    currentBuffer++;
    currentBuffer %= (buffer_count_ + 1);  // + 1 because we need one temporary buffer
    [[maybe_unused]] uint32_t hr = FAudioSourceVoice_SubmitSourceBuffer(sVoice, &buf, nullptr);
    VBAM_CHECK(hr == 0);
}

void FAudio_Output::pause() {
    if (!initialized || failed)
        return;

    if (playing) {
        [[maybe_unused]] uint32_t hr = FAudioSourceVoice_Stop(sVoice, 0, FAUDIO_COMMIT_NOW);
        VBAM_CHECK(hr == 0);
        playing = false;
    }
}

void FAudio_Output::resume() {
    if (!initialized || failed)
        return;

    if (!playing) {
        [[maybe_unused]] int32_t hr = FAudioSourceVoice_Start(sVoice, 0, FAUDIO_COMMIT_NOW);
        VBAM_CHECK(hr == 0);
        playing = true;
    }
}

void FAudio_Output::reset() {
    if (!initialized || failed)
        return;

    if (playing) {
        [[maybe_unused]] uint32_t hr = FAudioSourceVoice_Stop(sVoice, 0, FAUDIO_COMMIT_NOW);
        VBAM_CHECK(hr == 0);
    }

    FAudioSourceVoice_FlushSourceBuffers(sVoice);
    FAudioSourceVoice_Start(sVoice, 0, FAUDIO_COMMIT_NOW);
    playing = true;
}

void FAudio_Output::setThrottle(unsigned short throttle_) {
    if (!initialized || failed)
        return;

    if (throttle_ == 0)
        throttle_ = 100;

    [[maybe_unused]] uint32_t hr =
        FAudioSourceVoice_SetFrequencyRatio(sVoice, (float)throttle_ / 100.0f, FAUDIO_COMMIT_NOW);
    VBAM_CHECK(hr == 0);
}

}  // namespace

std::vector<AudioDevice> GetFAudioDevices() {
    FAudio* fa = nullptr;
    uint32_t hr;
    uint32_t flags = 0;
#ifdef _DEBUG
    flags = FAUDIO_DEBUG_ENGINE;
#endif
    hr = FAudioCreate(&fa, flags, FAUDIO_DEFAULT_PROCESSOR);

    if (hr != 0) {
        wxLogError(_("The FAudio interface failed to initialize!"));
        return {};
    }

    uint32_t dev_count = 0;
    hr = FAudio_GetDeviceCount(fa, &dev_count);
    if (hr != 0) {
        wxLogError(_("FAudio: Enumerating devices failed!"));
        return {};
    }

    std::vector<AudioDevice> devices;
    devices.reserve(dev_count + 1);
    devices.push_back({_("Default device"), wxEmptyString});

    for (uint32_t i = 0; i < dev_count; i++) {
        FAudioDeviceDetails dd;
        hr = FAudio_GetDeviceDetails(fa, i, &dd);
        if (hr != 0) {
            continue;
        }

        const wxString display_name(reinterpret_cast<wchar_t*>(dd.DisplayName));
        const wxString device_id(reinterpret_cast<wchar_t*>(dd.DeviceID));

        devices.push_back({display_name, device_id});
    }

    FAudio_Release(fa);
    return devices;
}

std::unique_ptr<SoundDriver> CreateFAudioDriver() {
    return std::make_unique<FAudio_Output>();
}

}  // namespace internal
}  // namespace audio