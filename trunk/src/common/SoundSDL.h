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

#ifndef __VBA_SOUND_SDL_H__
#define __VBA_SOUND_SDL_H__

#include "SoundDriver.h"

class SoundSDL: public SoundDriver
{
public:
	virtual ~SoundSDL();

	virtual bool init(long sampleRate);
	virtual void pause();
	virtual void reset();
	virtual void resume();
	virtual void write(const u16 * finalWave, int length);
	virtual int getBufferLength();

private:
	static const  int  _sampleCount    = 4096;
	static const  int  _bufferAlign    = 4;
	static const  int  _bufferCapacity = _sampleCount * 2;
	static const  int  _bufferTotalLen = _bufferCapacity + _bufferAlign;

	static u8          _buffer[_bufferTotalLen];
	static int         _readPosition;
	static int         _writePosition;
	static SDL_cond  * _cond;
	static SDL_mutex * _mutex;
	int                _bufferLen;

	static int getBufferFree();
	static int getBufferUsed();
	static void soundCallback(void *, u8 *stream, int len);
};

#endif // __VBA_SOUND_SDL_H__
