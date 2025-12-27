/// Interframe blending filters with thread-safe per-region buffering
/// Supports multithreaded IFB synchronized with FilterThread

#include "components/filters_interframe/interframe.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <vector>

// SIMD headers
#if defined(__SSE2__) || (defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2))
#define USE_SSE2
#include <emmintrin.h>
#endif

#if defined(__SSE__) || (defined(_M_IX86_FP) && _M_IX86_FP >= 1)
#define USE_SSE
#include <xmmintrin.h>
#endif

#ifdef MMX
extern "C" bool cpu_mmx;
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
        regions_[i].frm1.reset(new uint8_t[buffer_size_]());
        regions_[i].frm2.reset(new uint8_t[buffer_size_]());
        regions_[i].frm3.reset(new uint8_t[buffer_size_]());
    }

    initialized_ = true;
}

void InterframeManager::Cleanup() {
    std::lock_guard<std::mutex> lock(init_mutex_);
    CleanupInternal();
}

void InterframeManager::CleanupInternal() {
    // Caller must hold init_mutex_
    regions_.clear();
    initialized_ = false;
    width_ = height_ = pitch_ = 0;
    buffer_size_ = 0;
}

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
    // colorMask depends on RGB_LOW_BITS_MASK which is set globally
    const uint16_t colorMask = static_cast<uint16_t>(~RGB_LOW_BITS_MASK);
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

