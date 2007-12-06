/* Compressed file archive C interface (also usable from C++) */

/* File_Extractor 0.4.2 */
#ifndef FEX_H
#define FEX_H

#ifdef __cplusplus
	extern "C" {
#endif

/* Error string returned by library functions, or NULL if no error (success) */
typedef const char* fex_err_t;

/* First parameter of most extractor_ functions is a pointer to the File_Extractor */
typedef struct File_Extractor File_Extractor;


/**** Basics ****/

/* Opens archive and points *out to it */
fex_err_t fex_open( const char* path, File_Extractor** out );

/* True if at end of archive */
int fex_done( File_Extractor const* );

/* Name of current file */
const char* fex_name( File_Extractor const* );

/* Size of current file */
long fex_size( File_Extractor const* );

/* Extracts at 'count' bytes and writes them to *out. Returns error if that many
bytes couldn't be extracted. */
fex_err_t fex_read( File_Extractor*, void* out, long count );

/* Goes to next file in archive (skips directories) */
fex_err_t fex_next( File_Extractor* );

/* Closes archive and frees memory */
void fex_close( File_Extractor* );


/**** Advanced ****/

/* Goes back to first file in archive */
fex_err_t fex_rewind( File_Extractor* );

/* Hints to fex_next() that no file extraction will occur, speeding scanning
of some archive types. */
void fex_scan_only( File_Extractor* );

/* Number of bytes remaining to be read from current file */
long fex_remain( File_Extractor const* );

/* Reads at most n bytes and returns number actually read, or negative if error */
long fex_read_avail( File_Extractor*, void* out, long n );

/* Extracts first n bytes and ignores rest. Faster than a normal read since it
doesn't need to read any more data. Must not be called twice in a row. */
fex_err_t fex_read_once( File_Extractor*, void* out, long n );

/* Gets pointer to file data in memory (NULL if error). Returned pointer is valid
until next call to fex_next(), fex_rewind(), or fex_close(). Will return same
pointer if called more than once. */
fex_err_t fex_data( File_Extractor*, const char** data_out );


/**** Archive types ****/

/* fex_type_t is a pointer to this structure. For example, fex_zip_type->extension is
"ZIP" and ex_zip_type->new_fex() is equilvant to 'new Zip_Extractor' (in C++). */
struct fex_type_t_
{
	const char* extension;      /* file extension/type */
	File_Extractor* (*new_fex)();
};

/* Archive type constants for each supported file type */
extern struct fex_type_t_ const fex_7z_type [1], fex_gz_type [1],
		fex_zip_type [1], fex_single_file_type [1];
typedef struct fex_type_t_ const* fex_type_t;

/* Pointer to array of archive types, with NULL entry at end. Allows a program linked
to this library to support new archive types without having to be updated. */
fex_type_t const* fex_type_list();

/* Type of this extractor */
fex_type_t fex_type( File_Extractor const* );


/******** Advanced opening ********/

/* Error returned if file is wrong type */
extern const char fex_wrong_file_type [29];

/* Determines likely archive type based on first four bytes of file. Returns
string containing proper file suffix (i.e. "ZIP", "GZ", etc.) or "" if
file header is not recognized. */
const char* fex_identify_header( void const* header );

/* Gets corresponding archive type for file path or extension passed in. Returns NULL
if type isn't recognized. */
fex_type_t fex_identify_extension( const char* path_or_extension );

/* Determines file type based on file's extension or header (if extension isn't recognized).
Sets *type_out to result, or NULL if type isn't recognized or error occurs. */
fex_err_t fex_identify_file( const char* path, fex_type_t* type_out );

/* Opens archive of specific type. Points *out to archive. */
fex_err_t fex_open_type( fex_type_t, const char* path, File_Extractor** out );


/******** User data ********/

/* Sets/gets pointer to data you want to associate with this extractor.
You can use this for whatever you want. */
void  fex_set_user_data( File_Extractor*, void* new_user_data );
void* fex_user_data( File_Extractor const* );

/* Registers cleanup function to be called when deleting extractor, or NULL to
clear it. Passes user_data to cleanup function. */
typedef void (*fex_user_cleanup_t)( void* user_data );
void fex_set_user_cleanup( File_Extractor*, fex_user_cleanup_t func );


#ifdef __cplusplus
	}
#endif

#endif
