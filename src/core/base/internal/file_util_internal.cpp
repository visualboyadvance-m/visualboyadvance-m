#include "core/base/internal/file_util_internal.h"

#if defined(_WIN32)
#include <windows.h>
#endif  // defined(_WIN32)

namespace core {
namespace internal {

#if defined(_WIN32)

std::wstring ToUTF16(const char* utf8) {
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
    if (len == 0) {
        return std::wstring();
    }

    std::wstring result(len, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, (LPWSTR)result.data(), len);
    return result;
}

#endif  // defined(_WIN32)

}  // namespace internal
}  // namespace core

// Dummy symbol to suppress ranlib "has no symbols" warning
[[maybe_unused]] void vbam_dummy_symbol_file_util_internal() {}