static void SmartIB32_SSE2(uint8_t* srcPtr, uint8_t* frm1Ptr, uint8_t* frm2Ptr, uint8_t* frm3Ptr,
                           uint32_t srcPitch, int width, int starty, int height) {
    (void)width;  // Width is implicit in srcPitch for SIMD version
    const __m128i colorMask = _mm_set1_epi32(0x00FEFEFE);

    uint32_t* src = reinterpret_cast<uint32_t*>(srcPtr) + starty * (srcPitch / 4);
    uint32_t* frm1 = reinterpret_cast<uint32_t*>(frm1Ptr) + starty * (srcPitch / 4);
    uint32_t* frm2 = reinterpret_cast<uint32_t*>(frm2Ptr) + starty * (srcPitch / 4);
    uint32_t* frm3 = reinterpret_cast<uint32_t*>(frm3Ptr) + starty * (srcPitch / 4);

    int sPitch = srcPitch >> 2;

    for (int y = 0; y < height; y++) {
        int x = 0;
        // Process 4 pixels at a time
        for (; x + 4 <= sPitch; x += 4) {
            __m128i src0 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&src[x]));
            __m128i src1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&frm1[x]));
            __m128i src2 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&frm2[x]));
            __m128i src3 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&frm3[x]));

            // Save current to frm3 (oldest buffer gets newest frame)
            _mm_storeu_si128(reinterpret_cast<__m128i*>(&frm3[x]), src0);

            // Smart IB logic:
            // A = (src1 != src2)
            // B = (src3 != src0)  (note: src3 is old value, src0 is current)
            // C = (src0 == src2)
            // D = (src1 == src3)
            // If (A && B && (C || D)): blend, else: keep original

            __m128i eq_1_2 = _mm_cmpeq_epi32(src1, src2);  // src1 == src2
            __m128i eq_3_0 = _mm_cmpeq_epi32(src3, src0);  // src3 == src0
            __m128i eq_0_2 = _mm_cmpeq_epi32(src0, src2);  // src0 == src2
            __m128i eq_1_3 = _mm_cmpeq_epi32(src1, src3);  // src1 == src3

            // A = NOT(src1 == src2) = src1 != src2
            // B = NOT(src3 == src0) = src3 != src0
            // We need A && B, which is NOT(eq_1_2) && NOT(eq_3_0)
            // Using De Morgan: NOT(eq_1_2 || eq_3_0)
            __m128i eq_1_2_or_eq_3_0 = _mm_or_si128(eq_1_2, eq_3_0);
            __m128i ones = _mm_set1_epi32(-1);
            __m128i a_and_b = _mm_xor_si128(eq_1_2_or_eq_3_0, ones);  // NOT(eq_1_2 || eq_3_0)

            // C || D = (src0 == src2) || (src1 == src3)
            __m128i c_or_d = _mm_or_si128(eq_0_2, eq_1_3);

            // blend_mask = A && B && (C || D)
            __m128i blend_mask = _mm_and_si128(a_and_b, c_or_d);

            // Calculate blended value
            __m128i src0_masked = _mm_and_si128(src0, colorMask);
            __m128i src1_masked = _mm_and_si128(src1, colorMask);
            __m128i blended = _mm_add_epi32(_mm_srli_epi32(src0_masked, 1),
                                            _mm_srli_epi32(src1_masked, 1));

            // Select: blend_mask ? blended : src0
            __m128i result = _mm_or_si128(_mm_and_si128(blend_mask, blended),
                                          _mm_andnot_si128(blend_mask, src0));

            _mm_storeu_si128(reinterpret_cast<__m128i*>(&src[x]), result);
        }

        // Scalar tail
        for (; x < sPitch; x++) {
            uint32_t color = src[x];
            bool should_blend = (frm1[x] != frm2[x]) &&
                               (frm3[x] != color) &&
                               ((color == frm2[x]) || (frm1[x] == frm3[x]));

            frm3[x] = color;
            if (should_blend) {
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
    (void)width;  // Width is implicit in srcPitch for SIMD version
    const uint16_t colorMaskVal = static_cast<uint16_t>(~RGB_LOW_BITS_MASK);
    const __m128i colorMask = _mm_set1_epi16(colorMaskVal);

    uint16_t* src = reinterpret_cast<uint16_t*>(srcPtr) + starty * (srcPitch / 2);
    uint16_t* frm1 = reinterpret_cast<uint16_t*>(frm1Ptr) + starty * (srcPitch / 2);
    uint16_t* frm2 = reinterpret_cast<uint16_t*>(frm2Ptr) + starty * (srcPitch / 2);
    uint16_t* frm3 = reinterpret_cast<uint16_t*>(frm3Ptr) + starty * (srcPitch / 2);

    int sPitch = srcPitch >> 1;

    for (int y = 0; y < height; y++) {
        int x = 0;
        // Process 8 pixels at a time
        for (; x + 8 <= sPitch; x += 8) {
            __m128i src0 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&src[x]));
            __m128i src1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&frm1[x]));
            __m128i src2 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&frm2[x]));
            __m128i src3 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&frm3[x]));

            _mm_storeu_si128(reinterpret_cast<__m128i*>(&frm3[x]), src0);

            __m128i eq_1_2 = _mm_cmpeq_epi16(src1, src2);
            __m128i eq_3_0 = _mm_cmpeq_epi16(src3, src0);
            __m128i eq_0_2 = _mm_cmpeq_epi16(src0, src2);
            __m128i eq_1_3 = _mm_cmpeq_epi16(src1, src3);

            // A = (src1 != src2), B = (src3 != src0)
            // We need A && B = NOT(eq_1_2) && NOT(eq_3_0) = NOT(eq_1_2 || eq_3_0)
            __m128i eq_1_2_or_eq_3_0 = _mm_or_si128(eq_1_2, eq_3_0);
            __m128i ones = _mm_set1_epi16(-1);
            __m128i a_and_b = _mm_xor_si128(eq_1_2_or_eq_3_0, ones);  // NOT(eq_1_2 || eq_3_0)

            // C || D = (src0 == src2) || (src1 == src3)
            __m128i c_or_d = _mm_or_si128(eq_0_2, eq_1_3);

            // blend_mask = A && B && (C || D)
            __m128i blend_mask = _mm_and_si128(a_and_b, c_or_d);

            __m128i src0_masked = _mm_and_si128(src0, colorMask);
            __m128i src1_masked = _mm_and_si128(src1, colorMask);
            __m128i blended = _mm_add_epi16(_mm_srli_epi16(src0_masked, 1),
                                            _mm_srli_epi16(src1_masked, 1));

            __m128i result = _mm_or_si128(_mm_and_si128(blend_mask, blended),
                                          _mm_andnot_si128(blend_mask, src0));

            _mm_storeu_si128(reinterpret_cast<__m128i*>(&src[x]), result);
        }

        // Scalar tail
        for (; x < sPitch; x++) {
            uint16_t color = src[x];
            bool should_blend = (frm1[x] != frm2[x]) &&
                               (frm3[x] != color) &&
                               ((color == frm2[x]) || (frm1[x] == frm3[x]));

            frm3[x] = color;
            if (should_blend) {
                src[x] = ((color & colorMaskVal) >> 1) + ((frm1[x] & colorMaskVal) >> 1);
            }
        }

        src += sPitch;
        frm1 += sPitch;
        frm2 += sPitch;
        frm3 += sPitch;
    }
}

