// MFC
#include "stdafx.h"

// Application
#include "VBA.h"

// Tools
#include "AVIWrite.h"
#include "WavWriter.h"

// Internals
#include "../System.h"
#include "../gba/GBA.h"
#include "../gba/Globals.h"
#include "../gba/Sound.h"
#include "../common/SoundDriver.h"

// DirectSound8
#define DIRECTSOUND_VERSION 0x0800
#include <dsound.h>

extern bool soundBufferLow;

class DirectSound : public SoundDriver
{
private:
	LPDIRECTSOUND8       pDirectSound; // DirectSound interface
	LPDIRECTSOUNDBUFFER  dsbPrimary;   // Primary DirectSound buffer
	LPDIRECTSOUNDBUFFER  dsbSecondary; // Secondary DirectSound buffer
	LPDIRECTSOUNDNOTIFY8 dsbNotify;
	HANDLE               dsbEvent;
	WAVEFORMATEX         wfx;          // Primary buffer wave format
	int					 soundBufferLen;
	int                  soundBufferTotalLen;
	unsigned int         soundNextPosition;

public:
	DirectSound();
	virtual ~DirectSound();

	bool init(long sampleRate);   // initialize the primary and secondary sound buffer
	void pause();  // pause the secondary sound buffer
	void reset();  // stop and reset the secondary sound buffer
	void resume(); // resume the secondary sound buffer
	void write(u16 * finalWave, int length);  // write the emulated sound to the secondary sound buffer
};


DirectSound::DirectSound()
{
	pDirectSound  = NULL;
	dsbPrimary    = NULL;
	dsbSecondary  = NULL;
	dsbNotify     = NULL;
	dsbEvent      = NULL;

	soundBufferTotalLen = 14700;
	soundNextPosition = 0;
}


DirectSound::~DirectSound()
{
	if(dsbNotify) {
		dsbNotify->Release();
		dsbNotify = NULL;
	}

	if(dsbEvent) {
		CloseHandle(dsbEvent);
		dsbEvent = NULL;
	}

	if(pDirectSound) {
		if(dsbPrimary) {
			dsbPrimary->Release();
			dsbPrimary = NULL;
		}

		if(dsbSecondary) {
			dsbSecondary->Release();
			dsbSecondary = NULL;
		}

		pDirectSound->Release();
		pDirectSound = NULL;
	}
}


bool DirectSound::init(long sampleRate)
{
	HRESULT hr;
	DWORD freq;
	DSBUFFERDESC dsbdesc;
	int i;

	hr = CoCreateInstance( CLSID_DirectSound8, NULL, CLSCTX_INPROC_SERVER, IID_IDirectSound8, (LPVOID *)&pDirectSound );
	if( hr != S_OK ) {
		systemMessage( IDS_CANNOT_CREATE_DIRECTSOUND, NULL, hr );
		return false;
	}

	pDirectSound->Initialize( &DSDEVID_DefaultPlayback );
	if( hr != DS_OK ) {
		systemMessage( IDS_CANNOT_CREATE_DIRECTSOUND, NULL, hr );
		return false;
	}

	if( FAILED( hr = pDirectSound->SetCooperativeLevel( theApp.m_pMainWnd->GetSafeHwnd(), DSSCL_EXCLUSIVE ) ) ) {
		systemMessage( IDS_CANNOT_SETCOOPERATIVELEVEL, _T("Cannot SetCooperativeLevel %08x"), hr );
		return false;
	}


	// Create primary sound buffer
	ZeroMemory( &dsbdesc, sizeof(DSBUFFERDESC) );
	dsbdesc.dwSize = sizeof(DSBUFFERDESC);
	dsbdesc.dwFlags = DSBCAPS_PRIMARYBUFFER;
	if( dsoundDisableHardwareAcceleration ) {
		dsbdesc.dwFlags |= DSBCAPS_LOCSOFTWARE;
	}

	if( FAILED( hr = pDirectSound->CreateSoundBuffer( &dsbdesc, &dsbPrimary, NULL ) ) ) {
		systemMessage(IDS_CANNOT_CREATESOUNDBUFFER, _T("Cannot CreateSoundBuffer %08x"), hr);
		return false;
	}

	freq = sampleRate;
	// calculate the number of samples per frame first
	// then multiply it with the size of a sample frame (16 bit * stereo)
	soundBufferLen = ( freq / 60 ) * 4;
	soundBufferTotalLen = soundBufferLen * 10;
	soundNextPosition = 0;

	ZeroMemory( &wfx, sizeof(WAVEFORMATEX) );
	wfx.wFormatTag = WAVE_FORMAT_PCM;
	wfx.nChannels = 2;
	wfx.nSamplesPerSec = freq;
	wfx.wBitsPerSample = 16;
	wfx.nBlockAlign = wfx.nChannels * wfx.wBitsPerSample / 8;
	wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;

	if( FAILED( hr = dsbPrimary->SetFormat( &wfx ) ) ) {
		systemMessage( IDS_CANNOT_SETFORMAT_PRIMARY, _T("CreateSoundBuffer(primary) failed %08x"), hr );
		return false;
	}


	// Create secondary sound buffer
	ZeroMemory( &dsbdesc, sizeof(DSBUFFERDESC) );
	dsbdesc.dwSize = sizeof(DSBUFFERDESC);
	dsbdesc.dwFlags = DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_CTRLPOSITIONNOTIFY | DSBCAPS_GLOBALFOCUS;
	if( dsoundDisableHardwareAcceleration ) {
		dsbdesc.dwFlags |= DSBCAPS_LOCSOFTWARE;
	}
	dsbdesc.dwBufferBytes = soundBufferTotalLen;
	dsbdesc.lpwfxFormat = &wfx;

	if( FAILED( hr = pDirectSound->CreateSoundBuffer( &dsbdesc, &dsbSecondary, NULL ) ) ) {
		systemMessage( IDS_CANNOT_CREATESOUNDBUFFER, _T("CreateSoundBuffer(secondary) failed %08x"), hr );
		return false;
	}

	if( FAILED( hr = dsbSecondary->SetCurrentPosition( 0 ) ) ) {
		systemMessage( 0, _T("dsbSecondary->SetCurrentPosition failed %08x"), hr );
		return false;
	}


	if( SUCCEEDED( hr = dsbSecondary->QueryInterface( IID_IDirectSoundNotify8, (LPVOID*)&dsbNotify ) ) ) {
		dsbEvent = CreateEvent( NULL, FALSE, FALSE, NULL );
		DSBPOSITIONNOTIFY notify[10];
		for( i = 0; i < 10; i++ ) {
			notify[i].dwOffset = i * soundBufferLen;
			notify[i].hEventNotify = dsbEvent;
		}

		if( FAILED( dsbNotify->SetNotificationPositions( 10, notify ) ) ) {
			dsbNotify->Release();
			dsbNotify = NULL;
			CloseHandle(dsbEvent);
			dsbEvent = NULL;
		}
	}


	// Play primary buffer
	if( FAILED( hr = dsbPrimary->Play( 0, 0, DSBPLAY_LOOPING ) ) ) {
		systemMessage( IDS_CANNOT_PLAY_PRIMARY, _T("Cannot Play primary %08x"), hr );
		return false;
	}

	return true;
}


