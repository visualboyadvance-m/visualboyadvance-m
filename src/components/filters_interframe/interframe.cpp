/// Interframe blending filters with thread-safe per-region buffering
/// Supports multithreaded IFB synchronized with FilterThread

#include "components/filters_interframe/interframe.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <vector>

// SIMD headers
// Don't use SSE2 on WINXP builds (32-bit) - target older CPUs with only SSE/MMX
#if !defined(WINXP) && (defined(__SSE2__) || (defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)))
#define USE_SSE2
#include <emmintrin.h>
#endif

#if defined(__SSE__) || (defined(_M_IX86_FP) && _M_IX86_FP >= 1)
#define USE_SSE
#include <xmmintrin.h>
#endif

// MMX intrinsics for 32-bit builds (when SSE2 not available)
// Uses compiler's built-in __MMX__ define (set by -mmmx flag), NOT the legacy
// MMX define used by 2xSaI which requires NASM assembly
#if defined(__MMX__) && !defined(USE_SSE2)
#define USE_MMX
#include <mmintrin.h>
#endif

extern uint32_t qRGB_COLOR_MASK[2];

// Threshold for fuzzy color comparison (per channel, 0-255 range)
// Colors within this threshold are considered "equal" for pattern detection
static constexpr int kColorThreshold = 16;

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
// Forward declarations of scalar implementations (needed for SIMD fallbacks)
// ============================================================================

static void SmartIB16_Scalar(uint8_t* srcPtr, uint8_t* frm1Ptr, uint8_t* frm2Ptr, uint8_t* frm3Ptr,
                              uint32_t srcPitch, int width, int starty, int height);

// ============================================================================
// SIMD Implementations - SSE2 (64-bit)
// ============================================================================

#ifdef USE_SSE2

static void MotionBlurIB32_SSE2(uint8_t* srcPtr, uint8_t* histPtr,
                                 uint32_t srcPitch, int width, int starty, int height) {
    (void)width;  // Width is implicit in srcPitch for SIMD version
    const __m128i colorMask = _mm_set1_epi32(0x00FEFEFE);

    uint32_t* src = reinterpret_cast<uint32_t*>(srcPtr) + starty * (srcPitch / 4);
    uint32_t* hist = reinterpret_cast<uint32_t*>(histPtr) + starty * (srcPitch / 4);

    int sPitch = srcPitch >> 2;

    for (int y = 0; y < height; y++) {
        int x = 0;
        // Process 4 pixels at a time (128-bit = 4 x 32-bit)
        for (; x + 4 <= sPitch; x += 4) {
            __m128i curr = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&src[x]));
            __m128i prev = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&hist[x]));

            // Save current to history before blending
            _mm_storeu_si128(reinterpret_cast<__m128i*>(&hist[x]), curr);

            // Blend: (current & mask) >> 1 + (prev & mask) >> 1
            __m128i curr_masked = _mm_and_si128(curr, colorMask);
            __m128i prev_masked = _mm_and_si128(prev, colorMask);
            __m128i curr_half = _mm_srli_epi32(curr_masked, 1);
            __m128i prev_half = _mm_srli_epi32(prev_masked, 1);
            __m128i blended = _mm_add_epi32(curr_half, prev_half);

            _mm_storeu_si128(reinterpret_cast<__m128i*>(&src[x]), blended);
        }

        // Handle remaining pixels (scalar)
        for (; x < sPitch; x++) {
            uint32_t color = src[x];
            uint32_t hist_color = hist[x];
            hist[x] = color;
            src[x] = ((color & 0xFEFEFE) >> 1) + ((hist_color & 0xFEFEFE) >> 1);
        }

        src += sPitch;
        hist += sPitch;
    }
}

