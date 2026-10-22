// 128-bit vector layer for the display filters: SSE2 on x86, NEON on AArch64,
// a lane-by-lane scalar emulation everywhere else.
//
// Every filter kernel in src/components/filters*, is written once against the
// operations below. They follow SSE2 semantics exactly (lane widths, saturation,
// rounding of avg_u8, byte order of packs and unpacks), so a kernel produces the
// same bytes on all three backends; the scalar backend is what the unit test
// compares the SIMD build against (ForceScalar()).
//
//   VBAM_SIMD_SSE2   x86-64, and x86 with SSE2 enabled -- not the Windows XP
//                    build, which targets pre-SSE2 CPUs
//   VBAM_SIMD_NEON   AArch64 only; 32-bit ARM lacks several of the instructions
//                    used (vsqrtq_f32, vminvq, tbl on 128-bit tables)
//   VBAM_SIMD_SCALAR everything else, or any build with VBAM_SIMD_FORCE_SCALAR
//
// Vector values are passed and returned by value; all functions are inline.
// `V` is a 128-bit integer register viewed as 16 x u8, 8 x u16, 4 x u32 or
// 2 x u64 depending on the operation; `VF` is 4 x float.

#ifndef VBAM_COMPONENTS_FILTERS_SIMD128_H_
#define VBAM_COMPONENTS_FILTERS_SIMD128_H_

#include <cmath>
#include <cstdint>
#include <cstring>

#if !defined(VBAM_SIMD_FORCE_SCALAR) && !defined(__DJGPP__) && !defined(WINXP) && \
    (defined(__SSE2__) || defined(_M_X64) || defined(_M_AMD64) || \
     (defined(_M_IX86_FP) && _M_IX86_FP >= 2))
#define VBAM_SIMD_SSE2 1
#include <emmintrin.h>
#elif !defined(VBAM_SIMD_FORCE_SCALAR) && \
    (defined(__aarch64__) || defined(_M_ARM64) || defined(_M_ARM64EC))
#define VBAM_SIMD_NEON 1
#include <arm_neon.h>
#else
#define VBAM_SIMD_SCALAR 1
#endif

#ifdef _MSC_VER
#define VBAM_SIMD_INLINE __forceinline
#elif defined(__GNUC__)
#define VBAM_SIMD_INLINE __attribute__((always_inline)) inline
#else
#define VBAM_SIMD_INLINE inline
#endif

