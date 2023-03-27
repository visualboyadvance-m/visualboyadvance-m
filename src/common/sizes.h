#ifndef VBAM_COMMON_SIZES_H_
#define VBAM_COMMON_SIZES_H_

#include <cstddef>

// Various size constants.
constexpr size_t k256B = 256;
constexpr size_t k512B = 2 * k256B;
constexpr size_t k1KiB = 2 * k512B;
constexpr size_t k2KiB = 2 * k1KiB;
constexpr size_t k4KiB = 2 * k2KiB;
constexpr size_t k8KiB = 2 * k4KiB;
constexpr size_t k16KiB = 2 * k8KiB;
constexpr size_t k32KiB = 2 * k16KiB;
constexpr size_t k64KiB = 2 * k32KiB;
constexpr size_t k128KiB = 2 * k64KiB;
constexpr size_t k256KiB = 2 * k128KiB;
constexpr size_t k512KiB = 2 * k256KiB;
constexpr size_t k1MiB = 2 * k512KiB;
constexpr size_t k2MiB = 2 * k1MiB;
constexpr size_t k4MiB = 2 * k2MiB;
constexpr size_t k8MiB = 2 * k4MiB;

#endif  // VBAM_COMMON_SIZES_H_
