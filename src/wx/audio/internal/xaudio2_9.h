#ifndef WX_AUDIO_INTERNAL_XAUDIO2_9_H_
#define WX_AUDIO_INTERNAL_XAUDIO2_9_H_

#include "core/base/sound_driver.h"
#include "wx/audio/audio.h"
#include <memory>
#include <vector>

// Include XAudio2 2.9 header
#include <xaudio2.h>

namespace audio {
namespace internal {

std::unique_ptr<SoundDriver> CreateXAudio2_9_Driver(IXAudio2* xaudio2);
std::vector<audio::AudioDevice> GetXAudio2_9_Devices(IXAudio2* xaudio2);

}  // namespace internal
}  // namespace audio

#endif  // WX_AUDIO_INTERNAL_XAUDIO2_9_H_