static void MotionBlurIB16_SSE2(uint8_t* srcPtr, uint8_t* histPtr,
                                 uint32_t srcPitch, int width, int starty, int height) {
    (void)width;  // Width is implicit in srcPitch for SIMD version
    // Use constant mask for RGB555 - don't read RGB_LOW_BITS_MASK which may be
    // racily modified by filter threads running concurrently
    const uint16_t colorMask = 0x7BDE;  // ~0x0421 & 0x7FFF for RGB555
    const __m128i mask = _mm_set1_epi16(colorMask);

    uint16_t* src = reinterpret_cast<uint16_t*>(srcPtr) + starty * (srcPitch / 2);
    uint16_t* hist = reinterpret_cast<uint16_t*>(histPtr) + starty * (srcPitch / 2);

    int sPitch = srcPitch >> 1;

    for (int y = 0; y < height; y++) {
        int x = 0;
        // Process 8 pixels at a time (128-bit = 8 x 16-bit)
        for (; x + 8 <= sPitch; x += 8) {
            __m128i curr = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&src[x]));
            __m128i prev = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&hist[x]));

            _mm_storeu_si128(reinterpret_cast<__m128i*>(&hist[x]), curr);

            __m128i curr_masked = _mm_and_si128(curr, mask);
            __m128i prev_masked = _mm_and_si128(prev, mask);
            __m128i curr_half = _mm_srli_epi16(curr_masked, 1);
            __m128i prev_half = _mm_srli_epi16(prev_masked, 1);
            __m128i blended = _mm_add_epi16(curr_half, prev_half);

            _mm_storeu_si128(reinterpret_cast<__m128i*>(&src[x]), blended);
        }

        for (; x < sPitch; x++) {
            uint16_t color = src[x];
            uint16_t hist_color = hist[x];
            hist[x] = color;
            src[x] = ((color & colorMask) >> 1) + ((hist_color & colorMask) >> 1);
        }

        src += sPitch;
        hist += sPitch;
    }
}

#endif // USE_SSE2

// ============================================================================
// Helper functions for threshold-based color comparison (all builds)
// ============================================================================

// Helper: check if two 32-bit colors are "similar" (within threshold per channel)
static inline bool ColorsSimilar32(uint32_t a, uint32_t b, int threshold) {
    int dr = std::abs(static_cast<int>((a >> 16) & 0xFF) - static_cast<int>((b >> 16) & 0xFF));
    int dg = std::abs(static_cast<int>((a >> 8) & 0xFF) - static_cast<int>((b >> 8) & 0xFF));
    int db = std::abs(static_cast<int>(a & 0xFF) - static_cast<int>(b & 0xFF));
    return (dr <= threshold) && (dg <= threshold) && (db <= threshold);
}

