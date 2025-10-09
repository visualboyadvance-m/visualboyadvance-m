// Separate file to avoid linking to f* functions unless user calls unrar_open_file()

#include "unrar.h"
#include "rar.hpp"
#include <stdio.h>

extern "C" {
	static unrar_err_t unrar_read_file( void* user_data, void* out, int* count, unrar_pos_t pos )
	{
		FILE* file = (FILE*) user_data;
		
		// most of the time, seeking won't be necessary
		if ( pos != ftell( file ) && fseek( file, (long)pos, SEEK_SET ) != 0 )
			return unrar_err_corrupt;
		
		*count = (int) fread( out, 1, *count, file );
		
		if ( ferror( file ) != 0 )
			return unrar_err_io;
		
		return unrar_ok;
	}
}

static void unrar_close_file( void* user_data )
{
	fclose( (FILE*) user_data );
}

unrar_err_t unrar_open( unrar_t** arc_out, const char path [] )
{
	*arc_out = NULL;

#if _MSC_VER >= 1300
	FILE* file = NULL;
	fopen_s(&file, path, "rb");
#else
	FILE* file = fopen( path, "rb" );
#endif
	if ( file == NULL )
		return unrar_err_open;
	
	unrar_err_t err = unrar_open_custom( arc_out, &unrar_read_file, file );
	if ( err != unrar_ok )
		fclose( file );
	else
		(*arc_out)->close_file = &unrar_close_file;
	
	return err;
}
