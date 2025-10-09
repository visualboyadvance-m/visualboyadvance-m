// File_Extractor 1.0.0. http://www.slack.net/~ant/

#if FEX_ENABLE_LZMA

#include <stdio.h>

#include "XZ_Reader.h"

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

XZ_Reader::XZ_Reader()
{
	close();
}

XZ_Reader::~XZ_Reader()
{ }

static blargg_err_t XZ_reader_read( void* file, void* out, int* count )
{
	return STATIC_CAST(File_Reader*,file)->read_avail( out, count );
}

size_t XZ_Reader::get_uncompressed_size()
{
   lzma_stream_flags stream_flags;

   const uint8_t *footer_ptr = NULL;
   const uint8_t *index_ptr = NULL;
   const uint8_t *data = (const uint8_t *)malloc(in->size());

   if (data == NULL) {
        fprintf(stderr, "Error: Couldn't allocate data\n");
        return 0;
   }

   in->seek(0);
   in->read((void *)data, in->size());

   // 12 is the size of the footer per the file-spec...
   footer_ptr = data + (in->size() - 12);

   // Decode the footer, so we have the backward_size pointing to the index
   (void)lzma_stream_footer_decode(&stream_flags, (const uint8_t *)footer_ptr);
   // This is the index pointer, where the size is ultimately stored...
   index_ptr = data + ((in->size() - 12) - stream_flags.backward_size);

   // Allocate an index
   lzma_index *index = lzma_index_init(NULL);
   uint64_t memlimit;
   size_t in_pos = 0;
   // decode the index we calculated
   lzma_index_buffer_decode(&index, &memlimit, NULL, (const uint8_t *)index_ptr, &in_pos, footer_ptr - index_ptr);
   // Just make sure the whole index was decoded, otherwise, we might be
   // dealing with something utterly corrupt
   if (in_pos != stream_flags.backward_size) {
     lzma_index_end(index, NULL);
     free((void *)data);
     fprintf(stderr, "Error: input position %u is not equal to backward size %llu\n", in_pos, stream_flags.backward_size);
     return 0;
   }

   // Finally get the size
   lzma_vli uSize = lzma_index_uncompressed_size(index);
   lzma_index_end(index, NULL);

   free((void *)data);
   in->seek(0);

   return (size_t) uSize;
}

blargg_err_t XZ_Reader::calc_size()
{
	size_  = (int)get_uncompressed_size();
    fprintf(stderr, "XZ uncompressed size: %d\n", size_);

	crc32_ = 0;
	return blargg_ok;
}

blargg_err_t XZ_Reader::open( File_Reader* new_in )
{
	close();
	
	in = new_in;
	RETURN_ERR( in->seek( 0 ) );
	RETURN_ERR( inflater.begin( XZ_reader_read, new_in ) );
	RETURN_ERR( inflater.set_mode( inflater.mode_auto ) );
	RETURN_ERR( calc_size() );
	set_remain( size_ );
	
	return blargg_ok;
}

void XZ_Reader::close()
{
	in = NULL;
	inflater.end();
}

blargg_err_t XZ_Reader::read_v( void* out, int count )
{
	assert( in );
	int actual = count;
	RETURN_ERR( inflater.read( out, &actual ) );

    fprintf(stderr, "XZ: Actual read: %d, count: %d\n", actual, count);

	if ( actual != count )
        return blargg_err_file_corrupt;

	return blargg_ok;
}

#endif
