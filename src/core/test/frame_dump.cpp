// Headless frame dumper, for chasing rendering artifacts.
//
// Runs a ROM for N frames with no GUI and writes each visible frame (240x160
// for a GBA ROM, 160x144 for a Game Boy ROM) to a raw RGBA stream, and
// optionally to one PNG per frame, so the pixels a renderer would be handed
// can be inspected, diffed against a screen capture, or bisected by frame
// number.
//
// It drives the core's emuMain() directly and reads the framebuffer in the
// layout each core writes it; Game Boy ROMs run without a boot ROM and with
// the SGB border off.
//
// Usage: frame_dump <rom> [--frames N] [--skip N] [--out FILE]
//                   [--png DIR] [--lcd [--dccp N] [--darken F] [--variant N]]
//                   [--bios FILE [--no-skip-bios]] [--save-type N]

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include <zlib.h>

#include "core/base/file_util.h"
#include "core/base/sizes.h"
#include "core/base/system.h"
#include "core/base/sound_driver.h"
#include "core/gb/gb.h"
#include "core/gb/gbGlobals.h"
#include "core/gb/gbSound.h"
#include "core/gba/gba.h"
#include "core/gba/gbaGlobals.h"
#include "core/gba/gbaEeprom.h"
#include "core/gba/gbaFlash.h"
#include "core/gba/gbaSound.h"
#include "components/filters_agb/filters_agb.h"

static uint32_t g_joy_mask = 0;
struct CoreOptions coreOptions;

uint8_t  systemColorMap8[0x10000];
uint16_t systemColorMap16[0x10000];
uint32_t systemColorMap32[0x10000];
int systemRedShift = 0;
int systemGreenShift = 0;
int systemBlueShift = 0;
int systemColorDepth = 32;
int systemVerbose = 0;
int systemFrameSkip = 0;
int systemSaveUpdateCounter = 0;
int systemSpeed = 0;
int emulating = 0;
uint16_t systemGbPalette[24];

void systemMessage(int, const char*, ...) {}
void log(const char*, ...) {}
bool systemPauseOnFrame() { return false; }
void systemGbPrint(uint8_t*, int, int, int, int, int) {}
void systemScreenCapture(int) {}
void systemDrawScreen() {}
void systemSendScreen() {}
bool systemReadJoypads() { return true; }
uint32_t systemReadJoypad(int) { return g_joy_mask; }
uint32_t systemGetClock() { return 0; }
void systemSetTitle(const char*) {}
namespace {
class NullSoundDriver : public SoundDriver {
  public:
    bool init(long) override { return true; }
    void pause() override {}
    void reset() override {}
    void resume() override {}
    void write(uint16_t*, int) override {}
    void setThrottle(unsigned short) override {}
};
}
std::unique_ptr<SoundDriver> systemSoundInit() {
    soundShutdown();
    return std::make_unique<NullSoundDriver>();
}
void systemOnWriteDataToSoundBuffer(const uint16_t*, int) {}
void systemOnSoundShutdown() {}
void systemScreenMessage(const char*) {}
void systemUpdateMotionSensor() {}
int systemGetSensorX() { return 0; }
int systemGetSensorY() { return 0; }
int systemGetSensorZ() { return 0; }
uint8_t systemGetSensorDarkness() { return 0; }
void systemCartridgeRumble(bool) {}
void systemPossibleCartridgeRumble(bool) {}
void updateRumbleFrame() {}
bool systemCanChangeSoundQuality() { return false; }
void systemShowSpeed(int) {}
void system10Frames() {}
static int g_frames_done = 0;
void systemFrame() { ++g_frames_done; }
void systemGbBorderOn() {}
void (*dbgOutput)(const char* s, uint32_t addr);
void (*dbgSignal)(int sig, int number);

// One frame of cycles per core, as the frontends count them.
static constexpr int kGbaTicksPerFrame = 280896;
static constexpr int kGbTicksPerFrame = 70000;

// Screen size of the ROM being run; set in main() from its type.
static int kW = 240, kH = 160;
static size_t kFrameBytes = 4 * 240 * 160;
static bool g_is_gb = false;

// g_pix is allocated one column and two rows larger than the screen (241x162
// for the GBA in gba.cpp, 161x146 for the Game Boy with no border): the
// renderer writes scanline N at `(uint32_t*)g_pix + kPixStride * (N + 1)`, so
// the visible image starts one whole row in and each row is followed by a
// spare pixel. Filters that read a neighbourhood rely on that padding, which
// is why this is not a plain width x height buffer.
static int kPixStride = 241;
static constexpr int kPixFirstRow = 1;

