// File_Extractor 0.4.2. http://www.slack.net/~ant/

#include "unrarlib.h"

#include "File_Extractor.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

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

// log error message to stderr when debugging
inline const char* log_error( const char* str )
{
	#ifndef NDEBUG
		if ( str )
			fprintf( stderr, "unrarlib Error: %s\n", str );
	#endif
	return str;
}

int urarlib_list( const char* path, ArchiveList_struct** list )
{
	ArchiveList_struct** tail = list;
	int count = 0;
	*list = 0;
	
	File_Extractor* fex = 0;
	if ( log_error( fex_open( path, &fex ) ) )
		goto error;
	
	fex->scan_only(); // significantly faster for solid archives
	
	while ( !fex->done() )
	{
		ArchiveList_struct* item = (ArchiveList_struct*) calloc( sizeof *item, 1 );
		if ( !item )
			goto error;
		
		// insert at end
		*tail = item;
		item->next = 0;
		tail = &item->next;
		count++;
		
		item->item.NameSize = strlen( fex->name() );
		item->item.Name = (char*) malloc( item->item.NameSize + 1 );
		if ( !item->item.Name )
			goto error;
		
		strcpy( item->item.Name, fex->name() );
		item->item.UnpSize = fex->size();
		
		if ( log_error( fex->next() ) )
			goto error;
	}
	
	if ( 0 )
	{
	error:
		urarlib_freelist( *list );
		*list = 0;
		count = 0;
	}
	delete fex;
	return count;
}

void urarlib_freelist( ArchiveList_struct* node )
{
	while ( node )
	{
		ArchiveList_struct* next = node->next;
		free( node->item.Name );
		free( node );
		node = next;
	}
}

int urarlib_get( void* output, unsigned long* size, const char* filename,
		const char* path, const char* password )
{
	assert( !password ); // password not supported
	
	*(void**) output = 0;
	*size = 0;
	void* buf = 0;
	
	File_Extractor* fex = 0;
	if ( log_error( fex_open( path, &fex ) ) )
		goto error;
	
	while ( !fex->done() )
	{
		if ( !strcmp( fex->name(), filename ) )
		{
			buf = malloc( fex->size() );
			if ( !buf || log_error( fex->read( buf, fex->size() ) ) )
				goto error;
			
			*(void**) output = buf;
			*size = fex->size();
			delete fex;
			return true;
		}
		
		if ( log_error( fex->next() ) )
			goto error;
	}
	// not found
error:
	free( buf );
	delete fex;
	return false;
}
