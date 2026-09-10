#ifndef VBAM_QT_AUDIO_INTERNAL_OPENAL_H_
#define VBAM_QT_AUDIO_INTERNAL_OPENAL_H_

#if !defined(VBAM_ENABLE_OPENAL)
#error "This file should only be included if OpenAL is enabled"
#endif

#include "qt/audio/audio.h"

namespace audio {
namespace internal {

// Returns the set of OpenAL devices.
std::vector<AudioDevice> GetOpenALDevices();

// Creates an OpenAL sound driver.
std::unique_ptr<SoundDriver> CreateOpenALDriver();

}  // namespace internal
}  // namespace audio

#endif  // VBAM_QT_AUDIO_INTERNAL_OPENAL_H_
