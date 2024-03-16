#include "file_util.h"

#if defined(__LIBRETRO__)
#error "This file is only for non-libretro builds"
#endif

#include <cstdlib>
#include <cstring>

#include "core/base/internal/file_util_internal.h"
#include "core/base/internal/memgzio.h"
#include "core/base/message.h"
#include "core/fex/fex.h"

#if defined(_MSC_VER)
#define strcasecmp _stricmp
#endif  // defined(_MSC_VER)

#define MAX_CART_SIZE 0x8000000  // 128MB

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

// Opens and scans archive using accept(). Returns fex_t if found.
// If error or not found, displays message and returns nullptr.
fex_t* scanArchive(const char* file, bool (*accept)(const char*), char (&buffer)[2048]) {
    fex_t* fe;
    fex_err_t err = fex_open(&fe, file);
    if (!fe) {
        systemMessage(MSG_CANNOT_OPEN_FILE, N_("Cannot open file %s: %s"), file, err);
        return nullptr;
    }

    // Scan filenames
    bool found = false;
    while (!fex_done(fe)) {
        strncpy(buffer, fex_name(fe), sizeof buffer);
        buffer[sizeof buffer - 1] = '\0';

        utilStripDoubleExtension(buffer, buffer);

        if (accept(buffer)) {
            found = true;
            break;
        }

        err = fex_next(fe);
        if (err) {
            systemMessage(MSG_BAD_ZIP_FILE, N_("Cannot read archive %s: %s"), file, err);
            fex_close(fe);
            return nullptr;
        }
    }

    if (!found) {
        systemMessage(MSG_NO_IMAGE_ON_ZIP, N_("No image found in file %s"), file);
        fex_close(fe);
        return nullptr;
    }
    return fe;
}

bool utilIsImage(const char* file) {
    return utilIsGBAImage(file) || utilIsGBImage(file);
}

int utilGetSize(int size) {
    int res = 1;
    while (res < size)
        res <<= 1;
    return res;
}

IMAGE_TYPE utilFindType(const char* file, char (&buffer)[2048]) {
    fex_t* fe = scanArchive(file, utilIsImage, buffer);
    if (!fe) {
        return IMAGE_UNKNOWN;
    }

    fex_close(fe);
    file = buffer;
    return utilIsGBAImage(file) ? IMAGE_GBA : IMAGE_GB;
}

int(ZEXPORT* utilGzWriteFunc)(gzFile, const voidp, unsigned int) = nullptr;
int(ZEXPORT* utilGzReadFunc)(gzFile, voidp, unsigned int) = nullptr;
int(ZEXPORT* utilGzCloseFunc)(gzFile) = nullptr;
z_off_t(ZEXPORT* utilGzSeekFunc)(gzFile, z_off_t, int) = nullptr;

}  // namespace

uint8_t* utilLoad(const char* file, bool (*accept)(const char*), uint8_t* data, int& size) {
    // find image file
    char buffer[2048];
    fex_t* fe = scanArchive(file, accept, buffer);
    if (!fe)
        return nullptr;

    // Allocate space for image
    fex_err_t err = fex_stat(fe);
    int fileSize = fex_size(fe);
    if (size == 0)
        size = fileSize;

    if (size > MAX_CART_SIZE)
        return nullptr;

    uint8_t* image = data;

    if (image == nullptr) {
        // allocate buffer memory if none was passed to the function
        image = (uint8_t*)malloc(utilGetSize(size));
        if (image == nullptr) {
            fex_close(fe);
            systemMessage(MSG_OUT_OF_MEMORY, N_("Failed to allocate memory for %s"), "data");
            return nullptr;
        }
        size = fileSize;
    }

    // Read image
    int read = fileSize <= size ? fileSize : size;  // do not read beyond file
    err = fex_read(fe, image, read);
    fex_close(fe);
    if (err) {
        systemMessage(MSG_ERROR_READING_IMAGE, N_("Error reading image from %s: %s"), buffer, err);
        if (data == nullptr)
            free(image);
        return nullptr;
    }

    size = fileSize;

    return image;
}

IMAGE_TYPE utilFindType(const char* file) {
    char buffer[2048];
    return utilFindType(file, buffer);
}

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
