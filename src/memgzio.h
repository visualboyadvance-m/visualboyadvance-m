/* gzio.c -- IO on .gz files
 * Copyright (C) 1995-2002 Jean-loup Gailly.
 * For conditions of distribution and use, see copyright notice in zlib.h
 *
 * Compile this file with -DNO_DEFLATE to avoid the compression code.
 */

/* memgzio.c - IO on .gz files in memory
 * Adapted from original gzio.c from zlib library by Forgotten
 */
#include <zutil.h>

gzFile ZEXPORT memgzopen(char *memory, int, const char *);
int ZEXPORT memgzread(gzFile, voidp, unsigned);
int ZEXPORT memgzwrite(gzFile, const voidp, unsigned);
int ZEXPORT memgzclose(gzFile);
long ZEXPORT memtell(gzFile);
