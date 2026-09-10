#ifndef VBAM_QT_AUDIO_INTERNAL_AAUDIO_H_
#define VBAM_QT_AUDIO_INTERNAL_AAUDIO_H_

#include "qt/audio/audio.h"

namespace audio {
namespace internal {

// Returns the AAudio "devices": AAudio routes to the system output itself, so
// this is just the default device entry.
std::vector<AudioDevice> GetAAudioDevices();

// Creates the Android AAudio sound driver. Android only, and only when the
// ENABLE_AAUDIO build option is on (VBAM_ENABLE_AAUDIO).
std::unique_ptr<SoundDriver> CreateAAudioDriver();

}  // namespace internal
}  // namespace audio

#endif  // VBAM_QT_AUDIO_INTERNAL_AAUDIO_H_
