// VisualBoyAdvance - Nintendo Gameboy/GameboyAdvance (TM) emulator.
// Copyright (C) 2007-2008 VBA-M development team

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


// === LOGALL writes very detailed informations to vba-trace.log ===
//#define LOGALL


#include "stdafx.h" // includes VBA.h for 'theApp.throttle'

// Interface
#include "Sound.h"

// OpenAL
#include <al.h>
#include <alc.h>
#include "LoadOAL.h"

// Windows
#include <windows.h> // for 'Sleep' function

// Internals
#include "../Sound.h"
#include "../Globals.h" // for 'speedup' and 'synchronize'

// Debug
#include <assert.h>

#ifndef LOGALL
// replace logging functions with comments
#define winlog //
#define debugState() //
#endif

class OpenAL : public ISound
{
public:
	OpenAL();
	virtual ~OpenAL();

	bool init();   // initialize the sound buffer queue
	void pause();  // pause the secondary sound buffer
	void reset();  // stop and reset the secondary sound buffer
	void resume(); // play/resume the secondary sound buffer
	void write();  // write the emulated sound to a sound buffer

private:
	OPENALFNTABLE  ALFunction;
	bool           initialized;
	bool           buffersLoaded;
	ALCdevice     *device;
	ALCcontext    *context;
	ALuint        *buffer;
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
	buffer = (ALuint*)malloc( theApp.oalBufferCount * sizeof( ALuint ) );
	memset( buffer, 0, theApp.oalBufferCount * sizeof( ALuint ) );
	tempBuffer = 0;
	source = 0;
}


OpenAL::~OpenAL()
{
	if( !initialized ) return;

	ALFunction.alSourceStop( source );
	assert( AL_NO_ERROR == ALFunction.alGetError() );

	ALFunction.alSourcei( source, AL_BUFFER, 0 );
	assert( AL_NO_ERROR == ALFunction.alGetError() );

	ALFunction.alDeleteSources( 1, &source );
	assert( AL_NO_ERROR == ALFunction.alGetError() );

	ALFunction.alDeleteBuffers( theApp.oalBufferCount, buffer );
	assert( AL_NO_ERROR == ALFunction.alGetError() );

	free( buffer );

	ALFunction.alcMakeContextCurrent( NULL );
	assert( AL_NO_ERROR == ALFunction.alGetError() );

	ALFunction.alcDestroyContext( context );
	assert( AL_NO_ERROR == ALFunction.alGetError() );
	
	ALFunction.alcCloseDevice( device );
	assert( AL_NO_ERROR == ALFunction.alGetError() );
}

#ifdef LOGALL
void OpenAL::debugState()
{

	ALint value = 0;
	ALFunction.alGetSourcei( source, AL_SOURCE_STATE, &value );
	assert( AL_NO_ERROR == ALFunction.alGetError() );

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


	ALFunction.alGetSourcei( source, AL_BUFFERS_QUEUED, &value );
	assert( AL_NO_ERROR == ALFunction.alGetError() );
	winlog( "  Buffers in queue: %i\n", value );

	ALFunction.alGetSourcei( source, AL_BUFFERS_PROCESSED, &value );
	assert( AL_NO_ERROR == ALFunction.alGetError() );
	winlog( "  Buffers processed: %i\n", value );
}
#endif


bool OpenAL::init()
{
	winlog( "OpenAL::init\n" );
	assert( initialized == false );

	if( !LoadOAL10Library( NULL, &ALFunction ) ) {
		systemMessage( IDS_OAL_NODLL, "OpenAL32.dll could not be found on your system. Please install the runtime from http://openal.org" );
		return false;
	}

	if( theApp.oalDevice ) {
		device = ALFunction.alcOpenDevice( theApp.oalDevice );
	} else {
		device = ALFunction.alcOpenDevice( NULL );
	}
	assert( device != NULL );

	context = ALFunction.alcCreateContext( device, NULL );
	assert( context != NULL );

	ALCboolean retVal = ALFunction.alcMakeContextCurrent( context );
	assert( ALC_TRUE == retVal );

	ALFunction.alGenBuffers( theApp.oalBufferCount, buffer );
	assert( AL_NO_ERROR == ALFunction.alGetError() );

	ALFunction.alGenSources( 1, &source );
	assert( AL_NO_ERROR == ALFunction.alGetError() );

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
	if( !initialized ) return;
	winlog( "OpenAL::resume\n" );
	if( !buffersLoaded ) return;
	debugState();


	ALint sourceState = 0;
	ALFunction.alGetSourcei( source, AL_SOURCE_STATE, &sourceState );
	assert( AL_NO_ERROR == ALFunction.alGetError() );
	if( sourceState != AL_PLAYING ) {
		ALFunction.alSourcePlay( source );
		assert( AL_NO_ERROR == ALFunction.alGetError() );
	}
	debugState();
}


