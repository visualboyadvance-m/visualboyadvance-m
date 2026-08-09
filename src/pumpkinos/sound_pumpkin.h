#ifndef VBAM_PUMPKINOS_SOUND_PUMPKIN_H_
#define VBAM_PUMPKINOS_SOUND_PUMPKIN_H_

#include "core/base/sound_driver.h"

// SoundDriver backed by a PalmOS SndStream. The stream callback runs on the
// host audio thread and drains a mutex-protected ring buffer that write()
// fills from the emulation thread.
class SoundPumpkin : public SoundDriver {
public:
    SoundPumpkin();
    ~SoundPumpkin() override;

    bool init(long sampleRate) override;
    void pause() override;
    void reset() override;
    void resume() override;
    void write(uint16_t* finalWave, int length) override;
    void setThrottle(unsigned short throttle) override;
};

// Stereo frames currently queued for output; the frame loop uses this as the
// pacing clock. Returns -1 when no stream is running (fall back to wall clock).
int soundPumpkinQueuedFrames();

#endif  // VBAM_PUMPKINOS_SOUND_PUMPKIN_H_
