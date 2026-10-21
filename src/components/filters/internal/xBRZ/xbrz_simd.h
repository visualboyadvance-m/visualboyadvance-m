// ****************************************************************************
// * This file is part of the xBRZ project. It is distributed under           *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0         *
// * Copyright (C) Zenju (zenju AT gmx DOT de) - All Rights Reserved          *
// *                                                                          *
// * Additionally and as a special exception, the author gives permission     *
// * to link the code of this program with the following libraries            *
// * (or with modified versions that use the same licenses), and distribute   *
// * linked combinations including the two: MAME, FreeFileSync, Snes9x, ePSXe *
// *                                                                          *
// * You must obey the GNU General Public License in all respects for all of  *
// * the code used other than MAME, FreeFileSync, Snes9x, ePSXe.              *
// * If you modify this file, you may extend this exception to your version   *
// * of the file, but you are not obligated to do so. If you do not wish to   *
// * do so, delete this exception statement from your version.                *
// ****************************************************************************

// VBA-M: four-lane SIMD layer for the xBRZ scaler.
//
// xbrz.cpp is written once against the small vector vocabulary below and
// compiled with one of three backends:
//
//   XBRZ_SIMD_SSE2   x86-64 and x86 with SSE2 (not the Windows XP build, which
//                    targets pre-SSE2 CPUs like the rest of the filters)
//   XBRZ_SIMD_NEON   AArch64 (Apple silicon, Linux arm64, Windows on ARM,
//                    Android arm64-v8a); 32-bit ARM lacks vsqrtq_f32 and
//                    falls through to the scalar backend
//   XBRZ_SIMD_SCALAR everything else (DJGPP i586, RISC-V, armeabi-v7a), and
//                    any build with XBRZ_FORCE_SCALAR defined -- the unit test
//                    builds a second copy of xbrz.cpp this way and checks the
//                    SIMD build against it bit for bit
//
// Every lane operation has the same rounding on every backend: the float
// ops are single IEEE operations (no fused multiply-add; xbrz.cpp is compiled
// with contraction off) and the integer ops are exact, so the three backends
// produce identical pixels.

#ifndef XBRZ_SIMD_H_3C1E7A4F9B2D6085
#define XBRZ_SIMD_H_3C1E7A4F9B2D6085

#include <cmath>
#include <cstdint>
#include <cstring>

#if !defined(XBRZ_FORCE_SCALAR) && !defined(WINXP) && \
    (defined(__SSE2__) || defined(_M_X64) || defined(_M_AMD64) || \
     (defined(_M_IX86_FP) && _M_IX86_FP >= 2))
    #define XBRZ_SIMD_SSE2 1
    #include <emmintrin.h>
#elif !defined(XBRZ_FORCE_SCALAR) && \
    (defined(__aarch64__) || defined(_M_ARM64) || defined(_M_ARM64EC))
    #define XBRZ_SIMD_NEON 1
    #include <arm_neon.h>
#else
    #define XBRZ_SIMD_SCALAR 1
#endif

#ifdef _MSC_VER
    #define XBRZ_SIMD_INLINE __forceinline
#elif defined __GNUC__
    #define XBRZ_SIMD_INLINE __attribute__((always_inline)) inline
#else
    #define XBRZ_SIMD_INLINE inline
#endif

