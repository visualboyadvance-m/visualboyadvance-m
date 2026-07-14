#include "core/base/null_sound_driver.h"

#include <thread>

#include "core/base/system.h"

bool NullSoundDriver::init(long sampleRate) {
    sample_rate_ = sampleRate > 0 ? sampleRate : 44100;
    have_deadline_ = false;
    return true;
}

void NullSoundDriver::pause() { have_deadline_ = false; }
void NullSoundDriver::reset() { have_deadline_ = false; }
void NullSoundDriver::resume() { have_deadline_ = false; }
void NullSoundDriver::setThrottle(unsigned short /*throttle*/) {}

void NullSoundDriver::write(uint16_t* /*finalWave*/, int length) {
    if (length <= 0)
        return;

    // Don't pace during fast-forward or when throttling is off -- the real
    // drivers likewise skip their buffer wait in that case.
    if (coreOptions.speedup || coreOptions.throttle == 0) {
        have_deadline_ = false;
        return;
    }

    // `length` is the amount just produced: the wx audio path passes a byte
    // count (4 bytes per stereo frame), the libretro path a 16-bit sample count
    // (2 samples per stereo frame).
#ifdef __LIBRETRO__
    const double frames = length / 2.0;
#else
    const double frames = length / 4.0;
#endif
    const double rate =
        static_cast<double>(sample_rate_) * (coreOptions.throttle / 100.0);
    if (rate <= 0.0)
        return;

    const auto play_time = std::chrono::duration_cast<Clock::duration>(
        std::chrono::duration<double>(frames / rate));
    const auto now = Clock::now();
    if (!have_deadline_) {
        deadline_ = now;
        have_deadline_ = true;
    }
    deadline_ += play_time;
    if (deadline_ > now) {
        // Ahead of schedule: sleep off only the surplus. Do NOT resync the
        // deadline to `now` here. The frame's non-audio work (emulation,
        // filtering, paint) runs between write() calls, so on entry `now` is
        // normally a little past the previous deadline; resetting to `now` every
        // call would then sleep a full buffer on top of that work and roughly
        // halve the frame rate. Letting the deadline accumulate means we sleep
        // only the surplus -- the same effect as the real drivers blocking in
        // write() only until their output buffer drains.
        std::this_thread::sleep_until(deadline_);
    } else if (now - deadline_ > std::chrono::milliseconds(250)) {
        // Fell far behind (host too slow, or resumed after a pause/stall):
        // resync so we don't then race to catch up.
        deadline_ = now;
    }
}
