// === LOGALL writes very detailed informations to vba-trace.log ===
//#define LOGALL

#ifndef NO_OAL

// for gopts
// also, wx-related
#include "wxvbam.h"

// Interface
#include "../common/ConfigManager.h"
#include "../common/SoundDriver.h"

// OpenAL
#include "openal.h"

// Internals
#include "../gba/Globals.h" // for 'speedup' and 'synchronize'
#include "../gba/Sound.h"

// Debug
#include <assert.h>
#define ASSERT_SUCCESS assert(AL_NO_ERROR == alGetError())

#ifndef LOGALL
// replace logging functions with comments
#ifdef winlog
#undef winlog
#endif
#define winlog //
#define debugState() //
#endif

struct OPENALFNTABLE;

class OpenAL : public SoundDriver {
public:
    OpenAL();
    virtual ~OpenAL();

    static bool GetDevices(wxArrayString& names, wxArrayString& ids);
    bool init(long sampleRate); // initialize the sound buffer queue
    void setThrottle(unsigned short throttle_); // set game speed
    void pause(); // pause the secondary sound buffer
    void reset(); // stop and reset the secondary sound buffer
    void resume(); // play/resume the secondary sound buffer
    void write(uint16_t* finalWave, int length); // write the emulated sound to a sound buffer

private:
    bool initialized;
    bool buffersLoaded;
    ALCdevice* device;
    ALCcontext* context;
    ALuint* buffer;
    ALuint tempBuffer;
    ALuint source;
    int freq;
    int soundBufferLen;

#ifdef LOGALL
    void debugState();
#endif
};

OpenAL::OpenAL()
{
    initialized = false;
    buffersLoaded = false;
    device = NULL;
    context = NULL;
    buffer = (ALuint*)malloc(gopts.audio_buffers * sizeof(ALuint));
    memset(buffer, 0, gopts.audio_buffers * sizeof(ALuint));
    tempBuffer = 0;
    source = 0;
}

OpenAL::~OpenAL()
{
    if (!initialized)
        return;

    alSourceStop(source);
    ASSERT_SUCCESS;
    alSourcei(source, AL_BUFFER, 0);
    ASSERT_SUCCESS;
    alDeleteSources(1, &source);
    ASSERT_SUCCESS;
    alDeleteBuffers(gopts.audio_buffers, buffer);
    ASSERT_SUCCESS;
    free(buffer);
    alcMakeContextCurrent(NULL);
    // Wine incorrectly returns ALC_INVALID_VALUE
    // and then fails the rest of these functions as well
    // so there will be a leak under Wine, but that's a bug in Wine, not
    // this code
    //ASSERT_SUCCESS;
    alcDestroyContext(context);
    //ASSERT_SUCCESS;
    alcCloseDevice(device);
    //ASSERT_SUCCESS;
    alGetError(); // reset error state
}

#ifdef LOGALL
void OpenAL::debugState()
{
    ALint value = 0;
    alGetSourcei(source, AL_SOURCE_STATE, &value);
    ASSERT_SUCCESS;
    winlog(" soundPaused = %i\n", soundPaused);
    winlog(" Source:\n");
    winlog("  State: ");

    switch (value) {
    case AL_INITIAL:
        winlog("AL_INITIAL\n");
        break;

    case AL_PLAYING:
        winlog("AL_PLAYING\n");
        break;

    case AL_PAUSED:
        winlog("AL_PAUSED\n");
        break;

    case AL_STOPPED:
        winlog("AL_STOPPED\n");
        break;

    default:
        winlog("!unknown!\n");
        break;
    }

    alGetSourcei(source, AL_BUFFERS_QUEUED, &value);
    ASSERT_SUCCESS;
    winlog("  Buffers in queue: %i\n", value);
    alGetSourcei(source, AL_BUFFERS_PROCESSED, &value);
    ASSERT_SUCCESS;
    winlog("  Buffers processed: %i\n", value);
}
#endif

bool OpenAL::init(long sampleRate)
{
    winlog("OpenAL::init\n");
    assert(initialized == false);

    if (!gopts.audio_dev.empty()) {
        device = alcOpenDevice(gopts.audio_dev.mb_str());
    } else {
        device = alcOpenDevice(NULL);
    }

    assert(device != NULL);
    context = alcCreateContext(device, NULL);
    assert(context != NULL);
    ALCboolean retVal = alcMakeContextCurrent(context);
    assert(ALC_TRUE == retVal);
    alGenBuffers(gopts.audio_buffers, buffer);
    ASSERT_SUCCESS;
    alGenSources(1, &source);
    ASSERT_SUCCESS;
    freq = sampleRate;
    // calculate the number of samples per frame first
    // then multiply it with the size of a sample frame (16 bit * stereo)
    soundBufferLen = (freq / 60) * 4;
    initialized = true;
    return true;
}

void OpenAL::setThrottle(unsigned short throttle_) {
    if (!initialized)
        return;

    if (!throttle_)
        throttle_ = 100;

    alSourcef(source, AL_PITCH, throttle_ / 100.0);
    ASSERT_SUCCESS;
}

