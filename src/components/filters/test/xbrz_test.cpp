// xBRZ scaler tests: the SIMD build (SSE2 on x86, NEON on AArch64) against
// the scalar backend, slices against whole frames, and bounds.
//
// Set VBAM_FILTER_BENCH=1 to also run the Bench cases, which print the time
// per 240x160 frame for every scale factor.

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "../internal/xBRZ/xbrz.h"
#include "../internal/xBRZ/xbrz_simd.h"

// xbrz_ref_shim.cpp
void xbrzScalarRefScale(size_t factor, const uint32_t* src, uint32_t* trg, int srcWidth, int srcHeight,
                        int colFmt, int srcPitch, int trgPitch, int yFirst, int yLast);
bool xbrzScalarRefEqualColorTest2(uint32_t col1, uint32_t col2, int colFmt, double equalColorTolerance);

namespace {

constexpr int kFactors[] = {2, 3, 4, 5, 6, 9};
constexpr xbrz::ColorFormat kFormats[] = {xbrz::ColorFormat::rgb, xbrz::ColorFormat::argb};

const char* BackendName() {
#if defined(XBRZ_SIMD_SSE2)
    return "SSE2";
#elif defined(XBRZ_SIMD_NEON)
    return "NEON";
#else
    return "scalar";
#endif
}

// A source image with a pitch: `pitch` pixels per row, of which `width` are
// the picture. The padding column holds a sentinel that must never be read as
// picture (the duplicate reader clamps to width - 1, not to the pitch).
struct Image {
    int width = 0;
    int height = 0;
    int pitch = 0;  // in pixels
    std::vector<uint32_t> pix;

    Image(int w, int h, int extraCols = 1) : width(w), height(h), pitch(w + extraCols), pix(pitch * h, 0xDEADBEEF) {}

    uint32_t& at(int x, int y) { return pix[y * pitch + x]; }
    int pitchBytes() const { return pitch * static_cast<int>(sizeof(uint32_t)); }
};

uint32_t Rgb(uint32_t r, uint32_t g, uint32_t b, uint32_t a = 0) { return (a << 24) | (r << 16) | (g << 8) | b; }

// Uniform noise: the preprocessing never takes its flat-area early out.
Image NoiseImage(int w, int h, bool alpha, uint32_t seed) {
    Image img(w, h);
    std::mt19937 rng(seed);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            uint32_t p = rng();
            if (!alpha)
                p &= 0x00FFFFFF;
            img.at(x, y) = p;
        }
    return img;
}

// Pixel art: a small palette, filled shapes, 1-pixel lines, text-like
// speckle. This is what the scaler is for and where every blend rule fires.
Image PixelArtImage(int w, int h, bool alpha, uint32_t seed) {
    Image img(w, h);
    std::mt19937 rng(seed);
    const uint32_t palette[] = {
        Rgb(0, 0, 0, 255),       Rgb(255, 255, 255, 255), Rgb(248, 56, 0, 255),   Rgb(252, 152, 56, 255),
        Rgb(0, 168, 0, 255),     Rgb(0, 88, 248, 255),    Rgb(255, 216, 0, 255),  Rgb(104, 68, 252, 255),
        Rgb(216, 40, 0, alpha ? 128 : 255), Rgb(60, 188, 252, alpha ? 0 : 255), Rgb(124, 124, 124, 255),
    };
    const int np = sizeof(palette) / sizeof(palette[0]);
    auto pick = [&] { return palette[rng() % np]; };

    const uint32_t bg = pick();
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            img.at(x, y) = bg;

    // rectangles and circles
    for (int n = 0; n < 24; ++n) {
        const int cx = rng() % w, cy = rng() % h, r = 1 + rng() % 12;
        const uint32_t col = pick();
        const bool round = rng() & 1;
        for (int y = std::max(0, cy - r); y < std::min(h, cy + r); ++y)
            for (int x = std::max(0, cx - r); x < std::min(w, cx + r); ++x)
                if (!round || (x - cx) * (x - cx) + (y - cy) * (y - cy) < r * r)
                    img.at(x, y) = col;
    }
    // thin lines at assorted slopes
    for (int n = 0; n < 16; ++n) {
        const int x0 = rng() % w, y0 = rng() % h, x1 = rng() % w, y1 = rng() % h;
        const uint32_t col = pick();
        const int steps = std::max(std::abs(x1 - x0), std::abs(y1 - y0)) + 1;
        for (int s = 0; s < steps; ++s)
            img.at(x0 + (x1 - x0) * s / steps, y0 + (y1 - y0) * s / steps) = col;
    }
    // speckle: isolated pixels (the "mario eyes" rules)
    for (int n = 0; n < w * h / 40; ++n)
        img.at(rng() % w, rng() % h) = pick();

    if (!alpha)
        for (uint32_t& p : img.pix)
            if (p != 0xDEADBEEF)
                p &= 0x00FFFFFF;
    return img;
}

