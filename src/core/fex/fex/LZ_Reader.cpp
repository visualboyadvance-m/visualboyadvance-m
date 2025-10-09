// File_Extractor 1.0.0. http://www.slack.net/~ant/

#if FEX_ENABLE_LZMA

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

size_t LZ_Reader::get_uncompressed_size()
{
   uint8_t *header_ptr = (uint8_t *)malloc(header_size);
   uint8_t *trailer_ptr = (uint8_t *)malloc(trailer_size);

   if ((header_ptr == NULL) || (trailer_ptr == NULL)) {
        fprintf(stderr, "Error: Couldn't allocate data\n");
        return 0;
   }

   in->seek(0);
   in->read((void *)header_ptr, header_size);

   unsigned dict_size = 1 << (header_ptr[5] & 0x1F);
   dict_size -= (dict_size / 16) * ((header_ptr[5] >> 5 ) & 7);

   if(dict_size < (1 << 12) || dict_size > (1 << 29))
   {
      fprintf(stderr, "Invalid dictionary size in member header.\n");
      free((void *)header_ptr);
      free((void *)trailer_ptr);
      return 0;
   }

   in->seek(in->size() - trailer_size);
   in->read((void *)trailer_ptr, trailer_size);

   unsigned long long data_size = 0;
   for (int i = 11; i >= 4; --i)
       data_size = ( data_size << 8 ) + trailer_ptr[i];

   in->seek(0);

   free((void *)header_ptr);
   free((void *)trailer_ptr);

   return (size_t)data_size;
}

blargg_err_t LZ_Reader::calc_size()
{
	size_  = (int)get_uncompressed_size();
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
