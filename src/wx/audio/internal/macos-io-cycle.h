#ifndef WX_AUDIO_INTERNAL_MACOS_IO_CYCLE_H_
#define WX_AUDIO_INTERNAL_MACOS_IO_CYCLE_H_

#if defined(__APPLE__)

#include <cstdint>

#include <wx/string.h>

namespace audio {
namespace internal {

// The output device named `name` (a CoreAudio device name, as the audio device
// option holds it), or the default output device for an empty name, the "Default
// device" entry or a name no device has. 0 if there is none at all.
uint32_t MacosOutputDevice(const wxString& name);

// Sets this process's I/O cycle on the CoreAudio device `device` to the period that
// divides the driver's buffers (DividingPeriod): `buffer_frames` fed at `buffer_rate`,
// the throttle included. Every audio API on macOS plays through CoreAudio, and the
// device hands buffers back at the end of an I/O cycle whichever API queued them, so
// this is what spaces them evenly for the CoreAudio, SDL and OpenAL drivers alike. The
// size is per process; other clients of the device keep theirs. Left as it is where no
// period divides the buffer.
void SetMacosIoCycle(uint32_t device, uint32_t buffer_frames, uint32_t buffer_rate);

}  // namespace internal
}  // namespace audio

#endif  // __APPLE__

#endif  // WX_AUDIO_INTERNAL_MACOS_IO_CYCLE_H_