// Smooth gradient with a few hard edges: distances near the tolerance.
Image GradientImage(int w, int h, bool alpha) {
    Image img(w, h);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            uint32_t r = (x * 255) / std::max(1, w - 1), g = (y * 255) / std::max(1, h - 1), b = ((x + y) * 7) & 0xFF;
            if ((x / 17 + y / 13) & 1)
                r = 255 - r;
            img.at(x, y) = Rgb(r, g, b, alpha ? (255 - (x * 3) % 256) : 0);
        }
    return img;
}

struct Target {
    int width, height, pitch;  // pixels
    static constexpr int kGuard = 64;  // pixels before and after the frame
    std::vector<uint32_t> buf;

    Target(int w, int h, int extraCols) : width(w), height(h), pitch(w + extraCols), buf(kGuard + pitch * h + kGuard) {}

    void Fill(uint32_t v) { std::fill(buf.begin(), buf.end(), v); }
    uint32_t* frame() { return buf.data() + kGuard; }
    int pitchBytes() const { return pitch * static_cast<int>(sizeof(uint32_t)); }
};

void ScaleSimd(int factor, const Image& src, Target& dst, xbrz::ColorFormat fmt, int yFirst = 0, int yLast = 1 << 30) {
    xbrz::scale(factor, src.pix.data(), dst.frame(), src.width, src.height, fmt, src.pitchBytes(), dst.pitchBytes(),
                xbrz::ScalerCfg(), yFirst, yLast);
}

void ScaleRef(int factor, const Image& src, Target& dst, xbrz::ColorFormat fmt, int yFirst = 0, int yLast = 1 << 30) {
    xbrzScalarRefScale(factor, src.pix.data(), dst.frame(), src.width, src.height, static_cast<int>(fmt),
                       src.pitchBytes(), dst.pitchBytes(), yFirst, yLast);
}

// Compare the picture area of two targets; report the first mismatch.
::testing::AssertionResult SameFrame(Target& a, Target& b, int scale, int width, int height) {
    for (int y = 0; y < height * scale; ++y)
        for (int x = 0; x < width * scale; ++x) {
            const uint32_t pa = a.frame()[y * a.pitch + x];
            const uint32_t pb = b.frame()[y * b.pitch + x];
            if (pa != pb) {
                char msg[160];
                snprintf(msg, sizeof msg, "pixel (%d, %d) [source (%d, %d)]: %08x vs %08x", x, y, x / scale, y / scale, pa, pb);
                return ::testing::AssertionFailure() << msg;
            }
        }
    return ::testing::AssertionSuccess();
}

// Everything outside the picture area still holds the sentinel.
::testing::AssertionResult OnlyFrameWritten(Target& t, int scale, int width, int height, uint32_t sentinel) {
    for (size_t i = 0; i < t.buf.size(); ++i) {
        const long rel = static_cast<long>(i) - Target::kGuard;
        bool inFrame = false;
        if (rel >= 0) {
            const long y = rel / t.pitch, x = rel % t.pitch;
            inFrame = y < height * scale && x < width * scale;
        }
        if (!inFrame && t.buf[i] != sentinel)
            return ::testing::AssertionFailure() << "write outside the frame at buffer index " << i;
    }
    return ::testing::AssertionSuccess();
}

struct Case {
    const char* name;
    Image (*make)(int, int, bool, uint32_t);
};

Image MakeGradient(int w, int h, bool alpha, uint32_t) { return GradientImage(w, h, alpha); }
Image MakeFlat(int w, int h, bool alpha, uint32_t) {
    Image img(w, h);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            img.at(x, y) = Rgb(80, 160, 240, alpha ? 200 : 0);
    return img;
}

const Case kCases[] = {
    {"noise", NoiseImage},
    {"pixelart", PixelArtImage},
    {"gradient", MakeGradient},
    {"flat", MakeFlat},
};

const char* FormatName(xbrz::ColorFormat f) { return f == xbrz::ColorFormat::rgb ? "rgb" : "argb"; }

}  // namespace

TEST(Xbrz, Backend) {
    printf("xBRZ backend under test: %s\n", BackendName());
#if defined(__x86_64__) || defined(_M_X64)
    EXPECT_TRUE(XBRZ_SIMD_SSE2) << "x86-64 build should use SSE2";
#elif defined(__aarch64__) || defined(_M_ARM64)
    EXPECT_TRUE(XBRZ_SIMD_NEON) << "AArch64 build should use NEON";
#endif
}

