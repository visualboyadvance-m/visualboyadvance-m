// Display filter tests: every filter's SIMD build (SSE2 on x86, NEON on
// AArch64) against its scalar kernels, run through the same entry points with
// vbam_simd::ForceScalar() flipped. Covers the 2x / 3x / 4x scalers at 16 and
// 32 bits, ScaleFX, and the interframe blenders at 8 / 16 / 24 / 32 bits.
//
// Set VBAM_FILTER_BENCH=1 to also run the Bench cases, which print the time
// per 240x160 frame for every filter, SIMD and scalar.

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "../filters.h"
#include "../simd128.h"
#include "components/filters_interframe/interframe.h"
#include "components/filters_scalefx/scalefx.h"

// The globals the filters take from the core
int RGB_LOW_BITS_MASK = 0x010101;
int systemRedShift = 19;
int systemGreenShift = 11;
int systemBlueShift = 3;
int systemColorDepth = 32;

namespace {

using FilterFn = void (*)(uint8_t*, uint32_t, uint8_t*, uint8_t*, uint32_t, int, int);

enum Format { kRgb32, kRgb565, kRgb555 };

struct FilterCase {
    const char* name;
    FilterFn fn;
    int scale;
    bool wide;  // 32-bit; otherwise 16-bit
};

const FilterCase kFilters[] = {
    {"Simple2x32", Simple2x32, 2, true},       {"Simple2x16", Simple2x16, 2, false},
    {"Simple3x32", Simple3x32, 3, true},       {"Simple3x16", Simple3x16, 3, false},
    {"Simple4x32", Simple4x32, 4, true},       {"Simple4x16", Simple4x16, 4, false},
    {"Scanlines32", Scanlines32, 2, true},     {"Scanlines", Scanlines, 2, false},
    {"ScanlinesTV32", ScanlinesTV32, 2, true}, {"ScanlinesTV", ScanlinesTV, 2, false},
    {"Pixelate32", Pixelate32, 2, true},       {"Pixelate", Pixelate, 2, false},
    {"AdMame2x32", AdMame2x32, 2, true},       {"AdMame2x", AdMame2x, 2, false},
    {"Bilinear32", Bilinear32, 2, true},       {"Bilinear", Bilinear, 2, false},
    {"BilinearPlus32", BilinearPlus32, 2, true}, {"BilinearPlus", BilinearPlus, 2, false},
    {"_2xSaI32", _2xSaI32, 2, true},           {"_2xSaI", _2xSaI, 2, false},
    {"Super2xSaI32", Super2xSaI32, 2, true},   {"Super2xSaI", Super2xSaI, 2, false},
    {"SuperEagle32", SuperEagle32, 2, true},   {"SuperEagle", SuperEagle, 2, false},
    {"hq2x32", hq2x32, 2, true},               {"hq2x", hq2x, 2, false},
    {"lq2x32", lq2x32, 2, true},               {"lq2x", lq2x, 2, false},
    {"hq3x32_32", hq3x32_32, 3, true},         {"hq3x16", hq3x16, 3, false},
    {"hq4x32_32", hq4x32_32, 4, true},         {"hq4x16", hq4x16, 4, false},
    {"scalefx3x32", scalefx3x32, 3, true},     {"scalefx9x32", scalefx9x32, 9, true},
};

uint32_t Rgb(uint32_t r, uint32_t g, uint32_t b) { return (r << 16) | (g << 8) | b; }

// 32-bit pictures: noise, and pixel art with a small palette, shapes, lines
// and speckle (where the edge rules of the smart scalers fire)
std::vector<uint32_t> Noise32(int w, int h, uint32_t seed) {
    std::vector<uint32_t> img(w * h);
    std::mt19937 rng(seed);
    for (auto& p : img)
        p = rng() & 0x00FFFFFF;
    return img;
}

std::vector<uint32_t> PixelArt32(int w, int h, uint32_t seed) {
    std::vector<uint32_t> img(w * h);
    std::mt19937 rng(seed);
    const uint32_t palette[] = {Rgb(0, 0, 0),       Rgb(255, 255, 255), Rgb(248, 56, 0),   Rgb(252, 152, 56),
                                Rgb(0, 168, 0),     Rgb(0, 88, 248),    Rgb(255, 216, 0),  Rgb(104, 68, 252),
                                Rgb(216, 40, 0),    Rgb(60, 188, 252),  Rgb(124, 124, 124), Rgb(250, 250, 240)};
    const int np = sizeof(palette) / sizeof(palette[0]);
    auto pick = [&] { return palette[rng() % np]; };
    auto at = [&](int x, int y) -> uint32_t& { return img[y * w + x]; };
    const uint32_t bg = pick();
    for (auto& p : img)
        p = bg;
    for (int n = 0; n < 24; ++n) {
        const int cx = rng() % w, cy = rng() % h, r = 1 + rng() % 12;
        const uint32_t col = pick();
        const bool round = rng() & 1;
        for (int y = std::max(0, cy - r); y < std::min(h, cy + r); ++y)
            for (int x = std::max(0, cx - r); x < std::min(w, cx + r); ++x)
                if (!round || (x - cx) * (x - cx) + (y - cy) * (y - cy) < r * r)
                    at(x, y) = col;
    }
    for (int n = 0; n < 16; ++n) {
        const int x0 = rng() % w, y0 = rng() % h, x1 = rng() % w, y1 = rng() % h;
        const uint32_t col = pick();
        const int steps = std::max(std::abs(x1 - x0), std::abs(y1 - y0)) + 1;
        for (int s = 0; s < steps; ++s)
            at(x0 + (x1 - x0) * s / steps, y0 + (y1 - y0) * s / steps) = col;
    }
    for (int n = 0; n < w * h / 40; ++n)
        at(rng() % w, rng() % h) = pick();
    return img;
}

// a 32-bit picture in the given 16-bit format
uint16_t To16(uint32_t p, Format f) {
    const uint32_t r = (p >> 16) & 0xff, g = (p >> 8) & 0xff, b = p & 0xff;
    if (f == kRgb565)
        return static_cast<uint16_t>(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
    return static_cast<uint16_t>(((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3));
}

// the globals of a format, as the frontends set them
void SetFormat(Format f) {
    if (f == kRgb32) {
        systemColorDepth = 32;
        systemRedShift = 19;
        systemGreenShift = 11;
        systemBlueShift = 3;
        Init_2xSaI(32);
    } else {
        systemColorDepth = 16;
        systemRedShift = f == kRgb565 ? 11 : 10;
        systemGreenShift = 5;
        systemBlueShift = 0;
        Init_2xSaI(f == kRgb565 ? 565 : 555);
    }
}

// a frame buffer with a pitch wider than the picture and guard bytes around it
struct Frame {
    int width, height, bpp, pitch;  // pitch in bytes
    static constexpr int kGuard = 256;
    std::vector<uint8_t> buf;

    Frame(int w, int h, int bytesPerPixel, int extraPixels)
        : width(w), height(h), bpp(bytesPerPixel), pitch((w + extraPixels) * bytesPerPixel),
          buf(kGuard + static_cast<size_t>(pitch) * h + kGuard) {}

    uint8_t* data() { return buf.data() + kGuard; }
    void Fill(uint8_t v) { std::fill(buf.begin(), buf.end(), v); }

    bool PictureEquals(const Frame& o, int* firstDiff = nullptr) const {
        for (int y = 0; y < height; y++)
            if (std::memcmp(buf.data() + kGuard + y * pitch, o.buf.data() + kGuard + y * o.pitch, width * bpp) != 0) {
                if (firstDiff) {
                    *firstDiff = y;
                }
                return false;
            }
        return true;
    }

    // everything outside the picture still holds v
    bool OutsidePictureIs(uint8_t v) const {
        for (size_t i = 0; i < buf.size(); i++) {
            const long rel = static_cast<long>(i) - kGuard;
            bool inside = false;
            if (rel >= 0 && rel < static_cast<long>(pitch) * height) {
                const long y = rel / pitch, x = rel % pitch;
                inside = y < height && x < width * bpp;
            }
            if (!inside && buf[i] != v)
                return false;
        }
        return true;
    }
};

Frame SourceFrame(const std::vector<uint32_t>& img, int w, int h, Format f) {
    Frame src(w, h, f == kRgb32 ? 4 : 2, 2);
    src.Fill(0);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            if (f == kRgb32)
                reinterpret_cast<uint32_t*>(src.data() + y * src.pitch)[x] = img[y * w + x];
            else
                reinterpret_cast<uint16_t*>(src.data() + y * src.pitch)[x] = To16(img[y * w + x], f);
        }
    return src;
}

// run a filter on a picture with SIMD and with the scalar kernels; both outputs
// (and the delta buffers) must match and nothing outside the picture may change
void CheckFilter(const FilterCase& fc, const std::vector<uint32_t>& img, int w, int h, Format f, const char* what) {
    Frame src = SourceFrame(img, w, h, f);
    const int bpp = src.bpp;
    Frame out[2] = {Frame(w * fc.scale, h * fc.scale, bpp, 3), Frame(w * fc.scale, h * fc.scale, bpp, 3)};
    Frame delta[2] = {Frame(w, h, bpp, 2), Frame(w, h, bpp, 2)};
    for (int i = 0; i < 2; i++) {
        out[i].Fill(0xA5);
        delta[i].Fill(0x5A);
        vbam_simd::ForceScalar(i == 1);
        fc.fn(src.data(), src.pitch, delta[i].data(), out[i].data(), out[i].pitch, w, h);
    }
    vbam_simd::ForceScalar(false);
    int row = -1;
    EXPECT_TRUE(out[0].PictureEquals(out[1], &row)) << fc.name << " " << what << " " << w << "x" << h
                                                    << ": SIMD and scalar differ, first row " << row;
    EXPECT_TRUE(delta[0].PictureEquals(delta[1])) << fc.name << " " << what << ": delta buffers differ";
    EXPECT_TRUE(out[0].OutsidePictureIs(0xA5)) << fc.name << " " << what << ": SIMD wrote outside the picture";
    EXPECT_TRUE(out[1].OutsidePictureIs(0xA5)) << fc.name << " " << what << ": scalar wrote outside the picture";
}

const int kSizes[][2] = {{240, 160}, {37, 23}, {17, 5}, {9, 3}, {160, 144}};

}  // namespace

TEST(Filters, Backend) {
    printf("filter backend under test: %s\n", vbam_simd::BackendName());
#if defined(__x86_64__) || defined(_M_X64)
    EXPECT_TRUE(VBAM_SIMD_SSE2) << "x86-64 build should use SSE2";
#elif defined(__aarch64__) || defined(_M_ARM64)
    EXPECT_TRUE(VBAM_SIMD_NEON) << "AArch64 build should use NEON";
#endif
    EXPECT_TRUE(vbam_simd::Enabled());
}

TEST(Filters, SimdMatchesScalar32) {
    SetFormat(kRgb32);
    for (const FilterCase& fc : kFilters) {
        if (!fc.wide)
            continue;
        for (const auto& wh : kSizes) {
            const int w = wh[0], h = wh[1];
            CheckFilter(fc, PixelArt32(w, h, 7u + w), w, h, kRgb32, "pixelart");
            CheckFilter(fc, Noise32(w, h, 11u + w), w, h, kRgb32, "noise");
        }
    }
}

TEST(Filters, SimdMatchesScalar16) {
    for (Format f : {kRgb565, kRgb555}) {
        SetFormat(f);
        for (const FilterCase& fc : kFilters) {
            if (fc.wide)
                continue;
            for (const auto& wh : kSizes) {
                const int w = wh[0], h = wh[1];
                CheckFilter(fc, PixelArt32(w, h, 3u + w), w, h, f, f == kRgb565 ? "pixelart565" : "pixelart555");
                CheckFilter(fc, Noise32(w, h, 5u + w), w, h, f, f == kRgb565 ? "noise565" : "noise555");
            }
        }
    }
    SetFormat(kRgb32);
}

// The filters are deterministic: two SIMD runs give the same bytes (catches
// uninitialised padding in the vector paths).
TEST(Filters, Deterministic) {
    SetFormat(kRgb32);
    const int w = 37, h = 23;
    const std::vector<uint32_t> img = PixelArt32(w, h, 99);
    for (const FilterCase& fc : kFilters) {
        if (!fc.wide)
            continue;
        Frame src = SourceFrame(img, w, h, kRgb32);
        Frame a(w * fc.scale, h * fc.scale, 4, 1), b(w * fc.scale, h * fc.scale, 4, 1);
        Frame d(w, h, 4, 2);
        a.Fill(1);
        b.Fill(2);
        fc.fn(src.data(), src.pitch, d.data(), a.data(), a.pitch, w, h);
        fc.fn(src.data(), src.pitch, d.data(), b.data(), b.pitch, w, h);
        EXPECT_TRUE(a.PictureEquals(b)) << fc.name;
    }
}

// ---------------------------------------------------------------------------
// Interframe blending
// ---------------------------------------------------------------------------

namespace {

// a frame sequence with still areas, flicker (A B A B) and a scrolling band
void FillIfbFrame(std::vector<uint8_t>& buf, int pitch, int w, int h, int depth, int frame, uint32_t seed) {
    std::mt19937 rng(seed);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            uint32_t c;
            if (y < h / 3)
                c = Rgb(x * 3 & 0xff, y * 5 & 0xff, 128);  // still
            else if (y < 2 * h / 3)
                c = ((x / 4 + frame) & 1) ? Rgb(255, 255, 255) : Rgb(0, 0, 0);  // flicker
            else
                c = Rgb((x + frame * 2) * 7 & 0xff, 64, (x + frame) * 3 & 0xff);  // scroll
            if (((x ^ y) & 7) == 0)
                c ^= rng() & 0x0f0f0f;  // a little noise
            uint8_t* row = buf.data() + y * pitch;
            switch (depth) {
                case 32:
                    reinterpret_cast<uint32_t*>(row)[x] = c;
                    break;
                case 16:
                    reinterpret_cast<uint16_t*>(row)[x] = To16(c, kRgb555);
                    break;
                case 24:
                    row[3 * x] = (c >> 16) & 0xff;
                    row[3 * x + 1] = (c >> 8) & 0xff;
                    row[3 * x + 2] = c & 0xff;
                    break;
                case 8:
                    row[x] = static_cast<uint8_t>(((c >> 16) & 0xe0) | ((c >> 11) & 0x1c) | ((c >> 6) & 0x03));
                    break;
            }
        }
}

void CheckInterframe(int depth, bool smart) {
    const int w = 97, h = 30;
    const int pitch = depth == 24 ? w * 3 : w * (depth / 8);
    auto& mgr = InterframeManager::Instance();
    mgr.Cleanup();
    mgr.Init(w, h, pitch, 2);
    for (int frame = 0; frame < 6; frame++) {
        std::vector<uint8_t> a(static_cast<size_t>(pitch) * h), b;
        FillIfbFrame(a, pitch, w, h, depth, frame, 42 + frame);
        b = a;
        vbam_simd::ForceScalar(false);
        if (smart)
            mgr.ApplySmartIBRegion(a.data(), pitch, w, 0, h, depth, 0);
        else
            mgr.ApplyMotionBlurRegion(a.data(), pitch, w, 0, h, depth, 0);
        vbam_simd::ForceScalar(true);
        if (smart)
            mgr.ApplySmartIBRegion(b.data(), pitch, w, 0, h, depth, 1);
        else
            mgr.ApplyMotionBlurRegion(b.data(), pitch, w, 0, h, depth, 1);
        vbam_simd::ForceScalar(false);
        for (int y = 0; y < h && !::testing::Test::HasFailure(); y++)
            EXPECT_EQ(0, std::memcmp(a.data() + y * pitch, b.data() + y * pitch, pitch))
                << (smart ? "SmartIB" : "MotionBlur") << depth << " frame " << frame << " row " << y;
    }
    mgr.Cleanup();
}

}  // namespace

