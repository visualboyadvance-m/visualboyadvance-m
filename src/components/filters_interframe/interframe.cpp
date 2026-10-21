/// Interframe blending filters with thread-safe per-region buffering
/// Supports multithreaded IFB synchronized with FilterThread
///
/// Every kernel exists as a vector version (SSE2 / NEON, see
/// components/filters/simd128.h) and a scalar version with the same
/// per-pixel logic; the test suite checks that they agree. The MMX versions
/// remain for 32-bit x86 builds without SSE2.

#include "components/filters_interframe/interframe.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <vector>

#include "components/filters/simd128.h"

// MMX intrinsics for 32-bit builds without SSE2 (the compiler's own __MMX__,
// set by -mmmx; not the legacy MMX define of 2xSaI's NASM assembly)
#if defined(__MMX__) && defined(VBAM_SIMD_SCALAR) && !defined(VBAM_SIMD_FORCE_SCALAR)
#define USE_MMX
#include <mmintrin.h>
#endif

extern uint32_t qRGB_COLOR_MASK[2];

// ============================================================================
// InterframeManager Implementation
// ============================================================================

InterframeManager& InterframeManager::Instance() {
    static InterframeManager instance;
    return instance;
}

void InterframeManager::Init(int width, int height, int pitch, int numRegions) {
    std::lock_guard<std::mutex> lock(init_mutex_);

    // Only reinitialize if dimensions or region count changed
    if (initialized_ && width_ == width && height_ == height &&
        pitch_ == pitch && static_cast<int>(regions_.size()) == numRegions) {
        return;
    }

    CleanupInternal();

    width_ = width;
    height_ = height;
    pitch_ = pitch;

    // Calculate buffer size for each region
    // Each region needs full-frame sized buffers to store its slice of history
    // Buffer size: pitch * height (covers the full frame for offset calculations)
    buffer_size_ = static_cast<size_t>(pitch) * height;

    regions_.resize(numRegions);
    for (int i = 0; i < numRegions; i++) {
        regions_[i].frame1_.reset(new uint8_t[buffer_size_]());
        regions_[i].frame2_.reset(new uint8_t[buffer_size_]());
        regions_[i].frame3_.reset(new uint8_t[buffer_size_]());
    }

    initialized_ = true;
}

void InterframeManager::Cleanup() {
    std::lock_guard<std::mutex> lock(init_mutex_);
    CleanupInternal();
}

void InterframeManager::Clear() {
    std::lock_guard<std::mutex> lock(init_mutex_);

    if (!initialized_) {
        return;
    }

    // Zero out all frame buffers without reallocating
    for (auto& region : regions_) {
        if (region.frame1_) {
            memset(region.frame1_.get(), 0, buffer_size_);
        }
        if (region.frame2_) {
            memset(region.frame2_.get(), 0, buffer_size_);
        }
        if (region.frame3_) {
            memset(region.frame3_.get(), 0, buffer_size_);
        }
    }
}

void InterframeManager::CleanupInternal() {
    // Caller must hold init_mutex_
    regions_.clear();
    initialized_ = false;
    width_ = height_ = pitch_ = 0;
    buffer_size_ = 0;
}

// ============================================================================
// Color comparison helpers (all builds)
// ============================================================================

// Threshold for fuzzy color comparison (per channel, 0-255 range)
// Colors within this threshold are considered "equal" for pattern detection
static constexpr int kColorThreshold = 16;

// Helper: check if two 32-bit colors are "similar" (within threshold per channel)
static inline bool ColorsSimilar32(uint32_t a, uint32_t b, int threshold) {
    int dr = std::abs(static_cast<int>((a >> 16) & 0xFF) - static_cast<int>((b >> 16) & 0xFF));
    int dg = std::abs(static_cast<int>((a >> 8) & 0xFF) - static_cast<int>((b >> 8) & 0xFF));
    int db = std::abs(static_cast<int>(a & 0xFF) - static_cast<int>(b & 0xFF));
    return (dr <= threshold) && (dg <= threshold) && (db <= threshold);
}

// Helper: check if two 32-bit colors are "different" (any channel exceeds threshold)
static inline bool ColorsDifferent32(uint32_t a, uint32_t b, int threshold) {
    return !ColorsSimilar32(a, b, threshold);
}

// Threshold for 16-bit colors (scaled for 5 bit channels)
// RGB555: R=5bits, G=5bits, B=5bits; threshold ~2 is equivalent to ~16 for 8-bit
static constexpr int kColorThreshold16 = 2;

// Helper: check if two 16-bit colors are "similar" (RGB555 format)
// Always uses RGB555 format - don't check RGB_LOW_BITS_MASK which may be
// racily modified by filter threads running concurrently
static inline bool ColorsSimilar16(uint16_t a, uint16_t b, int threshold) {
    // RGB555: R(14:10), G(9:5), B(4:0)
    int dr = std::abs(static_cast<int>((a >> 10) & 0x1F) - static_cast<int>((b >> 10) & 0x1F));
    int dg = std::abs(static_cast<int>((a >> 5) & 0x1F) - static_cast<int>((b >> 5) & 0x1F));
    int db = std::abs(static_cast<int>(a & 0x1F) - static_cast<int>(b & 0x1F));
    return (dr <= threshold) && (dg <= threshold) && (db <= threshold);
}

// Helper: check if two 16-bit colors are "different"
static inline bool ColorsDifferent16(uint16_t a, uint16_t b, int threshold) {
    return !ColorsSimilar16(a, b, threshold);
}

// Threshold for 24-bit/8-bit colors (per byte, same as 32-bit)
static constexpr int kColorThreshold24 = 16;

// Helper: check if three consecutive bytes (R,G,B) are similar
static inline bool ColorsSimilar24(const uint8_t* a, const uint8_t* b, int threshold) {
    int dr = std::abs(static_cast<int>(a[0]) - static_cast<int>(b[0]));
    int dg = std::abs(static_cast<int>(a[1]) - static_cast<int>(b[1]));
    int db = std::abs(static_cast<int>(a[2]) - static_cast<int>(b[2]));
    return (dr <= threshold) && (dg <= threshold) && (db <= threshold);
}

// Helper: convert 8-bit palette index to 16-bit RGB555
static inline uint16_t PaletteToRGB555(uint8_t idx) {
    if (idx == 0xff) return 0x7fff;
    return static_cast<uint16_t>(((idx & 0xe0) << 7) | ((idx & 0x1c) << 5) | ((idx & 0x3) << 3));
}

// Helper: convert 16-bit RGB555 back to 8-bit palette approximation
static inline uint8_t RGB555ToPalette(uint16_t c) {
    return static_cast<uint8_t>(((c >> 7) & 0xe0) | ((c >> 5) & 0x1c) | ((c >> 3) & 0x3));
}

// RGB555 mask that clears the low bit of every channel: (c & mask) >> 1 halves the channels
static constexpr uint16_t kMask555 = 0x7BDE;

// ============================================================================
// Smart IB, one pixel: the reference logic. The scalar kernels run it for
// every pixel, the vector kernels for the pixels at the row ends where the
// neighbour window would leave the row.
//
// The 3-frames-ago history (src3) is read for the neighbours and replaced by
// the current frame; the replacement goes to new3 and is copied over src3
// once the row is done, so every pixel of a row compares against the old
// history. (Writing in place would let the left neighbours see the new frame.)
//
// Only pixels that CHANGED from the previous frame AND show an oscillation
// pattern (similar to one of the two older frames, at the same position or
// up to two pixels to either side) are blended with the previous frame.
// ============================================================================

