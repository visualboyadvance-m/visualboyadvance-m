#ifndef PATCH_H
#define PATCH_H

#include <cstdint>

bool applyPatch(const char *patchname, uint8_t **rom, int *size);

#endif // PATCH_H