// The visible frame, top-left pixel first, 4 bytes per pixel. With the shifts
// set in main() each pixel is R, G, B, A in memory order.
static void copy_framebuffer(uint8_t* dst) {
    if (!g_pix) {
        std::memset(dst, 0, kFrameBytes);
        return;
    }
    const uint32_t* src = (const uint32_t*)g_pix + kPixStride * kPixFirstRow;
    uint8_t* out = dst;
    for (int y = 0; y < kH; ++y) {
        std::memcpy(out, src + kPixStride * y, 4 * kW);
        out += 4 * kW;
    }
}

// Run the loaded ROM for one frame. The GBA core takes a frame's worth of
// ticks per call; the Game Boy core stops early at a frame boundary, so it is
// called until the core reports one through systemFrame(), with a cap in case
// the LCD is off.
static void emulate_frame() {
    if (!g_is_gb) {
        GBASystem.emuMain(kGbaTicksPerFrame);
        return;
    }
    const int before = g_frames_done;
    for (int i = 0; i < 4 && g_frames_done == before; ++i)
        GBSystem.emuMain(kGbTicksPerFrame);
}

// ---- PNG output (1:1, so a dump can be compared pixel for pixel) -----------

static void png_chunk(FILE* f, const char* type, const uint8_t* data, uint32_t len) {
    uint8_t hdr[8] = { (uint8_t)(len >> 24), (uint8_t)(len >> 16),
                       (uint8_t)(len >> 8), (uint8_t)len,
                       (uint8_t)type[0], (uint8_t)type[1],
                       (uint8_t)type[2], (uint8_t)type[3] };
    fwrite(hdr, 1, 8, f);
    if (len)
        fwrite(data, 1, len, f);
    uLong crc = crc32(0, hdr + 4, 4);
    if (len)
        crc = crc32(crc, data, len);
    uint8_t cb[4] = { (uint8_t)(crc >> 24), (uint8_t)(crc >> 16),
                      (uint8_t)(crc >> 8), (uint8_t)crc };
    fwrite(cb, 1, 4, f);
}

static bool write_png(const char* path, const uint8_t* rgba) {
    // Raw image: per-row filter byte + RGB triples.
    std::vector<uint8_t> raw((size_t)kH * (1 + kW * 3));
    for (int y = 0; y < kH; ++y) {
        uint8_t* out = raw.data() + (size_t)y * (1 + kW * 3);
        *out++ = 0; // filter: none
        const uint8_t* row = rgba + (size_t)y * 4 * kW;
        for (int x = 0; x < kW; ++x) {
            *out++ = row[4 * x];
            *out++ = row[4 * x + 1];
            *out++ = row[4 * x + 2];
        }
    }
    uLongf zlen = compressBound((uLong)raw.size());
    std::vector<uint8_t> z(zlen);
    if (compress2(z.data(), &zlen, raw.data(), (uLong)raw.size(), 6) != Z_OK)
        return false;
    FILE* f = fopen(path, "wb");
    if (!f)
        return false;
    static const uint8_t sig[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };
    fwrite(sig, 1, 8, f);
    uint8_t ihdr[13] = {
        (uint8_t)(kW >> 24), (uint8_t)(kW >> 16), (uint8_t)(kW >> 8), (uint8_t)kW,
        (uint8_t)(kH >> 24), (uint8_t)(kH >> 16), (uint8_t)(kH >> 8), (uint8_t)kH,
        8, 2 /* truecolor */, 0, 0, 0
    };
    png_chunk(f, "IHDR", ihdr, 13);
    png_chunk(f, "IDAT", z.data(), (uint32_t)zlen);
    png_chunk(f, "IEND", nullptr, 0);
    fclose(f);
    return true;
}

static void usage() {
    fprintf(stderr,
        "usage: frame_dump <rom> [--frames N] [--skip N] [--out FILE]\n"
        "                  [--png DIR] [--lcd] [--dccp N] [--darken F]\n"
        "                  [--variant N] [--bios FILE] [--no-skip-bios]\n"
        "                  [--save-type N]\n"
        "\n"
        "  --frames N      number of frames to write (default 600)\n"
        "  --skip N        emulate and discard N frames first (default 0)\n"
        "  --out FILE      raw RGBA stream, width x height x 4 bytes per\n"
        "                  frame (240x160 for GBA, 160x144 for Game Boy)\n"
        "                  (default frames.raw; \"-\" writes no raw stream)\n"
        "  --png DIR       also write DIR/f<frame>.png, one per frame\n"
        "  --lcd           apply the AGB LCD colour filter, as the Display\n"
        "                  options do; --dccp, --darken and --variant are its\n"
        "                  colour correction profile, darkening and variant\n"
        "  --bios FILE     use a real BIOS instead of the built-in HLE one\n"
        "  --no-skip-bios  run the BIOS intro; only has an effect with --bios\n"
        "  --save-type N   0 auto, 1 EEPROM, 2 SRAM, 3 flash (default 0)\n"
        "The last four are GBA only; a Game Boy ROM runs without a boot ROM.\n");
}

