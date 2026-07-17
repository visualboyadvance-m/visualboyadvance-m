// File_Extractor 1.0.0. http://www.slack.net/~ant/

#if FEX_ENABLE_LZMA

#include <climits>
#include <stdio.h>

#include "LZ_Reader.h"

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

LZ_Reader::LZ_Reader()
{
	close();
}

LZ_Reader::~LZ_Reader()
{ }

static blargg_err_t LZ_reader_read( void* file, void* out, int* count )
{
	return STATIC_CAST(File_Reader*,file)->read_avail( out, count );
}

enum { header_size = 6, trailer_size = 20 };

blargg_err_t LZ_Reader::get_uncompressed_size(int& size)
{
   if (in->size() < header_size + trailer_size)
       return blargg_err_file_corrupt;

   uint8_t header[header_size];
   uint8_t trailer[trailer_size];

   RETURN_ERR( in->seek(0) );
   RETURN_ERR( in->read(header, header_size) );

   uint64_t dict_size = (uint64_t)1 << (header[5] & 0x1F);
   dict_size -= (dict_size / 16) * ((header[5] >> 5) & 7);

   if (dict_size < (1 << 12) || dict_size > (1 << 29))
       return blargg_err_file_corrupt;

   RETURN_ERR( in->seek(in->size() - trailer_size) );
   RETURN_ERR( in->read(trailer, trailer_size) );

   uint64_t data_size = 0;
   for (int i = 11; i >= 4; --i)
       data_size = (data_size << 8) + trailer[i];

   if (data_size > (uint64_t)INT_MAX)
       return BLARGG_ERR(BLARGG_ERR_FILE_FEATURE, "LZIP larger than 2GB");

   RETURN_ERR( in->seek(0) );
   size = (int)data_size;
   return blargg_ok;
}

blargg_err_t LZ_Reader::calc_size()
{
    RETURN_ERR( get_uncompressed_size(size_) );
    fprintf(stderr, "LZ uncompressed size: %d\n", size_);

	crc32_ = 0;
	return blargg_ok;
}

blargg_err_t LZ_Reader::open( File_Reader* new_in )
{
	close();
	
	in = new_in;
	RETURN_ERR( in->seek( 0 ) );
	RETURN_ERR( inflater.begin( LZ_reader_read, new_in ) );
	RETURN_ERR( inflater.set_mode( inflater.mode_auto ) );
	RETURN_ERR( calc_size() );
	set_remain( size_ );
	
	return blargg_ok;
}

void LZ_Reader::close()
{
	in = NULL;
	inflater.end();
}

blargg_err_t LZ_Reader::read_v( void* out, int count )
{
	assert( in );
	int actual = count;
	RETURN_ERR( inflater.read( out, &actual ) );

    fprintf(stderr, "LZ: Actual read: %d, count: %d\n", actual, count);

	if ( actual != count )
        return blargg_err_file_corrupt;

	return blargg_ok;
}

#endif
