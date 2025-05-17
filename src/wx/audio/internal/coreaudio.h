#ifndef WX_AUDIO_INTERNAL_COREAUDIO_H_
#define WX_AUDIO_INTERNAL_COREAUDIO_H_

#include "wx/audio/audio.h"

namespace audio {
namespace internal {

// Returns the set of OpenAL devices.
std::vector<AudioDevice> GetCoreAudioDevices();

// Creates an OpenAL sound driver.
std::unique_ptr<SoundDriver> CreateCoreAudioDriver();

}  // namespace internal
}  // namespace audio

#endif  // WX_AUDIO_INTERNAL_COREAUDIO_H_