namespace vbam_simd {

// ---------------------------------------------------------------------------
// Dispatch
// ---------------------------------------------------------------------------

#if defined(VBAM_SIMD_SCALAR)
constexpr bool kHaveSimd = false;
#else
constexpr bool kHaveSimd = true;
#endif

// Tests set this to run the scalar kernels on a SIMD build and compare.
inline bool g_force_scalar = false;

inline void ForceScalar(bool force) { g_force_scalar = force; }

// True when the vector kernels should run: a SIMD backend is compiled in and
// nothing asked for the scalar kernels. On a scalar build this folds to false
// and the vector kernels are dead code.
VBAM_SIMD_INLINE bool Enabled() { return kHaveSimd && !g_force_scalar; }

inline const char* BackendName() {
#if defined(VBAM_SIMD_SSE2)
    return "SSE2";
#elif defined(VBAM_SIMD_NEON)
    return "NEON";
#else
    return "scalar";
#endif
}

// Shift count helper for the variable-shift functions
VBAM_SIMD_INLINE int ClampShift(int n, int width) { return n < 0 ? 0 : n > width ? width : n; }

#if defined(VBAM_SIMD_SSE2)
// ===========================================================================
// SSE2
// ===========================================================================
struct V { __m128i v; };
struct VF { __m128 v; };

VBAM_SIMD_INLINE V load(const void* p) { return {_mm_loadu_si128(static_cast<const __m128i*>(p))}; }
VBAM_SIMD_INLINE void store(void* p, V a) { _mm_storeu_si128(static_cast<__m128i*>(p), a.v); }
VBAM_SIMD_INLINE V load64(const void* p) { return {_mm_loadl_epi64(static_cast<const __m128i*>(p))}; }
VBAM_SIMD_INLINE void store64(void* p, V a) { _mm_storel_epi64(static_cast<__m128i*>(p), a.v); }
VBAM_SIMD_INLINE V load32(const void* p) { uint32_t x; std::memcpy(&x, p, 4); return {_mm_cvtsi32_si128(static_cast<int>(x))}; }
VBAM_SIMD_INLINE void store32(void* p, V a) { const uint32_t x = static_cast<uint32_t>(_mm_cvtsi128_si32(a.v)); std::memcpy(p, &x, 4); }

VBAM_SIMD_INLINE V zero() { return {_mm_setzero_si128()}; }
VBAM_SIMD_INLINE V set1_u8(uint8_t x) { return {_mm_set1_epi8(static_cast<char>(x))}; }
VBAM_SIMD_INLINE V set1_u16(uint16_t x) { return {_mm_set1_epi16(static_cast<short>(x))}; }
VBAM_SIMD_INLINE V set1_u32(uint32_t x) { return {_mm_set1_epi32(static_cast<int>(x))}; }
VBAM_SIMD_INLINE V set_u32(uint32_t a, uint32_t b, uint32_t c, uint32_t d) //lane 0 = a
{
    return {_mm_setr_epi32(static_cast<int>(a), static_cast<int>(b), static_cast<int>(c), static_cast<int>(d))};
}
VBAM_SIMD_INLINE V from_u32(uint32_t x) { return {_mm_cvtsi32_si128(static_cast<int>(x))}; } //lane 0, rest zero
VBAM_SIMD_INLINE uint32_t to_u32(V a) { return static_cast<uint32_t>(_mm_cvtsi128_si32(a.v)); } //lane 0

VBAM_SIMD_INLINE V and_(V a, V b) { return {_mm_and_si128(a.v, b.v)}; }
VBAM_SIMD_INLINE V or_(V a, V b) { return {_mm_or_si128(a.v, b.v)}; }
VBAM_SIMD_INLINE V xor_(V a, V b) { return {_mm_xor_si128(a.v, b.v)}; }
VBAM_SIMD_INLINE V andnot(V a, V b) { return {_mm_andnot_si128(a.v, b.v)}; } //~a & b

VBAM_SIMD_INLINE V add_u8(V a, V b) { return {_mm_add_epi8(a.v, b.v)}; }
VBAM_SIMD_INLINE V add_u16(V a, V b) { return {_mm_add_epi16(a.v, b.v)}; }
VBAM_SIMD_INLINE V add_u32(V a, V b) { return {_mm_add_epi32(a.v, b.v)}; }
VBAM_SIMD_INLINE V sub_u8(V a, V b) { return {_mm_sub_epi8(a.v, b.v)}; }
VBAM_SIMD_INLINE V sub_u16(V a, V b) { return {_mm_sub_epi16(a.v, b.v)}; }
VBAM_SIMD_INLINE V sub_u32(V a, V b) { return {_mm_sub_epi32(a.v, b.v)}; }
VBAM_SIMD_INLINE V adds_u8(V a, V b) { return {_mm_adds_epu8(a.v, b.v)}; }
VBAM_SIMD_INLINE V subs_u8(V a, V b) { return {_mm_subs_epu8(a.v, b.v)}; }
VBAM_SIMD_INLINE V subs_u16(V a, V b) { return {_mm_subs_epu16(a.v, b.v)}; }
VBAM_SIMD_INLINE V avg_u8(V a, V b) { return {_mm_avg_epu8(a.v, b.v)}; } //(a + b + 1) >> 1
VBAM_SIMD_INLINE V avg_u16(V a, V b) { return {_mm_avg_epu16(a.v, b.v)}; }
VBAM_SIMD_INLINE V min_u8(V a, V b) { return {_mm_min_epu8(a.v, b.v)}; }
VBAM_SIMD_INLINE V max_u8(V a, V b) { return {_mm_max_epu8(a.v, b.v)}; }
VBAM_SIMD_INLINE V min_u16(V a, V b) { return {_mm_sub_epi16(a.v, _mm_subs_epu16(a.v, b.v))}; }
VBAM_SIMD_INLINE V max_u16(V a, V b) { return {_mm_add_epi16(b.v, _mm_subs_epu16(a.v, b.v))}; }
VBAM_SIMD_INLINE V mullo_u16(V a, V b) { return {_mm_mullo_epi16(a.v, b.v)}; }
VBAM_SIMD_INLINE V mulhi_u16(V a, V b) { return {_mm_mulhi_epu16(a.v, b.v)}; }

template <int N> VBAM_SIMD_INLINE V srl_u16(V a) { return {_mm_srli_epi16(a.v, N)}; }
template <int N> VBAM_SIMD_INLINE V sll_u16(V a) { return {_mm_slli_epi16(a.v, N)}; }
template <int N> VBAM_SIMD_INLINE V srl_u32(V a) { return {_mm_srli_epi32(a.v, N)}; }
template <int N> VBAM_SIMD_INLINE V sll_u32(V a) { return {_mm_slli_epi32(a.v, N)}; }
template <int N> VBAM_SIMD_INLINE V sra_s16(V a) { return {_mm_srai_epi16(a.v, N)}; }
template <int N> VBAM_SIMD_INLINE V sra_s32(V a) { return {_mm_srai_epi32(a.v, N)}; }
VBAM_SIMD_INLINE V srl_u16_var(V a, int n) { return {_mm_srl_epi16(a.v, _mm_cvtsi32_si128(ClampShift(n, 16)))}; }
VBAM_SIMD_INLINE V sll_u16_var(V a, int n) { return {_mm_sll_epi16(a.v, _mm_cvtsi32_si128(ClampShift(n, 16)))}; }
VBAM_SIMD_INLINE V srl_u32_var(V a, int n) { return {_mm_srl_epi32(a.v, _mm_cvtsi32_si128(ClampShift(n, 32)))}; }
VBAM_SIMD_INLINE V sll_u32_var(V a, int n) { return {_mm_sll_epi32(a.v, _mm_cvtsi32_si128(ClampShift(n, 32)))}; }
template <int N> VBAM_SIMD_INLINE V bsrl(V a) { return {_mm_srli_si128(a.v, N)}; } //whole register, by N bytes, zero fill
template <int N> VBAM_SIMD_INLINE V bsll(V a) { return {_mm_slli_si128(a.v, N)}; }

VBAM_SIMD_INLINE V cmpeq_u8(V a, V b) { return {_mm_cmpeq_epi8(a.v, b.v)}; }
VBAM_SIMD_INLINE V cmpeq_u16(V a, V b) { return {_mm_cmpeq_epi16(a.v, b.v)}; }
VBAM_SIMD_INLINE V cmpeq_u32(V a, V b) { return {_mm_cmpeq_epi32(a.v, b.v)}; }
VBAM_SIMD_INLINE V cmpgt_s16(V a, V b) { return {_mm_cmpgt_epi16(a.v, b.v)}; }
VBAM_SIMD_INLINE V cmpgt_s32(V a, V b) { return {_mm_cmpgt_epi32(a.v, b.v)}; }

VBAM_SIMD_INLINE V unpacklo_u8(V a, V b) { return {_mm_unpacklo_epi8(a.v, b.v)}; }
VBAM_SIMD_INLINE V unpackhi_u8(V a, V b) { return {_mm_unpackhi_epi8(a.v, b.v)}; }
VBAM_SIMD_INLINE V unpacklo_u16(V a, V b) { return {_mm_unpacklo_epi16(a.v, b.v)}; }
VBAM_SIMD_INLINE V unpackhi_u16(V a, V b) { return {_mm_unpackhi_epi16(a.v, b.v)}; }
VBAM_SIMD_INLINE V unpacklo_u32(V a, V b) { return {_mm_unpacklo_epi32(a.v, b.v)}; }
VBAM_SIMD_INLINE V unpackhi_u32(V a, V b) { return {_mm_unpackhi_epi32(a.v, b.v)}; }
VBAM_SIMD_INLINE V unpacklo_u64(V a, V b) { return {_mm_unpacklo_epi64(a.v, b.v)}; }
VBAM_SIMD_INLINE V unpackhi_u64(V a, V b) { return {_mm_unpackhi_epi64(a.v, b.v)}; }

template <int i0, int i1, int i2, int i3> VBAM_SIMD_INLINE V shuffle_u32(V a)
{
    return {_mm_shuffle_epi32(a.v, _MM_SHUFFLE(i3, i2, i1, i0))};
}

//saturating pack of two vectors of 8 x i16 into 16 x u8
VBAM_SIMD_INLINE V packus_s16_u8(V a, V b) { return {_mm_packus_epi16(a.v, b.v)}; }
//pack of two vectors of 4 x u32 into 8 x u16; exact for lanes <= 0xffff
VBAM_SIMD_INLINE V pack_u32_u16(V a, V b)
{
    const __m128i bias = _mm_set1_epi32(0x8000);
    const __m128i p = _mm_packs_epi32(_mm_sub_epi32(a.v, bias), _mm_sub_epi32(b.v, bias));
    return {_mm_xor_si128(p, _mm_set1_epi16(static_cast<short>(0x8000)))};
}

VBAM_SIMD_INLINE int movemask_u8(V a) { return _mm_movemask_epi8(a.v); } //bit i = top bit of byte i
VBAM_SIMD_INLINE bool all_zero(V a) { return _mm_movemask_epi8(_mm_cmpeq_epi8(a.v, _mm_setzero_si128())) == 0xffff; }
VBAM_SIMD_INLINE bool all_ones(V a) { return _mm_movemask_epi8(a.v) == 0xffff; } //top bit of every byte: a mask of any lane width
VBAM_SIMD_INLINE bool all_ones_u32(V a) { return _mm_movemask_ps(_mm_castsi128_ps(a.v)) == 0xf; } //top bit of every u32 lane
VBAM_SIMD_INLINE bool any_u32(V a) { return _mm_movemask_ps(_mm_castsi128_ps(a.v)) != 0; }

VBAM_SIMD_INLINE V vselect(V mask, V a, V b) { return {_mm_or_si128(_mm_and_si128(mask.v, a.v), _mm_andnot_si128(mask.v, b.v))}; }

template <int i> VBAM_SIMD_INLINE uint32_t lane_u32(V a) { return static_cast<uint32_t>(_mm_cvtsi128_si32(_mm_shuffle_epi32(a.v, _MM_SHUFFLE(i, i, i, i)))); }

// interleaved stores: a0 b0 a1 b1 ... (8 x u32 / 16 x u16 at p)
VBAM_SIMD_INLINE void store2_u32(uint32_t* p, V a, V b)
{
    _mm_storeu_si128(reinterpret_cast<__m128i*>(p), _mm_unpacklo_epi32(a.v, b.v));
    _mm_storeu_si128(reinterpret_cast<__m128i*>(p + 4), _mm_unpackhi_epi32(a.v, b.v));
}
VBAM_SIMD_INLINE void store2_u16(uint16_t* p, V a, V b)
{
    _mm_storeu_si128(reinterpret_cast<__m128i*>(p), _mm_unpacklo_epi16(a.v, b.v));
    _mm_storeu_si128(reinterpret_cast<__m128i*>(p + 8), _mm_unpackhi_epi16(a.v, b.v));
}
// every lane repeated n times: a0 a0 a1 a1 ... (4n x u32 / 8n x u16 at p)
VBAM_SIMD_INLINE void store_rep2_u32(uint32_t* p, V a) { store2_u32(p, a, a); }
VBAM_SIMD_INLINE void store_rep3_u32(uint32_t* p, V a)
{
    _mm_storeu_si128(reinterpret_cast<__m128i*>(p), _mm_shuffle_epi32(a.v, _MM_SHUFFLE(1, 0, 0, 0)));
    _mm_storeu_si128(reinterpret_cast<__m128i*>(p + 4), _mm_shuffle_epi32(a.v, _MM_SHUFFLE(2, 2, 1, 1)));
    _mm_storeu_si128(reinterpret_cast<__m128i*>(p + 8), _mm_shuffle_epi32(a.v, _MM_SHUFFLE(3, 3, 3, 2)));
}
VBAM_SIMD_INLINE void store_rep4_u32(uint32_t* p, V a)
{
    _mm_storeu_si128(reinterpret_cast<__m128i*>(p), _mm_shuffle_epi32(a.v, _MM_SHUFFLE(0, 0, 0, 0)));
    _mm_storeu_si128(reinterpret_cast<__m128i*>(p + 4), _mm_shuffle_epi32(a.v, _MM_SHUFFLE(1, 1, 1, 1)));
    _mm_storeu_si128(reinterpret_cast<__m128i*>(p + 8), _mm_shuffle_epi32(a.v, _MM_SHUFFLE(2, 2, 2, 2)));
    _mm_storeu_si128(reinterpret_cast<__m128i*>(p + 12), _mm_shuffle_epi32(a.v, _MM_SHUFFLE(3, 3, 3, 3)));
}
VBAM_SIMD_INLINE void store_rep2_u16(uint16_t* p, V a) { store2_u16(p, a, a); }
VBAM_SIMD_INLINE void store_rep3_u16(uint16_t* p, V a)
{
    //widen to u32, repeat, pack: exact for 16-bit lanes
    const __m128i z = _mm_setzero_si128();
    const __m128i lo = _mm_unpacklo_epi16(a.v, z), hi = _mm_unpackhi_epi16(a.v, z);
    uint32_t tmp[24];
    store_rep3_u32(tmp, V{lo});
    store_rep3_u32(tmp + 12, V{hi});
    for (int i = 0; i < 3; i++)
    {
        const __m128i w0 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(tmp + 8 * i));
        const __m128i w1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(tmp + 8 * i + 4));
        _mm_storeu_si128(reinterpret_cast<__m128i*>(p + 8 * i), pack_u32_u16(V{w0}, V{w1}).v);
    }
}
VBAM_SIMD_INLINE void store_rep4_u16(uint16_t* p, V a)
{
    const __m128i z = _mm_setzero_si128();
    const __m128i lo = _mm_unpacklo_epi16(a.v, z), hi = _mm_unpackhi_epi16(a.v, z);
    uint32_t tmp[32];
    store_rep4_u32(tmp, V{lo});
    store_rep4_u32(tmp + 16, V{hi});
    for (int i = 0; i < 4; i++)
    {
        const __m128i w0 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(tmp + 8 * i));
        const __m128i w1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(tmp + 8 * i + 4));
        _mm_storeu_si128(reinterpret_cast<__m128i*>(p + 8 * i), pack_u32_u16(V{w0}, V{w1}).v);
    }
}

