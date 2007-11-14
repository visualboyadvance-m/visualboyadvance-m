// File_Extractor 0.4.2. http://www.slack.net/~ant/

#include "Zip_Extractor.h"

#include "blargg_endian.h"
#include <stdlib.h>
#include <assert.h>
#include <string.h>

/* Copyright (C) 2005-2006 Shay Green. This module is free software; you
can redistribute it and/or modify it under the terms of the GNU Lesser
General Public License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version. This
module is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
details. You should have received a copy of the GNU Lesser General Public
License along with this module; if not, write to the Free Software Foundation,
Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA */

// To avoid copying filename string from catalog, I terminate it by modifying
// catalog data. This potentially requires moving the first byte of the type
// of the next entry elsewhere; I move it to the first byte of made_by. Kind
// of hacky, but I'd rather not have to allocate memory for a copy of it.

#include "blargg_source.h"

// Reads this much from end of file when first opening. Only this much is
// searched for the end catalog entry. If whole catalog is within this data,
// nothing more needs to be read on open.
int const end_read_size = 8 * 1024L;

// Reads are aligned to this size, increasing performance
int const disk_block_size = 4 * 1024L;

// Read buffer used for extracting file data
int const read_buf_size = 16 * 1024L;

struct header_t
{
	char type [4];
	byte vers [2];
	byte flags [2];
	byte method [2];
	byte time [2];
	byte date [2];
	byte crc [4];
	byte raw_size [4];
	byte size [4];
	byte filename_len [2];
	byte extra_len [2];
	char filename [2]; // [filename_len]
	//char extra [extra_len];
};
int const header_size = 30;

struct entry_t
{
	char type [4];
	byte made_by [2];
	byte vers [2];
	byte flags [2];
	byte method [2];
	byte time [2];
	byte date [2];
	byte crc [4];
	byte raw_size [4];
	byte size [4];
	byte filename_len [2];
	byte extra_len [2];
	byte comment_len [2];
	byte disk [2];
	byte int_attrib [2];
	byte ext_attrib [4];
	byte file_offset [4];
	char filename [2]; // [filename_len]
	//char extra [extra_len];
	//char comment [comment_len];
};
int const entry_size = 46;

struct end_entry_t
{
	char type [4];
	byte disk [2];
	byte first_disk [2];
	byte disk_entry_count [2];
	byte entry_count [2];
	byte dir_size [4];
	byte dir_offset [4];
	byte comment_len [2];
	char comment [2]; // [comment_len]
};
int const end_entry_size = 22;

Zip_Extractor::Zip_Extractor() : File_Extractor( fex_zip_type )
{
	Zip_Extractor::clear_file_();
	assert( offsetof (header_t,filename) == header_size );
	assert( offsetof (entry_t,filename) == entry_size );
	assert( offsetof (end_entry_t,comment) == end_entry_size );
}

static File_Extractor* new_zip() { return BLARGG_NEW Zip_Extractor; }

fex_type_t_ const fex_zip_type [1] = {{ "ZIP", &new_zip }};

void Zip_Extractor::close_() { catalog.clear(); }

Zip_Extractor::~Zip_Extractor() { close(); }

void Zip_Extractor::open_filter_( const char*, Std_File_Reader* file )
{
	file->make_unbuffered(); // faster since Zlib_Inflater already buffers data
}

blargg_err_t Zip_Extractor::open_()
{
	file_size = file().size();
	if ( file_size < end_entry_size )
		return fex_wrong_file_type;

	// read end of file
	long buf_offset = file_size - end_read_size;
	if ( buf_offset < 0 )
		buf_offset = 0;
	buf_offset &= ~(disk_block_size - 1); // align beginning of read
	RETURN_ERR( catalog.resize( file_size - buf_offset ) );
	RETURN_ERR( file().seek( buf_offset ) );
	RETURN_ERR( file().read( catalog.begin(), catalog.size() ) );

	// find end entry
	int offset = catalog.size() - end_entry_size;
	while ( memcmp( &catalog [offset], "PK\5\6", 4 ) )
	{
		if ( --offset < 0 )
			return fex_wrong_file_type;
	}
	long end_offset = buf_offset + offset;
	end_entry_t const& entry = (end_entry_t&) catalog [offset];

	// some idiotic zip compressors add data to end of zip without setting comment len
	check( file_size == end_offset + end_entry_size + get_le16( entry.comment_len ) );

	// find beginning of catalog
	catalog_begin = get_le32( entry.dir_offset );
	long catalog_size = end_offset - catalog_begin;
	if ( catalog_size < 0 )
		return buf.corrupt_error;
	catalog_size += end_entry_size;

	// catalog might already be fully read
	long begin_offset = catalog_begin - buf_offset;
	if ( begin_offset >= 0 ) // all catalog data already in memory
		memmove( catalog.begin(), &catalog [begin_offset], catalog_size );

	RETURN_ERR( catalog.resize( catalog_size ) );

	if ( begin_offset < 0 )
	{
		// catalog begins before the data we read
		RETURN_ERR( file().seek( catalog_begin ) );
		RETURN_ERR( file().read( catalog.begin(), catalog.size() ) );
	}

	// first entry in catalog should be a file or end of archive
	if ( memcmp( catalog.begin(), "PK\1\2", 4 ) && memcmp( catalog.begin(), "PK\5\6", 4 ) )
		return fex_wrong_file_type;

	// move first byte
	catalog [4] = catalog [0];
	catalog [0] = 0;

	return rewind();
}