static inline void SmartPixel32(uint32_t* src0, const uint32_t* src1, const uint32_t* src2, const uint32_t* src3, uint32_t* new3, int i,
                                int sPitch) {
    const uint32_t color = src0[i];
    bool oscillates = false;
    if (ColorsDifferent32(color, src1[i], kColorThreshold)) {
        oscillates = ColorsSimilar32(color, src2[i], kColorThreshold) ||
                     ColorsSimilar32(color, src3[i], kColorThreshold);
        if (!oscillates && i >= 2 && i < sPitch - 2) {
            for (int offset = 1; offset <= 2 && !oscillates; offset++) {
                oscillates = ColorsSimilar32(color, src2[i - offset], kColorThreshold) ||
                             ColorsSimilar32(color, src2[i + offset], kColorThreshold) ||
                             ColorsSimilar32(color, src3[i - offset], kColorThreshold) ||
                             ColorsSimilar32(color, src3[i + offset], kColorThreshold);
            }
        }
    }
    // 2-frame average preserves scroll motion better
    src0[i] = oscillates ? (((color & 0xFEFEFE) >> 1) + ((src1[i] & 0xFEFEFE) >> 1)) : color;
    new3[i] = color;
}

static inline void SmartPixel16(uint16_t* src0, const uint16_t* src1, const uint16_t* src2, const uint16_t* src3, uint16_t* new3, int i,
                                int sPitch) {
    const uint16_t color = src0[i];
    bool oscillates = false;
    if (ColorsDifferent16(color, src1[i], kColorThreshold16)) {
        oscillates = ColorsSimilar16(color, src2[i], kColorThreshold16) ||
                     ColorsSimilar16(color, src3[i], kColorThreshold16);
        if (!oscillates && i >= 2 && i < sPitch - 2) {
            for (int offset = 1; offset <= 2 && !oscillates; offset++) {
                oscillates = ColorsSimilar16(color, src2[i - offset], kColorThreshold16) ||
                             ColorsSimilar16(color, src2[i + offset], kColorThreshold16) ||
                             ColorsSimilar16(color, src3[i - offset], kColorThreshold16) ||
                             ColorsSimilar16(color, src3[i + offset], kColorThreshold16);
            }
        }
    }
    src0[i] = oscillates ? static_cast<uint16_t>(((color & kMask555) >> 1) + ((src1[i] & kMask555) >> 1)) : color;
    new3[i] = color;
}

// i is the pixel index, the bytes are at 3 * i
static inline void SmartPixel24(uint8_t* src0, const uint8_t* src1, const uint8_t* src2, const uint8_t* src3, uint8_t* new3, int i,
                                int sPitch) {
    const int pos = 3 * i;
    const uint8_t r0 = src0[pos], g0 = src0[pos + 1], b0 = src0[pos + 2];
    const uint8_t c[3] = {r0, g0, b0};
    bool oscillates = false;
    if (!ColorsSimilar24(c, src1 + pos, kColorThreshold24)) {
        oscillates = ColorsSimilar24(c, src2 + pos, kColorThreshold24) ||
                     ColorsSimilar24(c, src3 + pos, kColorThreshold24);
        if (!oscillates && i >= 2 && i < sPitch - 2) {
            for (int offset = 1; offset <= 2 && !oscillates; offset++) {
                const int byteOffset = offset * 3;
                oscillates = ColorsSimilar24(c, src2 + pos - byteOffset, kColorThreshold24) ||
                             ColorsSimilar24(c, src2 + pos + byteOffset, kColorThreshold24) ||
                             ColorsSimilar24(c, src3 + pos - byteOffset, kColorThreshold24) ||
                             ColorsSimilar24(c, src3 + pos + byteOffset, kColorThreshold24);
            }
        }
    }
    if (oscillates) {
        src0[pos] = static_cast<uint8_t>(((r0 & 0xFE) >> 1) + ((src1[pos] & 0xFE) >> 1));
        src0[pos + 1] = static_cast<uint8_t>(((g0 & 0xFE) >> 1) + ((src1[pos + 1] & 0xFE) >> 1));
        src0[pos + 2] = static_cast<uint8_t>(((b0 & 0xFE) >> 1) + ((src1[pos + 2] & 0xFE) >> 1));
    }
    new3[pos] = r0;
    new3[pos + 1] = g0;
    new3[pos + 2] = b0;
}

static inline void SmartPixel8(uint8_t* src0, const uint8_t* src1, const uint8_t* src2, const uint8_t* src3, uint8_t* new3, int i,
                               int sPitch) {
    // Convert palette indices to RGB555 for comparison
    const uint16_t c0 = PaletteToRGB555(src0[i]);
    const uint16_t c1 = PaletteToRGB555(src1[i]);
    bool oscillates = false;
    if (ColorsDifferent16(c0, c1, kColorThreshold16)) {
        oscillates = ColorsSimilar16(c0, PaletteToRGB555(src2[i]), kColorThreshold16) ||
                     ColorsSimilar16(c0, PaletteToRGB555(src3[i]), kColorThreshold16);
        if (!oscillates && i >= 2 && i < sPitch - 2) {
            for (int offset = 1; offset <= 2 && !oscillates; offset++) {
                oscillates = ColorsSimilar16(c0, PaletteToRGB555(src2[i - offset]), kColorThreshold16) ||
                             ColorsSimilar16(c0, PaletteToRGB555(src2[i + offset]), kColorThreshold16) ||
                             ColorsSimilar16(c0, PaletteToRGB555(src3[i - offset]), kColorThreshold16) ||
                             ColorsSimilar16(c0, PaletteToRGB555(src3[i + offset]), kColorThreshold16);
            }
        }
    }
    if (oscillates) {
        const uint16_t blended = static_cast<uint16_t>(((c0 & kMask555) >> 1) + ((c1 & kMask555) >> 1));
        src0[i] = RGB555ToPalette(blended);
    } else {
        src0[i] = RGB555ToPalette(c0);
    }
    new3[i] = RGB555ToPalette(c0);
}

// ============================================================================
// Scalar kernels
// ============================================================================

static void SmartIB32_Scalar(uint8_t* srcPtr, uint8_t* frm1Ptr, uint8_t* frm2Ptr, uint8_t* frm3Ptr,
                              uint32_t srcPitch, int width, int starty, int height) {
    (void)width;  // Width is implicit in srcPitch for consistency with SIMD versions
    const int sPitch = srcPitch >> 2;
    uint32_t* src0 = reinterpret_cast<uint32_t*>(srcPtr) + starty * sPitch;
    const uint32_t* src1 = reinterpret_cast<uint32_t*>(frm1Ptr) + starty * sPitch;
    const uint32_t* src2 = reinterpret_cast<uint32_t*>(frm2Ptr) + starty * sPitch;
    uint32_t* src3 = reinterpret_cast<uint32_t*>(frm3Ptr) + starty * sPitch;

    std::vector<uint32_t> new3(sPitch);
    for (int j = 0; j < height; j++) {
        for (int i = 0; i < sPitch; i++)
            SmartPixel32(src0, src1, src2, src3, new3.data(), i, sPitch);
        std::memcpy(src3, new3.data(), new3.size() * sizeof(uint32_t));
        src0 += sPitch; src1 += sPitch; src2 += sPitch; src3 += sPitch;
    }
}

static void SmartIB16_Scalar(uint8_t* srcPtr, uint8_t* frm1Ptr, uint8_t* frm2Ptr, uint8_t* frm3Ptr,
                              uint32_t srcPitch, int width, int starty, int height) {
    (void)width;
    const int sPitch = srcPitch >> 1;
    uint16_t* src0 = reinterpret_cast<uint16_t*>(srcPtr) + starty * sPitch;
    const uint16_t* src1 = reinterpret_cast<uint16_t*>(frm1Ptr) + starty * sPitch;
    const uint16_t* src2 = reinterpret_cast<uint16_t*>(frm2Ptr) + starty * sPitch;
    uint16_t* src3 = reinterpret_cast<uint16_t*>(frm3Ptr) + starty * sPitch;

    std::vector<uint16_t> new3(sPitch);
    for (int j = 0; j < height; j++) {
        for (int i = 0; i < sPitch; i++)
            SmartPixel16(src0, src1, src2, src3, new3.data(), i, sPitch);
        std::memcpy(src3, new3.data(), new3.size() * sizeof(uint16_t));
        src0 += sPitch; src1 += sPitch; src2 += sPitch; src3 += sPitch;
    }
}