// floats
VBAM_SIMD_INLINE VF fload(const float* p) { return {_mm_loadu_ps(p)}; }
VBAM_SIMD_INLINE void fstore(float* p, VF a) { _mm_storeu_ps(p, a.v); }
VBAM_SIMD_INLINE VF fzero() { return {_mm_setzero_ps()}; }
VBAM_SIMD_INLINE VF fset1(float x) { return {_mm_set1_ps(x)}; }
VBAM_SIMD_INLINE VF fset(float a, float b, float c, float d) { return {_mm_setr_ps(a, b, c, d)}; }
VBAM_SIMD_INLINE VF fadd(VF a, VF b) { return {_mm_add_ps(a.v, b.v)}; }
VBAM_SIMD_INLINE VF fsub(VF a, VF b) { return {_mm_sub_ps(a.v, b.v)}; }
VBAM_SIMD_INLINE VF fmul(VF a, VF b) { return {_mm_mul_ps(a.v, b.v)}; }
VBAM_SIMD_INLINE VF fdiv(VF a, VF b) { return {_mm_div_ps(a.v, b.v)}; }
VBAM_SIMD_INLINE VF fmin(VF a, VF b) { return {_mm_min_ps(a.v, b.v)}; }
VBAM_SIMD_INLINE VF fmax(VF a, VF b) { return {_mm_max_ps(a.v, b.v)}; }
VBAM_SIMD_INLINE VF fsqrt(VF a) { return {_mm_sqrt_ps(a.v)}; }
VBAM_SIMD_INLINE VF fabs(VF a) { return {_mm_andnot_ps(_mm_set1_ps(-0.0f), a.v)}; }
VBAM_SIMD_INLINE V fcmplt(VF a, VF b) { return {_mm_castps_si128(_mm_cmplt_ps(a.v, b.v))}; }
VBAM_SIMD_INLINE V fcmple(VF a, VF b) { return {_mm_castps_si128(_mm_cmple_ps(a.v, b.v))}; }
VBAM_SIMD_INLINE V fcmpgt(VF a, VF b) { return {_mm_castps_si128(_mm_cmpgt_ps(a.v, b.v))}; }
VBAM_SIMD_INLINE V fcmpge(VF a, VF b) { return {_mm_castps_si128(_mm_cmpge_ps(a.v, b.v))}; }
VBAM_SIMD_INLINE VF fselect(V mask, VF a, VF b) { return {_mm_or_ps(_mm_and_ps(_mm_castsi128_ps(mask.v), a.v), _mm_andnot_ps(_mm_castsi128_ps(mask.v), b.v))}; }
VBAM_SIMD_INLINE VF fand(VF a, V mask) { return {_mm_and_ps(a.v, _mm_castsi128_ps(mask.v))}; }
VBAM_SIMD_INLINE VF cvt_s32_f(V a) { return {_mm_cvtepi32_ps(a.v)}; }
VBAM_SIMD_INLINE V cvt_f_s32(VF a) { return {_mm_cvttps_epi32(a.v)}; } //truncate toward zero
VBAM_SIMD_INLINE V fbits(VF a) { return {_mm_castps_si128(a.v)}; }
VBAM_SIMD_INLINE VF ffrombits(V a) { return {_mm_castsi128_ps(a.v)}; }

#elif defined(VBAM_SIMD_NEON)
// ===========================================================================
// NEON (AArch64)
// ===========================================================================
struct V { uint8x16_t v; };
struct VF { float32x4_t v; };

