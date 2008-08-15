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

// MFC
#include "stdafx.h"

// Interface
#include "Sound.h"

// XAudio2
#include <xaudio2.h>

// Internals
#include "../Sound.h" // for soundBufferLen, soundFinalWave and soundQuality
#include "../System.h" // for systemMessage()
#include "../Globals.h"


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
	STDMETHOD_( void, OnVoiceProcessingPassStart ) ( UINT32 BytesRequired ) {}
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
	UINT32 bufferCount;
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
	bufferCount = theApp.xa2BufferCount;
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
			HRESULT hr = sVoice->Stop( 0 );
			ASSERT( hr == S_OK );
		}
		sVoice->DestroyVoice();
	}

	if( buffers ) {
		free( buffers );
		buffers = NULL;
	}

	if( mVoice ) {
		mVoice->DestroyVoice();
	}

	if( xaud ) {
		xaud->Release();
		xaud = NULL;
	}
}


bool XAudio2_Output::init()
{
	if( failed || initialized ) return false;

	HRESULT hr;

	// Initialize XAudio2
	UINT32 flags = 0;
#ifdef _DEBUG
	flags = XAUDIO2_DEBUG_ENGINE;
#endif

	hr = XAudio2Create( &xaud, flags );
	if( hr != S_OK ) {
		systemMessage( IDS_XAUDIO2_FAILURE, NULL );
		failed = true;
		return false;
	}


	freq = 44100 / (UINT32)soundQuality;

	// calculate the number of samples per frame first
	// then multiply it with the size of a sample frame (16 bit * stereo)
	soundBufferLen = ( freq / 60 ) * 4;

	// create own buffers to store sound data because it must not be
	// manipulated while the voice plays from it
	buffers = (BYTE *)malloc( ( bufferCount + 1 ) * soundBufferLen );
	// + 1 because we need one temporary buffer when all others are in use

	WAVEFORMATEX wfx;
	ZeroMemory( &wfx, sizeof( wfx ) );
	wfx.wFormatTag = WAVE_FORMAT_PCM;
	wfx.nChannels = 2;
	wfx.nSamplesPerSec = freq;
	wfx.wBitsPerSample = 16;
	wfx.nBlockAlign = wfx.nChannels * ( wfx.wBitsPerSample / 8 );
	wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;


	// create sound receiver
	hr = xaud->CreateMasteringVoice(
		&mVoice,
		XAUDIO2_DEFAULT_CHANNELS,
		XAUDIO2_DEFAULT_SAMPLERATE,
		0,
		theApp.xa2Device,
		NULL );
	if( hr != S_OK ) {
		systemMessage( IDS_XAUDIO2_CANNOT_CREATE_MASTERINGVOICE, NULL );
		failed = true;
		return false;
	}


	// create sound emitter
	hr = xaud->CreateSourceVoice( &sVoice, &wfx, 0, 4.0f, &notify );
	if( hr != S_OK ) {
		systemMessage( IDS_XAUDIO2_CANNOT_CREATE_SOURCEVOICE, NULL );
		failed = true;
		return false;
	}


	if( theApp.xa2Upmixing ) {
		// set up stereo upmixing
		XAUDIO2_DEVICE_DETAILS dd;
		ZeroMemory( &dd, sizeof( dd ) );
		hr = xaud->GetDeviceDetails( 0, &dd );
		ASSERT( hr == S_OK );
		float *matrix = NULL;
		matrix = (float*)malloc( sizeof( float ) * 2 * dd.OutputFormat.Format.nChannels );
		switch( dd.OutputFormat.Format.nChannels ) {
			case 4: // 4.0
	//Speaker \ Left Source           Right Source
	/*Front L*/	matrix[0] = 1.0000f;  matrix[1] = 0.0000f;
	/*Front R*/	matrix[2] = 0.0000f;  matrix[3] = 1.0000f;
	/*Back  L*/	matrix[4] = 1.0000f;  matrix[5] = 0.0000f;
	/*Back  R*/	matrix[6] = 0.0000f;  matrix[7] = 1.0000f;
				break;
			case 5: // 5.0
	//Speaker \ Left Source           Right Source
	/*Front L*/	matrix[0] = 1.0000f;  matrix[1] = 0.0000f;
	/*Front R*/	matrix[2] = 0.0000f;  matrix[3] = 1.0000f;
	/*Front C*/	matrix[4] = 0.7071f;  matrix[5] = 0.7071f;
	/*Side  L*/	matrix[6] = 1.0000f;  matrix[7] = 0.0000f;
	/*Side  R*/	matrix[8] = 0.0000f;  matrix[9] = 1.0000f;
				break;
			case 6: // 5.1
	//Speaker \ Left Source           Right Source
	/*Front L*/	matrix[0] = 1.0000f;  matrix[1] = 0.0000f;
	/*Front R*/	matrix[2] = 0.0000f;  matrix[3] = 1.0000f;
	/*Front C*/	matrix[4] = 0.7071f;  matrix[5] = 0.7071f;
	/*LFE    */	matrix[6] = 0.0000f;  matrix[7] = 0.0000f;
	/*Side  L*/	matrix[8] = 1.0000f;  matrix[9] = 0.0000f;
	/*Side  R*/	matrix[10] = 0.0000f;  matrix[11] = 1.0000f;
				break;
			case 7: // 6.1
	//Speaker \ Left Source           Right Source
	/*Front L*/	matrix[0] = 1.0000f;  matrix[1] = 0.0000f;
	/*Front R*/	matrix[2] = 0.0000f;  matrix[3] = 1.0000f;
	/*Front C*/	matrix[4] = 0.7071f;  matrix[5] = 0.7071f;
	/*LFE    */	matrix[6] = 0.0000f;  matrix[7] = 0.0000f;
	/*Side  L*/	matrix[8] = 1.0000f;  matrix[9] = 0.0000f;
	/*Side  R*/	matrix[10] = 0.0000f;  matrix[11] = 1.0000f;
	/*Back  C*/	matrix[12] = 0.7071f;  matrix[13] = 0.7071f;
				break;
			case 8: // 7.1
	//Speaker \ Left Source           Right Source
	/*Front L*/	matrix[0] = 1.0000f;  matrix[1] = 0.0000f;
	/*Front R*/	matrix[2] = 0.0000f;  matrix[3] = 1.0000f;
	/*Front C*/	matrix[4] = 0.7071f;  matrix[5] = 0.7071f;
	/*LFE    */	matrix[6] = 0.0000f;  matrix[7] = 0.0000f;
	/*Back  L*/	matrix[8] = 1.0000f;  matrix[9] = 0.0000f;
	/*Back  R*/	matrix[10] = 0.0000f;  matrix[11] = 1.0000f;
	/*Side  L*/	matrix[12] = 1.0000f;  matrix[13] = 0.0000f;
	/*Side  R*/	matrix[14] = 0.0000f;  matrix[15] = 1.0000f;
				break;
		}
		hr = sVoice->SetOutputMatrix( NULL, 2, dd.OutputFormat.Format.nChannels, matrix );
		ASSERT( hr == S_OK );
		free( matrix );
		matrix = NULL;
	}


	hr = sVoice->Start( 0 );
	ASSERT( hr == S_OK );
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

		ASSERT( vState.BuffersQueued <= bufferCount );

		if( vState.BuffersQueued < bufferCount ) {
			if( vState.BuffersQueued == 0 ) {
				// buffers ran dry
				if( systemVerbose & VERBOSE_SOUNDOUTPUT ) {
					static unsigned int i = 0;
					log( "XAudio2: Buffers were not refilled fast enough (i=%i)\n", i++ );
				}
			}
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
	currentBuffer %= ( bufferCount + 1 ); // + 1 because we need one temporary buffer

	HRESULT hr = sVoice->SubmitSourceBuffer( &buf ); // send buffer to queue
	ASSERT( hr == S_OK );
}


void XAudio2_Output::pause()
{
	if( !initialized || failed ) return;

	if( playing ) {
		HRESULT hr = sVoice->Stop( 0 );
		ASSERT( hr == S_OK );
		playing = false;
	}
}


void XAudio2_Output::resume()
{
	if( !initialized || failed ) return;

	if( !playing ) {
		HRESULT hr = sVoice->Start( 0 );
		ASSERT( hr == S_OK );
		playing = true;
	}
}


void XAudio2_Output::reset()
{
	if( !initialized || failed ) return;

	if( playing ) {
		HRESULT hr = sVoice->Stop( 0 );
		ASSERT( hr == S_OK );
	}

	sVoice->FlushSourceBuffers();
	sVoice->Start( 0 );
	playing = true;	
}


void XAudio2_Output::setThrottle( unsigned short throttle )
{
	if( !initialized || failed ) return;

	if( throttle == 0 ) throttle = 100;
	HRESULT hr = sVoice->SetFrequencyRatio( (float)throttle / 100.0f );
	ASSERT( hr == S_OK );
}


ISound *newXAudio2_Output()
{
	return new XAudio2_Output();
}


#endif // #ifndef NO_XAUDIO2
