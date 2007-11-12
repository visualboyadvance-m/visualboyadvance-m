/* Accesses file archives using unrarlib 0.4.0 (UniquE RAR File Library) interface.
With this you can use File_Extractor in place of unrarlib with minimal
changes to your code. If you're writing new code, use the fex.h interface. */

/* File_Extractor 0.4.2 */
#ifndef URARLIB_H
#define URARLIB_H

#ifdef __cplusplus
	extern "C" {
#endif

struct RAR20_archive_entry
{
	/* Only Name, NameSize, and UnpSize are valid; all other fields are set to zero */
	char* Name;
	unsigned short NameSize;
	unsigned long PackSize;
	unsigned long UnpSize;
	unsigned char HostOS;
	unsigned long FileCRC;
	unsigned long FileTime;
	unsigned char UnpVer;
	unsigned char Method;
	unsigned long FileAttr;
};

typedef struct ArchiveList_struct
{
	struct RAR20_archive_entry item;
	struct ArchiveList_struct* next; /* next entry in list, or NULL if end of list */
	
} ArchiveList_struct;

/* Allocate and set *list_out to first node of linked list of files from file archive at
'archive_path' and return number of items in list. Use urarlib_freelist() to free memory
used by the list. */
int urarlib_list( const char* archive_path, ArchiveList_struct** list_out );

/* Extract 'filename' from a file archive at 'archive_path' into internal buffer.
Sets *buffer_out to beginning of data and *size_out to number of bytes extracted.
Returns true if successful otherwise false. Password is not supported (must be NULL).
Caller must free() returned buffer when done with it.
Note: buffer_out is treated as void**, but declared as void* to match original urarlib.h. */
int urarlib_get( void* buffer_out, unsigned long* size_out, const char* filename,
		const char* archive_path, const char* password );

/* Free memory used by list of nodes returned by urarlib_list(). Pointer must be that
returned by urarlib_list() (that is, the first node in the list). */
void urarlib_freelist( ArchiveList_struct* list );

#ifdef __cplusplus
	}
#endif

#endif
