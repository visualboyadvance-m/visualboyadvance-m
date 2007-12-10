// Compressed file archive interface (C++)

// File_Extractor 0.4.3
#ifndef FILE_EXTRACTOR_H
#define FILE_EXTRACTOR_H

#include "fex.h"
#include "blargg_common.h"
#include "abstract_file.h"

struct File_Extractor : private Data_Reader {
public:
// Open/close

	// Opens archive
	blargg_err_t open( const char* path );

	// Opens archive from custom data source. Keeps pointer to input
	// until close(), but does not delete reader at close() unless you
	// call own_file() before then.
	blargg_err_t open( File_Reader* input );

	// Transfers ownership of current input file passed to open() so that
	// it will be deleted on close()
	void own_file() { own_file_ = true; }

	// Closes archive and frees memory
	void close();

// Scanning

	// True if end of archive has been reached
	bool done() const           { return done_; }

	// Path of current file in archive, using '/' as directory separator
	const char* name() const    { return name_; }

	// Size of current file, in bytes
	long size() const           { return size_; }

	// Modification date of file, in MS-DOS format (0 if unavailable).
	unsigned long dos_date() const  { return date_; }

	// Goes to next file (skips directories)
	blargg_err_t next();

	// Goes back to first file in archive
	blargg_err_t rewind();

// Data extraction

	// Read exactly n bytes and returns error if they couldn't ALL be read.
	// Reading past end of file results in eof_error.
	blargg_err_t read( void* out, long n );

	// Reads at most n bytes and returns number actually read, or negative if error.
	// Trying to read past end of file is NOT considered an error.
	long read_avail( void* out, long n );

	// Number of bytes remaining until end of current file
	long remain() const;

	// Reads and discards n bytes. Skipping past end of file results in eof_error.
	blargg_err_t skip( long n ) { return Data_Reader::skip( n ); }

	// Data_Reader to current file's data, so you can use standard Data_Reader
	// interface and not have to treat archives specially.
	Data_Reader& reader() { return *this; }

	// Alternate ways of extracting file. Only one method may be used
	// once before calling next() or rewind().

	// Extracts first n bytes and ignores rest.  Faster than a normal read since it
	// doesn't have to be prepared to read/decompress more data.
	virtual blargg_err_t read_once( void* out, long n );

	// Extracts all data and writes it to out
	virtual blargg_err_t extract( Data_Writer& out );

	// Gets pointer to file's data. Returned pointer is valid until next call to
	// next(), rewind(), close(), or open(). Sets *error_out to error. OK to call
	// more than once in a row; returns same pointer in that case.
	typedef unsigned char byte;
	byte const* data( blargg_err_t* error_out );

// Other features

	// Type of this extractor (fex_zip_type, etc.)
	fex_type_t type() const             { return type_; }

	// Hints that archive will either be only scanned (true), or might be extracted
	// from (false), improving performance on solid archives. Calling rewind()
	// resets this to the best default.
	void scan_only( bool b = true )     { scan_only_ = b; }

	// Sets/gets pointer to data you want to associate with this object.
	// You can use this for whatever you want.
	void set_user_data( void* p )       { user_data_ = p; }
	void* user_data() const             { return user_data_; }

	// Registers cleanup function to be called when deleting this object. Passes
	// user_data to cleanup function. Pass NULL to clear any cleanup function.
	void set_user_cleanup( fex_user_cleanup_t func ) { user_cleanup_ = func; }

	virtual ~File_Extractor();

public:
	BLARGG_DISABLE_NOTHROW
	File_Extractor( fex_type_t );
	Data_Reader::eof_error;
protected:
	// Services for extractors
	File_Reader& file() const                       { return *reader_; }
	void set_done()                                 { done_ = true; }
	void set_info( long size, const char* name, unsigned long date = 0 );
	bool is_scan_only() const                       { return scan_only_; }

	// Overridden by extractors
	virtual void open_filter_( const char* path, Std_File_Reader* );
	virtual blargg_err_t open_()                    BLARGG_PURE( { return 0; } )
	virtual blargg_err_t next_()                    BLARGG_PURE( { return 0; } )
	virtual blargg_err_t rewind_()                  BLARGG_PURE( { return 0; } )
	virtual void         close_()                   BLARGG_PURE( { } )
	virtual void         clear_file_()              { }
	virtual blargg_err_t data_( byte const*& out );
private:
	fex_type_t const type_;
	File_Reader* reader_;
	bool own_file_;
	bool scan_only_;
	bool done_;

	const char* name_;
	unsigned long date_;
	long size_;
	long data_pos_; // current position in data
	byte const* data_ptr_;
	byte* own_data_; // memory we allocated for data

	void* user_data_;
	fex_user_cleanup_t user_cleanup_;

	void init_state();
	void clear_file();
	void free_data();
};

// Default to Std_File_Reader for archive access
#ifndef FEX_FILE_READER
	#define FEX_FILE_READER Std_File_Reader
#endif

#endif
