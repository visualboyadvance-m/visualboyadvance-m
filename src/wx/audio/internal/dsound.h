#ifndef WX_AUDIO_INTERNAL_DSOUND_H_
#define WX_AUDIO_INTERNAL_DSOUND_H_

#if !defined(__WXMSW__)
#error "This file should only be included on Windows"
#endif

#include "wx/audio/audio.h"

namespace audio {
namespace internal {

// Returns the set of DirectSound devices.
std::vector<AudioDevice> GetDirectSoundDevices();

// Creates a DirectSound sound driver.
std::unique_ptr<SoundDriver> CreateDirectSoundDriver();

}  // namespace internal
}  // namespace audio

#endif  // WX_AUDIO_INTERNAL_DSOUND_H_
