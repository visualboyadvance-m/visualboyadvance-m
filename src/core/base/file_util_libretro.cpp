#include "core/base/file_util.h"

#if !defined(__LIBRETRO__)
#error "This file is only for libretro builds"
#endif

#include <cstdlib>
#include <cstring>

IMAGE_TYPE utilFindType(const char* file) {
    if (utilIsGBAImage(file))
        return IMAGE_GBA;

    if (utilIsGBImage(file))
        return IMAGE_GB;

    return IMAGE_UNKNOWN;
}

static int utilGetSize(int size) {
    int res = 1;
    while (res < size)
        res <<= 1;
    return res;
}

uint8_t* utilLoad(const char* file, bool (*)(const char*), uint8_t* data, int& size) {
    FILE* fp = nullptr;

    fp = fopen(file, "rb");
    if (!fp) {
        log("Failed to open file %s", file);
        return nullptr;
    }
    fseek(fp, 0, SEEK_END);  // go to end

    size = ftell(fp);  // get position at end (length)
    rewind(fp);

    uint8_t* image = data;
    if (image == nullptr) {
        image = (uint8_t*)malloc(utilGetSize(size));
        if (image == nullptr) {
            log("Failed to allocate memory for %s", file);
            return nullptr;
        }
    }

    if (fread(image, 1, size, fp) != (size_t)size) {
        log("Failed to read from %s", file);
        fclose(fp);
        return nullptr;
    }

    fclose(fp);
    return image;
}

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
