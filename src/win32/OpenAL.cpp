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

#ifndef NO_OAL

#include "stdafx.h" // includes VBA.h for 'theApp.throttle'

// Interface
#include "Sound.h"

// OpenAL
#include <al.h>
#include <alc.h>
#pragma comment( lib, "OpenAL32.lib" )

// Windows
#include <windows.h> // for 'Sleep' function

// Internals
#include "../Sound.h"
#include "../Globals.h" // for 'speedup' and 'synchronize'

// Debug
#include <assert.h>


#define NBUFFERS 8
//#define LOGALL
// LOGALL writes very detailed informations to vba-trace.log

#ifndef LOGALL
#define winlog //
#define debugState() //
#endif

class OpenAL : public ISound
{
public:
	OpenAL();
	virtual ~OpenAL();

	bool init();   // initialize the primary and secondary sound buffer
	void pause();  // pause the secondary sound buffer
	void reset();  // stop and reset the secondary sound buffer
	void resume(); // resume the secondary sound buffer
	void write();  // write the emulated sound to the secondary sound buffer

private:
	bool           initialized;
	bool           buffersLoaded;
	ALCdevice     *device;
	ALCcontext    *context;
	ALuint         buffer[NBUFFERS];
	ALuint         tempBuffer;
	ALuint         source;
	int            freq;

#ifdef LOGALL
	void           debugState();
#endif
};


OpenAL::OpenAL()
{
	initialized = false;
	buffersLoaded = false;
	device = NULL;
	context = NULL;
	memset( buffer, 0, NBUFFERS * sizeof( ALuint ) );
	tempBuffer = 0;
	source = 0;
}


OpenAL::~OpenAL()
{
	alSourceStop( source );
	assert( AL_NO_ERROR == alGetError() );

	alSourcei( source, AL_BUFFER, 0 );
	assert( AL_NO_ERROR == alGetError() );

	alDeleteSources( 1, &source );
	assert( AL_NO_ERROR == alGetError() );

	alDeleteBuffers( NBUFFERS, buffer );
	assert( AL_NO_ERROR == alGetError() );

	alcMakeContextCurrent( NULL );
	assert( AL_NO_ERROR == alGetError() );

	alcDestroyContext( context );
	assert( AL_NO_ERROR == alGetError() );
	
	alcCloseDevice( device );
	assert( AL_NO_ERROR == alGetError() );
}

#ifdef LOGALL
void OpenAL::debugState()
{

	ALint value = 0;
	alGetSourcei( source, AL_SOURCE_STATE, &value );
	assert( AL_NO_ERROR == alGetError() );

	winlog( " soundPaused = %i\n", soundPaused );
	winlog( " Source:\n" );
	winlog( "  State: " );
	switch( value )
	{
	case AL_INITIAL:
		winlog( "AL_INITIAL\n" );
		break;
	case AL_PLAYING:
		winlog( "AL_PLAYING\n" );
		break;
	case AL_PAUSED:
		winlog( "AL_PAUSED\n" );
		break;
	case AL_STOPPED:
		winlog( "AL_STOPPED\n" );
		break;
	default:
		winlog( "!unknown!\n" );
		break;
	}


	alGetSourcei( source, AL_BUFFERS_QUEUED, &value );
	assert( AL_NO_ERROR == alGetError() );
	winlog( "  Buffers in queue: %i\n", value );

	alGetSourcei( source, AL_BUFFERS_PROCESSED, &value );
	assert( AL_NO_ERROR == alGetError() );
	winlog( "  Buffers processed: %i\n", value );
}
#endif


bool OpenAL::init()
{
	winlog( "OpenAL::init\n" );
	assert( initialized == false );

	if( theApp.oalDevice ) {
		device = alcOpenDevice( theApp.oalDevice );
	} else {
		device = alcOpenDevice( NULL );
	}
	assert( device != NULL );

	context = alcCreateContext( device, NULL );
	assert( context != NULL );

	ALCboolean retVal = alcMakeContextCurrent( context );
	assert( ALC_TRUE == retVal );

	alGenBuffers( NBUFFERS, buffer );
	assert( AL_NO_ERROR == alGetError() );

	alGenSources( 1, &source );
	assert( AL_NO_ERROR == alGetError() );

	freq = 44100 / soundQuality;
	// calculate the number of samples per frame first
	// then multiply it with the size of a sample frame (16 bit * stereo)
	soundBufferLen = ( freq / 60 ) * 4;


	setsystemSoundOn( true );
	initialized = true;
	return true;
}


