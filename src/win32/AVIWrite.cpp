// VisualBoyAdvance - Nintendo Gameboy/GameboyAdvance (TM) emulator.
// Copyright (C) 1999-2003 Forgotten
// Copyright (C) 2004 Forgotten and the VBA development team

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
#include "AVIWrite.h"

AVIWrite::AVIWrite()
{
  m_failed = false;
  m_file = NULL;
  m_stream = NULL;
  m_streamCompressed = NULL;
  m_streamSound = NULL;
  m_samplesSound = 0;
  
  AVIFileInit();
}

AVIWrite::~AVIWrite()
{
  if(m_streamSound)
    AVIStreamClose(m_streamSound);

  if(m_streamCompressed)
    AVIStreamClose(m_streamCompressed);
  
  if(m_stream)
    AVIStreamClose(m_stream);

  if(m_file)
    AVIFileClose(m_file);

  AVIFileExit();
}

void AVIWrite::SetVideoFormat(BITMAPINFOHEADER *bh)
{
  // force size to 0x28 to avoid extra fields
  memcpy(&m_bitmap, bh, 0x28);
}

void AVIWrite::SetSoundFormat(WAVEFORMATEX *format)
{
  memcpy(&m_soundFormat, format, sizeof(WAVEFORMATEX));
  ZeroMemory(&m_soundHeader, sizeof(AVISTREAMINFO));
  // setup the sound stream header
  m_soundHeader.fccType = streamtypeAUDIO;
  m_soundHeader.dwQuality = (DWORD)-1;
  m_soundHeader.dwScale = format->nBlockAlign;
  m_soundHeader.dwInitialFrames = 1;
  m_soundHeader.dwRate = format->nAvgBytesPerSec;
  m_soundHeader.dwSampleSize = format->nBlockAlign;
  
  // create the sound stream
  if(FAILED(AVIFileCreateStream(m_file, &m_streamSound, &m_soundHeader))) {
    m_failed = true;
    return;
  }
  
  // setup the sound stream format
  if(FAILED(AVIStreamSetFormat(m_streamSound, 0 , (void *)&m_soundFormat,
                               sizeof(WAVEFORMATEX)))) {
    m_failed = true;
    return;
  }
}

bool AVIWrite::Open(const char *filename)
{
  // create the AVI file
  if(FAILED(AVIFileOpen(&m_file,
                        filename,
                        OF_WRITE | OF_CREATE,
                        NULL))) {
    m_failed = true;
    return false;
  }
  // setup the video stream information
  ZeroMemory(&m_header, sizeof(AVISTREAMINFO));
  m_header.fccType = streamtypeVIDEO;
  m_header.dwScale = 1;
  m_header.dwRate = m_fps;
  m_header.dwSuggestedBufferSize  = m_bitmap.biSizeImage;

  // create the video stream
  if(FAILED(AVIFileCreateStream(m_file,
                                &m_stream,
                                &m_header))) {
    m_failed = true;
    return false;
  }
      
  ZeroMemory(&m_options, sizeof(AVICOMPRESSOPTIONS));
  m_arrayOptions[0] = &m_options;

  // call the dialog to setup the compress options to be used
  if(!AVISaveOptions(AfxGetApp()->m_pMainWnd->GetSafeHwnd(), 0, 1, &m_stream, m_arrayOptions)) {
    m_failed = true;
    return false;
  }
  
  // create the compressed stream
  if(FAILED(AVIMakeCompressedStream(&m_streamCompressed, m_stream, &m_options, NULL))) {
    m_failed = true;
    return false;
  }
  
  // setup the video stream format
  if(FAILED( AVIStreamSetFormat(m_streamCompressed, 0,
                                &m_bitmap,
                                m_bitmap.biSize +
                                m_bitmap.biClrUsed * sizeof(RGBQUAD)))) {
    m_failed = true;
    return false;
  }

  return true;
}

bool AVIWrite::AddSound(const char *sound, int len)
{
  // return if we failed somewhere already
  if(m_failed)
    return false;

  int samples = len / m_soundFormat.nBlockAlign;

  if(FAILED(AVIStreamWrite(m_streamSound,
                           m_samplesSound,
                           samples,
                           (LPVOID)sound,
                           len,
                           0,
                           NULL,
                           NULL))) {
    m_failed = true;
    return false;
  }
  m_samplesSound += samples;

  return true;
}

bool AVIWrite::AddFrame(const int frame, const char *bmp)
{
  if (m_failed)
    return false;
  
  // write the frame to the video stream
  if(FAILED(AVIStreamWrite(m_streamCompressed,
                           frame,
                           1,
                           (LPVOID)bmp,
                           m_bitmap.biSizeImage,
                           AVIIF_KEYFRAME,
                           NULL,
                           NULL))) {
    m_failed = true;
    return false;
  }
  return true;
}

bool AVIWrite::IsSoundAdded()
{
  return m_streamSound != NULL;
}

void AVIWrite::SetFPS(int f)
{
  m_fps = f;
}
