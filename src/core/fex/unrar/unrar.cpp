// unrar_core 3.8.5. http://www.slack.net/~ant/

#include "unrar.h"

#include "rar.hpp"
#include <assert.h>

// This source code is a heavily modified version based on the unrar package.
// It may not be used to develop a RAR (WinRAR) compatible archiver.
// See unrar/license.txt for copyright and licensing.

// Same as printf when debugging, otherwise 0
#ifndef debug_printf
	#define debug_printf 1 ? (void)0 : (void)
#endif

// If expr != unrar_ok, returns its value
#define RETURN_ERR( expr ) \
	do {\
		unrar_err_t err_;\
		if ( (err_ = (expr)) != unrar_ok )\
			return err_;\
	} while ( 0 )
	

// Receives errors reported from deep within library.
// MUST be macro.
#define NONLOCAL_ERROR( p ) \
	setjmp( p->Arc.jmp_env )

void Rar_Error_Handler::ReportError( unrar_err_t err )
{
	if ( err )
		longjmp( jmp_env, err );
}

void Rar_Error_Handler::MemoryError()
{
	ReportError( unrar_err_memory );
}


//// Internal

unrar_t::unrar_t()
{
	Arc.user_read   = NULL;
	Arc.user_write  = NULL;
	Arc.Tell_       = 0;
	Arc.write_error = unrar_ok;
	data_           = NULL;
	own_data_       = NULL;
	close_file      = NULL;
	FileCount       = 0;
	Unp             = NULL;
	
	unrar_init();
}

unrar_t::~unrar_t()
{
	if ( Arc.write_error ) { }
	
	if ( close_file )
		close_file( Arc.user_read_data );
	
	delete Unp;
	
	free( own_data_ );
}

// True if current file is compressed in way that affects solid extraction state
static inline bool solid_file( const unrar_t* p )
{
	return p->Arc.Solid &&
			p->Arc.FileHead.Method != 0 &&
			p->Arc.FileHead.PackSize != 0;
}

static void update_solid_pos( unrar_t* p )
{
	if ( p->solid_pos == p->Arc.CurBlockPos )
		p->solid_pos = p->Arc.NextBlockPos;
}

static unrar_err_t extract_( unrar_t* p, unrar_write_func user_write, void* user_data )
{
	assert( !p->done );
	assert( !solid_file( p ) || p->solid_pos == p->Arc.CurBlockPos );
	
	if ( p->Arc.write_error ) { }
	p->Arc.write_error     = unrar_ok;
	p->Arc.user_write      = user_write;
	p->Arc.user_write_data = user_data;
	RETURN_ERR( p->ExtractCurrentFile( user_write == NULL ) );
	p->Arc.user_write      = NULL;
	RETURN_ERR( p->Arc.write_error );
	
	update_solid_pos( p );
	
	return unrar_ok;
}

static unrar_err_t skip_solid( unrar_t* p )
{
	if ( !solid_file( p ) )
	{
		update_solid_pos( p );
		return unrar_ok;
	}
	
	return extract_( p, NULL, NULL );
}

static inline bool IsLink(uint Attr)
{
	return((Attr & 0xF000)==0xA000);
}

static unrar_err_t next_( unrar_t* p, bool skipping_solid )
{
	if ( p->done )
		return unrar_err_arc_eof;
	
	free( p->own_data_ );
	p->own_data_ = NULL;
	p->data_     = NULL;
	
	for (;;)
	{
        size_t ReadSize;
		p->Arc.SeekToNext();
		unrar_err_t const err = p->Arc.ReadHeader(&ReadSize);
		if ( err != unrar_err_arc_eof )
			RETURN_ERR( err );
		//else
		//  debug_printf( "unrar: Didn't end with ENDARC_HEAD\n" ); // rar -en causes this

		HEADER_TYPE const type = (HEADER_TYPE) p->Arc.GetHeaderType();
		
		if ( err != unrar_ok || type == HEAD_ENDARC )
		{
			p->done = true;
			break;
		}
		
		if ( type != HEAD_FILE )
		{
			// Skip non-files
#if 0
			if ( type != HEAD_SERVICE && type != HEAD_CRYPT && type != HEAD_MARK )
				debug_printf( "unrar: Skipping unknown block type: %X\n", (unsigned) type );
#endif
			
			update_solid_pos( p );
		}
		else
		{
			// Update even for non-solid files, in case it's not extracted
			if ( !solid_file( p ) )
				update_solid_pos( p );
			
			if ( p->Arc.IsArcLabel() )
			{
				// Ignore labels
			}
			else if ( IsLink( p->Arc.FileHead.FileAttr ) )
			{
				// Ignore links
				
				p->update_first_file_pos();
				p->FileCount++; // Links are treated as files
			}
			else if ( p->Arc.IsArcDir() )
			{
				// Ignore directories
			}
			else
			{
				p->info.size       = p->Arc.FileHead.UnpSize;
				p->info.name_w     = p->Arc.FileHead.FileName;
                WideToChar(p->info.name_w, p->info.name);
				p->info.is_unicode = (p->Arc.FileHead.Flags & LHD_UNICODE) != 0;
				p->info.dos_date   = p->Arc.FileHead.mtime.GetDos();
				p->info.crc        = p->Arc.FileHead.FileHash.CRC32;
				p->info.is_crc32   = !p->Arc.OldFormat;
				
				// Stop for files
				break;
			}
			
			// Original code assumed that non-file items were never solid compressed
			check( !solid_file( p ) );
			
			// Skip non-file solid-compressed items (original code assumed there were none)
			if ( skipping_solid )
				RETURN_ERR( skip_solid( p ) );
		}
	}
	
	return unrar_ok;
}
	
