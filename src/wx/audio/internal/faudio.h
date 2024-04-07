#ifndef WX_AUDIO_INTERNAL_FAUDIO_H_
#define WX_AUDIO_INTERNAL_FAUDIO_H_

#if !defined(VBAM_ENABLE_FAUDIO)
#error "This file should only be included if FAudio is enabled"
#endif

#include "wx/audio/audio.h"

namespace audio {
namespace internal {

// Returns the set of FAudio devices.
std::vector<AudioDevice> GetFAudioDevices();

// Creates an FAudio sound driver.
std::unique_ptr<SoundDriver> CreateFAudioDriver();

}  // namespace internal
}  // namespace audio

#endif  // WX_AUDIO_INTERNAL_FAUDIO_H_
