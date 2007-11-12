// File_Extractor 0.4.2
// This source code is a heavily modified version based on the unrar package.
// It may not be used to develop a RAR (WinRAR) compatible archiver.
// See unrar_license.txt for copyright and licensing.

#include "blargg_common.h"

#if FEX_ENABLE_RAR

#include "Rar_Extractor.h"

#include "unrar/rar.hpp"

#include "blargg_source.h"

const char end_of_rar [] = "Unexpected end of RAR archive";

Rar_Extractor::Rar_Extractor() : File_Extractor( fex_rar_type )
{
	impl = 0;
}

static File_Extractor* new_rar() { return BLARGG_NEW Rar_Extractor; }

fex_type_t_ const fex_rar_type [1] = {{ "RAR", &new_rar }};

void Rar_Extractor::close_()
{
	delete impl;
	impl = 0;
}

Rar_Extractor::~Rar_Extractor() { close(); }

blargg_err_t Rar_Extractor::open_()
{
	RETURN_ERR( file().seek( 0 ) );
	
	CHECK_ALLOC( impl = BLARGG_NEW Rar_Extractor_Impl( &file() ) );
	
	CHECK_ALLOC( !setjmp( impl->jmp_env ) );
	
	impl->Unp.Init( 0 );
	
	if ( !impl->arc.IsArchive() )
		return fex_wrong_file_type;
	
	RETURN_ERR( impl->arc.IsArchive2() );
	
	index       = -1;
	can_extract = true;
	
	return next_item();
}

blargg_err_t Rar_Extractor::rewind_()
{
	assert( impl );
	close_();
	return open_();
}

blargg_err_t Rar_Extractor::next_item()
{
	CHECK_ALLOC( !setjmp( impl->jmp_env ) );
	
	index++;
	for ( ;; impl->arc.SeekToNext() )
	{
		blargg_err_t err = impl->arc.ReadHeader();
		if ( err == end_of_rar )
		{
			set_done();
			break;
		}
		if ( err )
			return err;
		
		HEADER_TYPE type = (HEADER_TYPE) impl->arc.GetHeaderType();
		if ( type == ENDARC_HEAD )
		{
			// no files beyond this point
			set_done();
			break;
		}
		
		// skip non-files
		if ( type != FILE_HEAD )
		{
			if ( type != NEWSUB_HEAD && type != PROTECT_HEAD )
				dprintf( "Skipping unknown RAR block type: %X\n", (int) type );
			continue;
		}
		
		// skip label
		if ( impl->arc.NewLhd.HostOS <= HOST_WIN32 && (impl->arc.NewLhd.FileAttr & 8) )
			continue;
		
		// skip links
		if ( (impl->arc.NewLhd.FileAttr & 0xF000) == 0xA000 )
			continue;
		
		// skip directories
		if ( (impl->arc.NewLhd.Flags & LHD_WINDOWMASK) == LHD_DIRECTORY )
			continue;
		
		set_info( impl->arc.NewLhd.UnpSize, impl->arc.NewLhd.FileName );
		break;
	}
	
	extracted = false;
	return 0;
}

blargg_err_t Rar_Extractor::next_()
{
	CHECK_ALLOC( !setjmp( impl->jmp_env ) );
	
	if ( !extracted && impl->arc.Solid )
	{
		if ( is_scan_only() )
		{
			can_extract = false;
		}
		else
		{
			// must extract every file in a solid archive
			Null_Writer out;
			RETURN_ERR( impl->extract( out, false ) );
		}
	}
	
	impl->arc.SeekToNext();
	return next_item();
}

blargg_err_t Rar_Extractor::extract( Data_Writer& out )
{
	if ( !impl || extracted )
		return Data_Reader::eof_error;
	
	CHECK_ALLOC( !setjmp( impl->jmp_env ) );
	
	if ( !can_extract )
	{
		int saved_index = index;
		
		RETURN_ERR( rewind_() );
		scan_only( false );
		while ( index < saved_index )
		{
			if ( done() )
				return "Corrupt RAR archive";
			RETURN_ERR( next_() );
		}
		assert( can_extract );
	}
	extracted = true;
	
	impl->write_error = 0;
	RETURN_ERR( impl->extract( out ) );
	return impl->write_error;
}
	
// Rar_Extractor_Impl

void Rar_Extractor_Impl::Seek( long n )
{
	if ( reader->seek( n ) )
		check( false );
}

long Rar_Extractor_Impl::Tell() { return reader->tell(); }

long Rar_Extractor_Impl::Read( void* p, long n ) { return reader->read_avail( p, n ); }

long Rar_Extractor_Impl::Size() { return reader->size(); }

void Rar_Extractor_Impl::UnpWrite( byte const* out, uint count )
{
	if ( !write_error )
		write_error = writer->write( out, count );
	if ( MaintainCRC )
	{
		if ( arc.OldFormat )
			UnpFileCRC = OldCRC( (ushort) UnpFileCRC, out, count );
		else
			UnpFileCRC = CRC( UnpFileCRC, out, count );
	}
}
#endif
