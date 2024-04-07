#ifndef WX_AUDIO_INTERNAL_OPENAL_H_
#define WX_AUDIO_INTERNAL_OPENAL_H_

#include "wx/audio/audio.h"

namespace audio {
namespace internal {

// Returns the set of OpenAL devices.
std::vector<AudioDevice> GetOpenALDevices();

// Creates an OpenAL sound driver.
std::unique_ptr<SoundDriver> CreateOpenALDriver();

}  // namespace internal
}  // namespace audio

#endif  // WX_AUDIO_INTERNAL_OPENAL_H_
