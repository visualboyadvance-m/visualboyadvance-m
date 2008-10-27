#ifndef VBA_UTIL_H
#define VBA_UTIL_H

#include "System.h"

enum IMAGE_TYPE {
  IMAGE_UNKNOWN = -1,
  IMAGE_GBA     = 0,
  IMAGE_GB      = 1
};

// save game

typedef struct {
  void *address;
  int size;
} variable_desc;

extern bool utilWritePNGFile(const char *, int, int, u8 *);
extern bool utilWriteBMPFile(const char *, int, int, u8 *);
extern void utilApplyIPS(const char *ips, u8 **rom, int *size);
extern bool utilIsGBAImage(const char *);
extern bool utilIsGBImage(const char *);
extern bool utilIsGzipFile(const char *);
extern void utilStripDoubleExtension(const char *, char *);
extern IMAGE_TYPE utilFindType(const char *);
extern u8 *utilLoad(const char *,
                    bool (*)(const char*),
                    u8 *,
                    int &);

extern void utilPutDword(u8 *, u32);
extern void utilPutWord(u8 *, u16);
extern void utilWriteData(gzFile, variable_desc *);
extern void utilReadData(gzFile, variable_desc *);
extern void utilReadDataSkip(gzFile, variable_desc *);
extern int utilReadInt(gzFile);
extern void utilWriteInt(gzFile, int);
extern gzFile utilGzOpen(const char *file, const char *mode);
extern gzFile utilMemGzOpen(char *memory, int available, const char *mode);
extern int utilGzWrite(gzFile file, const voidp buffer, unsigned int len);
extern int utilGzRead(gzFile file, voidp buffer, unsigned int len);
extern int utilGzClose(gzFile file);
extern z_off_t utilGzSeek(gzFile file, z_off_t offset, int whence);
extern long utilGzMemTell(gzFile file);
extern void utilGBAFindSave(const u8 *, const int);
extern void utilUpdateSystemColorMaps();
#endif
