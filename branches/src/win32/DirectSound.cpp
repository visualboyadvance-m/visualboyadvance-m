// VisualBoyAdvance - Nintendo Gameboy/GameboyAdvance (TM) emulator.
// Copyright (C) 1999-2003 Forgotten
// Copyright (C) 2004 Forgotten and the VBA development team
// Copyright (C) 2004-2006 VBA development team

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

// MFC
#include "stdafx.h"

// Tools
#include "AVIWrite.h"
#include "WavWriter.h"

// Internals
#include "../System.h"
#include "../GBA.h"
#include "../Globals.h"
#include "../Sound.h"

// DirectSound8
#include <Dsound.h>
#pragma comment( lib, "Dsound" )
#pragma comment( lib, "Dxguid" )


class DirectSound : public ISound
{
private:
	LPDIRECTSOUND8       pDirectSound; // DirectSound interface
	LPDIRECTSOUNDBUFFER  dsbPrimary;   // Primary DirectSound buffer
	LPDIRECTSOUNDBUFFER  dsbSecondary; // Secondary DirectSound buffer
	LPDIRECTSOUNDNOTIFY8 dsbNotify;
	HANDLE               dsbEvent;
	WAVEFORMATEX         wfx;          // Primary buffer wave format

public:
	DirectSound();
	virtual ~DirectSound();

	bool init();   // initialize the primary and secondary sound buffer
	void pause();  // pause the secondary sound buffer
	void reset();  // stop and reset the secondary sound buffer
	void resume(); // resume the secondary sound buffer
	void write();  // write the emulated sound to the secondary sound buffer
};


DirectSound::DirectSound()
{
	CoInitialize( NULL );
	
	pDirectSound  = NULL;
	dsbPrimary    = NULL;
	dsbSecondary  = NULL;
	dsbNotify     = NULL;
	dsbEvent      = NULL;
}


DirectSound::~DirectSound()
{
	if(theApp.aviRecorder) {
		delete theApp.aviRecorder;
		theApp.aviRecorder = NULL;
		theApp.aviFrameNumber = 0;
	}
	
	if(theApp.soundRecording) {
		if(theApp.soundRecorder) {
			delete theApp.soundRecorder;
			theApp.soundRecorder = NULL;
		}
		theApp.soundRecording = false;
	}
	
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
	
	CoUninitialize();
}


bool DirectSound::init()
{
	HRESULT hr;
	DWORD freq;
	DSBUFFERDESC dsbdesc;
	int i;


	// Initialize DirectSound
	if( FAILED( hr = DirectSoundCreate8( &DSDEVID_DefaultPlayback, &pDirectSound, NULL ) ) ) {
		systemMessage( IDS_CANNOT_CREATE_DIRECTSOUND, _T("Cannot create DirectSound %08x"), hr );
		pDirectSound = NULL;
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
	if( theApp.dsoundDisableHardwareAcceleration ) {
		dsbdesc.dwFlags |= DSBCAPS_LOCSOFTWARE;
	}

	if( FAILED( hr = pDirectSound->CreateSoundBuffer( &dsbdesc, &dsbPrimary, NULL ) ) ) {
		systemMessage(IDS_CANNOT_CREATESOUNDBUFFER, _T("Cannot CreateSoundBuffer %08x"), hr);
		return false;
	}

	switch(soundQuality)
	{
	case 4:
		freq = 11025;
		break;
	case 2:
		freq = 22050;
		break;
	default:
		soundQuality = 1;
	case 1:
		freq = 44100;
		break;
	}
	soundBufferLen = freq*2/30;
	soundBufferTotalLen = soundBufferLen * 10;

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
	if( theApp.dsoundDisableHardwareAcceleration ) {
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


	if( !theApp.useOldSync ) {
		if( FAILED( hr = dsbSecondary->QueryInterface( IID_IDirectSoundNotify8, (LPVOID*)&dsbNotify ) ) ) {
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
	}


	// Play primary buffer
	if( FAILED( hr = dsbPrimary->Play( 0, 0, DSBPLAY_LOOPING ) ) ) {
		systemMessage( IDS_CANNOT_PLAY_PRIMARY, _T("Cannot Play primary %08x"), hr );
		return false;
	}
	
	systemSoundOn = true;
	
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
}


void DirectSound::resume()
{
	if( dsbSecondary == NULL ) return;
	
	dsbSecondary->Play( 0, 0, DSBPLAY_LOOPING );
}


void DirectSound::write()
{
	if(!pDirectSound) return;


	HRESULT      hr;
	DWORD        status = 0;
	DWORD        play = 0;
	WAVEFORMATEX format;
	LPVOID       lpvPtr1;
	DWORD        dwBytes1 = 0;
	LPVOID       lpvPtr2;
	DWORD        dwBytes2 = 0;


	if( theApp.soundRecording ) {
		if( dsbSecondary ) {
			if( theApp.soundRecorder ) {
				theApp.soundRecorder->AddSound( (u8 *)soundFinalWave, soundBufferLen );
			} else {
				theApp.soundRecorder = new WavWriter;
				dsbSecondary->GetFormat( &format, sizeof(format), NULL );
				if( theApp.soundRecorder->Open( theApp.soundRecordName ) ) {
					theApp.soundRecorder->SetFormat( &format );
				}
			}
		}
	}
	

	if( theApp.aviRecording ) {
		if( theApp.aviRecorder ) {
			if( dsbSecondary ) {
				if( !theApp.aviRecorder->IsSoundAdded() ) {
					dsbSecondary->GetFormat( &format, sizeof(format), NULL );
					theApp.aviRecorder->SetSoundFormat( &format );
				}
			}
			theApp.aviRecorder->AddSound( (const char *)soundFinalWave, soundBufferLen );
		}
	}
	
	
	if( !speedup && synchronize && !theApp.throttle ) {
		hr = dsbSecondary->GetStatus(&status);
		if( status & DSBSTATUS_PLAYING ) {
			if( !soundPaused ) {      
				while( true ) {
					dsbSecondary->GetCurrentPosition(&play, NULL);
					if( ( play < soundNextPosition ) || ( play > (soundNextPosition + soundBufferLen) ) ) {
						break;
					}
					if( dsbEvent ) {
						WaitForSingleObject(dsbEvent, 50);
					}
				}
			}
		} else {
			soundPaused = 1;
		}
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
		CopyMemory( lpvPtr1, soundFinalWave, dwBytes1 );
		if ( lpvPtr2 ) {
			CopyMemory( lpvPtr2, soundFinalWave + dwBytes1, dwBytes2 );
		}
		
		// Release the data back to DirectSound.
		hr = dsbSecondary->Unlock( lpvPtr1, dwBytes1, lpvPtr2, dwBytes2 );
	} else {
		systemMessage( 0, _T("dsbSecondary->Lock() failed: %08x"), hr );
		return;
	}
}


ISound *newDirectSound()
{
	return new DirectSound();
}
