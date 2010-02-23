// 7-zip archive extractor

// File_Extractor 0.4.3
#ifndef ZIP7_EXTRACTOR_H
#define ZIP7_EXTRACTOR_H

#include "File_Extractor.h"

struct Zip7_Extractor_Impl;

class Zip7_Extractor : public File_Extractor {
public:
	Zip7_Extractor();
	~Zip7_Extractor();
protected:
	blargg_err_t open_();
	blargg_err_t next_();
	blargg_err_t rewind_();
	void close_();
	blargg_err_t data_( byte const*& out );
private:
	Zip7_Extractor_Impl* impl;
	int index;

	blargg_err_t extract_();
};

#endif
