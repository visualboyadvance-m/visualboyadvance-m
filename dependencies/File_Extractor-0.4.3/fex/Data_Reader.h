// Lightweight interface for reading data

// File_Extractor 0.4.3
#ifndef DATA_READER_H
#define DATA_READER_H

#include "blargg_common.h"

// Supports reading and finding out how many bytes are remaining
class Data_Reader {
public:
	virtual ~Data_Reader() { }

	static const char eof_error []; // returned by read() when request goes beyond end

	// Reads at most n bytes and returns number actually read, or negative if error.
	// Trying to read past end of file is NOT considered an error.
	virtual long read_avail( void*, long n ) BLARGG_PURE( { (void) n; return 0; } )

	// Read exactly n bytes and returns error if they couldn't ALL be read.
	// Reading past end of file results in eof_error.
	virtual blargg_err_t read( void*, long n );

	// Number of bytes remaining until end of file
	virtual long remain() const BLARGG_PURE( { return 0; } )

	// Reads and discards n bytes. Skipping past end of file results in eof_error.
	virtual blargg_err_t skip( long n );

public:
	Data_Reader() { }
	typedef blargg_err_t error_t; // deprecated
	BLARGG_DISABLE_NOTHROW
private:
	// noncopyable
	Data_Reader( const Data_Reader& );
	Data_Reader& operator = ( const Data_Reader& );
};

// Supports seeking in addition to Data_Reader operations
class File_Reader : public Data_Reader {
public:
	// Size of file
	virtual long size() const BLARGG_PURE( { return 0; } )

	// Current position in file
	virtual long tell() const BLARGG_PURE( { return 0; } )

	// Goes to new position
	virtual blargg_err_t seek( long ) BLARGG_PURE( { return 0; } )

	long remain() const;
	blargg_err_t skip( long n );
};

// Disk file reader
class Std_File_Reader : public File_Reader {
public:
	blargg_err_t open( const char* path );
	void close();

	// Switches to unbuffered mode. Useful if you are doing your
	// own buffering already.
	void make_unbuffered();

public:
	Std_File_Reader();
	~Std_File_Reader();
	long size() const;
	blargg_err_t read( void*, long );
	long read_avail( void*, long );
	long tell() const;
	blargg_err_t seek( long );
private:
	void* file_;
};

// Treats range of memory as a file
class Mem_File_Reader : public File_Reader {
public:
	Mem_File_Reader( const void* begin, long size );

public:
	long size() const;
	long read_avail( void*, long );
	long tell() const;
	blargg_err_t seek( long );
private:
	const char* const begin;
	const long size_;
	long pos;
};

// Makes it look like there are only count bytes remaining
class Subset_Reader : public Data_Reader {
public:
	Subset_Reader( Data_Reader*, long count );

public:
	long remain() const;
	long read_avail( void*, long );
private:
	Data_Reader* in;
	long remain_;
};

// Joins already-read header and remaining data into original file.
// Meant for cases where you've already read header and don't want
// to seek and re-read data (for efficiency).
class Remaining_Reader : public Data_Reader {
public:
	Remaining_Reader( void const* header, long header_size, Data_Reader* );

public:
	long remain() const;
	long read_avail( void*, long );
	blargg_err_t read( void*, long );
private:
	char const* header;
	char const* header_end;
	Data_Reader* in;
	long read_first( void* out, long count );
};

// Invokes callback function to read data. Size of data must be specified in advance.
// Passes user_data unchanged to your callback.
class Callback_Reader : public Data_Reader {
public:
	typedef const char* (*callback_t)( void* user_data, void* out, long count );
	Callback_Reader( callback_t, long size, void* user_data = 0 );
public:
	long read_avail( void*, long );
	blargg_err_t read( void*, long );
	long remain() const;
private:
	callback_t const callback;
	void* const data;
	long remain_;
};

#ifdef HAVE_ZLIB_H
// Gzip compressed file reader
class Gzip_File_Reader : public File_Reader {
public:
	blargg_err_t open( const char* path );
	void close();

public:
	Gzip_File_Reader();
	~Gzip_File_Reader();
	long size() const;
	long read_avail( void*, long );
	long tell() const;
	blargg_err_t seek( long );
private:
	void* file_;
	long size_;
};
#endif

#endif
