// -*- C++ -*-
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

// WavWriter.h: interface for the WavWriter class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_WAVWRITER_H__BE6C9DE9_60E7_4192_9797_8C7F55B3CE46__INCLUDED_)
#define AFX_WAVWRITER_H__BE6C9DE9_60E7_4192_9797_8C7F55B3CE46__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include <mmreg.h>

class WavWriter  
{
 private:
  FILE *m_file;
  int m_len;
  long m_posSize;

 public:
  WavWriter();
  ~WavWriter();

  bool Open(const char *name);
  void SetFormat(const WAVEFORMATEX *format);
  void AddSound(const u8 *data, int len);

 private:
  void Close();
};

#endif // !defined(AFX_WAVWRITER_H__BE6C9DE9_60E7_4192_9797_8C7F55B3CE46__INCLUDED_)
