/*
 * This file is part of the Advance project.
 *
 * Copyright (C) 1999, 2000, 2001, 2002, 2003, 2004 Andrea Mazzoleni
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

/** \file
 * Functions and defines required for portability.
 */

#ifndef __PORTABLE_H
#define __PORTABLE_H

#if HAVE_CONFIG_H
#include "config.h" /* Use " to include first in the same directory of this file */
#endif

/***************************************************************************/
/* Config */

/* Customize for MSDOS DJGPP */
#ifdef __MSDOS__
#define TIME_WITH_SYS_TIME 1
#define HAVE_SYS_TIME_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_UNISTD_H 1
#define HAVE_DIRENT_H 1
#define HAVE_SYS_WAIT_H 1
#define restrict __restrict
#endif

/* Customize for Windows Mingw/Cygwin */
#ifdef __WIN32__
#define TIME_WITH_SYS_TIME 1
#define HAVE_SYS_TIME_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_UNISTD_H 1
#define HAVE_DIRENT_H 1
#define restrict __restrict
#endif

/* Include some standard headers */
#include <stdio.h>
#include <stdlib.h> /* On many systems (e.g., Darwin), `stdio.h' is a prerequisite. */
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <math.h>

#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#if TIME_WITH_SYS_TIME
#include <sys/time.h>
#include <time.h>
#else
#if HAVE_SYS_TIME_H
#include <sys/time.h>
#else
#include <time.h>
#endif
#endif

#if HAVE_DIRENT_H
#include <dirent.h>
#define NAMLEN(dirent) strlen((dirent)->d_name)
#else
#define dirent direct
#define NAMLEN(dirent) (dirent)->d_namlen
#if HAVE_SYS_NDIR_H
#include <sys/ndir.h>
#endif
#if HAVE_SYS_DIR_H
#include <sys/dir.h>
#endif
#if HAVE_NDIR_H
#include <ndir.h>
#endif
#endif

#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#if HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#ifdef __WIN32__
#ifndef WEXITSTATUS
#define WEXITSTATUS(r) (r)
#endif
#ifndef WIFEXITED
#define WIFEXITED(r) 1
#endif
#else
#ifndef WEXITSTATUS
#define WEXITSTATUS(r) ((unsigned)(r) >> 8)
#endif
#ifndef WIFEXITED
#define WIFEXITED(r) (((r) & 255) == 0)
#endif
#endif

#ifndef WIFSTOPPED
#define WIFSTOPPED(r) 0
#endif
#ifndef WIFSIGNALED
#define WIFSIGNALED(r) 0
#endif
#ifndef WTERMSIG
#define WTERMSIG(r) 0
#endif
#ifndef WSTOPSIG
#define WSTOPSIG(r) 0
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __MSDOS__
int rpl_snprintf(char* str, size_t count, const char* fmt, ...);
int rpl_vsnprintf(char* str, size_t count, const char* fmt, va_list arg);
#define snprintf rpl_snprintf
#define vsnprintf rpl_vsnprintf
#endif

#ifdef __WIN32__
#define snprintf _snprintf
#define vsnprintf _vsnprintf
#endif

#ifdef __WIN32__
#if (__GNUC__ == 2)
/* math functions for gcc for Windows */
int rpl_isnan(double x);
int rpl_isunordered(double x, double y);
#define isnan rpl_isnan
#define isunordered rpl_isunordered
#endif

#if (__GNUC__ == 2) || (__GNUC__ == 3 && __GNUC_MINOR__ <= 2)
/* math functions for gcc for Windows */
double rpl_asinh(double x);
double rpl_acosh(double x);
double rpl_alogb(double x);
#define asinh rpl_asinh
#define acosh rpl_acosh
#define logb rpl_logb
#endif
#endif

/* M_PI isn't a POSIX standard */
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* faster lrint implementation */
#if defined(__GNUC__) && defined(__i386__)
static inline int rpl_lrint(double x)
{
	int r;
	__asm__ __volatile__ (
		"fistpl %0"
		: "=m" (r)
		: "t" (x)
		: "st"
	);
	return r;
}
#else
static inline int rpl_lrint(double x)
{
	return (int)x;
}
#endif
#define lrint rpl_lrint

#ifdef __cplusplus
}
#endif

#endif

