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

#ifndef VBAM_CORE_BASE_SOUND_DRIVER_H_
#define VBAM_CORE_BASE_SOUND_DRIVER_H_

#include <cstdint>

// Sound driver abstract interface for the core to use to output sound.
// Subclass this to implement a new sound driver.
class SoundDriver {
public:
    virtual ~SoundDriver() = default;

    // Initialize the sound driver. `sampleRate` in Hertz.
    // Returns true if the driver was successfully initialized.
    virtual bool init(long sampleRate) = 0;

    // Pause the sound driver.
    virtual void pause() = 0;

    // Reset the sound driver.
    virtual void reset() = 0;

    // Resume the sound driver, following a pause.
    virtual void resume() = 0;

    // Write length bytes of data from the finalWave buffer to the driver output buffer.
    virtual void write(uint16_t* finalWave, int length) = 0;

    virtual void setThrottle(unsigned short throttle) = 0;
};

#endif  // VBAM_CORE_BASE_SOUND_DRIVER_H_
