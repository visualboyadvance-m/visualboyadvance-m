// File_Extractor 0.4.3. http://www.slack.net/~ant/

#include "abstract_file.h"

#include <string.h>
#include <stdio.h>

/* Copyright (C) 2005-2007 Shay Green. This module is free software; you
can redistribute it and/or modify it under the terms of the GNU Lesser
General Public License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version. This
module is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
details. You should have received a copy of the GNU Lesser General Public
License along with this module; if not, write to the Free Software Foundation,
Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA */

#include "blargg_source.h"

Data_Writer::~Data_Writer() { }

// Std_File_Writer

Std_File_Writer::Std_File_Writer() : file_( 0 ) { }

Std_File_Writer::~Std_File_Writer() { close(); }

blargg_err_t Std_File_Writer::open( const char* path )
{
	file_ = fopen( path, "wb" );
	if ( !file_ )
		return "Couldn't open file for writing";

	// TODO: increase file buffer size?
	//setvbuf( file_, 0, _IOFBF, 32 * 1024L );

	return 0;
}

blargg_err_t Std_File_Writer::write( const void* p, long s )
{
	long result = fwrite( p, 1, s, (FILE*) file_ );
	if ( result != s )
		return "Couldn't write to file";
	return 0;
}

void Std_File_Writer::close()
{
	if ( file_ )
	{
		fclose( (FILE*) file_ );
		file_ = 0;
	}
}

// Mem_Writer

Mem_Writer::Mem_Writer( void* p, long s, int b )
{
	data_     = (char*) p;
	size_     = 0;
	allocated = s;
	mode      = b ? ignore_excess : fixed;
}

Mem_Writer::Mem_Writer()
{
	data_     = 0;
	size_     = 0;
	allocated = 0;
	mode      = expanding;
}

Mem_Writer::~Mem_Writer()
{
	if ( mode == expanding )
		free( data_ );
}

blargg_err_t Mem_Writer::write( const void* p, long s )
{
	long remain = allocated - size_;
	if ( s > remain )
	{
		if ( mode == fixed )
			return "Tried to write more data than expected";

		if ( mode == ignore_excess )
		{
			s = remain;
		}
		else // expanding
		{
			long new_allocated = size_ + s;
			new_allocated += (new_allocated >> 1) + 2048;

			void* p = realloc( data_, new_allocated );
			if ( !p )
				return "Out of memory";

			data_     = (char*) p;
			allocated = new_allocated;
		}
	}

	assert( size_ + s <= allocated );
	memcpy( data_ + size_, p, s );
	size_ += s;

	return 0;
}

// Null_Writer

blargg_err_t Null_Writer::write( const void*, long ) { return 0; }

// Gzip_File_Writer

#ifdef HAVE_ZLIB_H
#include "zlib.h"

Gzip_File_Writer::Gzip_File_Writer() : file_( 0 ) { }

Gzip_File_Writer::~Gzip_File_Writer() { close(); }

blargg_err_t Gzip_File_Writer::open( const char* path, int level )
{
	close();
	char mode [4] = "wb\0";
	if ( (unsigned) level <= 9 )
		mode [2] = '0' + level;
	file_ = gzopen( path, mode );
	if ( !file_ )
		return "Couldn't open file for writing";
	return 0;
}

blargg_err_t Gzip_File_Writer::write( const void* p, long s )
{
	if ( s != gzwrite( file_ , (void*) p, s ) )
		return "Couldn't write to file";
	return 0;
}

void Gzip_File_Writer::close()
{
	if ( file_ )
	{
		gzclose( file_ );
		file_ = 0;
	}
}
#endif
