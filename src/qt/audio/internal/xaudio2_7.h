#ifndef VBAM_QT_AUDIO_INTERNAL_XAUDIO2_7_H_
#define VBAM_QT_AUDIO_INTERNAL_XAUDIO2_7_H_

#if !defined(_WIN32)
#error "This file should only be included on Windows"
#endif

#if !defined(VBAM_ENABLE_XAUDIO2)
#error "This file should only be compiled if XAudio2 is enabled"
#endif

#include <memory>
#include <vector>

#include "core/base/sound_driver.h"
#include "qt/audio/audio.h"

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

#endif  // VBAM_QT_AUDIO_INTERNAL_XAUDIO2_7_H_
