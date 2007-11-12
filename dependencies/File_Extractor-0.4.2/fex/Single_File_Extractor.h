// Presents a single file as an "archive" of just that file.
// Also handles gzipped file.

// File_Extractor 0.4.2
#ifndef SINGLE_FILE_EXTRACTOR_H
#define SINGLE_FILE_EXTRACTOR_H

#include "File_Extractor.h"
#include "Gzip_Reader.h"

class Single_File_Extractor : public File_Extractor {
public:
	// Set reported name of file
	void set_name( const char* );
	
public:
	Single_File_Extractor();
	~Single_File_Extractor();
	File_Extractor::open;
	long read_avail( void*, long );
	error_t read( void*, long );
	long remain() const;
	error_t read_once( void*, long );
protected:
	void open_filter_( const char*, Std_File_Reader* );
	error_t open_();
	error_t next_();
	error_t rewind_();
	void close_();
private:
	Gzip_Reader gr;
	long size_;
	char name [256];
};

#endif
