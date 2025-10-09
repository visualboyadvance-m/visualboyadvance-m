/*
    VisualBoyAdvance - a Game Boy & Game Boy Advance emulator

    Copyright (C) 2008 VBA-M development team


    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

	hq filter by Maxim Stepin ( http://hiend3d.com )
*/


#include "hq_shared.h"


#define _16BIT
	#define _HQ3X
	// hq3x, 16bit
	#include "hq_base.h"
	#undef _HQ3X

	#define _HQ4X
	// hq4x, 16bit
	#include "hq_base.h"
	#undef _HQ4X
#undef _16BIT


#define _32BIT
	#define _HQ3X
	// hq3x, 32bit
	#include "hq_base.h"
	#undef _HQ3X

	#define _HQ4X
	// hq4x, 32bit
	#include "hq_base.h"
	#undef _HQ4X
#undef _32BIT



#undef GMASK
#undef RBMASK
#undef GSHIFT1MASK
#undef RBSHIFT1MASK
#undef GSHIFT2MASK
#undef RBSHIFT2MASK
#undef GSHIFT3MASK
#undef RBSHIFT3MASK
#undef GSHIFT4MASK
#undef RBSHIFT4MASK
