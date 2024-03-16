#ifndef VBAM_CORE_BASE_IMAGE_UTIL_H_
#define VBAM_CORE_BASE_IMAGE_UTIL_H_

#if defined(__LIBRETRO__)
#error "This file is not meant for compilation in libretro builds."
#endif  // defined(__LIBRETRO__)

#include <cstdint>

bool utilWritePNGFile(const char*, int, int, uint8_t*);
bool utilWriteBMPFile(const char*, int, int, uint8_t*);

#endif  // VBAM_CORE_BASE_IMAGE_UTIL_H_