#endif // USE_SSE2

// ============================================================================
// MMX Implementations (32-bit)
// ============================================================================

#ifdef MMX

static void SmartIB_MMX(uint8_t* srcPtr, uint8_t* frm1Ptr, uint8_t* frm2Ptr, uint8_t* frm3Ptr,
                        uint32_t srcPitch, int width, int starty, int height) {
    uint16_t* src0 = reinterpret_cast<uint16_t*>(srcPtr) + starty * srcPitch / 2;
    uint16_t* src1 = reinterpret_cast<uint16_t*>(frm1Ptr) + srcPitch * starty / 2;
    uint16_t* src2 = reinterpret_cast<uint16_t*>(frm2Ptr) + srcPitch * starty / 2;
    uint16_t* src3 = reinterpret_cast<uint16_t*>(frm3Ptr) + srcPitch * starty / 2;

    int count = width >> 2;

    for (int i = 0; i < height; i++) {
#ifdef __GNUC__
        asm volatile(
            "push %4\n"
            "movq 0(%5), %%mm7\n"  // colorMask
            "0:\n"
            "movq 0(%0), %%mm0\n"  // src0
            "movq 0(%1), %%mm1\n"  // src1
            "movq 0(%2), %%mm2\n"  // src2
            "movq 0(%3), %%mm3\n"  // src3
            "movq %%mm0, 0(%3)\n"  // src3 = src0
            "movq %%mm0, %%mm4\n"
            "movq %%mm1, %%mm5\n"
            "pcmpeqw %%mm2, %%mm5\n"  // src1 == src2 (A)
            "pcmpeqw %%mm3, %%mm4\n"  // src3 == src0 (B)
            "por %%mm5, %%mm4\n"      // A | B
            "movq %%mm2, %%mm5\n"
            "pcmpeqw %%mm0, %%mm5\n"  // src0 == src2 (C)
            "pcmpeqw %%mm1, %%mm3\n"  // src1 == src3 (D)
            "por %%mm3, %%mm5\n"      // C|D
            "pandn %%mm5, %%mm4\n"    // (!(A|B))&(C|D)
            "movq %%mm0, %%mm2\n"
            "pand %%mm7, %%mm2\n"  // color & colorMask
            "pand %%mm7, %%mm1\n"  // src1 & colorMask
            "psrlw $1, %%mm2\n"    // (color & colorMask) >> 1 (E)
            "psrlw $1, %%mm1\n"    // (src & colorMask) >> 1 (F)
            "paddw %%mm2, %%mm1\n"  // E+F
            "pand %%mm4, %%mm1\n"   // (E+F) & res
            "pandn %%mm0, %%mm4\n"  // color& !res

            "por %%mm1, %%mm4\n"
            "movq %%mm4, 0(%0)\n"  // src0 = res

            "addl $8, %0\n"
            "addl $8, %1\n"
            "addl $8, %2\n"
            "addl $8, %3\n"

            "decl %4\n"
            "jnz 0b\n"
            "pop %4\n"
            "emms\n"
            : "+r"(src0), "+r"(src1), "+r"(src2), "+r"(src3)
            : "r"(count), "r"(qRGB_COLOR_MASK));
#else
        __asm {
            movq mm7, qword ptr[qRGB_COLOR_MASK];
            mov eax, src0;
            mov ebx, src1;
            mov ecx, src2;
            mov edx, src3;
            mov edi, count;
        label0:
            movq mm0, qword ptr[eax];  // src0
            movq mm1, qword ptr[ebx];  // src1
            movq mm2, qword ptr[ecx];  // src2
            movq mm3, qword ptr[edx];  // src3
            movq qword ptr[edx], mm0;  // src3 = src0
            movq mm4, mm0;
            movq mm5, mm1;
            pcmpeqw mm5, mm2;  // src1 == src2 (A)
            pcmpeqw mm4, mm3;  // src3 == src0 (B)
            por mm4, mm5;      // A | B
            movq mm5, mm2;
            pcmpeqw mm5, mm0;  // src0 == src2 (C)
            pcmpeqw mm3, mm1;  // src1 == src3 (D)
            por mm5, mm3;      // C|D
            pandn mm4, mm5;    // (!(A|B))&(C|D)
            movq mm2, mm0;
            pand mm2, mm7;  // color & colorMask
            pand mm1, mm7;  // src1 & colorMask
            psrlw mm2, 1;   // (color & colorMask) >> 1 (E)
            psrlw mm1, 1;   // (src & colorMask) >> 1 (F)
            paddw mm1, mm2;  // E+F
            pand mm1, mm4;   // (E+F) & res
            pandn mm4, mm0;  // color & !res

            por mm4, mm1;
            movq qword ptr[eax], mm4;  // src0 = res

            add eax, 8;
            add ebx, 8;
            add ecx, 8;
            add edx, 8;

            dec edi;
            jnz label0;
            mov src0, eax;
            mov src1, ebx;
            mov src2, ecx;
            mov src3, edx;
            emms;
        }
#endif
        src0 += 2;
        src1 += 2;
        src2 += 2;
        src3 += 2;
    }
}