namespace xbrz
{
namespace simd
{
#if defined(XBRZ_SIMD_SSE2)
//----------------------------------------------------------------------------
// SSE2
//----------------------------------------------------------------------------
struct U32x4 { __m128i v; };
struct F32x4 { __m128 v; };

XBRZ_SIMD_INLINE U32x4 loadu(const uint32_t* p) { return {_mm_loadu_si128(reinterpret_cast<const __m128i*>(p))}; }
XBRZ_SIMD_INLINE void storeu(uint32_t* p, U32x4 a) { _mm_storeu_si128(reinterpret_cast<__m128i*>(p), a.v); }
XBRZ_SIMD_INLINE void storeu2(uint32_t* p, U32x4 a) { _mm_storel_epi64(reinterpret_cast<__m128i*>(p), a.v); } //lanes 0, 1 only

XBRZ_SIMD_INLINE U32x4 set1(uint32_t x) { return {_mm_set1_epi32(static_cast<int>(x))}; }
XBRZ_SIMD_INLINE U32x4 set4(uint32_t a, uint32_t b, uint32_t c, uint32_t d) //lane 0 = a
{
    return {_mm_setr_epi32(static_cast<int>(a), static_cast<int>(b), static_cast<int>(c), static_cast<int>(d))};
}

XBRZ_SIMD_INLINE U32x4 bitAnd   (U32x4 a, U32x4 b) { return {_mm_and_si128   (a.v, b.v)}; }
XBRZ_SIMD_INLINE U32x4 bitOr    (U32x4 a, U32x4 b) { return {_mm_or_si128    (a.v, b.v)}; }
XBRZ_SIMD_INLINE U32x4 bitAndNot(U32x4 a, U32x4 b) { return {_mm_andnot_si128(a.v, b.v)}; } //~a & b
XBRZ_SIMD_INLINE U32x4 add      (U32x4 a, U32x4 b) { return {_mm_add_epi32   (a.v, b.v)}; }
XBRZ_SIMD_INLINE U32x4 cmpEq    (U32x4 a, U32x4 b) { return {_mm_cmpeq_epi32 (a.v, b.v)}; }

template <int N> XBRZ_SIMD_INLINE U32x4 shr(U32x4 a) { return {_mm_srli_epi32(a.v, N)}; }
template <int N> XBRZ_SIMD_INLINE U32x4 shl(U32x4 a) { return {_mm_slli_epi32(a.v, N)}; }

XBRZ_SIMD_INLINE F32x4 fset1(float x) { return {_mm_set1_ps(x)}; }
XBRZ_SIMD_INLINE F32x4 floadu(const float* p) { return {_mm_loadu_ps(p)}; }
XBRZ_SIMD_INLINE F32x4 toFloat(U32x4 a) { return {_mm_cvtepi32_ps(a.v)}; } //lanes must be < 2^31

XBRZ_SIMD_INLINE F32x4 fadd (F32x4 a, F32x4 b) { return {_mm_add_ps(a.v, b.v)}; }
XBRZ_SIMD_INLINE F32x4 fsub (F32x4 a, F32x4 b) { return {_mm_sub_ps(a.v, b.v)}; }
XBRZ_SIMD_INLINE F32x4 fmul (F32x4 a, F32x4 b) { return {_mm_mul_ps(a.v, b.v)}; }
XBRZ_SIMD_INLINE F32x4 fmin (F32x4 a, F32x4 b) { return {_mm_min_ps(a.v, b.v)}; }
XBRZ_SIMD_INLINE F32x4 fsqrt(F32x4 a)          { return {_mm_sqrt_ps(a.v)}; }
XBRZ_SIMD_INLINE F32x4 fabs (F32x4 a)          { return {_mm_andnot_ps(_mm_set1_ps(-0.0f), a.v)}; }

XBRZ_SIMD_INLINE U32x4 fcmpLt(F32x4 a, F32x4 b) { return {_mm_castps_si128(_mm_cmplt_ps(a.v, b.v))}; }
XBRZ_SIMD_INLINE U32x4 fcmpLe(F32x4 a, F32x4 b) { return {_mm_castps_si128(_mm_cmple_ps(a.v, b.v))}; }

XBRZ_SIMD_INLINE bool maskAll (U32x4 m) { return _mm_movemask_ps(_mm_castsi128_ps(m.v)) == 0xF; }
XBRZ_SIMD_INLINE bool maskNone(U32x4 m) { return _mm_movemask_ps(_mm_castsi128_ps(m.v)) == 0; }

XBRZ_SIMD_INLINE void storeLanes(uint32_t* out, U32x4 a) { _mm_storeu_si128(reinterpret_cast<__m128i*>(out), a.v); }
XBRZ_SIMD_INLINE void storeLanes(float* out, F32x4 a) { _mm_storeu_ps(out, a.v); }

//lanes i0 .. i3 of a, in that order
template <int i0, int i1, int i2, int i3> XBRZ_SIMD_INLINE U32x4 lanes(U32x4 a)
{
    return {_mm_shuffle_epi32(a.v, _MM_SHUFFLE(i3, i2, i1, i0))};
}

//the low byte of each lane, lane 0 in the low byte
XBRZ_SIMD_INLINE uint32_t packLowBytes(U32x4 a)
{
    const __m128i w = _mm_packs_epi32(a.v, a.v);
    return static_cast<uint32_t>(_mm_cvtsi128_si32(_mm_packus_epi16(w, w)));
}

//one pixel's four channels as 16-bit lanes (upper four lanes unused)
struct Pix16 { __m128i v; };

XBRZ_SIMD_INLINE Pix16 unpackPixel(uint32_t pix)
{
    return {_mm_unpacklo_epi8(_mm_cvtsi32_si128(static_cast<int>(pix)), _mm_setzero_si128())};
}

XBRZ_SIMD_INLINE uint32_t packPixel(Pix16 a)
{
    return static_cast<uint32_t>(_mm_cvtsi128_si32(_mm_packus_epi16(a.v, a.v)));
}

template <unsigned int N> XBRZ_SIMD_INLINE __m128i div16(__m128i x) //exact x / N for 0 <= x < 32768
{
    if constexpr ((N & (N - 1)) == 0)
    {
        constexpr int shift = N == 2 ? 1 : N == 4 ? 2 : N == 8 ? 3 : N == 16 ? 4 : N == 32 ? 5 : N == 64 ? 6 : 7;
        static_assert(N == (1u << shift));
        return _mm_srli_epi16(x, shift);
    }
    else
    {
        //q = floor(x * floor(65536 / N) / 65536) is floor(x / N) or one less; fix up by the remainder
        const __m128i q = _mm_mulhi_epu16(x, _mm_set1_epi16(static_cast<short>(65536 / N)));
        const __m128i r = _mm_sub_epi16(x, _mm_mullo_epi16(q, _mm_set1_epi16(static_cast<short>(N))));
        const __m128i fix = _mm_cmpgt_epi16(r, _mm_set1_epi16(static_cast<short>(N - 1))); //-1 where r >= N
        return _mm_sub_epi16(q, fix);
    }
}

//per channel: (front * M + back * (N - M) + N / 2) / N
template <unsigned int M, unsigned int N> XBRZ_SIMD_INLINE Pix16 gradient(Pix16 front, Pix16 back)
{
    static_assert(0 < M && M < N && N <= 128); //255 * N + N / 2 must fit a signed 16-bit lane
    const __m128i num = _mm_add_epi16(_mm_add_epi16(_mm_mullo_epi16(front.v, _mm_set1_epi16(static_cast<short>(M))),
                                                    _mm_mullo_epi16(back.v,  _mm_set1_epi16(static_cast<short>(N - M)))),
                                      _mm_set1_epi16(static_cast<short>(N / 2)));
    return {div16<N>(num)};
}

#elif defined(XBRZ_SIMD_NEON)
//----------------------------------------------------------------------------
// NEON (AArch64)
//----------------------------------------------------------------------------
struct U32x4 { uint32x4_t v; };
struct F32x4 { float32x4_t v; };

XBRZ_SIMD_INLINE U32x4 loadu(const uint32_t* p) { return {vld1q_u32(p)}; }
XBRZ_SIMD_INLINE void storeu(uint32_t* p, U32x4 a) { vst1q_u32(p, a.v); }
XBRZ_SIMD_INLINE void storeu2(uint32_t* p, U32x4 a) { vst1_u32(p, vget_low_u32(a.v)); }

XBRZ_SIMD_INLINE U32x4 set1(uint32_t x) { return {vdupq_n_u32(x)}; }
XBRZ_SIMD_INLINE U32x4 set4(uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{
    //through the general registers; a store to a temporary and a vector load
    //of it would wait on store forwarding that cannot merge four writes
    return {vcombine_u32(vcreate_u32(static_cast<uint64_t>(a) | (static_cast<uint64_t>(b) << 32)),
                         vcreate_u32(static_cast<uint64_t>(c) | (static_cast<uint64_t>(d) << 32)))};
}

XBRZ_SIMD_INLINE U32x4 bitAnd   (U32x4 a, U32x4 b) { return {vandq_u32(a.v, b.v)}; }
XBRZ_SIMD_INLINE U32x4 bitOr    (U32x4 a, U32x4 b) { return {vorrq_u32(a.v, b.v)}; }
XBRZ_SIMD_INLINE U32x4 bitAndNot(U32x4 a, U32x4 b) { return {vbicq_u32(b.v, a.v)}; } //~a & b
XBRZ_SIMD_INLINE U32x4 add      (U32x4 a, U32x4 b) { return {vaddq_u32(a.v, b.v)}; }
XBRZ_SIMD_INLINE U32x4 cmpEq    (U32x4 a, U32x4 b) { return {vceqq_u32(a.v, b.v)}; }

template <int N> XBRZ_SIMD_INLINE U32x4 shr(U32x4 a) { return {vshrq_n_u32(a.v, N)}; }
template <int N> XBRZ_SIMD_INLINE U32x4 shl(U32x4 a) { return {vshlq_n_u32(a.v, N)}; }

XBRZ_SIMD_INLINE F32x4 fset1(float x) { return {vdupq_n_f32(x)}; }
XBRZ_SIMD_INLINE F32x4 floadu(const float* p) { return {vld1q_f32(p)}; }
XBRZ_SIMD_INLINE F32x4 toFloat(U32x4 a) { return {vcvtq_f32_u32(a.v)}; }

XBRZ_SIMD_INLINE F32x4 fadd (F32x4 a, F32x4 b) { return {vaddq_f32(a.v, b.v)}; }
XBRZ_SIMD_INLINE F32x4 fsub (F32x4 a, F32x4 b) { return {vsubq_f32(a.v, b.v)}; }
XBRZ_SIMD_INLINE F32x4 fmul (F32x4 a, F32x4 b) { return {vmulq_f32(a.v, b.v)}; }
XBRZ_SIMD_INLINE F32x4 fmin (F32x4 a, F32x4 b) { return {vminq_f32(a.v, b.v)}; }
XBRZ_SIMD_INLINE F32x4 fsqrt(F32x4 a)          { return {vsqrtq_f32(a.v)}; }
XBRZ_SIMD_INLINE F32x4 fabs (F32x4 a)          { return {vabsq_f32(a.v)}; }

XBRZ_SIMD_INLINE U32x4 fcmpLt(F32x4 a, F32x4 b) { return {vcltq_f32(a.v, b.v)}; }
XBRZ_SIMD_INLINE U32x4 fcmpLe(F32x4 a, F32x4 b) { return {vcleq_f32(a.v, b.v)}; }

XBRZ_SIMD_INLINE bool maskAll (U32x4 m) { return vminvq_u32(m.v) != 0; }
XBRZ_SIMD_INLINE bool maskNone(U32x4 m) { return vmaxvq_u32(m.v) == 0; }

XBRZ_SIMD_INLINE void storeLanes(uint32_t* out, U32x4 a) { vst1q_u32(out, a.v); }
XBRZ_SIMD_INLINE void storeLanes(float* out, F32x4 a) { vst1q_f32(out, a.v); }

template <int i0, int i1, int i2, int i3> XBRZ_SIMD_INLINE U32x4 lanes(U32x4 a)
{
    //one table lookup; the index vector is a constant the compiler keeps in a register
    const uint8_t idx[16] = {uint8_t(4 * i0), uint8_t(4 * i0 + 1), uint8_t(4 * i0 + 2), uint8_t(4 * i0 + 3),
                             uint8_t(4 * i1), uint8_t(4 * i1 + 1), uint8_t(4 * i1 + 2), uint8_t(4 * i1 + 3),
                             uint8_t(4 * i2), uint8_t(4 * i2 + 1), uint8_t(4 * i2 + 2), uint8_t(4 * i2 + 3),
                             uint8_t(4 * i3), uint8_t(4 * i3 + 1), uint8_t(4 * i3 + 2), uint8_t(4 * i3 + 3)};
    return {vreinterpretq_u32_u8(vqtbl1q_u8(vreinterpretq_u8_u32(a.v), vld1q_u8(idx)))};
}

XBRZ_SIMD_INLINE uint32_t packLowBytes(U32x4 a)
{
    const uint16x4_t w = vmovn_u32(a.v);
    return vget_lane_u32(vreinterpret_u32_u8(vmovn_u16(vcombine_u16(w, w))), 0);
}

struct Pix16 { uint16x4_t v; };

XBRZ_SIMD_INLINE Pix16 unpackPixel(uint32_t pix)
{
    return {vget_low_u16(vmovl_u8(vreinterpret_u8_u32(vdup_n_u32(pix))))};
}

XBRZ_SIMD_INLINE uint32_t packPixel(Pix16 a)
{
    return vget_lane_u32(vreinterpret_u32_u8(vqmovn_u16(vcombine_u16(a.v, a.v))), 0);
}

template <unsigned int N> XBRZ_SIMD_INLINE uint16x4_t div16(uint16x4_t x) //exact x / N for 0 <= x < 32768
{
    if constexpr ((N & (N - 1)) == 0)
    {
        constexpr int shift = N == 2 ? 1 : N == 4 ? 2 : N == 8 ? 3 : N == 16 ? 4 : N == 32 ? 5 : N == 64 ? 6 : 7;
        static_assert(N == (1u << shift));
        return vshr_n_u16(x, shift);
    }
    else
    {
        const uint16x4_t q = vshrn_n_u32(vmull_u16(x, vdup_n_u16(static_cast<uint16_t>(65536 / N))), 16);
        const uint16x4_t r = vsub_u16(x, vmul_u16(q, vdup_n_u16(static_cast<uint16_t>(N))));
        const uint16x4_t fix = vcge_u16(r, vdup_n_u16(static_cast<uint16_t>(N))); //0xffff where r >= N
        return vsub_u16(q, fix);
    }
}

template <unsigned int M, unsigned int N> XBRZ_SIMD_INLINE Pix16 gradient(Pix16 front, Pix16 back)
{
    static_assert(0 < M && M < N && N <= 128);
    uint16x4_t num = vmul_n_u16(front.v, static_cast<uint16_t>(M));
    num = vmla_n_u16(num, back.v, static_cast<uint16_t>(N - M));
    num = vadd_u16(num, vdup_n_u16(static_cast<uint16_t>(N / 2)));
    return {div16<N>(num)};
}

#else
//----------------------------------------------------------------------------
// Scalar emulation: same lane semantics, plain loops
//----------------------------------------------------------------------------
struct U32x4 { uint32_t v[4]; };
struct F32x4 { float v[4]; };

XBRZ_SIMD_INLINE U32x4 loadu(const uint32_t* p) { U32x4 r; std::memcpy(r.v, p, sizeof r.v); return r; }
XBRZ_SIMD_INLINE void storeu(uint32_t* p, U32x4 a) { std::memcpy(p, a.v, sizeof a.v); }
XBRZ_SIMD_INLINE void storeu2(uint32_t* p, U32x4 a) { p[0] = a.v[0]; p[1] = a.v[1]; }

XBRZ_SIMD_INLINE U32x4 set1(uint32_t x) { return {{x, x, x, x}}; }
XBRZ_SIMD_INLINE U32x4 set4(uint32_t a, uint32_t b, uint32_t c, uint32_t d) { return {{a, b, c, d}}; }

#define XBRZ_SIMD_LANEWISE(name, T, expr)                            \
    XBRZ_SIMD_INLINE T name(T a, T b)                                \
    {                                                                \
        T r;                                                         \
        for (int i = 0; i < 4; ++i)                                  \
            r.v[i] = (expr);                                         \
        return r;                                                    \
    }

XBRZ_SIMD_LANEWISE(bitAnd,    U32x4, a.v[i] & b.v[i])
XBRZ_SIMD_LANEWISE(bitOr,     U32x4, a.v[i] | b.v[i])
XBRZ_SIMD_LANEWISE(bitAndNot, U32x4, ~a.v[i] & b.v[i])
XBRZ_SIMD_LANEWISE(add,       U32x4, a.v[i] + b.v[i])
XBRZ_SIMD_LANEWISE(cmpEq,     U32x4, a.v[i] == b.v[i] ? 0xffffffffu : 0u)
XBRZ_SIMD_LANEWISE(fadd,      F32x4, a.v[i] + b.v[i])
XBRZ_SIMD_LANEWISE(fsub,      F32x4, a.v[i] - b.v[i])
XBRZ_SIMD_LANEWISE(fmul,      F32x4, a.v[i] * b.v[i])
XBRZ_SIMD_LANEWISE(fmin,      F32x4, b.v[i] < a.v[i] ? b.v[i] : a.v[i])
#undef XBRZ_SIMD_LANEWISE

template <int N> XBRZ_SIMD_INLINE U32x4 shr(U32x4 a) { for (uint32_t& x : a.v) x >>= N; return a; }
template <int N> XBRZ_SIMD_INLINE U32x4 shl(U32x4 a) { for (uint32_t& x : a.v) x <<= N; return a; }

XBRZ_SIMD_INLINE F32x4 fset1(float x) { return {{x, x, x, x}}; }
XBRZ_SIMD_INLINE F32x4 floadu(const float* p) { F32x4 r; std::memcpy(r.v, p, sizeof r.v); return r; }
XBRZ_SIMD_INLINE F32x4 toFloat(U32x4 a) { F32x4 r; for (int i = 0; i < 4; ++i) r.v[i] = static_cast<float>(a.v[i]); return r; }
XBRZ_SIMD_INLINE F32x4 fsqrt(F32x4 a) { for (float& x : a.v) x = std::sqrt(x); return a; }
XBRZ_SIMD_INLINE F32x4 fabs (F32x4 a) { for (float& x : a.v) x = std::fabs(x); return a; }

XBRZ_SIMD_INLINE U32x4 fcmpLt(F32x4 a, F32x4 b) { U32x4 r; for (int i = 0; i < 4; ++i) r.v[i] = a.v[i] <  b.v[i] ? 0xffffffffu : 0u; return r; }
XBRZ_SIMD_INLINE U32x4 fcmpLe(F32x4 a, F32x4 b) { U32x4 r; for (int i = 0; i < 4; ++i) r.v[i] = a.v[i] <= b.v[i] ? 0xffffffffu : 0u; return r; }

XBRZ_SIMD_INLINE bool maskAll (U32x4 m) { return (m.v[0] & m.v[1] & m.v[2] & m.v[3]) >> 31; }
XBRZ_SIMD_INLINE bool maskNone(U32x4 m) { return !((m.v[0] | m.v[1] | m.v[2] | m.v[3]) >> 31); }

XBRZ_SIMD_INLINE void storeLanes(uint32_t* out, U32x4 a) { storeu(out, a); }
XBRZ_SIMD_INLINE void storeLanes(float* out, F32x4 a) { std::memcpy(out, a.v, sizeof a.v); }

template <int i0, int i1, int i2, int i3> XBRZ_SIMD_INLINE U32x4 lanes(U32x4 a)
{
    return {{a.v[i0], a.v[i1], a.v[i2], a.v[i3]}};
}

XBRZ_SIMD_INLINE uint32_t packLowBytes(U32x4 a)
{
    return (a.v[0] & 0xff) | ((a.v[1] & 0xff) << 8) | ((a.v[2] & 0xff) << 16) | ((a.v[3] & 0xff) << 24);
}

struct Pix16 { uint16_t v[4]; };

XBRZ_SIMD_INLINE Pix16 unpackPixel(uint32_t pix)
{
    return {{static_cast<uint16_t>(pix & 0xff), static_cast<uint16_t>((pix >> 8) & 0xff),
             static_cast<uint16_t>((pix >> 16) & 0xff), static_cast<uint16_t>(pix >> 24)}};
}

XBRZ_SIMD_INLINE uint32_t packPixel(Pix16 a)
{
    auto sat = [](uint16_t x) { return static_cast<uint32_t>(x > 255 ? 255 : x); };
    return sat(a.v[0]) | (sat(a.v[1]) << 8) | (sat(a.v[2]) << 16) | (sat(a.v[3]) << 24);
}

template <unsigned int M, unsigned int N> XBRZ_SIMD_INLINE Pix16 gradient(Pix16 front, Pix16 back)
{
    static_assert(0 < M && M < N && N <= 128);
    Pix16 r;
    for (int i = 0; i < 4; ++i)
        r.v[i] = static_cast<uint16_t>((front.v[i] * M + back.v[i] * (N - M) + N / 2) / N);
    return r;
}
#endif
}
}

#endif //XBRZ_SIMD_H_3C1E7A4F9B2D6085
