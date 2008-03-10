// VisualBoyAdvance - Nintendo Gameboy/GameboyAdvance (TM) emulator.
// Copyright (C) 1999-2003 Forgotten
// Copyright (C) 2004 Forgotten and the VBA development team
// Copyright (C) 2005-2006 VBA development team
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


#ifndef NO_XAUDIO2


#define NBUFFERS 4

// MFC
#include "stdafx.h"

// Interface
#include "Sound.h"

// XAudio2
#include <Xaudio2.h>

// Internals
#include "../Sound.h" // for soundBufferLen, soundFinalWave and soundQuality
#include "../System.h" // for systemMessage()
#include "../Globals.h" // for 'speedup' and 'synchronize'


// Synchronization Event
class XAudio2_BufferNotify : public IXAudio2VoiceCallback
{
public:
	HANDLE hBufferEndEvent;

	XAudio2_BufferNotify() {
		hBufferEndEvent = NULL;
		hBufferEndEvent = CreateEvent( NULL, FALSE, FALSE, NULL );
		ASSERT( hBufferEndEvent != NULL );
	}

	~XAudio2_BufferNotify() {
		CloseHandle( hBufferEndEvent );
		hBufferEndEvent = NULL;
	}

    STDMETHOD_( void, OnBufferEnd ) ( void *pBufferContext ) {
		ASSERT( hBufferEndEvent != NULL );
		SetEvent( hBufferEndEvent );
	}


	// dummies:
	STDMETHOD_( void, OnVoiceProcessingPassStart ) () {}
	STDMETHOD_( void, OnVoiceProcessingPassEnd ) () {}
	STDMETHOD_( void, OnStreamEnd ) () {}
	STDMETHOD_( void, OnBufferStart ) ( void *pBufferContext ) {}
	STDMETHOD_( void, OnLoopEnd ) ( void *pBufferContext ) {}
	STDMETHOD_( void, OnVoiceError ) ( void *pBufferContext, HRESULT Error ) {};
};


// Class Declaration
class XAudio2_Output
	: public ISound
{
public:
	XAudio2_Output();
	~XAudio2_Output();

	// Initialization
	bool init();

	// Sound Data Feed
	void write();

	// Play Control
	void pause();
	void resume();
	void reset();

	// Configuration Changes
	void setThrottle( unsigned short throttle );


private:
	bool   failed;
	bool   initialized;
	bool   playing;
	UINT32 freq;
	BYTE  *buffers;
	int    currentBuffer;

	IXAudio2               *xaud;
	IXAudio2MasteringVoice *mVoice; // listener
	IXAudio2SourceVoice    *sVoice; // sound source
	XAUDIO2_BUFFER          buf;
	XAUDIO2_VOICE_STATE     vState;
	XAudio2_BufferNotify    notify; // buffer end notification
};


// Class Implementation
XAudio2_Output::XAudio2_Output()
{
	failed = false;
	initialized = false;
	playing = false;
	freq = 0;
	buffers = NULL;
	currentBuffer = 0;

	xaud = NULL;
	mVoice = NULL;
	sVoice = NULL;
	ZeroMemory( &buf, sizeof( buf ) );
	ZeroMemory( &vState, sizeof( vState ) );
}


XAudio2_Output::~XAudio2_Output()
{
	initialized = false;

	if( sVoice ) {
		if( playing ) {
			sVoice->Stop( 0 );
		}
		sVoice->DestroyVoice();
	}

	if( buffers ) {
		free( buffers );
	}

	if( mVoice ) {
		mVoice->DestroyVoice();
	}

	if( xaud ) {
		xaud->Release();
	}
}


