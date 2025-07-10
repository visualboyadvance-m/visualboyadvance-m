/** RAR archive scanning and extraction \file */

/* unrar_core 3.8.5 */
#ifndef UNRAR_H
#define UNRAR_H

#include <stddef.h>
#include <limits.h>

#include "../fex/blargg_common.h"

#if !defined (UNRAR_NO_LONG_LONG) && defined (LLONG_MAX)
	typedef long long unrar_long_long;
    typedef unsigned long long unrar_ulong_long;
#else
	typedef long unrar_long_long;
    typedef unsigned long unrar_ulong_long;
#endif

#ifdef __cplusplus
	extern "C" {
#endif


/** Error code, or 0 if function was successful. See Errors for more. Except
where noted, once an operation returns an error, that archive should not be
used any further, other than with unrar_close(). */
#ifndef unrar_err_t /* (#ifndef allows better testing of library) */
	typedef int unrar_err_t;
#endif

/** First parameter of most functions is unrar_t*, or const unrar_t* if nothing
is changed. */
typedef struct unrar_t unrar_t;

/** File position */
typedef unrar_long_long unrar_pos_t;

/** Boolean, where 0 is false and 1 is true */
typedef int unrar_bool;


/******** Open/close ********/

/** Initializes static tables used by library. Automatically called by
unrar_open(). OK to call more than once. */
void unrar_init( void );

/** Opens archive and points *out at it. If error, sets *out to NULL. */
unrar_err_t unrar_open( unrar_t** out, const char path [] );

/** User archive read callback. When called, user_data is a copy of that passed
to unrar_open_custom(). Callback must do the following: Read avail bytes from
file at offset pos and set *count to avail, where avail is the lesser of *count
and file_size-pos. Put read bytes into *out and return unrar_ok. If fewer than
avail bytes could be read successfully, return a non-zero error code. */
typedef unrar_err_t (*unrar_read_func)( void* user_data,
		void* out, int* count, unrar_pos_t pos );

/** Same as unrar_open(), except data is read using supplied function rather
than from file. */
unrar_err_t unrar_open_custom( unrar_t** unrar_out,
		unrar_read_func, void* user_data );

/** Closes archive and frees memory. OK to pass NULL. */
void unrar_close( unrar_t* );


/******** Scanning ********/

/** True if at end of archive. Must be called after unrar_open() or
unrar_rewind(), as an archive might contain no files. */
unrar_bool unrar_done( const unrar_t* );

/** Goes to next file in archive. If there are no more files, unrar_done() will
now return true. */
unrar_err_t unrar_next( unrar_t* );

/** Goes back to first file in archive, as if it were just opened with
unrar_open(). */
unrar_err_t unrar_rewind( unrar_t* );

/** Position of current file in archive. Will never return zero. */
unrar_pos_t unrar_tell( const unrar_t* );

/** Returns to file at previously-saved position. */
unrar_err_t unrar_seek( unrar_t*, unrar_pos_t );


/**** Info ****/

/** Information about current file */
typedef struct unrar_info_t
{
	unrar_pos_t     size;       /**< Uncompressed size */
	char     name[32767];       /**< Name, in Unicode if is_unicode is true */
    const wchar_t*  name_w;     /**< Name in Unicode, "" if unavailable */
	unrar_bool      is_unicode; /**< True if name is Unicode (UTF-8) */
	unsigned int    dos_date;   /**< Date in DOS-style format, 0 if unavailable */
	unsigned int    crc;        /**< Checksum; algorithm depends on archive */
	unrar_bool      is_crc32;   /**< True if crc is CRC-32 */
} unrar_info_t;

/** Information about current file. Pointer is valid until unrar_next(),
unrar_rewind(), unrar_seek(), or unrar_close(). */
const unrar_info_t* unrar_info( const unrar_t* );


/**** Extraction ****/

/**  Returns unrar_ok if current file can be extracted, otherwise error
indicating why it can't be extracted (too new/old compression algorithm,
encrypted, segmented). Archive is still usable if this returns error,
just the current file can't be extracted. */
unrar_err_t unrar_try_extract( const unrar_t* );

/**  Extracts at most size bytes from current file into out. If file is larger,
discards excess bytes. If file is smaller, only writes unrar_size() bytes. */
unrar_err_t unrar_extract( unrar_t*, void* out, unrar_pos_t size );

/**  Extracts data to memory and returns pointer to it in *out. Pointer is
valid until unrar_next(), unrar_rewind(), unrar_seek(), or unrar_close(). OK to
call more than once for same file. Optimized to avoid allocating memory when
entire file will already be kept in internal window. */
unrar_err_t unrar_extract_mem( unrar_t* p, void const** out );

/**  User extracted data write callback. When called, user_data is a copy of
that passed to unrar_extract_custom(). Callback must do the following: Write
count bytes from *in to wherever extracted data goes and return unrar_ok. If
data cannot be written successfully, return a non-zero error code. */
typedef unrar_err_t (*unrar_write_func)( void* user_data,
		const void* in, int count );

/**  Extracts current file and writes data using supplied function. Any error
it returns will be returned by this function, and archive will still be
usable. */
unrar_err_t unrar_extract_custom( unrar_t*,
		unrar_write_func, void* user_data );


/******** Errors ********/

/**  Error string associated with unrar error code. Always returns valid
pointer to a C string; never returns NULL. Returns "" for unrar_ok. */
const char* unrar_err_str( unrar_err_t );

enum {
	unrar_ok            =  0,/**< No error; success. Guaranteed to be zero. */
	unrar_err_memory    =  1,/**< Out of memory */
	unrar_err_open      =  2,/**< Couldn't open file (not found/permissions) */
	unrar_err_not_arc   =  3,/**< Not a RAR archive */
	unrar_err_corrupt   =  4,/**< Archive is corrupt */
	unrar_err_io        =  5,/**< Read failed */
	unrar_err_arc_eof   =  6,/**< At end of archive; no more files */ 
	unrar_err_encrypted =  7,/**< Encryption not supported */
	unrar_err_segmented =  8,/**< Segmentation not supported */
	unrar_err_huge      =  9,/**< Huge (2GB+) archives not supported */
	unrar_err_old_algo  = 10,/**< Compressed with unsupported old algorithm */
	unrar_err_new_algo  = 11,/**< Compressed with unsupported new algorithm */
	unrar_next_err      = 100/**< Errors range from 0 to unrar_next_err-1 */
};


#ifdef __cplusplus
	}
#endif

#endif