int main(int argc, char** argv) {
    const char* rom_path = nullptr;
    const char* out_path = "frames.raw";
    const char* png_dir = nullptr;
    const char* bios_path = "";
    int frames = 600, skip = 0;
    int lcd = 0, dccp = 2, variant = 0;
    float darken = 0.37f;
    int skip_bios = 1, save_type = GBA_SAVE_AUTO;

    for (int i = 1; i < argc; ++i) {
        const char* a = argv[i];
        if (!std::strcmp(a, "--frames") && i + 1 < argc) frames = std::atoi(argv[++i]);
        else if (!std::strcmp(a, "--skip") && i + 1 < argc) skip = std::atoi(argv[++i]);
        else if (!std::strcmp(a, "--out") && i + 1 < argc) out_path = argv[++i];
        else if (!std::strcmp(a, "--png") && i + 1 < argc) png_dir = argv[++i];
        else if (!std::strcmp(a, "--lcd")) lcd = 1;
        else if (!std::strcmp(a, "--bios") && i + 1 < argc) bios_path = argv[++i];
        else if (!std::strcmp(a, "--no-skip-bios")) skip_bios = 0;
        else if (!std::strcmp(a, "--save-type") && i + 1 < argc) save_type = std::atoi(argv[++i]);
        else if (!std::strcmp(a, "--dccp") && i + 1 < argc) dccp = std::atoi(argv[++i]);
        else if (!std::strcmp(a, "--darken") && i + 1 < argc) darken = (float)std::atof(argv[++i]);
        else if (!std::strcmp(a, "--variant") && i + 1 < argc) variant = std::atoi(argv[++i]);
        else if (!std::strcmp(a, "--help") || !std::strcmp(a, "-h")) { usage(); return 0; }
        else if (a[0] == '-') {
            fprintf(stderr, "frame_dump: unknown option %s\n", a);
            usage();
            return 2;
        }
        else if (!rom_path) rom_path = a;
        else {
            fprintf(stderr, "frame_dump: extra argument %s\n", a);
            usage();
            return 2;
        }
    }
    if (!rom_path) { usage(); return 2; }
    const IMAGE_TYPE rom_type = utilFindType(rom_path);
    if (rom_type == IMAGE_GB) {
        g_is_gb = true;
        kW = (int)kGBWidth;
        kH = (int)kGBHeight;
    } else if (rom_type != IMAGE_GBA) {
        fprintf(stderr, "frame_dump: %s is neither a GBA nor a Game Boy image\n",
                rom_path);
        return 2;
    }
    kFrameBytes = (size_t)4 * kW * kH;
    kPixStride = kW + 1;
    if (frames <= 0 || skip < 0) {
        fprintf(stderr, "frame_dump: --frames must be positive, --skip non-negative\n");
        return 2;
    }

    if (g_is_gb) {
        // What the SDL port does for a Game Boy ROM, minus the boot ROM and
        // the per-game overrides: load, pick the hardware from the header,
        // init the CPU, reset. gbBorderOn stays false, so the picture is the
        // plain 160x144 screen.
        gbBorderOn = false;
        // The default DMG greyscale palette, white to black in BGR555, for
        // all three palette slots; a colour game ignores it.
        systemGbPalette[0] = 0x7FFF;
        systemGbPalette[1] = 0x56B5;
        systemGbPalette[2] = 0x294A;
        systemGbPalette[3] = 0x0000;
        for (int i = 4; i < 24; ++i)
            systemGbPalette[i] = systemGbPalette[i & 3];
        if (!gbLoadRom(rom_path)) {
            fprintf(stderr, "frame_dump: gbLoadRom failed\n");
            return 2;
        }
        gbGetHardwareType();
        gbCPUInit(nullptr, false);
        soundInit();
        gbReset();
        emulating = 1;
        fprintf(stderr, "frame_dump: Game Boy ROM, hardware type %d\n", gbHardware);
    } else {
        FILE* rf = fopen(rom_path, "rb");
        if (!rf) { fprintf(stderr, "frame_dump: cannot open %s\n", rom_path); return 2; }
        fseek(rf, 0, SEEK_END);
        const long rom_bytes = ftell(rf);
        fseek(rf, 0, SEEK_SET);
        std::vector<uint8_t> rom_buf((size_t)rom_bytes);
        if (fread(rom_buf.data(), 1, (size_t)rom_bytes, rf) != (size_t)rom_bytes) {
            fprintf(stderr, "frame_dump: short read on %s\n", rom_path);
            fclose(rf);
            return 2;
        }
        fclose(rf);

        coreOptions.skipBios = skip_bios != 0;
        if (!CPULoadRomData((const char*)rom_buf.data(), (int)rom_bytes)) {
            fprintf(stderr, "frame_dump: CPULoadRomData failed\n");
            return 2;
        }

        // CPULoadRomData() does not resolve the save type, the frontends do, and a
        // game whose mapper is left unmapped hangs on a white screen. Follow what
        // the wxWidgets panel does after loading a ROM: for auto, scan the image
        // for the mapper's ID string, and give EEPROM a size up front so an
        // uninitialised battery cannot freeze a DMA. CPUReset() then applies
        // coreOptions.saveType, and CPUInit() decides coreOptions.useBios from
        // whether the BIOS file loaded.
        if (save_type == GBA_SAVE_AUTO) {
            flashDetectSaveType((int)rom_bytes);
            fprintf(stderr, "frame_dump: detected save type %d\n", coreOptions.saveType);
        } else {
            coreOptions.saveType = save_type;
            flashSetSize(0x10000);
        }
        if (coreOptions.saveType == GBA_SAVE_EEPROM)
            eepromSetSize(SIZE_EEPROM_512);

        CPUInit(bios_path, bios_path[0] != '\0');
        if (bios_path[0] && !coreOptions.useBios)
            fprintf(stderr, "frame_dump: BIOS %s not loaded, using HLE\n", bios_path);
        soundInit();
        CPUReset();
        emulating = 1;
    }

    // The same 32-bit pixel layout the wxWidgets panel asks the core for, so a
    // dump holds the bytes a renderer would be handed.
    systemColorDepth = 32;
    systemRedShift = 3; systemGreenShift = 11; systemBlueShift = 19;
    for (int i = 0; i < 0x10000; ++i) {
        const uint32_t r5 = (uint32_t)(i & 0x1F);
        const uint32_t g5 = (uint32_t)((i >> 5) & 0x1F);
        const uint32_t b5 = (uint32_t)((i >> 10) & 0x1F);
        systemColorMap32[i] = (r5 << systemRedShift) | (g5 << systemGreenShift) |
                              (b5 << systemBlueShift) | 0xFF000000u;
    }

    if (lcd) {
        gbafilter_set_params(dccp, darken, variant);
        gbafilter_update_colors(true);
        fprintf(stderr, "frame_dump: LCD filter on (dccp=%d darken=%.2f variant=%d)\n",
                dccp, darken, variant);
    }

    for (int i = 0; i < skip; ++i)
        emulate_frame();

    const bool want_raw = std::strcmp(out_path, "-") != 0;
    FILE* of = nullptr;
    if (want_raw) {
        of = fopen(out_path, "wb");
        if (!of) { fprintf(stderr, "frame_dump: cannot write %s\n", out_path); return 2; }
    }

    std::vector<uint8_t> frame(kFrameBytes);
    for (int i = 0; i < frames; ++i) {
        emulate_frame();
        copy_framebuffer(frame.data());
        if (of)
            fwrite(frame.data(), 1, kFrameBytes, of);
        if (png_dir) {
            char path[1024];
            snprintf(path, sizeof(path), "%s/f%06d.png", png_dir, skip + i);
            if (!write_png(path, frame.data())) {
                fprintf(stderr, "frame_dump: cannot write %s\n", path);
                if (of)
                    fclose(of);
                return 2;
            }
        }
    }
    if (of)
        fclose(of);

    fprintf(stderr, "frame_dump: wrote %d frames (skipped %d)", frames, skip);
    if (want_raw)
        fprintf(stderr, " to %s", out_path);
    if (png_dir)
        fprintf(stderr, "%s PNGs in %s", want_raw ? " and as" : " to", png_dir);
    fprintf(stderr, "\n");
    return 0;
}
