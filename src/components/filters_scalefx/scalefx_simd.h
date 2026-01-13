// ScaleFX SIMD Helper Functions
// SSE2 for 64-bit, MMX for 32-bit Windows XP compatibility

#ifndef SCALEFX_SIMD_H
#define SCALEFX_SIMD_H

#include <cstdint>
#include <cmath>
#include <algorithm>

// SIMD detection (following interframe.cpp patterns)
// 64-bit builds always have SSE2/SSE3 available
#if !defined(WINXP) && (defined(__SSE2__) || (defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)))
#define SCALEFX_USE_SSE2
#include <emmintrin.h>
#endif

// SSE3 detection - available on all x64 CPUs
#if !defined(WINXP) && (defined(__SSE3__) || defined(_M_X64))
#define SCALEFX_USE_SSE3
#include <pmmintrin.h>
#endif

#if defined(__SSE__) || (defined(_M_IX86_FP) && _M_IX86_FP >= 1)
#define SCALEFX_USE_SSE
#include <xmmintrin.h>
#endif

// MMX detection for WINXP 32-bit builds
// MSVC defines _M_IX86 for 32-bit, GCC/Clang define __MMX__
#if defined(WINXP) && (defined(__MMX__) || defined(_M_IX86))
#define SCALEFX_USE_MMX
#ifdef _MSC_VER
#include <intrin.h>
#else
#include <mmintrin.h>
#endif
#endif

