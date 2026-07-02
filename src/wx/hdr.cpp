#include "wx/hdr.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace hdr {

namespace {

Settings g_settings;

bool g_hdr_available = false;
bool g_deep10_available = false;

// 8-bit gamma-2.2 input -> linear [0,1].
float g_lin_lut[256];

// PQ transfer LUT indexed by normalized luminance (nits/10000) in [0,1] ->
// 10-bit PQ code. Applied per channel after the luminance boost.
constexpr int kPqLutSize = 4096;
uint16_t g_pq_lut[kPqLutSize + 1];

// Luma coefficients for the highlight-boost luminance, in the output primaries.
constexpr float kLumaRec709[3]  = {0.2126f, 0.7152f, 0.0722f};
constexpr float kLumaRec2020[3] = {0.2627f, 0.6780f, 0.0593f};
constexpr float kLumaP3[3]      = {0.2290f, 0.6917f, 0.0793f};

// 8x8 ordered (Bayer) dither matrix. The source frame is only 8-bit, and the
// highlight boost has gain > 1, so its quantization steps get stretched and can
// band visibly during fades. Spreading each source step across the higher-
// precision output with a fixed spatial dither dissolves the steps without
// adding temporal shimmer.
constexpr int kBayer8[64] = {
     0, 32,  8, 40,  2, 34, 10, 42,
    48, 16, 56, 24, 50, 18, 58, 26,
    12, 44,  4, 36, 14, 46,  6, 38,
    60, 28, 52, 20, 62, 30, 54, 22,
     3, 35, 11, 43,  1, 33,  9, 41,
    51, 19, 59, 27, 49, 17, 57, 25,
    15, 47,  7, 39, 13, 45,  5, 37,
    63, 31, 55, 23, 61, 29, 53, 21,
};

// Source display gamma the corrected 8-bit buffer is encoded with (see
// filters_agb.cpp display_gamma).
constexpr float kSourceGamma = 2.2f;


// BT.2020 -> BT.709 (linear) primary conversion, for the scRGB path when the
// corrected pixels are in Rec2020 primaries. Row-major.
constexpr float kRec2020ToRec709[3][3] = {
    { 1.66049100f, -0.58764114f, -0.07284986f},
    {-0.12455047f,  1.13289990f, -0.00834942f},
    {-0.01815076f, -0.10057889f,  1.11872966f},
};

// BT.709 -> BT.2020 (linear) primary conversion, for the PQ path when the
// corrected pixels are still in Rec.709/sRGB primaries. Row-major.
constexpr float kRec709ToRec2020[3][3] = {
    {0.62740390f, 0.32928304f, 0.04331307f},
    {0.06909729f, 0.91954040f, 0.01136232f},
    {0.01639144f, 0.08801331f, 0.89559525f},
};

// Linear primary conversions to Display P3 (D65), for the macOS EDR scRGB path
// when the surface is extended-linear Display P3. Row-major.
constexpr float kRec709ToDisplayP3[3][3] = {
    {0.82246197f, 0.17753803f, 0.00000000f},
    {0.03319420f, 0.96680580f, 0.00000000f},
    {0.01708263f, 0.07239744f, 0.91051993f},
};
constexpr float kRec2020ToDisplayP3[3][3] = {
    { 1.34357825f, -0.28217967f, -0.06125249f},
    {-0.06529745f,  1.07578792f, -0.01045499f},
    { 0.00282179f, -0.01959644f,  1.01685063f},
};

// SMPTE ST.2084 (PQ) inverse-EOTF: linear value normalized so 1.0 == 10000
// nits -> normalized code value in [0,1].
float PqOetf(float l) {
    constexpr float m1 = 2610.0f / 16384.0f;
    constexpr float m2 = 2523.0f / 4096.0f * 128.0f;
    constexpr float c1 = 3424.0f / 4096.0f;
    constexpr float c2 = 2413.0f / 4096.0f * 32.0f;
    constexpr float c3 = 2392.0f / 4096.0f * 32.0f;
    l = std::max(0.0f, std::min(1.0f, l));
    const float lp = std::pow(l, m1);
    return std::pow((c1 + c2 * lp) / (1.0f + c3 * lp), m2);
}

// Luminance-based highlight boost. Given a pixel's luminance L in [0,1]
// (linear, in the output primaries), return the scalar every channel is
// multiplied by to produce nits. Applying one scale to all channels preserves
// hue, so fades stay neutral and bright whites brighten cleanly.
//
// The luminance->nits transfer is reference*L below the knee, then a smooth
// (C1) cubic-Hermite shoulder up to peak. The shoulder's slope at the knee
// equals the linear slope (reference) and eases to a flat top at peak, so
// there is no slope discontinuity -- avoiding the brightness inflection a
// piecewise-linear knee produced during fades.
float LumScale(float L) {
    const float ref = g_settings.sdr_reference_nits;
    const float peak = std::max(ref, g_settings.peak_nits);
    const float knee = std::max(0.0f, std::min(1.0f, g_settings.highlight_knee));
    const float sc = g_settings.shadow_contrast;

    if (knee >= 1.0f || L <= knee) {
        // Below the knee the transfer is linear (outL = ref * L). Optionally
        // shape it with a gamma pivoted at the knee to deepen shadows: at the
        // pivot the value is unchanged (continuity into the shoulder), and for
        // L below it sc > 1 pulls the output down, steepening the low end. The
        // returned value is a per-pixel scale on the channels, so hue is
        // preserved as with the highlight boost.
        if (sc != 1.0f && L > 0.0f) {
            const float pivot = knee > 0.0f ? knee : 1.0f;
            const float shaped = pivot * std::pow(L / pivot, sc);
            return ref * shaped / L;
        }
        return ref;  // outL = ref * L
    }

    const float dx = 1.0f - knee;
    const float t = (L - knee) / dx;
    const float t2 = t * t;
    const float t3 = t2 * t;
    // Cubic Hermite from (knee, ref*knee) to (1, peak): slope ref at the knee
    // (tangent scaled by the interval), zero slope at the top.
    const float p0 = ref * knee;
    const float p1 = peak;
    const float m0 = ref * dx;
    const float h00 = 2.0f * t3 - 3.0f * t2 + 1.0f;
    const float h10 = t3 - 2.0f * t2 + t;
    const float h01 = -2.0f * t3 + 3.0f * t2;
    const float out_nits = h00 * p0 + h10 * m0 + h01 * p1;  // h11*m1, m1 = 0
    return out_nits / L;  // L > knee > 0, so safe
}

// IEEE-754 binary16 encode of a finite float. Handles the small magnitudes and
// the >1.0 / negative values scRGB produces.
uint16_t FloatToHalf(float f) {
    uint32_t x;
    std::memcpy(&x, &f, sizeof(x));
    const uint32_t sign = (x >> 16) & 0x8000u;
    int32_t exp = static_cast<int32_t>((x >> 23) & 0xff) - 127 + 15;
    const uint32_t mant = x & 0x7fffffu;

    if (((x >> 23) & 0xff) == 0xff) {
        // Inf / NaN.
        return static_cast<uint16_t>(sign | 0x7c00u | (mant ? 0x200u : 0u));
    }
    if (exp >= 0x1f) {
        // Overflow -> Inf.
        return static_cast<uint16_t>(sign | 0x7c00u);
    }
    if (exp <= 0) {
        if (exp < -10)
            return static_cast<uint16_t>(sign);  // Underflow -> +-0.
        // Subnormal half.
        const uint32_t m = (mant | 0x800000u) >> (1 - exp);
        return static_cast<uint16_t>(sign | (m >> 13));
    }
    return static_cast<uint16_t>(sign | (static_cast<uint32_t>(exp) << 10) |
                                 (mant >> 13));
}

}  // namespace

