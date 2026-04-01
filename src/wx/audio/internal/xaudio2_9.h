#ifndef WX_AUDIO_INTERNAL_XAUDIO2_9_H_
#define WX_AUDIO_INTERNAL_XAUDIO2_9_H_

#if !defined(__WXMSW__)
#error "This file should only be included on Windows"
#endif

#if !defined(VBAM_ENABLE_XAUDIO2)
#error "This file should only be compiled if XAudio2 is enabled"
#endif


#include "core/base/sound_driver.h"
#include "wx/audio/audio.h"
#include <memory>
#include <vector>

// Include XAudio2 2.9 header
// XAudio2 2.9 requires _WIN32_WINNT >= 0x0602 (Windows 8) or later.
#if defined(_WIN32_WINNT) && _WIN32_WINNT < 0x0602
#pragma push_macro("_WIN32_WINNT")
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0602
#include <xaudio2.h>
#pragma pop_macro("_WIN32_WINNT")
#else
#include <xaudio2.h>
#endif

namespace audio {
namespace internal {

std::unique_ptr<SoundDriver> CreateXAudio2_9_Driver(IXAudio2* xaudio2);
std::vector<audio::AudioDevice> GetXAudio2_9_Devices(IXAudio2* xaudio2);

}  // namespace internal
}  // namespace audio

#endif  // WX_AUDIO_INTERNAL_XAUDIO2_9_H_