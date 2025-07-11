#include <xmmintrin.h>
#include <cmath>

// Compute the square root using the Newton-Raphson method with SSE
__forceinline __m128 sqrt_sse(__m128 x) {
    const __m128 one_half = _mm_set1_ps(0.5f);
    const __m128 three_halves = _mm_set1_ps(1.5f);
    const __m128 zero = _mm_set1_ps(0.0f);

    // Initial guess (approximation)
    __m128 guess = _mm_rsqrt_ps(x);

    // Newton-Raphson iterations for precision
    for (int i = 0; i < 3; ++i) {
        guess = _mm_mul_ps(guess, _mm_sub_ps(three_halves, _mm_mul_ps(one_half, _mm_mul_ps(guess, _mm_mul_ps(guess, x)))));
    }

    // Ensure that we don't process zero values to avoid artifacts
    __m128 mask = _mm_cmpneq_ps(x, zero);
    return _mm_and_ps(mask, _mm_mul_ps(x, guess));
}

void process_sqrt(float* input, float* output, size_t count) {
    size_t i;
    for (i = 0; i < count; i += 4) {
        __m128 x = _mm_loadu_ps(&input[i]);
        __m128 result = sqrt_sse(x);
        _mm_storeu_ps(&output[i], result);
    }

    // Handle any remaining elements if count is not a multiple of 4
    for (; i < count; ++i) {
        __m128 x = _mm_set1_ps(input[i]);
        __m128 result = sqrt_sse(x);
        output[i] = _mm_cvtss_f32(result);
    }
}

double sqrt_sse(float n) {
    if (n == 0.0f) return 0.0; // Handle zero input to avoid artifacts
    __m128 x = _mm_set1_ps(n);
    __m128 result = sqrt_sse(x);
    return static_cast<double>(_mm_cvtss_f32(result));
}
