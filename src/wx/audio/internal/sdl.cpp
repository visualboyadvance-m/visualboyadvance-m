#include "wx/audio/internal/sdl.h"

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

#ifdef _WIN32
#include <windows.h>
#include <versionhelpers.h>
#endif

#ifdef ENABLE_SDL3
#include <SDL3/SDL.h>
#else
#include <SDL.h>
#endif

#include <wx/arrstr.h>
#include <wx/log.h>
#include <wx/translation.h>
#include <wx/utils.h>

#include "core/base/sound_driver.h"
#include "core/base/check.h"
#include "core/gba/gbaGlobals.h"
#include "core/gba/gbaSound.h"
#include "wx/config/option-proxy.h"

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

extern int emulating;

namespace audio {
namespace internal {

namespace {

class SDLAudio : public SoundDriver {
public:
    SDLAudio();
    ~SDLAudio() override;
    
    bool init(long sampleRate) override;                  // initialize the sound buffer queue
    void deinit();
    void setThrottle(unsigned short throttle_) override;  // set game speed
    void pause() override;                                // pause the secondary sound buffer
    void reset() override;   // stop and reset the secondary sound buffer
    void resume() override;  // play/resume the secondary sound buffer
    void write(uint16_t* finalWave, int length) override;  // write the emulated sound to a sound buffer

private:
    SDL_AudioDeviceID sound_device = 0;
    SDL_AudioSpec audio;

#ifdef ENABLE_SDL3
    SDL_AudioStream *sound_stream = NULL;
    SDL_Mutex* mutex;
#else
    SDL_mutex* mutex;
#endif

#ifdef ENABLE_SDL3
    SDL_AudioDeviceID *sdl_devices;
    int sdl_devices_count = 0;
#else
    unsigned short current_rate;
#endif
    
    bool initialized = false;
};

SDLAudio::SDLAudio():
sound_device(0),
#ifndef ENABLE_SDL3
current_rate(static_cast<unsigned short>(coreOptions.throttle)),
#endif
initialized(false)
{}

void SDLAudio::deinit() {
    if (!initialized)
        return;
    
    initialized = false;
    
    SDL_LockMutex(mutex);
    int is_emulating = emulating;
    emulating = 0;
    SDL_UnlockMutex(mutex);
    
    SDL_DestroyMutex(mutex);
    mutex = NULL;
    
    SDL_CloseAudioDevice(sound_device);

    emulating = is_emulating;
}

SDLAudio::~SDLAudio() {
    deinit();
}

bool SDLAudio::init(long sampleRate) {
#ifdef ENABLE_SDL3

#ifdef _WIN32
    // On Windows XP, use the winmm audio driver instead of the default wasapi driver.
    if (!IsWindowsVistaOrGreater()) {
        SDL_SetHint("SDL_AUDIODRIVER", "winmm");
    }
#endif

    int current_device = SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK;
    sdl_devices = SDL_GetAudioPlaybackDevices(&sdl_devices_count);
    const char *devs = NULL;
#endif

    winlog("SDLAudio::init\n");
    if (initialized) deinit();
    
    SDL_memset(&audio, 0, sizeof(audio));
    
#ifdef ENABLE_SDL3
    // for "no throttle" use regular rate, audio is just dropped
    audio.freq     = sampleRate;
    
    audio.format   = SDL_AUDIO_S16;
#else
    // for "no throttle" use regular rate, audio is just dropped
    audio.freq     = current_rate ? static_cast<int>(sampleRate * ((float)current_rate / 100.0)) : sampleRate;
    
    audio.format   = AUDIO_S16SYS;
#endif
    
    audio.channels = 2;

#ifdef ENABLE_SDL3
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) == false) {
#else
    audio.samples  = 2048;
    audio.callback = NULL;
    audio.userdata = NULL;

    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
#endif
        return false;
    }

#ifdef ENABLE_SDL3
    if (SDL_Init(SDL_INIT_AUDIO) == false) {
#else
    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
#endif
        return false;
    }
    
#ifdef ENABLE_SDL3
#ifdef ONLY_DEFAULT_AUDIO_DEVICE
    sound_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &audio, NULL, NULL);
#else
    for (int i = 0; i < sdl_devices_count; i++) {
        devs = SDL_GetAudioDeviceName(sdl_devices[i]);
        const wxString device_name(devs, wxConvLibc);

        if (device_name == OPTION(kSoundAudioDevice))
        {
            current_device = i;
            break;
        }
    }

    if (OPTION(kSoundAudioDevice) == _("Default device")) {
        sound_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &audio, NULL, NULL);
    } else {
        sound_stream = SDL_OpenAudioDeviceStream(sdl_devices[current_device], &audio, NULL, NULL);
    }

    if(sound_stream == NULL) {
        return false;
    }
        
    sound_device = SDL_GetAudioStreamDevice(sound_stream);
#endif
#else
#ifdef ONLY_DEFAULT_AUDIO_DEVICE
    sound_device = SDL_OpenAudioDevice(NULL, 0, &audio, NULL, 0);
#else
    const wxString device_name = OPTION(kSoundAudioDevice);

    if (device_name == _("Default device")) {
        sound_device = SDL_OpenAudioDevice(NULL, 0, &audio, NULL, 0);
    } else {
        sound_device = SDL_OpenAudioDevice(device_name.mb_str(), 0, &audio, NULL, 0);
    }
