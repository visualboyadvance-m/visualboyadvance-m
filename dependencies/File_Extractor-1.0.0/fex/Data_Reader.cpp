// File_Extractor 1.0.0. http://www.slack.net/~ant/

#include "Data_Reader.h"

#include "blargg_endian.h"
#include <stdio.h>
#include <errno.h>

#if BLARGG_UTF8_PATHS
	#include <windows.h>
#endif

/* Copyright (C) 2005-2009 Shay Green. This module is free software; you
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

// Data_Reader

blargg_err_t Data_Reader::read( void* p, int n )
{
	assert( n >= 0 );
	
	if ( n < 0 )
		return blargg_err_caller;
	
	if ( n <= 0 )
		return blargg_ok;
	
	if ( n > remain() )
		return blargg_err_file_eof;
	
	blargg_err_t err = read_v( p, n );
	if ( !err )
		remain_ -= n;
	
	return err;
}

blargg_err_t Data_Reader::read_avail( void* p, int* n_ )
{
	assert( *n_ >= 0 );
	
	int n = min( *n_, remain() );
	*n_ = 0;
	
	if ( n < 0 )
		return blargg_err_caller;
	
	if ( n <= 0 )
		return blargg_ok;
	
	blargg_err_t err = read_v( p, n );
	if ( !err )
	{
		remain_ -= n;
		*n_ = n;
	}
	
	return err;
}

blargg_err_t Data_Reader::read_avail( void* p, long* n )
{
	int i = STATIC_CAST(int, *n);
	blargg_err_t err = read_avail( p, &i );
	*n = i;
	return err;
}

blargg_err_t Data_Reader::skip_v( int count )
{
	char buf [512];
	while ( count )
	{
		int n = min( count, (int) sizeof buf );
		count -= n;
		RETURN_ERR( read_v( buf, n ) );
	}
	return blargg_ok;
}

blargg_err_t Data_Reader::skip( int n )
{
	assert( n >= 0 );
	
	if ( n < 0 )
		return blargg_err_caller;
	
	if ( n <= 0 )
		return blargg_ok;
	
	if ( n > remain() )
		return blargg_err_file_eof;
	
	blargg_err_t err = skip_v( n );
	if ( !err )
		remain_ -= n;
	
	return err;
}


// File_Reader

blargg_err_t File_Reader::seek( int n )
{
	assert( n >= 0 );
	
	if ( n < 0 )
		return blargg_err_caller;
	
	if ( n == tell() )
		return blargg_ok;
	
	if ( n > size() )
		return blargg_err_file_eof;
	
	blargg_err_t err = seek_v( n );
	if ( !err )
		set_tell( n );
	
	return err;
}

blargg_err_t File_Reader::skip_v( int n )
{
	return seek_v( tell() + n );
}


// Subset_Reader

Subset_Reader::Subset_Reader( Data_Reader* dr, int size ) :
	in( dr )
{
	set_remain( min( size, dr->remain() ) );
}

blargg_err_t Subset_Reader::read_v( void* p, int s )
{
	return in->read( p, s );
}


// Remaining_Reader

Remaining_Reader::Remaining_Reader( void const* h, int size, Data_Reader* r ) :
	in( r )
{
	header        = h;
	header_remain = size;
	
	set_remain( size + r->remain() );
}

blargg_err_t Remaining_Reader::read_v( void* out, int count )
{
	int first = min( count, header_remain );
	if ( first )
	{
		memcpy( out, header, first );
		header = STATIC_CAST(char const*, header) + first;
		header_remain -= first;
	}
	
	return in->read( STATIC_CAST(char*, out) + first, count - first );
}


// Mem_File_Reader

Mem_File_Reader::Mem_File_Reader( const void* p, long s ) :
	begin( STATIC_CAST(const char*, p) )
{
	set_size( s );
}

blargg_err_t Mem_File_Reader::read_v( void* p, int s )
{
	memcpy( p, begin + tell(), s );
	return blargg_ok;
}

blargg_err_t Mem_File_Reader::seek_v( int )
{
	return blargg_ok;
}


// Callback_Reader

Callback_Reader::Callback_Reader( callback_t c, long s, void* d ) :
	callback( c ),
	user_data( d )
{
	set_remain( s );
}

blargg_err_t Callback_Reader::read_v( void* out, int count )
{
	return callback( user_data, out, count );
}


// Callback_File_Reader

Callback_File_Reader::Callback_File_Reader( callback_t c, long s, void* d ) :
	callback( c ),
	user_data( d )
{
	set_size( s );
}

blargg_err_t Callback_File_Reader::read_v( void* out, int count )
{
	return callback( user_data, out, count, tell() );
}

blargg_err_t Callback_File_Reader::seek_v( int )
{
	return blargg_ok;
}


// BLARGG_UTF8_PATHS

#if BLARGG_UTF8_PATHS

// Thanks to byuu for the idea for BLARGG_UTF8_PATHS and the implementations

// Converts wide-character path to UTF-8. Free result with free(). Only supported on Windows.
char* blargg_to_utf8( const wchar_t* wpath )
{
	if ( wpath == NULL )
		return NULL;
	
	int needed = WideCharToMultiByte( CP_UTF8, 0, wpath, -1, NULL, 0, NULL, NULL );
	if ( needed <= 0 )
		return NULL;
	
	char* path = (char*) malloc( needed );
	if ( path == NULL )
		return NULL;
	
	int actual = WideCharToMultiByte( CP_UTF8, 0, wpath, -1, path, needed, NULL, NULL );
	if ( actual == 0 )
	{
		free( path );
		return NULL;
	}
	
	assert( actual == needed );
	return path;
}

// Converts UTF-8 path to wide-character. Free result with free() Only supported on Windows.
wchar_t* blargg_to_wide( const char* path )
{
	if ( path == NULL )
		return NULL;
	
	int needed = MultiByteToWideChar( CP_UTF8, 0, path, -1, NULL, 0 );
	if ( needed <= 0 )
		return NULL;
	
	wchar_t* wpath = (wchar_t*) malloc( needed * sizeof *wpath );
	if ( wpath == NULL )
		return NULL;
	
	int actual = MultiByteToWideChar( CP_UTF8, 0, path, -1, wpath, needed );
	if ( actual == 0 )
	{
		free( wpath );
		return NULL;
	}
	
	assert( actual == needed );
	return wpath;
}

static FILE* blargg_fopen( const char path [], const char mode [] )
{
	FILE* file = NULL;
	wchar_t* wmode = NULL;
	wchar_t* wpath = NULL;
	
	wpath = blargg_to_wide( path );
	if ( wpath )
	{
		wmode = blargg_to_wide( mode );
		if ( wmode )
			file = _wfopen( wpath, wmode );
	}
	
	// Save and restore errno in case free() clears it
	int saved_errno = errno;
	free( wmode );
	free( wpath );
	errno = saved_errno;
	
	return file;
}

#else

static inline FILE* blargg_fopen( const char path [], const char mode [] )
{
	return fopen( path, mode );
}

#endif


// Std_File_Reader

Std_File_Reader::Std_File_Reader()
{
	file_ = NULL;
}

Std_File_Reader::~Std_File_Reader()
{
	close();
}

static blargg_err_t blargg_fopen( FILE** out, const char path [] )
{
	errno = 0;
	*out = blargg_fopen( path, "rb" );
	if ( !*out )
	{
		#ifdef ENOENT
			if ( errno == ENOENT )
				return blargg_err_file_missing;
		#endif
		#ifdef ENOMEM
			if ( errno == ENOMEM )
				return blargg_err_memory;
		#endif
		return blargg_err_file_read;
	}
	
	return blargg_ok;
}

static blargg_err_t blargg_fsize( FILE* f, long* out )
{
	if ( fseek( f, 0, SEEK_END ) )
		return blargg_err_file_io;
	
	*out = ftell( f );
	if ( *out < 0 )
		return blargg_err_file_io;
	
	if ( fseek( f, 0, SEEK_SET ) )
		return blargg_err_file_io;
	
	return blargg_ok;
}

blargg_err_t Std_File_Reader::open( const char path [] )
{
	close();
	
	FILE* f;
	RETURN_ERR( blargg_fopen( &f, path ) );
	
	long s;
	blargg_err_t err = blargg_fsize( f, &s );
	if ( err )
	{
		fclose( f );
		return err;
	}
	
	file_ = f;
	set_size( s );
	
	return blargg_ok;
}

void Std_File_Reader::make_unbuffered()
{
	if ( setvbuf( STATIC_CAST(FILE*, file_), NULL, _IONBF, 0 ) )
		check( false ); // shouldn't fail, but OK if it does
}

blargg_err_t Std_File_Reader::read_v( void* p, int s )
{
	if ( (size_t) s != fread( p, 1, s, STATIC_CAST(FILE*, file_) ) )
	{
		// Data_Reader's wrapper should prevent EOF
		check( !feof( STATIC_CAST(FILE*, file_) ) );
		
		return blargg_err_file_io;
	}
	
	return blargg_ok;
}

blargg_err_t Std_File_Reader::seek_v( int n )
{
	if ( fseek( STATIC_CAST(FILE*, file_), n, SEEK_SET ) )
	{
		// Data_Reader's wrapper should prevent EOF
		check( !feof( STATIC_CAST(FILE*, file_) ) );
		
		return blargg_err_file_io;
	}
	
	return blargg_ok;
}

void Std_File_Reader::close()
{
	if ( file_ )
	{
		fclose( STATIC_CAST(FILE*, file_) );
		file_ = NULL;
	}
}


// Gzip_File_Reader

#ifdef HAVE_ZLIB_H

#include "zlib.h"

static const char* get_gzip_eof( const char path [], long* eof )
{
	FILE* file;
	RETURN_ERR( blargg_fopen( &file, path ) );

	int const h_size = 4;
	unsigned char h [h_size];
	
	// read four bytes to ensure that we can seek to -4 later
	if ( fread( h, 1, h_size, file ) != (size_t) h_size || h[0] != 0x1F || h[1] != 0x8B )
	{
		// Not gzipped
		if ( ferror( file ) )
			return blargg_err_file_io;
		
		if ( fseek( file, 0, SEEK_END ) )
			return blargg_err_file_io;
		
		*eof = ftell( file );
		if ( *eof < 0 )
			return blargg_err_file_io;
	}
	else
	{
		// Gzipped; get uncompressed size from end
		if ( fseek( file, -h_size, SEEK_END ) )
			return blargg_err_file_io;
		
		if ( fread( h, 1, h_size, file ) != (size_t) h_size )
			return blargg_err_file_io;
		
		*eof = get_le32( h );
	}
	
	if ( fclose( file ) )
		check( false );
	
	return blargg_ok;
}

Gzip_File_Reader::Gzip_File_Reader()
{
	file_ = NULL;
}

Gzip_File_Reader::~Gzip_File_Reader()
{
	close();
}

blargg_err_t Gzip_File_Reader::open( const char path [] )
{
	close();
	
	long s;
	RETURN_ERR( get_gzip_eof( path, &s ) );

	file_ = gzopen( path, "rb" );
	if ( !file_ )
		return blargg_err_file_read;
	
	set_size( s );
	return blargg_ok;
}

static blargg_err_t convert_gz_error( gzFile file )
{
	int err;
	gzerror( file, &err );
	
	switch ( err )
	{
	case Z_STREAM_ERROR:    break;
	case Z_DATA_ERROR:      return blargg_err_file_corrupt;
	case Z_MEM_ERROR:       return blargg_err_memory;
	case Z_BUF_ERROR:       break;
	}
	return blargg_err_internal;
}

blargg_err_t Gzip_File_Reader::read_v( void* p, int s )
{
	int result = gzread( file_, p, s );
	if ( result != s )
	{
		if ( result < 0 )
			return convert_gz_error( file_ );
		
		return blargg_err_file_corrupt;
	}
	
	return blargg_ok;
}

blargg_err_t Gzip_File_Reader::seek_v( int n )
{
	if ( gzseek( file_, n, SEEK_SET ) < 0 )
		return convert_gz_error( file_ );

	return blargg_ok;
}

void Gzip_File_Reader::close()
{
	if ( file_ )
	{
		if ( gzclose( file_ ) )
			check( false );
		file_ = NULL;
	}
}

#endif