// Helper: check if two 32-bit colors are "different" (any channel exceeds threshold)
static inline bool ColorsDifferent32(uint32_t a, uint32_t b, int threshold) {
    int dr = std::abs(static_cast<int>((a >> 16) & 0xFF) - static_cast<int>((b >> 16) & 0xFF));
    int dg = std::abs(static_cast<int>((a >> 8) & 0xFF) - static_cast<int>((b >> 8) & 0xFF));
    int db = std::abs(static_cast<int>(a & 0xFF) - static_cast<int>(b & 0xFF));
    return (dr > threshold) || (dg > threshold) || (db > threshold);
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
static inline bool ColorsSimilar24(uint8_t r1, uint8_t g1, uint8_t b1,
                                    uint8_t r2, uint8_t g2, uint8_t b2, int threshold) {
    int dr = std::abs(static_cast<int>(r1) - static_cast<int>(r2));
    int dg = std::abs(static_cast<int>(g1) - static_cast<int>(g2));
    int db = std::abs(static_cast<int>(b1) - static_cast<int>(b2));
    return (dr <= threshold) && (dg <= threshold) && (db <= threshold);
}

// Helper: check if three consecutive bytes (R,G,B) are different
static inline bool ColorsDifferent24(uint8_t r1, uint8_t g1, uint8_t b1,
                                      uint8_t r2, uint8_t g2, uint8_t b2, int threshold) {
    return !ColorsSimilar24(r1, g1, b1, r2, g2, b2, threshold);
}

#ifdef USE_SSE2

// SSE2 helper: compute absolute difference of packed bytes
static inline __m128i AbsDiff8(__m128i a, __m128i b) {
    // SSE2 doesn't have abs diff for unsigned, use max(a-b, b-a) via saturating sub
    return _mm_or_si128(_mm_subs_epu8(a, b), _mm_subs_epu8(b, a));
}

// SSE2 helper: check if colors are "similar" (all channel diffs <= threshold)
// Returns mask where all bits are set for similar pixels
static inline __m128i ColorsSimilar32_SSE2(__m128i a, __m128i b, __m128i threshold) {
    __m128i diff = AbsDiff8(a, b);
    // Check if all bytes <= threshold (compare diff > threshold, then invert)
    __m128i exceeds = _mm_subs_epu8(diff, threshold);  // saturating sub: 0 if diff <= threshold
    // exceeds is 0 for each byte where diff <= threshold
    // We need all 3 color channels (not alpha) to be 0
    // Compare with zero - if all bytes are 0, colors are similar
    __m128i zero = _mm_setzero_si128();
    // Mask off alpha channel for comparison (we only care about RGB)
    __m128i alphaMask = _mm_set1_epi32(0x00FFFFFF);
    exceeds = _mm_and_si128(exceeds, alphaMask);
    __m128i is_zero = _mm_cmpeq_epi32(exceeds, zero);
    return is_zero;
}

// SSE2 helper: check if colors are "different" (any channel diff > threshold)
static inline __m128i ColorsDifferent32_SSE2(__m128i a, __m128i b, __m128i threshold) {
    __m128i similar = ColorsSimilar32_SSE2(a, b, threshold);
    __m128i ones = _mm_set1_epi32(-1);
    return _mm_xor_si128(similar, ones);  // NOT similar = different
}

#endif // USE_SSE2 (SSE2 helper functions)

#ifdef USE_SSE2

static void SmartIB32_SSE2(uint8_t* srcPtr, uint8_t* frm1Ptr, uint8_t* frm2Ptr, uint8_t* frm3Ptr,
                           uint32_t srcPitch, int width, int starty, int height) {
    (void)width;  // Width is implicit in srcPitch for SIMD version
    const __m128i colorMask = _mm_set1_epi32(0x00FEFEFE);

    uint32_t* src = reinterpret_cast<uint32_t*>(srcPtr) + starty * (srcPitch / 4);
    uint32_t* frm1 = reinterpret_cast<uint32_t*>(frm1Ptr) + starty * (srcPitch / 4);
    uint32_t* frm2 = reinterpret_cast<uint32_t*>(frm2Ptr) + starty * (srcPitch / 4);
    uint32_t* frm3 = reinterpret_cast<uint32_t*>(frm3Ptr) + starty * (srcPitch / 4);

    int sPitch = srcPitch >> 2;

#ifdef USE_SSE2
    // Threshold as a byte value replicated across all bytes
    const __m128i threshold = _mm_set1_epi8(static_cast<char>(kColorThreshold));
#endif

    for (int y = 0; y < height; y++) {
        int x = 0;
#ifdef USE_SSE2
        // Process 4 pixels at a time
        for (; x + 4 <= sPitch; x += 4) {
            __m128i src0 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&src[x]));
            __m128i src1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&frm1[x]));
            __m128i src2 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&frm2[x]));
            __m128i src3 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&frm3[x]));

            // Save current to frm3 (oldest buffer gets newest frame)
            _mm_storeu_si128(reinterpret_cast<__m128i*>(&frm3[x]), src0);

            // Smart IB logic: Only blend pixels that CHANGED from previous frame
            // AND show oscillation pattern (similar to older frames).
            // This prevents static pixels from being incorrectly blended when
            // adjacent scrolling content happens to match history.
            //
            // changed = (src0 ~!= src1)  - current differs from previous
            // oscillates = (src0 ~== src2) || (src0 ~== src3)
            // blend = changed && oscillates

            // First check if pixel changed from previous frame
            __m128i changed = ColorsDifferent32_SSE2(src0, src1, threshold);

            // Check oscillation: current similar to 2 or 3 frames ago (A->B->A pattern)
            __m128i similar_0_2 = ColorsSimilar32_SSE2(src0, src2, threshold);
            __m128i similar_0_3 = ColorsSimilar32_SSE2(src0, src3, threshold);
            __m128i oscillates = _mm_or_si128(similar_0_2, similar_0_3);

            // Also check neighbors in history (for scrolling content)
            // ONLY if pixel changed - prevents static edge artifacts
            // Check ±1, ±2 pixels (reduced from ±3 for better precision)
            if (x >= 2 && x + 6 <= sPitch) {
                __m128i neighbor_osc = _mm_setzero_si128();

                // Check ±1 pixel offsets
                neighbor_osc = _mm_or_si128(neighbor_osc,
                    _mm_or_si128(ColorsSimilar32_SSE2(src0, _mm_loadu_si128(reinterpret_cast<const __m128i*>(&frm2[x-1])), threshold),
                                 ColorsSimilar32_SSE2(src0, _mm_loadu_si128(reinterpret_cast<const __m128i*>(&frm2[x+1])), threshold)));
                neighbor_osc = _mm_or_si128(neighbor_osc,
                    _mm_or_si128(ColorsSimilar32_SSE2(src0, _mm_loadu_si128(reinterpret_cast<const __m128i*>(&frm3[x-1])), threshold),
                                 ColorsSimilar32_SSE2(src0, _mm_loadu_si128(reinterpret_cast<const __m128i*>(&frm3[x+1])), threshold)));

                // Check ±2 pixel offsets
                neighbor_osc = _mm_or_si128(neighbor_osc,
                    _mm_or_si128(ColorsSimilar32_SSE2(src0, _mm_loadu_si128(reinterpret_cast<const __m128i*>(&frm2[x-2])), threshold),
                                 ColorsSimilar32_SSE2(src0, _mm_loadu_si128(reinterpret_cast<const __m128i*>(&frm2[x+2])), threshold)));
                neighbor_osc = _mm_or_si128(neighbor_osc,
                    _mm_or_si128(ColorsSimilar32_SSE2(src0, _mm_loadu_si128(reinterpret_cast<const __m128i*>(&frm3[x-2])), threshold),
                                 ColorsSimilar32_SSE2(src0, _mm_loadu_si128(reinterpret_cast<const __m128i*>(&frm3[x+2])), threshold)));

                // Neighbor oscillation only counts if pixel changed
                oscillates = _mm_or_si128(oscillates, _mm_and_si128(changed, neighbor_osc));
            }

            // blend = changed && oscillates
            // Only blend pixels that actually changed AND show oscillation
            __m128i blend_mask = _mm_and_si128(changed, oscillates);

            // Calculate blended value: average current and previous frame
            __m128i src0_masked = _mm_and_si128(src0, colorMask);
            __m128i src1_masked = _mm_and_si128(src1, colorMask);
            __m128i blended = _mm_add_epi32(_mm_srli_epi32(src0_masked, 1),
                                            _mm_srli_epi32(src1_masked, 1));

            // Select: blend_mask ? blended : src0
            __m128i result = _mm_or_si128(_mm_and_si128(blend_mask, blended),
                                          _mm_andnot_si128(blend_mask, src0));

            _mm_storeu_si128(reinterpret_cast<__m128i*>(&src[x]), result);
        }
