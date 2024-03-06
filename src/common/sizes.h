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
constexpr size_t k16MiB = 2 * k8MiB;
constexpr size_t k32MiB = 2 * k16MiB;

// Game Boy BIOS Sizes.
constexpr size_t kGBBiosSize = k256B;
constexpr size_t kCGBBiosSize = k2KiB + k256B;

// Size of the buffer containing the BIOS, we alawys use the larger size.
constexpr size_t kGBBiosBufferSize = kCGBBiosSize;

// Game Boy-related screen sizes.
constexpr size_t kGBWidth = 160;
constexpr size_t kGBHeight = 144;
constexpr size_t kSGBWidth = 256;
constexpr size_t kSGBHeight = 224;
#ifdef __LIBRETRO__
constexpr size_t kGBPixSize = 4 * kSGBWidth * kSGBHeight;
#else
// Some of the filters and interframe blenders write beyond the end of the line
// or the screen, so we allocate extra space here.
constexpr size_t kGBPixSize = 4 * (kSGBWidth + 1) * (kSGBHeight + 2);
#endif

// Game Boy VRAM is 16 KiB.
constexpr size_t kGBVRamSize = k16KiB;

// Game Boy WRAM is 32 KiB.
constexpr size_t kGBWRamSize = k32KiB;

// A Game Boy line buffer size is the width of the screen at 2 bytes per pizel.
constexpr size_t kGBLineBufferSize = kGBWidth * 2;

#endif  // VBAM_COMMON_SIZES_H_
