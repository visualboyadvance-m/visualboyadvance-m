// VBA-M, A Nintendo Handheld Console Emulator
// Copyright (C) 2008 VBA-M development team
//
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


// The EmuManager class is the interface to the core.
// All communications with the core MUST be handled by this class for cleaner code.


#ifndef EMUMANAGER_H
#define EMUMANAGER_H


#include "precompile.h"


class EmuManager
{
public:
	enum SYSTEM_TYPE {
		SYSTEM_UNKNOWN,
		SYSTEM_GB,
		SYSTEM_GBA
	};


public:
	EmuManager();
	~EmuManager();

	bool loadRom( const QString &filePath );
	void unloadRom();
	bool isRomLoaded();
	QString getRomPath();

	SYSTEM_TYPE getSystemType();

	bool startEmulation();
	void stopEmulation();
	bool isEmulating();


private:
	QString romPath;
	unsigned char *romBuffer;
	SYSTEM_TYPE systemType; // set by loadRom()
	bool romLoaded;
	bool emulating;
};


#endif // #ifndef EMUMANAGER_H