bool XAudio2_Output::init()
{
	if( failed || initialized ) return false;

	// Initialize XAudio2 using COM
	HRESULT hr;

	hr = CoCreateInstance(
#ifdef _DEBUG
		__uuidof( XAudio2_Debug ),
#else
		__uuidof( XAudio2 ),
#endif
		NULL,
		CLSCTX_INPROC_SERVER,
		__uuidof( IXAudio2 ),
		(LPVOID *)&xaud
		);
	if( hr != S_OK ) {
		systemMessage( IDS_XAUDIO2_FAILURE, NULL );
		failed = true;
		return false;
	}

	hr = xaud->Initialize(
#ifdef _DEBUG
		XAUDIO2_DEBUG_ENGINE,
#else
		0,
#endif
		XAUDIO2_DEFAULT_PROCESSOR
		);
	if( hr != S_OK ) {
		systemMessage( IDS_XAUDIO2_FAILURE, NULL );
		failed = true;
		return false;
	}


	freq = 44100 / (UINT32)soundQuality;

	// calculate the number of samples per frame first
	// then multiply it with the size of a sample frame (16 bit * stereo)
	soundBufferLen = ( freq / 60 ) * 4;

	// create own buffers because sound data must not be manipulated
	// by the audio emulation core while it is still being played back
	buffers = (BYTE *)malloc( ( NBUFFERS + 1 ) * soundBufferLen );
	// + 1 because we need one temporary buffer when all others are still playing

	WAVEFORMATEX wfx;
	ZeroMemory( &wfx, sizeof( wfx ) );
	wfx.wFormatTag = WAVE_FORMAT_PCM;
	wfx.nChannels = 2;
	wfx.nSamplesPerSec = freq;
	wfx.wBitsPerSample = 16;
	wfx.nBlockAlign = wfx.nChannels * ( wfx.wBitsPerSample / 8 );
	wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;


	// Create Sound Receiver
	hr = xaud->CreateMasteringVoice( &mVoice, 2, freq );

	if( FAILED( hr ) ) {
		systemMessage( IDS_XAUDIO2_CANNOT_CREATE_MASTERINGVOICE, NULL );
		failed = true;
		return false;
	}


	// Create Sound Emitter
	hr = xaud->CreateSourceVoice( &sVoice, &wfx, 0, 4.0f, &notify );

	if( FAILED( hr ) ) {
		systemMessage( IDS_XAUDIO2_CANNOT_CREATE_SOURCEVOICE, NULL );
		failed = true;
		return false;
	}

	sVoice->Start( 0 );
	playing = true;


	setsystemSoundOn( true );
	initialized = true;
	return true;
}


void XAudio2_Output::write()
{
	if( !initialized || failed ) return;

	while( true ) {
		sVoice->GetState( &vState );

		ASSERT( vState.BuffersQueued <= NBUFFERS );

		if( vState.BuffersQueued < NBUFFERS ) {
			// there is at least one free buffer
			break;
		} else {
			// the maximum number of buffers is currently queued
			if( synchronize && !speedup && !theApp.throttle ) {
				// wait for one buffer to finish playing
				WaitForSingleObject( notify.hBufferEndEvent, INFINITE );
			} else {
				// drop current audio frame
				return;
			}
		}
	}

	// copy & protect the audio data in own memory area while playing it
	CopyMemory( &buffers[ currentBuffer * soundBufferLen ], soundFinalWave, soundBufferLen );

	buf.AudioBytes = soundBufferLen;
	buf.pAudioData = &buffers[ currentBuffer * soundBufferLen ];

	currentBuffer++;
	currentBuffer %= ( NBUFFERS + 1 );

	sVoice->SubmitSourceBuffer( &buf ); // send buffer to queue
}


void XAudio2_Output::pause()
{
	if( !initialized || failed ) return;

	if( playing ) {
		sVoice->Stop( 0 );
		playing = false;
	}
}


void XAudio2_Output::resume()
{
	if( !initialized || failed ) return;

	if( !playing ) {
		sVoice->Start( 0 );
		playing = true;
	}
}


void XAudio2_Output::reset()
{
	if( !initialized || failed ) return;

	if( playing ) {
		sVoice->Stop( 0 );
	}

	sVoice->FlushSourceBuffers();
	sVoice->Start( 0 );
	playing = true;	
}


void XAudio2_Output::setThrottle( unsigned short throttle )
{
	if( !initialized || failed ) return;

	if( throttle == 0 ) throttle = 100;
	sVoice->SetFrequencyRatio( (float)throttle / 100.0f );
}


ISound *newXAudio2_Output()
{
	return new XAudio2_Output();
}


#endif // #ifndef NO_XAUDIO2
