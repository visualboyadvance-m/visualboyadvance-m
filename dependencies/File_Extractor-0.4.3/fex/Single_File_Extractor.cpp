// File_Extractor 0.4.3. http://www.slack.net/~ant/

#include "Single_File_Extractor.h"

#include <string.h>

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

Single_File_Extractor::Single_File_Extractor() : File_Extractor( fex_bin_type )
{
	size_ = 0;
}

extern "C" {
	static File_Extractor* new_single_file() { return BLARGG_NEW Single_File_Extractor; }
}

fex_type_t_ const fex_gz_type  [1] = {{ "GZ", &new_single_file }};
fex_type_t_ const fex_bin_type [1] = {{ ""  , &new_single_file }};

Single_File_Extractor::~Single_File_Extractor() { close(); }

void Single_File_Extractor::close_() { gr.close();  }

void Single_File_Extractor::set_name( const char* n )
{
	name [sizeof name-1] = 0;
	strncpy( name, n, sizeof name-1 );
}

void Single_File_Extractor::open_filter_( const char* path, Std_File_Reader* file )
{
	file->make_unbuffered(); // faster since Gzip_Reader already buffers data

	const char* name = strrchr( path, '\\' ); // DOS
	if ( !name )
		name = strrchr( path, '/' ); // UNIX
	if ( !name )
		name = strrchr( path, ':' ); // Mac
	if ( !name )
		name = path;
	set_name( name );
}

blargg_err_t Single_File_Extractor::open_()
{
	RETURN_ERR( gr.open( &file() ) );

	size_ = gr.remain();
	if ( size_ < 0 )
		return "Read error";

	set_info( size_, name );
	return 0;
}

blargg_err_t Single_File_Extractor::rewind_()
{
	if ( gr.tell() )
	{
		RETURN_ERR( file().seek( 0 ) );
		RETURN_ERR( gr.open( &file() ) );
	}
	set_info( size_, name );
	return 0;
}

blargg_err_t Single_File_Extractor::next_()
{
	set_done();
	return 0;
}

long         Single_File_Extractor::remain    () const            { return size_ - gr.tell(); }
long         Single_File_Extractor::read_avail( void* p, long n ) { return gr.read_avail( p, n ); }
blargg_err_t Single_File_Extractor::read      ( void* p, long n ) { return gr.read( p, n ); }
blargg_err_t Single_File_Extractor::read_once ( void* p, long n ) { return read( p, n ); }
