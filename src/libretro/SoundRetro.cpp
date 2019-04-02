// VisualBoyAdvance - Nintendo Gameboy/GameboyAdvance (TM) emulator.
// Copyright (C) 2008 VBA-M development team

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

#include "SoundRetro.h"
#include "libretro.h"

extern retro_audio_sample_batch_t audio_batch_cb;

SoundRetro::SoundRetro()
{
}

void SoundRetro::write(uint16_t* finalWave, int length)
{
    const int16_t* wave = (const int16_t*)finalWave;
    int frames = length >> 1;
    audio_batch_cb(wave, frames);
}

bool SoundRetro::init(long sampleRate)
{
    (void)sampleRate; // unused param
    return true;
}

SoundRetro::~SoundRetro()
{
}

void SoundRetro::pause()
{
}

void SoundRetro::resume()
{
}

void SoundRetro::reset()
{
}

void SoundRetro::setThrottle(unsigned short throttle)
{
    (void)throttle; // unused param
}
