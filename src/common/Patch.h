#ifndef PATCH_H
#define PATCH_H

#ifndef __LIBRETRO__
#include "cstdint.h"
#else
#include <stdint.h>
#endif

bool applyPatch(const char *patchname, uint8_t **rom, int *size);

#endif // PATCH_H
