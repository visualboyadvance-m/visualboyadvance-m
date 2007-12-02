// File_Extractor 0.4.2. http://www.slack.net/~ant/

#include "Zip7_Extractor.h"

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>

extern "C" {
	#include "../7z_C/LZMA_C/LzmaTypes.h"
}
#include "../7z_C/7zTypes.h"
extern "C" {
	#include "../7z_C/7zExtract.h"
	#include "../7z_C/7zCrc.h"
}

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

#include "blargg_source.h"

static ISzAlloc alloc = { SzAlloc, SzFree };
static ISzAlloc alloc_temp = { SzAllocTemp, SzFreeTemp };

struct Zip7_Extractor_Impl
{
	ISzInStream         stream; // must be first
	CArchiveDatabaseEx  db;

	// SzExtract state
	UInt32  block_index;
	Byte*   buf;
	size_t  buf_size;

	File_Reader* in;

#ifdef _LZMA_IN_CB
	enum { read_buf_size = 32 * 1024L };
	char read_buf [read_buf_size];
#endif
};

extern "C"
{
#ifdef _LZMA_IN_CB
	static SZ_RESULT zip7_read_( void* data, void** out, size_t size, size_t* size_out )
	{
		assert( out && size_out );
		Zip7_Extractor_Impl* impl = (Zip7_Extractor_Impl*) data;
		if ( size > impl->read_buf_size )
			size = impl->read_buf_size;
		*out = impl->read_buf;
		*size_out = impl->in->read_avail( impl->read_buf, size );
		return SZ_OK;
	}
#else
	static SZ_RESULT zip7_read_( void* data, void* out, size_t size, size_t* size_out )
	{
		assert( out && size_out );
		Zip7_Extractor_Impl* impl = (Zip7_Extractor_Impl*) data;
		*size_out = impl->in->read_avail( out, size );
		return SZ_OK;
	}
#endif

	static SZ_RESULT zip7_seek_( void* data, CFileSize offset )
	{
		Zip7_Extractor_Impl* impl = (Zip7_Extractor_Impl*) data;
		if ( impl->in->seek( offset ) )
			return SZE_FAIL;
		return SZ_OK;
	}
}

Zip7_Extractor::Zip7_Extractor() : File_Extractor( fex_7z_type )
{
	impl = 0;
}

static File_Extractor* new_7z() { return BLARGG_NEW Zip7_Extractor; }

fex_type_t_ const fex_7z_type [1] = {{ "7Z", &new_7z }};

Zip7_Extractor::~Zip7_Extractor() { close(); }

static const char* zip7_err( int err )
{
	switch ( err )
	{
		case SZ_OK:           return 0;
		case SZE_OUTOFMEMORY: return "Out of memory";
		case SZE_CRC_ERROR:   return "7-zip archive is corrupt";
		case SZE_NOTIMPL:     return "Unsupported 7-zip feature";
		//case SZE_FAIL:
		//case SZE_DATA_ERROR:
		//case SZE_ARCHIVE_ERROR:
	}
	return "7-zip error";
}

blargg_err_t Zip7_Extractor::open_()
{
	if ( !impl )
		CHECK_ALLOC( impl = (Zip7_Extractor_Impl*) malloc( sizeof *impl ) );
	impl->stream.Read = zip7_read_;
	impl->stream.Seek = zip7_seek_;
	impl->in          = &file();
	impl->block_index = (unsigned) -1;
	impl->buf         = 0;
	impl->buf_size    = 0;

	InitCrcTable();
	SzArDbExInit( &impl->db );
	int code = SzArchiveOpen( &impl->stream, &impl->db, &alloc, &alloc_temp );
	RETURN_ERR( (code == SZE_ARCHIVE_ERROR ? fex_wrong_file_type : zip7_err( code )) );
	return rewind();
}

blargg_err_t Zip7_Extractor::rewind_()
{
	index = -1;
	return next();
}

void Zip7_Extractor::close_()
{
	if ( impl )
	{
		if ( impl->in )
		{
			impl->in = 0;
			SzArDbExFree( &impl->db, alloc.Free );
		}
		alloc.Free( impl->buf );
		free( impl );
		impl = 0;
	}
}

blargg_err_t Zip7_Extractor::next_()
{
	while ( ++index < (int) impl->db.Database.NumFiles )
	{
		CFileItem const& item = impl->db.Database.Files [index];
		if ( !item.IsDirectory )
		{
			set_info( item.Size, item.Name );
			return 0;
		}
	}
	set_done();
	return 0;
}

blargg_err_t Zip7_Extractor::data_( byte const*& out )
{
	if ( !impl )
		return "Archive not open";

	size_t offset = 0;
	size_t size = 0;
	RETURN_ERR( zip7_err( SzExtract( &impl->stream, &impl->db, index,
			&impl->block_index, &impl->buf, &impl->buf_size,
			&offset, &size, &alloc, &alloc_temp ) ) );
	assert( size == (size_t) this->size() );
	out = impl->buf + offset;
	return 0;
}