#endif

        // Scalar tail
        for (; x < sPitch; x++) {
            uint32_t color = src[x];

            // First check if pixel changed from previous frame
            bool changed = ColorsDifferent32(color, frm1[x], kColorThreshold);

            // Only consider oscillation if pixel actually changed
            bool oscillates = false;
            if (changed) {
                // Check oscillation at current position (A->B->A pattern)
                oscillates = ColorsSimilar32(color, frm2[x], kColorThreshold) ||
                             ColorsSimilar32(color, frm3[x], kColorThreshold);

                // Also check neighbors in history (for scrolling content)
                // Check ±1, ±2 pixels (reduced from ±3 for better precision)
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
}

static void SmartIB16_SSE2(uint8_t* srcPtr, uint8_t* frm1Ptr, uint8_t* frm2Ptr, uint8_t* frm3Ptr,
                           uint32_t srcPitch, int width, int starty, int height) {
    // 16-bit SIMD for packed RGB565/555 threshold comparison is complex,
    // delegate to scalar implementation which has the full algorithm
    SmartIB16_Scalar(srcPtr, frm1Ptr, frm2Ptr, frm3Ptr, srcPitch, width, starty, height);
}

#endif // USE_SSE2

// ============================================================================
// MMX Implementations (32-bit) - With threshold-based comparison
// ============================================================================

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
    MotionBlurIB16_Scalar(srcPtr, histPtr, srcPitch, width, starty, height);
}

static void MotionBlurIB32_MMX(uint8_t* srcPtr, uint8_t* histPtr,
                                uint32_t srcPitch, int width, int starty, int height) {
    MotionBlurIB32_Scalar(srcPtr, histPtr, srcPitch, width, starty, height);
}

#endif  // USE_MMX

// ============================================================================
// Scalar Implementations (fallback)
// ============================================================================

