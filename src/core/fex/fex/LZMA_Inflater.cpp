// File_Extractor 1.0.0. http://www.slack.net/~ant/

#if FEX_ENABLE_LZMA

#include <stdio.h>

#include "LZMA_Inflater.h"

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

static const char* get_lzma_err( int code )
{
	assert( code != LZMA_OK );
	switch ( code )
	{
	case LZMA_MEM_ERROR:   return blargg_err_memory;
	case LZMA_DATA_ERROR:  return blargg_err_file_corrupt;
	// TODO: handle more error codes
	}

	const char* str = BLARGG_ERR( BLARGG_ERR_GENERIC, "problem uncompressing LZMA data" );
	
	return str;
}

void LZMA_Inflater::end()
{
	if ( deflated_ )
	{
		deflated_ = false;
        lzma_end(&zbuf);
	}
	buf.clear();

	static lzma_stream const empty = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, (lzma_reserved_enum)0, (lzma_reserved_enum)0 };
	memcpy( &zbuf, &empty, sizeof zbuf );
}

LZMA_Inflater::LZMA_Inflater()
{
	deflated_ = false;
	end(); // initialize things
}

LZMA_Inflater::~LZMA_Inflater()
{
	end();
}

blargg_err_t LZMA_Inflater::fill_buf( int count )
{
	byte* out = buf.end() - count;
	RETURN_ERR( callback( user_data, out, &count ) );
	zbuf.avail_in = count;
	zbuf.next_in  = (uint8_t *)out;
	return blargg_ok;
}

