/*
 * This file is part of the Advance project.
 *
 * Copyright (C) 2003, 2004 Andrea Mazzoleni
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * In addition, as a special exception, Andrea Mazzoleni
 * gives permission to link the code of this program with
 * the MAME library (or with modified versions of MAME that use the
 * same license as MAME), and distribute linked combinations including
 * the two.  You must obey the GNU General Public License in all
 * respects for all of the code used other than MAME.  If you modify
 * this file, you may extend this exception to your version of the
 * file, but you are not obligated to do so.  If you do not wish to
 * do so, delete this exception statement from your version.
 */

#ifndef __HQ4X_H
#define __HQ4X_H

#include "interp.h"

void hq4x_16_def(interp_uint16* dst0, interp_uint16* dst1, interp_uint16* dst2, interp_uint16* dst3, const interp_uint16* src0, const interp_uint16* src1, const interp_uint16* src2, unsigned count);
void hq4x_32_def(interp_uint32* dst0, interp_uint32* dst1, interp_uint32* dst2, interp_uint32* dst3, const interp_uint32* src0, const interp_uint32* src1, const interp_uint32* src2, unsigned count);

#endif

