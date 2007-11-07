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

#ifndef VBA_UTIL_H
#define VBA_UTIL_H
enum IMAGE_TYPE {
  IMAGE_UNKNOWN = -1,
  IMAGE_GBA     = 0,
  IMAGE_GB      = 1
};

// save game

typedef struct {
  void *address;
  int size;
} variable_desc;

extern bool utilWritePNGFile(const char *, int, int, u8 *);
extern bool utilWriteBMPFile(const char *, int, int, u8 *);
extern void utilApplyIPS(const char *ips, u8 **rom, int *size);
extern void utilWriteBMP(char *, int, int, u8 *);
extern bool utilIsGBAImage(const char *);
extern bool utilIsGBImage(const char *);
extern bool utilIsZipFile(const char *);
extern bool utilIsGzipFile(const char *);
extern bool utilIsRarFile(const char *);
extern void utilGetBaseName(const char *, char *);
extern IMAGE_TYPE utilFindType(const char *);
extern u8 *utilLoad(const char *,
                    bool (*)(const char*),
                    u8 *,
                    int &);

extern void utilPutDword(u8 *, u32);
extern void utilPutWord(u8 *, u16);
extern void utilWriteData(gzFile, variable_desc *);
extern void utilReadData(gzFile, variable_desc *);
extern int utilReadInt(gzFile);
extern void utilWriteInt(gzFile, int);
extern gzFile utilGzOpen(const char *file, const char *mode);
extern gzFile utilMemGzOpen(char *memory, int available, char *mode);
extern int utilGzWrite(gzFile file, const voidp buffer, unsigned int len);
extern int utilGzRead(gzFile file, voidp buffer, unsigned int len);
extern int utilGzClose(gzFile file);
extern long utilGzMemTell(gzFile file);
extern void utilGBAFindSave(const u8 *, const int);
extern void utilUpdateSystemColorMaps();
#endif
