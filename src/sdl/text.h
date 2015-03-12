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
// Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

extern void drawText(u8 *, int, int, int, const char *, bool);

//Each char is 8 * 8 pixels in size

/*
 * Draw Text directly to a buffer
 *
 * Now auto wraps if there is too much text.
 * WARNING:  There are no limits to keep you from going out of bounds.  Passing too much text is dangerous.
 */
void drawText(u8 *screen, int width,int bytes_per_pixel, int x, int y,
                             const char *string, bool trans);