static void SmartIB24_Scalar(uint8_t* srcPtr, uint8_t* frm1Ptr, uint8_t* frm2Ptr, uint8_t* frm3Ptr,
                              uint32_t srcPitch, int width, int starty, int height) {
    (void)width;
    const int sPitch = srcPitch / 3;  // pixels per row
    uint8_t* src0 = srcPtr + starty * srcPitch;
    const uint8_t* src1 = frm1Ptr + starty * srcPitch;
    const uint8_t* src2 = frm2Ptr + starty * srcPitch;
    uint8_t* src3 = frm3Ptr + starty * srcPitch;

    std::vector<uint8_t> new3(3 * sPitch);
    for (int j = 0; j < height; j++) {
        for (int i = 0; i < sPitch; i++)
            SmartPixel24(src0, src1, src2, src3, new3.data(), i, sPitch);
        std::memcpy(src3, new3.data(), new3.size() * sizeof(uint8_t));
        src0 += srcPitch; src1 += srcPitch; src2 += srcPitch; src3 += srcPitch;
    }
}

static void SmartIB8_Scalar(uint8_t* srcPtr, uint8_t* frm1Ptr, uint8_t* frm2Ptr, uint8_t* frm3Ptr,
                             uint32_t srcPitch, int width, int starty, int height) {
    (void)width;
    const int sPitch = srcPitch;
    uint8_t* src0 = srcPtr + starty * srcPitch;
    const uint8_t* src1 = frm1Ptr + starty * srcPitch;
    const uint8_t* src2 = frm2Ptr + starty * srcPitch;
    uint8_t* src3 = frm3Ptr + starty * srcPitch;

    std::vector<uint8_t> new3(sPitch);
    for (int j = 0; j < height; j++) {
        for (int i = 0; i < sPitch; i++)
            SmartPixel8(src0, src1, src2, src3, new3.data(), i, sPitch);
        std::memcpy(src3, new3.data(), new3.size() * sizeof(uint8_t));
        src0 += srcPitch; src1 += srcPitch; src2 += srcPitch; src3 += srcPitch;
    }
}

static void MotionBlurIB32_Scalar(uint8_t* srcPtr, uint8_t* histPtr,
                                   uint32_t srcPitch, int width, int starty, int height) {
    (void)width;
    const int sPitch = srcPitch >> 2;
    uint32_t* src0 = reinterpret_cast<uint32_t*>(srcPtr) + starty * sPitch;
    uint32_t* src1 = reinterpret_cast<uint32_t*>(histPtr) + starty * sPitch;
    const uint32_t colorMask = 0xfefefe;

    for (int j = 0; j < height; j++) {
        for (int i = 0; i < sPitch; i++) {
            // Exponential decay: blend current with accumulated history
            const uint32_t blended = ((src0[i] & colorMask) >> 1) + ((src1[i] & colorMask) >> 1);
            src1[i] = blended;  // Save blended result (accumulates decay)
            src0[i] = blended;
        }
        src0 += sPitch; src1 += sPitch;
    }
}

static void MotionBlurIB16_Scalar(uint8_t* srcPtr, uint8_t* histPtr,
                                   uint32_t srcPitch, int width, int starty, int height) {
    (void)width;
    const int sPitch = srcPitch >> 1;
    uint16_t* src0 = reinterpret_cast<uint16_t*>(srcPtr) + starty * sPitch;
    uint16_t* src1 = reinterpret_cast<uint16_t*>(histPtr) + starty * sPitch;

    for (int j = 0; j < height; j++) {
        for (int i = 0; i < sPitch; i++) {
            const uint16_t blended = static_cast<uint16_t>(((src0[i] & kMask555) >> 1) + ((src1[i] & kMask555) >> 1));
            src1[i] = blended;
            src0[i] = blended;
        }
        src0 += sPitch; src1 += sPitch;
    }
}

static void MotionBlurIB24_Scalar(uint8_t* srcPtr, uint8_t* histPtr,
                                   uint32_t srcPitch, int width, int starty, int height) {
    (void)width;
    const int bytes = (srcPitch / 3) * 3;
    uint8_t* src0 = srcPtr + starty * srcPitch;
    uint8_t* src1 = histPtr + starty * srcPitch;

    for (int j = 0; j < height; j++) {
        for (int i = 0; i < bytes; i++) {
            const uint8_t blended = static_cast<uint8_t>(((src0[i] & 0xFE) >> 1) + ((src1[i] & 0xFE) >> 1));
            src1[i] = blended;
            src0[i] = blended;
        }
        src0 += srcPitch; src1 += srcPitch;
    }
}

static void MotionBlurIB8_Scalar(uint8_t* srcPtr, uint8_t* histPtr,
                                  uint32_t srcPitch, int width, int starty, int height) {
    (void)width;
    const int sPitch = srcPitch;
    uint8_t* src0 = srcPtr + starty * srcPitch;
    uint8_t* src1 = histPtr + starty * srcPitch;

    for (int j = 0; j < height; j++) {
        for (int i = 0; i < sPitch; i++) {
            const uint16_t color = PaletteToRGB555(src0[i]);
            const uint16_t color2 = PaletteToRGB555(src1[i]);
            const uint16_t blended = static_cast<uint16_t>(((color & kMask555) >> 1) + ((color2 & kMask555) >> 1));
            const uint8_t pal = RGB555ToPalette(blended);
            src1[i] = pal;
            src0[i] = pal;
        }
        src0 += srcPitch; src1 += srcPitch;
    }
}

#ifdef USE_MMX

// Forward declarations of scalar implementations
static void SmartIB16_Scalar(uint8_t* srcPtr, uint8_t* frm1Ptr, uint8_t* frm2Ptr, uint8_t* frm3Ptr,
                              uint32_t srcPitch, int width, int starty, int height);
static void SmartIB32_Scalar(uint8_t* srcPtr, uint8_t* frm1Ptr, uint8_t* frm2Ptr, uint8_t* frm3Ptr,
                              uint32_t srcPitch, int width, int starty, int height);
static void MotionBlurIB16_Scalar(uint8_t* srcPtr, uint8_t* histPtr,
                                   uint32_t srcPitch, int width, int starty, int height);
static void MotionBlurIB32_Scalar(uint8_t* srcPtr, uint8_t* histPtr,
                                   uint32_t srcPitch, int width, int starty, int height);

// MMX helper: compute absolute difference of packed bytes
// Returns |a - b| for each byte position
static inline __m64 MMX_AbsDiff8(__m64 a, __m64 b) {
    // Use saturating subtraction: max(a-b, b-a) = |a-b|
    return _mm_or_si64(_mm_subs_pu8(a, b), _mm_subs_pu8(b, a));
}

// MMX helper: check if two 32-bit colors are similar (all RGB bytes within threshold)
// Returns mask with all bits set (0xFFFFFFFF) for similar pixels, 0 otherwise
// Processes 2 pixels at a time
static inline __m64 MMX_ColorsSimilar32(__m64 a, __m64 b, __m64 threshold, __m64 alphaMask) {
    __m64 diff = MMX_AbsDiff8(a, b);
    // Subtract threshold - if result is 0 for RGB bytes, colors are similar
    __m64 exceeds = _mm_subs_pu8(diff, threshold);
    // Mask off alpha channel (we only care about RGB)
    exceeds = _mm_and_si64(exceeds, alphaMask);
    // Compare with zero - if all RGB bytes are 0, colors are similar
    __m64 zero = _mm_setzero_si64();
    return _mm_cmpeq_pi32(exceeds, zero);
}

