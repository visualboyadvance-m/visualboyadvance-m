#include "wx/audio/audio.h"

#include "wx/audio/internal/openal.h"

#if defined(__WXMSW__)
#include "wx/audio/internal/dsound.h"
#endif


#if defined(VBAM_ENABLE_FAUDIO)
#include "wx/audio/internal/faudio.h"
#endif

#if defined(VBAM_ENABLE_XAUDIO2)
#include "wx/audio/internal/xaudio2.h"
#endif

namespace audio {
 
std::vector<AudioDevice> EnumerateAudioDevices(const config::AudioApi& audio_api) {
    switch (audio_api) {
        case config::AudioApi::kOpenAL:
            return audio::internal::GetOpenALDevices();

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

        case config::AudioApi::kLast:
        default:
            // This should never happen.
            assert(false);
            return {};
    }
}

std::unique_ptr<SoundDriver> CreateSoundDriver(const config::AudioApi& api) {
    switch (api) {
        case config::AudioApi::kOpenAL:
            return audio::internal::CreateOpenALDriver();

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

        case config::AudioApi::kLast:
        default:
            // This should never happen.
            assert(false);
            return nullptr;
    }
}

}  // namespace audio