static void SmartIB32_Scalar(uint8_t* srcPtr, uint8_t* frm1Ptr, uint8_t* frm2Ptr, uint8_t* frm3Ptr,
                              uint32_t srcPitch, int width, int starty, int height) {
    (void)width;  // Width is implicit in srcPitch for consistency with SIMD versions
    uint32_t* src0 = reinterpret_cast<uint32_t*>(srcPtr) + starty * srcPitch / 4;
    uint32_t* src1 = reinterpret_cast<uint32_t*>(frm1Ptr) + starty * srcPitch / 4;
    uint32_t* src2 = reinterpret_cast<uint32_t*>(frm2Ptr) + starty * srcPitch / 4;
    uint32_t* src3 = reinterpret_cast<uint32_t*>(frm3Ptr) + starty * srcPitch / 4;

    const uint32_t colorMask = 0xFEFEFE;  // Mask for divide-by-2
    int sPitch = srcPitch >> 2;
    int pos = 0;

    for (int j = 0; j < height; j++) {
        for (int i = 0; i < sPitch; i++) {
            uint32_t color = src0[pos];

            // First check if pixel changed from previous frame
            bool changed = ColorsDifferent32(color, src1[pos], kColorThreshold);

            // Only consider oscillation if pixel actually changed
            bool oscillates = false;
            if (changed) {
                // Check oscillation at current position (A->B->A pattern)
                oscillates = ColorsSimilar32(color, src2[pos], kColorThreshold) ||
                             ColorsSimilar32(color, src3[pos], kColorThreshold);

                // Also check neighbors in history (for scrolling content)
                // Check ±1, ±2 pixels (reduced from ±3 for better precision)
                if (!oscillates && i >= 2 && i < sPitch - 2) {
                    for (int offset = 1; offset <= 2 && !oscillates; offset++) {
                        oscillates = ColorsSimilar32(color, src2[pos-offset], kColorThreshold) ||
                                     ColorsSimilar32(color, src2[pos+offset], kColorThreshold) ||
                                     ColorsSimilar32(color, src3[pos-offset], kColorThreshold) ||
                                     ColorsSimilar32(color, src3[pos+offset], kColorThreshold);
                    }
                }
            }

            // 2-frame average preserves scroll motion better
            src0[pos] = oscillates
                            ? (((color & colorMask) >> 1) + ((src1[pos] & colorMask) >> 1))
                            : color;
            src3[pos] = color;
            pos++;
        }
    }
}

static void SmartIB16_Scalar(uint8_t* srcPtr, uint8_t* frm1Ptr, uint8_t* frm2Ptr, uint8_t* frm3Ptr,
                              uint32_t srcPitch, int width, int starty, int height) {
    (void)width;  // Width is implicit in srcPitch for consistency with SIMD versions
    // Use constant mask for RGB555 (0x7BDE) - don't read RGB_LOW_BITS_MASK which may be
    // racily modified by filter threads running concurrently
    const uint16_t colorMask = 0x7BDE;  // ~0x0421 & 0x7FFF for RGB555

    uint16_t* src0 = reinterpret_cast<uint16_t*>(srcPtr) + starty * srcPitch / 2;
    uint16_t* src1 = reinterpret_cast<uint16_t*>(frm1Ptr) + srcPitch * starty / 2;
    uint16_t* src2 = reinterpret_cast<uint16_t*>(frm2Ptr) + srcPitch * starty / 2;
    uint16_t* src3 = reinterpret_cast<uint16_t*>(frm3Ptr) + srcPitch * starty / 2;

    int sPitch = srcPitch >> 1;
    int pos = 0;

    for (int j = 0; j < height; j++) {
        for (int i = 0; i < sPitch; i++) {
            uint16_t color = src0[pos];

            // First check if pixel changed from previous frame
            bool changed = ColorsDifferent16(color, src1[pos], kColorThreshold16);

            // Only consider oscillation if pixel actually changed
            bool oscillates = false;
            if (changed) {
                // Check oscillation at current position (A->B->A pattern)
                oscillates = ColorsSimilar16(color, src2[pos], kColorThreshold16) ||
                             ColorsSimilar16(color, src3[pos], kColorThreshold16);

                // Also check neighbors in history (for scrolling content)
                // Check ±1, ±2 pixels (reduced from ±3 for better precision)
                if (!oscillates && i >= 2 && i < sPitch - 2) {
                    for (int offset = 1; offset <= 2 && !oscillates; offset++) {
                        oscillates = ColorsSimilar16(color, src2[pos-offset], kColorThreshold16) ||
                                     ColorsSimilar16(color, src2[pos+offset], kColorThreshold16) ||
                                     ColorsSimilar16(color, src3[pos-offset], kColorThreshold16) ||
                                     ColorsSimilar16(color, src3[pos+offset], kColorThreshold16);
                    }
                }
            }

            src0[pos] = oscillates
                            ? (((color & colorMask) >> 1) + ((src1[pos] & colorMask) >> 1))
                            : color;
            src3[pos] = color;
            pos++;
        }
    }
}