void DirectSound::pause()
{
	if( dsbSecondary == NULL ) return;

	DWORD status;

	dsbSecondary->GetStatus( &status );

	if( status & DSBSTATUS_PLAYING ) dsbSecondary->Stop();
}


void DirectSound::reset()
{
	if( dsbSecondary == NULL ) return;

	dsbSecondary->Stop();

	dsbSecondary->SetCurrentPosition( 0 );

	soundNextPosition = 0;
}


void DirectSound::resume()
{
	if( dsbSecondary == NULL ) return;

	dsbSecondary->Play( 0, 0, DSBPLAY_LOOPING );
}


void DirectSound::write(u16 * finalWave, int length)
{
	if(!pDirectSound) return;


	HRESULT      hr;
	DWORD        status = 0;
	DWORD        play = 0;
	LPVOID       lpvPtr1;
	DWORD        dwBytes1 = 0;
	LPVOID       lpvPtr2;
	DWORD        dwBytes2 = 0;

	if( !speedup && throttle && !gba_joybus_active) {
		hr = dsbSecondary->GetStatus(&status);
		if( status & DSBSTATUS_PLAYING ) {
			if( !soundPaused ) {
				while( true ) {
					dsbSecondary->GetCurrentPosition(&play, NULL);
					  int BufferLeft = ((soundNextPosition <= play) ?
					  play - soundNextPosition :
					  soundBufferTotalLen - soundNextPosition + play);

		          if(BufferLeft > soundBufferLen)
				  {
					if (BufferLeft > soundBufferTotalLen - (soundBufferLen * 3))
						soundBufferLow = true;
					break;
				   }
				   soundBufferLow = false;

		          if(dsbEvent) {
		            WaitForSingleObject(dsbEvent, 50);
		          }
		        }
			}
		}/* else {
		 // TODO: remove?
			 setsoundPaused(true);
		}*/
	}


	// Obtain memory address of write block.
	// This will be in two parts if the block wraps around.
	if( DSERR_BUFFERLOST == ( hr = dsbSecondary->Lock(
		soundNextPosition,
		soundBufferLen,
		&lpvPtr1,
		&dwBytes1,
		&lpvPtr2,
		&dwBytes2,
		0 ) ) ) {
			// If DSERR_BUFFERLOST is returned, restore and retry lock.
			dsbSecondary->Restore();
			hr = dsbSecondary->Lock(
				soundNextPosition,
				soundBufferLen,
				&lpvPtr1,
				&dwBytes1,
				&lpvPtr2,
				&dwBytes2,
				0 );
	}

	soundNextPosition += soundBufferLen;
	soundNextPosition = soundNextPosition % soundBufferTotalLen;

	if( SUCCEEDED( hr ) ) {
		// Write to pointers.
		CopyMemory( lpvPtr1, finalWave, dwBytes1 );
		if ( lpvPtr2 ) {
			CopyMemory( lpvPtr2, finalWave + dwBytes1, dwBytes2 );
		}

		// Release the data back to DirectSound.
		hr = dsbSecondary->Unlock( lpvPtr1, dwBytes1, lpvPtr2, dwBytes2 );
	} else {
		systemMessage( 0, _T("dsbSecondary->Lock() failed: %08x"), hr );
		return;
	}
}

SoundDriver *newDirectSound()
{
	return new DirectSound();
}
