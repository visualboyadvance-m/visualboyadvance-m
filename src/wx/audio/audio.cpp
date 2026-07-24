#include "wx/audio/audio.h"

#include <memory>

#include <wx/translation.h>

#include "core/base/check.h"
#include "core/base/null_sound_driver.h"
#include "wx/audio/internal/sdl.h"

#if defined(VBAM_ENABLE_OPENAL)
#include "wx/audio/internal/openal.h"
#endif

#if defined(__WXMSW__)
#include "wx/audio/internal/dsound.h"
#endif

#if defined(VBAM_ENABLE_FAUDIO)
#include "wx/audio/internal/faudio.h"
#endif

#if defined(__WXMAC__)
#include "wx/audio/internal/coreaudio.h"
#endif

#if defined(VBAM_ENABLE_XAUDIO2)
#include "wx/audio/internal/xaudio2.h"
#endif

#if defined(__ANDROID__)
#include "wx/audio/internal/aaudio.h"
#endif

namespace audio {
 
std::vector<AudioDevice> EnumerateAudioDevices(const config::AudioApi& audio_api) {
    switch (audio_api) {
#if defined(VBAM_ENABLE_OPENAL)
        case config::AudioApi::kOpenAL:
            return audio::internal::GetOpenALDevices();
#endif

        case config::AudioApi::kSDL:
            return audio::internal::GetSDLDevices();

#if defined(__WXMSW__)
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

#if defined(__WXMAC__)
        case config::AudioApi::kCoreAudio:
            return audio::internal::GetCoreAudioDevices();
#endif

#if defined(__ANDROID__)
        case config::AudioApi::kAAudio:
            // AAudio plays to the default output; no device selection.
            return std::vector<AudioDevice>({{_("Default device"), wxEmptyString}});
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

#if defined(__WXMSW__)
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

#if defined(__WXMAC__)
        case config::AudioApi::kCoreAudio:
            return audio::internal::CreateCoreAudioDriver();
#endif

#if defined(__ANDROID__)
        case config::AudioApi::kAAudio:
            return audio::internal::CreateAAudioDriver();
#endif

        case config::AudioApi::kNull:
            return std::make_unique<NullSoundDriver>();

        case config::AudioApi::kLast:
        default:
            VBAM_NOTREACHED_RETURN(NULL);
    }
}

}  // namespace audio
