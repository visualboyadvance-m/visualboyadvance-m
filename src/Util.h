#ifndef UTIL_H
#define UTIL_H

#include <cstdint>
#include <string>

#ifdef _WIN32
#define FILE_SEP '\\'
#else // MacOS, Unix
#define FILE_SEP '/'
#endif

#define FREAD_UNCHECKED(A,B,C,D) (void)(fread(A,B,C,D) + 1)

enum IMAGE_TYPE { IMAGE_UNKNOWN = -1, IMAGE_GBA = 0, IMAGE_GB = 1 };

std::string get_xdg_user_config_home();
std::string get_xdg_user_data_home();

void utilReadScreenPixels(uint8_t *dest, int w, int h);
#ifndef __LIBRETRO__
bool utilWritePNGFile(const char *, int, int, uint8_t *);
bool utilWriteBMPFile(const char *, int, int, uint8_t *);
#endif
bool utilIsGBAImage(const char *);
bool utilIsGBImage(const char *);
IMAGE_TYPE utilFindType(const char *);
uint8_t *utilLoad(const char *, bool (*)(const char *), uint8_t *, int &);

void utilPutDword(uint8_t *, uint32_t);
void utilGBAFindSave(const int);
void utilUpdateSystemColorMaps(bool lcd = false);

#endif // UTIL_H
