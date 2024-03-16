#include "file_util.h"

#include <cstring>

#include "core/base/internal/file_util_internal.h"

#if defined(_MSC_VER)
#define strcasecmp _stricmp
#endif  // defined(_MSC_VER)

void utilPutDword(uint8_t* p, uint32_t value) {
    *p++ = value & 255;
    *p++ = (value >> 8) & 255;
    *p++ = (value >> 16) & 255;
    *p = (value >> 24) & 255;
}

FILE* utilOpenFile(const char* filename, const char* mode) {
#ifdef _WIN32
    std::wstring wfilename = core::internal::ToUTF16(filename);
    if (wfilename.empty()) {
        return nullptr;
    }

    std::wstring wmode = core::internal::ToUTF16(mode);
    if (wmode.empty()) {
        return nullptr;
    }

    return _wfopen(wfilename.data(), wmode.data());
#else
    return fopen(filename, mode);
#endif  // _WIN32
}

bool utilIsGBAImage(const char* file) {
    coreOptions.cpuIsMultiBoot = false;
    if (strlen(file) > 4) {
        const char* p = strrchr(file, '.');

        if (p != nullptr) {
            if ((strcasecmp(p, ".agb") == 0) || (strcasecmp(p, ".gba") == 0) ||
                (strcasecmp(p, ".bin") == 0) || (strcasecmp(p, ".elf") == 0))
                return true;
            if (strcasecmp(p, ".mb") == 0) {
                coreOptions.cpuIsMultiBoot = true;
                return true;
            }
        }
    }

    return false;
}

bool utilIsGBImage(const char* file) {
    if (strlen(file) > 4) {
        const char* p = strrchr(file, '.');

        if (p != nullptr) {
            if ((strcasecmp(p, ".dmg") == 0) || (strcasecmp(p, ".gb") == 0) ||
                (strcasecmp(p, ".gbc") == 0) || (strcasecmp(p, ".cgb") == 0) ||
                (strcasecmp(p, ".sgb") == 0))
                return true;
        }
    }

    return false;
}
