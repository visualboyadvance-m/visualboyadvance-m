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

#ifndef __VBA_SOUND_SDL_H__
#define __VBA_SOUND_SDL_H__

#include "ringbuffer.h"
#include "SoundDriver.h"

#include "SDL.h"

class SoundSDL : public SoundDriver {
public:
        SoundSDL();
        virtual ~SoundSDL();

        virtual bool init(long sampleRate);
        virtual void pause();
        virtual void reset();
        virtual void resume();
        virtual void write(uint16_t *finalWave, int length);
        virtual void setThrottle(unsigned short throttle_);

protected:
        static void soundCallback(void* data, uint8_t* stream, int length);
        virtual void read(uint16_t* stream, int length);
        virtual bool should_wait();
        virtual std::size_t buffer_size();
        virtual void deinit();

private:
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

#endif // __VBA_SOUND_SDL_H__
