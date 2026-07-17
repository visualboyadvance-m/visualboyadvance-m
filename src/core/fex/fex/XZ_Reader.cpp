// File_Extractor 1.0.0. http://www.slack.net/~ant/

#if FEX_ENABLE_LZMA

#include <climits>
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

blargg_err_t XZ_Reader::get_uncompressed_size(int& size)
{
   enum { footer_size = 12 };
   const int file_size = in->size();
   if (file_size < footer_size)
       return blargg_err_file_corrupt;

   uint8_t* data = (uint8_t*)malloc((size_t)file_size);

   if (data == NULL) {
        return BLARGG_ERR(BLARGG_ERR_MEMORY, "couldn't allocate XZ data");
   }

   blargg_err_t err = in->seek(0);
   if (err) {
       free(data);
       return err;
   }
   err = in->read(data, file_size);
   if (err) {
       free(data);
       return err;
   }

   // 12 is the size of the footer per the file-spec...
   const uint8_t* footer_ptr = data + (file_size - footer_size);

   // Decode the footer, so we have the backward_size pointing to the index
   lzma_stream_flags stream_flags;
   if (lzma_stream_footer_decode(&stream_flags, footer_ptr) != LZMA_OK) {
       free(data);
       return blargg_err_file_corrupt;
   }

   if (stream_flags.backward_size == 0
       || stream_flags.backward_size > (lzma_vli)(footer_ptr - data)) {
       free(data);
       return blargg_err_file_corrupt;
   }

   // This is the index pointer, where the size is ultimately stored...
   const uint8_t* index_ptr = footer_ptr - stream_flags.backward_size;

   // Allocate an index
   lzma_index* index = NULL;
   uint64_t memlimit = ~(uint64_t)0;
   size_t in_pos = 0;
   // decode the index we calculated
   lzma_ret result = lzma_index_buffer_decode(&index, &memlimit, NULL,
       index_ptr, &in_pos, (size_t)stream_flags.backward_size);
   // Just make sure the whole index was decoded, otherwise, we might be
   // dealing with something utterly corrupt
   if (result != LZMA_OK || index == NULL
       || in_pos != (size_t)stream_flags.backward_size) {
     if (index != NULL)
       lzma_index_end(index, NULL);
     free((void *)data);
     return blargg_err_file_corrupt;
   }

   // Finally get the size
   lzma_vli uSize = lzma_index_uncompressed_size(index);
   lzma_index_end(index, NULL);

   free((void *)data);
   if (uSize > (lzma_vli)INT_MAX)
       return BLARGG_ERR(BLARGG_ERR_FILE_FEATURE, "XZ larger than 2GB");

   RETURN_ERR( in->seek(0) );
   size = (int)uSize;
   return blargg_ok;
}

blargg_err_t XZ_Reader::calc_size()
{
    RETURN_ERR( get_uncompressed_size(size_) );
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
