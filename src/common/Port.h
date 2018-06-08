#ifndef PORT_H
#define PORT_H

#include "Types.h"

#ifdef __CELLOS_LV2__
/* PlayStation3 */
#include <ppu_intrinsics.h>
#endif

#ifdef _XBOX360
/* XBox 360 */
#include <ppcintrinsics.h>
#endif

// swaps a 16-bit value
static inline uint16_t swap16(uint16_t v)
{
        return (v << 8) | (v >> 8);
}

// swaps a 32-bit value
static inline uint32_t swap32(uint32_t v)
{
        return (v << 24) | ((v << 8) & 0xff0000) | ((v >> 8) & 0xff00) | (v >> 24);
}

#ifdef WORDS_BIGENDIAN
#if defined(__GNUC__) && defined(__ppc__)

#define READ16LE(base)                                                                             \
        ({                                                                                         \
                unsigned short lhbrxResult;                                                        \
                __asm__("lhbrx %0, 0, %1" : "=r"(lhbrxResult) : "r"(base) : "memory");             \
                lhbrxResult;                                                                       \
        })

#define READ32LE(base)                                                                             \
        ({                                                                                         \
                unsigned long lwbrxResult;                                                         \
                __asm__("lwbrx %0, 0, %1" : "=r"(lwbrxResult) : "r"(base) : "memory");             \
                lwbrxResult;                                                                       \
        })

#define WRITE16LE(base, value) __asm__("sthbrx %0, 0, %1" : : "r"(value), "r"(base) : "memory")

#define WRITE32LE(base, value) __asm__("stwbrx %0, 0, %1" : : "r"(value), "r"(base) : "memory")

#else
#define READ16LE(x) swap16(*((uint16_t *)(x)))
#define READ32LE(x) swap32(*((uint32_t *)(x)))
#define WRITE16LE(x, v) *((uint16_t *)x) = swap16((v))
#define WRITE32LE(x, v) *((uint32_t *)x) = swap32((v))
#endif
#else
#define READ16LE(x) *((uint16_t *)x)
#define READ32LE(x) *((uint32_t *)x)
#define WRITE16LE(x, v) *((uint16_t *)x) = (v)
#define WRITE32LE(x, v) *((uint32_t *)x) = (v)
#endif

#endif // PORT_H
