#include "sound_pumpkin.h"

#include <string.h>

extern "C" {
#include <PalmOS.h>

#include "sys.h"
#include "mutex.h"
#include "debug.h"
}

// PumpkinOS audio contract (liblsdl3): a dedicated audio thread calls the
// stream callback in an UNTHROTTLED loop and treats any short or zero
// return as end-of-stream — permanently, with no way to restart the pull
// loop. Consequences for this driver:
//  - the callback must always return a full buffer (silence on underrun),
//  - the callback paces itself to real time with a sleep (it is the only
//    place the pull rate can be controlled),
//  - pause()/reset() must never call SndStreamStop/SndStreamPause, because
//    a single zero-length pull while stopped kills the stream for good.
// The stream is created lazily on the first write() so the throwaway
// driver instance made before soundSetSampleRate() never opens a stream.

// Ring of interleaved stereo frames (~0.37s at 44100 Hz).
// Power of two so the free-running indices can be masked.
#define RING_FRAMES 16384
#define RING_MASK (RING_FRAMES - 1)

static int16_t ring[RING_FRAMES * 2];
static uint32_t ringRead;   // frame index, only advanced by the audio thread
static uint32_t ringWrite;  // frame index, only advanced by the emu thread
static mutex_t* ringMutex;

static SndStreamRef stream;
static bool streamActive;
static bool streamFailed;
static bool streamPaused;
static long streamRate = 44100;
static int64_t pullDue;  // pacing clock for the audio thread pulls (us)

// temp pull stats, logged roughly once a second from the audio thread
static int statPulls, statServed, statZero;
static int64_t statT0;

static Err streamCallback(void* userdata, SndStreamRef channel, void* buffer, UInt32 numberofframes) {
    int16_t* out = (int16_t*)buffer;
    UInt32 filled = 0;
    int64_t now;

    (void)userdata;
    (void)channel;

    // Pace the unthrottled pull loop to real time.
    now = sys_get_clock();
    if (pullDue == 0 || pullDue < now - 500000) pullDue = now;  // resync after gaps
    if (pullDue > now) sys_usleep((uint32_t)(pullDue - now));
    pullDue += (int64_t)numberofframes * 1000000 / streamRate;

    if (!streamPaused && ringMutex && mutex_lock(ringMutex) == 0) {
        uint32_t avail = ringWrite - ringRead;
        if (avail > numberofframes) avail = numberofframes;
        for (uint32_t i = 0; i < avail; i++, ringRead++) {
            out[i * 2] = ring[(ringRead & RING_MASK) * 2];
            out[i * 2 + 1] = ring[(ringRead & RING_MASK) * 2 + 1];
        }
        filled = avail;
        mutex_unlock(ringMutex);
    }

    if (filled < numberofframes) {
        memset(out + filled * 2, 0, (numberofframes - filled) * 2 * sizeof(int16_t));
    }

    statPulls++;
    statServed += filled;
    statZero += numberofframes - filled;
    if (statT0 == 0) statT0 = now;
    if (now - statT0 >= 1000000) {
        debug(DEBUG_INFO, "VBAM", "audio: pulls=%d frames=%d served=%d silence=%d paused=%d",
              statPulls, (int)numberofframes, statServed, statZero, streamPaused ? 1 : 0);
        statPulls = statServed = statZero = 0;
        statT0 = now;
    }

    return errNone;
}

static void ringClear(void) {
    if (ringMutex && mutex_lock(ringMutex) == 0) {
        ringRead = ringWrite = 0;
        mutex_unlock(ringMutex);
    }
}

SoundPumpkin::SoundPumpkin() {}

SoundPumpkin::~SoundPumpkin() {
    if (streamActive) {
        SndStreamDelete(stream);
        streamActive = false;
        stream = 0;
    }
}

bool SoundPumpkin::init(long sampleRate) {
    if (!ringMutex) {
        ringMutex = mutex_create((char*)"vbamsnd");
    }

    if (streamActive) {  // rate change: drop the old stream, recreate lazily
        SndStreamDelete(stream);
        streamActive = false;
        stream = 0;
    }

    streamRate = sampleRate;
    streamFailed = false;
    streamPaused = false;
    pullDue = 0;
    ringClear();

    return true;
}

void SoundPumpkin::pause() {
    streamPaused = true;  // never SndStreamStop: a zero pull kills the stream
}

void SoundPumpkin::resume() {
    streamPaused = false;
}

void SoundPumpkin::reset() {
    ringClear();
}

void SoundPumpkin::write(uint16_t* finalWave, int length) {
    // finalWave is interleaved signed 16-bit stereo; length counts int16
    // samples (left and right separately).
    const int16_t* in = (const int16_t*)finalWave;
    int frames = length >> 1;

    if (!streamActive && !streamFailed) {
        Err err = SndStreamCreate(&stream, sndOutput, (UInt32)streamRate, sndInt16, sndStereo,
                                  streamCallback, nullptr, 0, false);
        if (err == errNone) {
            if ((err = SndStreamStart(stream)) == errNone) {
                streamActive = true;
                debug(DEBUG_INFO, "VBAM", "audio stream started (rate %ld)", streamRate);
            } else {
                SndStreamDelete(stream);
                stream = 0;
                streamFailed = true;
                debug(DEBUG_ERROR, "VBAM", "SndStreamStart failed: %d", err);
            }
        } else {
            streamFailed = true;
            debug(DEBUG_ERROR, "VBAM", "SndStreamCreate failed: %d", err);
        }
    }

    if (!streamActive || !ringMutex) return;

    streamPaused = false;  // the core is producing audio: make sure it plays

    if (mutex_lock(ringMutex) == 0) {
        for (int i = 0; i < frames; i++, ringWrite++) {
            if (ringWrite - ringRead >= RING_FRAMES) break;  // full: drop the tail
            ring[(ringWrite & RING_MASK) * 2] = in[i * 2];
            ring[(ringWrite & RING_MASK) * 2 + 1] = in[i * 2 + 1];
        }
        mutex_unlock(ringMutex);
    }
}

void SoundPumpkin::setThrottle(unsigned short throttle) {
    (void)throttle;
}

int soundPumpkinQueuedFrames() {
    int queued = -1;

    if (streamActive && !streamFailed && ringMutex && mutex_lock(ringMutex) == 0) {
        queued = (int)(ringWrite - ringRead);
        mutex_unlock(ringMutex);
    }

    return queued;
}
