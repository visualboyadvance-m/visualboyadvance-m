// Times dlssnr::Filter::Apply32() over real game frames.
//
// The per-frame composition is what sets the frame rate with this filter -- the
// network runs on its own threads -- and it scales with the *output* extent, so
// it has to be measured at the size a display filter hands it. A synthetic
// scrolling pattern does not do: it misses the data-dependent paths (the scroll
// hypotheses, the pass before, the correction memory), which is how two earlier
// A/Bs came back flat and wrong. So this replays a raw RGBA stream dumped from a
// running game by frame_dump, scales each frame with the real xBRZ kernel, and
// times Apply32() on the result.
//
// Frames are paced at --fps, because the filter is a pipeline: run flat out, a
// whole 400-frame stream replays in half a second and no pass has finished, so
// every frame is a copy and the thing under measurement never runs. At 60 a
// pass lands after a second or two and the rest of the stream composes.
//
// Frames before the first pass lands are copies -- nothing to compose -- so they
// are reported apart from the ones that do the work. The split is measured, not
// assumed: the first frames are copies by construction, and anything far above
// what they cost is composing.

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <numeric>
#include <thread>
#include <string>
#include <vector>

#include "../../filters/filters.h"
#include "../dlssnr.h"

// The display filters read VBA-M's globals; the wx panel sets them to this for
// the 32-bit path (R, G, B, A in memory order), which is what the frames a
// frame_dump stream holds are laid out as.
int systemRedShift = 3;
int systemGreenShift = 11;
int systemBlueShift = 19;
int systemColorDepth = 32;
int RGB_LOW_BITS_MASK = 0x00010101;

namespace {

constexpr int kW = 240;
constexpr int kH = 160;

// VBA-M's 32-bit layout: systemRedShift - 3 and friends.
constexpr int kRedShift = 0;
constexpr int kGreenShift = 8;
constexpr int kBlueShift = 16;

double Median(std::vector<double> v) {
    if (v.empty())
        return 0.0;
    std::sort(v.begin(), v.end());
    return v[v.size() / 2];
}

void Usage() {
    fprintf(stderr,
            "usage: apply_bench --raw FILE [--scale 6|9] [--frames N] [--warmup N]\n"
            "                   [--max-height N] [--no-history] [--csv FILE]\n"
            "  --raw     a frame_dump stream of 240x160 RGBA frames\n"
            "  --scale   the xBRZ factor the pass runs behind (default 9)\n");
}

}  // namespace