void OpenAL::resume()
{
	winlog( "OpenAL::resume\n" );
	assert( initialized );
	if( !buffersLoaded ) return;
	debugState();


	ALint sourceState = 0;
	alGetSourcei( source, AL_SOURCE_STATE, &sourceState );
	assert( AL_NO_ERROR == alGetError() );
	if( sourceState != AL_PLAYING ) {
		alSourcePlay( source );
		assert( AL_NO_ERROR == alGetError() );
	}
	debugState();
}


void OpenAL::pause()
{
	winlog( "OpenAL::pause\n" );
	assert( initialized );
	if( !buffersLoaded ) return;
	debugState();


	ALint sourceState = 0;
	alGetSourcei( source, AL_SOURCE_STATE, &sourceState );
	assert( AL_NO_ERROR == alGetError() );
	if( sourceState == AL_PLAYING ) {
		alSourcePause( source );
		assert( AL_NO_ERROR == alGetError() );
	}
	debugState();
}


void OpenAL::reset()
{
	winlog( "OpenAL::reset\n" );
	assert( initialized );
	if( !buffersLoaded ) return;
	debugState();

	ALint sourceState = 0;
	alGetSourcei( source, AL_SOURCE_STATE, &sourceState );
	assert( AL_NO_ERROR == alGetError() );
	if( sourceState != AL_STOPPED ) {
		alSourceStop( source );
		assert( AL_NO_ERROR == alGetError() );
	}
	debugState();
}


void OpenAL::write()
{
	winlog( "OpenAL::write\n" );
	assert( initialized );

	debugState();

	ALint sourceState = 0;
	ALint nBuffersProcessed = 0;

	if( !buffersLoaded ) {
		// ==initial buffer filling==
		winlog( " initial buffer filling\n" );
		for( int i = 0 ; i < NBUFFERS ; i++ ) {
			// filling the buffers explicitly with silence would be cleaner...
			alBufferData( buffer[i], AL_FORMAT_STEREO16, soundFinalWave, soundBufferLen, freq );
			assert( AL_NO_ERROR == alGetError() );
		}

		alSourceQueueBuffers( source, NBUFFERS, buffer );
		assert( AL_NO_ERROR == alGetError() );

		buffersLoaded = true;
	} else {
		// ==normal buffer refreshing==
		nBuffersProcessed = 0;
		alGetSourcei( source, AL_BUFFERS_PROCESSED, &nBuffersProcessed );
		assert( AL_NO_ERROR == alGetError() );

		if( !speedup && synchronize && !theApp.throttle ) {
			// wait until at least one buffer has finished
			while( nBuffersProcessed == 0 ) {
				winlog( " waiting...\n" );
				// wait for about half the time one buffer needs to finish
				// unoptimized: ( sourceBufferLen * 1000 ) / ( freq * 2 * 2 ) * 1/2
				Sleep( soundBufferLen / ( freq >> 7 ) );
				alGetSourcei( source, AL_BUFFERS_PROCESSED, &nBuffersProcessed );
				assert( AL_NO_ERROR == alGetError() );
			}
		} else {
			if( nBuffersProcessed == 0 ) return;
		}
		
		assert( nBuffersProcessed > 0 );

		// tempBuffer contains the Buffer ID for the unqueued Buffer
		tempBuffer = 0;
		alSourceUnqueueBuffers( source, 1, &tempBuffer );
		assert( AL_NO_ERROR == alGetError() );

		alBufferData( tempBuffer, AL_FORMAT_STEREO16, soundFinalWave, soundBufferLen, freq );
		assert( AL_NO_ERROR == alGetError() );

		// refill buffer
		alSourceQueueBuffers( source, 1, &tempBuffer );
		assert( AL_NO_ERROR == alGetError() );
	}

	// start playing the source if necessary
	alGetSourcei( source, AL_SOURCE_STATE, &sourceState );
	assert( AL_NO_ERROR == alGetError() );
	if( !soundPaused && ( sourceState != AL_PLAYING ) ) {
		alSourcePlay( source );
		assert( AL_NO_ERROR == alGetError() );
	}
}


ISound *newOpenAL()
{
	winlog( "newOpenAL\n" );
	return new OpenAL();
}

#endif