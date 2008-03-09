// VisualBoyAdvance - Nintendo Gameboy/GameboyAdvance (TM) emulator.
// Copyright (C) 1999-2003 Forgotten
// Copyright (C) 2004 Forgotten and the VBA development team
// Copyright (C) 2007-2008 VBA-M development team

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

#pragma once

// Exclude rarely-used stuff from Windows headers like
// Cryptography, DDE, RPC, Shell, and Windows Sockets
#define WIN32_LEAN_AND_MEAN

// Target for Windows 2000 (Required by GetMenuBar and other functions)
#define NTDDI_VERSION NTDDI_WIN2K // new
#define _WIN32_WINNT 0x0500       // old

// Enable STRICT type checking
#define STRICT

#include <afxwin.h>
#include <afxcmn.h>
#include <afxdlgs.h>
#include "VBA.h"
#include "resource.h"