static unrar_err_t open_( unrar_t* p, unrar_read_func read, void* user_data )
{
	p->Arc.user_read      = read;
	p->Arc.user_read_data = user_data;
	
	RETURN_ERR( p->Arc.IsArchive() );
	
	p->begin_pos      = p->Arc.NextBlockPos;
	p->solid_pos      = p->Arc.NextBlockPos;
	p->first_file_pos = INT_MAX;
	p->done           = false;
	
	return unrar_ok;
}


//// Interface

	// Needed when user read throws exception
	struct unrar_ptr {
		unrar_t* p;
		unrar_ptr() { p = NULL; }
		~unrar_ptr() { delete p; }
	};

unrar_err_t unrar_open_custom( unrar_t** impl_out, unrar_read_func read, void* user_data )
{
	*impl_out = NULL;
	
	unrar_ptr ptr;
	ptr.p = new unrar_t;
	if ( !ptr.p )
		return unrar_err_memory;
	
	RETURN_ERR( NONLOCAL_ERROR( ptr.p ) );
	RETURN_ERR( open_( ptr.p, read, user_data ) );
	RETURN_ERR( next_( ptr.p, false ) );
	
	*impl_out = ptr.p;
	ptr.p = NULL;
		
	//delete ptr.p; // done automatically at end of function
	
	return unrar_ok;
}

void unrar_close( unrar_t* ar )
{
	delete ar;
}

unrar_bool unrar_done( const unrar_t* p )
{
	return p->done;
}

unrar_err_t unrar_next( unrar_t* p )
{
	assert( !unrar_done( p ) );
	
	RETURN_ERR( NONLOCAL_ERROR( p ) );
	return next_( p, false );
}

const unrar_info_t* unrar_info( unrar_t const* p )
{
	assert( !unrar_done( p ) );
	
	return &p->info;
}

unrar_pos_t unrar_tell( const unrar_t* p )
{
	return p->Arc.CurBlockPos;
}

unrar_err_t unrar_seek( unrar_t* p, unrar_pos_t n )
{
	p->Arc.NextBlockPos = n;
	p->done             = false;
	p->FileCount        = (n <= p->first_file_pos ? 0 : 1);
	
	return unrar_next( p );
}

unrar_err_t unrar_rewind( unrar_t* p )
{
	return unrar_seek( p, p->begin_pos );
}

unrar_err_t unrar_try_extract( const unrar_t* p )
{
	assert( !unrar_done( p ) );
	
	return ((unrar_t*) p)->ExtractCurrentFile( true, true );
}

	static unrar_err_t reopen( unrar_t* p )
	{
		// Save and restore archive reader
		unrar_read_func read = p->Arc.user_read;
		void* user_data      = p->Arc.user_read_data;
		
		void (*close_file)( void* ) = p->close_file;
		p->close_file = NULL;
		
		p->~unrar_t();
		new (p) unrar_t;
		
		p->close_file = close_file;
		
		return open_( p, read, user_data );
	}

unrar_err_t unrar_extract_custom( unrar_t* p, unrar_write_func user_write, void* user_data )
{
	assert( !unrar_done( p ) );
	
	RETURN_ERR( NONLOCAL_ERROR( p ) );
	
	if ( solid_file( p ) )
	{
		unrar_pos_t pos = p->Arc.CurBlockPos;
		if ( p->solid_pos != pos )
		{
			// Next file to solid extract isn't current one
			
			if ( p->solid_pos > pos )
				RETURN_ERR( reopen( p ) );
			else
				p->Arc.NextBlockPos = p->solid_pos;
			
			RETURN_ERR( next_( p, true ) );
			
			// Keep extracting until solid position is at desired file
			while ( !p->done && p->solid_pos < pos )
			{
				RETURN_ERR( skip_solid( p ) );
				RETURN_ERR( next_( p, true ) );
			}
			
			// Be sure we're at right file
			if ( p->solid_pos != pos || p->Arc.CurBlockPos != pos )
				return unrar_err_corrupt;
		}
	}
	
	return extract_( p, user_write, user_data );
}
