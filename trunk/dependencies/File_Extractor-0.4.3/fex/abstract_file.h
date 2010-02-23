// Lightweight interfaces for reading and writing data

// File_Extractor 0.4.3
#ifndef ABSTRACT_FILE_H
#define ABSTRACT_FILE_H

#include "blargg_common.h"
#include "Data_Reader.h"

// Supports writing
class Data_Writer {
public:
	Data_Writer() { }
	virtual ~Data_Writer();

	// Writes 'n' bytes. Returns error if they couldn't all be written.
	virtual blargg_err_t write( void const*, long n ) BLARGG_PURE( { (void) n; return 0; } )

public:
	typedef blargg_err_t error_t; // deprecated
	BLARGG_DISABLE_NOTHROW
private:
	// noncopyable
	Data_Writer( const Data_Writer& );
	Data_Writer& operator = ( const Data_Writer& );
};

// Disk file
class Std_File_Writer : public Data_Writer {
public:
	blargg_err_t open( const char* path );
	void close();

public:
	Std_File_Writer();
	~Std_File_Writer();
	blargg_err_t write( void const*, long );
private:
	void* file_;
};

// Writes data to memory
class Mem_Writer : public Data_Writer {
public:
	// Keeps all written data in expanding block of memory
	Mem_Writer();

	// Writes to fixed-size block of memory. If ignore_excess is false, returns
	// error if more than 'size' data is written, otherwise ignores any excess.
	Mem_Writer( void*, long size, int ignore_excess = 0 );

	// Pointer to beginning of written data
	char* data() { return data_; }

	// Number of bytes written
	long size() const { return size_; }

public:
	~Mem_Writer();
	blargg_err_t write( void const*, long );
private:
	char* data_;
	long size_;
	long allocated;
	enum { expanding, fixed, ignore_excess } mode;
};

// Written data is ignored
class Null_Writer : public Data_Writer {
public:
	blargg_err_t write( void const*, long );
};

// Gzip compressor
#ifdef HAVE_ZLIB_H
class Gzip_File_Writer : public Data_Writer {
public:
	// Compression level varies from 0 (min, fast) to 9 (max, slow). -1 = default.
	blargg_err_t open( const char* path, int compression_level = -1 );
	void close();

public:
	Gzip_File_Writer();
	~Gzip_File_Writer();
	blargg_err_t write( const void*, long );
private:
	void* file_;
};
#endif

#endif