int main(int argc, char** argv) {
    std::string raw_path, csv_path;
    int scale = 9, want_frames = 0, max_height = 480, loops = 1;
    double fps = 60.0;
    bool history = true;

    for (int i = 1; i < argc; i++) {
        const std::string a = argv[i];
        const auto next = [&]() { return i + 1 < argc ? argv[++i] : nullptr; };
        if (a == "--raw") { const char* v = next(); if (v) raw_path = v; }
        else if (a == "--scale") { const char* v = next(); if (v) scale = atoi(v); }
        else if (a == "--frames") { const char* v = next(); if (v) want_frames = atoi(v); }
        else if (a == "--fps") { const char* v = next(); if (v) fps = atof(v); }
        else if (a == "--loops") { const char* v = next(); if (v) loops = atoi(v); }
        else if (a == "--max-height") { const char* v = next(); if (v) max_height = atoi(v); }
        else if (a == "--csv") { const char* v = next(); if (v) csv_path = v; }
        else if (a == "--no-history") history = false;
        else { Usage(); return 2; }
    }
    if (raw_path.empty() || (scale != 6 && scale != 9)) {
        Usage();
        return 2;
    }

    FILE* f = fopen(raw_path.c_str(), "rb");
    if (!f) {
        fprintf(stderr, "apply_bench: cannot open %s\n", raw_path.c_str());
        return 1;
    }
    const size_t frame_bytes = static_cast<size_t>(kW) * kH * 4;
    std::vector<uint8_t> data;
    {
        std::vector<uint8_t> chunk(frame_bytes);
        size_t got;
        while ((got = fread(chunk.data(), 1, frame_bytes, f)) == frame_bytes)
            data.insert(data.end(), chunk.begin(), chunk.end());
        fclose(f);
    }
    const int have = static_cast<int>(data.size() / frame_bytes);
    if (have == 0) {
        fprintf(stderr, "apply_bench: %s holds no whole 240x160 frames\n", raw_path.c_str());
        return 1;
    }
    const int stream = want_frames > 0 ? std::min(want_frames, have) : have;
    const int frames = stream * std::max(1, loops);

    // The display filters read a neighbourhood, so the source carries a spare
    // column and a row either side, laid out as g_pix is.
    const int src_stride = (kW + 1) * 4;
    std::vector<uint8_t> src(static_cast<size_t>(src_stride) * (kH + 2), 0);

    const int ow = kW * scale, oh = kH * scale;
    const int out_stride = (ow + 1) * 4;
    std::vector<uint8_t> out(static_cast<size_t>(out_stride) * (oh + 2), 0);

    dlssnr::Settings settings;
    settings.max_height = max_height;
    settings.history = history;
    settings.target_fps = 0.0;  // the governor off: measure the work, not the spacing

    dlssnr::Filter filter;

    std::vector<double> all;
    all.reserve(frames);

    const auto started = std::chrono::steady_clock::now();
    const double frame_ms = fps > 0.0 ? 1000.0 / fps : 0.0;

    for (int n = 0; n < frames; n++) {
        if (frame_ms > 0.0) {
            const auto due = started + std::chrono::duration<double, std::milli>(frame_ms * n);
            std::this_thread::sleep_until(due);
        }
        const uint8_t* in = data.data() + static_cast<size_t>(n % stream) * frame_bytes;
        for (int y = 0; y < kH; y++)
            memcpy(src.data() + static_cast<size_t>(y + 1) * src_stride,
                   in + static_cast<size_t>(y) * kW * 4, static_cast<size_t>(kW) * 4);

        uint8_t* const sfirst = src.data() + src_stride;
        uint8_t* const ofirst = out.data() + out_stride;
        if (scale == 6)
            xbrz6x32(sfirst, src_stride, nullptr, ofirst, out_stride, kW, kH);
        else
            xbrz9x32(sfirst, src_stride, nullptr, ofirst, out_stride, kW, kH);

        // In place over the filter's output, as the post-filter stage does.
        const auto t0 = std::chrono::steady_clock::now();
        filter.Apply32(ofirst, out_stride, ofirst, out_stride, ow, oh, kRedShift, kGreenShift,
                       kBlueShift, settings);
        const auto t1 = std::chrono::steady_clock::now();
        const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        all.push_back(ms);
    }

    // What a copy costs here: the opening frames, before any pass can have
    // finished. Anything well above that composed.
    const int kBaseFrames = 16;
    std::vector<double> composed, passthrough;
    if (static_cast<int>(all.size()) > kBaseFrames) {
        const double base =
            Median(std::vector<double>(all.begin(), all.begin() + kBaseFrames));
        const double cut = std::max(base * 2.5, base + 1.0);
        for (size_t i = kBaseFrames; i < all.size(); i++)
            (all[i] > cut ? composed : passthrough).push_back(all[i]);
        printf("apply_bench: a copy of this frame costs %.2f ms; composing counted above %.2f\n",
               base, cut);
        // That split needs the opening frames to be copies, and on a fast GPU a
        // pass lands within one, so it cannot be trusted there. The median over
        // everything after them is the honest total on any machine: by then
        // almost every frame composes.
        printf("  all after %2d        %6zu frames   median %8.2f ms\n", kBaseFrames,
               all.size() - kBaseFrames,
               Median(std::vector<double>(all.begin() + kBaseFrames, all.end())));
    }

    if (!csv_path.empty()) {
        FILE* c = fopen(csv_path.c_str(), "wb");
        if (c) {
            fprintf(c, "frame,apply_ms\n");
            for (size_t i = 0; i < all.size(); i++)
                fprintf(c, "%zu,%.4f\n", i, all[i]);
            fclose(c);
        }
    }

    printf("apply_bench: xBRZ %dx -> %dx%d, %d frames, scaled height %d, history %s\n", scale, ow,
           oh, frames, max_height, history ? "on" : "off");
    printf("  composing   %6zu frames   median %8.2f ms   mean %8.2f ms   min %8.2f   max %8.2f\n",
           composed.size(), Median(composed),
           composed.empty() ? 0.0
                            : std::accumulate(composed.begin(), composed.end(), 0.0) /
                                  static_cast<double>(composed.size()),
           composed.empty() ? 0.0 : *std::min_element(composed.begin(), composed.end()),
           composed.empty() ? 0.0 : *std::max_element(composed.begin(), composed.end()));
    printf("  passing     %6zu frames   median %8.2f ms\n", passthrough.size(),
           Median(passthrough));
    return 0;
}
