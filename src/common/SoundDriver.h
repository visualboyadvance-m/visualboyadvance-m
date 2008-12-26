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
// Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

#ifndef __VBA_SOUND_DRIVER_H__
#define __VBA_SOUND_DRIVER_H__

class SoundDriver
{
 public:
  virtual ~SoundDriver() {};

  virtual bool init(int quality) = 0;
  virtual void pause() = 0;
  virtual void reset() = 0;
  virtual void resume() = 0;
  virtual void write(const u16 * finalWave, int length) = 0;
  virtual int getBufferLength() = 0;
};

#endif // __VBA_SOUND_DRIVER_H__
