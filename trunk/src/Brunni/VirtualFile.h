/*! \file VirtualFile.h
    \brief
    	Virtual File support for Oldschool Library.
		This API is meant to be an universal mean to manipulate every file source possible as you can define your own.
*/

#ifndef __OSL_VIRTUALFILE_H__
#define __OSL_VIRTUALFILE_H__

#ifdef __cplusplus
extern "C" {
#endif

/** @defgroup virtualfile Virtual Files

	Virtual File support for OSLib.
	This API is meant to be an universal mean to manipulate every file source possible as you can define your own.
	@{
*/

/** Virtual File type. */
typedef struct		{
	void *ioPtr;				//!< User data for IO processing (usually a pointer to data, FILE*, usw.)
	unsigned short type;		//!< Virtual file type (source number).
	unsigned long userData;		//!< Additional data
	int offset, maxSize;		//!< Internal variables for memory-based (RAM / ROM) sources
} VIRTUAL_FILE;

/** Enumeration describing the available file open modes. Please note that some sources do not support some modes like READWRITE or WRITE, in
this case they'll fail when an attempt to open a file in one of these modes is made. The resulting file returned by VirtualFileOpen will be NULL. */
enum VF_OPEN_MODES {
	VF_O_READ,					//!< Read only
	VF_O_READWRITE,				//!< Read & Write
	VF_O_WRITE					//!< Write only
};
//enum {SEEK_SET, SEEK_CUR, SEEK_END};

/** Structure of a Virtual File Source. */
typedef struct			{
	/**
		Open a file.
		
		Return 1 if anything went well, or 0 to throw an error (the opening will be cancelled and NULL will be returned to the user).
	*/
	int (*fOpen)(void *param1, int param2, int type, int mode, VIRTUAL_FILE* f);

	/**
		Close a file.
		
		Return 1 if anything went well, 0 to throw an error.
	*/
	int (*fClose)(VIRTUAL_FILE *f);

	/**
		Read in a file.
		
		Returns the number of bytes effectively read.
	*/
	int (*fRead)(void *ptr, size_t size, size_t n, VIRTUAL_FILE* f);

	/**
		Write in a file
		
		Returns the number of bytes effectively written.
	*/
	int (*fWrite)(const void *ptr, size_t size, size_t n, VIRTUAL_FILE* f);

	/**
		Read a single character.

		Returns the next character (byte) in the file.
	*/
	int (*fGetc)(VIRTUAL_FILE *f);

	/**
		Write a single character
		
		Writes a single character. Returns the character value if anything went well, -1 else.
	*/
	int (*fPutc)(int caractere, VIRTUAL_FILE *f);

	/**
		Read a string
		
		Reads a string to the buffer pointed to by str, of a maximum size of maxLen. Returns a pointer to the read string (str in general).
		Reading stops to the next carriage return found (\\n). The routine should handle correctly the following sequences by reading them
		entirely: \\r, \\r\\n, \\n. If a \\r is found, you should read the following byte and rewind one byte behind if it's not a \\n.
		If you cannot afford a rewind, then keep the next character in cache and return it to the next read.
	*/
	char* (*fGets)(char *str, int maxLen, VIRTUAL_FILE *f);

	/**
		Write a string

		Writes an entire string to the file.
	*/
	void (*fPuts)(const char *s, VIRTUAL_FILE *f);

	/**
		Moving in the file
		
		Sets the current file position and returns the old one. The whence parameter uses the same values as stdio (SEEK_SET, SEEK_CUR, SEEK_END).
	*/
	void (*fSeek)(VIRTUAL_FILE *f, int offset, int whence);

	/**
		Get current file position

		Returns the current pointer position in the file. You can then use VirtualFileSeek(f, position, SEEK_SET) to return to it.
	*/
	int (*fTell)(VIRTUAL_FILE *f);

	/**
		End of file

		Returns true (1) if it's the end of the file, false (0) else.
	*/
	int (*fEof)(VIRTUAL_FILE *f);

} VIRTUAL_FILE_SOURCE;

/** Virtual file list item. Used for RAM based devices. */
typedef struct			{
   const char *name;						//!< Virtual file name
   void *data;								//!< RAM data block
   int size;								//!< Block data size
   int *type;								//!< Associated source (e.g. &VF_MEMORY). Don't forget the &, which is there so you can pass a variable which is not known at compile time (virtual file sources are registered upon start, so the compiler doesn't know the ID it will be given in advance).
} OSL_VIRTUALFILENAME;

/** Initializes the virtual filesystem. Done by default by OSLib, so there is no need to call it by yourself. */
void VirtualFileInit();

/** Open a new file.
	\param param1
		Pointer to a string representing the file name.
	\param type
		File type. By default, can be:
			- VF_MEMORY: read/write from a memory block
			- VF_FILE: read from standard stdio routines.
	\param mode
		One of VF_OPEN_MODES.
*/	
VIRTUAL_FILE *VirtualFileOpen(void *param1, int param2, int type, int mode);
/** Closes an open file. */
int VirtualFileClose(VIRTUAL_FILE *f);


/** @defgroup virtualfile_io I/O routines

	Routines for reading / writing to a virtual file. Make sure to check the Virtual File main page to know how to open files, etc.
	@{
*/

/** Writes in a file and returns the number of bytes effectively written. */
#define VirtualFileWrite(ptr, size, n, f)	(VirtualFileGetSource(f)->fWrite(ptr, size, n, f))
/** Reads in a file and returns the number of bytes effectively read. */
#define VirtualFileRead(ptr, size, n, f)	(VirtualFileGetSource(f)->fRead(ptr, size, n, f))
/** Reads a single character. Returns the next character (byte) in the file. */
#define VirtualFileGetc(f)					(VirtualFileGetSource(f)->fGetc(f))
/** Writes a single character. Returns the character value if anything went well, -1 else. */
#define VirtualFilePutc(caractere, f)		(VirtualFileGetSource(f)->fPutc(caractere, f))
/** Reads a string to the buffer pointed to by str, of a maximum size of maxLen. Returns a pointer to the read string (str in general).

Reading stops to the next carriage return found (\\n, \\r\\n or \\r), supporting files created by every OS (I think). */
#define VirtualFileGets(str, maxLen, f)		(VirtualFileGetSource(f)->fGets(str, maxLen, f))
/** Writes a string to the file. */
#define VirtualFilePuts(s, f)				(VirtualFileGetSource(f)->fPuts(s, f))
/** Sets the current file position and returns the old one. The whence parameter uses the same values as stdio (SEEK_SET, SEEK_CUR, SEEK_END). */
#define VirtualFileSeek(f, offset, whence)	(VirtualFileGetSource(f)->fSeek(f, offset, whence))
/** Returns the current file pointer. */
#define VirtualFileTell(f)					(VirtualFileGetSource(f)->fTell(f))
/** Returns true (1) if it's the end of the file, false (0) else. */
#define VirtualFileEof(f)					(VirtualFileGetSource(f)->fEof(f))

/** @} */ // end of virtualfile_io


/** Adds a new file source to the virtual file system.

	\param vfs
		Must be a pointer to a valid VIRTUAL_FILE_SOURCE interface containing all your functions for handling the file source.
	\return
		Returns the identifier of your source or -1 if it has failed. You can then use this ID as a file type (parameter for VirtualFileOpen). */
int VirtualFileRegisterSource(VIRTUAL_FILE_SOURCE *vfs);

/*  UNSUPPORTED! SEE oslAddVirtualFileList INSTEAD!

	Registers a file list for RAM based devices (such as VF_MEMORY).
	\param vfl
		List of files (each file is a UL_VIRTUALFILENAME item)
*/
//void oslSetVirtualFilenameList(OSL_VIRTUALFILENAME *vfl, int numberOfEntries);

/** Call this function in your virtual file source OPEN handler, if it's a RAM based device and param2 == 0 (means null size, impossible).
It will return a UL_VIRTUALFILENAME where you can get a pointer to the data and their size. Note that the return value can be NULL if the file has not been found or an error occured (e.g. fname is NULL). */
OSL_VIRTUALFILENAME *oslFindFileInVirtualFilenameList(const char *fname, int type);

//Maximum number of sources
#define VF_MAX_SOURCES 16
//Gets a pointer to the virtual file source associated to a file
#define VirtualFileGetSource(vf)		(VirtualFileSources[(vf)->type])
//List of virtual file sources
extern VIRTUAL_FILE_SOURCE *VirtualFileSources[VF_MAX_SOURCES];
extern int VirtualFileSourcesNb;
extern OSL_VIRTUALFILENAME *osl_virtualFileList;
extern int osl_virtualFileListNumber;
extern int osl_defaultVirtualFileSource;
extern const char *osl_tempFileName;

/** Reads an entire file to memory and returns a pointer to the memory block. The block memory usage is stepped by 4 kB, so even a 1 kB file will take 16 kB, so this function is not recommended for opening a lot of small files. The bloc must be freed (using free) once you've finished with it!
	\param f
		Pointer to an open virtual file.
	\param size
		Pointer to an integer that will contain the number of bytes read from the file. Can be NULL (in this case the value is discarded).
		
\code
int size;
VIRTUAL_FILE *f;
char *data;
//Open a file
f = VirtualFileOpen("test.txt", 0, VF_AUTO, VF_O_READ);
//Read it entirely to RAM
data = oslReadEntireFileToMemory(f, &size);
//Print each character
for (i=0;i<size;i++)
	oslPrintf("%c", data[i]);
\endcode */
extern void *oslReadEntireFileToMemory(VIRTUAL_FILE *f, int *size);



/*
	Source par défaut: mémoire
*/
extern int vfsMemOpen(void *param1, int param2, int type, int mode, VIRTUAL_FILE* f);
extern int vfsMemClose(VIRTUAL_FILE *f);
extern int vfsMemWrite(const void *ptr, size_t size, size_t n, VIRTUAL_FILE* f);
extern int vfsMemRead(void *ptr, size_t size, size_t n, VIRTUAL_FILE* f);
extern int vfsMemGetc(VIRTUAL_FILE *f);
extern int vfsMemPutc(int caractere, VIRTUAL_FILE *f);
extern char *vfsMemGets(char *str, int maxLen, VIRTUAL_FILE *f);
extern void vfsMemPuts(const char *s, VIRTUAL_FILE *f);
extern void vfsMemSeek(VIRTUAL_FILE *f, int offset, int whence);
extern int vfsMemTell(VIRTUAL_FILE *f);
extern int vfsMemEof(VIRTUAL_FILE *f);
extern VIRTUAL_FILE_SOURCE vfsMemory;



/** @defgroup virtualfile_sources Virtual file sources

	There are two virtual file sources available by default: memory and file.
	@{
*/

/** Initializes the file system. You do not need to call it by yourself as it's done automatically by OSLib. */
int oslInitVfsFile();

/**
	Sets the default virtual file source (VF_FILE by default). Used if you use VF_AUTO, that is any oslLoad[...]File routine.

	OSLib will search in the current file list to find wether the file exists and is of a different type (e.g. VF_MEMORY). If the file is not found, it will treat it with the default type.

	\param source
		Can be VF_FILE, VF_MEMORY or any virtual file device registered by you.
*/
extern __inline void oslSetDefaultVirtualFileSource(int source)		{
	osl_defaultVirtualFileSource = source;
}

/** Read and write from memory. Automatically registered when initializing OSLib. */
extern int VF_MEMORY;
/** Read and write from a file. */
extern int VF_FILE;
/** Auto select source. Uses the current file list to find whether the file name exists in it. If it's not found, it uses the currently active source. */
#define VF_AUTO -2

/** @} */ // end of virtualfile_sources


/** @defgroup virtualfile_ram RAM virtual files

	There are two virtual file sources available by default: memory and file.
	@{
*/


/** Gets the name of the temporary file. See #oslSetTempFileData for a code sample. */
extern __inline char *oslGetTempFileName()		{
	return (char*)osl_tempFileName;
}

/** Sets the data associated to a temporary file.

\code
//Binary data representing a JPG file
const int test_data[] = {...};
//The file is 1280 bytes (it's the length of the test_data array)
const int test_data_size = 1280;
//Set data for the temporary file
oslSetTempFileData(test_data, test_data_size, &VF_MEMORY);
//Load a JPG file using the temporary file (oslGetTempFileName to get its name)
image = oslLoadImageFileJPG(oslGetTempFileName(), OSL_IN_RAM | OSL_SWIZZLED, OSL_PF_5650);
\endcode */
extern void oslSetTempFileData(void *data, int size, int *type);

/** Adds a list of virtual files. You will then be able to open these files as if they were real.
	\param vfl
		Pointer to an array of #OSL_VIRTUALFILENAME entries
	\param numberOfEntries
		Number of #OSL_VIRTUALFILENAME entries in the array

\code
//Binary data representing a JPG file
const int test_jpg[] = {...};
const char *text = "This is a text file";

//Each entry consists of a name, a pointer to the data, the size of the data, and
//a file type. See the OSL_VIRTUALFILENAME documentation for more information.
OSL_VIRTUALFILENAME ram_files[] =		{
	{"ram:/test.jpg", (void*)test_jpg, sizeof(test_jpg), &VF_MEMORY},
	{"ram:/something.txt", (void*)text, strlen(text), &VF_MEMORY},
};

//Add these files to the list
oslAddVirtualFileList(ram_files, oslNumberof(ram_files));

//We can now open them as if they were real files...
VirtualFileOpen("ram:/something.txt", 0, VF_AUTO, VF_O_READ);
oslLoadImageFileJPG("ram:/test.jpg", OSL_IN_VRAM, OSL_PF_5650);

[...]
\endcode */
extern int oslAddVirtualFileList(OSL_VIRTUALFILENAME *vfl, int numberOfEntries);

/** Removes file entries from the virtual file list.

\code
OSL_VIRTUALFILENAME ram_files[] =		{
	[..]
};

//Add these files to the list
oslAddVirtualFileList(ram_files, oslNumberof(ram_files));
//Removing them is as simple as adding them
oslRemoveVirtualFileList(ram_files, oslNumberof(ram_files));
\endcode */
extern void oslRemoveVirtualFileList(OSL_VIRTUALFILENAME *vfl, int numberOfEntries);

/** @} */ // end of virtualfile_ram


/** @} */ // end of virtualfile

#ifdef __cplusplus
}
#endif

#endif