#define VBAM_U16(a) vreinterpretq_u16_u8((a).v)
#define VBAM_U32(a) vreinterpretq_u32_u8((a).v)
#define VBAM_U64(a) vreinterpretq_u64_u8((a).v)
#define VBAM_S16(a) vreinterpretq_s16_u8((a).v)
#define VBAM_S32(a) vreinterpretq_s32_u8((a).v)
#define VBAM_FROM_U16(x) V{vreinterpretq_u8_u16(x)}
#define VBAM_FROM_U32(x) V{vreinterpretq_u8_u32(x)}
#define VBAM_FROM_U64(x) V{vreinterpretq_u8_u64(x)}
#define VBAM_FROM_S16(x) V{vreinterpretq_u8_s16(x)}
#define VBAM_FROM_S32(x) V{vreinterpretq_u8_s32(x)}

VBAM_SIMD_INLINE V load(const void* p) { return {vld1q_u8(static_cast<const uint8_t*>(p))}; }
VBAM_SIMD_INLINE void store(void* p, V a) { vst1q_u8(static_cast<uint8_t*>(p), a.v); }
VBAM_SIMD_INLINE V load64(const void* p) { return {vcombine_u8(vld1_u8(static_cast<const uint8_t*>(p)), vdup_n_u8(0))}; }
VBAM_SIMD_INLINE void store64(void* p, V a) { vst1_u8(static_cast<uint8_t*>(p), vget_low_u8(a.v)); }
VBAM_SIMD_INLINE V load32(const void* p) { uint32_t x; std::memcpy(&x, p, 4); return VBAM_FROM_U32(vsetq_lane_u32(x, vdupq_n_u32(0), 0)); }
VBAM_SIMD_INLINE void store32(void* p, V a) { const uint32_t x = vgetq_lane_u32(VBAM_U32(a), 0); std::memcpy(p, &x, 4); }

