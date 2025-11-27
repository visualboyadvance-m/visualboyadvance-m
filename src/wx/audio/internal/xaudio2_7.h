#ifndef WX_AUDIO_INTERNAL_XAUDIO2_7_H_
#define WX_AUDIO_INTERNAL_XAUDIO2_7_H_

#include "core/base/sound_driver.h"
#include "wx/audio/audio.h"
#include <memory>
#include <vector>

// Include the appropriate XAudio2 header
#if _MSC_VER
#include <xaudio2.legacy.h>
#else
#include <XAudio2.h>
#endif

namespace audio {
namespace internal {

std::unique_ptr<SoundDriver> CreateXAudio2_7_Driver(IXAudio2* xaudio2);
std::vector<audio::AudioDevice> GetXAudio2_7_Devices(IXAudio2* xaudio2);

}  // namespace internal
}  // namespace audio

#endif  // WX_AUDIO_INTERNAL_XAUDIO2_7_H_