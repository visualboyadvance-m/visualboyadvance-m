// File_Extractor 1.0.0. http://www.slack.net/~ant/

#include <stdio.h>
#include <stdlib.h>
#include <zlib.h>

#include "Tar_Extractor.h"

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

// TODO: could close file once data has been read into memory

static blargg_err_t init_tar_file()
{
	get_crc_table(); // initialize zlib's CRC-32 tables
	return blargg_ok;
}

static File_Extractor* new_tar()
{
	return BLARGG_NEW Tar_Extractor;
}

fex_type_t_ const fex_tar_type [1]  = {{
	".tar",
	&new_tar,
	"tar file",
	&init_tar_file
}};

Tar_Extractor::Tar_Extractor() :
	File_Extractor( fex_tar_type )
{ }

Tar_Extractor::~Tar_Extractor()
{
	close();
}

blargg_err_t Tar_Extractor::open_path_v()
{
    RETURN_ERR( open_arc_file(true) );
    RETURN_ERR( arc().seek( 0 ) );
	return open_v();
}

blargg_err_t Tar_Extractor::stat_v()
{
	set_info( arc().remain(), 0, 0 );
	return blargg_ok;
}

blargg_err_t Tar_Extractor::open_v()
{
    arc().read(&header, BLOCKSIZE);
	set_name( header.name );
#if __STDC_WANT_SECURE_LIB__
    sscanf_s(header.size, "%o", &tarsize);
#else
    sscanf(header.size, "%o", &tarsize);
#endif
	return blargg_ok;
}
    
void Tar_Extractor::close_v()
{
	name.clear();
}

blargg_err_t Tar_Extractor::next_v()
{
    arc().read(&header, BLOCKSIZE);
    set_name( header.name );
#if __STDC_WANT_SECURE_LIB__
    sscanf_s(header.size, "%o", &tarsize);
#else
    sscanf(header.size, "%o", &tarsize);
#endif
	return blargg_ok;
}

blargg_err_t Tar_Extractor::rewind_v()
{
    arc().seek(0);
	return blargg_ok;
}

blargg_err_t Tar_Extractor::extract_v( void* p, int n )
{
	return arc().read( p, n );
}
