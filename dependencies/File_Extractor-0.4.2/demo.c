/* C example that opens an archive, lists its contents and
extracts the first several characters of each file. */

#include "fex/fex.h"

#include <stdlib.h>
#include <stdio.h>

/* If error is not NULL, prints it and exits program */
void error( const char* error );

int main( int argc, char* argv [] )
{
	/* Path to archive */
	const char* path = (argc > 1 ? argv [argc - 1] : "test.zip");

	/* Open archive */
	File_Extractor* fex;
	error( fex_open( path, &fex ) );

	/* Iterate over each file in archive */
	while ( !fex_done( fex ) )
	{
		const char* name = fex_name( fex );
		long size = fex_size( fex );
		char buf [50] = "";

		/* Print size and name */
		printf( "## %3ld K  %s\n", size / 1024, name );

		/* Print first several characters of data */
		error( fex_read( fex, buf, (size < 49 ? size : 49) ) );
		printf( "%s\n", buf );

		/* Go to next file in archive (skips directories) */
		error( fex_next( fex ) );
	}

	/* Cleanup */
	fex_close( fex );

	return 0;
}

void error( const char* error )
{
	if ( error )
	{
		fprintf( stderr, "Error: %s\n", error );
		exit( EXIT_FAILURE );
	}
}