// MMX helper: check if two 32-bit colors are different (any RGB byte exceeds threshold)
static inline __m64 MMX_ColorsDifferent32(__m64 a, __m64 b, __m64 threshold, __m64 alphaMask) {
    __m64 similar = MMX_ColorsSimilar32(a, b, threshold, alphaMask);
    __m64 ones = _mm_set1_pi32(-1);
    return _mm_xor_si64(similar, ones);  // NOT similar = different
}

static void SmartIB32_MMX(uint8_t* srcPtr, uint8_t* frm1Ptr, uint8_t* frm2Ptr, uint8_t* frm3Ptr,
                          uint32_t srcPitch, int width, int starty, int height) {
    (void)width;
    uint32_t* src = reinterpret_cast<uint32_t*>(srcPtr) + starty * srcPitch / 4;
    uint32_t* frm1 = reinterpret_cast<uint32_t*>(frm1Ptr) + starty * srcPitch / 4;
    uint32_t* frm2 = reinterpret_cast<uint32_t*>(frm2Ptr) + starty * srcPitch / 4;
    uint32_t* frm3 = reinterpret_cast<uint32_t*>(frm3Ptr) + starty * srcPitch / 4;

    int sPitch = srcPitch >> 2;

    // MMX constants
    const __m64 colorMask = _mm_set1_pi32(0x00FEFEFE);  // Mask for divide-by-2
    const __m64 threshold = _mm_set1_pi8(static_cast<char>(kColorThreshold));
    const __m64 alphaMask = _mm_set1_pi32(0x00FFFFFF);  // Mask off alpha for comparison

    for (int y = 0; y < height; y++) {
        int x = 0;

        // Process 2 pixels at a time with MMX
        for (; x + 2 <= sPitch; x += 2) {
            __m64 src0 = *reinterpret_cast<const __m64*>(&src[x]);
            __m64 src1 = *reinterpret_cast<const __m64*>(&frm1[x]);
            __m64 src2 = *reinterpret_cast<const __m64*>(&frm2[x]);
            __m64 src3 = *reinterpret_cast<const __m64*>(&frm3[x]);

            // Save current to frm3 (oldest buffer gets newest frame)
            *reinterpret_cast<__m64*>(&frm3[x]) = src0;

            // First check if pixel changed from previous frame
            __m64 changed = MMX_ColorsDifferent32(src0, src1, threshold, alphaMask);

            // Check oscillation: current similar to 2 or 3 frames ago (A->B->A pattern)
            __m64 similar_0_2 = MMX_ColorsSimilar32(src0, src2, threshold, alphaMask);
            __m64 similar_0_3 = MMX_ColorsSimilar32(src0, src3, threshold, alphaMask);
            __m64 oscillates = _mm_or_si64(similar_0_2, similar_0_3);

            // Check neighbors for scrolling content (±1, ±2)
            // ONLY if pixel changed - prevents static edge artifacts
            if (x >= 2 && x + 4 <= sPitch) {
                __m64 neighbor_osc = _mm_setzero_si64();

                // ±1 pixel offsets
                neighbor_osc = _mm_or_si64(neighbor_osc, _mm_or_si64(
                    MMX_ColorsSimilar32(src0, *reinterpret_cast<const __m64*>(&frm2[x-1]), threshold, alphaMask),
                    MMX_ColorsSimilar32(src0, *reinterpret_cast<const __m64*>(&frm2[x+1]), threshold, alphaMask)));
                neighbor_osc = _mm_or_si64(neighbor_osc, _mm_or_si64(
                    MMX_ColorsSimilar32(src0, *reinterpret_cast<const __m64*>(&frm3[x-1]), threshold, alphaMask),
                    MMX_ColorsSimilar32(src0, *reinterpret_cast<const __m64*>(&frm3[x+1]), threshold, alphaMask)));

                // ±2 pixel offsets
                neighbor_osc = _mm_or_si64(neighbor_osc, _mm_or_si64(
                    MMX_ColorsSimilar32(src0, *reinterpret_cast<const __m64*>(&frm2[x-2]), threshold, alphaMask),
                    MMX_ColorsSimilar32(src0, *reinterpret_cast<const __m64*>(&frm2[x+2]), threshold, alphaMask)));
                neighbor_osc = _mm_or_si64(neighbor_osc, _mm_or_si64(
                    MMX_ColorsSimilar32(src0, *reinterpret_cast<const __m64*>(&frm3[x-2]), threshold, alphaMask),
                    MMX_ColorsSimilar32(src0, *reinterpret_cast<const __m64*>(&frm3[x+2]), threshold, alphaMask)));

                // Neighbor oscillation only counts if pixel changed
                oscillates = _mm_or_si64(oscillates, _mm_and_si64(changed, neighbor_osc));
            }

            // blend = changed && oscillates
            __m64 blend_mask = _mm_and_si64(changed, oscillates);

            // Calculate blended value: (src0 + src1) / 2
            __m64 src0_masked = _mm_and_si64(src0, colorMask);
            __m64 src1_masked = _mm_and_si64(src1, colorMask);
            __m64 blended = _mm_add_pi32(_mm_srli_pi32(src0_masked, 1),
                                          _mm_srli_pi32(src1_masked, 1));

            // Select: blend_mask ? blended : src0
            __m64 result = _mm_or_si64(_mm_and_si64(blend_mask, blended),
                                        _mm_andnot_si64(blend_mask, src0));

            *reinterpret_cast<__m64*>(&src[x]) = result;
        }

        // Scalar tail
        for (; x < sPitch; x++) {
            uint32_t color = src[x];

            // First check if pixel changed from previous frame
            bool changed = ColorsDifferent32(color, frm1[x], kColorThreshold);

            bool oscillates = false;
            if (changed) {
                // Check oscillation at current position (A->B->A pattern)
                oscillates = ColorsSimilar32(color, frm2[x], kColorThreshold) ||
                             ColorsSimilar32(color, frm3[x], kColorThreshold);

                // Check neighbors for scrolling content (±1, ±2)
                if (!oscillates && x >= 2 && x < sPitch - 2) {
                    for (int offset = 1; offset <= 2 && !oscillates; offset++) {
                        oscillates = ColorsSimilar32(color, frm2[x-offset], kColorThreshold) ||
                                     ColorsSimilar32(color, frm2[x+offset], kColorThreshold) ||
                                     ColorsSimilar32(color, frm3[x-offset], kColorThreshold) ||
                                     ColorsSimilar32(color, frm3[x+offset], kColorThreshold);
                    }
                }
            }

            frm3[x] = color;
            if (oscillates) {
                src[x] = ((color & 0xFEFEFE) >> 1) + ((frm1[x] & 0xFEFEFE) >> 1);
            }
        }

        src += sPitch;
        frm1 += sPitch;
        frm2 += sPitch;
        frm3 += sPitch;
    }

    _mm_empty();  // Clear MMX state
}

// 16-bit MMX implementation delegates to scalar (threshold comparison for packed RGB565/555 is complex)
static void SmartIB_MMX(uint8_t* srcPtr, uint8_t* frm1Ptr, uint8_t* frm2Ptr, uint8_t* frm3Ptr,
                        uint32_t srcPitch, int width, int starty, int height) {
    SmartIB16_Scalar(srcPtr, frm1Ptr, frm2Ptr, frm3Ptr, srcPitch, width, starty, height);
}