#endif
#endif
    
    if(sound_device == 0) {
        return false;
    }
    
    mutex = SDL_CreateMutex();
    
    // turn off audio events because we are not processing them
#ifdef ENABLE_SDL3
    SDL_SetEventEnabled(SDL_EVENT_AUDIO_DEVICE_ADDED, false);
    SDL_SetEventEnabled(SDL_EVENT_AUDIO_DEVICE_REMOVED, false);
#elif SDL_VERSION_ATLEAST(2, 0, 4)
    SDL_EventState(SDL_AUDIODEVICEADDED,   SDL_IGNORE);
    SDL_EventState(SDL_AUDIODEVICEREMOVED, SDL_IGNORE);
#endif

    return initialized = true;
}

void SDLAudio::setThrottle(unsigned short throttle_) {
    if (!initialized)
        return;

    if (throttle_ == 0)
        throttle_ = 450;

#ifdef ENABLE_SDL3
    SDL_SetAudioStreamFrequencyRatio(sound_stream, (float)throttle_ / 100.0f);
#else
    current_rate = throttle_;

    reset();
#endif
}

void SDLAudio::resume() {
    if (!initialized)
        return;
    
    winlog("SDLAudio::resume\n");

#ifdef ENABLE_SDL3
    if (SDL_AudioDevicePaused(sound_device) == true) {
        SDL_ResumeAudioStreamDevice(sound_stream);
        SDL_ResumeAudioDevice(sound_device);
    }
#else
    if (SDL_GetAudioDeviceStatus(sound_device) != SDL_AUDIO_PLAYING) {
        SDL_PauseAudioDevice(sound_device, 0);
    }
#endif
}

void SDLAudio::pause() {
    if (!initialized)
        return;
    
    winlog("SDLAudio::pause\n");
    
#ifdef ENABLE_SDL3
    if (SDL_AudioDevicePaused(sound_device) == true) {
        SDL_PauseAudioStreamDevice(sound_stream);
        SDL_PauseAudioDevice(sound_device);
    }
#else
    if (SDL_GetAudioDeviceStatus(sound_device) != SDL_AUDIO_PLAYING) {
        SDL_PauseAudioDevice(sound_device, 1);
    }
#endif
}

void SDLAudio::reset() {
    if (!initialized)
        return;
    
    winlog("SDLAudio::reset\n");
    
    init(soundGetSampleRate());
}

void SDLAudio::write(uint16_t* finalWave, int length) {
    int res = 0;

    if (!initialized)
        return;
    
    SDL_LockMutex(mutex);

#ifdef ENABLE_SDL3
    if (SDL_AudioDevicePaused(sound_device) == true) {
        SDL_ResumeAudioStreamDevice(sound_stream);
        SDL_ResumeAudioDevice(sound_device);
    }
#else
    if (SDL_GetAudioDeviceStatus(sound_device) != SDL_AUDIO_PLAYING) {
        SDL_PauseAudioDevice(sound_device, 0);
    }
#endif

#ifdef ENABLE_SDL3
    res = (int)SDL_PutAudioStreamData(sound_stream, finalWave, length) == true;

    while (res && ((size_t)SDL_GetAudioStreamQueued(sound_stream) > (size_t)(2048 * audio.channels * sizeof(uint16_t)))) {
        SDL_Delay(1);
    }
#else
    res = SDL_QueueAudio(sound_device, finalWave, length) == 0;

    while (res && ((size_t)SDL_GetQueuedAudioSize(sound_device) > (size_t)(audio.samples * audio.channels * sizeof(uint16_t)))) {
        SDL_Delay(1);
    }
#endif

    winlog("SDL audio queue result: %d\n", res);

    SDL_UnlockMutex(mutex);
}

}  // namespace

std::vector<AudioDevice> GetSDLDevices() {
    std::vector<AudioDevice> devices;

#ifdef ONLY_DEFAULT_AUDIO_DEVICE
    devices.push_back({_("Default device"), wxEmptyString});
#else
#ifdef ENABLE_SDL3
    const char *devs = NULL;
    SDL_AudioDeviceID *sdl_devices = NULL;
    int sdl_devices_count = 0;

    SDL_InitSubSystem(SDL_INIT_AUDIO);

    sdl_devices = SDL_GetAudioPlaybackDevices(&sdl_devices_count);

    devices.push_back({_("Default device"), _("Default device")});
    
    for (int i = 0; i < sdl_devices_count; i++)
    {
        devs = SDL_GetAudioDeviceName(sdl_devices[i]);

        if (devs != NULL)
        {
            const wxString device_name(devs, wxConvLibc);
            devices.push_back({device_name, device_name});
        }
    }
#else
    const char *devs = NULL;
    int sdl_devices_count = 0;

    SDL_InitSubSystem(SDL_INIT_AUDIO);

    sdl_devices_count = SDL_GetNumAudioDevices(0);

    devices.push_back({_("Default device"), _("Default device")});

    for (int i = 0; i < sdl_devices_count; i++)
    {
        devs = SDL_GetAudioDeviceName(i, 0);
        const wxString device_name(devs, wxConvLibc);
        devices.push_back({device_name, device_name});
    }
#endif
#endif

    return devices;
}

std::unique_ptr<SoundDriver> CreateSDLDriver() {
    winlog("newSDL\n");
    return std::make_unique<SDLAudio>();
}

}  // namespace internal
}  // namespace audio
