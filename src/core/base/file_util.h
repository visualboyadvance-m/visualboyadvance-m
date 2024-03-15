#ifndef VBAM_CORE_BASE_FILE_UTIL_H_
#define VBAM_CORE_BASE_FILE_UTIL_H_

#include <cstdio>

#if defined(__LIBRETRO__)
#include <cstdint>
#else  // !defined(__LIBRETRO__)
#include <zlib.h>
#endif  // defined(__LIBRETRO__)

// save game
typedef struct {
        void *address;
        int size;
} variable_desc;

FILE* utilOpenFile(const char *filename, const char *mode);

#if defined(__LIBRETRO__)

void utilWriteIntMem(uint8_t *&data, int);
void utilWriteMem(uint8_t *&data, const void *in_data, unsigned size);
void utilWriteDataMem(uint8_t *&data, variable_desc *);

int utilReadIntMem(const uint8_t *&data);
void utilReadMem(void *buf, const uint8_t *&data, unsigned size);
void utilReadDataMem(const uint8_t *&data, variable_desc *);

#else  // !defined(__LIBRETRO__)

// strip .gz or .z off end
void utilStripDoubleExtension(const char *, char *);

gzFile utilAutoGzOpen(const char *file, const char *mode);
gzFile utilGzOpen(const char *file, const char *mode);
gzFile utilMemGzOpen(char *memory, int available, const char *mode);
int utilGzWrite(gzFile file, const voidp buffer, unsigned int len);
int utilGzRead(gzFile file, voidp buffer, unsigned int len);
int utilGzClose(gzFile file);
z_off_t utilGzSeek(gzFile file, z_off_t offset, int whence);
long utilGzMemTell(gzFile file);
void utilWriteData(gzFile, variable_desc *);
void utilReadData(gzFile, variable_desc *);
void utilReadDataSkip(gzFile, variable_desc *);
int utilReadInt(gzFile);
void utilWriteInt(gzFile, int);

#endif  // defined(__LIBRETRO__)

#endif  // VBAM_CORE_BASE_FILE_UTIL_H_