static void MotionBlurIB_MMX(uint8_t* srcPtr, uint8_t* histPtr,
                              uint32_t srcPitch, int width, int starty, int height) {
    (void)width;
    // Use constant mask for RGB555
    const uint16_t colorMask = 0x7BDE;
    const __m64 mask = _mm_set1_pi16(colorMask);

    uint16_t* src = reinterpret_cast<uint16_t*>(srcPtr) + starty * (srcPitch / 2);
    uint16_t* hist = reinterpret_cast<uint16_t*>(histPtr) + starty * (srcPitch / 2);

    int sPitch = srcPitch >> 1;

    for (int y = 0; y < height; y++) {
        int x = 0;
        // Process 4 pixels at a time with MMX (64-bit = 4 x 16-bit)
        for (; x + 4 <= sPitch; x += 4) {
            __m64 curr = *reinterpret_cast<const __m64*>(&src[x]);
            __m64 prev = *reinterpret_cast<const __m64*>(&hist[x]);

            // Exponential decay: blend = (current + prev) / 2
            __m64 curr_masked = _mm_and_si64(curr, mask);
            __m64 prev_masked = _mm_and_si64(prev, mask);
            __m64 curr_half = _mm_srli_pi16(curr_masked, 1);
            __m64 prev_half = _mm_srli_pi16(prev_masked, 1);
            __m64 blended = _mm_add_pi16(curr_half, prev_half);

            // Save blended result to history (creates exponential decay)
            *reinterpret_cast<__m64*>(&hist[x]) = blended;
            *reinterpret_cast<__m64*>(&src[x]) = blended;
        }

        // Scalar tail
        for (; x < sPitch; x++) {
            uint16_t color = src[x];
            uint16_t hist_color = hist[x];
            uint16_t blended = ((color & colorMask) >> 1) + ((hist_color & colorMask) >> 1);
            hist[x] = blended;
            src[x] = blended;
        }

        src += sPitch;
        hist += sPitch;
    }

    _mm_empty();  // Clear MMX state
}

static void MotionBlurIB32_MMX(uint8_t* srcPtr, uint8_t* histPtr,
                                uint32_t srcPitch, int width, int starty, int height) {
    (void)width;
    const __m64 colorMask = _mm_set1_pi32(0x00FEFEFE);

    uint32_t* src = reinterpret_cast<uint32_t*>(srcPtr) + starty * (srcPitch / 4);
    uint32_t* hist = reinterpret_cast<uint32_t*>(histPtr) + starty * (srcPitch / 4);

    int sPitch = srcPitch >> 2;

    for (int y = 0; y < height; y++) {
        int x = 0;
        // Process 2 pixels at a time with MMX (64-bit = 2 x 32-bit)
        for (; x + 2 <= sPitch; x += 2) {
            __m64 curr = *reinterpret_cast<const __m64*>(&src[x]);
            __m64 prev = *reinterpret_cast<const __m64*>(&hist[x]);

            // Exponential decay: blend = (current + prev) / 2
            __m64 curr_masked = _mm_and_si64(curr, colorMask);
            __m64 prev_masked = _mm_and_si64(prev, colorMask);
            __m64 curr_half = _mm_srli_pi32(curr_masked, 1);
            __m64 prev_half = _mm_srli_pi32(prev_masked, 1);
            __m64 blended = _mm_add_pi32(curr_half, prev_half);

            // Save blended result to history (creates exponential decay)
            *reinterpret_cast<__m64*>(&hist[x]) = blended;
            *reinterpret_cast<__m64*>(&src[x]) = blended;
        }

        // Scalar tail
        for (; x < sPitch; x++) {
            uint32_t color = src[x];
            uint32_t hist_color = hist[x];
            uint32_t blended = ((color & 0xFEFEFE) >> 1) + ((hist_color & 0xFEFEFE) >> 1);
            hist[x] = blended;
            src[x] = blended;
        }

        src += sPitch;
        hist += sPitch;
    }

    _mm_empty();  // Clear MMX state
}

#endif  // USE_MMX


// ============================================================================
// Vector kernels (SSE2 / NEON, see components/filters/simd128.h)
// ============================================================================

