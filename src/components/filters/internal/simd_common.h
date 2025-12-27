/// SIMD detection and common utilities for display filters
/// SSE2 for 64-bit builds, MMX for 32-bit builds

#ifndef VBAM_COMPONENTS_FILTERS_INTERNAL_SIMD_COMMON_H_
#define VBAM_COMPONENTS_FILTERS_INTERNAL_SIMD_COMMON_H_

#include <cstdint>

// ============================================================================
// SIMD Detection (following interframe.cpp pattern)
// ============================================================================

// SSE2 detection: available on all x86_64 and x86 with SSE2 support
#if defined(__SSE2__) || (defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2))
#define USE_SSE2
#include <emmintrin.h>
#endif

// SSE detection
#if defined(__SSE__) || (defined(_M_IX86_FP) && _M_IX86_FP >= 1)
#define USE_SSE
#include <xmmintrin.h>
#endif

// MMX detection (32-bit only, controlled by build system)
#ifdef MMX
extern "C" bool cpu_mmx;
#endif

// ============================================================================
// Common Color Masks
// ============================================================================

extern int RGB_LOW_BITS_MASK;

#ifdef USE_SSE2

// 32-bit ARGB masks
static inline __m128i simd_color_mask_32() {
    return _mm_set1_epi32(~RGB_LOW_BITS_MASK);
}

// For 0x00FEFEFE mask (common for 32-bit blending)
static inline __m128i simd_blend_mask_32() {
    return _mm_set1_epi32(0x00FEFEFE);
}

// ============================================================================
// Common SIMD Interpolation Functions
// ============================================================================

/// Interpolate two 32-bit pixels: (A + B) / 2
/// Processes 4 pixels at a time
static inline __m128i simd_interp_32(__m128i a, __m128i b) {
    const __m128i mask = _mm_set1_epi32(0x00FEFEFE);
    __m128i a_masked = _mm_and_si128(a, mask);
    __m128i b_masked = _mm_and_si128(b, mask);
    return _mm_add_epi32(_mm_srli_epi32(a_masked, 1),
                         _mm_srli_epi32(b_masked, 1));
}

/// Interpolate two 16-bit pixels: (A + B) / 2
/// Processes 8 pixels at a time
static inline __m128i simd_interp_16(__m128i a, __m128i b, __m128i mask) {
    __m128i a_masked = _mm_and_si128(a, mask);
    __m128i b_masked = _mm_and_si128(b, mask);
    return _mm_add_epi16(_mm_srli_epi16(a_masked, 1),
                         _mm_srli_epi16(b_masked, 1));
}

/// Quarter interpolation: (3*A + B) / 4
/// Processes 4 pixels at a time for 32-bit
static inline __m128i simd_q_interp_32(__m128i a, __m128i b) {
    const __m128i mask = _mm_set1_epi32(0x00FCFCFC);
    __m128i a_masked = _mm_and_si128(a, mask);
    __m128i b_masked = _mm_and_si128(b, mask);
    // (3*A + B) / 4 = A - A/4 + B/4
    __m128i a_quarter = _mm_srli_epi32(a_masked, 2);
    __m128i b_quarter = _mm_srli_epi32(b_masked, 2);
    return _mm_add_epi32(_mm_sub_epi32(a, a_quarter), b_quarter);
}

/// Darken color by shifting: color >> 1 + color >> 2 = color * 0.75
/// Used for scanline effects
static inline __m128i simd_darken_32(__m128i color) {
    const __m128i mask = _mm_set1_epi32(0x00FEFEFE);
    __m128i masked = _mm_and_si128(color, mask);
    __m128i half = _mm_srli_epi32(masked, 1);
    __m128i half_masked = _mm_and_si128(half, mask);
    __m128i quarter = _mm_srli_epi32(half_masked, 1);
    return _mm_add_epi32(half, quarter);
}

#endif  // USE_SSE2

#endif  // VBAM_COMPONENTS_FILTERS_INTERNAL_SIMD_COMMON_H_
