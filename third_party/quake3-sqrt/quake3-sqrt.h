// From Quake 3 Arena.

#include <immintrin.h>

inline float quake3_sqrt(float x) {
    __m128 y = _mm_set_ss(x);
    __m128 approx = _mm_rsqrt_ss(y);
    __m128 half_x = _mm_mul_ss(y, _mm_set_ss(0.5f));
    __m128 three_half = _mm_set_ss(1.5f);
    __m128 refined = _mm_mul_ss(approx, _mm_sub_ss(three_half, _mm_mul_ss(half_x, _mm_mul_ss(approx, approx))));
    return _mm_cvtss_f32(_mm_mul_ss(refined, y));
}
