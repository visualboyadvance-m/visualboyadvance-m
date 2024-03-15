#include "file_util.h"

#include "core/base/internal/file_util_internal.h"

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