namespace {

using namespace vbam_simd;

// "similar" per u32 lane: every colour byte within threshold (the upper byte ignored)
VBAM_SIMD_INLINE V Similar32(V a, V b, V thr) {
    const V exceeds = and_(subs_u8(absdiff_u8(a, b), thr), set1_u32(0x00FFFFFF));
    return cmpeq_u32(exceeds, zero());
}

// "similar" per u16 lane of RGB555 pixels: every 5-bit channel within kColorThreshold16
VBAM_SIMD_INLINE V Similar16(V a, V b) {
    const V m5 = set1_u16(0x1f);
    const V thr = set1_u16(kColorThreshold16);
    const V dr = absdiff_u16(and_(srl_u16<10>(a), m5), and_(srl_u16<10>(b), m5));
    const V dg = absdiff_u16(and_(srl_u16<5>(a), m5), and_(srl_u16<5>(b), m5));
    const V db = absdiff_u16(and_(a, m5), and_(b, m5));
    return not_(or_(or_(cmpgt_s16(dr, thr), cmpgt_s16(dg, thr)), cmpgt_s16(db, thr)));
}

// 8 palette indices (u16 lanes) -> RGB555
VBAM_SIMD_INLINE V PalTo555(V p) {
    const V v = or_(or_(sll_u16<7>(and_(p, set1_u16(0xe0))), sll_u16<5>(and_(p, set1_u16(0x1c)))),
                    sll_u16<3>(and_(p, set1_u16(0x03))));
    return vselect(cmpeq_u16(p, set1_u16(0xff)), set1_u16(0x7fff), v);
}

// RGB555 (u16 lanes) -> palette index
VBAM_SIMD_INLINE V Pal555ToPal(V c) {
    return or_(or_(and_(srl_u16<7>(c), set1_u16(0xe0)), and_(srl_u16<5>(c), set1_u16(0x1c))),
               and_(srl_u16<3>(c), set1_u16(0x03)));
}

void SmartIB32_Vector(uint8_t* srcPtr, uint8_t* frm1Ptr, uint8_t* frm2Ptr, uint8_t* frm3Ptr, uint32_t srcPitch,
                      int width, int starty, int height) {
    (void)width;
    const int sPitch = srcPitch >> 2;
    uint32_t* src0 = reinterpret_cast<uint32_t*>(srcPtr) + starty * sPitch;
    const uint32_t* src1 = reinterpret_cast<uint32_t*>(frm1Ptr) + starty * sPitch;
    const uint32_t* src2 = reinterpret_cast<uint32_t*>(frm2Ptr) + starty * sPitch;
    uint32_t* src3 = reinterpret_cast<uint32_t*>(frm3Ptr) + starty * sPitch;
    const V thr = set1_u8(kColorThreshold);
    const V mask = set1_u32(0x00FEFEFE);
    std::vector<uint32_t> new3(sPitch);

    for (int j = 0; j < height; j++) {
        int x = 0;
        for (; x + 4 <= sPitch; x += 4) {
            if (x < 2 || x + 6 > sPitch) {
                // the neighbour window of some lane leaves the row: reference logic
                for (int i = x; i < x + 4; i++)
                    SmartPixel32(src0, src1, src2, src3, new3.data(), i, sPitch);
                continue;
            }
            const V c = load(src0 + x);
            const V p1 = load(src1 + x);
            const V changed = not_(Similar32(c, p1, thr));
            V osc = or_(Similar32(c, load(src2 + x), thr), Similar32(c, load(src3 + x), thr));
            for (int offset = 1; offset <= 2; offset++) {
                osc = or_(osc, or_(Similar32(c, load(src2 + x - offset), thr), Similar32(c, load(src2 + x + offset), thr)));
                osc = or_(osc, or_(Similar32(c, load(src3 + x - offset), thr), Similar32(c, load(src3 + x + offset), thr)));
            }
            const V blend = and_(changed, osc);
            store(new3.data() + x, c);
            store(src0 + x, vselect(blend, halfsum_masked_u32(c, p1, mask), c));
        }
        for (; x < sPitch; x++)
            SmartPixel32(src0, src1, src2, src3, new3.data(), x, sPitch);
        std::memcpy(src3, new3.data(), sPitch * sizeof(uint32_t));
        src0 += sPitch; src1 += sPitch; src2 += sPitch; src3 += sPitch;
    }
}

void SmartIB16_Vector(uint8_t* srcPtr, uint8_t* frm1Ptr, uint8_t* frm2Ptr, uint8_t* frm3Ptr, uint32_t srcPitch,
                      int width, int starty, int height) {
    (void)width;
    const int sPitch = srcPitch >> 1;
    uint16_t* src0 = reinterpret_cast<uint16_t*>(srcPtr) + starty * sPitch;
    const uint16_t* src1 = reinterpret_cast<uint16_t*>(frm1Ptr) + starty * sPitch;
    const uint16_t* src2 = reinterpret_cast<uint16_t*>(frm2Ptr) + starty * sPitch;
    uint16_t* src3 = reinterpret_cast<uint16_t*>(frm3Ptr) + starty * sPitch;
    const V mask = set1_u16(kMask555);
    std::vector<uint16_t> new3(sPitch);

    for (int j = 0; j < height; j++) {
        int x = 0;
        for (; x + 8 <= sPitch; x += 8) {
            if (x < 2 || x + 10 > sPitch) {
                for (int i = x; i < x + 8; i++)
                    SmartPixel16(src0, src1, src2, src3, new3.data(), i, sPitch);
                continue;
            }
            const V c = load(src0 + x);
            const V p1 = load(src1 + x);
            const V changed = not_(Similar16(c, p1));
            V osc = or_(Similar16(c, load(src2 + x)), Similar16(c, load(src3 + x)));
            for (int offset = 1; offset <= 2; offset++) {
                osc = or_(osc, or_(Similar16(c, load(src2 + x - offset)), Similar16(c, load(src2 + x + offset))));
                osc = or_(osc, or_(Similar16(c, load(src3 + x - offset)), Similar16(c, load(src3 + x + offset))));
            }
            const V blend = and_(changed, osc);
            store(new3.data() + x, c);
            store(src0 + x, vselect(blend, halfsum_masked_u16(c, p1, mask), c));
        }
        for (; x < sPitch; x++)
            SmartPixel16(src0, src1, src2, src3, new3.data(), x, sPitch);
        std::memcpy(src3, new3.data(), sPitch * sizeof(uint16_t));
        src0 += sPitch; src1 += sPitch; src2 += sPitch; src3 += sPitch;
    }
}

// 16 palette pixels as two vectors of 8 RGB555 pixels in u16 lanes
struct Pal16 {
    V lo, hi;
};
VBAM_SIMD_INLINE Pal16 LoadPal(const uint8_t* p) {
    const V b = load(p);
    const V z = zero();
    return {PalTo555(unpacklo_u8(b, z)), PalTo555(unpackhi_u8(b, z))};
}

void SmartIB8_Vector(uint8_t* srcPtr, uint8_t* frm1Ptr, uint8_t* frm2Ptr, uint8_t* frm3Ptr, uint32_t srcPitch,
                     int width, int starty, int height) {
    (void)width;
    const int sPitch = srcPitch;
    uint8_t* src0 = srcPtr + starty * srcPitch;
    const uint8_t* src1 = frm1Ptr + starty * srcPitch;
    const uint8_t* src2 = frm2Ptr + starty * srcPitch;
    uint8_t* src3 = frm3Ptr + starty * srcPitch;
    const V mask = set1_u16(kMask555);
    std::vector<uint8_t> new3(sPitch);

    for (int j = 0; j < height; j++) {
        int x = 0;
        for (; x + 16 <= sPitch; x += 16) {
            if (x < 2 || x + 18 > sPitch) {
                for (int i = x; i < x + 16; i++)
                    SmartPixel8(src0, src1, src2, src3, new3.data(), i, sPitch);
                continue;
            }
            const Pal16 c = LoadPal(src0 + x);
            const Pal16 p1 = LoadPal(src1 + x);
            V changedLo = not_(Similar16(c.lo, p1.lo)), changedHi = not_(Similar16(c.hi, p1.hi));
            V oscLo = zero(), oscHi = zero();
            for (int offset = -2; offset <= 2; offset++) {
                const Pal16 n2 = LoadPal(src2 + x + offset), n3 = LoadPal(src3 + x + offset);
                oscLo = or_(oscLo, or_(Similar16(c.lo, n2.lo), Similar16(c.lo, n3.lo)));
                oscHi = or_(oscHi, or_(Similar16(c.hi, n2.hi), Similar16(c.hi, n3.hi)));
            }
            const V blendLo = and_(changedLo, oscLo), blendHi = and_(changedHi, oscHi);
            const V outLo = Pal555ToPal(vselect(blendLo, halfsum_masked_u16(c.lo, p1.lo, mask), c.lo));
            const V outHi = Pal555ToPal(vselect(blendHi, halfsum_masked_u16(c.hi, p1.hi, mask), c.hi));
            store(new3.data() + x, packus_s16_u8(Pal555ToPal(c.lo), Pal555ToPal(c.hi)));
            store(src0 + x, packus_s16_u8(outLo, outHi));
        }
        for (; x < sPitch; x++)
            SmartPixel8(src0, src1, src2, src3, new3.data(), x, sPitch);
        std::memcpy(src3, new3.data(), sPitch);
        src0 += srcPitch; src1 += srcPitch; src2 += srcPitch; src3 += srcPitch;
    }
}

// 16 pixels of 24-bit RGB are 48 bytes, three vectors. The byte-wise work is
// vector; the per-pixel "all three bytes similar" reduction goes through a
// 48-bit mask.
struct Bytes48 {
    V v[3];
};
VBAM_SIMD_INLINE Bytes48 Load48(const uint8_t* p) { return {{load(p), load(p + 16), load(p + 32)}}; }

// bit i set where pixel i (of 16) has a byte differing from c by more than the threshold
VBAM_SIMD_INLINE uint32_t Different24(const Bytes48& c, const Bytes48& o, V thr) {
    uint64_t bits = 0;
    for (int k = 0; k < 3; k++)
        bits |= static_cast<uint64_t>(movemask_u8(not_(cmpeq_u8(subs_u8(absdiff_u8(c.v[k], o.v[k]), thr), zero()))))
                << (16 * k);
    uint32_t px = 0;
    for (int i = 0; i < 16; i++)
        if ((bits >> (3 * i)) & 7)
            px |= 1u << i;
    return px;
}

void SmartIB24_Vector(uint8_t* srcPtr, uint8_t* frm1Ptr, uint8_t* frm2Ptr, uint8_t* frm3Ptr, uint32_t srcPitch,
                      int width, int starty, int height) {
    (void)width;
    const int sPitch = srcPitch / 3;
    uint8_t* src0 = srcPtr + starty * srcPitch;
    const uint8_t* src1 = frm1Ptr + starty * srcPitch;
    const uint8_t* src2 = frm2Ptr + starty * srcPitch;
    uint8_t* src3 = frm3Ptr + starty * srcPitch;
    const V thr = set1_u8(kColorThreshold24);
    const V fe = set1_u8(0xfe);
    std::vector<uint8_t> new3(3 * sPitch);

    for (int j = 0; j < height; j++) {
        int x = 0;
        for (; x + 16 <= sPitch; x += 16) {
            if (x < 2 || x + 18 > sPitch) {
                for (int i = x; i < x + 16; i++)
                    SmartPixel24(src0, src1, src2, src3, new3.data(), i, sPitch);
                continue;
            }
            const Bytes48 c = Load48(src0 + 3 * x);
            const Bytes48 p1 = Load48(src1 + 3 * x);
            const uint32_t changed = Different24(c, p1, thr);
            uint32_t similar = 0;
            for (int offset = -2; offset <= 2; offset++) {
                similar |= ~Different24(c, Load48(src2 + 3 * (x + offset)), thr);
                similar |= ~Different24(c, Load48(src3 + 3 * (x + offset)), thr);
            }
            const uint32_t blend = changed & similar & 0xffff;
            uint8_t mb[48];
            for (int i = 0; i < 16; i++)
                mb[3 * i] = mb[3 * i + 1] = mb[3 * i + 2] = (blend >> i) & 1 ? 0xff : 0;
            for (int k = 0; k < 3; k++) {
                const V m = load(mb + 16 * k);
                const V blended = add_u8(srl_u16<1>(and_(c.v[k], fe)), srl_u16<1>(and_(p1.v[k], fe)));
                store(new3.data() + 3 * x + 16 * k, c.v[k]);
                store(src0 + 3 * x + 16 * k, vselect(m, blended, c.v[k]));
            }
        }
        for (; x < sPitch; x++)
            SmartPixel24(src0, src1, src2, src3, new3.data(), x, sPitch);
        std::memcpy(src3, new3.data(), 3 * sPitch);
        src0 += srcPitch; src1 += srcPitch; src2 += srcPitch; src3 += srcPitch;
    }
}

void MotionBlurIB32_Vector(uint8_t* srcPtr, uint8_t* histPtr, uint32_t srcPitch, int width, int starty, int height) {
    (void)width;
    const int sPitch = srcPitch >> 2;
    uint32_t* src = reinterpret_cast<uint32_t*>(srcPtr) + starty * sPitch;
    uint32_t* hist = reinterpret_cast<uint32_t*>(histPtr) + starty * sPitch;
    const V mask = set1_u32(0x00FEFEFE);

    for (int y = 0; y < height; y++) {
        int x = 0;
        for (; x + 4 <= sPitch; x += 4) {
            const V blended = halfsum_masked_u32(load(src + x), load(hist + x), mask);
            store(hist + x, blended);
            store(src + x, blended);
        }
        for (; x < sPitch; x++) {
            const uint32_t blended = ((src[x] & 0xFEFEFE) >> 1) + ((hist[x] & 0xFEFEFE) >> 1);
            hist[x] = blended;
            src[x] = blended;
        }
        src += sPitch; hist += sPitch;
    }
}

void MotionBlurIB16_Vector(uint8_t* srcPtr, uint8_t* histPtr, uint32_t srcPitch, int width, int starty, int height) {
    (void)width;
    const int sPitch = srcPitch >> 1;
    uint16_t* src = reinterpret_cast<uint16_t*>(srcPtr) + starty * sPitch;
    uint16_t* hist = reinterpret_cast<uint16_t*>(histPtr) + starty * sPitch;
    const V mask = set1_u16(kMask555);

    for (int y = 0; y < height; y++) {
        int x = 0;
        for (; x + 8 <= sPitch; x += 8) {
            const V blended = halfsum_masked_u16(load(src + x), load(hist + x), mask);
            store(hist + x, blended);
            store(src + x, blended);
        }
        for (; x < sPitch; x++) {
            const uint16_t blended = static_cast<uint16_t>(((src[x] & kMask555) >> 1) + ((hist[x] & kMask555) >> 1));
            hist[x] = blended;
            src[x] = blended;
        }
        src += sPitch; hist += sPitch;
    }
}

void MotionBlurIB24_Vector(uint8_t* srcPtr, uint8_t* histPtr, uint32_t srcPitch, int width, int starty, int height) {
    (void)width;
    const int bytes = (srcPitch / 3) * 3;
    uint8_t* src = srcPtr + starty * srcPitch;
    uint8_t* hist = histPtr + starty * srcPitch;
    const V fe = set1_u8(0xfe);

    for (int y = 0; y < height; y++) {
        int x = 0;
        for (; x + 16 <= bytes; x += 16) {
            const V blended = add_u8(srl_u16<1>(and_(load(src + x), fe)), srl_u16<1>(and_(load(hist + x), fe)));
            store(hist + x, blended);
            store(src + x, blended);
        }
        for (; x < bytes; x++) {
            const uint8_t blended = static_cast<uint8_t>(((src[x] & 0xFE) >> 1) + ((hist[x] & 0xFE) >> 1));
            hist[x] = blended;
            src[x] = blended;
        }
        src += srcPitch; hist += srcPitch;
    }
}

void MotionBlurIB8_Vector(uint8_t* srcPtr, uint8_t* histPtr, uint32_t srcPitch, int width, int starty, int height) {
    (void)width;
    const int sPitch = srcPitch;
    uint8_t* src = srcPtr + starty * srcPitch;
    uint8_t* hist = histPtr + starty * srcPitch;
    const V mask = set1_u16(kMask555);

    for (int y = 0; y < height; y++) {
        int x = 0;
        for (; x + 16 <= sPitch; x += 16) {
            const Pal16 c = LoadPal(src + x), h = LoadPal(hist + x);
            const V lo = Pal555ToPal(halfsum_masked_u16(c.lo, h.lo, mask));
            const V hi = Pal555ToPal(halfsum_masked_u16(c.hi, h.hi, mask));
            const V out = packus_s16_u8(lo, hi);
            store(hist + x, out);
            store(src + x, out);
        }
        for (; x < sPitch; x++) {
            const uint16_t color = PaletteToRGB555(src[x]);
            const uint16_t color2 = PaletteToRGB555(hist[x]);
            const uint8_t pal = RGB555ToPalette(static_cast<uint16_t>(((color & kMask555) >> 1) + ((color2 & kMask555) >> 1)));
            hist[x] = pal;
            src[x] = pal;
        }
        src += srcPitch; hist += srcPitch;
    }
}

}  // namespace