namespace scalefx {

// Extract RGB components from 32-bit ARGB pixel
inline uint8_t GetR(uint32_t pixel) { return (pixel >> 16) & 0xFF; }
inline uint8_t GetG(uint32_t pixel) { return (pixel >> 8) & 0xFF; }
inline uint8_t GetB(uint32_t pixel) { return pixel & 0xFF; }

// CIE-weighted color distance (perception-based)
// Uses weighted Euclidean distance where weights depend on average red
inline float ColorDistance(uint32_t a, uint32_t b) {
    float r_avg = 0.5f * (GetR(a) + GetR(b));

    // CIE weights: more sensitive to green, red-dependent blue sensitivity
    float w_r = 2.0f + r_avg / 256.0f;
    float w_g = 4.0f;
    float w_b = 3.0f - r_avg / 256.0f;

    float dr = static_cast<float>(GetR(a)) - static_cast<float>(GetR(b));
    float dg = static_cast<float>(GetG(a)) - static_cast<float>(GetG(b));
    float db = static_cast<float>(GetB(a)) - static_cast<float>(GetB(b));

    return std::sqrt(w_r * dr * dr + w_g * dg * dg + w_b * db * db) / 3.0f;
}

#ifdef SCALEFX_USE_SSE2

// Fast reciprocal square root approximation (Newton-Raphson iteration)
inline __m128 FastRsqrt(__m128 x) {
    __m128 half = _mm_set1_ps(0.5f);
    __m128 three = _mm_set1_ps(3.0f);
    __m128 approx = _mm_rsqrt_ps(x);
    // One Newton-Raphson iteration for better accuracy
    return _mm_mul_ps(_mm_mul_ps(half, approx),
                      _mm_sub_ps(three, _mm_mul_ps(_mm_mul_ps(x, approx), approx)));
}

// Fast square root using reciprocal sqrt
inline __m128 FastSqrt(__m128 x) {
    // sqrt(x) = x * rsqrt(x), but handle zero case
    __m128 zero = _mm_setzero_ps();
    __m128 mask = _mm_cmpneq_ps(x, zero);
    __m128 rsqrt = FastRsqrt(x);
    return _mm_and_ps(_mm_mul_ps(x, rsqrt), mask);
}

// Unpack 4 pixels from __m128i to separate R, G, B float vectors
inline void UnpackPixels4(__m128i pixels, __m128& r, __m128& g, __m128& b) {
    // pixels contains 4 x ARGB (32-bit each)
    __m128i mask_r = _mm_set1_epi32(0x00FF0000);
    __m128i mask_g = _mm_set1_epi32(0x0000FF00);
    __m128i mask_b = _mm_set1_epi32(0x000000FF);

    __m128i r_int = _mm_srli_epi32(_mm_and_si128(pixels, mask_r), 16);
    __m128i g_int = _mm_srli_epi32(_mm_and_si128(pixels, mask_g), 8);
    __m128i b_int = _mm_and_si128(pixels, mask_b);

    r = _mm_cvtepi32_ps(r_int);
    g = _mm_cvtepi32_ps(g_int);
    b = _mm_cvtepi32_ps(b_int);
}

// Calculate color distance for 4 pixel pairs in parallel
inline __m128 ColorDistance4(__m128i a, __m128i b) {
    __m128 ra, ga, ba, rb, gb, bb;
    UnpackPixels4(a, ra, ga, ba);
    UnpackPixels4(b, rb, gb, bb);

    // Calculate average red for weight computation
    __m128 r_avg = _mm_mul_ps(_mm_set1_ps(0.5f), _mm_add_ps(ra, rb));
    __m128 inv256 = _mm_set1_ps(1.0f / 256.0f);

    // CIE weights
    __m128 w_r = _mm_add_ps(_mm_set1_ps(2.0f), _mm_mul_ps(r_avg, inv256));
    __m128 w_g = _mm_set1_ps(4.0f);
    __m128 w_b = _mm_sub_ps(_mm_set1_ps(3.0f), _mm_mul_ps(r_avg, inv256));

    // Color differences
    __m128 dr = _mm_sub_ps(ra, rb);
    __m128 dg = _mm_sub_ps(ga, gb);
    __m128 db = _mm_sub_ps(ba, bb);

    // Weighted squared differences
    __m128 sum = _mm_add_ps(
        _mm_add_ps(_mm_mul_ps(w_r, _mm_mul_ps(dr, dr)),
                   _mm_mul_ps(w_g, _mm_mul_ps(dg, dg))),
        _mm_mul_ps(w_b, _mm_mul_ps(db, db))
    );

    // sqrt(sum) / 3
    __m128 inv3 = _mm_set1_ps(1.0f / 3.0f);
    return _mm_mul_ps(FastSqrt(sum), inv3);
}

// Linear interpolation: a + t * (b - a)
inline __m128 Lerp(__m128 a, __m128 b, __m128 t) {
    return _mm_add_ps(a, _mm_mul_ps(t, _mm_sub_ps(b, a)));
}

// Clamp values to [0, max]
inline __m128 Clamp(__m128 x, __m128 max_val) {
    __m128 zero = _mm_setzero_ps();
    return _mm_min_ps(_mm_max_ps(x, zero), max_val);
}

#endif // SCALEFX_USE_SSE2

#ifdef SCALEFX_USE_MMX

// MMX helper: end MMX state (must call after MMX operations)
inline void MMXEnd() {
    _mm_empty();
}

// MMX 50% blend of two pixels (processes 2 pixels at once)
// Uses the formula: avg = ((a ^ b) >> 1) + (a & b) for each byte
inline __m64 MMXBlend50(__m64 a, __m64 b) {
    __m64 xor_ab = _mm_xor_si64(a, b);
    __m64 and_ab = _mm_and_si64(a, b);
    // Mask to clear high bit before shift (0xFEFEFEFE for each 32-bit pixel)
    __m64 mask = _mm_set1_pi32(static_cast<int>(0xFEFEFEFE));
    __m64 shifted = _mm_srli_pi32(_mm_and_si64(xor_ab, mask), 1);
    return _mm_add_pi32(shifted, and_ab);
}

// Load 2 pixels into MMX register
inline __m64 MMXLoad2(const uint32_t* p) {
    return *reinterpret_cast<const __m64*>(p);
}

// Store 2 pixels from MMX register
inline void MMXStore2(uint32_t* p, __m64 v) {
    *reinterpret_cast<__m64*>(p) = v;
}

#endif // SCALEFX_USE_MMX

// Scalar helper functions used by all implementations

// Clamp float to [min, max]
inline float Clamp(float x, float min_val, float max_val) {
    return std::max(min_val, std::min(max_val, x));
}

// Linear interpolation
inline float Lerp(float a, float b, float t) {
    return a + t * (b - a);
}

// Step function: returns 0 if x < edge, 1 otherwise
inline float Step(float edge, float x) {
    return x < edge ? 0.0f : 1.0f;
}

// Smooth step (cubic Hermite interpolation)
inline float SmoothStep(float edge0, float edge1, float x) {
    float t = Clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

} // namespace scalefx

#endif // SCALEFX_SIMD_H