void Configure(const Settings& s) {
    g_settings = s;
    for (int v = 0; v < 256; ++v)
        g_lin_lut[v] = std::pow(v / 255.0f, kSourceGamma);
    for (int i = 0; i <= kPqLutSize; ++i) {
        const float code = PqOetf(static_cast<float>(i) / kPqLutSize);
        g_pq_lut[i] = static_cast<uint16_t>(
            std::min(1023.0f, std::max(0.0f, code * 1023.0f + 0.5f)));
    }
}

const Settings& CurrentSettings() {
    return g_settings;
}

// DetectAvailability() lives in the platform layer (panel.cpp / macsupport.mm)
// where the Vulkan/DXGI/Wayland/X11 headers are; it calls SetAvailability().
void SetAvailability(bool hdr_available, bool deep10_available) {
    g_hdr_available = hdr_available;
    g_deep10_available = deep10_available;
}

bool HdrAvailable() {
    return g_hdr_available;
}

bool DeepColor10Available() {
    return g_deep10_available;
}

// Pack three 10-bit codes as A2B10G10R10 (R in bits 0-9, opaque alpha).
static inline uint32_t PackA2B10G10R10(uint32_t r, uint32_t g, uint32_t b) {
    return (3u << 30) | (b << 20) | (g << 10) | r;
}

// Linearize an 8-bit channel value with ordered dither: offset by up to +-half
// a source step (using the pixel's Bayer threshold d in [0,1)) and interpolate,
// so the 8-bit gradient is spread across the higher-precision output. Neutral
// for equal channels (no color noise).
static inline float DitherLin(int v, float d) {
    const float lo = g_lin_lut[v];
    const float hi = g_lin_lut[v < 255 ? v + 1 : 255];
    return lo + (d - 0.5f) * (hi - lo);
}

