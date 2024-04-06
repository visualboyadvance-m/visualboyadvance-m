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

#include <SDL.h>

#include "core/base/ringbuffer.h"
#include "core/base/sound_driver.h"

class SoundSDL final : public SoundDriver {
public:
        SoundSDL();
        ~SoundSDL() override;

private:
        static void soundCallback(void* data, uint8_t* stream, int length);
        void read(uint16_t* stream, int length);
        bool should_wait();
        std::size_t buffer_size();
        void deinit();

        // SoundDriver implementation.
        bool init(long sampleRate) override;
        void pause() override;
        void reset() override;
        void resume() override;
        void write(uint16_t *finalWave, int length) override;
        void setThrottle(unsigned short throttle_) override;

        RingBuffer<uint16_t> samples_buf;

        SDL_AudioDeviceID sound_device = 0;

        SDL_mutex* mutex;
        SDL_sem* data_available;
        SDL_sem* data_read;
        SDL_AudioSpec audio_spec;

        unsigned short current_rate;

        bool initialized = false;

        // Defines what delay in seconds we keep in the sound buffer
        static const double buftime;
};

#endif  // VBAM_SDL_AUDIO_SDL_H_
