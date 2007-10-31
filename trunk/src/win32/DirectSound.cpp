// VisualBoyAdvance - Nintendo Gameboy/GameboyAdvance (TM) emulator.
// Copyright (C) 1999-2003 Forgotten
// Copyright (C) 2004-2005 Forgotten and the VBA development team

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
#include "VBA.h"
#include "AVIWrite.h"
#include "Sound.h"
#include "WavWriter.h"

#include "../System.h"
#include "../GBA.h"
#include "../Globals.h"
#include "../Sound.h"

#include <mmreg.h>
#include <Dsound.h> //DirectSound

extern bool soundBufferLow;

class DirectSound : public ISound
{
private:
  HINSTANCE            dsoundDLL;
  LPDIRECTSOUND        pDirectSound;
  LPDIRECTSOUNDBUFFER  dsbPrimary;
  LPDIRECTSOUNDBUFFER  dsbSecondary;
  LPDIRECTSOUNDNOTIFY  dsbNotify;
  HANDLE               dsbEvent;
  WAVEFORMATEX         wfx;

public:
  DirectSound();
  virtual ~DirectSound();

  bool init();
  void pause();
  void reset();
  void resume();
  void write();
};

DirectSound::DirectSound()
{
  dsoundDLL    = NULL;
  pDirectSound = NULL;
  dsbPrimary   = NULL;
  dsbSecondary = NULL;
  dsbNotify    = NULL;
  dsbEvent     = NULL;
}

DirectSound::~DirectSound()
{
  if(theApp.aviRecorder != NULL) {
    delete theApp.aviRecorder;
    theApp.aviRecorder = NULL;
    theApp.aviFrameNumber = 0;
  }

  if(theApp.soundRecording) {
    if(theApp.soundRecorder != NULL) {
      delete theApp.soundRecorder;
      theApp.soundRecorder = NULL;
    }
    theApp.soundRecording = false;
  }
  
  if(dsbNotify != NULL) {
    dsbNotify->Release();
    dsbNotify = NULL;
  }

  if(dsbEvent != NULL) {
    CloseHandle(dsbEvent);
    dsbEvent = NULL;
  }
  
  if(pDirectSound != NULL) {
    if(dsbPrimary != NULL) {
      dsbPrimary->Release();
      dsbPrimary = NULL;
    }
    
    if(dsbSecondary != NULL) {
      dsbSecondary->Release();
      dsbSecondary = NULL;
    }
    
    pDirectSound->Release();
    pDirectSound = NULL;
  }
  
  if(dsoundDLL != NULL) {
    FreeLibrary(dsoundDLL);
    dsoundDLL = NULL;
  }
}

