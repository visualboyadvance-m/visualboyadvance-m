#include "wx/hdr.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <functional>
#include <atomic>
#include <memory>
#ifdef __APPLE__
#include <dispatch/dispatch.h>
#else
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>
#endif

namespace hdr {

namespace {

Settings g_settings;

// Moves on every Configure(), so the encoders' remembered scales (ScaleCache) are
// dropped when what they were computed from changes.
std::atomic<unsigned> g_generation{0};

bool g_hdr_available = false;
bool g_deep10_available = false;
bool g_window_on_hdr = true;

// 8-bit gamma-2.2 input -> linear [0,1].
float g_lin_lut[256];

// DitherLin() by Bayer threshold, a row of codes for each of the 64: one lookup a
// channel in the encoders instead of two and the arithmetic. Built by Configure().
float g_dither_lin[64][256];

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
// The nits the bottom of the transfer is anchored to. The display's reported
// black floor, but never more than a small fraction of the below-knee range:
// a real floor is a fraction of a nit against a `top` of a hundred or more, so
// this never binds in practice and only keeps a nonsense value from a
// compositor or a driver from washing the whole picture out.
float ShadowFloor(float top) {
    const float m = g_settings.display_min_nits;
    if (!(m > 0.0f) || top <= 0.0f)
        return 0.0f;
    return std::min(m, top * 0.05f);
}

// `top` is the pixel's largest channel, linear: it decides how much of the
// display's black floor the pixel gets (see below).
float LumScale(float L, float top_channel) {
    const float ref = g_settings.sdr_reference_nits;
    const float peak = std::max(ref, g_settings.peak_nits);
    const float knee = std::max(0.0f, std::min(1.0f, g_settings.highlight_knee));
    const float sc = g_settings.shadow_contrast;

    if (knee >= 1.0f || L <= knee) {
        if (L <= 0.0f)
            return ref;  // every channel is 0; the scale cannot matter.

        // Below the knee the transfer runs from the display's black floor at
        // L = 0 up to ref * pivot at the pivot, where it meets the shoulder.
        // Optionally shaped by a gamma to deepen shadows: sc > 1 pulls the
        // output down and steepens the low end. The returned value is a
        // per-pixel scale on the channels, so hue is preserved as with the
        // highlight boost.
        const float pivot = knee > 0.0f ? knee : 1.0f;
        const float top = ref * pivot;  // nits where the shoulder takes over
        const float floor_nits = ShadowFloor(top);
        if (floor_nits <= 0.0f) {
            // No usable floor reported: the original transfer, anchored at 0.
            if (sc != 1.0f)
                return ref * pivot * std::pow(L / pivot, sc) / L;
            return ref;  // outL = ref * L
        }
        const float t = sc != 1.0f ? std::pow(L / pivot, sc) : L / pivot;
        // The floor fades in over the darkest step a 5-bit source has (8 of
        // 255) in the pixel's largest channel, not at the first value above 0.
        // Lifted at once, the faint values a filter such as DLSS NR leaves in
        // a black background -- 1 or 2 of 255 in one channel -- each became a
        // dot at the floor, in that channel's colour, since the scale applies
        // to every channel. By the largest channel rather than the luminance,
        // so the game's darkest blue gets the whole floor as before.
        const float ramp = std::min(1.0f, top_channel / g_lin_lut[8]);
        // t == 1 at the pivot, so this still lands on ref * pivot there and
        // the shoulder joins without a step.
        return (floor_nits * ramp + (top - floor_nits) * t) / L;
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

// IEEE-754 binary16 encode of a float, truncating. Handles the small magnitudes and
// the >1.0 / negative values scRGB produces. Without a branch, so the loops that
// convert a run of values with it vectorize (SSE2, AVX, NEON): at a branch a
// pixel and channel it was most of the scRGB encode.
inline uint16_t FloatToHalf(float f) {
    uint32_t x;
    std::memcpy(&x, &f, sizeof(x));
    const uint32_t sign = (x >> 16) & 0x8000u;
    const uint32_t ax = x & 0x7fffffffu;
    const uint32_t e8 = ax >> 23;
    float af;
    std::memcpy(&af, &ax, sizeof(af));

    // A normal half: the exponent rebiased from 127 to 15, 13 bits of mantissa
    // dropped.
    const uint32_t normal = (ax >> 13) - (112u << 10);
    // A subnormal half, or zero below it: the magnitude in units of 2^-24, cut
    // to an integer -- what shifting the mantissa down gives. Kept below 2^24 so
    // the conversion is defined where the lane is not used.
    const float sub_f = (af < 1.0f ? af : 1.0f) * 16777216.0f;
    const uint32_t sub = static_cast<uint32_t>(static_cast<int32_t>(sub_f));

    uint32_t h = e8 <= 112 ? sub : normal;
    h = e8 >= 143 ? 0x7c00u : h;                                // overflow -> Inf
    h = e8 == 255 && (ax & 0x7fffffu) ? 0x7e00u : h;            // NaN
    return static_cast<uint16_t>(sign | h);
}

}  // namespace

static float DitherLin(int v, float d);

void Configure(const Settings& s) {
    g_settings = s;
    g_generation.fetch_add(1, std::memory_order_relaxed);
    for (int v = 0; v < 256; ++v)
        g_lin_lut[v] = std::pow(v / 255.0f, kSourceGamma);
    for (int t = 0; t < 64; ++t) {
        const float d = (kBayer8[t] + 0.5f) * (1.0f / 64.0f);
        for (int v = 0; v < 256; ++v)
            g_dither_lin[t][v] = DitherLin(v, d);
    }
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

bool WindowOnHdrDisplays() {
    return g_window_on_hdr;
}

void SetWindowOnHdrDisplays(bool on_hdr) {
    g_window_on_hdr = on_hdr;
}

bool HdrForWindow() {
    return g_hdr_available && g_window_on_hdr;
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
//
// Black is not dithered: there is nothing below it to spread toward, and the
// half step under 0 would swing every other pixel between negative (clamped
// to 0 nits) and positive -- which LumScale() lifts to the display's black
// floor, so a black area came out as a Bayer grid of 0 and floor nits. Code 0
// stays exactly 0, and the floor starts at code 1. Code 1's half step reaches
// below 0 too, so the result is clamped there.
static float DitherLin(int v, float d) {
    if (v == 0)
        return 0.0f;
    const float lo = g_lin_lut[v];
    const float hi = g_lin_lut[v < 255 ? v + 1 : 255];
    return std::max(0.0f, lo + (d - 0.5f) * (hi - lo));
}

// The scale LumScale() gives a pixel, from its undithered linear channels after
// the primaries' matrix m (null for none). Taken from the dither, the scale --
// floor / L at the dark end -- followed each pixel's dithered L, and a dark
// pixel whose channels the Bayer threshold pulled to 0 or near it was blown up
// into a dot at the floor, in whatever hue the dither left: a grid of coloured
// dots over the near-black a filter such as DLSS NR leaves.
static inline float PixelScale(const uint8_t* sp, const float (*m)[3], const float* lw) {
    float r = g_lin_lut[sp[0]], g = g_lin_lut[sp[1]], b = g_lin_lut[sp[2]];
    if (m) {
        const float nr = m[0][0] * r + m[0][1] * g + m[0][2] * b;
        const float ng = m[1][0] * r + m[1][1] * g + m[1][2] * b;
        const float nb = m[2][0] * r + m[2][1] * g + m[2][2] * b;
        r = nr; g = ng; b = nb;
    }
    return LumScale(lw[0] * r + lw[1] * g + lw[2] * b, std::max(r, std::max(g, b)));
}

// PixelScale() by colour, one table a thread and encoder. The scale depends only on the
// pixel's three 8-bit channels and the settings, and a frame has far fewer colours than
// pixels: at xBRZ 6x a 1440x960 frame was 63 ms of CPU time to encode, a third of it in
// LumScale()'s pow(); remembered, half that. The output is the same but where the compiler
// rounds the inlined scale differently: none of 1.38 million pixels on a Mario 3 frame at
// three shadow contrasts, one on an Advance Wars frame, by 2 of 1023 in one channel.
class ScaleCache {
public:
    // Ready for an encode under the current settings.
    void Begin() {
        const unsigned gen = g_generation.load(std::memory_order_relaxed);
        if (!keys_) {
            keys_.reset(new uint32_t[kEntries]);
            scales_.reset(new float[kEntries]);
            generation_ = gen + 1;
        }
        if (generation_ != gen) {
            std::memset(keys_.get(), 0, sizeof(uint32_t) * kEntries);
            generation_ = gen;
        }
        last_key_ = 0;
    }

    float Get(const uint8_t* sp, const float (*m)[3], const float* lw) {
        // + 1 so that no colour's key is the empty slot's 0.
        const uint32_t key =
            (static_cast<uint32_t>(sp[0]) | (static_cast<uint32_t>(sp[1]) << 8) |
             (static_cast<uint32_t>(sp[2]) << 16)) + 1;
        // A filtered frame has long runs of one colour.
        if (key == last_key_)
            return last_scale_;
        last_key_ = key;
        const uint32_t i = (key * 2654435761u) >> (32 - kBits);
        if (keys_[i] != key) {
            keys_[i] = key;
            scales_[i] = PixelScale(sp, m, lw);
        }
        return last_scale_ = scales_[i];
    }

private:
    static constexpr int kBits = 15;
    static constexpr uint32_t kEntries = 1u << kBits;
    std::unique_ptr<uint32_t[]> keys_;
    std::unique_ptr<float[]> scales_;
    unsigned generation_ = 0;
    uint32_t last_key_ = 0;
    float last_scale_ = 0.0f;
};

// nits -> the index of its 10-bit PQ code in the precomputed LUT (nits clamped to
// 0..10000). Clamped before the conversion to int, which has no branch to vectorize
// around and is defined for any nits.
static inline int PqIndex(float nits) {
    float f = nits * (kPqLutSize / 10000.0f) + 0.5f;
    f = f > 0.0f ? f : 0.0f;
    f = f < static_cast<float>(kPqLutSize) ? f : static_cast<float>(kPqLutSize);
    return static_cast<int>(f);
}

#ifndef __APPLE__
// The worker threads for ForRowBands() where there is no GCD. They sleep on a
// condition variable between frames, and the calling thread takes rows too.
class RowBandPool {
public:
    // Never destroyed: the workers sleep until the process exits, so nothing
    // has to join them from a static destructor, where Windows can deadlock.
    static RowBandPool& Get() {
        static RowBandPool* pool = new RowBandPool;
        return *pool;
    }

    // Calls fn over chunks of [0, rows), each drawn by whichever thread comes
    // free first, and returns once all of them have finished.
    void Run(int rows, const std::function<void(int, int)>& fn) {
        if (workers_ == 0 || rows < 2 * (workers_ + 1)) {
            fn(0, rows);
            return;
        }
        // One frame at a time; a second caller waits for the first.
        std::lock_guard<std::mutex> one(run_mutex_);
        {
            std::lock_guard<std::mutex> lock(mutex_);
            fn_ = &fn;
            rows_ = rows;
            // Four chunks a thread, so a thread that starts late, or whose rows
            // are dearer, does not hold up the rest.
            chunk_ = std::max(1, rows / ((workers_ + 1) * 4));
            next_row_.store(0, std::memory_order_relaxed);
            remaining_ = workers_;
            ++generation_;
        }
        start_.notify_all();
        Take();
        std::unique_lock<std::mutex> lock(mutex_);
        done_.wait(lock, [this] { return remaining_ == 0; });
        fn_ = nullptr;
    }

private:
    RowBandPool() {
        const unsigned hw = std::thread::hardware_concurrency();
        // As many bands as GCD runs on macOS, at most: past that, waking the
        // threads costs more than a 480x320 frame's rows save.
        workers_ = std::min(hw ? static_cast<int>(hw) : 1, 16) - 1;
        for (int i = 0; i < workers_; ++i)
            std::thread([this] { Work(); }).detach();
    }

    void Take() {
        for (;;) {
            const int y = next_row_.fetch_add(chunk_, std::memory_order_relaxed);
            if (y >= rows_)
                return;
            (*fn_)(y, std::min(rows_, y + chunk_));
        }
    }

    void Work() {
        unsigned seen = 0;
        for (;;) {
            {
                std::unique_lock<std::mutex> lock(mutex_);
                start_.wait(lock, [&] { return generation_ != seen; });
                seen = generation_;
            }
            Take();
            {
                std::lock_guard<std::mutex> lock(mutex_);
                if (--remaining_ == 0)
                    done_.notify_one();
            }
        }
    }

    int workers_ = 0;
    std::mutex run_mutex_;
    std::mutex mutex_;
    std::condition_variable start_;
    std::condition_variable done_;
    unsigned generation_ = 0;
    int remaining_ = 0;
    // Written under mutex_ before generation_ moves, read by the workers after
    // they have seen it move, so the mutex orders them.
    const std::function<void(int, int)>* fn_ = nullptr;
    int rows_ = 0;
    int chunk_ = 1;
    std::atomic<int> next_row_{0};
};
#endif

// Calls fn(y0, y1) over bands of rows covering [0, rows). The encoders run once a
// frame on the emulation thread, and at the curve's settings LumScale() takes a
// pow() a pixel: about 8 ms for a 480x320 frame on a 2018 Intel Mac mini, half a
// 60 Hz frame. Every row depends only on its own input, so the bands write exactly
// what one loop would. GCD's pool on macOS, RowBandPool elsewhere.
static void ForRowBands(int rows, const std::function<void(int, int)>& fn) {
#ifdef __APPLE__
    const int bands = std::min(rows, 16);
    if (bands > 1) {
        const std::function<void(int, int)>* f = &fn;
        dispatch_apply(static_cast<size_t>(bands), DISPATCH_APPLY_AUTO, ^(size_t i) {
            const int y0 = static_cast<int>(static_cast<long long>(rows) * i / bands);
            const int y1 = static_cast<int>(static_cast<long long>(rows) * (i + 1) / bands);
            (*f)(y0, y1);
        });
        return;
    }
    fn(0, rows);
#else
    RowBandPool::Get().Run(rows, fn);
#endif
}

void EncodePQ10(const uint8_t* src, int src_stride, int row_pixels, int rows,
                uint32_t* dst, int dst_stride) {
    // Output is BT.2020 PQ. Convert Rec.709/sRGB input to BT.2020 in linear
    // light first; then a luminance-based boost (hue-preserving) and PQ encode.
    const bool convert = !g_settings.input_is_rec2020;
    const float (*m)[3] = convert ? kRec709ToRec2020 : nullptr;
    ForRowBands(rows, [&](int y_begin, int y_end) {
    thread_local ScaleCache cache;
    cache.Begin();
    // By blocks, as EncodeScRGBFp16(): the lookups a pixel at a time, the
    // arithmetic over the block.
    constexpr int kBlock = 32;
    float lr[kBlock], lg[kBlock], lb[kBlock], ls[kBlock];
    int ir[kBlock], ig[kBlock], ib[kBlock];
    for (int y = y_begin; y < y_end; ++y) {
        const uint8_t* sp = src + static_cast<size_t>(y) * src_stride;
        uint32_t* dp = reinterpret_cast<uint32_t*>(
            reinterpret_cast<uint8_t*>(dst) + static_cast<size_t>(y) * dst_stride);
        const float (*dither)[256] = g_dither_lin + (y & 7) * 8;
        for (int x0 = 0; x0 < row_pixels; x0 += kBlock) {
            const int n = std::min(kBlock, row_pixels - x0);

            for (int i = 0; i < n; ++i, sp += 4) {
                const float* dl = dither[(x0 + i) & 7];
                lr[i] = dl[sp[0]];
                lg[i] = dl[sp[1]];
                lb[i] = dl[sp[2]];
                ls[i] = cache.Get(sp, m, kLumaRec2020);
            }

            if (m) {
                const float m00 = m[0][0], m01 = m[0][1], m02 = m[0][2];
                const float m10 = m[1][0], m11 = m[1][1], m12 = m[1][2];
                const float m20 = m[2][0], m21 = m[2][1], m22 = m[2][2];
                for (int i = 0; i < n; ++i) {
                    const float r = lr[i], g = lg[i], b = lb[i];
                    lr[i] = m00 * r + m01 * g + m02 * b;
                    lg[i] = m10 * r + m11 * g + m12 * b;
                    lb[i] = m20 * r + m21 * g + m22 * b;
                }
            }

            for (int i = 0; i < n; ++i) {
                ir[i] = PqIndex(lr[i] * ls[i]);
                ig[i] = PqIndex(lg[i] * ls[i]);
                ib[i] = PqIndex(lb[i] * ls[i]);
            }

            for (int i = 0; i < n; ++i)
                dp[x0 + i] = PackA2B10G10R10(g_pq_lut[ir[i]], g_pq_lut[ig[i]], g_pq_lut[ib[i]]);
        }
    }
    });
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

    ForRowBands(rows, [&](int y_begin, int y_end) {
    thread_local ScaleCache cache;
    cache.Begin();
    // A block of pixels at a time: the lookups a pixel at a time, then the
    // arithmetic and the conversion to halves over the whole block, which
    // vectorize. The same operations in the same order, so the same output.
    constexpr int kBlock = 32;
    float lr[kBlock], lg[kBlock], lb[kBlock], ls[kBlock];
    float px[kBlock * 4];
    for (int y = y_begin; y < y_end; ++y) {
        const uint8_t* sp = src + static_cast<size_t>(y) * src_stride;
        uint16_t* dp = reinterpret_cast<uint16_t*>(
            reinterpret_cast<uint8_t*>(dst) + static_cast<size_t>(y) * dst_stride);
        const float (*dither)[256] = g_dither_lin + (y & 7) * 8;
        for (int x0 = 0; x0 < row_pixels; x0 += kBlock) {
            const int n = std::min(kBlock, row_pixels - x0);

            // Dithered linearize, and the scale from the undithered pixel.
            for (int i = 0; i < n; ++i, sp += 4) {
                const float* dl = dither[(x0 + i) & 7];
                lr[i] = dl[sp[0]];
                lg[i] = dl[sp[1]];
                lb[i] = dl[sp[2]];
                ls[i] = cache.Get(sp, m, lw);
            }

            // Convert to the target primaries.
            if (m) {
                const float m00 = m[0][0], m01 = m[0][1], m02 = m[0][2];
                const float m10 = m[1][0], m11 = m[1][1], m12 = m[1][2];
                const float m20 = m[2][0], m21 = m[2][1], m22 = m[2][2];
                for (int i = 0; i < n; ++i) {
                    const float r = lr[i], g = lg[i], b = lb[i];
                    lr[i] = m00 * r + m01 * g + m02 * b;
                    lg[i] = m10 * r + m11 * g + m12 * b;
                    lb[i] = m20 * r + m21 * g + m22 * b;
                }
            }

            // Hue-preserving luminance boost -> nits, then scRGB (1.0 == white).
            for (int i = 0; i < n; ++i) {
                px[i * 4 + 0] = lr[i] * ls[i] / white;
                px[i * 4 + 1] = lg[i] * ls[i] / white;
                px[i * 4 + 2] = lb[i] * ls[i] / white;
                px[i * 4 + 3] = 1.0f;
            }

            uint16_t* out = dp + static_cast<size_t>(x0) * 4;
            for (int i = 0; i < n * 4; ++i)
                out[i] = FloatToHalf(px[i]);
        }
    }
    });
}

GpuTransfer GpuTransferParams(Encoding encoding) {
    GpuTransfer t = {};
    std::memcpy(t.lin, g_lin_lut, sizeof(t.lin));

    // As the encoders pick them.
    const float (*m)[3] = nullptr;
    const float* lw = nullptr;
    if (encoding == Encoding::kPQ10) {
        if (!g_settings.input_is_rec2020)
            m = kRec709ToRec2020;
        lw = kLumaRec2020;
    } else {
        if (g_settings.scrgb_target_p3)
            m = g_settings.input_is_rec2020 ? kRec2020ToDisplayP3 : kRec709ToDisplayP3;
        else if (g_settings.input_is_rec2020)
            m = kRec2020ToRec709;
        lw = g_settings.scrgb_target_p3 ? kLumaP3 : kLumaRec709;
    }
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c)
            t.m[r][c] = m ? m[r][c] : (r == c ? 1.0f : 0.0f);
        t.luma[r] = lw[r];
    }

    // As LumScale() derives them.
    t.ref = g_settings.sdr_reference_nits;
    t.peak = std::max(t.ref, g_settings.peak_nits);
    t.knee = std::max(0.0f, std::min(1.0f, g_settings.highlight_knee));
    t.shadow_contrast = g_settings.shadow_contrast;
    t.floor_nits = ShadowFloor(t.ref * (t.knee > 0.0f ? t.knee : 1.0f));
    t.ramp_top = g_lin_lut[8];
    t.white = g_settings.scrgb_white_nits > 0.0f ? g_settings.scrgb_white_nits : 80.0f;
    return t;
}

}  // namespace hdr
