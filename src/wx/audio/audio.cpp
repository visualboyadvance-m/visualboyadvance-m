#include "wx/audio/audio.h"

#include "core/base/check.h"
#include "wx/audio/internal/sdl.h"
#include "wx/audio/internal/openal.h"

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

namespace audio {
 
std::vector<AudioDevice> EnumerateAudioDevices(const config::AudioApi& audio_api) {
    switch (audio_api) {
        case config::AudioApi::kOpenAL:
            return audio::internal::GetOpenALDevices();

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

        case config::AudioApi::kLast:
        default:
            VBAM_NOTREACHED();
            return {};
    }
}

std::unique_ptr<SoundDriver> CreateSoundDriver(const config::AudioApi& api) {
    switch (api) {
        case config::AudioApi::kOpenAL:
            return audio::internal::CreateOpenALDriver();

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

        case config::AudioApi::kLast:
        default:
            VBAM_NOTREACHED();
            return nullptr;
    }
}

}  // namespace audio