static void SmartIB32_MMX(uint8_t* srcPtr, uint8_t* frm1Ptr, uint8_t* frm2Ptr, uint8_t* frm3Ptr,
                          uint32_t srcPitch, int width, int starty, int height) {
    uint32_t* src0 = reinterpret_cast<uint32_t*>(srcPtr) + starty * srcPitch / 4;
    uint32_t* src1 = reinterpret_cast<uint32_t*>(frm1Ptr) + starty * srcPitch / 4;
    uint32_t* src2 = reinterpret_cast<uint32_t*>(frm2Ptr) + starty * srcPitch / 4;
    uint32_t* src3 = reinterpret_cast<uint32_t*>(frm3Ptr) + starty * srcPitch / 4;

    int count = width >> 1;

    for (int i = 0; i < height; i++) {
#ifdef __GNUC__
        asm volatile(
            "push %4\n"
            "movq 0(%5), %%mm7\n"  // colorMask
            "0:\n"
            "movq 0(%0), %%mm0\n"  // src0
            "movq 0(%1), %%mm1\n"  // src1
            "movq 0(%2), %%mm2\n"  // src2
            "movq 0(%3), %%mm3\n"  // src3
            "movq %%mm0, 0(%3)\n"  // src3 = src0
            "movq %%mm0, %%mm4\n"
            "movq %%mm1, %%mm5\n"
            "pcmpeqd %%mm2, %%mm5\n"  // src1 == src2 (A)
            "pcmpeqd %%mm3, %%mm4\n"  // src3 == src0 (B)
            "por %%mm5, %%mm4\n"      // A | B
            "movq %%mm2, %%mm5\n"
            "pcmpeqd %%mm0, %%mm5\n"  // src0 == src2 (C)
            "pcmpeqd %%mm1, %%mm3\n"  // src1 == src3 (D)
            "por %%mm3, %%mm5\n"      // C|D
            "pandn %%mm5, %%mm4\n"    // (!(A|B))&(C|D)
            "movq %%mm0, %%mm2\n"
            "pand %%mm7, %%mm2\n"  // color & colorMask
            "pand %%mm7, %%mm1\n"  // src1 & colorMask
            "psrld $1, %%mm2\n"    // (color & colorMask) >> 1 (E)
            "psrld $1, %%mm1\n"    // (src & colorMask) >> 1 (F)
            "paddd %%mm2, %%mm1\n"  // E+F
            "pand %%mm4, %%mm1\n"   // (E+F) & res
            "pandn %%mm0, %%mm4\n"  // color& !res

            "por %%mm1, %%mm4\n"
            "movq %%mm4, 0(%0)\n"  // src0 = res

            "addl $8, %0\n"
            "addl $8, %1\n"
            "addl $8, %2\n"
            "addl $8, %3\n"

            "decl %4\n"
            "jnz 0b\n"
            "pop %4\n"
            "emms\n"
            : "+r"(src0), "+r"(src1), "+r"(src2), "+r"(src3)
            : "r"(count), "r"(qRGB_COLOR_MASK));
#else
        __asm {
            movq mm7, qword ptr[qRGB_COLOR_MASK];
            mov eax, src0;
            mov ebx, src1;
            mov ecx, src2;
            mov edx, src3;
            mov edi, count;
        label0:
            movq mm0, qword ptr[eax];  // src0
            movq mm1, qword ptr[ebx];  // src1
            movq mm2, qword ptr[ecx];  // src2
            movq mm3, qword ptr[edx];  // src3
            movq qword ptr[edx], mm0;  // src3 = src0
            movq mm4, mm0;
            movq mm5, mm1;
            pcmpeqd mm5, mm2;  // src1 == src2 (A)
            pcmpeqd mm4, mm3;  // src3 == src0 (B)
            por mm4, mm5;      // A | B
            movq mm5, mm2;
            pcmpeqd mm5, mm0;  // src0 == src2 (C)
            pcmpeqd mm3, mm1;  // src1 == src3 (D)
            por mm5, mm3;      // C|D
            pandn mm4, mm5;    // (!(A|B))&(C|D)
            movq mm2, mm0;
            pand mm2, mm7;  // color & colorMask
            pand mm1, mm7;  // src1 & colorMask
            psrld mm2, 1;   // (color & colorMask) >> 1 (E)
            psrld mm1, 1;   // (src & colorMask) >> 1 (F)
            paddd mm1, mm2;  // E+F
            pand mm1, mm4;   // (E+F) & res
            pandn mm4, mm0;  // color & !res

            por mm4, mm1;
            movq qword ptr[eax], mm4;  // src0 = res

            add eax, 8;
            add ebx, 8;
            add ecx, 8;
            add edx, 8;

            dec edi;
            jnz label0;
            mov src0, eax;
            mov src1, ebx;
            mov src2, ecx;
            mov src3, edx;
            emms;
        }
#endif
        src0++;
        src1++;
        src2++;
        src3++;
    }
}

