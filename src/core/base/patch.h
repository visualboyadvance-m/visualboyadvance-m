#ifndef VBAM_CORE_BASE_PATCH_H_
#define VBAM_CORE_BASE_PATCH_H_

#include <cstdint>

bool applyPatch(const char *patchname, uint8_t **rom, int *size);

#endif  // VBAM_CORE_BASE_PATCH_H_
