#include "qt/audio/audio.h"

#include <memory>

#include <QCoreApplication>

#include "core/base/check.h"
#include "core/base/null_sound_driver.h"
#include "qt/audio/internal/sdl.h"

#if defined(VBAM_ENABLE_OPENAL)
#include "qt/audio/internal/openal.h"
#endif

#if defined(_WIN32)
#include "qt/audio/internal/dsound.h"
#endif

#if defined(VBAM_ENABLE_FAUDIO)
#include "qt/audio/internal/faudio.h"
#endif

#if defined(__APPLE__)
#include "qt/audio/internal/coreaudio.h"
#endif

#if defined(VBAM_ENABLE_XAUDIO2)
#include "qt/audio/internal/xaudio2.h"
#endif

#if defined(VBAM_ENABLE_AAUDIO)
#include "qt/audio/internal/aaudio.h"
#endif

namespace audio {

QString DefaultDeviceName() {
    return QCoreApplication::translate("vbam", "Default device");
}

std::vector<AudioDevice> EnumerateAudioDevices(const config::AudioApi& audio_api) {
    switch (audio_api) {
#if defined(VBAM_ENABLE_OPENAL)
        case config::AudioApi::kOpenAL:
            return audio::internal::GetOpenALDevices();
#endif

        case config::AudioApi::kSDL:
            return audio::internal::GetSDLDevices();

#if defined(_WIN32)
        case config::AudioApi::kDirectSound:
            return audio::internal::GetDirectSoundDevices();
#endif

#if defined(VBAM_ENABLE_XAUDIO2)
        case config::AudioApi::kXAudio2:
            return audio::internal::GetXAudio2Devices();
#endif

#if defined(VBAM_ENABLE_FAUDIO)
        case config::AudioApi::kFAudio:
            return audio::internal::GetFAudioDevices();
#endif

#if defined(__APPLE__)
        case config::AudioApi::kCoreAudio:
            return audio::internal::GetCoreAudioDevices();
#endif

#if defined(VBAM_ENABLE_AAUDIO)
        case config::AudioApi::kAAudio:
            return audio::internal::GetAAudioDevices();
#endif

        case config::AudioApi::kNull:
            // No device selection for the null driver.
            return std::vector<AudioDevice>();

        case config::AudioApi::kLast:
        default:
            VBAM_NOTREACHED_RETURN(std::vector<AudioDevice>());
    }
}

std::unique_ptr<SoundDriver> CreateSoundDriver(const config::AudioApi& api) {
    switch (api) {
#if defined(VBAM_ENABLE_OPENAL)
        case config::AudioApi::kOpenAL:
            return audio::internal::CreateOpenALDriver();
#endif

        case config::AudioApi::kSDL:
            return audio::internal::CreateSDLDriver();

#if defined(_WIN32)
        case config::AudioApi::kDirectSound:
            return audio::internal::CreateDirectSoundDriver();
#endif

#if defined(VBAM_ENABLE_XAUDIO2)
        case config::AudioApi::kXAudio2:
            return audio::internal::CreateXAudio2Driver();
#endif

#if defined(VBAM_ENABLE_FAUDIO)
        case config::AudioApi::kFAudio:
            return audio::internal::CreateFAudioDriver();
#endif

#if defined(__APPLE__)
        case config::AudioApi::kCoreAudio:
            return audio::internal::CreateCoreAudioDriver();
#endif

#if defined(VBAM_ENABLE_AAUDIO)
        case config::AudioApi::kAAudio:
            return audio::internal::CreateAAudioDriver();
#endif

        case config::AudioApi::kNull:
            return std::make_unique<NullSoundDriver>();

        case config::AudioApi::kLast:
        default:
            VBAM_NOTREACHED_RETURN(nullptr);
    }
}

}  // namespace audio
