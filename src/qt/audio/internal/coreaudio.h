#ifndef VBAM_QT_AUDIO_INTERNAL_COREAUDIO_H_
#define VBAM_QT_AUDIO_INTERNAL_COREAUDIO_H_

#if !defined(__APPLE__)
#error "This file should only be included on Apple platforms"
#endif

#include "qt/audio/audio.h"

namespace audio {
namespace internal {

// Returns the set of CoreAudio devices.
std::vector<AudioDevice> GetCoreAudioDevices();

// Creates a CoreAudio sound driver.
std::unique_ptr<SoundDriver> CreateCoreAudioDriver();

}  // namespace internal
}  // namespace audio

#endif  // VBAM_QT_AUDIO_INTERNAL_COREAUDIO_H_
