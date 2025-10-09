// Presents a gzipped file as an "archive" of just that file.
// Also handles non-gzipped files.

// File_Extractor 1.0.0
#ifndef XZ_EXTRACTOR_H
#define XZ_EXTRACTOR_H

#include "File_Extractor.h"
#include "XZ_Reader.h"

class XZ_Extractor : public File_Extractor
{
        public:
        XZ_Extractor();
        virtual ~XZ_Extractor();

        protected:
        virtual blargg_err_t open_path_v();
        virtual blargg_err_t open_v();
        virtual void close_v();

        virtual blargg_err_t next_v();
        virtual blargg_err_t rewind_v();

        virtual blargg_err_t stat_v();
        virtual blargg_err_t extract_v(void *, int);

        private:
        XZ_Reader gr;
        blargg_vector<char> name;

        void set_info_();
};

#endif