TEST(Interframe, SmartIB32) { CheckInterframe(32, true); }
TEST(Interframe, SmartIB16) { CheckInterframe(16, true); }
TEST(Interframe, SmartIB24) { CheckInterframe(24, true); }
TEST(Interframe, SmartIB8) { CheckInterframe(8, true); }
TEST(Interframe, MotionBlur32) { CheckInterframe(32, false); }
TEST(Interframe, MotionBlur16) { CheckInterframe(16, false); }
TEST(Interframe, MotionBlur24) { CheckInterframe(24, false); }
TEST(Interframe, MotionBlur8) { CheckInterframe(8, false); }

// ---------------------------------------------------------------------------
// Benchmark (opt-in)
// ---------------------------------------------------------------------------

namespace {
bool BenchEnabled() {
    const char* e = getenv("VBAM_FILTER_BENCH");
    return e && *e && *e != '0';
}
}  // namespace

TEST(Bench, Filters) {
    if (!BenchEnabled()) {
        GTEST_SKIP() << "set VBAM_FILTER_BENCH=1";
    }
    const int w = 240, h = 160;
    const std::vector<uint32_t> img = PixelArt32(w, h, 42);
    printf("240x160 pixel art, us/frame:\n");
    for (const FilterCase& fc : kFilters) {
        const Format f = fc.wide ? kRgb32 : kRgb565;
        SetFormat(f);
        Frame src = SourceFrame(img, w, h, f);
        Frame out(w * fc.scale, h * fc.scale, src.bpp, 1);
        Frame delta(w, h, src.bpp, 2);
        out.Fill(0);
        delta.Fill(0);
        double us[2];
        for (int i = 0; i < 2; i++) {
            vbam_simd::ForceScalar(i == 1);
            fc.fn(src.data(), src.pitch, delta.data(), out.data(), out.pitch, w, h);
            const int iters = fc.scale >= 4 ? 20 : 50;
            const auto t0 = std::chrono::steady_clock::now();
            for (int k = 0; k < iters; k++)
                fc.fn(src.data(), src.pitch, delta.data(), out.data(), out.pitch, w, h);
            us[i] = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count() / iters;
        }
        vbam_simd::ForceScalar(false);
        printf("  %-16s %-6s %9.1f   scalar %9.1f   (%.2fx)\n", fc.name, vbam_simd::BackendName(), us[0], us[1],
               us[1] / us[0]);
    }
    SetFormat(kRgb32);
}
