#include "wx/audio/internal/openal.h"

// === LOGALL writes very detailed informations to vba-trace.log ===
// #define LOGALL

// on win32 and mac, pointer typedefs only happen with AL_NO_PROTOTYPES
// on mac, ALC_NO_PROTOTYPES as well

// #define AL_NO_PROTOTYPES 1

// on mac, alc pointer typedefs ony happen for ALC if ALC_NO_PROTOTYPES
// unfortunately, there is a bug in the system headers (use of ALCvoid when
// void should be used; shame on Apple for introducing this error, and shame
// on Creative for making a typedef to void in the first place)
// #define ALC_NO_PROTOTYPES 1

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunknown-pragmas"
#endif
#include <al.h>
#include <alc.h>
#ifdef __clang__
#pragma clang diagnostic pop
#endif

// since the ALC typedefs are broken on Mac:

#ifdef __WXMAC__
typedef ALCcontext*(ALC_APIENTRY* LPALCCREATECONTEXT)(ALCdevice* device, const ALCint* attrlist);
typedef ALCboolean(ALC_APIENTRY* LPALCMAKECONTEXTCURRENT)(ALCcontext* context);
typedef void(ALC_APIENTRY* LPALCDESTROYCONTEXT)(ALCcontext* context);
typedef ALCdevice*(ALC_APIENTRY* LPALCOPENDEVICE)(const ALCchar* devicename);
typedef ALCboolean(ALC_APIENTRY* LPALCCLOSEDEVICE)(ALCdevice* device);
typedef ALCboolean(ALC_APIENTRY* LPALCISEXTENSIONPRESENT)(ALCdevice* device,
                                                          const ALCchar* extname);
typedef const ALCchar*(ALC_APIENTRY* LPALCGETSTRING)(ALCdevice* device, ALCenum param);
#endif

#include <wx/arrstr.h>
#include <wx/log.h>
#include <wx/translation.h>
#include <wx/utils.h>

#include "core/base/check.h"
#include "core/gba/gbaGlobals.h"
#include "core/gba/gbaSound.h"
#include "wx/config/option-proxy.h"

namespace audio {
namespace internal {

namespace {

// Debug
#define ASSERT_SUCCESS VBAM_CHECK(AL_NO_ERROR == alGetError())

#ifndef LOGALL
// replace logging functions with comments
#ifdef winlog
#undef winlog
#endif
// https://stackoverflow.com/a/1306690/262458
#define winlog(x, ...) \
    do {               \
    } while (0)
#define debugState()  //
#endif

struct OPENALFNTABLE;

class OpenAL : public SoundDriver {
public:
    OpenAL();
    ~OpenAL() override;