bool DirectSound::init()
{
  HRESULT hr;

  dsoundDLL = LoadLibrary("dsound.dll");
  HRESULT (WINAPI *DSoundCreate)(LPCGUID,LPDIRECTSOUND *,IUnknown *);  
  if(dsoundDLL != NULL) {    
    DSoundCreate = (HRESULT (WINAPI *)(LPCGUID,LPDIRECTSOUND *,IUnknown *))
      GetProcAddress(dsoundDLL, "DirectSoundCreate8");
    
    if(DSoundCreate == NULL) {
      theApp.directXMessage("DirectSoundCreate8");
      return false;
    }
  } else {
    theApp.directXMessage("dsound.dll");
    return false;
  }
  
  if((hr = DSoundCreate(NULL,&pDirectSound,NULL) != DS_OK)) {
    //    errorMessage(myLoadString(IDS_ERROR_SOUND_CREATE), hr);
    systemMessage(IDS_CANNOT_CREATE_DIRECTSOUND,
                  "Cannot create DirectSound %08x", hr);
    pDirectSound = NULL;
    dsbSecondary = NULL;
    return false;
  }

  if((hr=pDirectSound->SetCooperativeLevel((HWND)*theApp.m_pMainWnd,
                                           DSSCL_EXCLUSIVE)) != DS_OK) {
    //    errorMessage(myLoadString(IDS_ERROR_SOUND_LEVEL), hr);
    systemMessage(IDS_CANNOT_SETCOOPERATIVELEVEL,
                  "Cannot SetCooperativeLevel %08x", hr);
    return false;
  }

  DSBUFFERDESC dsbdesc;
  ZeroMemory(&dsbdesc,sizeof(DSBUFFERDESC));
  dsbdesc.dwSize=sizeof(DSBUFFERDESC);
  dsbdesc.dwFlags = DSBCAPS_PRIMARYBUFFER;
  
  if((hr=pDirectSound->CreateSoundBuffer(&dsbdesc,
                                         &dsbPrimary,
                                         NULL) != DS_OK)) {
    //    errorMessage(myLoadString(IDS_ERROR_SOUND_BUFFER),hr);
    systemMessage(IDS_CANNOT_CREATESOUNDBUFFER,
                  "Cannot CreateSoundBuffer %08x", hr);
    return false;
  }
  
  // Set primary buffer format

  memset(&wfx, 0, sizeof(WAVEFORMATEX)); 
  wfx.wFormatTag = WAVE_FORMAT_PCM; 
  wfx.nChannels = 2;
  switch(soundQuality) {
  case 2:
    wfx.nSamplesPerSec = 22050;
    soundBufferLen = 736*2;
    soundBufferTotalLen = 7360*2;
    break;
  case 4:
    wfx.nSamplesPerSec = 11025;
    soundBufferLen = 368*2;
    soundBufferTotalLen = 3680*2;
    break;
  default:
    soundQuality = 1;
    wfx.nSamplesPerSec = 44100;
    soundBufferLen = 1470*2;
    soundBufferTotalLen = 14700*2;
  }
  wfx.wBitsPerSample = 16; 
  wfx.nBlockAlign = (wfx.wBitsPerSample / 8) * wfx.nChannels;
  wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;

  if((hr = dsbPrimary->SetFormat(&wfx)) != DS_OK) {
    //    errorMessage(myLoadString(IDS_ERROR_SOUND_PRIMARY),hr);
    systemMessage(IDS_CANNOT_SETFORMAT_PRIMARY,
                  "Cannot SetFormat for primary %08x", hr);
    return false;
  }
  
  ZeroMemory(&dsbdesc,sizeof(DSBUFFERDESC));  
  dsbdesc.dwSize = sizeof(DSBUFFERDESC);
  dsbdesc.dwFlags = DSBCAPS_GETCURRENTPOSITION2|DSBCAPS_CTRLPOSITIONNOTIFY;
  dsbdesc.dwBufferBytes = soundBufferTotalLen;
  dsbdesc.lpwfxFormat = &wfx;

  if((hr = pDirectSound->CreateSoundBuffer(&dsbdesc, &dsbSecondary, NULL))
     != DS_OK) {
    dsbdesc.dwFlags = DSBCAPS_GETCURRENTPOSITION2;
    if((hr = pDirectSound->CreateSoundBuffer(&dsbdesc, &dsbSecondary, NULL))
       != DS_OK) {
      systemMessage(IDS_CANNOT_CREATESOUNDBUFFER_SEC,
                    "Cannot CreateSoundBuffer secondary %08x", hr);
      return false;
    }
  }

  dsbSecondary->SetCurrentPosition(0);

  if(!theApp.useOldSync) {
    hr = dsbSecondary->QueryInterface(IID_IDirectSoundNotify,
                                      (void **)&dsbNotify);
    if(!FAILED(hr)) {
      dsbEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
      
      DSBPOSITIONNOTIFY notify[10];
      
      for(int i = 0; i < 10; i++) {
        notify[i].dwOffset = i*soundBufferLen;
        notify[i].hEventNotify = dsbEvent;
      }
      if(FAILED(dsbNotify->SetNotificationPositions(10, notify))) {
        dsbNotify->Release();
        dsbNotify = NULL;
        CloseHandle(dsbEvent);
        dsbEvent = NULL;
      }
    }
  }
  
  hr = dsbPrimary->Play(0,0,DSBPLAY_LOOPING);

  if(hr != DS_OK) {
    //    errorMessage(myLoadString(IDS_ERROR_SOUND_PLAYPRIM), hr);
    systemMessage(IDS_CANNOT_PLAY_PRIMARY, "Cannot Play primary %08x", hr);
    return false;
  }

  systemSoundOn = true;
  
  return true;
}

