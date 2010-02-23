// File_Extractor 0.4.3. http://www.slack.net/~ant/

#include "Gzip_Reader.h"

#include "blargg_endian.h"

/* Copyright (C) 2006-2007 Shay Green. This module is free software; you
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

void Gzip_Reader::close()
{
	in    = 0;
	tell_ = 0;
	size_ = 0;
	inflater.end();
}

Gzip_Reader::Gzip_Reader() { close(); }

Gzip_Reader::~Gzip_Reader() { }

static blargg_err_t gzip_reader_read( void* file, void* out, long* count )
{
	*count = ((File_Reader*) file)->read_avail( out, *count );
	return (*count < 0 ? "Read error" : 0);
}

blargg_err_t Gzip_Reader::open( File_Reader* new_in )
{
	close();

	RETURN_ERR( inflater.begin( gzip_reader_read, new_in ) );
	RETURN_ERR( inflater.set_mode( inflater.mode_auto ) );

	size_ = -1; // defer seeking to end of file until size is actually needed
	in    = new_in;
	return 0;
}

blargg_err_t Gzip_Reader::calc_size()
{
	long size = in->size();
	if ( inflater.deflated() )
	{
		byte trailer [4];
		long pos = in->tell();
		RETURN_ERR( in->seek( size - sizeof trailer ) );
		RETURN_ERR( in->read( trailer, sizeof trailer ) );
		RETURN_ERR( in->seek( pos ) );
		size = get_le32( trailer );
	}
	size_ = size;
	return 0;
}

long Gzip_Reader::remain() const
{
	if ( size_ < 0 )
	{
		if ( !in )
			return 0;

		// need to cast away constness to change cached value
		if ( ((Gzip_Reader*) this)->calc_size() )
			return -1;
	}
	return size_ - tell_;
}

blargg_err_t Gzip_Reader::read_( void* out, long* count )
{
	blargg_err_t err = inflater.read( out, count );
	tell_ += *count;
	if ( size_ >= 0 && tell_ > size_ )
	{
		tell_ = size_;
		return inflater.corrupt_error;
	}
	return err;
}

blargg_err_t Gzip_Reader::read( void* out, long count )
{
	if ( in )
	{
		long actual = count;
		RETURN_ERR( read_( out, &actual ) );
		if ( actual == count )
			return 0;
	}
	return eof_error;
}

long Gzip_Reader::read_avail( void* out, long count )
{
	if ( !in || read_( out, &count ) )
		count = -1;
	return count;
}