// ============================================================================
// InterframeManager Region Methods
// ============================================================================

void InterframeManager::ApplySmartIBRegion(uint8_t* srcPtr, uint32_t srcPitch,
                                            int width, int starty, int height,
                                            int colorDepth, int regionId) {
    if (!initialized_ || regionId < 0 || regionId >= static_cast<int>(regions_.size())) {
        return;
    }

    // Validate that the pitch matches what we were initialized with to prevent buffer overflow
    // This can happen if color depth changes and InterframeManager hasn't been reinitialized
    if (static_cast<int>(srcPitch) != pitch_) {
        return;
    }

    RegionState& region = regions_[regionId];
    uint8_t* hist1 = region.frame1_.get();
    uint8_t* hist2 = region.frame2_.get();
    uint8_t* hist3 = region.frame3_.get();

    const bool vec = vbam_simd::Enabled();
    switch (colorDepth) {
        case 32:
            if (vec)
                SmartIB32_Vector(srcPtr, hist1, hist2, hist3, srcPitch, width, starty, height);
            else
#ifdef USE_MMX
                SmartIB32_MMX(srcPtr, hist1, hist2, hist3, srcPitch, width, starty, height);
#else
                SmartIB32_Scalar(srcPtr, hist1, hist2, hist3, srcPitch, width, starty, height);
#endif
            break;

        case 16:
            if (vec)
                SmartIB16_Vector(srcPtr, hist1, hist2, hist3, srcPitch, width, starty, height);
            else
#ifdef USE_MMX
                SmartIB_MMX(srcPtr, hist1, hist2, hist3, srcPitch, width, starty, height);
#else
                SmartIB16_Scalar(srcPtr, hist1, hist2, hist3, srcPitch, width, starty, height);
#endif
            break;

        case 24:
            if (vec)
                SmartIB24_Vector(srcPtr, hist1, hist2, hist3, srcPitch, width, starty, height);
            else
                SmartIB24_Scalar(srcPtr, hist1, hist2, hist3, srcPitch, width, starty, height);
            break;

        case 8:
            if (vec)
                SmartIB8_Vector(srcPtr, hist1, hist2, hist3, srcPitch, width, starty, height);
            else
                SmartIB8_Scalar(srcPtr, hist1, hist2, hist3, srcPitch, width, starty, height);
            break;
    }

    // Rotate buffers for this region
    region.frame1_.swap(region.frame3_);
    region.frame3_.swap(region.frame2_);
}