void DirectSound::pause()
{
  if(dsbSecondary != NULL) {
    DWORD status = 0;
    dsbSecondary->GetStatus(&status);
    
    if(status & DSBSTATUS_PLAYING) {
      dsbSecondary->Stop();
    }
  }  
}

void DirectSound::reset()
{
  if(dsbSecondary) {
    dsbSecondary->Stop();
    dsbSecondary->SetCurrentPosition(0);
  }  
}

void DirectSound::resume()
{
  if(dsbSecondary != NULL) {
    dsbSecondary->Play(0,0,DSBPLAY_LOOPING);
  }  
}

void DirectSound::write()
{
  int len = soundBufferLen;
  LPVOID  lpvPtr1; 
  DWORD   dwBytes1; 
  LPVOID  lpvPtr2; 
  DWORD   dwBytes2; 

  if(!pDirectSound)
    return;

  if(theApp.soundRecording) {
    if(dsbSecondary) {
      if(theApp.soundRecorder == NULL) {
        theApp.soundRecorder = new WavWriter;
        WAVEFORMATEX format;
        dsbSecondary->GetFormat(&format, sizeof(format), NULL);
        if(theApp.soundRecorder->Open(theApp.soundRecordName))
          theApp.soundRecorder->SetFormat(&format);
      }
    }
      
    if(theApp.soundRecorder) {
      theApp.soundRecorder->AddSound((u8 *)soundFinalWave, len);
    }
  }

  if(theApp.aviRecording) {
    if(theApp.aviRecorder) {
      if(dsbSecondary) {
        if(!theApp.aviRecorder->IsSoundAdded()) {
          WAVEFORMATEX format;
          dsbSecondary->GetFormat(&format, sizeof(format), NULL);
          theApp.aviRecorder->SetSoundFormat(&format);
        }
      }
      
      theApp.aviRecorder->AddSound((const char *)soundFinalWave, len);
    }
  }
  
  HRESULT hr;

  if(!speedup && synchronize && !theApp.throttle) {
    DWORD status=0;
    hr = dsbSecondary->GetStatus(&status);
    if(status && DSBSTATUS_PLAYING) {
      if(!soundPaused) {      
        DWORD play;
        while(true) {
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
    } else {
      soundPaused = 1;
    }
  }
  // Obtain memory address of write block. This will be in two parts
  // if the block wraps around.
  hr = dsbSecondary->Lock(soundNextPosition, soundBufferLen, &lpvPtr1, 
                          &dwBytes1, &lpvPtr2, &dwBytes2,
                          0);
  
  // If DSERR_BUFFERLOST is returned, restore and retry lock. 
  if (DSERR_BUFFERLOST == hr) { 
    dsbSecondary->Restore(); 
    hr = dsbSecondary->Lock(soundNextPosition, soundBufferLen,&lpvPtr1,
                            &dwBytes1, &lpvPtr2, &dwBytes2,
                            0);
  } 

  soundNextPosition += soundBufferLen;
  soundNextPosition = soundNextPosition % soundBufferTotalLen;
  
  if SUCCEEDED(hr) { 
    // Write to pointers. 
    CopyMemory(lpvPtr1, soundFinalWave, dwBytes1); 
    if (NULL != lpvPtr2) { 
      CopyMemory(lpvPtr2, soundFinalWave+dwBytes1, dwBytes2); 
    } 
    // Release the data back to DirectSound. 
    hr = dsbSecondary->Unlock(lpvPtr1, dwBytes1, lpvPtr2, 
                              dwBytes2);
  }
}

ISound *newDirectSound()
{
  return new DirectSound();
}
