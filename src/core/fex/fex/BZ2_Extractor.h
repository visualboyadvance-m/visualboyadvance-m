// Presents a gzipped file as an "archive" of just that file.
// Also handles non-gzipped files.

// File_Extractor 1.0.0
#ifndef BZ2_EXTRACTOR_H
#define BZ2_EXTRACTOR_H

#include "File_Extractor.h"
#include "BZ2_Reader.h"

class BZ2_Extractor : public File_Extractor
{
        public:
        BZ2_Extractor();
        virtual ~BZ2_Extractor();

        protected:
        virtual blargg_err_t open_path_v();
        virtual blargg_err_t open_v();
        virtual void close_v();

        virtual blargg_err_t next_v();
        virtual blargg_err_t rewind_v();

        virtual blargg_err_t stat_v();
        virtual blargg_err_t extract_v(void *, int);

        private:
        BZ2_Reader gr;
        blargg_vector<char> name;

        void set_info_();
};

#endif
