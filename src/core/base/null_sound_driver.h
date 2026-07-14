#ifndef VBAM_CORE_BASE_NULL_SOUND_DRIVER_H_
#define VBAM_CORE_BASE_NULL_SOUND_DRIVER_H_

#include <chrono>

#include "core/base/sound_driver.h"

// A sound driver that outputs no audio. It exists so emulation keeps running,
// and stays paced to real time, when no working audio device is available --
// both as an explicit "null" choice in the sound API list and as the automatic
// fallback when the selected driver fails to initialize.
//
// The real drivers throttle emulation by blocking in write() until their output
// buffer drains. This driver reproduces that pacing with a monotonic clock
// instead of a device: write() advances a running deadline by the real time its
// samples represent and sleeps only the surplus. Nothing else is done.
// Fast-forward and the throttle setting are honored.
class NullSoundDriver : public SoundDriver {
public:
    bool init(long sampleRate) override;
    void pause() override;
    void reset() override;
    void resume() override;
    void write(uint16_t* finalWave, int length) override;
    void setThrottle(unsigned short throttle) override;

private:
    using Clock = std::chrono::steady_clock;
    long sample_rate_ = 44100;
    bool have_deadline_ = false;
    Clock::time_point deadline_;
};

#endif  // VBAM_CORE_BASE_NULL_SOUND_DRIVER_H_
