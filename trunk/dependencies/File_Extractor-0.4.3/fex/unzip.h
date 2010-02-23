/* Accesses file archives using basic unzip 1.01e library interface functions.
With this you can use File_Extractor in place of the unzip library with minimal
changes to your code. If you're writing new code, use the fex.h interface. */

/* File_Extractor 0.4.3 */
#ifndef _unz_H
#define _unz_H

/* Your code can check for this to know to support other archive types */
#define FEX_WRAPPER 1

#include "zlib.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef int unzError;

#define UNZ_OK                      0
#define UNZ_END_OF_LIST_OF_FILE     -100
#define UNZ_ERRNO                   Z_ERRNO
#define UNZ_EOF                     0
#define UNZ_PARAMERROR              -102
#define UNZ_BADZIPFILE              -103
#define UNZ_INTERNALERROR           -104
#define UNZ_CRCERROR                -105

typedef struct File_Extractor* unzFile;

/* Only indicated fields are supported; all others are set to 0 (and
named with a trailing _ so your code won't compile if it uses them). */
typedef struct tm_unz_s
{
	uInt tm_sec_;
	uInt tm_min_;
	uInt tm_hour_;
	uInt tm_mday_;
	uInt tm_mon_;
	uInt tm_year_;
} tm_unz;

typedef struct unz_file_info_s
{
	uLong version_;
	uLong version_needed_;
	uLong flag_;
	uLong compression_method_;
	uLong dosDate;              /* supported */
	uLong crc_;
	uLong compressed_size_;
	uLong uncompressed_size;    /* supported */
	uLong size_filename;        /* supported */
	uLong size_file_extra_;
	uLong size_file_comment_;

	uLong disk_num_start_;
	uLong internal_fa_;
	uLong external_fa_;

	tm_unz tmu_date_;
} unz_file_info;

/* Opens file archive or returns NULL if error */
unzFile unzOpen( const char* path );

/* Goes to first file in archive */
unzError unzGoToFirstFile( unzFile );

/* Goes to next file in archive */
unzError unzGoToNextFile( unzFile );

/* Scans for file of given name and returns UNZ_OK if found */
unzError unzLocateFile( unzFile, const char* name, int case_sensitive );

/* Compares two strings with optional case sensitivity */
int unzStringFileNameCompare( const char* x, const char* y, int case_sensitive );

/* Gets name and size of current file. Doesn't support extra_out and
comment_out. */
unzError unzGetCurrentFileInfo( unzFile, unz_file_info* info_out,
		char* filename_out, uLong filename_size,
		void* extra_out, uLong extra_size,
		char* comment_out, uLong comment_size );

/* Prepares to read data from current file */
unzError unzOpenCurrentFile( unzFile );

/* Reads at most 'count' bytes from current file into 'out' and returns number of
bytes actually read. Returns UNZ_EOF if end of file is reached, negative if error. */
unzError unzReadCurrentFile( unzFile, void* out, unsigned count );

/* Number of bytes read from current file */
z_off_t unztell( unzFile );

/* True if end of file has been reached */
int unzeof( unzFile );

/* Finishes reading from current file */
unzError unzCloseCurrentFile( unzFile );

/* Closes a file archive and frees memory */
unzError unzClose( unzFile );

#ifdef __cplusplus
	}
#endif

#endif
