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

#ifndef VBAM_SDL_AUDIO_SDL_H_
#define VBAM_SDL_AUDIO_SDL_H_

#ifndef ENABLE_SDL3
#include <SDL.h>
#else
#include <SDL3/SDL.h>
#endif

#include "core/base/ringbuffer.h"
#include "core/base/sound_driver.h"

class SoundSDL final : public SoundDriver {
public:
        SoundSDL();
        ~SoundSDL() override;

private:
#ifdef ENABLE_SDL3
        static void soundCallback(void* data, SDL_AudioStream *stream, int additional_length, int length);
#else
        static void soundCallback(void* data, uint8_t* stream, int len);
#endif

        bool should_wait();
        std::size_t buffer_size();
        void deinit();
        void read(uint16_t* stream, int length);

        // SoundDriver implementation.
        bool init(long sampleRate) override;
        void pause() override;
        void reset() override;
        void resume() override;
        void write(uint16_t *finalWave, int length) override;
        void setThrottle(unsigned short throttle_) override;

        RingBuffer<uint16_t> samples_buf;

        SDL_AudioDeviceID sound_device = 0;

#ifdef ENABLE_SDL3
        SDL_AudioStream *sound_stream = NULL;
        SDL_Mutex* mutex;
        SDL_Semaphore* data_available;
        SDL_Semaphore* data_read;
#else
        SDL_mutex* mutex;
        SDL_semaphore* data_available;
        SDL_semaphore* data_read;
#endif

        [[maybe_unused]] SDL_AudioSpec audio_spec;

        unsigned short current_rate;

        bool initialized = false;

        // Defines what delay in seconds we keep in the sound buffer
        static const double buftime;
};

#endif  // VBAM_SDL_AUDIO_SDL_H_