blargg_err_t LZMA_Inflater::begin( callback_t new_callback, void* new_user_data,
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

bool LZMA_Inflater::is_format_xz(void)
{
    // Specify the magic as hex to be compatible with EBCDIC systems.
    static const uint8_t magic[6] = { 0xFD, 0x37, 0x7A, 0x58, 0x5A, 0x00 };
    return zbuf.avail_in >= sizeof(magic)
            && memcmp(zbuf.next_in, magic, sizeof(magic)) == 0;
}


/// Return true if the data in in_buf seems to be in the .lzma format.
bool LZMA_Inflater::is_format_lzma(void)
{
    // The .lzma header is 13 bytes.
    if (zbuf.avail_in < 13)
        return false;

    // Decode the LZMA1 properties.
    lzma_filter filter = { .id = LZMA_FILTER_LZMA1 };
    if (lzma_properties_decode(&filter, NULL, zbuf.next_in, 5) != LZMA_OK)
        return false;

    // A hack to ditch tons of false positives: We allow only dictionary
    // sizes that are 2^n or 2^n + 2^(n-1) or UINT32_MAX. LZMA_Alone
    // created only files with 2^n, but accepts any dictionary size.
    // If someone complains, this will be reconsidered.
    lzma_options_lzma *opt = (lzma_options_lzma *)filter.options;
    const uint32_t dict_size = opt->dict_size;
    free(opt);

    if (dict_size != UINT32_MAX) {
        uint32_t d = dict_size - 1;
        d |= d >> 2;
        d |= d >> 3;
        d |= d >> 4;
        d |= d >> 8;
        d |= d >> 16;
        ++d;
        if (d != dict_size || dict_size == 0)
            return false;
    }

    // Another hack to ditch false positives: Assume that if the
    // uncompressed size is known, it must be less than 256 GiB.
    // Again, if someone complains, this will be reconsidered.
    uint64_t uncompressed_size = 0;
    for (size_t i = 0; i < 8; ++i)
        uncompressed_size |= (uint64_t)(zbuf.next_in[5 + i]) << (i * 8);

    if (uncompressed_size != UINT64_MAX
            && uncompressed_size > (UINT64_C(1) << 38))
        return false;

    return true;
}


/// Return true if the data in in_buf seems to be in the .lz format.
bool LZMA_Inflater::is_format_lzip(void)
{
    static const uint8_t magic[4] = { 0x4C, 0x5A, 0x49, 0x50 };
    return zbuf.avail_in >= sizeof(magic)
            && memcmp(zbuf.next_in, magic, sizeof(magic)) == 0;
}

blargg_err_t LZMA_Inflater::set_mode( mode_t mode, int data_offset )
{
    int err = LZMA_OK;

	zbuf.next_in  += data_offset;
	zbuf.avail_in -= data_offset;
    buf_ptr = zbuf.next_in;

	if ( mode == mode_auto )
	{
		// examine buffer for gzip header
		mode = mode_copy;
        if ( is_format_lzip() ) {
            fprintf(stderr, "LZIP detected\n");
			mode = mode_unlz;
        }

        if ( is_format_xz() ) {
            fprintf(stderr, "XZ detected\n");
            mode = mode_unxz;
        }

        if ( is_format_lzma() ) {
            fprintf(stderr, "LZMA detected\n");
            mode = mode_raw_deflate;
        }
    }

	if ( mode != mode_copy )
	{
        zbuf = LZMA_STREAM_INIT;

        if (mode == mode_raw_deflate)
            err = lzma_alone_decoder( &zbuf, UINT64_MAX);
        else if (mode == mode_unlz)
            err = lzma_lzip_decoder( &zbuf, UINT64_MAX, LZMA_CONCATENATED);
        else
            err = lzma_stream_decoder( &zbuf, UINT64_MAX, LZMA_CONCATENATED);

        if (err != LZMA_OK) {
            fprintf(stderr, "Couldn't initialize LZMA stream decoder\n");
            return blargg_err_file_corrupt;
        }

        deflated_ = true;
	}

    mode_ = mode;

	return blargg_ok;
}

/*
// Reads/inflates entire stream. All input must be in buffer, and count must be total
// of all output.
blargg_err_t read_all( void* out, int count );


// zlib automatically applies this optimization (uses inflateFast)
// TODO: remove
blargg_err_t LZMA_Inflater::read_all( void* out, int count )
{
    int err = LZMA_OK;

	if ( deflated_ )
	{
		zbuf.next_out  = (char*) out;
		zbuf.avail_out = count;

        if ((buf.size() - zbuf.total_out) <= block_size)
            action = LZMA_FINISH;
        else
            action = LZMA_RUN;

        err = lzma_code(&zbuf, action);

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

blargg_err_t LZMA_Inflater::read( void* out, int* count_io )
{
	int remain = *count_io;

    zbuf.next_in = buf_ptr;

    fprintf(stderr, "LZMA - Read remaining: %d, next in: 0x%p\n", remain, zbuf.next_in);
	if ( remain && zbuf.next_in )
	{
        fprintf(stderr, "LZMA - deflated: %d\n", deflated_);

		if ( deflated_ )
		{
			zbuf.next_out  = (uint8_t*) out;
			zbuf.avail_out = remain;

			while ( 1 )
			{
                int err = LZMA_OK;
				unsigned int old_avail_in = (unsigned int)zbuf.avail_in;

                if ((buf.size() - zbuf.total_out) <= block_size)
                    action = LZMA_FINISH;
                else
                    action = LZMA_RUN;

                err = lzma_code(&zbuf, action);

				if ( err == LZMA_STREAM_END )
				{
					remain = zbuf.avail_out;
					end();
					break; // no more data to inflate
				}

                if ( err && (err != LZMA_BUF_ERROR || old_avail_in) ) {
                    fprintf(stderr, "LZMA error: %d, old available in: %d\n", err, old_avail_in);
                    return get_lzma_err( err );
                }

				if ( !zbuf.avail_out )
				{
					remain = 0;
					break; // requested number of bytes inflated
				}

				if ( zbuf.avail_in )
				{
                    fprintf(stderr, "Available in: %zu, file corrupt\n", zbuf.avail_in);
					// inflate() should never leave input if there's still space for output
					check( false );
					return blargg_err_file_corrupt;
				}

				RETURN_ERR( fill_buf( (int)buf.size() ) );
                if ( !zbuf.avail_in ) {
                    fprintf(stderr, "No more available input data\n");
                    return blargg_err_file_corrupt; // stream didn't end but there's no more data
                }
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
					zbuf.total_out += count;
					out = (char*) out + count;
					remain        -= count;
					zbuf.next_in  += count;
					zbuf.avail_in -= count;
				}

				if ( !zbuf.avail_in && zbuf.next_in < (uint8_t *)buf.end() )
				{
					end();
					break;
				}

				// read large request directly
				if ( remain + zbuf.total_out % block_size >= buf.size() )
				{
					int count = remain;
					RETURN_ERR( callback( user_data, out, &count ) );
					zbuf.total_out += count;
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

				RETURN_ERR( fill_buf( (int)(buf.size() - zbuf.total_out % block_size) ) );
			}
		}
	}
	*count_io -= remain;
	return blargg_ok;
}

#endif