static void MotionBlurIB_MMX(uint8_t* srcPtr, uint8_t* histPtr,
                              uint32_t srcPitch, int width, int starty, int height) {
    uint16_t* src0 = reinterpret_cast<uint16_t*>(srcPtr) + starty * srcPitch / 2;
    uint16_t* src1 = reinterpret_cast<uint16_t*>(histPtr) + starty * srcPitch / 2;

    int count = width >> 2;

    for (int i = 0; i < height; i++) {
#ifdef __GNUC__
        asm volatile(
            "push %2\n"
            "movq 0(%3), %%mm7\n"  // colorMask
            "0:\n"
            "movq 0(%0), %%mm0\n"  // src0
            "movq 0(%1), %%mm1\n"  // src1
            "movq %%mm0, 0(%1)\n"  // src1 = src0
            "pand %%mm7, %%mm0\n"  // color & colorMask
            "pand %%mm7, %%mm1\n"  // src1 & colorMask
            "psrlw $1, %%mm0\n"    // (color & colorMask) >> 1 (E)
            "psrlw $1, %%mm1\n"    // (src & colorMask) >> 1 (F)
            "paddw %%mm1, %%mm0\n"  // E+F

            "movq %%mm0, 0(%0)\n"  // src0 = res

            "addl $8, %0\n"
            "addl $8, %1\n"

            "decl %2\n"
            "jnz 0b\n"
            "pop %2\n"
            "emms\n"
            : "+r"(src0), "+r"(src1)
            : "r"(count), "r"(qRGB_COLOR_MASK));
#else
        __asm {
            movq mm7, qword ptr[qRGB_COLOR_MASK];
            mov eax, src0;
            mov ebx, src1;
            mov edi, count;
        label0:
            movq mm0, qword ptr[eax];  // src0
            movq mm1, qword ptr[ebx];  // src1
            movq qword ptr[ebx], mm0;  // src1 = src0
            pand mm0, mm7;  // color & colorMask
            pand mm1, mm7;  // src1 & colorMask
            psrlw mm0, 1;   // (color & colorMask) >> 1 (E)
            psrlw mm1, 1;   // (src & colorMask) >> 1 (F)
            paddw mm0, mm1;  // E+F

            movq qword ptr[eax], mm0;  // src0 = res

            add eax, 8;
            add ebx, 8;

            dec edi;
            jnz label0;
            mov src0, eax;
            mov src1, ebx;
            emms;
        }
#endif
        src0 += 2;
        src1 += 2;
    }
}

