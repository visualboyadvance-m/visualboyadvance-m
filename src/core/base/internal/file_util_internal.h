#ifndef VBAM_CORE_BASE_INTERNAL_FILE_UTIL_INTERNAL_H_
#define VBAM_CORE_BASE_INTERNAL_FILE_UTIL_INTERNAL_H_

#if defined(_WIN32)
#include <string>
#endif  // defined(_WIN32)

namespace core {
namespace internal {

#if defined(_WIN32)

// Convert UTF-8 to UTF-16. Returns an empty string on error.
std::wstring ToUTF16(const char* utf8);

#endif  // defined(_WIN32)

}  // namespace internal
}  // namespace core

#endif  // VBAM_CORE_BASE_INTERNAL_FILE_UTIL_INTERNAL_H_
