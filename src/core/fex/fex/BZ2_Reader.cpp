// File_Extractor 1.0.0. http://www.slack.net/~ant/

#if FEX_ENABLE_BZ2

#include "BZ2_Reader.h"

#include "blargg_endian.h"

/* Copyright (C) 2025 Andy Vandijck. This module is free software; you
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

#include <bzlib.h>

BZ2_Reader::BZ2_Reader()
{
	close();
}

BZ2_Reader::~BZ2_Reader()
{ }

static blargg_err_t BZ2_reader_read( void* file, void* out, int* count )
{
	return STATIC_CAST(File_Reader*,file)->read_avail( out, count );
}

blargg_err_t BZ2_Reader::calc_size()
{
	size_  = 0x8000000; // Max cart size
	crc32_ = 0;

    set_remain(size_);
    inflater.get_size(&size_);

    fprintf(stderr, "Calculated BZ2 size: %d\n", size_);

    in->seek(0);

	return blargg_ok;
}

blargg_err_t BZ2_Reader::open( File_Reader* new_in )
{
	close();
	
	in = new_in;
	RETURN_ERR( in->seek( 0 ) );
	RETURN_ERR( inflater.begin( BZ2_reader_read, new_in ) );
	RETURN_ERR( inflater.set_mode( inflater.mode_auto ) );
	RETURN_ERR( calc_size() );
    inflater.end();
    RETURN_ERR( inflater.begin( BZ2_reader_read, new_in ) );
    RETURN_ERR( inflater.set_mode( inflater.mode_auto ) );
	set_remain( size_ );
	
	return blargg_ok;
}

void BZ2_Reader::close()
{
	in = NULL;
	inflater.end();
}

blargg_err_t BZ2_Reader::read_v( void* out, int count )
{
	assert( in );
	int actual = count;
	RETURN_ERR( inflater.read( out, &actual ) );

    if ( actual < size_ ) {
        size_ = actual;
        inflater.resize_buffer(actual);
        set_remain(0);
    }

	return blargg_ok;
}

#endif
