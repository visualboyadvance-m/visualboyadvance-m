// VisualBoyAdvance - Nintendo Gameboy/GameboyAdvance (TM) emulator.
// Copyright (C) 2015 VBA-M development team

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or(at your option)
// any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#include <cmath>
#include <iostream>
#include "SoundSDL.h"
#include "ConfigManager.h"
#include "../gba/Globals.h"
#include "../gba/Sound.h"

extern int emulating;

// Hold up to 300 ms of data in the ring buffer
const double SoundSDL::buftime = 0.300;

SoundSDL::SoundSDL():
    samples_buf(0),
    sound_device(-1),
    current_rate(throttle),
    initialized(false)
{}

void SoundSDL::soundCallback(void* data, uint8_t* stream, int len) {
    reinterpret_cast<SoundSDL*>(data)->read(reinterpret_cast<uint16_t*>(stream), len);
}

bool SoundSDL::should_wait() {
    return emulating && !speedup && current_rate && !gba_joybus_active;
}

std::size_t SoundSDL::buffer_size() {
    SDL_LockMutex(mutex);
    std::size_t size = samples_buf.used();
    SDL_UnlockMutex(mutex);

    return size;
}

void SoundSDL::read(uint16_t* stream, int length) {
    if (length <= 0)
        return;

    SDL_memset(stream, audio_spec.silence, length);

    // if not initialzed, paused or shutting down, do nothing
    if (!initialized || !emulating)
        return;

    if (!buffer_size())
        if (should_wait())
            SDL_SemWait(data_available);
        else
            return;

    SDL_LockMutex(mutex);

    samples_buf.read(stream, std::min((std::size_t)(length / 2), samples_buf.used()));

    SDL_UnlockMutex(mutex);

    SDL_SemPost(data_read);
}

void SoundSDL::write(uint16_t * finalWave, int length) {
    if (!initialized)
        return;

    SDL_LockMutex(mutex);

    if (SDL_GetAudioDeviceStatus(sound_device) != SDL_AUDIO_PLAYING)
	SDL_PauseAudioDevice(sound_device, 0);

    unsigned int samples = length / 4;
    std::size_t avail;

    while ((avail = samples_buf.avail() / 2) < samples) {
	samples_buf.write(finalWave, avail * 2);

	finalWave += avail * 2;
	samples -= avail;

	SDL_UnlockMutex(mutex);

	SDL_SemPost(data_available);

	if (should_wait())
	    SDL_SemWait(data_read);
	else
	    // Drop the remainder of the audio data
	    return;

	SDL_LockMutex(mutex);
    }

    samples_buf.write(finalWave, samples * 2);

    SDL_UnlockMutex(mutex);
}


bool SoundSDL::init(long sampleRate) {
    if (initialized) deinit();

    // no sound on windows unless we do this
#ifdef _WIN32
    SDL_setenv("SDL_AUDIODRIVER", "directsound", true);
#endif

    SDL_AudioSpec audio;
    SDL_memset(&audio, 0, sizeof(audio));

    // for "no throttle" use regular rate, audio is just dropped
    audio.freq     = current_rate ? sampleRate * (current_rate / 100.0) : sampleRate;

    audio.format   = AUDIO_S16SYS;
    audio.channels = 2;
    audio.samples  = 2048;
    audio.callback = soundCallback;
    audio.userdata = this;

    if (!SDL_WasInit(SDL_INIT_AUDIO)) SDL_Init(SDL_INIT_AUDIO);

    sound_device = SDL_OpenAudioDevice(NULL, 0, &audio, &audio_spec, SDL_AUDIO_ALLOW_ANY_CHANGE);

    if(sound_device < 0) {
        std::cerr << "Failed to open audio: " << SDL_GetError() << std::endl;
        return false;
    }

    samples_buf.reset(std::ceil(buftime * sampleRate * 2));

    mutex          = SDL_CreateMutex();
    data_available = SDL_CreateSemaphore(0);
    data_read      = SDL_CreateSemaphore(1);

    return initialized = true;
}

void SoundSDL::deinit() {
    if (!initialized)
        return;

    initialized = false;

    SDL_LockMutex(mutex);
    int is_emulating = emulating;
    emulating = 0;
    SDL_SemPost(data_available);
    SDL_SemPost(data_read);
    SDL_UnlockMutex(mutex);

    SDL_Delay(100);

    SDL_DestroySemaphore(data_available);
    data_available = nullptr;
    SDL_DestroySemaphore(data_read);
    data_read      = nullptr;

    SDL_DestroyMutex(mutex);
    mutex = nullptr;

    SDL_CloseAudioDevice(sound_device);

    emulating = is_emulating;
}

SoundSDL::~SoundSDL() {
    deinit();
}

void SoundSDL::pause() {}
void SoundSDL::resume() {}

void SoundSDL::reset() {
    init(soundGetSampleRate());
}

void SoundSDL::setThrottle(unsigned short throttle_) {
    current_rate = throttle_;
    reset();
}
