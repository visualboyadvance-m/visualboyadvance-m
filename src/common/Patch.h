#ifndef PATCH_H
#define PATCH_H

#include "Types.h"

bool applyPatch(const char *patchname, u8 **rom, unsigned int *size);

#endif // PATCH_H