void OpenAL::pause()
{
	if( !initialized ) return;
	winlog( "OpenAL::pause\n" );
	if( !buffersLoaded ) return;
	debugState();


	ALint sourceState = 0;
	ALFunction.alGetSourcei( source, AL_SOURCE_STATE, &sourceState );
	assert( AL_NO_ERROR == ALFunction.alGetError() );
	if( sourceState == AL_PLAYING ) {
		ALFunction.alSourcePause( source );
		assert( AL_NO_ERROR == ALFunction.alGetError() );
	}
	debugState();
}


void OpenAL::reset()
{
	if( !initialized ) return;
	winlog( "OpenAL::reset\n" );
	if( !buffersLoaded ) return;
	debugState();

	ALint sourceState = 0;
	ALFunction.alGetSourcei( source, AL_SOURCE_STATE, &sourceState );
	assert( AL_NO_ERROR == ALFunction.alGetError() );
	if( sourceState != AL_STOPPED ) {
		ALFunction.alSourceStop( source );
		assert( AL_NO_ERROR == ALFunction.alGetError() );
	}
	debugState();
}


void OpenAL::write()
{
	if( !initialized ) return;
	winlog( "OpenAL::write\n" );

	debugState();

	ALint sourceState = 0;
	ALint nBuffersProcessed = 0;

	if( !buffersLoaded ) {
		// ==initial buffer filling==
		winlog( " initial buffer filling\n" );
		for( int i = 0 ; i < theApp.oalBufferCount ; i++ ) {
			// Filling the buffers explicitly with silence would be cleaner,
			// but the very first sample is usually silence anyway.
			ALFunction.alBufferData( buffer[i], AL_FORMAT_STEREO16, soundFinalWave, soundBufferLen, freq );
			assert( AL_NO_ERROR == ALFunction.alGetError() );
		}

		ALFunction.alSourceQueueBuffers( source, theApp.oalBufferCount, buffer );
		assert( AL_NO_ERROR == ALFunction.alGetError() );

		buffersLoaded = true;
	} else {
		// ==normal buffer refreshing==
		nBuffersProcessed = 0;
		ALFunction.alGetSourcei( source, AL_BUFFERS_PROCESSED, &nBuffersProcessed );
		assert( AL_NO_ERROR == ALFunction.alGetError() );

		if( nBuffersProcessed == theApp.oalBufferCount ) {
			if( ( theApp.throttle >= 100 ) || ( theApp.throttle == 0 ) ) {
				// we only want to know about it when we are emulating at full speed (or faster)
				static int i = 0;
				log( "OpenAL: Buffers were not refilled fast enough (%i)\n", i++ );
			}
		}

		if( !speedup && synchronize && !theApp.throttle ) {
			// wait until at least one buffer has finished
			while( nBuffersProcessed == 0 ) {
				winlog( " waiting...\n" );
				// wait for about half the time one buffer needs to finish
				// unoptimized: ( sourceBufferLen * 1000 ) / ( freq * 2 * 2 ) * 1/2
				Sleep( soundBufferLen / ( freq >> 7 ) );
				ALFunction.alGetSourcei( source, AL_BUFFERS_PROCESSED, &nBuffersProcessed );
				assert( AL_NO_ERROR == ALFunction.alGetError() );
			}
		} else {
			if( nBuffersProcessed == 0 ) return;
		}
		
		assert( nBuffersProcessed > 0 );

		// tempBuffer contains the Buffer ID for the unqueued Buffer
		tempBuffer = 0;
		ALFunction.alSourceUnqueueBuffers( source, 1, &tempBuffer );
		assert( AL_NO_ERROR == ALFunction.alGetError() );

		ALFunction.alBufferData( tempBuffer, AL_FORMAT_STEREO16, soundFinalWave, soundBufferLen, freq );
		assert( AL_NO_ERROR == ALFunction.alGetError() );

		// refill buffer
		ALFunction.alSourceQueueBuffers( source, 1, &tempBuffer );
		assert( AL_NO_ERROR == ALFunction.alGetError() );
	}

	// start playing the source if necessary
	ALFunction.alGetSourcei( source, AL_SOURCE_STATE, &sourceState );
	assert( AL_NO_ERROR == ALFunction.alGetError() );
	if( !soundPaused && ( sourceState != AL_PLAYING ) ) {
		ALFunction.alSourcePlay( source );
		assert( AL_NO_ERROR == ALFunction.alGetError() );
	}
}


ISound *newOpenAL()
{
	winlog( "newOpenAL\n" );
	return new OpenAL();
}

#endif