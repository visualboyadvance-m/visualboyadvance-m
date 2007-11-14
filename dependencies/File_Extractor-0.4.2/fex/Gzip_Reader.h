// Transparently decompresses gzip files, as well as uncompressed

// File_Extractor 0.4.2
#ifndef GZIP_READER_H
#define GZIP_READER_H

#include "Data_Reader.h"
#include "Zlib_Inflater.h"

class Gzip_Reader : public Data_Reader {
public:
	error_t open( File_Reader* );
	void close();
	long tell() const { return tell_; }

	// True if file is compressed
	bool deflated() const { return inflater.deflated(); }

public:
	Gzip_Reader();
	~Gzip_Reader();
	long remain() const;
	error_t read( void*, long );
	long read_avail( void*, long );
private:
	File_Reader* in;
	long tell_;
	long size_;
	Zlib_Inflater inflater;

	error_t calc_size();
	blargg_err_t read_( void* out, long* count );
};

#endif
