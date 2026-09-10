#ifndef VBAM_QT_AUDIO_AUDIO_H_
#define VBAM_QT_AUDIO_AUDIO_H_

#include <memory>
#include <vector>

#include <QString>

#include "core/base/sound_driver.h"
#include "qt/config/option.h"

namespace audio {

// Represents an audio device.
struct AudioDevice {
    // The device user-friendly name.
    QString name;
    // The underlying device ID.
    QString id;
};

// The (translated) display name used for the default output device entry.
QString DefaultDeviceName();

// Returns the set of audio devices for the given API.
std::vector<AudioDevice> EnumerateAudioDevices(const config::AudioApi& api);

// Creates a sound driver for the given API.
std::unique_ptr<SoundDriver> CreateSoundDriver(const config::AudioApi& api);

}  // namespace audio

#endif  // VBAM_QT_AUDIO_AUDIO_H_
