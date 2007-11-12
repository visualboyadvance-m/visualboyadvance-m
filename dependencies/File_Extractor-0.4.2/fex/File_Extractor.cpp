// File_Extractor 0.4.2. http://www.slack.net/~ant/

#include "File_Extractor.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

/* Copyright (C) 2005-2007 Shay Green. This module is free software; you
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

const char fex_wrong_file_type [] = "Archive format not supported";

File_Extractor::File_Extractor( fex_type_t t ) :
	type_( t )
{
	user_data_    = 0;
	user_cleanup_ = 0;
	init_state();
}

// Open

void File_Extractor::open_filter_( const char*, Std_File_Reader* ) { }

blargg_err_t File_Extractor::open( const char* path )
{
	Std_File_Reader* in = BLARGG_NEW Std_File_Reader;
	CHECK_ALLOC( in );
	
	blargg_err_t err = in->open( path );
	if ( !err )
	{
		open_filter_( path, in );
		err = this->open( in );
	}
	
	if ( err )
	{
		delete in;
		return err;
	}
	
	own_file();
	return 0;
}

blargg_err_t File_Extractor::open( File_Reader* input )
{
	require( input->tell() == 0 ); // needs to be at beginning
	close();
	reader_ = input;
	blargg_err_t err = open_();
	if ( !err )
		done_ = false;
	else
		close();
	return err;
}

// Close

void File_Extractor::init_state()
{
	done_      = true;
	scan_only_ = false;
	reader_    = 0;
	own_file_  = false;
	own_data_  = 0;
	clear_file();
}

void File_Extractor::close()
{
	close_();
	if ( own_file_ && reader_ )
		delete reader_;
	clear_file();
	init_state();
}

File_Extractor::~File_Extractor()
{
	// fails if derived class didn't call close() in its destructor
	check( !own_file_ && !reader_ );
	if ( user_cleanup_ )
		user_cleanup_( user_data_ );
}

// Scanning

void File_Extractor::free_data()
{
	data_ptr_ = 0;
	free( own_data_ );
	own_data_ = 0;
}

void File_Extractor::clear_file()
{
	name_     = 0;
	size_     = 0;
	data_pos_ = 0;
	free_data();
	clear_file_();
}

blargg_err_t File_Extractor::next()
{
	if ( done() )
		return "End of archive";
	
	clear_file();
	blargg_err_t err = next_();
	if ( err )
		done_ = true;
	if ( done_ )
		clear_file();
	return err;
}

blargg_err_t File_Extractor::rewind()
{
	if ( !reader_ )
		return "No open archive";
	
	done_      = false;
	scan_only_ = false;
	clear_file();
	
	blargg_err_t err = rewind_();
	if ( err )
	{
		done_ = true;
		clear_file();
	}
	return err;
}

// Full extraction

blargg_err_t File_Extractor::data_( byte const*& out )
{
	if ( !own_data_ )
		CHECK_ALLOC( own_data_ = (byte*) malloc( size_ ? size_ : 1 ) );
	
	out = own_data_;
	return read_once( own_data_, size_ );
}

unsigned char const* File_Extractor::data( blargg_err_t* error_out )
{
	const char* error = 0;
	if ( !reader_ )
	{
		error = eof_error;
	}
	else if ( !data_ptr_ )
	{
		error = data_( data_ptr_ );
		if ( error )
			free_data();
	}
	
	if ( error_out )
		*error_out = error;
	
	return data_ptr_;
}

blargg_err_t File_Extractor::extract( Data_Writer& out )
{
	blargg_err_t error;
	byte const* p = data( &error );
	RETURN_ERR( error );
	
	return out.write( p, size_ );
}

blargg_err_t File_Extractor::read_once( void* out, long count )
{
	Mem_Writer mem( out, count, 1 );
	return extract( mem );
}

// Data_Reader functions

long File_Extractor::remain() const { return size_ - data_pos_; }

long File_Extractor::read_avail( void* out, long count )
{
	if ( count )
	{
		long r = remain();
		if ( count > r )
			count = r;
		
		if ( read( out, count ) )
			count = -1;
	}
	return count;
}

blargg_err_t File_Extractor::read( void* out, long count )
{
	if ( count > remain() )
		return "End of file";
	
	if ( count == size_ && !data_ptr_ )
	{
		// avoid temporary buffer when reading entire file in one call
		assert( data_pos_ == 0 );
		RETURN_ERR( read_once( out, count ) );
	}
	else
	{
		if ( count && !data_ptr_ )
		{
			blargg_err_t err;
			data( &err ); // sets data_ptr_
			RETURN_ERR( err );
		}
	
		memcpy( out, data_ptr_ + data_pos_, count );
	}
	data_pos_ += count;
	if ( data_pos_ == size_ )
		free_data();
	
	return 0;
}
