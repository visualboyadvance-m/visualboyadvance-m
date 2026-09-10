#include "qt/audio/internal/sdl.h"

// === LOGALL writes very detailed informations to stderr ===
// #define LOGALL

#ifdef _WIN32
#include <windows.h>
#include <versionhelpers.h>
#endif

#ifdef ENABLE_SDL3
#include <SDL3/SDL.h>
#else
#include <SDL.h>
#endif

#include <QString>

#include "core/base/check.h"
#include "core/base/sound_driver.h"
#include "core/gba/gbaGlobals.h"
#include "core/gba/gbaSound.h"
#include "qt/config/option-proxy.h"

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
#else
#include <cstdio>
#define winlog(...) fprintf(stderr, __VA_ARGS__)
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
    SDL_AudioStream* sound_stream = nullptr;
    SDL_Mutex* mutex = nullptr;
    SDL_AudioDeviceID* sdl_devices = nullptr;
    int sdl_devices_count = 0;
#else
    SDL_mutex* mutex = nullptr;
    unsigned short current_rate;
#endif

    bool initialized = false;
};

SDLAudio::SDLAudio()
    : sound_device(0),
#ifndef ENABLE_SDL3
      current_rate(static_cast<unsigned short>(coreOptions.throttle)),
#endif
      initialized(false) {
}

void SDLAudio::deinit() {
    if (!initialized)
        return;

    initialized = false;

    SDL_LockMutex(mutex);
    int is_emulating = emulating;
    emulating = 0;
    SDL_UnlockMutex(mutex);

    SDL_DestroyMutex(mutex);
    mutex = nullptr;

#ifdef ENABLE_SDL3
    if (sound_stream) {
        // Closing the stream also closes the device it was bound to.
        SDL_DestroyAudioStream(sound_stream);
        sound_stream = nullptr;
    }
    if (sdl_devices) {
        SDL_free(sdl_devices);
        sdl_devices = nullptr;
        sdl_devices_count = 0;
    }
#else
    SDL_CloseAudioDevice(sound_device);
#endif
    sound_device = 0;

    emulating = is_emulating;
}

SDLAudio::~SDLAudio() {
    deinit();
}

bool SDLAudio::init(long sampleRate) {
#ifdef _WIN32
    // On Windows XP, use the winmm audio driver instead of the default wasapi driver.
    if (!IsWindowsVistaOrGreater()) {
        SDL_SetHint("SDL_AUDIODRIVER", "winmm");
    }
#endif

    winlog("SDLAudio::init\n");
    if (initialized)
        deinit();

    SDL_memset(&audio, 0, sizeof(audio));

#ifdef ENABLE_SDL3
    // for "no throttle" use regular rate, audio is just dropped
    audio.freq = static_cast<int>(sampleRate);
    audio.format = SDL_AUDIO_S16;
#else
    // for "no throttle" use regular rate, audio is just dropped
    audio.freq = current_rate ? static_cast<int>(sampleRate * ((float)current_rate / 100.0))
                              : static_cast<int>(sampleRate);
    audio.format = AUDIO_S16SYS;
#endif

    audio.channels = 2;

#ifdef ENABLE_SDL3
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) == false) {
#else
    audio.samples = 2048;
    audio.callback = nullptr;
    audio.userdata = nullptr;

    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
#endif
        return false;
    }

    const QString device_name = OPTION(kSoundAudioDevice);
    const bool use_default_device =
        device_name.isEmpty() || device_name == audio::DefaultDeviceName();

#ifdef ENABLE_SDL3
    SDL_AudioDeviceID current_device = SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK;
    sdl_devices = SDL_GetAudioPlaybackDevices(&sdl_devices_count);

    if (!use_default_device && sdl_devices) {
        for (int i = 0; i < sdl_devices_count; i++) {
            const char* devs = SDL_GetAudioDeviceName(sdl_devices[i]);
            if (devs && device_name == QString::fromUtf8(devs)) {
                current_device = sdl_devices[i];
                break;
            }
        }
    }

    sound_stream = SDL_OpenAudioDeviceStream(current_device, &audio, nullptr, nullptr);

    if (sound_stream == nullptr) {
        return false;
    }

    sound_device = SDL_GetAudioStreamDevice(sound_stream);
#else
    if (use_default_device) {
        sound_device = SDL_OpenAudioDevice(nullptr, 0, &audio, nullptr, 0);
    } else {
        sound_device = SDL_OpenAudioDevice(device_name.toUtf8().constData(), 0, &audio, nullptr, 0);
    }
#endif

    if (sound_device == 0) {
        return false;
    }

    mutex = SDL_CreateMutex();

    // turn off audio events because we are not processing them
#ifdef ENABLE_SDL3
    SDL_SetEventEnabled(SDL_EVENT_AUDIO_DEVICE_ADDED, false);
    SDL_SetEventEnabled(SDL_EVENT_AUDIO_DEVICE_REMOVED, false);
#elif SDL_VERSION_ATLEAST(2, 0, 4)
    SDL_EventState(SDL_AUDIODEVICEADDED, SDL_IGNORE);
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
    if (SDL_AudioDevicePaused(sound_device) == false) {
        SDL_PauseAudioStreamDevice(sound_stream);
        SDL_PauseAudioDevice(sound_device);
    }
#else
    if (SDL_GetAudioDeviceStatus(sound_device) == SDL_AUDIO_PLAYING) {
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
    res = SDL_PutAudioStreamData(sound_stream, finalWave, length) == true;

    while (res && ((size_t)SDL_GetAudioStreamQueued(sound_stream) >
                   (size_t)(2048 * audio.channels * sizeof(uint16_t)))) {
        SDL_Delay(1);
    }
#else
    res = SDL_QueueAudio(sound_device, finalWave, length) == 0;

    while (res && ((size_t)SDL_GetQueuedAudioSize(sound_device) >
                   (size_t)(audio.samples * audio.channels * sizeof(uint16_t)))) {
        SDL_Delay(1);
    }
#endif

    winlog("SDL audio queue result: %d\n", res);

    SDL_UnlockMutex(mutex);
}

}  // namespace

std::vector<AudioDevice> GetSDLDevices() {
    std::vector<AudioDevice> devices;

    SDL_InitSubSystem(SDL_INIT_AUDIO);

    devices.push_back({audio::DefaultDeviceName(), audio::DefaultDeviceName()});

#ifdef ENABLE_SDL3
    int sdl_devices_count = 0;
    SDL_AudioDeviceID* sdl_devices = SDL_GetAudioPlaybackDevices(&sdl_devices_count);

    if (sdl_devices) {
        for (int i = 0; i < sdl_devices_count; i++) {
            const char* devs = SDL_GetAudioDeviceName(sdl_devices[i]);
            if (devs != nullptr) {
                const QString device_name = QString::fromUtf8(devs);
                devices.push_back({device_name, device_name});
            }
        }
        SDL_free(sdl_devices);
    }
#else
    const int sdl_devices_count = SDL_GetNumAudioDevices(0);

    for (int i = 0; i < sdl_devices_count; i++) {
        const char* devs = SDL_GetAudioDeviceName(i, 0);
        if (devs != nullptr) {
            const QString device_name = QString::fromUtf8(devs);
            devices.push_back({device_name, device_name});
        }
    }
#endif

    return devices;
}

std::unique_ptr<SoundDriver> CreateSDLDriver() {
    winlog("newSDL\n");
    return std::make_unique<SDLAudio>();
}

}  // namespace internal
}  // namespace audio
