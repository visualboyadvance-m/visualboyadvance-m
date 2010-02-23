// ZIP archive extractor. Only supports deflation and store (no compression).

// File_Extractor 0.4.3
#ifndef ZIP_EXTRACTOR_H
#define ZIP_EXTRACTOR_H

#include "File_Extractor.h"
#include "Zlib_Inflater.h"

class Zip_Extractor : public File_Extractor {
public:
	Zip_Extractor();
	~Zip_Extractor();
	blargg_err_t read( void*, long );
	blargg_err_t read_once( void*, long );
	long remain() const;
protected:
	void open_filter_( const char*, Std_File_Reader* );
	blargg_err_t open_();
	blargg_err_t next_();
	blargg_err_t rewind_();
	void clear_file_();
	void close_();
private:
	blargg_vector<char> catalog;
	long file_size;
	long catalog_begin; // offset of first catalog entry in file (to detect corruption)
	long catalog_pos;   // position in catalog data
	long raw_remain;    // bytes remaining in raw data for current file
	long remain_;       // byres remaining to be extracted
	unsigned long crc;
	unsigned long correct_crc;
	int method;
	Zlib_Inflater buf;

	blargg_err_t fill_buf( long offset, long buf_size, long initial_read );
	blargg_err_t update_info( bool advance_first );
	blargg_err_t first_read( long count );
	static blargg_err_t inflater_read( void* data, void* out, long* count );
};

#endif