// The SIMD build and the scalar build of the same source produce identical
// pixels, for every scale factor, colour format and image kind, with widths
// that are not a multiple of the lane count and with a padded pitch.
TEST(Xbrz, SimdMatchesScalar) {
    const int sizes[][2] = {{240, 160}, {37, 23}, {1, 1}, {2, 3}, {5, 1}, {1, 7}, {160, 144}};
    for (const Case& c : kCases)
        for (xbrz::ColorFormat fmt : kFormats)
            for (const auto& wh : sizes)
                for (int factor : kFactors) {
                    const int w = wh[0], h = wh[1];
                    const Image src = c.make(w, h, fmt == xbrz::ColorFormat::argb, 1234u + w * 7 + h);
                    Target a(w * factor, h * factor, 3), b(w * factor, h * factor, 3);
                    a.Fill(0xCAFEF00D);
                    b.Fill(0xCAFEF00D);
                    ScaleSimd(factor, src, a, fmt);
                    ScaleRef(factor, src, b, fmt);
                    EXPECT_TRUE(SameFrame(a, b, factor, w, h))
                        << c.name << " " << FormatName(fmt) << " " << w << "x" << h << " x" << factor;
                    EXPECT_TRUE(OnlyFrameWritten(a, factor, w, h, 0xCAFEF00D))
                        << c.name << " " << FormatName(fmt) << " " << w << "x" << h << " x" << factor;
                }
}

// Processing the frame in row slices (what the wx and Qt filter threads do
// through the seam re-pass, and what the yFirst/yLast API offers) yields the
// same picture as one call.
TEST(Xbrz, SlicesMatchWholeFrame) {
    const int w = 97, h = 61;
    for (xbrz::ColorFormat fmt : kFormats)
        for (int factor : kFactors) {
            const Image src = PixelArtImage(w, h, fmt == xbrz::ColorFormat::argb, 99);
            Target whole(w * factor, h * factor, 2), sliced(w * factor, h * factor, 2);
            whole.Fill(0);
            sliced.Fill(0);
            ScaleSimd(factor, src, whole, fmt);
            const int cuts[] = {0, 1, 2, 17, 18, 40, 60, h};
            for (size_t i = 0; i + 1 < sizeof(cuts) / sizeof(cuts[0]); ++i)
                ScaleSimd(factor, src, sliced, fmt, cuts[i], cuts[i + 1]);
            EXPECT_TRUE(SameFrame(whole, sliced, factor, w, h)) << FormatName(fmt) << " x" << factor;
        }
}

// Slice ranges outside the image are clamped, an empty range is a no-op.
TEST(Xbrz, SliceRangesClamp) {
    const int w = 16, h = 8, factor = 3;
    const Image src = PixelArtImage(w, h, false, 5);
    Target full(w * factor, h * factor, 0), t(w * factor, h * factor, 0);
    full.Fill(0x11111111);
    t.Fill(0x11111111);
    ScaleSimd(factor, src, full, xbrz::ColorFormat::rgb);
    ScaleSimd(factor, src, t, xbrz::ColorFormat::rgb, -5, 100);
    EXPECT_TRUE(SameFrame(full, t, factor, w, h));

    Target untouched(w * factor, h * factor, 0);
    untouched.Fill(0x11111111);
    ScaleSimd(factor, src, untouched, xbrz::ColorFormat::rgb, 4, 4);
    ScaleSimd(factor, src, untouched, xbrz::ColorFormat::rgb, 6, 2);
    ScaleSimd(factor, src, untouched, xbrz::ColorFormat::rgb, h, h + 3);
    for (uint32_t p : untouched.buf)
        ASSERT_EQ(p, 0x11111111u);
}

// Factor 1 copies the picture through the pitches.
TEST(Xbrz, Factor1Copies) {
    const int w = 23, h = 9;
    const Image src = NoiseImage(w, h, true, 77);
    Target t(w, h, 5);
    t.Fill(0x55555555);
    ScaleSimd(1, src, t, xbrz::ColorFormat::argb);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            ASSERT_EQ(t.frame()[y * t.pitch + x], src.pix[y * src.pitch + x]);
    EXPECT_TRUE(OnlyFrameWritten(t, 1, w, h, 0x55555555));
}

