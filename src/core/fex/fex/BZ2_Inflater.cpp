// File_Extractor 1.0.0. http://www.slack.net/~ant/

#if FEX_ENABLE_BZ2

#include "BZ2_Inflater.h"

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

int const block_size = 4096;

static const char* get_bz2_err( int code )
{
	assert( code != Z_OK );
	switch ( code )
	{
	case BZ_MEM_ERROR:   return blargg_err_memory;
	case BZ_DATA_ERROR:  return blargg_err_file_corrupt;
	// TODO: handle more error codes
	}

	const char* str = BLARGG_ERR( BLARGG_ERR_GENERIC, "problem unzipping data" );
	
	return str;
}

void BZ2_Inflater::end()
{
	if ( deflated_ )
	{
		deflated_ = false;
		if ( BZ2_bzDecompressEnd ( &zbuf ) )
			check( false );
	}
	buf.clear();

	static bz_stream const empty = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	memcpy( &zbuf, &empty, sizeof zbuf );
}

BZ2_Inflater::BZ2_Inflater()
{
	deflated_ = false;
	end(); // initialize things
}

BZ2_Inflater::~BZ2_Inflater()
{
	end();
}

blargg_err_t BZ2_Inflater::fill_buf( int count )
{
	byte* out = buf.end() - count;
	RETURN_ERR( callback( user_data, out, &count ) );
	zbuf.avail_in = count;
	zbuf.next_in  = (char *)out;
	return blargg_ok;
}

blargg_err_t BZ2_Inflater::begin( callback_t new_callback, void* new_user_data,
		int new_buf_size, int initial_read )
{
	callback  = new_callback;
	user_data = new_user_data;

	end();

	// TODO: decide whether using different size on alloc failure is a good idea
	//RETURN_ERR( buf.resize( new_buf_size ? new_buf_size : 4 * block_size ) );
	if ( new_buf_size && buf.resize( new_buf_size ) )
	{
		ACK_FAILURE();
		new_buf_size = 0;
	}
	
	if ( !new_buf_size )
	{
		RETURN_ERR( buf.resize( 4 * block_size ) );
		initial_read = 0;
	}
	
	// Fill buffer with some data, less than normal buffer size since caller might
	// just be examining beginning of file.
	return fill_buf( initial_read ? initial_read : block_size );
}

blargg_err_t BZ2_Inflater::set_mode( mode_t mode, int data_offset )
{
	zbuf.next_in  += data_offset;
	zbuf.avail_in -= data_offset;

	if ( mode == mode_auto )
	{
		// examine buffer for gzip header
		mode = mode_copy;
		unsigned const min_gzip_size = 2 + 8 + 8;
		if ( zbuf.avail_in >= min_gzip_size &&
				zbuf.next_in [0] == 0x42 && zbuf.next_in [1] == 0x5A )
			mode = mode_unbz2;
	}

	if ( mode != mode_copy )
	{
        int zerr = BZ2_bzDecompressInit( &zbuf, 0, 0 );
		if ( zerr )
		{
			zbuf.next_in = NULL;
			return get_bz2_err( zerr );
		}

		deflated_ = true;
	}
	return blargg_ok;
}

/*
// Reads/inflates entire stream. All input must be in buffer, and count must be total
// of all output.
blargg_err_t read_all( void* out, int count );


// zlib automatically applies this optimization (uses inflateFast)
// TODO: remove
blargg_err_t BZ2_Inflater::read_all( void* out, int count )
{
	if ( deflated_ )
	{
		zbuf.next_out  = (char*) out;
		zbuf.avail_out = count;

		int err = BZ2_bzDecompress( &zbuf );
		
		if ( zbuf.avail_out || err != Z_STREAM_END )
			return blargg_err_file_corrupt;
	}
	else
	{
		if ( zbuf.avail_in < count )
			return blargg_err_file_corrupt;
		
		memcpy( out, zbuf.next_in, count );
		
		zbuf.next_in  += count;
		zbuf.avail_in -= count;
	}
	
	return blargg_ok;
}
*/

blargg_err_t BZ2_Inflater::read( void* out, int* count_io )
{
	int remain = *count_io;
	if ( remain && zbuf.next_in )
	{
		if ( deflated_ )
		{
			zbuf.next_out  = (char*) out;
			zbuf.avail_out = remain;

			while ( 1 )
			{
				unsigned int old_avail_in = (unsigned int)zbuf.avail_in;
				int err = BZ2_bzDecompress( &zbuf );
				if ( err == BZ_STREAM_END )
				{
					remain = zbuf.avail_out;
					end();
					break; // no more data to inflate
				}

				if ( err && (err != BZ_OUTBUFF_FULL || old_avail_in) )
					return get_bz2_err( err );

				if ( !zbuf.avail_out )
				{
					remain = 0;
					break; // requested number of bytes inflated
				}

				if ( zbuf.avail_in )
				{
					// inflate() should never leave input if there's still space for output
					check( false );
					return blargg_err_file_corrupt;
				}

				RETURN_ERR( fill_buf( (int)buf.size() ) );
				if ( !zbuf.avail_in )
					return blargg_err_file_corrupt; // stream didn't end but there's no more data
			}
		}
		else
		{
			while ( 1 )
			{
				// copy buffered data
				if ( zbuf.avail_in )
				{
					long count = zbuf.avail_in;
					if ( count > remain )
						count = remain;
					memcpy( out, zbuf.next_in, count );
					zbuf.total_out_lo32 += count;
					out = (char*) out + count;
					remain        -= count;
					zbuf.next_in  += count;
					zbuf.avail_in -= count;
				}

				if ( !zbuf.avail_in && zbuf.next_in < (char *)buf.end() )
				{
					end();
					break;
				}

				// read large request directly
				if ( remain + zbuf.total_out_lo32 % block_size >= buf.size() )
				{
					int count = remain;
					RETURN_ERR( callback( user_data, out, &count ) );
					zbuf.total_out_lo32 += count;
					out = (char*) out + count;
					remain -= count;

					if ( remain )
					{
						end();
						break;
					}
				}

				if ( !remain )
					break;

				RETURN_ERR( fill_buf( (int)(buf.size() - zbuf.total_out_lo32 % block_size) ) );
			}
		}
	}
	*count_io -= remain;
	return blargg_ok;
}

#endif
