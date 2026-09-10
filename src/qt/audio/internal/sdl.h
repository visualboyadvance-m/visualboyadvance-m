#ifndef VBAM_QT_AUDIO_INTERNAL_SDL_H_
#define VBAM_QT_AUDIO_INTERNAL_SDL_H_

#include "qt/audio/audio.h"

namespace audio {
namespace internal {

// Returns the set of SDL audio devices.
std::vector<AudioDevice> GetSDLDevices();

// Creates an SDL sound driver.
std::unique_ptr<SoundDriver> CreateSDLDriver();

}  // namespace internal
}  // namespace audio

#endif  // VBAM_QT_AUDIO_INTERNAL_SDL_H_
