#ifndef VBAM_QT_AUDIO_INTERNAL_DSOUND_H_
#define VBAM_QT_AUDIO_INTERNAL_DSOUND_H_

#if !defined(_WIN32)
#error "This file should only be included on Windows"
#endif

#include "qt/audio/audio.h"

namespace audio {
namespace internal {

// Returns the set of DirectSound devices.
std::vector<AudioDevice> GetDirectSoundDevices();

// Creates a DirectSound sound driver.
std::unique_ptr<SoundDriver> CreateDirectSoundDriver();

}  // namespace internal
}  // namespace audio

#endif  // VBAM_QT_AUDIO_INTERNAL_DSOUND_H_
