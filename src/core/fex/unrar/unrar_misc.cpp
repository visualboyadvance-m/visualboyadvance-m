// Misc functions outside the core interface

#include "unrar.h"

#include "rar.hpp"
#include <assert.h>

// This source code is a heavily modified version based on the unrar package.
// It may not be used to develop a RAR (WinRAR) compatible archiver.
// See unrar/license.txt for copyright and licensing.

void unrar_init()
{
	if (crc_tables[0][1]==0)
		InitCRCTables();
	
	Unpack::init_tables();
}

struct unrar_extract_mem_t
{
	char* out;
	char* end;
};

extern "C" {
	static unrar_err_t extract_write( void* user_data, const void* in, int count )
	{
		unrar_extract_mem_t* p = (unrar_extract_mem_t*) user_data;
		
		unrar_pos_t remain = p->end - p->out;
		if ( remain > 0 )
		{
			if ( count > remain )
				count = (int)remain;
			
			memcpy( p->out, in, count );
			p->out += count;
		}
		
		return unrar_ok;
	}
}

unrar_err_t unrar_extract( unrar_t* p, void* out, unrar_pos_t size )
{
	assert( !unrar_done( p ) );
	
	unrar_extract_mem_t m;
	m.out = (char*) out;
	m.end = m.out + size;
	return unrar_extract_custom( p, &extract_write, &m );
}

inline
static bool is_entire_file( const unrar_t* p, const void* in, int count )
{
	return (count == p->Arc.SubHead.UnpSize && p->Unp && in == p->Unp->window_wrptr());
}
	
extern "C" {
	static unrar_err_t extract_mem( void* data, void const* in, int count )
	{
		unrar_t* p = (unrar_t*) data;
		
		// We might have pointer to entire file
		if ( !p->data_ && is_entire_file( p, in, count ) )
		{
			p->data_ = in;
			return unrar_ok;
		}
		
		// We don't have it, so allocate memory to read entire file into
		if ( !p->own_data_ )
		{
			assert( !p->data_ );
			
			unrar_pos_t size = unrar_info( p )->size;
			p->own_data_ = malloc( size ? (size_t)size : 1 );
			if ( !p->own_data_ )
				return unrar_err_memory;
			
			p->data_ = p->own_data_;
		}
		
		memcpy( (void*) p->data_, in, count );
		p->data_ = (char*) p->data_ + count;
		
		return unrar_ok;
	}
}

unrar_err_t unrar_extract_mem( unrar_t* p, void const** out )
{
	assert( !unrar_done( p ) );
	
	*out = NULL;
	
	if ( !p->data_ )
	{
		unrar_err_t err = unrar_extract_custom( p, &extract_mem, p );
		if ( err )
			return err;
	}
	
	*out = (p->own_data_ ? p->own_data_ : p->data_);
	return unrar_ok;
}

const char* unrar_err_str( unrar_err_t err )
{
	switch ( err )
	{
	case unrar_ok:              return "";
	case unrar_err_memory:      return "out of memory";
	case unrar_err_open:        return "couldn't open RAR archive";
	case unrar_err_not_arc:     return "not a RAR archive";
	case unrar_err_corrupt:     return "RAR archive is corrupt";
	case unrar_err_io:          return "couldn't read/write";
	case unrar_err_arc_eof:     return "unexpected end of archive";
	case unrar_err_encrypted:   return "encryption not supported";
	case unrar_err_segmented:   return "segmentation not supported";
	case unrar_err_huge:        return "huge (2GB+) archives are not supported";
	case unrar_err_old_algo:    return "compressed using older algorithm than supported";
	case unrar_err_new_algo:    return "compressed using newer algorithm than supported";
	}
	
	assert( false );
	return "problem with RAR";
}

int ComprDataIO::Read( void* p, int n )
{
	unrar_err_t err = user_read( user_read_data, p, &n, Tell_ );
	if ( err )
		ReportError( err );
	
	Tell_ += n;
	if ( Tell_ < 0 )
		ReportError( unrar_err_huge );
	
	return n;
}

void ComprDataIO::UnpWrite( byte* out, uint count )
{
	if ( !SkipUnpCRC )
	{
		if ( write_error == unrar_ok )
			write_error = user_write( user_write_data, out, count );

        UnpHash.Update(out,count);
	}
}

int ComprDataIO::UnpRead( byte* out, uint count )
{
	if ( count <= 0 )
		return 0;

	if ( count > (uint) UnpPackedSize )
		count = (uint) UnpPackedSize;

	int result = Read( out, count );
	UnpPackedSize -= result;
	return result;
}
