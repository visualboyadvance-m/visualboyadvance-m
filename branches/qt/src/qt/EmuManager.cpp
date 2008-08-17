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


#include "EmuManager.h"

//#include "../dmg/gb.h"


EmuManager::EmuManager()
: romBuffer( 0 )
, systemType( SYSTEM_UNKNOWN )
, romLoaded( false )
, emulating( false )
{
}


EmuManager::~EmuManager()
{
	if( romBuffer != 0 ) {
		free( romBuffer );
		romBuffer = 0;
	}
}


bool EmuManager::loadRom( const QString &filePath )
{
	if( 0 == filePath.compare( romPath ) ) {
		// we don't need to reload the ROM into romBuffer
		return true;
	}

	if( filePath.isEmpty() ) {
		unloadRom();
		return false;
	}

	romPath = filePath;
	romLoaded = false;
	
	// validate ROM
	if( romPath.isEmpty() ) return false;

	QFile file( romPath );

	if( !file.exists() ) return false;

	qint64 size = file.size();
	if( ( size == 0 ) || ( size > 0x2000000 /* 32MB */ ) ) return false;

	// TODO: add further validation


	// read ROM into memory
	if( !file.open( QIODevice::ReadOnly ) ) return false;

	if( romBuffer != 0 ) {
		// resize the buffer
		unsigned char *temp;
		temp = (unsigned char *)realloc( romBuffer, size );
		if( temp == 0 ) {
			free( romBuffer );
			romBuffer = 0;
			return false;
		} else {
			romBuffer = temp;
		}
	} else {
		// create the buffer
		romBuffer = (unsigned char *)malloc( size );
		if( romBuffer == 0 ) return false;
	}

	if( -1 == file.read( (char *)romBuffer, size ) ) return false;

	file.close();


	// determine system type
	QFileInfo fileInfo( romPath );
	QString suffix = fileInfo.suffix();

	systemType = SYSTEM_UNKNOWN;

	if( 0 == suffix.compare( "gb", Qt::CaseInsensitive ) ) {
		systemType = SYSTEM_GB;
	}

	if( 0 == suffix.compare( "gbc", Qt::CaseInsensitive ) ) {
		systemType = SYSTEM_GB;
	}

	if( 0 == suffix.compare( "gba", Qt::CaseInsensitive ) ) {
		systemType = SYSTEM_GBA;
	}


	// tell the core where to find the ROM data
	switch( systemType ) {
		case SYSTEM_UNKNOWN:
			return false;
			break;
		case SYSTEM_GB:
			//gbLoadRom();
			break;
		case SYSTEM_GBA:
			break;
	}


	romLoaded = true;
	return true;
}


void EmuManager::unloadRom()
{
	if( emulating ) {
		stopEmulation();
	}

	romPath.clear();
	romLoaded = false;

	if( romBuffer != 0 ) {
		free( romBuffer );
		romBuffer = 0;
	}
}


bool EmuManager::isRomLoaded()
{
	return romLoaded;
}


QString EmuManager::getRomPath()
{
	return romPath;
}


EmuManager::SYSTEM_TYPE EmuManager::getSystemType()
{
	return systemType;
}


bool EmuManager::startEmulation()
{
	if( emulating ) return true;

	switch( systemType ) {
		case SYSTEM_UNKNOWN:
			return false;
			break;
		case SYSTEM_GB:
			break;
		case SYSTEM_GBA:
			break;
	}

	emulating = true;
	return true;
}


void EmuManager::stopEmulation()
{
	emulating = false;
}


bool EmuManager::isEmulating()
{
	return emulating;
}
