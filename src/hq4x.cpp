/*
 * This file is part of the Advance project.
 *
 * Copyright (C) 2003 Andrea Mazzoleni
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

#include "System.h"
#include "portable.h"

#include "hq4x.h"

#include <assert.h>

/***************************************************************************/
/* HQ4x C implementation */

/*
 * This effect is a rewritten implementation of the hq4x effect made by Maxim Stepin
 */

void hq4x_16_def(interp_uint16*  dst0, interp_uint16*  dst1, interp_uint16*  dst2, interp_uint16*  dst3, const interp_uint16*  src0, const interp_uint16*  src1, const interp_uint16*  src2, unsigned count)
{
	unsigned i;

	for(i=0;i<count;++i) {
		unsigned char mask;

		interp_uint16 c[9];

		c[1] = src0[0];
		c[4] = src1[0];
		c[7] = src2[0];

		if (i>0) {
			c[0] = src0[-1];
			c[3] = src1[-1];
			c[6] = src2[-1];
		} else {
			c[0] = c[1];
			c[3] = c[4];
			c[6] = c[7];
		}

		if (i<count-1) {
			c[2] = src0[1];
			c[5] = src1[1];
			c[8] = src2[1];
		} else {
			c[2] = c[1];
			c[5] = c[4];
			c[8] = c[7];
		}

		mask = 0;

		if (interp_16_diff(c[0], c[4]))
			mask |= 1 << 0;
		if (interp_16_diff(c[1], c[4]))
			mask |= 1 << 1;
		if (interp_16_diff(c[2], c[4]))
			mask |= 1 << 2;
		if (interp_16_diff(c[3], c[4]))
			mask |= 1 << 3;
		if (interp_16_diff(c[5], c[4]))
			mask |= 1 << 4;
		if (interp_16_diff(c[6], c[4]))
			mask |= 1 << 5;
		if (interp_16_diff(c[7], c[4]))
			mask |= 1 << 6;
		if (interp_16_diff(c[8], c[4]))
			mask |= 1 << 7;

#define P(a, b) dst##b[a]
#define MUR interp_16_diff(c[1], c[5])
#define MDR interp_16_diff(c[5], c[7])
#define MDL interp_16_diff(c[7], c[3])
#define MUL interp_16_diff(c[3], c[1])
#define I1(p0) c[p0]
#define I2(i0, i1, p0, p1) interp_16_##i0##i1(c[p0], c[p1])
#define I3(i0, i1, i2, p0, p1, p2) interp_16_##i0##i1##i2(c[p0], c[p1], c[p2])

		switch (mask) {
		#include "hq4x.dat"
		}

#undef P
#undef MUR
#undef MDR
#undef MDL
#undef MUL
#undef I1
#undef I2
#undef I3

		src0 += 1;
		src1 += 1;
		src2 += 1;
		dst0 += 4;
		dst1 += 4;
		dst2 += 4;
		dst3 += 4;
	}
}

void hq4x_32_def(interp_uint32*  dst0, interp_uint32*  dst1, interp_uint32*  dst2, interp_uint32*  dst3, const interp_uint32*  src0, const interp_uint32*  src1, const interp_uint32*  src2, unsigned count)
{
	unsigned i;

	for(i=0;i<count;++i) {
		unsigned char mask;

		interp_uint32 c[9];

		c[1] = src0[0];
		c[4] = src1[0];
		c[7] = src2[0];

		if (i>0) {
			c[0] = src0[-1];
			c[3] = src1[-1];
			c[6] = src2[-1];
		} else {
			c[0] = c[1];
			c[3] = c[4];
			c[6] = c[7];
		}

		if (i<count-1) {
			c[2] = src0[1];
			c[5] = src1[1];
			c[8] = src2[1];
		} else {
			c[2] = c[1];
			c[5] = c[4];
			c[8] = c[7];
		}

		mask = 0;

		if (interp_32_diff(c[0], c[4]))
			mask |= 1 << 0;
		if (interp_32_diff(c[1], c[4]))
			mask |= 1 << 1;
		if (interp_32_diff(c[2], c[4]))
			mask |= 1 << 2;
		if (interp_32_diff(c[3], c[4]))
			mask |= 1 << 3;
		if (interp_32_diff(c[5], c[4]))
			mask |= 1 << 4;
		if (interp_32_diff(c[6], c[4]))
			mask |= 1 << 5;
		if (interp_32_diff(c[7], c[4]))
			mask |= 1 << 6;
		if (interp_32_diff(c[8], c[4]))
			mask |= 1 << 7;

#define P(a, b) dst##b[a]
#define MUR interp_32_diff(c[1], c[5])
#define MDR interp_32_diff(c[5], c[7])
#define MDL interp_32_diff(c[7], c[3])
#define MUL interp_32_diff(c[3], c[1])
#define I1(p0) c[p0]
#define I2(i0, i1, p0, p1) interp_32_##i0##i1(c[p0], c[p1])
#define I3(i0, i1, i2, p0, p1, p2) interp_32_##i0##i1##i2(c[p0], c[p1], c[p2])

		switch (mask) {
		#include "hq4x.dat"
		}

#undef P
#undef MUR
#undef MDR
#undef MDL
#undef MUL
#undef I1
#undef I2
#undef I3

		src0 += 1;
		src1 += 1;
		src2 += 1;
		dst0 += 4;
		dst1 += 4;
		dst2 += 4;
		dst3 += 4;
	}
}

