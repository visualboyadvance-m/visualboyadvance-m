// File_Extractor 1.0.0. http://www.slack.net/~ant/

#include "Zip7_Extractor.h"

#include "7z_C/7zExtract.h"
#include "7z_C/7zAlloc.h"
#include "7z_C/7zCrc.h"

/* Copyright (C) 2005-2009 Shay Green. This module is free software; you
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

static ISzAlloc zip7_alloc      = { SzAlloc,     SzFree     };
static ISzAlloc zip7_alloc_temp = { SzAllocTemp, SzFreeTemp };

struct Zip7_Extractor_Impl :
	ISeekInStream
{
	CLookToRead look;
	CSzArEx db;
	
	// SzExtract state
	UInt32  block_index;
	Byte*   buf;
	size_t  buf_size;

	File_Reader* in;
	const char* in_err;
};

extern "C"
{
	// 7-zip callbacks pass an ISeekInStream* for data, so we must cast it
	// back to ISeekInStream* FIRST, then cast to our Impl structure
	
	static SRes zip7_read_( void* vstream, void* out, size_t* size )
	{
		assert( out && size );
		ISeekInStream* stream = STATIC_CAST(ISeekInStream*,vstream);
		Zip7_Extractor_Impl* impl = STATIC_CAST(Zip7_Extractor_Impl*,stream);
		
		long lsize = *size;
		blargg_err_t err = impl->in->read_avail( out, &lsize );
		if ( err )
		{
			*size = 0;
			impl->in_err = err;
			return SZ_ERROR_READ;
		}
		
		*size = lsize;
		return SZ_OK;
	}

	static SRes zip7_seek_( void* vstream, Int64* pos, ESzSeek mode )
	{
		ISeekInStream* stream = STATIC_CAST(ISeekInStream*,vstream);
		Zip7_Extractor_Impl* impl = STATIC_CAST(Zip7_Extractor_Impl*,stream);
		
		assert( mode != SZ_SEEK_CUR ); // never used
		
		if ( mode == SZ_SEEK_END )
		{
			assert( *pos == 0 ); // only used to find file length
			*pos = impl->in->size();
			return SZ_OK;
		}
		
		assert( mode == SZ_SEEK_SET );
		blargg_err_t err = impl->in->seek( *pos );
		if ( err )
		{
			// don't set in_err in this case, since it might be benign
			if ( err == blargg_err_file_eof )
				return SZ_ERROR_INPUT_EOF;
			
			impl->in_err = err;
			return SZ_ERROR_READ;
		}
		
		return SZ_OK;
	}
}

blargg_err_t Zip7_Extractor::zip7_err( int err )
{
	// TODO: ignore in_err in some cases? unsure about which error to use
	blargg_err_t in_err = impl->in_err;
	impl->in_err = NULL;
	if ( in_err )
	{
		check( err != SZ_OK );
		return in_err;
	}
	
	switch ( err )
	{
	case SZ_OK:                 return blargg_ok;
	case SZ_ERROR_MEM:          return blargg_err_memory;
	case SZ_ERROR_READ:         return blargg_err_file_io;
	case SZ_ERROR_CRC:
	case SZ_ERROR_DATA:
	case SZ_ERROR_INPUT_EOF:
	case SZ_ERROR_ARCHIVE:      return blargg_err_file_corrupt;
	case SZ_ERROR_UNSUPPORTED:  return blargg_err_file_feature;
	case SZ_ERROR_NO_ARCHIVE:   return blargg_err_file_type;
	}
	
	return blargg_err_generic;
}

static blargg_err_t init_7z()
{
	static bool inited;
	if ( !inited )
	{
		inited = true;
		CrcGenerateTable();
	}
	return blargg_ok;
}

static File_Extractor* new_7z()
{
	return BLARGG_NEW Zip7_Extractor;
}

fex_type_t_ const fex_7z_type [1] = {{
	".7z",
	&new_7z,
	"7-zip archive",
	&init_7z
}};

Zip7_Extractor::Zip7_Extractor() :
	File_Extractor( fex_7z_type )
{
	impl = NULL;
}

Zip7_Extractor::~Zip7_Extractor()
{
	close();
}

blargg_err_t Zip7_Extractor::open_v()
{
	RETURN_ERR( init_7z() );
	
	if ( !impl )
	{
		impl = (Zip7_Extractor_Impl*) malloc( sizeof *impl );
		CHECK_ALLOC( impl );
	}
	
	impl->in          = &arc();
	impl->block_index = (UInt32) -1;
	impl->buf         = NULL;
	impl->buf_size    = 0;

	LookToRead_CreateVTable( &impl->look, false );
	impl->ISeekInStream::Read = zip7_read_;
	impl->ISeekInStream::Seek = zip7_seek_;
	impl->look.realStream     = impl;
	LookToRead_Init( &impl->look );
	
	SzArEx_Init( &impl->db );
	
	impl->in_err = NULL;
	RETURN_ERR( zip7_err( SzArEx_Open( &impl->db, &impl->look.s,
			&zip7_alloc, &zip7_alloc_temp ) ) );
	
	return seek_arc_v( 0 );
}

void Zip7_Extractor::close_v()
{
	if ( impl )
	{
		if ( impl->in )
		{
			impl->in = NULL;
			SzArEx_Free( &impl->db, &zip7_alloc );
		}
		IAlloc_Free( &zip7_alloc, impl->buf );
		free( impl );
		impl = NULL;
	}
}

blargg_err_t Zip7_Extractor::next_v()
{
	while ( ++index < (int) impl->db.db.NumFiles )
	{
		CSzFileItem const& item = impl->db.db.Files [index];
		if ( !item.IsDir )
		{
			// TODO: Support date.
			// NTFS representation, stored as 64-bit value.
			// Divide by 10000000 (ten million) to get seconds
			//item.MTime.Low + (.High << 32)
			// How to convert to DOS style?
			
			set_name( item.Name );
			set_info( item.Size, 0, (item.FileCRCDefined ? item.FileCRC : 0) );
			break;
		}
	}
	
	return blargg_ok;
}

blargg_err_t Zip7_Extractor::rewind_v()
{
	return seek_arc_v( 0 );
}

fex_pos_t Zip7_Extractor::tell_arc_v() const
{
	return index;
}

blargg_err_t Zip7_Extractor::seek_arc_v( fex_pos_t pos )
{
	assert( 0 <= pos && pos <= (int) impl->db.db.NumFiles );
	
	index = pos - 1;
	return next_v();
}

blargg_err_t Zip7_Extractor::data_v( void const** out )
{
	impl->in_err = NULL;
	size_t offset = 0;
	size_t count  = 0;
	RETURN_ERR( zip7_err( SzAr_Extract( &impl->db, &impl->look.s, index,
			&impl->block_index, &impl->buf, &impl->buf_size,
			&offset, &count, &zip7_alloc, &zip7_alloc_temp ) ) );
	assert( count == (size_t) size() );
	
	*out = impl->buf + offset;
	return blargg_ok;
}
