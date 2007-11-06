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

// StringTokenizer.h: interface for the StringTokenizer class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_STRINGTOKENIZER_H__1AB4CD12_6B7A_49E4_A87F_75D3DC3FF20F__INCLUDED_)
#define AFX_STRINGTOKENIZER_H__1AB4CD12_6B7A_49E4_A87F_75D3DC3FF20F__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class StringTokenizer  
{
 public:
  const char * next();
  StringTokenizer(CString str, CString token);
  virtual ~StringTokenizer();

 private:
  CString m_token;
  CString m_delim;
  CString m_right;
};

#endif // !defined(AFX_STRINGTOKENIZER_H__1AB4CD12_6B7A_49E4_A87F_75D3DC3FF20F__INCLUDED_)
