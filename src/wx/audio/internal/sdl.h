#ifndef WX_AUDIO_INTERNAL_SDL_H_
#define WX_AUDIO_INTERNAL_SDL_H_

#include "wx/audio/audio.h"

namespace audio {
namespace internal {

// Returns the set of OpenAL devices.
std::vector<AudioDevice> GetSDLDevices();

// Creates an OpenAL sound driver.
std::unique_ptr<SoundDriver> CreateSDLDriver();

}  // namespace internal
}  // namespace audio

#endif  // WX_AUDIO_INTERNAL_SDL_H_
