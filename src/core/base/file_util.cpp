#include "file_util.h"

#if defined(__LIBRETRO__)
#error "This file is only for non-libretro builds"
#endif

#include <cstring>

#include "core/base/internal/file_util_internal.h"
#include "core/base/internal/memgzio.h"

#if defined(_MSC_VER)
#define strcasecmp _stricmp
#endif  // defined(_MSC_VER)

namespace {

bool utilIsGzipFile(const char* file) {
    if (strlen(file) > 3) {
        const char* p = strrchr(file, '.');

        if (p != nullptr) {
            if (strcasecmp(p, ".gz") == 0)
                return true;
            if (strcasecmp(p, ".z") == 0)
                return true;
        }
    }

    return false;
}

int(ZEXPORT *utilGzWriteFunc)(gzFile, const voidp, unsigned int) = nullptr;
int(ZEXPORT *utilGzReadFunc)(gzFile, voidp, unsigned int) = nullptr;
int(ZEXPORT *utilGzCloseFunc)(gzFile) = nullptr;
z_off_t(ZEXPORT *utilGzSeekFunc)(gzFile, z_off_t, int) = nullptr;

}  // namespace

void utilStripDoubleExtension(const char* file, char* buffer) {
    if (buffer != file)  // allows conversion in place
        strcpy(buffer, file);

    if (utilIsGzipFile(file)) {
        char* p = strrchr(buffer, '.');

        if (p)
            *p = 0;
    }
}

gzFile utilAutoGzOpen(const char* file, const char* mode) {
#if defined(_WIN32)

    std::wstring wfile = core::internal::ToUTF16(file);
    if (wfile.empty()) {
        return nullptr;
    }

    return gzopen_w(wfile.data(), mode);

#else  // !defined(_WIN32)

    return gzopen(file, mode);

#endif  // defined(_WIN32)
}

gzFile utilGzOpen(const char* file, const char* mode) {
    utilGzWriteFunc = (int(ZEXPORT*)(gzFile, void* const, unsigned int))gzwrite;
    utilGzReadFunc = gzread;
    utilGzCloseFunc = gzclose;
    utilGzSeekFunc = gzseek;

    return utilAutoGzOpen(file, mode);
}

gzFile utilMemGzOpen(char* memory, int available, const char* mode) {
    utilGzWriteFunc = memgzwrite;
    utilGzReadFunc = memgzread;
    utilGzCloseFunc = memgzclose;
    utilGzSeekFunc = memgzseek;

    return memgzopen(memory, available, mode);
}

int utilGzWrite(gzFile file, const voidp buffer, unsigned int len) {
    return utilGzWriteFunc(file, buffer, len);
}

int utilGzRead(gzFile file, voidp buffer, unsigned int len) {
    return utilGzReadFunc(file, buffer, len);
}

int utilGzClose(gzFile file) {
    return utilGzCloseFunc(file);
}

z_off_t utilGzSeek(gzFile file, z_off_t offset, int whence) {
    return utilGzSeekFunc(file, offset, whence);
}


long utilGzMemTell(gzFile file) {
    return memtell(file);
}

void utilWriteData(gzFile gzFile, variable_desc* data) {
    while (data->address) {
        utilGzWrite(gzFile, data->address, data->size);
        data++;
    }
}

void utilReadData(gzFile gzFile, variable_desc* data) {
    while (data->address) {
        utilGzRead(gzFile, data->address, data->size);
        data++;
    }
}

void utilReadDataSkip(gzFile gzFile, variable_desc* data) {
    while (data->address) {
        utilGzSeek(gzFile, data->size, SEEK_CUR);
        data++;
    }
}

int utilReadInt(gzFile gzFile) {
    int i = 0;
    utilGzRead(gzFile, &i, sizeof(int));
    return i;
}

void utilWriteInt(gzFile gzFile, int i) {
    utilGzWrite(gzFile, &i, sizeof(int));
}