static void MotionBlurIB32_MMX(uint8_t* srcPtr, uint8_t* histPtr,
                                uint32_t srcPitch, int width, int starty, int height) {
    uint32_t* src0 = reinterpret_cast<uint32_t*>(srcPtr) + starty * srcPitch / 4;
    uint32_t* src1 = reinterpret_cast<uint32_t*>(histPtr) + starty * srcPitch / 4;

    int count = width >> 1;

    for (int i = 0; i < height; i++) {
#ifdef __GNUC__
        asm volatile(
            "push %2\n"
            "movq 0(%3), %%mm7\n"  // colorMask
            "0:\n"
            "movq 0(%0), %%mm0\n"  // src0
            "movq 0(%1), %%mm1\n"  // src1
            "movq %%mm0, 0(%1)\n"  // src1 = src0
            "pand %%mm7, %%mm0\n"  // color & colorMask
            "pand %%mm7, %%mm1\n"  // src1 & colorMask
            "psrld $1, %%mm0\n"    // (color & colorMask) >> 1 (E)
            "psrld $1, %%mm1\n"    // (src & colorMask) >> 1 (F)
            "paddd %%mm1, %%mm0\n"  // E+F

            "movq %%mm0, 0(%0)\n"  // src0 = res

            "addl $8, %0\n"
            "addl $8, %1\n"

            "decl %2\n"
            "jnz 0b\n"
            "pop %2\n"
            "emms\n"
            : "+r"(src0), "+r"(src1)
            : "r"(count), "r"(qRGB_COLOR_MASK));
#else
        __asm {
            movq mm7, qword ptr[qRGB_COLOR_MASK];
            mov eax, src0;
            mov ebx, src1;
            mov edi, count;
        label0:
            movq mm0, qword ptr[eax];  // src0
            movq mm1, qword ptr[ebx];  // src1
            movq qword ptr[ebx], mm0;  // src1 = src0
            pand mm0, mm7;  // color & colorMask
            pand mm1, mm7;  // src1 & colorMask
            psrld mm0, 1;   // (color & colorMask) >> 1 (E)
            psrld mm1, 1;   // (src & colorMask) >> 1 (F)
            paddd mm0, mm1;  // E+F

            movq qword ptr[eax], mm0;  // src0 = res

            add eax, 8;
            add ebx, 8;

            dec edi;
            jnz label0;
            mov src0, eax;
            mov src1, ebx;
            emms;
        }
#endif
        src0++;
        src1++;
    }
}