    bool init(long sampleRate) override;                  // initialize the sound buffer queue
    void setThrottle(unsigned short throttle_) override;  // set game speed
    void pause() override;                                // pause the secondary sound buffer
    void reset() override;   // stop and reset the secondary sound buffer
    void resume() override;  // play/resume the secondary sound buffer
    void write(uint16_t* finalWave,
               int length) override;  // write the emulated sound to a sound buffer

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

OpenAL::OpenAL() {
    initialized = false;
    buffersLoaded = false;
    device = NULL;
    context = NULL;
    buffer = (ALuint*)malloc(OPTION(kSoundBuffers) * sizeof(ALuint));
    memset(buffer, 0, OPTION(kSoundBuffers) * sizeof(ALuint));
    tempBuffer = 0;
    source = 0;
}

OpenAL::~OpenAL() {
    if (!initialized)
        return;

    alSourceStop(source);
    ASSERT_SUCCESS;
    alSourcei(source, AL_BUFFER, 0);
    ASSERT_SUCCESS;
    alDeleteSources(1, &source);
    ASSERT_SUCCESS;
    alDeleteBuffers(OPTION(kSoundBuffers), buffer);
    ASSERT_SUCCESS;
    free(buffer);
    alcMakeContextCurrent(NULL);
    // Wine incorrectly returns ALC_INVALID_VALUE
    // and then fails the rest of these functions as well
    // so there will be a leak under Wine, but that's a bug in Wine, not
    // this code
    // ASSERT_SUCCESS;
    alcDestroyContext(context);
    // ASSERT_SUCCESS;
    alcCloseDevice(device);
    // ASSERT_SUCCESS;
    alGetError();  // reset error state
}

#ifdef LOGALL
void OpenAL::debugState() {
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

bool OpenAL::init(long sampleRate) {
    winlog("OpenAL::init\n");
    VBAM_CHECK(initialized == false);

    const wxString audio_device = OPTION(kSoundAudioDevice);
    if (!audio_device.empty()) {
        device = alcOpenDevice(audio_device.utf8_str());
        if (device == NULL) {
            // Might be the default device. Try again.
            OPTION(kSoundAudioDevice) = wxEmptyString;
            device = alcOpenDevice(NULL);
        }
    } else {
        device = alcOpenDevice(NULL);
    }

    if (!device) {
        wxLogError(_("OpenAL: Failed to open audio device"));
        return false;
    }

    context = alcCreateContext(device, NULL);
    VBAM_CHECK(context != NULL);
    ALCboolean retVal = alcMakeContextCurrent(context);
    VBAM_CHECK(ALC_TRUE == retVal);
    alGenBuffers(OPTION(kSoundBuffers), buffer);
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

void OpenAL::resume() {
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

void OpenAL::pause() {
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

void OpenAL::reset() {
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

void OpenAL::write(uint16_t* finalWave, int length) {
    (void)length;  // unused param
    if (!initialized)
        return;

    winlog("OpenAL::write\n");
    debugState();
    ALint sourceState = 0;
    ALint nBuffersProcessed = 0;

    if (!buffersLoaded) {
        // ==initial buffer filling==
        winlog(" initial buffer filling\n");

        for (int i = 0; i < OPTION(kSoundBuffers); i++) {
            // Filling the buffers explicitly with silence would be cleaner,
            // but the very first sample is usually silence anyway.
            alBufferData(buffer[i], AL_FORMAT_STEREO16, finalWave, soundBufferLen, freq);
            ASSERT_SUCCESS;
        }

        alSourceQueueBuffers(source, OPTION(kSoundBuffers), buffer);
        ASSERT_SUCCESS;
        buffersLoaded = true;
    } else {
        // ==normal buffer refreshing==
        nBuffersProcessed = 0;
        alGetSourcei(source, AL_BUFFERS_PROCESSED, &nBuffersProcessed);
        ASSERT_SUCCESS;

        if (nBuffersProcessed == OPTION(kSoundBuffers)) {
            // we only want to know about it when we are emulating at full speed or faster:
            if ((coreOptions.throttle >= 100) || (coreOptions.throttle == 0)) {
                if (systemVerbose & VERBOSE_SOUNDOUTPUT) {
                    static unsigned int i = 0;
                    log("OpenAL: Buffers were not refilled fast enough (i=%i)\n", i++);
                }
            }
        }

        if (!coreOptions.speedup && coreOptions.throttle && !gba_joybus_active) {
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

        VBAM_CHECK(nBuffersProcessed > 0);

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

}  // namespace

std::vector<AudioDevice> GetOpenALDevices() {
    std::vector<AudioDevice> devices;

#ifdef ALC_DEVICE_SPECIFIER

    if (alcIsExtensionPresent(NULL, "ALC_ENUMERATION_EXT") == AL_FALSE) {
        // this extension isn't critical to OpenAL operating
        return devices;
    }

    const char* devs = alcGetString(NULL, ALC_DEVICE_SPECIFIER);

    while (*devs) {
        const wxString device_name(devs, wxConvLibc);
        devices.push_back({device_name, device_name});
        devs += strlen(devs) + 1;
    }

#else

    devices.push_back({_("Default device"), wxEmptyString});

#endif

    return devices;
}

std::unique_ptr<SoundDriver> CreateOpenALDriver() {
    winlog("newOpenAL\n");
    return std::make_unique<OpenAL>();
}

}  // namespace internal
}  // namespace audio
