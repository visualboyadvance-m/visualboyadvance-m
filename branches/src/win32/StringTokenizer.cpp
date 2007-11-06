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

// StringTokenizer.cpp: implementation of the StringTokenizer class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "vba.h"
#include "StringTokenizer.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

StringTokenizer::StringTokenizer(CString str, CString del)
{
  m_right = str;
  m_delim = del;
}

StringTokenizer::~StringTokenizer()
{

}


const char *StringTokenizer::next()
{
  int index = m_right.FindOneOf(m_delim);
  
  while(index == 0) {
    m_right = m_right.Right(m_right.GetLength()-1);
    index = m_right.FindOneOf(m_delim);
  }
  if(index == -1) {
    if(m_right.IsEmpty())
      return NULL;
    m_token = m_right;
    m_right.Empty();
    return m_token;
  }

  m_token = m_right.Left(index);
  m_right = m_right.Right(m_right.GetLength()-(1+index));

  return m_token;
}
