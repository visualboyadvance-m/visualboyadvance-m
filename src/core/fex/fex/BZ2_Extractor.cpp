// File_Extractor 1.0.0. http://www.slack.net/~ant/

#if FEX_ENABLE_BZ2

#include "BZ2_Extractor.h"
#include <zlib.h>

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

// TODO: could close file once data has been read into memory

static blargg_err_t init_bz2_file()
{
	get_crc_table(); // initialize zlib's CRC-32 tables
	return blargg_ok;
}

static File_Extractor* new_bzip2()
{
	return BLARGG_NEW BZ2_Extractor;
}

fex_type_t_ const fex_bz2_type [1]  = {{
	".bz2",
	&new_bzip2,
	"bzip2 file",
	&init_bz2_file
}};

BZ2_Extractor::BZ2_Extractor() :
	File_Extractor( fex_bz2_type )
{ }

BZ2_Extractor::~BZ2_Extractor()
{
	close();
}

blargg_err_t BZ2_Extractor::open_path_v()
{
	// skip opening file
	return open_v();
}

blargg_err_t BZ2_Extractor::stat_v()
{
	RETURN_ERR( open_arc_file( true ) );
	if ( !gr.opened() || gr.tell() != 0 )
		RETURN_ERR( gr.open( &arc() ) );
	
	set_info( gr.remain(), 0, gr.crc32() );
	return blargg_ok;
}

blargg_err_t BZ2_Extractor::open_v()
{
	// Remove .gz suffix
	size_t len = strlen( arc_path() );
	if ( fex_has_extension( arc_path(), ".bz2" ) )
		len -= 4;
	
	RETURN_ERR( name.resize( len + 1 ) );
	memcpy( name.begin(), arc_path(), name.size() );
	name [name.size() - 1] = '\0';
	
	set_name( name.begin() );
	return blargg_ok;
}

void BZ2_Extractor::close_v()
{
	name.clear();
	gr.close();
}

blargg_err_t BZ2_Extractor::next_v()
{
	return blargg_ok;
}

blargg_err_t BZ2_Extractor::rewind_v()
{
	set_name( name.begin() );
	return blargg_ok;
}

blargg_err_t BZ2_Extractor::extract_v( void* p, int n )
{
	return gr.read( p, n );
}

#endif
