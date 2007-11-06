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

#ifndef VBA_PROF_PROF_H
#define VBA_PROF_PROF_H

/* Control profiling;
   profiling is what mcount checks to see if
   all the data structures are ready.  */


typedef struct profile_segment {
  unsigned short *froms;
  struct tostruct *tos;
  long  tolimit;

  u32  s_lowpc;
  u32  s_highpc;
  unsigned long s_textsize;
  
  int ssiz;
  char *sbuf;
  int s_scale;
  
  struct profile_segment *next;

} profile_segment;

extern void profControl(int mode);
extern void profStartup(u32 lowpc, u32 highpc);
extern void profCleanup();
extern void profCount();

extern void profSetHertz(int hertz);
#endif