void OpenAL::resume()
{
    if (!initialized)
        return;

    winlog("OpenAL::resume\n");

    if (!buffersLoaded)
        return;

    debugState();
    ALint sourceState = 0;
    alGetSourcei(source, AL_SOURCE_STATE, &sourceState);
    ASSERT_SUCCESS;

    if (sourceState != AL_PLAYING) {
        alSourcePlay(source);
        ASSERT_SUCCESS;
    }

    debugState();
}

void OpenAL::pause()
{
    if (!initialized)
        return;

    winlog("OpenAL::pause\n");

    if (!buffersLoaded)
        return;

    debugState();
    ALint sourceState = 0;
    alGetSourcei(source, AL_SOURCE_STATE, &sourceState);
    ASSERT_SUCCESS;

    if (sourceState == AL_PLAYING) {
        alSourcePause(source);
        ASSERT_SUCCESS;
    }

    debugState();
}

void OpenAL::reset()
{
    if (!initialized)
        return;

    winlog("OpenAL::reset\n");

    if (!buffersLoaded)
        return;

    debugState();
    ALint sourceState = 0;
    alGetSourcei(source, AL_SOURCE_STATE, &sourceState);
    ASSERT_SUCCESS;

    if (sourceState != AL_STOPPED) {
        alSourceStop(source);
        ASSERT_SUCCESS;
    }

    debugState();
}

void OpenAL::write(uint16_t* finalWave, int length)
{
    if (!initialized)
        return;

    winlog("OpenAL::write\n");
    debugState();
    ALint sourceState = 0;
    ALint nBuffersProcessed = 0;

    if (!buffersLoaded) {
        // ==initial buffer filling==
        winlog(" initial buffer filling\n");

        for (int i = 0; i < gopts.audio_buffers; i++) {
            // Filling the buffers explicitly with silence would be cleaner,
            // but the very first sample is usually silence anyway.
            alBufferData(buffer[i], AL_FORMAT_STEREO16, finalWave, soundBufferLen, freq);
            ASSERT_SUCCESS;
        }

        alSourceQueueBuffers(source, gopts.audio_buffers, buffer);
        ASSERT_SUCCESS;
        buffersLoaded = true;
    } else {
        // ==normal buffer refreshing==
        nBuffersProcessed = 0;
        alGetSourcei(source, AL_BUFFERS_PROCESSED, &nBuffersProcessed);
        ASSERT_SUCCESS;

        if (nBuffersProcessed == gopts.audio_buffers) {
            // we only want to know about it when we are emulating at full speed or faster:
            if ((throttle >= 100) || (throttle == 0)) {
                if (systemVerbose & VERBOSE_SOUNDOUTPUT) {
                    static unsigned int i = 0;
                    log("OpenAL: Buffers were not refilled fast enough (i=%i)\n", i++);
                }
            }
        }

        if (!speedup && throttle && !gba_joybus_active) {
            // wait until at least one buffer has finished
            while (nBuffersProcessed == 0) {
                winlog(" waiting...\n");
                // wait for about half the time one buffer needs to finish
                // unoptimized: ( sourceBufferLen * 1000 ) / ( freq * 2 * 2 ) * 1/2
                wxMilliSleep(soundBufferLen / (freq >> 7));
                alGetSourcei(source, AL_BUFFERS_PROCESSED, &nBuffersProcessed);
                ASSERT_SUCCESS;
            }
        } else {
            if (nBuffersProcessed == 0)
                return;
        }

        assert(nBuffersProcessed > 0);

        // unqueue buffer
        tempBuffer = 0;
        alSourceUnqueueBuffers(source, 1, &tempBuffer);
        ASSERT_SUCCESS;
        // refill buffer
        alBufferData(tempBuffer, AL_FORMAT_STEREO16, finalWave, soundBufferLen, freq);
        ASSERT_SUCCESS;
        // requeue buffer
        alSourceQueueBuffers(source, 1, &tempBuffer);
        ASSERT_SUCCESS;
    }

    // start playing the source if necessary
    alGetSourcei(source, AL_SOURCE_STATE, &sourceState);
    ASSERT_SUCCESS;

    if (!soundPaused && (sourceState != AL_PLAYING)) {
        alSourcePlay(source);
        ASSERT_SUCCESS;
    }
}

SoundDriver* newOpenAL()
{
    winlog("newOpenAL\n");
    return new OpenAL();
}

bool GetOALDevices(wxArrayString& names, wxArrayString& ids)
{
    return OpenAL::GetDevices(names, ids);
}

bool OpenAL::GetDevices(wxArrayString& names, wxArrayString& ids)
{
#ifdef ALC_DEVICE_SPECIFIER

    if (alcIsExtensionPresent(NULL, "ALC_ENUMERATION_EXT") == AL_FALSE)
        // this extension isn't critical to OpenAL operating
        return true;

    const char* devs = alcGetString(NULL, ALC_DEVICE_SPECIFIER);

    while (*devs) {
        names.push_back(wxString(devs, wxConvLibc));
        ids.push_back(names[names.size() - 1]);
        devs += strlen(devs) + 1;
    }

#endif

    // should work anyway, but must always use default driver
    return true;
}

#endif
