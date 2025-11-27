#ifndef WX_AUDIO_INTERNAL_XAUDIO2_H_
#define WX_AUDIO_INTERNAL_XAUDIO2_H_

#include "wx/audio/audio.h"
#include <memory>
#include <vector>

namespace audio {
namespace internal {

// Returns the set of XAudio2 devices.
std::vector<AudioDevice> GetXAudio2Devices();

// Creates an XAudio2 sound driver.
std::unique_ptr<SoundDriver> CreateXAudio2Driver();

}  // namespace internal
}  // namespace audio

#endif  // WX_AUDIO_INTERNAL_XAUDIO2_H_