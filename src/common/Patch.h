#ifndef PATCH_H
#define PATCH_H

#include "../Util.h"
#include "Types.h"

bool applyPatch(const char *patchname, uint8_t **rom, int *size);

#endif // PATCH_H
