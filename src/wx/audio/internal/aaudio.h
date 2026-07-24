#ifndef VBAM_WX_AUDIO_INTERNAL_AAUDIO_H_
#define VBAM_WX_AUDIO_INTERNAL_AAUDIO_H_

#include <memory>

#include "core/base/sound_driver.h"

namespace audio {
namespace internal {

// Creates the Android AAudio sound driver. Android only.
std::unique_ptr<SoundDriver> CreateAAudioDriver();

}  // namespace internal
}  // namespace audio

#endif  // VBAM_WX_AUDIO_INTERNAL_AAUDIO_H_
