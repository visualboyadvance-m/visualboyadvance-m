// File_Extractor 0.4.2. http://www.slack.net/~ant/

#include "unzip.h"

#include "File_Extractor.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* Copyright (C) 2007 Shay Green. This module is free software; you
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

// log error message to stderr when debugging
inline unzError log_error( const char* str )
{
	if ( !str )
		return UNZ_OK;
	
	#ifndef NDEBUG
		fprintf( stderr, "unzip Error: %s\n", str );
	#endif
	
	return UNZ_INTERNALERROR;
}

int unzStringFileNameCompare( const char* x, const char* y, int case_sensitive )
{
	if ( !case_sensitive )
	{
		case_sensitive = 1;
		#if !defined (unix)
			case_sensitive = 2;
		#endif
	}
	
	if ( case_sensitive == 1 )
		return strcmp( x, y );
	
	int xx, yy;
	do
	{
		xx = tolower( *x++ );
		yy = tolower( *y++ );
	}
	while ( yy && xx == yy );
	return xx - yy;
}

unzFile unzOpen( const char* path )
{
	File_Extractor* fex;
	if ( log_error( fex_open( path, &fex ) ) )
		return 0;
	fex->scan_only();
	return fex;
}

unzError unzClose( unzFile fex )
{
	delete fex;
	return UNZ_OK;
}

unzError unzGoToFirstFile( unzFile fex )
{
	unzError err = log_error( fex->rewind() );
	if ( !err && fex->done() )
		err = UNZ_BADZIPFILE; // unzip interface can't handle an empty file
	fex->scan_only();
	return err;
}

unzError unzGoToNextFile( unzFile fex )
{
	if ( fex->done() )
		return UNZ_END_OF_LIST_OF_FILE;
	
	unzError err = log_error( fex->next() );
	if ( !err && fex->done() )
		err = UNZ_END_OF_LIST_OF_FILE;
	return err;
}

unzError unzLocateFile( unzFile fex, const char* name, int case_sensitive )
{
	unzError err = unzGoToFirstFile( fex );
	if ( !err )
	{
		fex->scan_only( false ); // user is likely to extract file
		do
		{
			if ( !unzStringFileNameCompare( fex->name(), name, case_sensitive ) )
				return UNZ_OK;
		}
		while ( !(err = unzGoToNextFile( fex )) );
	}
	return err;
}

unzError unzGetCurrentFileInfo( unzFile fex, unz_file_info* info_out,
		char* filename_out, uLong filename_size,
		void* extra_out, uLong, char* comment_out, uLong )
{
	// extra and comment aren't supported
	assert( !extra_out && !comment_out );
	
	if ( fex->done() )
		return UNZ_END_OF_LIST_OF_FILE;
	
	size_t name_len = strlen( fex->name() );
	if ( info_out )
	{
		info_out->uncompressed_size = fex->size();
		info_out->size_filename = name_len;
	}
	
	if ( filename_out )
	{
		if ( filename_size <= name_len )
			return UNZ_PARAMERROR;
		strcpy( filename_out, fex->name() );
	}
	
	return UNZ_OK;
}

unzError unzOpenCurrentFile( unzFile fex )
{
	if ( fex->done() )
		return UNZ_END_OF_LIST_OF_FILE;
	
	// fails if you try to re-open file again (I can add support if necessary)
	assert( fex->remain() == fex->size() );
	
	// fex automatically opens on first read
	return UNZ_OK;
}

unzError unzReadCurrentFile( unzFile fex, void* out, unsigned count )
{
	long actual = fex->read_avail( out, count );
	if ( actual < 0 )
		return UNZ_INTERNALERROR;
	return (unzError) actual;
}

z_off_t unztell( unzFile fex )
{
	return fex->size() - fex->remain();
}

int unzeof( unzFile fex )
{
	return !fex->remain();
}

unzError unzCloseCurrentFile( unzFile )
{
	// fex automatically closes
	return UNZ_OK;
}
