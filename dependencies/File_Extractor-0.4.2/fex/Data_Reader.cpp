// File_Extractor 0.4.2. http://www.slack.net/~ant/

#include "Data_Reader.h"

#include "blargg_endian.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

/* Copyright (C) 2005-2006 Shay Green. This module is free software; you
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

const char Data_Reader::eof_error [] = "Unexpected end of file";

blargg_err_t Data_Reader::read( void* p, long s )
{
	long result = read_avail( p, s );
	if ( result == s )
		return 0;

	if ( result < 0 )
		return "Read error";

	return eof_error;
}

blargg_err_t Data_Reader::skip( long count )
{
	char buf [512];
	while ( count )
	{
		long n = sizeof buf;
		if ( n > count )
			n = count;
		count -= n;
		RETURN_ERR( read( buf, n ) );
	}
	return 0;
}

long File_Reader::remain() const { return size() - tell(); }

blargg_err_t File_Reader::skip( long n )
{
	assert( n >= 0 );
	if ( !n )
		return 0;
	return seek( tell() + n );
}

// Subset_Reader

Subset_Reader::Subset_Reader( Data_Reader* dr, long size )
{
	in = dr;
	remain_ = dr->remain();
	if ( remain_ > size )
		remain_ = size;
}

long Subset_Reader::remain() const { return remain_; }

long Subset_Reader::read_avail( void* p, long s )
{
	if ( s > remain_ )
		s = remain_;
	remain_ -= s;
	return in->read_avail( p, s );
}

// Remaining_Reader

Remaining_Reader::Remaining_Reader( void const* h, long size, Data_Reader* r )
{
	header = (char const*) h;
	header_end = header + size;
	in = r;
}

long Remaining_Reader::remain() const { return header_end - header + in->remain(); }

long Remaining_Reader::read_first( void* out, long count )
{
	long first = header_end - header;
	if ( first )
	{
		if ( first > count )
			first = count;
		void const* old = header;
		header += first;
		memcpy( out, old, first );
	}
	return first;
}

long Remaining_Reader::read_avail( void* out, long count )
{
	long first = read_first( out, count );
	long second = count - first;
	if ( second )
	{
		second = in->read_avail( (char*) out + first, second );
		if ( second <= 0 )
			return second;
	}
	return first + second;
}

blargg_err_t Remaining_Reader::read( void* out, long count )
{
	long first = read_first( out, count );
	long second = count - first;
	if ( !second )
		return 0;
	return in->read( (char*) out + first, second );
}

// Mem_File_Reader

Mem_File_Reader::Mem_File_Reader( const void* p, long s ) :
	begin( (const char*) p ),
	size_( s )
{
	pos = 0;
}

long Mem_File_Reader::size() const { return size_; }

long Mem_File_Reader::read_avail( void* p, long s )
{
	long r = remain();
	if ( s > r )
		s = r;
	memcpy( p, begin + pos, s );
	pos += s;
	return s;
}

long Mem_File_Reader::tell() const { return pos; }

blargg_err_t Mem_File_Reader::seek( long n )
{
	if ( n > size_ )
		return eof_error;
	pos = n;
	return 0;
}

// Callback_Reader

Callback_Reader::Callback_Reader( callback_t c, long size, void* d ) :
	callback( c ),
	data( d )
{
	remain_ = size;
}

long Callback_Reader::remain() const { return remain_; }

long Callback_Reader::read_avail( void* out, long count )
{
	if ( count > remain_ )
		count = remain_;

	if ( Callback_Reader::read( out, count ) )
		return -1;

	return 0;
}

blargg_err_t Callback_Reader::read( void* out, long count )
{
	if ( count > remain_ )
		return eof_error;
	return callback( data, out, count );
}

// Std_File_Reader

Std_File_Reader::Std_File_Reader() : file_( 0 ) { }

Std_File_Reader::~Std_File_Reader() { close(); }

blargg_err_t Std_File_Reader::open( const char* path )
{
	file_ = fopen( path, "rb" );
	if ( !file_ )
		return "Couldn't open file";
	return 0;
}

void Std_File_Reader::make_unbuffered()
{
	if ( setvbuf( (FILE*) file_, 0, _IONBF, 0 ) )
		check( false ); // shouldn't fail, but OK if it does
}

long Std_File_Reader::size() const
{
	long pos = tell();
	fseek( (FILE*) file_, 0, SEEK_END );
	long result = tell();
	fseek( (FILE*) file_, pos, SEEK_SET );
	return result;
}

long Std_File_Reader::read_avail( void* p, long s )
{
	return fread( p, 1, s, (FILE*) file_ );
}

blargg_err_t Std_File_Reader::read( void* p, long s )
{
	if ( s == (long) fread( p, 1, s, (FILE*) file_ ) )
		return 0;
	if ( feof( (FILE*) file_ ) )
		return eof_error;
	return "Couldn't read from file";
}

long Std_File_Reader::tell() const { return ftell( (FILE*) file_ ); }

blargg_err_t Std_File_Reader::seek( long n )
{
	if ( !fseek( (FILE*) file_, n, SEEK_SET ) )
		return 0;
	if ( n > size() )
		return eof_error;
	return "Error seeking in file";
}

void Std_File_Reader::close()
{
	if ( file_ )
	{
		fclose( (FILE*) file_ );
		file_ = 0;
	}
}

// Gzip_File_Reader

#ifdef HAVE_ZLIB_H

#include "zlib.h"

static const char* get_gzip_eof( const char* path, long* eof )
{
	FILE* file = fopen( path, "rb" );
	if ( !file )
		return "Couldn't open file";

	unsigned char buf [4];
	if ( fread( buf, 2, 1, file ) > 0 && buf [0] == 0x1F && buf [1] == 0x8B )
	{
		fseek( file, -4, SEEK_END );
		fread( buf, 4, 1, file );
		*eof = get_le32( buf );
	}
	else
	{
		fseek( file, 0, SEEK_END );
		*eof = ftell( file );
	}
	const char* err = (ferror( file ) || feof( file )) ? "Couldn't get file size" : 0;
	fclose( file );
	return err;
}

Gzip_File_Reader::Gzip_File_Reader() : file_( 0 ) { }

Gzip_File_Reader::~Gzip_File_Reader() { close(); }

blargg_err_t Gzip_File_Reader::open( const char* path )
{
	close();

	RETURN_ERR( get_gzip_eof( path, &size_ ) );

	file_ = gzopen( path, "rb" );
	if ( !file_ )
		return "Couldn't open file";

	return 0;
}

long Gzip_File_Reader::size() const { return size_; }

long Gzip_File_Reader::read_avail( void* p, long s ) { return gzread( file_, p, s ); }

long Gzip_File_Reader::tell() const { return gztell( file_ ); }

blargg_err_t Gzip_File_Reader::seek( long n )
{
	if ( gzseek( file_, n, SEEK_SET ) >= 0 )
		return 0;

	if ( n > size_ )
		return eof_error;

	return "Error seeking in file";
}

void Gzip_File_Reader::close()
{
	if ( file_ )
	{
		gzclose( file_ );
		file_ = 0;
	}
}

#endif
