#ifndef WX_AUDIO_AUDIO_H_
#define WX_AUDIO_AUDIO_H_

#include <memory>
#include <vector>

#include <wx/string.h>

#include "core/base/sound_driver.h"
#include "wx/config/option.h"

namespace audio {

// Represents an audio device.
struct AudioDevice {
    // The device user-friendly name.
    wxString name;
    // The underlying device ID.
    wxString id;
};

// Returns the set of audio devices for the given API.
std::vector<AudioDevice> EnumerateAudioDevices(const config::AudioApi& api);

// Creates a sound driver for the given API.
std::unique_ptr<SoundDriver> CreateSoundDriver(const config::AudioApi& api);

}  // namespace audio

#endif  // WX_AUDIO_AUDIO_H_
