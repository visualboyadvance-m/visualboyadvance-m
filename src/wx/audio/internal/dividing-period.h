#ifndef WX_AUDIO_INTERNAL_DIVIDING_PERIOD_H_
#define WX_AUDIO_INTERNAL_DIVIDING_PERIOD_H_

#include <cstdint>

namespace audio {
namespace internal {

// The device period, in device frames, that a driver asks for so its buffers end
// evenly spaced in time.
//
// A driver's queue paces the emulator: write() waits for a buffer to come back. A
// device hands buffers back only at the end of a period, though, so a buffer that is
// not a whole number of periods ends one period and then another after the one before:
// a buffer of 800 frames against the usual 512 at 48 kHz came back 10.7 and 21.3 ms
// apart, and the emulator's frames with it, a steady judder at a full 60 fps. With a
// period that divides the buffer, every buffer ends exactly a buffer's time after the
// one before.
//
// The buffer is `buffer_frames` at `buffer_rate` -- the rate the driver feeds the
// device at, the throttle included, so at 200 % a 1/60 s buffer plays in 1/120 s --
// and the device plays at `device_rate`. The period is the largest divisor of the
// buffer's length in device frames up to about 5 ms, short enough to keep the
// wake-ups cheap and long enough to stay clear of underruns, within [`lo`, `hi`].
// 0 when there is none: the buffer is not a whole number of device frames (300 % at
// 48 kHz: 266.7), or nothing in range divides it.
inline uint32_t DividingPeriod(uint32_t buffer_frames, uint32_t buffer_rate, uint32_t device_rate,
                               uint32_t lo = 1, uint32_t hi = UINT32_MAX) {
    if (!buffer_frames || !buffer_rate || !device_rate)
        return 0;
    const uint64_t num = static_cast<uint64_t>(buffer_frames) * device_rate;
    if (num % buffer_rate)
        return 0;
    const uint64_t frames = num / buffer_rate;
    const uint64_t limit = device_rate / 200;  // 5 ms
    for (uint64_t d = 1; d <= frames; ++d) {
        if (frames % d)
            continue;
        const uint64_t period = frames / d;
        if (period <= limit && period >= lo && period <= hi)
            return static_cast<uint32_t>(period);
        if (period < lo)
            break;
    }
    return 0;
}

}  // namespace internal
}  // namespace audio

#endif  // WX_AUDIO_INTERNAL_DIVIDING_PERIOD_H_