VBAM_SIMD_INLINE V zero() { return {vdupq_n_u8(0)}; }
VBAM_SIMD_INLINE V set1_u8(uint8_t x) { return {vdupq_n_u8(x)}; }
VBAM_SIMD_INLINE V set1_u16(uint16_t x) { return VBAM_FROM_U16(vdupq_n_u16(x)); }
VBAM_SIMD_INLINE V set1_u32(uint32_t x) { return VBAM_FROM_U32(vdupq_n_u32(x)); }
VBAM_SIMD_INLINE V set_u32(uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{
    return VBAM_FROM_U32(vcombine_u32(vcreate_u32(static_cast<uint64_t>(a) | (static_cast<uint64_t>(b) << 32)),
                                      vcreate_u32(static_cast<uint64_t>(c) | (static_cast<uint64_t>(d) << 32))));
}
VBAM_SIMD_INLINE V from_u32(uint32_t x) { return VBAM_FROM_U32(vsetq_lane_u32(x, vdupq_n_u32(0), 0)); }
VBAM_SIMD_INLINE uint32_t to_u32(V a) { return vgetq_lane_u32(VBAM_U32(a), 0); }

VBAM_SIMD_INLINE V and_(V a, V b) { return {vandq_u8(a.v, b.v)}; }
VBAM_SIMD_INLINE V or_(V a, V b) { return {vorrq_u8(a.v, b.v)}; }
VBAM_SIMD_INLINE V xor_(V a, V b) { return {veorq_u8(a.v, b.v)}; }
VBAM_SIMD_INLINE V andnot(V a, V b) { return {vbicq_u8(b.v, a.v)}; }

VBAM_SIMD_INLINE V add_u8(V a, V b) { return {vaddq_u8(a.v, b.v)}; }
VBAM_SIMD_INLINE V add_u16(V a, V b) { return VBAM_FROM_U16(vaddq_u16(VBAM_U16(a), VBAM_U16(b))); }
VBAM_SIMD_INLINE V add_u32(V a, V b) { return VBAM_FROM_U32(vaddq_u32(VBAM_U32(a), VBAM_U32(b))); }
VBAM_SIMD_INLINE V sub_u8(V a, V b) { return {vsubq_u8(a.v, b.v)}; }
VBAM_SIMD_INLINE V sub_u16(V a, V b) { return VBAM_FROM_U16(vsubq_u16(VBAM_U16(a), VBAM_U16(b))); }
VBAM_SIMD_INLINE V sub_u32(V a, V b) { return VBAM_FROM_U32(vsubq_u32(VBAM_U32(a), VBAM_U32(b))); }
VBAM_SIMD_INLINE V adds_u8(V a, V b) { return {vqaddq_u8(a.v, b.v)}; }
VBAM_SIMD_INLINE V subs_u8(V a, V b) { return {vqsubq_u8(a.v, b.v)}; }
VBAM_SIMD_INLINE V subs_u16(V a, V b) { return VBAM_FROM_U16(vqsubq_u16(VBAM_U16(a), VBAM_U16(b))); }
VBAM_SIMD_INLINE V avg_u8(V a, V b) { return {vrhaddq_u8(a.v, b.v)}; }
VBAM_SIMD_INLINE V avg_u16(V a, V b) { return VBAM_FROM_U16(vrhaddq_u16(VBAM_U16(a), VBAM_U16(b))); }
VBAM_SIMD_INLINE V min_u8(V a, V b) { return {vminq_u8(a.v, b.v)}; }
VBAM_SIMD_INLINE V max_u8(V a, V b) { return {vmaxq_u8(a.v, b.v)}; }
VBAM_SIMD_INLINE V min_u16(V a, V b) { return VBAM_FROM_U16(vminq_u16(VBAM_U16(a), VBAM_U16(b))); }
VBAM_SIMD_INLINE V max_u16(V a, V b) { return VBAM_FROM_U16(vmaxq_u16(VBAM_U16(a), VBAM_U16(b))); }
VBAM_SIMD_INLINE V mullo_u16(V a, V b) { return VBAM_FROM_U16(vmulq_u16(VBAM_U16(a), VBAM_U16(b))); }
VBAM_SIMD_INLINE V mulhi_u16(V a, V b)
{
    const uint32x4_t lo = vmull_u16(vget_low_u16(VBAM_U16(a)), vget_low_u16(VBAM_U16(b)));
    const uint32x4_t hi = vmull_u16(vget_high_u16(VBAM_U16(a)), vget_high_u16(VBAM_U16(b)));
    return VBAM_FROM_U16(vuzp2q_u16(vreinterpretq_u16_u32(lo), vreinterpretq_u16_u32(hi)));
}

template <int N> VBAM_SIMD_INLINE V srl_u16(V a) { if constexpr (N == 0) return a; else return VBAM_FROM_U16(vshrq_n_u16(VBAM_U16(a), N)); }
template <int N> VBAM_SIMD_INLINE V sll_u16(V a) { if constexpr (N == 0) return a; else return VBAM_FROM_U16(vshlq_n_u16(VBAM_U16(a), N)); }
template <int N> VBAM_SIMD_INLINE V srl_u32(V a) { if constexpr (N == 0) return a; else return VBAM_FROM_U32(vshrq_n_u32(VBAM_U32(a), N)); }
template <int N> VBAM_SIMD_INLINE V sll_u32(V a) { if constexpr (N == 0) return a; else return VBAM_FROM_U32(vshlq_n_u32(VBAM_U32(a), N)); }
template <int N> VBAM_SIMD_INLINE V sra_s16(V a) { if constexpr (N == 0) return a; else return VBAM_FROM_S16(vshrq_n_s16(VBAM_S16(a), N)); }
template <int N> VBAM_SIMD_INLINE V sra_s32(V a) { if constexpr (N == 0) return a; else return VBAM_FROM_S32(vshrq_n_s32(VBAM_S32(a), N)); }
VBAM_SIMD_INLINE V srl_u16_var(V a, int n)
{
    n = ClampShift(n, 16);
    if (n == 16) return zero();
    return VBAM_FROM_U16(vshlq_u16(VBAM_U16(a), vdupq_n_s16(static_cast<int16_t>(-n))));
}
VBAM_SIMD_INLINE V sll_u16_var(V a, int n)
{
    n = ClampShift(n, 16);
    if (n == 16) return zero();
    return VBAM_FROM_U16(vshlq_u16(VBAM_U16(a), vdupq_n_s16(static_cast<int16_t>(n))));
}
VBAM_SIMD_INLINE V srl_u32_var(V a, int n)
{
    n = ClampShift(n, 32);
    if (n == 32) return zero();
    return VBAM_FROM_U32(vshlq_u32(VBAM_U32(a), vdupq_n_s32(-n)));
}
VBAM_SIMD_INLINE V sll_u32_var(V a, int n)
{
    n = ClampShift(n, 32);
    if (n == 32) return zero();
    return VBAM_FROM_U32(vshlq_u32(VBAM_U32(a), vdupq_n_s32(n)));
}
template <int N> VBAM_SIMD_INLINE V bsrl(V a)
{
    if constexpr (N == 0) return a;
    else if constexpr (N >= 16) return zero();
    else return {vextq_u8(a.v, vdupq_n_u8(0), N)};
}
template <int N> VBAM_SIMD_INLINE V bsll(V a)
{
    if constexpr (N == 0) return a;
    else if constexpr (N >= 16) return zero();
    else return {vextq_u8(vdupq_n_u8(0), a.v, 16 - N)};
}

VBAM_SIMD_INLINE V cmpeq_u8(V a, V b) { return {vceqq_u8(a.v, b.v)}; }
VBAM_SIMD_INLINE V cmpeq_u16(V a, V b) { return VBAM_FROM_U16(vceqq_u16(VBAM_U16(a), VBAM_U16(b))); }
VBAM_SIMD_INLINE V cmpeq_u32(V a, V b) { return VBAM_FROM_U32(vceqq_u32(VBAM_U32(a), VBAM_U32(b))); }
VBAM_SIMD_INLINE V cmpgt_s16(V a, V b) { return VBAM_FROM_U16(vcgtq_s16(VBAM_S16(a), VBAM_S16(b))); }
VBAM_SIMD_INLINE V cmpgt_s32(V a, V b) { return VBAM_FROM_U32(vcgtq_s32(VBAM_S32(a), VBAM_S32(b))); }

VBAM_SIMD_INLINE V unpacklo_u8(V a, V b) { return {vzip1q_u8(a.v, b.v)}; }
VBAM_SIMD_INLINE V unpackhi_u8(V a, V b) { return {vzip2q_u8(a.v, b.v)}; }
VBAM_SIMD_INLINE V unpacklo_u16(V a, V b) { return VBAM_FROM_U16(vzip1q_u16(VBAM_U16(a), VBAM_U16(b))); }
VBAM_SIMD_INLINE V unpackhi_u16(V a, V b) { return VBAM_FROM_U16(vzip2q_u16(VBAM_U16(a), VBAM_U16(b))); }
VBAM_SIMD_INLINE V unpacklo_u32(V a, V b) { return VBAM_FROM_U32(vzip1q_u32(VBAM_U32(a), VBAM_U32(b))); }
VBAM_SIMD_INLINE V unpackhi_u32(V a, V b) { return VBAM_FROM_U32(vzip2q_u32(VBAM_U32(a), VBAM_U32(b))); }
VBAM_SIMD_INLINE V unpacklo_u64(V a, V b) { return VBAM_FROM_U64(vzip1q_u64(VBAM_U64(a), VBAM_U64(b))); }
VBAM_SIMD_INLINE V unpackhi_u64(V a, V b) { return VBAM_FROM_U64(vzip2q_u64(VBAM_U64(a), VBAM_U64(b))); }

template <int i0, int i1, int i2, int i3> VBAM_SIMD_INLINE V shuffle_u32(V a)
{
    const uint8_t idx[16] = {uint8_t(4 * i0), uint8_t(4 * i0 + 1), uint8_t(4 * i0 + 2), uint8_t(4 * i0 + 3),
                             uint8_t(4 * i1), uint8_t(4 * i1 + 1), uint8_t(4 * i1 + 2), uint8_t(4 * i1 + 3),
                             uint8_t(4 * i2), uint8_t(4 * i2 + 1), uint8_t(4 * i2 + 2), uint8_t(4 * i2 + 3),
                             uint8_t(4 * i3), uint8_t(4 * i3 + 1), uint8_t(4 * i3 + 2), uint8_t(4 * i3 + 3)};
    return {vqtbl1q_u8(a.v, vld1q_u8(idx))};
}

VBAM_SIMD_INLINE V packus_s16_u8(V a, V b) { return {vcombine_u8(vqmovun_s16(VBAM_S16(a)), vqmovun_s16(VBAM_S16(b)))}; }
VBAM_SIMD_INLINE V pack_u32_u16(V a, V b) { return VBAM_FROM_U16(vcombine_u16(vmovn_u32(VBAM_U32(a)), vmovn_u32(VBAM_U32(b)))); }

VBAM_SIMD_INLINE int movemask_u8(V a)
{
    //top bit of each byte, weighted by its position within the 8-byte half, summed per half
    const uint8x16_t bits = vshrq_n_u8(a.v, 7);
    const uint8_t weights[16] = {1, 2, 4, 8, 16, 32, 64, 128, 1, 2, 4, 8, 16, 32, 64, 128};
    const uint8x16_t w = vmulq_u8(bits, vld1q_u8(weights));
    return vaddv_u8(vget_low_u8(w)) | (vaddv_u8(vget_high_u8(w)) << 8);
}
VBAM_SIMD_INLINE bool all_zero(V a) { return vmaxvq_u8(a.v) == 0; }
VBAM_SIMD_INLINE bool all_ones(V a) { return vminvq_u8(vshrq_n_u8(a.v, 7)) != 0; }
VBAM_SIMD_INLINE bool all_ones_u32(V a) { return vminvq_u32(vshrq_n_u32(VBAM_U32(a), 31)) != 0; }
VBAM_SIMD_INLINE bool any_u32(V a) { return vmaxvq_u32(vshrq_n_u32(VBAM_U32(a), 31)) != 0; }

VBAM_SIMD_INLINE V vselect(V mask, V a, V b) { return {vbslq_u8(mask.v, a.v, b.v)}; }

template <int i> VBAM_SIMD_INLINE uint32_t lane_u32(V a) { return vgetq_lane_u32(VBAM_U32(a), i); }

VBAM_SIMD_INLINE void store2_u32(uint32_t* p, V a, V b) { uint32x4x2_t t = {{VBAM_U32(a), VBAM_U32(b)}}; vst2q_u32(p, t); }
VBAM_SIMD_INLINE void store2_u16(uint16_t* p, V a, V b) { uint16x8x2_t t = {{VBAM_U16(a), VBAM_U16(b)}}; vst2q_u16(p, t); }
VBAM_SIMD_INLINE void store_rep2_u32(uint32_t* p, V a) { store2_u32(p, a, a); }
VBAM_SIMD_INLINE void store_rep3_u32(uint32_t* p, V a) { uint32x4x3_t t = {{VBAM_U32(a), VBAM_U32(a), VBAM_U32(a)}}; vst3q_u32(p, t); }
VBAM_SIMD_INLINE void store_rep4_u32(uint32_t* p, V a) { uint32x4x4_t t = {{VBAM_U32(a), VBAM_U32(a), VBAM_U32(a), VBAM_U32(a)}}; vst4q_u32(p, t); }
VBAM_SIMD_INLINE void store_rep2_u16(uint16_t* p, V a) { store2_u16(p, a, a); }
VBAM_SIMD_INLINE void store_rep3_u16(uint16_t* p, V a) { uint16x8x3_t t = {{VBAM_U16(a), VBAM_U16(a), VBAM_U16(a)}}; vst3q_u16(p, t); }
VBAM_SIMD_INLINE void store_rep4_u16(uint16_t* p, V a) { uint16x8x4_t t = {{VBAM_U16(a), VBAM_U16(a), VBAM_U16(a), VBAM_U16(a)}}; vst4q_u16(p, t); }

VBAM_SIMD_INLINE VF fload(const float* p) { return {vld1q_f32(p)}; }
VBAM_SIMD_INLINE void fstore(float* p, VF a) { vst1q_f32(p, a.v); }
VBAM_SIMD_INLINE VF fzero() { return {vdupq_n_f32(0)}; }
VBAM_SIMD_INLINE VF fset1(float x) { return {vdupq_n_f32(x)}; }
VBAM_SIMD_INLINE VF fset(float a, float b, float c, float d) { const float t[4] = {a, b, c, d}; return {vld1q_f32(t)}; }
VBAM_SIMD_INLINE VF fadd(VF a, VF b) { return {vaddq_f32(a.v, b.v)}; }
VBAM_SIMD_INLINE VF fsub(VF a, VF b) { return {vsubq_f32(a.v, b.v)}; }
VBAM_SIMD_INLINE VF fmul(VF a, VF b) { return {vmulq_f32(a.v, b.v)}; }
VBAM_SIMD_INLINE VF fdiv(VF a, VF b) { return {vdivq_f32(a.v, b.v)}; }
VBAM_SIMD_INLINE VF fmin(VF a, VF b) { return {vminq_f32(a.v, b.v)}; }
VBAM_SIMD_INLINE VF fmax(VF a, VF b) { return {vmaxq_f32(a.v, b.v)}; }
VBAM_SIMD_INLINE VF fsqrt(VF a) { return {vsqrtq_f32(a.v)}; }
VBAM_SIMD_INLINE VF fabs(VF a) { return {vabsq_f32(a.v)}; }
VBAM_SIMD_INLINE V fcmplt(VF a, VF b) { return VBAM_FROM_U32(vcltq_f32(a.v, b.v)); }
VBAM_SIMD_INLINE V fcmple(VF a, VF b) { return VBAM_FROM_U32(vcleq_f32(a.v, b.v)); }
VBAM_SIMD_INLINE V fcmpgt(VF a, VF b) { return VBAM_FROM_U32(vcgtq_f32(a.v, b.v)); }
VBAM_SIMD_INLINE V fcmpge(VF a, VF b) { return VBAM_FROM_U32(vcgeq_f32(a.v, b.v)); }
VBAM_SIMD_INLINE VF fselect(V mask, VF a, VF b) { return {vbslq_f32(VBAM_U32(mask), a.v, b.v)}; }
VBAM_SIMD_INLINE VF fand(VF a, V mask) { return {vreinterpretq_f32_u32(vandq_u32(vreinterpretq_u32_f32(a.v), VBAM_U32(mask)))}; }
VBAM_SIMD_INLINE VF cvt_s32_f(V a) { return {vcvtq_f32_s32(VBAM_S32(a))}; }
VBAM_SIMD_INLINE V cvt_f_s32(VF a) { return VBAM_FROM_S32(vcvtq_s32_f32(a.v)); }
VBAM_SIMD_INLINE V fbits(VF a) { return VBAM_FROM_U32(vreinterpretq_u32_f32(a.v)); }
VBAM_SIMD_INLINE VF ffrombits(V a) { return {vreinterpretq_f32_u32(VBAM_U32(a))}; }

#undef VBAM_U16
#undef VBAM_U32
#undef VBAM_U64
#undef VBAM_S16
#undef VBAM_S32
#undef VBAM_FROM_U16
#undef VBAM_FROM_U32
#undef VBAM_FROM_U64
#undef VBAM_FROM_S16
#undef VBAM_FROM_S32

#else
// ===========================================================================
// Scalar emulation
// ===========================================================================
struct V
{
    union
    {
        uint8_t u8[16];
        uint16_t u16[8];
        uint32_t u32[4];
        uint64_t u64[2];
        int16_t s16[8];
        int32_t s32[4];
    };
};
struct VF { float f[4]; };

VBAM_SIMD_INLINE V load(const void* p) { V r; std::memcpy(r.u8, p, 16); return r; }
VBAM_SIMD_INLINE void store(void* p, V a) { std::memcpy(p, a.u8, 16); }
VBAM_SIMD_INLINE V load64(const void* p) { V r; std::memcpy(r.u8, p, 8); r.u64[1] = 0; return r; }
VBAM_SIMD_INLINE void store64(void* p, V a) { std::memcpy(p, a.u8, 8); }
VBAM_SIMD_INLINE V load32(const void* p) { V r; r.u64[0] = r.u64[1] = 0; std::memcpy(r.u8, p, 4); return r; }
VBAM_SIMD_INLINE void store32(void* p, V a) { std::memcpy(p, a.u8, 4); }

VBAM_SIMD_INLINE V zero() { V r; r.u64[0] = r.u64[1] = 0; return r; }
VBAM_SIMD_INLINE V set1_u8(uint8_t x) { V r; for (auto& e : r.u8) e = x; return r; }
VBAM_SIMD_INLINE V set1_u16(uint16_t x) { V r; for (auto& e : r.u16) e = x; return r; }
VBAM_SIMD_INLINE V set1_u32(uint32_t x) { V r; for (auto& e : r.u32) e = x; return r; }
VBAM_SIMD_INLINE V set_u32(uint32_t a, uint32_t b, uint32_t c, uint32_t d) { V r; r.u32[0] = a; r.u32[1] = b; r.u32[2] = c; r.u32[3] = d; return r; }
VBAM_SIMD_INLINE V from_u32(uint32_t x) { V r = zero(); r.u32[0] = x; return r; }
VBAM_SIMD_INLINE uint32_t to_u32(V a) { return a.u32[0]; }

#define VBAM_SIMD_LANES(name, field, n, expr) \
    VBAM_SIMD_INLINE V name(V a, V b) { V r; for (int i = 0; i < n; ++i) r.field[i] = (expr); return r; }
#define VBAM_SIMD_CAST8(x) static_cast<uint8_t>(x)
#define VBAM_SIMD_CAST16(x) static_cast<uint16_t>(x)
VBAM_SIMD_LANES(and_, u64, 2, a.u64[i] & b.u64[i])
VBAM_SIMD_LANES(or_, u64, 2, a.u64[i] | b.u64[i])
VBAM_SIMD_LANES(xor_, u64, 2, a.u64[i] ^ b.u64[i])
VBAM_SIMD_LANES(andnot, u64, 2, ~a.u64[i] & b.u64[i])
VBAM_SIMD_LANES(add_u8, u8, 16, VBAM_SIMD_CAST8(a.u8[i] + b.u8[i]))
VBAM_SIMD_LANES(add_u16, u16, 8, VBAM_SIMD_CAST16(a.u16[i] + b.u16[i]))
VBAM_SIMD_LANES(add_u32, u32, 4, a.u32[i] + b.u32[i])
VBAM_SIMD_LANES(sub_u8, u8, 16, VBAM_SIMD_CAST8(a.u8[i] - b.u8[i]))
VBAM_SIMD_LANES(sub_u16, u16, 8, VBAM_SIMD_CAST16(a.u16[i] - b.u16[i]))
VBAM_SIMD_LANES(sub_u32, u32, 4, a.u32[i] - b.u32[i])
VBAM_SIMD_LANES(adds_u8, u8, 16, VBAM_SIMD_CAST8(a.u8[i] + b.u8[i] > 255 ? 255 : a.u8[i] + b.u8[i]))
VBAM_SIMD_LANES(subs_u8, u8, 16, VBAM_SIMD_CAST8(a.u8[i] > b.u8[i] ? a.u8[i] - b.u8[i] : 0))
VBAM_SIMD_LANES(subs_u16, u16, 8, VBAM_SIMD_CAST16(a.u16[i] > b.u16[i] ? a.u16[i] - b.u16[i] : 0))
VBAM_SIMD_LANES(avg_u8, u8, 16, VBAM_SIMD_CAST8((a.u8[i] + b.u8[i] + 1) >> 1))
VBAM_SIMD_LANES(avg_u16, u16, 8, VBAM_SIMD_CAST16((a.u16[i] + b.u16[i] + 1) >> 1))
VBAM_SIMD_LANES(min_u8, u8, 16, a.u8[i] < b.u8[i] ? a.u8[i] : b.u8[i])
VBAM_SIMD_LANES(max_u8, u8, 16, a.u8[i] > b.u8[i] ? a.u8[i] : b.u8[i])
VBAM_SIMD_LANES(min_u16, u16, 8, a.u16[i] < b.u16[i] ? a.u16[i] : b.u16[i])
VBAM_SIMD_LANES(max_u16, u16, 8, a.u16[i] > b.u16[i] ? a.u16[i] : b.u16[i])
VBAM_SIMD_LANES(mullo_u16, u16, 8, VBAM_SIMD_CAST16(static_cast<uint32_t>(a.u16[i]) * b.u16[i]))
VBAM_SIMD_LANES(mulhi_u16, u16, 8, VBAM_SIMD_CAST16((static_cast<uint32_t>(a.u16[i]) * b.u16[i]) >> 16))
VBAM_SIMD_LANES(cmpeq_u8, u8, 16, a.u8[i] == b.u8[i] ? 0xff : 0)
VBAM_SIMD_LANES(cmpeq_u16, u16, 8, a.u16[i] == b.u16[i] ? 0xffff : 0)
VBAM_SIMD_LANES(cmpeq_u32, u32, 4, a.u32[i] == b.u32[i] ? 0xffffffffu : 0)
VBAM_SIMD_LANES(cmpgt_s16, u16, 8, a.s16[i] > b.s16[i] ? 0xffff : 0)
VBAM_SIMD_LANES(cmpgt_s32, u32, 4, a.s32[i] > b.s32[i] ? 0xffffffffu : 0)
VBAM_SIMD_LANES(unpacklo_u8, u8, 16, (i & 1) ? b.u8[i >> 1] : a.u8[i >> 1])
VBAM_SIMD_LANES(unpackhi_u8, u8, 16, (i & 1) ? b.u8[8 + (i >> 1)] : a.u8[8 + (i >> 1)])
VBAM_SIMD_LANES(unpacklo_u16, u16, 8, (i & 1) ? b.u16[i >> 1] : a.u16[i >> 1])
VBAM_SIMD_LANES(unpackhi_u16, u16, 8, (i & 1) ? b.u16[4 + (i >> 1)] : a.u16[4 + (i >> 1)])
VBAM_SIMD_LANES(unpacklo_u32, u32, 4, (i & 1) ? b.u32[i >> 1] : a.u32[i >> 1])
VBAM_SIMD_LANES(unpackhi_u32, u32, 4, (i & 1) ? b.u32[2 + (i >> 1)] : a.u32[2 + (i >> 1)])
VBAM_SIMD_LANES(unpacklo_u64, u64, 2, i ? b.u64[0] : a.u64[0])
VBAM_SIMD_LANES(unpackhi_u64, u64, 2, i ? b.u64[1] : a.u64[1])
VBAM_SIMD_LANES(packus_s16_u8, u8, 16, VBAM_SIMD_CAST8([](int16_t x) { return x < 0 ? 0 : x > 255 ? 255 : x; }(i < 8 ? a.s16[i] : b.s16[i - 8])))
VBAM_SIMD_LANES(pack_u32_u16, u16, 8, VBAM_SIMD_CAST16(i < 4 ? a.u32[i] : b.u32[i - 4]))
#undef VBAM_SIMD_LANES

template <int N> VBAM_SIMD_INLINE V srl_u16(V a) { for (auto& e : a.u16) e = static_cast<uint16_t>(N >= 16 ? 0 : e >> N); return a; }
template <int N> VBAM_SIMD_INLINE V sll_u16(V a) { for (auto& e : a.u16) e = static_cast<uint16_t>(N >= 16 ? 0 : e << N); return a; }
template <int N> VBAM_SIMD_INLINE V srl_u32(V a) { for (auto& e : a.u32) e = N >= 32 ? 0 : e >> N; return a; }
template <int N> VBAM_SIMD_INLINE V sll_u32(V a) { for (auto& e : a.u32) e = N >= 32 ? 0 : e << N; return a; }
template <int N> VBAM_SIMD_INLINE V sra_s16(V a) { for (auto& e : a.s16) e = static_cast<int16_t>(e >> (N >= 16 ? 15 : N)); return a; }
template <int N> VBAM_SIMD_INLINE V sra_s32(V a) { for (auto& e : a.s32) e = e >> (N >= 32 ? 31 : N); return a; }
VBAM_SIMD_INLINE V srl_u16_var(V a, int n) { n = ClampShift(n, 16); for (auto& e : a.u16) e = static_cast<uint16_t>(n >= 16 ? 0 : e >> n); return a; }
VBAM_SIMD_INLINE V sll_u16_var(V a, int n) { n = ClampShift(n, 16); for (auto& e : a.u16) e = static_cast<uint16_t>(n >= 16 ? 0 : e << n); return a; }
VBAM_SIMD_INLINE V srl_u32_var(V a, int n) { n = ClampShift(n, 32); for (auto& e : a.u32) e = n >= 32 ? 0 : e >> n; return a; }
VBAM_SIMD_INLINE V sll_u32_var(V a, int n) { n = ClampShift(n, 32); for (auto& e : a.u32) e = n >= 32 ? 0 : e << n; return a; }
template <int N> VBAM_SIMD_INLINE V bsrl(V a) { V r = zero(); for (int i = 0; i + N < 16; ++i) r.u8[i] = a.u8[i + N]; return r; }
template <int N> VBAM_SIMD_INLINE V bsll(V a) { V r = zero(); for (int i = N; i < 16; ++i) r.u8[i] = a.u8[i - N]; return r; }

template <int i0, int i1, int i2, int i3> VBAM_SIMD_INLINE V shuffle_u32(V a)
{
    return set_u32(a.u32[i0], a.u32[i1], a.u32[i2], a.u32[i3]);
}

VBAM_SIMD_INLINE int movemask_u8(V a) { int m = 0; for (int i = 0; i < 16; ++i) m |= (a.u8[i] >> 7) << i; return m; }
VBAM_SIMD_INLINE bool all_zero(V a) { return (a.u64[0] | a.u64[1]) == 0; }
VBAM_SIMD_INLINE bool all_ones(V a) { return movemask_u8(a) == 0xffff; }
VBAM_SIMD_INLINE bool all_ones_u32(V a) { return ((a.u32[0] & a.u32[1] & a.u32[2] & a.u32[3]) >> 31) != 0; }
VBAM_SIMD_INLINE bool any_u32(V a) { return ((a.u32[0] | a.u32[1] | a.u32[2] | a.u32[3]) >> 31) != 0; }

VBAM_SIMD_INLINE V vselect(V mask, V a, V b) { return or_(and_(mask, a), andnot(mask, b)); }

template <int i> VBAM_SIMD_INLINE uint32_t lane_u32(V a) { return a.u32[i]; }

VBAM_SIMD_INLINE void store2_u32(uint32_t* p, V a, V b) { for (int i = 0; i < 4; i++) { p[2 * i] = a.u32[i]; p[2 * i + 1] = b.u32[i]; } }
VBAM_SIMD_INLINE void store2_u16(uint16_t* p, V a, V b) { for (int i = 0; i < 8; i++) { p[2 * i] = a.u16[i]; p[2 * i + 1] = b.u16[i]; } }
VBAM_SIMD_INLINE void store_rep2_u32(uint32_t* p, V a) { for (int i = 0; i < 4; i++) p[2 * i] = p[2 * i + 1] = a.u32[i]; }
VBAM_SIMD_INLINE void store_rep3_u32(uint32_t* p, V a) { for (int i = 0; i < 4; i++) p[3 * i] = p[3 * i + 1] = p[3 * i + 2] = a.u32[i]; }
VBAM_SIMD_INLINE void store_rep4_u32(uint32_t* p, V a) { for (int i = 0; i < 4; i++) p[4 * i] = p[4 * i + 1] = p[4 * i + 2] = p[4 * i + 3] = a.u32[i]; }
VBAM_SIMD_INLINE void store_rep2_u16(uint16_t* p, V a) { for (int i = 0; i < 8; i++) p[2 * i] = p[2 * i + 1] = a.u16[i]; }
VBAM_SIMD_INLINE void store_rep3_u16(uint16_t* p, V a) { for (int i = 0; i < 8; i++) p[3 * i] = p[3 * i + 1] = p[3 * i + 2] = a.u16[i]; }
VBAM_SIMD_INLINE void store_rep4_u16(uint16_t* p, V a) { for (int i = 0; i < 8; i++) p[4 * i] = p[4 * i + 1] = p[4 * i + 2] = p[4 * i + 3] = a.u16[i]; }

VBAM_SIMD_INLINE VF fload(const float* p) { VF r; std::memcpy(r.f, p, 16); return r; }
VBAM_SIMD_INLINE void fstore(float* p, VF a) { std::memcpy(p, a.f, 16); }
VBAM_SIMD_INLINE VF fzero() { return {{0, 0, 0, 0}}; }
VBAM_SIMD_INLINE VF fset1(float x) { return {{x, x, x, x}}; }
VBAM_SIMD_INLINE VF fset(float a, float b, float c, float d) { return {{a, b, c, d}}; }
#define VBAM_SIMD_FLANES(name, expr) \
    VBAM_SIMD_INLINE VF name(VF a, VF b) { VF r; for (int i = 0; i < 4; ++i) r.f[i] = (expr); return r; }
VBAM_SIMD_FLANES(fadd, a.f[i] + b.f[i])
VBAM_SIMD_FLANES(fsub, a.f[i] - b.f[i])
VBAM_SIMD_FLANES(fmul, a.f[i] * b.f[i])
VBAM_SIMD_FLANES(fdiv, a.f[i] / b.f[i])
VBAM_SIMD_FLANES(fmin, b.f[i] < a.f[i] ? b.f[i] : a.f[i])
VBAM_SIMD_FLANES(fmax, b.f[i] > a.f[i] ? b.f[i] : a.f[i])
#undef VBAM_SIMD_FLANES
VBAM_SIMD_INLINE VF fsqrt(VF a) { for (auto& e : a.f) e = std::sqrt(e); return a; }
VBAM_SIMD_INLINE VF fabs(VF a) { for (auto& e : a.f) e = std::fabs(e); return a; }
#define VBAM_SIMD_FCMP(name, op) \
    VBAM_SIMD_INLINE V name(VF a, VF b) { V r; for (int i = 0; i < 4; ++i) r.u32[i] = (a.f[i] op b.f[i]) ? 0xffffffffu : 0; return r; }
VBAM_SIMD_FCMP(fcmplt, <)
VBAM_SIMD_FCMP(fcmple, <=)
VBAM_SIMD_FCMP(fcmpgt, >)
VBAM_SIMD_FCMP(fcmpge, >=)
#undef VBAM_SIMD_FCMP
VBAM_SIMD_INLINE V fbits(VF a) { V r; std::memcpy(r.u8, a.f, 16); return r; }
VBAM_SIMD_INLINE VF ffrombits(V a) { VF r; std::memcpy(r.f, a.u8, 16); return r; }
VBAM_SIMD_INLINE VF fselect(V mask, VF a, VF b) { return ffrombits(vselect(mask, fbits(a), fbits(b))); }
VBAM_SIMD_INLINE VF fand(VF a, V mask) { return ffrombits(and_(fbits(a), mask)); }
VBAM_SIMD_INLINE VF cvt_s32_f(V a) { VF r; for (int i = 0; i < 4; ++i) r.f[i] = static_cast<float>(a.s32[i]); return r; }
VBAM_SIMD_INLINE V cvt_f_s32(VF a) { V r; for (int i = 0; i < 4; ++i) r.s32[i] = static_cast<int32_t>(a.f[i]); return r; }

#undef VBAM_SIMD_CAST8
#undef VBAM_SIMD_CAST16
#endif

// ---------------------------------------------------------------------------
// Helpers on top of the backends
// ---------------------------------------------------------------------------

//lane i of a 4 x u32 vector broadcast to all lanes
template <int i> VBAM_SIMD_INLINE V dup_u32(V a) { return shuffle_u32<i, i, i, i>(a); }

//~a
VBAM_SIMD_INLINE V not_(V a) { return xor_(a, cmpeq_u32(a, a)); }

//a != b, per u32 lane
VBAM_SIMD_INLINE V cmpne_u32(V a, V b) { return not_(cmpeq_u32(a, b)); }
VBAM_SIMD_INLINE V cmpne_u16(V a, V b) { return not_(cmpeq_u16(a, b)); }

//|a - b| per byte
VBAM_SIMD_INLINE V absdiff_u8(V a, V b) { return or_(subs_u8(a, b), subs_u8(b, a)); }
//|a - b| per u16 lane
VBAM_SIMD_INLINE V absdiff_u16(V a, V b) { return sub_u16(max_u16(a, b), min_u16(a, b)); }

//(a + b) / 2 per byte, rounding down (xBRZ / 2xSaI style), ignoring carries between bytes
VBAM_SIMD_INLINE V avgdown_u8(V a, V b)
{
    return add_u8(and_(a, b), srl_u16<1>(and_(xor_(a, b), set1_u8(0xfe))));
}

//truncating (a + b) / 2 on the masked channels of packed 16-bit pixels:
//mask clears the low bit of every channel so the shifted halves do not carry across
VBAM_SIMD_INLINE V halfsum_masked_u16(V a, V b, V mask)
{
    return add_u16(srl_u16<1>(and_(a, mask)), srl_u16<1>(and_(b, mask)));
}
VBAM_SIMD_INLINE V halfsum_masked_u32(V a, V b, V mask)
{
    return add_u32(srl_u32<1>(and_(a, mask)), srl_u32<1>(and_(b, mask)));
}

}  // namespace vbam_simd

#endif  // VBAM_COMPONENTS_FILTERS_SIMD128_H_
