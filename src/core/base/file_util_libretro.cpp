#include "core/base/file_util.h"

#if !defined(__LIBRETRO__)
#error "This file is only for libretro builds"
#endif

#include <cstring>

// Not endian safe, but VBA itself doesn't seem to care, so hey <_<
void utilWriteIntMem(uint8_t*& data, int val) {
    memcpy(data, &val, sizeof(int));
    data += sizeof(int);
}

void utilWriteMem(uint8_t*& data, const void* in_data, unsigned size) {
    memcpy(data, in_data, size);
    data += size;
}

void utilWriteDataMem(uint8_t*& data, variable_desc* desc) {
    while (desc->address) {
        utilWriteMem(data, desc->address, desc->size);
        desc++;
    }
}

int utilReadIntMem(const uint8_t*& data) {
    int res;
    memcpy(&res, data, sizeof(int));
    data += sizeof(int);
    return res;
}

void utilReadMem(void* buf, const uint8_t*& data, unsigned size) {
    memcpy(buf, data, size);
    data += size;
}

void utilReadDataMem(const uint8_t*& data, variable_desc* desc) {
    while (desc->address) {
        utilReadMem(desc->address, data, desc->size);
        desc++;
    }
}