void InterframeManager::ApplyMotionBlurRegion(uint8_t* srcPtr, uint32_t srcPitch,
                                               int width, int starty, int height,
                                               int colorDepth, int regionId) {
    if (!initialized_ || regionId < 0 || regionId >= static_cast<int>(regions_.size())) {
        return;
    }

    // Validate that the pitch matches what we were initialized with to prevent buffer overflow
    // This can happen if color depth changes and InterframeManager hasn't been reinitialized
    if (static_cast<int>(srcPitch) != pitch_) {
        return;
    }

    RegionState& region = regions_[regionId];
    uint8_t* hist = region.frame1_.get();

    const bool vec = vbam_simd::Enabled();
    switch (colorDepth) {
        case 32:
            if (vec)
                MotionBlurIB32_Vector(srcPtr, hist, srcPitch, width, starty, height);
            else
#ifdef USE_MMX
                MotionBlurIB32_MMX(srcPtr, hist, srcPitch, width, starty, height);
#else
                MotionBlurIB32_Scalar(srcPtr, hist, srcPitch, width, starty, height);
#endif
            break;

        case 16:
            if (vec)
                MotionBlurIB16_Vector(srcPtr, hist, srcPitch, width, starty, height);
            else
#ifdef USE_MMX
                MotionBlurIB_MMX(srcPtr, hist, srcPitch, width, starty, height);
#else
                MotionBlurIB16_Scalar(srcPtr, hist, srcPitch, width, starty, height);
#endif
            break;

        case 24:
            if (vec)
                MotionBlurIB24_Vector(srcPtr, hist, srcPitch, width, starty, height);
            else
                MotionBlurIB24_Scalar(srcPtr, hist, srcPitch, width, starty, height);
            break;

        case 8:
            if (vec)
                MotionBlurIB8_Vector(srcPtr, hist, srcPitch, width, starty, height);
            else
                MotionBlurIB8_Scalar(srcPtr, hist, srcPitch, width, starty, height);
            break;
    }
}

// ============================================================================
// Legacy API (for SDL/libretro ports - single-threaded)
// ============================================================================

static uint8_t* frm1 = nullptr;
static uint8_t* frm2 = nullptr;
static uint8_t* frm3 = nullptr;

void InterframeFilterInit() {
    frm1 = static_cast<uint8_t*>(calloc(322 * 242, 4));
    frm2 = static_cast<uint8_t*>(calloc(322 * 242, 4));
    frm3 = static_cast<uint8_t*>(calloc(322 * 242, 4));
}

void InterframeCleanup() {
    if (frm1) free(frm1);
    if (frm2 && (frm1 != frm2)) free(frm2);
    if (frm3 && (frm1 != frm3) && (frm2 != frm3)) free(frm3);
    frm1 = frm2 = frm3 = nullptr;

    // Also cleanup the manager
    InterframeManager::Instance().Cleanup();
}

void InterframeClear() {
    // Clear legacy buffers (used by SDL/libretro ports)
    if (frm1) {
        memset(frm1, 0, 322 * 242 * 4);
    }
    if (frm2 && frm2 != frm1) {
        memset(frm2, 0, 322 * 242 * 4);
    }
    if (frm3 && frm3 != frm1 && frm3 != frm2) {
        memset(frm3, 0, 322 * 242 * 4);
    }

    // Clear manager buffers
    InterframeManager::Instance().Clear();
}

// Legacy SmartIB functions
void SmartIB(uint8_t* srcPtr, uint32_t srcPitch, int width, int starty, int height) {
    if (frm1 == nullptr) InterframeFilterInit();
    SmartIB16_Scalar(srcPtr, frm1, frm2, frm3, srcPitch, width, starty, height);
    uint8_t* temp = frm1;
    frm1 = frm3;
    frm3 = frm2;
    frm2 = temp;
}

void SmartIB(uint8_t* srcPtr, uint32_t srcPitch, int width, int height) {
    SmartIB(srcPtr, srcPitch, width, 0, height);
}

void SmartIB8(uint8_t* srcPtr, uint32_t srcPitch, int width, int starty, int height) {
    if (frm1 == nullptr) InterframeFilterInit();
    SmartIB8_Scalar(srcPtr, frm1, frm2, frm3, srcPitch, width, starty, height);
    uint8_t* temp = frm1;
    frm1 = frm3;
    frm3 = frm2;
    frm2 = temp;
}

void SmartIB8(uint8_t* srcPtr, uint32_t srcPitch, int width, int height) {
    SmartIB8(srcPtr, srcPitch, width, 0, height);
}

void SmartIB24(uint8_t* srcPtr, uint32_t srcPitch, int width, int starty, int height) {
    if (frm1 == nullptr) InterframeFilterInit();
    SmartIB24_Scalar(srcPtr, frm1, frm2, frm3, srcPitch, width, starty, height);
    uint8_t* temp = frm1;
    frm1 = frm3;
    frm3 = frm2;
    frm2 = temp;
}

void SmartIB24(uint8_t* srcPtr, uint32_t srcPitch, int width, int height) {
    SmartIB24(srcPtr, srcPitch, width, 0, height);
}

void SmartIB32(uint8_t* srcPtr, uint32_t srcPitch, int width, int starty, int height) {
    if (frm1 == nullptr) InterframeFilterInit();
    SmartIB32_Scalar(srcPtr, frm1, frm2, frm3, srcPitch, width, starty, height);
    uint8_t* temp = frm1;
    frm1 = frm3;
    frm3 = frm2;
    frm2 = temp;
}

void SmartIB32(uint8_t* srcPtr, uint32_t srcPitch, int width, int height) {
    SmartIB32(srcPtr, srcPitch, width, 0, height);
}

// Legacy MotionBlurIB functions
void MotionBlurIB(uint8_t* srcPtr, uint32_t srcPitch, int width, int starty, int height) {
    if (frm1 == nullptr) InterframeFilterInit();
    MotionBlurIB16_Scalar(srcPtr, frm1, srcPitch, width, starty, height);
}

void MotionBlurIB(uint8_t* srcPtr, uint32_t srcPitch, int width, int height) {
    MotionBlurIB(srcPtr, srcPitch, width, 0, height);
}

void MotionBlurIB8(uint8_t* srcPtr, uint32_t srcPitch, int width, int starty, int height) {
    if (frm1 == nullptr) InterframeFilterInit();
    MotionBlurIB8_Scalar(srcPtr, frm1, srcPitch, width, starty, height);
}

void MotionBlurIB8(uint8_t* srcPtr, uint32_t srcPitch, int width, int height) {
    MotionBlurIB8(srcPtr, srcPitch, width, 0, height);
}

void MotionBlurIB24(uint8_t* srcPtr, uint32_t srcPitch, int width, int starty, int height) {
    if (frm1 == nullptr) InterframeFilterInit();
    MotionBlurIB24_Scalar(srcPtr, frm1, srcPitch, width, starty, height);
}

void MotionBlurIB24(uint8_t* srcPtr, uint32_t srcPitch, int width, int height) {
    MotionBlurIB24(srcPtr, srcPitch, width, 0, height);
}

void MotionBlurIB32(uint8_t* srcPtr, uint32_t srcPitch, int width, int starty, int height) {
    if (frm1 == nullptr) InterframeFilterInit();
    MotionBlurIB32_Scalar(srcPtr, frm1, srcPitch, width, starty, height);
}

void MotionBlurIB32(uint8_t* srcPtr, uint32_t srcPitch, int width, int height) {
    MotionBlurIB32(srcPtr, srcPitch, width, 0, height);
}