// nits -> 10-bit PQ code via the precomputed LUT (nits clamped to 0..10000).
static inline uint32_t PqCode(float nits) {
    int idx = static_cast<int>(nits * (kPqLutSize / 10000.0f) + 0.5f);
    if (idx < 0) idx = 0;
    if (idx > kPqLutSize) idx = kPqLutSize;
    return g_pq_lut[idx];
}

void EncodePQ10(const uint8_t* src, int src_stride, int row_pixels, int rows,
                uint32_t* dst, int dst_stride) {
    // Output is BT.2020 PQ. Convert Rec.709/sRGB input to BT.2020 in linear
    // light first; then a luminance-based boost (hue-preserving) and PQ encode.
    const bool convert = !g_settings.input_is_rec2020;
    for (int y = 0; y < rows; ++y) {
        const uint8_t* sp = src + static_cast<size_t>(y) * src_stride;
        uint32_t* dp = reinterpret_cast<uint32_t*>(
            reinterpret_cast<uint8_t*>(dst) + static_cast<size_t>(y) * dst_stride);
        for (int x = 0; x < row_pixels; ++x, sp += 4) {
            const float d = (kBayer8[(y & 7) * 8 + (x & 7)] + 0.5f) * (1.0f / 64.0f);
            float r = DitherLin(sp[0], d);
            float g = DitherLin(sp[1], d);
            float b = DitherLin(sp[2], d);
            if (convert) {
                const float nr = kRec709ToRec2020[0][0] * r + kRec709ToRec2020[0][1] * g + kRec709ToRec2020[0][2] * b;
                const float ng = kRec709ToRec2020[1][0] * r + kRec709ToRec2020[1][1] * g + kRec709ToRec2020[1][2] * b;
                const float nb = kRec709ToRec2020[2][0] * r + kRec709ToRec2020[2][1] * g + kRec709ToRec2020[2][2] * b;
                r = nr; g = ng; b = nb;
            }
            const float L = kLumaRec2020[0] * r + kLumaRec2020[1] * g + kLumaRec2020[2] * b;
            const float s = LumScale(L);
            dp[x] = PackA2B10G10R10(PqCode(r * s), PqCode(g * s), PqCode(b * s));
        }
    }
}

void EncodeScRGBFp16(const uint8_t* src, int src_stride, int row_pixels, int rows,
                     uint16_t* dst, int dst_stride) {
    const float white = g_settings.scrgb_white_nits > 0.0f
                            ? g_settings.scrgb_white_nits : 80.0f;

    // Pick the linear primary conversion from the input gamut to the target
    // scRGB gamut. null means identity (Rec.709 in, Rec.709 out).
    const float (*m)[3] = nullptr;
    if (g_settings.scrgb_target_p3)
        m = g_settings.input_is_rec2020 ? kRec2020ToDisplayP3 : kRec709ToDisplayP3;
    else if (g_settings.input_is_rec2020)
        m = kRec2020ToRec709;
    const float* lw = g_settings.scrgb_target_p3 ? kLumaP3 : kLumaRec709;

    for (int y = 0; y < rows; ++y) {
        const uint8_t* sp = src + static_cast<size_t>(y) * src_stride;
        uint16_t* dp = reinterpret_cast<uint16_t*>(
            reinterpret_cast<uint8_t*>(dst) + static_cast<size_t>(y) * dst_stride);
        for (int x = 0; x < row_pixels; ++x, sp += 4) {
            // Dithered linearize, then convert to the target primaries.
            const float d = (kBayer8[(y & 7) * 8 + (x & 7)] + 0.5f) * (1.0f / 64.0f);
            float r = DitherLin(sp[0], d);
            float g = DitherLin(sp[1], d);
            float b = DitherLin(sp[2], d);
            if (m) {
                const float nr = m[0][0] * r + m[0][1] * g + m[0][2] * b;
                const float ng = m[1][0] * r + m[1][1] * g + m[1][2] * b;
                const float nb = m[2][0] * r + m[2][1] * g + m[2][2] * b;
                r = nr; g = ng; b = nb;
            }

            // Hue-preserving luminance boost -> nits, then scRGB (1.0 == white).
            const float s = LumScale(lw[0] * r + lw[1] * g + lw[2] * b);
            uint16_t* px = dp + static_cast<size_t>(x) * 4;
            px[0] = FloatToHalf(r * s / white);
            px[1] = FloatToHalf(g * s / white);
            px[2] = FloatToHalf(b * s / white);
            px[3] = FloatToHalf(1.0f);
        }
    }
}

}  // namespace hdr