// Scanning

blargg_err_t Zip_Extractor::update_info( bool advance_first )
{
	while ( 1 )
	{
		entry_t& e = (entry_t&) catalog [catalog_pos];

		if ( memcmp( e.type, "\0K\1\2P", 5 ) )
		{
			check( !memcmp( e.type, "\0K\5\6P", 5 ) );
			set_done();
			break;
		}

		unsigned len = get_le16( e.filename_len );
		long next_offset = catalog_pos + entry_size + len + get_le16( e.extra_len ) +
				get_le16( e.comment_len );
		if ( (unsigned long) next_offset > catalog.size() - end_entry_size )
			return buf.corrupt_error;

		if ( catalog [next_offset] == 'P' )
		{
			// move first byte of type
			catalog [next_offset] = 0;
			catalog [next_offset + 4] = 'P';
		}

		if ( !advance_first )
		{
			e.filename [len] = 0; // terminate name
			set_info( get_le32( e.size ), e.filename );

			// ignore directories
			if ( size() || (e.filename [len - 1] != '/' && e.filename [len - 1] != '\\') )
			{
				remain_ = size();
				break;
			}
		}

		catalog_pos = next_offset;
		advance_first = false;
	}
	return 0;
}

blargg_err_t Zip_Extractor::rewind_()
{
	catalog_pos = 0;
	return update_info( false );
}

blargg_err_t Zip_Extractor::next_()
{
	return update_info( true );
}

// Reading

void Zip_Extractor::clear_file_()
{
	remain_ = 0;
	buf.end();
}

blargg_err_t Zip_Extractor::inflater_read( void* data, void* out, long* count )
{
	Zip_Extractor& self = *(Zip_Extractor*) data;
	if ( *count > self.raw_remain )
	{
		*count = self.raw_remain;
		if ( !*count )
			return 0;
	}
	self.raw_remain -= *count;
	return self.file().read( out, *count );
}

long Zip_Extractor::remain() const { return remain_; }

blargg_err_t Zip_Extractor::fill_buf( long offset, long buf_size, long initial_read )
{
	raw_remain = file_size - offset;
	RETURN_ERR( file().seek( offset ) );
	return buf.begin( inflater_read, this, buf_size, initial_read );
}

blargg_err_t Zip_Extractor::first_read( long count )
{
	entry_t const& e = (entry_t&) catalog [catalog_pos];

	method = get_le16( e.method );
	if ( (method && method != Z_DEFLATED) || get_le16( e.vers ) > 20 )
		return "Unsupported zip file compression";

	long raw_size = get_le32( e.raw_size );

	long file_offset = get_le32( e.file_offset );
	int align = file_offset & (disk_block_size - 1);
	{
		// read header
		long buf_size = 3 * disk_block_size - 1 + raw_size; // space for all raw data
		buf_size &= ~(disk_block_size - 1);
		long initial_read = buf_size;
		if ( !method || count < size() )
		{
			buf_size     = read_buf_size;
			initial_read = disk_block_size * 2;
		}
		RETURN_ERR( fill_buf( file_offset - align, buf_size, initial_read ) );
	}
	header_t const& h = (header_t&) buf.data() [align];
	if ( buf.filled() < align + header_size || memcmp( h.type, "PK\3\4", 4 ) )
		return buf.corrupt_error;

	// crc
	correct_crc = get_le32( h.crc );
	if ( !correct_crc )
		correct_crc = get_le32( e.crc );
	check( correct_crc == get_le32( e.crc ) ); // catalog CRC should match
	crc = crc32( 0, 0, 0 );

	// data offset
	long data_offset = file_offset + header_size +
			get_le16( h.filename_len ) + get_le16( h.extra_len );
	if ( data_offset + raw_size > catalog_begin )
		return buf.corrupt_error;

	// refill buffer if there's lots of extra data after header
	long buf_offset = data_offset - file_offset + align;
	if ( buf_offset > buf.filled() )
	{
		// TODO: this will almost never occur, making it a good place for bugs
		buf_offset = data_offset % disk_block_size;
		RETURN_ERR( fill_buf( data_offset - buf_offset, read_buf_size, disk_block_size ) );
	}

	raw_remain = raw_size - (buf.filled() - buf_offset);
	return buf.set_mode( (method ? buf.mode_raw_deflate : buf.mode_copy), buf_offset );
}

blargg_err_t Zip_Extractor::read( void* out, long count )
{
	if ( count > remain_ )
		return eof_error;

	if ( count )
	{
		if ( remain_ == size() )
			RETURN_ERR( first_read( count ) );

		long actual = count;
		RETURN_ERR( buf.read( out, &actual ) );
		if ( actual < count )
			return buf.corrupt_error;

		crc = crc32( crc, (byte const*) out, count );
		remain_ -= count;
		if ( !remain_ && crc != correct_crc )
			return buf.corrupt_error;
	}
	return 0;
}

blargg_err_t Zip_Extractor::read_once( void* out, long count )
{
	require( remain_ == size() );
	return read( out, count );
}
