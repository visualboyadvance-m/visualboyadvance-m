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

#ifndef VBA_REG_H
#define VBA_REG_H

extern bool regEnabled;

char *regQueryStringValue(const char *key, char *def);
DWORD regQueryDwordValue(const char *key, DWORD def, bool force=false);
BOOL regQueryBinaryValue(const char *key, char *value, int count);
void regSetStringValue(const char *key,const char *value);
void regSetDwordValue(const char *key,DWORD value,bool force=false);
void regSetBinaryValue(const char *key, char *value, int count);
void regDeleteValue(char *key);
void regInit(const char *);
void regShutdown();
bool regCreateFileType(const char *ext, const char *type);
bool regAssociateType(const char *type, const char *desc, const char *application);
void regExportSettingsToINI();
#endif // VBA_REG_H
