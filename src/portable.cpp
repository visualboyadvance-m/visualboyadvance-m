/*
 * This file is part of the Advance project.
 *
 * Copyright (C) 1999, 2000, 2001, 2002, 2003 Andrea Mazzoleni
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

#include "portable.h"

#ifdef __MSDOS__
int rpl_snprintf(char* str, size_t count, const char* fmt, ...)
{
	int r;

	/* Note that the snprintf implementation of "Patrick Powell 1995" has */
	/* various bugs on %f, %g and %e for example snprintf("%f",1.01) -> 1.1 */
	va_list arg;
	va_start(arg, fmt);
	r = vsprintf(str, fmt, arg);
	va_end(arg);

	return r;
}

int rpl_vsnprintf(char* str, size_t count, const char* fmt, va_list arg)
{
	return vsprintf(str, fmt, arg);
}
#endif

#ifdef __WIN32__
double rpl_asinh(double x)
{
	return log(x + sqrt(x*x + 1));
}

double rpl_acosh(double x)
{
	return log(x + sqrt(x*x - 1));
}

double rpl_logb(double x)
{
	return floor(log(x) / 0.69314718055994530942);
}

int rpl_isnan(double x)
{
	/* NaNs are the only values unordered */
	return x != x;
}

int rpl_isunordered(double x, double y)
{
	return rpl_isnan(x) || rpl_isnan(y);
}
#endif