#endif  // MMX

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

    const uint32_t colorMask = 0xfefefe;
    int sPitch = srcPitch >> 2;
    int pos = 0;

    for (int j = 0; j < height; j++) {
        for (int i = 0; i < sPitch; i++) {
            uint32_t color = src0[pos];
            src0[pos] = (src1[pos] != src2[pos]) && (src3[pos] != color) &&
                                ((color == src2[pos]) || (src1[pos] == src3[pos]))
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
    uint16_t colorMask = static_cast<uint16_t>(~RGB_LOW_BITS_MASK);

    uint16_t* src0 = reinterpret_cast<uint16_t*>(srcPtr) + starty * srcPitch / 2;
    uint16_t* src1 = reinterpret_cast<uint16_t*>(frm1Ptr) + srcPitch * starty / 2;
    uint16_t* src2 = reinterpret_cast<uint16_t*>(frm2Ptr) + srcPitch * starty / 2;
    uint16_t* src3 = reinterpret_cast<uint16_t*>(frm3Ptr) + srcPitch * starty / 2;

    int sPitch = srcPitch >> 1;
    int pos = 0;

    for (int j = 0; j < height; j++) {
        for (int i = 0; i < sPitch; i++) {
            uint16_t color = src0[pos];
            src0[pos] = (src1[pos] != src2[pos]) && (src3[pos] != color) &&
                                ((color == src2[pos]) || (src1[pos] == src3[pos]))
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
    const uint8_t colorMask = 0xfe;

    uint8_t* src0 = srcPtr + starty * srcPitch / 3;
    uint8_t* src1 = frm1Ptr + srcPitch * starty / 3;
    uint8_t* src2 = frm2Ptr + srcPitch * starty / 3;
    uint8_t* src3 = frm3Ptr + srcPitch * starty / 3;

    int sPitch = srcPitch / 3;
    int pos = 0;

    for (int j = 0; j < height; j++) {
        for (int i = 0; i < sPitch; i++) {
            uint8_t color = src0[pos];
            uint8_t color2 = src0[pos + 1];
            uint8_t color3 = src0[pos + 2];

            src0[pos] = (src1[pos] != src2[pos]) && (src3[pos] != color) &&
                                ((color == src2[pos]) || (src1[pos] == src3[pos]))
                            ? (((color & colorMask) >> 1) + ((src1[pos] & colorMask) >> 1))
                            : color;
            src0[pos + 1] = (src1[pos + 1] != src2[pos + 1]) && (src3[pos + 1] != color2) &&
                                    ((color2 == src2[pos + 1]) || (src1[pos + 1] == src3[pos + 1]))
                                ? (((color2 & colorMask) >> 1) + ((src1[pos + 1] & colorMask) >> 1))
                                : color2;
            src0[pos + 2] = (src1[pos + 2] != src2[pos + 2]) && (src3[pos + 2] != color3) &&
                                    ((color3 == src2[pos + 2]) || (src1[pos + 1] == src3[pos + 2]))
                                ? (((color3 & colorMask) >> 1) + ((src1[pos + 2] & colorMask) >> 1))
                                : color3;

            src3[pos] = color;
            src3[pos + 1] = color2;
            src3[pos + 2] = color3;
            pos += 3;
        }
    }
}

static void SmartIB8_Scalar(uint8_t* srcPtr, uint8_t* frm1Ptr, uint8_t* frm2Ptr, uint8_t* frm3Ptr,
                             uint32_t srcPitch, int width, int starty, int height) {
    (void)width;  // Width is implicit in srcPitch for consistency with SIMD versions
    uint16_t colorMask = static_cast<uint16_t>(~RGB_LOW_BITS_MASK);

    uint8_t* src0 = srcPtr + starty * srcPitch;
    uint8_t* src1 = frm1Ptr + srcPitch * starty;
    uint8_t* src2 = frm2Ptr + srcPitch * starty;
    uint8_t* src3 = frm3Ptr + srcPitch * starty;

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

            src0[pos] = (src1[pos] != src2[pos]) && (src3[pos] != static_cast<uint8_t>(color)) &&
                                ((color == src2[pos]) || (src1[pos] == src3[pos]))
                            ? static_cast<uint8_t>(((color_dst >> 7) & 0xe0) |
                                                   ((color_dst >> 5) & 0x1c) |
                                                   ((color_dst >> 3) & 0x3))
                            : static_cast<uint8_t>(((color >> 7) & 0xe0) | ((color >> 5) & 0x1c) |
                                                   ((color >> 3) & 0x3));
            src3[pos] = static_cast<uint8_t>(((color >> 7) & 0xe0) | ((color >> 5) & 0x1c) |
                                              ((color >> 3) & 0x3));
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
    uint16_t colorMask = static_cast<uint16_t>(~RGB_LOW_BITS_MASK);

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
    uint16_t colorMask = static_cast<uint16_t>(~RGB_LOW_BITS_MASK);

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

    RegionState& region = regions_[regionId];
    uint8_t* frm1 = region.frm1.get();
    uint8_t* frm2 = region.frm2.get();
    uint8_t* frm3 = region.frm3.get();

    switch (colorDepth) {
        case 32:
#ifdef USE_SSE2
            SmartIB32_SSE2(srcPtr, frm1, frm2, frm3, srcPitch, width, starty, height);
#elif defined(MMX)
            if (cpu_mmx) {
                SmartIB32_MMX(srcPtr, frm1, frm2, frm3, srcPitch, width, starty, height);
            } else {
                SmartIB32_Scalar(srcPtr, frm1, frm2, frm3, srcPitch, width, starty, height);
            }
#else
            SmartIB32_Scalar(srcPtr, frm1, frm2, frm3, srcPitch, width, starty, height);
#endif
            break;

        case 16:
#ifdef USE_SSE2
            SmartIB16_SSE2(srcPtr, frm1, frm2, frm3, srcPitch, width, starty, height);
#elif defined(MMX)
            if (cpu_mmx) {
                SmartIB_MMX(srcPtr, frm1, frm2, frm3, srcPitch, width, starty, height);
            } else {
                SmartIB16_Scalar(srcPtr, frm1, frm2, frm3, srcPitch, width, starty, height);
            }
#else
            SmartIB16_Scalar(srcPtr, frm1, frm2, frm3, srcPitch, width, starty, height);
#endif
            break;

        case 24:
            SmartIB24_Scalar(srcPtr, frm1, frm2, frm3, srcPitch, width, starty, height);
            break;

        case 8:
            SmartIB8_Scalar(srcPtr, frm1, frm2, frm3, srcPitch, width, starty, height);
            break;
    }

    // Rotate buffers for this region
    region.frm1.swap(region.frm3);
    region.frm3.swap(region.frm2);
}

void InterframeManager::ApplyMotionBlurRegion(uint8_t* srcPtr, uint32_t srcPitch,
                                               int width, int starty, int height,
                                               int colorDepth, int regionId) {
    if (!initialized_ || regionId < 0 || regionId >= static_cast<int>(regions_.size())) {
        return;
    }

    RegionState& region = regions_[regionId];
    uint8_t* frm1 = region.frm1.get();

    switch (colorDepth) {
        case 32:
#ifdef USE_SSE2
            MotionBlurIB32_SSE2(srcPtr, frm1, srcPitch, width, starty, height);
#elif defined(MMX)
            if (cpu_mmx) {
                MotionBlurIB32_MMX(srcPtr, frm1, srcPitch, width, starty, height);
            } else {
                MotionBlurIB32_Scalar(srcPtr, frm1, srcPitch, width, starty, height);
            }
#else
            MotionBlurIB32_Scalar(srcPtr, frm1, srcPitch, width, starty, height);
#endif
            break;

        case 16:
#ifdef USE_SSE2
            MotionBlurIB16_SSE2(srcPtr, frm1, srcPitch, width, starty, height);
#elif defined(MMX)
            if (cpu_mmx) {
                MotionBlurIB_MMX(srcPtr, frm1, srcPitch, width, starty, height);
            } else {
                MotionBlurIB16_Scalar(srcPtr, frm1, srcPitch, width, starty, height);
            }
#else
            MotionBlurIB16_Scalar(srcPtr, frm1, srcPitch, width, starty, height);
#endif
            break;

        case 24:
            MotionBlurIB24_Scalar(srcPtr, frm1, srcPitch, width, starty, height);
            break;

        case 8:
            MotionBlurIB8_Scalar(srcPtr, frm1, srcPitch, width, starty, height);
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