static void SmartIB24_Scalar(uint8_t* srcPtr, uint8_t* frm1Ptr, uint8_t* frm2Ptr, uint8_t* frm3Ptr,
                              uint32_t srcPitch, int width, int starty, int height) {
    (void)width;  // Width is implicit in srcPitch for consistency with SIMD versions
    const uint8_t colorMask = 0xFE;  // Mask for divide-by-2

    uint8_t* src0 = srcPtr + starty * srcPitch / 3;
    uint8_t* src1 = frm1Ptr + srcPitch * starty / 3;
    uint8_t* src2 = frm2Ptr + srcPitch * starty / 3;
    uint8_t* src3 = frm3Ptr + srcPitch * starty / 3;

    int sPitch = srcPitch / 3;
    int pos = 0;

    for (int j = 0; j < height; j++) {
        for (int i = 0; i < sPitch; i++) {
            uint8_t r0 = src0[pos];
            uint8_t g0 = src0[pos + 1];
            uint8_t b0 = src0[pos + 2];

            // First check if pixel changed from previous frame
            bool changed = ColorsDifferent24(r0, g0, b0,
                                              src1[pos], src1[pos + 1], src1[pos + 2], kColorThreshold24);

            // Only consider oscillation if pixel actually changed
            bool oscillates = false;
            if (changed) {
                // Check oscillation at current position (A->B->A pattern)
                oscillates = ColorsSimilar24(r0, g0, b0,
                                              src2[pos], src2[pos + 1], src2[pos + 2], kColorThreshold24) ||
                             ColorsSimilar24(r0, g0, b0,
                                              src3[pos], src3[pos + 1], src3[pos + 2], kColorThreshold24);

                // Also check neighbors in history (for scrolling content)
                // Check ±1, ±2 pixels (reduced from ±3 for better precision)
                // Each pixel is 3 bytes, so offset by 3*n
                if (!oscillates && i >= 2 && i < sPitch - 2) {
                    for (int offset = 1; offset <= 2 && !oscillates; offset++) {
                        int byteOffset = offset * 3;
                        oscillates = ColorsSimilar24(r0, g0, b0,
                                        src2[pos-byteOffset], src2[pos-byteOffset+1], src2[pos-byteOffset+2], kColorThreshold24) ||
                                     ColorsSimilar24(r0, g0, b0,
                                        src2[pos+byteOffset], src2[pos+byteOffset+1], src2[pos+byteOffset+2], kColorThreshold24) ||
                                     ColorsSimilar24(r0, g0, b0,
                                        src3[pos-byteOffset], src3[pos-byteOffset+1], src3[pos-byteOffset+2], kColorThreshold24) ||
                                     ColorsSimilar24(r0, g0, b0,
                                        src3[pos+byteOffset], src3[pos+byteOffset+1], src3[pos+byteOffset+2], kColorThreshold24);
                    }
                }
            }

            if (oscillates) {
                // 2-frame average preserves scroll motion better
                src0[pos] = ((r0 & colorMask) >> 1) + ((src1[pos] & colorMask) >> 1);
                src0[pos + 1] = ((g0 & colorMask) >> 1) + ((src1[pos + 1] & colorMask) >> 1);
                src0[pos + 2] = ((b0 & colorMask) >> 1) + ((src1[pos + 2] & colorMask) >> 1);
            }

            src3[pos] = r0;
            src3[pos + 1] = g0;
            src3[pos + 2] = b0;
            pos += 3;
        }
    }
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

static void SmartIB8_Scalar(uint8_t* srcPtr, uint8_t* frm1Ptr, uint8_t* frm2Ptr, uint8_t* frm3Ptr,
                             uint32_t srcPitch, int width, int starty, int height) {
    (void)width;  // Width is implicit in srcPitch for consistency with SIMD versions
    // Use constant mask for RGB555 (0x7BDE) - palette is converted to RGB555 for blending
    const uint16_t colorMask = 0x7BDE;  // ~0x0421 & 0x7FFF for RGB555

    uint8_t* src0 = srcPtr + starty * srcPitch;
    uint8_t* src1 = frm1Ptr + srcPitch * starty;
    uint8_t* src2 = frm2Ptr + srcPitch * starty;
    uint8_t* src3 = frm3Ptr + srcPitch * starty;

    int sPitch = srcPitch;
    int pos = 0;

    for (int j = 0; j < height; j++) {
        for (int i = 0; i < sPitch; i++) {
            // Convert palette indices to RGB555 for comparison
            uint16_t c0 = PaletteToRGB555(src0[pos]);
            uint16_t c1 = PaletteToRGB555(src1[pos]);
            uint16_t c2 = PaletteToRGB555(src2[pos]);
            uint16_t c3 = PaletteToRGB555(src3[pos]);

            // First check if pixel changed from previous frame
            bool changed = ColorsDifferent16(c0, c1, kColorThreshold16);

            // Only consider oscillation if pixel actually changed
            bool oscillates = false;
            if (changed) {
                // Check oscillation at current position (A->B->A pattern)
                oscillates = ColorsSimilar16(c0, c2, kColorThreshold16) ||
                             ColorsSimilar16(c0, c3, kColorThreshold16);

                // Also check neighbors in history (for scrolling content)
                // Check ±1, ±2 pixels (reduced from ±3 for better precision)
                if (!oscillates && i >= 2 && i < sPitch - 2) {
                    for (int offset = 1; offset <= 2 && !oscillates; offset++) {
                        uint16_t c2_left = PaletteToRGB555(src2[pos-offset]);
                        uint16_t c2_right = PaletteToRGB555(src2[pos+offset]);
                        uint16_t c3_left = PaletteToRGB555(src3[pos-offset]);
                        uint16_t c3_right = PaletteToRGB555(src3[pos+offset]);
                        oscillates = ColorsSimilar16(c0, c2_left, kColorThreshold16) ||
                                     ColorsSimilar16(c0, c2_right, kColorThreshold16) ||
                                     ColorsSimilar16(c0, c3_left, kColorThreshold16) ||
                                     ColorsSimilar16(c0, c3_right, kColorThreshold16);
                    }
                }
            }

            if (oscillates) {
                uint16_t blended = ((c0 & colorMask) >> 1) + ((c1 & colorMask) >> 1);
                src0[pos] = RGB555ToPalette(blended);
            } else {
                src0[pos] = RGB555ToPalette(c0);
            }
            src3[pos] = RGB555ToPalette(c0);
            pos++;
        }
    }
}

static void MotionBlurIB32_Scalar(uint8_t* srcPtr, uint8_t* histPtr,
                                   uint32_t srcPitch, int width, int starty, int height) {
    (void)width;  // Width is implicit in srcPitch for consistency with SIMD versions
    uint32_t* src0 = reinterpret_cast<uint32_t*>(srcPtr) + starty * srcPitch / 4;
    uint32_t* src1 = reinterpret_cast<uint32_t*>(histPtr) + starty * srcPitch / 4;

    const uint32_t colorMask = 0xfefefe;
    int sPitch = srcPitch >> 2;
    int pos = 0;

    for (int j = 0; j < height; j++) {
        for (int i = 0; i < sPitch; i++) {
            uint32_t color = src0[pos];
            src0[pos] = (((color & colorMask) >> 1) + ((src1[pos] & colorMask) >> 1));
            src1[pos] = color;
            pos++;
        }
    }
}

static void MotionBlurIB16_Scalar(uint8_t* srcPtr, uint8_t* histPtr,
                                   uint32_t srcPitch, int width, int starty, int height) {
    (void)width;  // Width is implicit in srcPitch for consistency with SIMD versions
    // Use constant mask for RGB555 - don't read RGB_LOW_BITS_MASK which may be
    // racily modified by filter threads running concurrently
    const uint16_t colorMask = 0x7BDE;  // ~0x0421 & 0x7FFF for RGB555

    uint16_t* src0 = reinterpret_cast<uint16_t*>(srcPtr) + starty * srcPitch / 2;
    uint16_t* src1 = reinterpret_cast<uint16_t*>(histPtr) + starty * srcPitch / 2;

    int sPitch = srcPitch >> 1;
    int pos = 0;

    for (int j = 0; j < height; j++) {
        for (int i = 0; i < sPitch; i++) {
            uint16_t color = src0[pos];
            src0[pos] = (((color & colorMask) >> 1) + ((src1[pos] & colorMask) >> 1));
            src1[pos] = color;
            pos++;
        }
    }
}

static void MotionBlurIB24_Scalar(uint8_t* srcPtr, uint8_t* histPtr,
                                   uint32_t srcPitch, int width, int starty, int height) {
    (void)width;  // Width is implicit in srcPitch for consistency with SIMD versions
    const uint8_t colorMask = 0xfe;

    uint8_t* src0 = srcPtr + starty * srcPitch / 3;
    uint8_t* src1 = histPtr + starty * srcPitch / 3;

    int sPitch = srcPitch / 3;
    int pos = 0;

    for (int j = 0; j < height; j++) {
        for (int i = 0; i < sPitch; i++) {
            uint8_t color = src0[pos];
            uint8_t color2 = src0[pos + 1];
            uint8_t color3 = src0[pos + 2];

            src0[pos] = ((color & colorMask) >> 1) + ((src1[pos] & colorMask) >> 1);
            src0[pos + 1] = ((color2 & colorMask) >> 1) + ((src1[pos + 1] & colorMask) >> 1);
            src0[pos + 2] = ((color3 & colorMask) >> 1) + ((src1[pos + 2] & colorMask) >> 1);

            src1[pos] = color;
            src1[pos + 1] = color2;
            src1[pos + 2] = color3;
            pos += 3;
        }
    }
}

static void MotionBlurIB8_Scalar(uint8_t* srcPtr, uint8_t* histPtr,
                                  uint32_t srcPitch, int width, int starty, int height) {
    (void)width;  // Width is implicit in srcPitch for consistency with SIMD versions
    // Use constant mask for RGB555 - palette is converted to RGB555 for blending
    const uint16_t colorMask = 0x7BDE;  // ~0x0421 & 0x7FFF for RGB555

    uint8_t* src0 = srcPtr + starty * srcPitch;
    uint8_t* src1 = histPtr + starty * srcPitch;

    int sPitch = srcPitch;
    int pos = 0;

    for (int j = 0; j < height; j++) {
        for (int i = 0; i < sPitch; i++) {
            uint16_t color = src0[pos] == 0xff
                                 ? 0x7fff
                                 : ((src0[pos] & 0xe0) << 7) | ((src0[pos] & 0x1c) << 5) |
                                       ((src0[pos] & 0x3) << 3);
            uint16_t color2 = src1[pos] == 0xff
                                  ? 0x7fff
                                  : ((src1[pos] & 0xe0) << 7) | ((src1[pos] & 0x1c) << 5) |
                                        ((src1[pos] & 0x3) << 3);
            uint16_t color_dst = ((color & colorMask) >> 1) + ((color2 & colorMask) >> 1);

            src0[pos] = static_cast<uint8_t>(((color_dst >> 7) & 0xe0) |
                                              ((color_dst >> 5) & 0x1c) | ((color_dst >> 3) & 0x3));
            src1[pos] = static_cast<uint8_t>(((color >> 7) & 0xe0) | ((color >> 5) & 0x1c) |
                                              ((color >> 3) & 0x3));
            pos++;
        }
    }
}

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

    switch (colorDepth) {
        case 32:
#ifdef USE_SSE2
            SmartIB32_SSE2(srcPtr, hist1, hist2, hist3, srcPitch, width, starty, height);
#elif defined(USE_MMX)
            SmartIB32_MMX(srcPtr, hist1, hist2, hist3, srcPitch, width, starty, height);
#else
            SmartIB32_Scalar(srcPtr, hist1, hist2, hist3, srcPitch, width, starty, height);
#endif
            break;

        case 16:
#ifdef USE_SSE2
            SmartIB16_SSE2(srcPtr, hist1, hist2, hist3, srcPitch, width, starty, height);
#elif defined(USE_MMX)
            SmartIB_MMX(srcPtr, hist1, hist2, hist3, srcPitch, width, starty, height);
#else
            SmartIB16_Scalar(srcPtr, hist1, hist2, hist3, srcPitch, width, starty, height);
#endif
            break;

        case 24:
            SmartIB24_Scalar(srcPtr, hist1, hist2, hist3, srcPitch, width, starty, height);
            break;

        case 8:
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

    switch (colorDepth) {
        case 32:
#ifdef USE_SSE2
            MotionBlurIB32_SSE2(srcPtr, hist, srcPitch, width, starty, height);
#elif defined(USE_MMX)
            MotionBlurIB32_MMX(srcPtr, hist, srcPitch, width, starty, height);
#else
            MotionBlurIB32_Scalar(srcPtr, hist, srcPitch, width, starty, height);
#endif
            break;

        case 16:
#ifdef USE_SSE2
            MotionBlurIB16_SSE2(srcPtr, hist, srcPitch, width, starty, height);
#elif defined(USE_MMX)
            MotionBlurIB_MMX(srcPtr, hist, srcPitch, width, starty, height);
#else
            MotionBlurIB16_Scalar(srcPtr, hist, srcPitch, width, starty, height);
#endif
            break;

        case 24:
            MotionBlurIB24_Scalar(srcPtr, hist, srcPitch, width, starty, height);
            break;

        case 8:
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