// The upper byte of an rgb-format source is passed through on copied blocks
// and comes out zero on blended pixels, as upstream's makePixel(r, g, b) did.
TEST(Xbrz, RgbUpperByte) {
    const int w = 8, h = 8, factor = 2;
    Image src(w, h);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            src.at(x, y) = (x < 4) ? Rgb(255, 255, 255, 0xAB) : Rgb(0, 0, 0, 0xAB);
    src.at(3, 3) = Rgb(0, 0, 0, 0xAB);  // a corner to blend around
    Target t(w * factor, h * factor, 0);
    t.Fill(0);
    ScaleSimd(factor, src, t, xbrz::ColorFormat::rgb);
    int blended = 0, copied = 0;
    for (int i = 0; i < w * factor * h * factor; ++i) {
        const uint32_t p = t.frame()[i];
        const uint32_t rgb = p & 0xFFFFFF;
        if (rgb == 0xFFFFFF || rgb == 0) {
            EXPECT_EQ(p >> 24, 0xABu);
            ++copied;
        } else {
            EXPECT_EQ(p >> 24, 0u);
            ++blended;
        }
    }
    EXPECT_GT(blended, 0);
    EXPECT_GT(copied, 0);
}

// Known blend geometry: a black diagonal step on white at 2x must put the
// 1/2 and 1/4 ramps where xBRZ 1.9 puts them. Catches a wrong rotation
// table or corner packing that would still agree between backends.
TEST(Xbrz, DiagonalStep2x) {
    const int w = 6, h = 6, factor = 2;
    Image src(w, h);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            src.at(x, y) = (x + y < 6) ? Rgb(255, 255, 255) : Rgb(0, 0, 0);
    Target t(w * factor, h * factor, 0);
    t.Fill(0);
    ScaleSimd(factor, src, t, xbrz::ColorFormat::rgb);
    auto at = [&](int x, int y) { return t.frame()[y * t.pitch + x] & 0xFF; };
    // along the step the outer corners of the white blocks pick up black at
    // 1/2 (diagonal blend) and the inner side stays pure
    EXPECT_EQ(at(2 * 2 + 1, 2 * 3 + 1), 128u);  // source (2, 3): white block, bottom-right corner blends toward black
    EXPECT_EQ(at(2 * 2, 2 * 3), 255u);
    EXPECT_EQ(at(2 * 3, 2 * 3), 128u);           // source (3, 3): black block, top-left corner blends toward white
    EXPECT_EQ(at(2 * 3 + 1, 2 * 3 + 1), 0u);
}

TEST(Xbrz, EqualColorTest2MatchesScalar) {
    std::mt19937 rng(3);
    for (int n = 0; n < 20000; ++n) {
        const uint32_t a = rng(), b = (n & 1) ? rng() : (a ^ (rng() & 0x0F0F0F0F));
        for (int fmt = 0; fmt < 3; ++fmt) {
            const double tol = (n % 50) * 2.5;
            ASSERT_EQ(xbrz::equalColorTest2(a, b, static_cast<xbrz::ColorFormat>(fmt), tol, 0),
                      xbrzScalarRefEqualColorTest2(a, b, fmt, tol))
                << std::hex << a << " " << b << " fmt " << fmt << " tol " << tol;
        }
    }
}

namespace {
bool BenchEnabled() {
    const char* e = getenv("VBAM_FILTER_BENCH");
    return e && *e && *e != '0';
}

void Bench(const char* what, Image (*make)(int, int, bool, uint32_t), bool ref) {
    const int w = 240, h = 160;
    const Image src = make(w, h, false, 42);
    for (int factor : kFactors) {
        Target t(w * factor, h * factor, 1);
        t.Fill(0);
        auto run = [&] {
            if (ref)
                ScaleRef(factor, src, t, xbrz::ColorFormat::rgb);
            else
                ScaleSimd(factor, src, t, xbrz::ColorFormat::rgb);
        };
        run();  // warm up
        const int iters = factor >= 6 ? 20 : 60;
        const auto t0 = std::chrono::steady_clock::now();
        for (int i = 0; i < iters; ++i)
            run();
        const double us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count() / iters;
        printf("  %-8s %-7s x%d  %9.1f us/frame\n", ref ? "scalar" : BackendName(), what, factor, us);
    }
}
}  // namespace

TEST(Bench, Xbrz) {
    if (!BenchEnabled()) {
        GTEST_SKIP() << "set VBAM_FILTER_BENCH=1";
    }
    printf("240x160 source, rgb:\n");
    Bench("pixelart", PixelArtImage, false);
    Bench("pixelart", PixelArtImage, true);
    Bench("noise", NoiseImage, false);
    Bench("noise", NoiseImage, true);
}
