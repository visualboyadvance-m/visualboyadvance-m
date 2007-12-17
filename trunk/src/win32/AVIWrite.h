// VisualBoyAdvance - Nintendo Gameboy/GameboyAdvance (TM) emulator.
// Copyright (C) 2007 VBA-M development team

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


#include "stdafx.h"
#include <vfw.h>


// info: recreate the whole AVIWrite object if any method fails
class AVIWrite
{
public:
	AVIWrite();
	virtual ~AVIWrite();

	bool CreateAVIFile( LPCTSTR filename );
	bool CreateVideoStream( LONG imageWidth, LONG imageHeight, WORD colorBits, DWORD framesPerSecond, HWND parentWnd );
	bool CreateAudioStream( WORD channelCount, DWORD sampleRate, WORD sampleBits, HWND parentWnd );
	bool AddVideoFrame( LPVOID imageData );
	bool AddAudioFrame( LPVOID soundData );

private:
	bool m_failed;
	PAVIFILE m_file;
	PAVISTREAM m_videoStream;
	PAVISTREAM m_audioStream;
	AVICOMPRESSOPTIONS m_videoCompSettings;
	AVICOMPRESSOPTIONS m_audioCompSettings;
	PAVISTREAM m_videoCompressed;
	PAVISTREAM m_audioCompressed;
	DWORD m_frameRate;
	LONG m_frameCounter;
	LONG m_sampleCounter;
	LONG m_videoFrameSize;
	LONG m_audioFrameSize;
	WORD m_audioBlockAlign;
};
