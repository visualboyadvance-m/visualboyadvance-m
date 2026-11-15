#ifndef VBAM_WX_AUDIO_INTERNAL_SOUNDMANAGER_H_
#define VBAM_WX_AUDIO_INTERNAL_SOUNDMANAGER_H_

#include <memory>

#include "core/base/sound_driver.h"

namespace audio {
namespace internal {

// Creates the classic Mac OS Sound Manager driver. Retro68 builds only
// (__WXRETRO68__): System 7 through Mac OS 9, 68k and PowerPC.
std::unique_ptr<SoundDriver> CreateSoundManagerDriver();

}  // namespace internal
}  // namespace audio

#endif  // VBAM_WX_AUDIO_INTERNAL_SOUNDMANAGER_H_
