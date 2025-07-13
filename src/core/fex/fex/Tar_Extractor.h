// Presents a gzipped file as an "archive" of just that file.
// Also handles non-gzipped files.

// File_Extractor 1.0.0
#ifndef TAR_EXTRACTOR_H
#define TAR_EXTRACTOR_H

#include "File_Extractor.h"

typedef struct posix_header
{                        /* byte offset */
  char name[100];        /*   0 */
  char mode[8];          /* 100 */
  char uid[8];           /* 108 */
  char gid[8];           /* 116 */
  char size[12];         /* 124 */
  char mtime[12];        /* 136 */
  char chksum[8];        /* 148 */
  char typeflag;         /* 156 */
  char linkname[100];    /* 157 */
  char magic[6];         /* 257 */
  char version[2];       /* 263 */
  char uname[32];        /* 265 */
  char gname[32];        /* 297 */
  char devmajor[8];      /* 329 */
  char devminor[8];      /* 337 */
  char prefix[155];      /* 345 */
  char padding[12];      /* 500 */
                         /* 512 */
} posix_header;

#define BLOCKSIZE 512

#define TMAGIC   "ustar"    /* ustar and a null */
#define TMAGLEN  6
#define TVERSION "00"        /* 00 and no null */
#define TVERSLEN 2

/* Values used in typeflag field.  */
#define REGTYPE  '0'        /* regular file */
#define AREGTYPE '\0'       /* regular file */
#define LNKTYPE  '1'        /* link */
#define SYMTYPE  '2'        /* reserved */
#define CHRTYPE  '3'        /* character special */
#define BLKTYPE  '4'        /* block special */
#define DIRTYPE  '5'        /* directory */
#define FIFOTYPE '6'        /* FIFO special */
#define CONTTYPE '7'        /* reserved */

class Tar_Extractor : public File_Extractor
{
        public:
        Tar_Extractor();
        virtual ~Tar_Extractor();

        protected:
        virtual blargg_err_t open_path_v();
        virtual blargg_err_t open_v();
        virtual void close_v();

        virtual blargg_err_t next_v();
        virtual blargg_err_t rewind_v();

        virtual blargg_err_t stat_v();
        virtual blargg_err_t extract_v(void *, int);

        private:
        File_Reader *gr;
        posix_header header;
        unsigned int tarsize;
        blargg_vector<char> name;

        void set_info_();
};

#endif
