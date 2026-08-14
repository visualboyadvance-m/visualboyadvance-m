// PumpkinOS frontend for VBA-M.
//
// The emulator core is compiled with the same defines as the libretro port
// (__LIBRETRO__ in particular selects the in-memory savestate API and the
// tightly packed g_pix stride), and this file plays the role libretro.cpp
// plays there. The PalmOS side follows the ChocoDoom port conventions:
// blit into the display window bitmap + pumpkin_screen_dirty(), poll key
// edges with pumpkin_status(), files through the VFS volume.

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern "C" {
#include <PalmOS.h>
#include <VFSMgr.h>

#include "sys.h"
#include "thread.h"
#include "pumpkin.h"
#include "debug.h"
#include "endianness.h"
}

#include "core/base/sizes.h"
#include "core/base/system.h"
#include "core/gba/gba.h"
#include "core/gba/gbaEeprom.h"
#include "core/gba/gbaFlash.h"
#include "core/gba/gbaGlobals.h"
#include "core/gba/gbaRtc.h"
#include "core/gba/gbaSound.h"
#include "core/gb/gb.h"
#include "core/gb/gbCartData.h"
#include "core/gb/gbGlobals.h"
#include "core/gb/gbSound.h"

#include "resource.h"
#include "sound_pumpkin.h"

#define VBAM_TAG "VBAM"

#define ROM_DIR "/PALM/Programs/VBAM"
#define MAX_ROMS 128
#define MAX_PATH 256

// Vertical offset that leaves the form title bar visible.
#define TITLE_H 30

// 44100 matches the rate liblsdl3 opens the SDL3 device with. Any other rate
// takes liblsdl3's resample branch, which produces non-frame-aligned buffers
// that SDL3 rejects ("Can't add partial sample frames") — i.e. silence.
#define SAMPLERATE 44100
#define FRAME_US 16743  // 1e6 / (16777216 / 280896)

// Pace against the audio ring: run ahead until this many stereo frames are
// queued (~4 frames of audio), then idle in the event pump.
#define AUDIO_HIGH_WATER 2200

#define STATE_BUF_SIZE (2 * 1024 * 1024)

// ---------------------------------------------------------------------------
// Globals required by the core (see core/base/system.h).
// ---------------------------------------------------------------------------

struct CoreOptions coreOptions;

uint8_t systemColorMap8[0x10000];
uint16_t systemColorMap16[0x10000];
uint32_t systemColorMap32[0x10000];

#define GS555(x) ((x) | ((x) << 5) | ((x) << 10))
uint16_t systemGbPalette[24] = {
    GS555(0x1f), GS555(0x15), GS555(0x0c), 0,
    GS555(0x1f), GS555(0x15), GS555(0x0c), 0,
    GS555(0x1f), GS555(0x15), GS555(0x0c), 0,
    GS555(0x1f), GS555(0x15), GS555(0x0c), 0,
    GS555(0x1f), GS555(0x15), GS555(0x0c), 0,
    GS555(0x1f), GS555(0x15), GS555(0x0c), 0,
};

int systemRedShift = 11;
int systemGreenShift = 6;  // the colormap builder shifts green by (this - 1)
int systemBlueShift = 0;
int systemColorDepth = 16;
int systemVerbose = 0;
int systemFrameSkip = 0;
int systemSaveUpdateCounter = SYSTEM_SAVE_NOT_UPDATED;
int systemSpeed = 0;
int emulating = 0;

// ---------------------------------------------------------------------------
// Frontend state
// ---------------------------------------------------------------------------

static UInt16 volref;
static UInt32 screenWidth, screenHeight;

static EmulatedSystem* core;
static IMAGE_TYPE romType = IMAGE_UNKNOWN;
static int romSize;
static char romFile[128];       // file name inside ROM_DIR
static char romBase[128];       // romFile without extension

static int numRoms;
static char* romItems[MAX_ROMS];

static int romIndex;
static Boolean ready;           // MainForm is open and may be blitted
static Boolean quitApp;         // Quit menu item or fatal condition
static Boolean appStopped;      // appStopEvent seen
static Boolean backToChooser;   // Open ROM... menu item
static Boolean doReset, doSaveState, doLoadState;

static int emuWidth, emuHeight; // core output dimensions
static int scale;               // 1 or 2

static uint32_t joyMask;        // active-high GBA KEYINPUT-layout button mask
static int frameDone;

static uint8_t* stateBuf;

// ---------------------------------------------------------------------------
// GBA per-game save type database, shared with the libretro port.
// ---------------------------------------------------------------------------

typedef struct {
    char romtitle[256];
    char romid[5];
    int saveSize;
    int saveType;   // 0 auto, 1 eeprom, 2 sram, 3 flash, 4 sensor+eeprom, 5 none
    int rtcEnabled;
    int mirroringEnabled;
    int useBios;
} ini_t;

static const ini_t gbaover[512] = {
#include "libretro/gba-over.inc"
};

static bool find_string(const uint8_t* buf, size_t size, const char* str) {
    size_t len = strlen(str);

    if (!buf || !len || size < len) return false;

    for (size_t i = 0; i + len <= size; i++) {
        if (buf[i] == (uint8_t)str[0] && !memcmp(buf + i, str, len)) return true;
    }

    return false;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

void log(const char* fmt, ...) {
    sys_va_list ap;

    sys_va_start(ap, fmt);
    debugva_full("vbam", "log", 0, DEBUG_INFO, VBAM_TAG, fmt, ap);
    sys_va_end(ap);
}

static void buildColorMap16(void) {
    // The renderers emit raw BGR555 pixels and look the final value up here.
    // The display window bitmap is big-endian RGB565, so the swap is baked
    // into the table and the blit becomes a straight copy.
    for (int i = 0; i < 0x10000; i++) {
        int r5 = i & 0x1f;
        int g5 = (i >> 5) & 0x1f;
        int b5 = (i >> 10) & 0x1f;
        int g6 = (g5 << 1) | (g5 >> 4);
        uint16_t c = (uint16_t)((r5 << systemRedShift) | (g6 << (systemGreenShift - 1)) |
                                (b5 << systemBlueShift));
        systemColorMap16[i] = sys_htobe16(c);
    }
}

static const char* getExt(const char* name) {
    const char* dot = strrchr(name, '.');
    return dot ? dot + 1 : NULL;
}

static IMAGE_TYPE typeFromName(const char* name) {
    const char* ext = getExt(name);

    if (!ext) return IMAGE_UNKNOWN;

    if (!StrCaselessCompare(ext, "gba") || !StrCaselessCompare(ext, "agb") ||
        !StrCaselessCompare(ext, "bin") || !StrCaselessCompare(ext, "elf") ||
        !StrCaselessCompare(ext, "mb")) {
        return IMAGE_GBA;
    }
    if (!StrCaselessCompare(ext, "gb") || !StrCaselessCompare(ext, "gbc") ||
        !StrCaselessCompare(ext, "cgb") || !StrCaselessCompare(ext, "sgb") ||
        !StrCaselessCompare(ext, "dmg")) {
        return IMAGE_GB;
    }

    return IMAGE_UNKNOWN;
}

static void romPath(char* out, int outlen, const char* name, const char* forcedExt) {
    if (forcedExt) {
        sys_snprintf(out, outlen, "%s/%s.%s", ROM_DIR, name, forcedExt);
    } else {
        sys_snprintf(out, outlen, "%s/%s", ROM_DIR, name);
    }
}

static void ensureRomDir(void) {
    FileRef fr;

    if (VFSFileOpen(volref, (char*)ROM_DIR, vfsModeRead, &fr) == errNone) {
        VFSFileClose(fr);
    } else {
        VFSDirCreate(volref, (char*)ROM_DIR);
    }
}

static uint8_t* readFile(const char* path, UInt32* size) {
    FileRef fr;
    uint8_t* buf = NULL;
    UInt32 nread;

    *size = 0;
    if (VFSFileOpen(volref, (char*)path, vfsModeRead, &fr) != errNone) return NULL;

    if (VFSFileSize(fr, size) == errNone && *size > 0) {
        buf = (uint8_t*)malloc(*size);
        if (buf && (VFSFileRead(fr, *size, buf, &nread) != errNone || nread != *size)) {
            free(buf);
            buf = NULL;
        }
    }

    VFSFileClose(fr);
    return buf;
}

static bool writeFile(const char* path, const uint8_t* buf, UInt32 size) {
    FileRef fr;
    UInt32 nwritten;
    Err err;

    if ((err = VFSFileOpen(volref, (char*)path, vfsModeWrite, &fr)) != errNone) {
        VFSFileCreate(volref, (char*)path);
        err = VFSFileOpen(volref, (char*)path, vfsModeWrite, &fr);
    }
    if (err != errNone) return false;

    err = VFSFileWrite(fr, size, buf, &nwritten);
    VFSFileClose(fr);

    return err == errNone && nwritten == size;
}

// ---------------------------------------------------------------------------
// Video
// ---------------------------------------------------------------------------

static Boolean validwindow(void) {
    FormType* frm = FrmGetActiveForm();
    return frm && FrmGetWindowHandle(frm) == WinGetActiveWindow();
}

static void updateScale(void) {
    scale = (screenWidth >= (UInt32)(emuWidth * 2) &&
             (screenHeight - TITLE_H) >= (UInt32)(emuHeight * 2)) ? 2 : 1;
}

void systemDrawScreen(void) {
    WinHandle wh;
    BitmapType* bmp;
    uint16_t *dst, c16;
    const uint16_t* src;
    int x, y, x0;

    if (!ready || !validwindow()) return;

    wh = WinGetDisplayWindow();
    bmp = WinGetBitmap(wh);
    dst = (uint16_t*)BmpGetBits(bmp);
    src = (const uint16_t*)g_pix;

    x0 = ((int)screenWidth - emuWidth * scale) / 2;
    if (x0 < 0) x0 = 0;
    dst += TITLE_H * screenWidth + x0;

    if (scale == 1) {
        if ((int)screenWidth == emuWidth) {
            MemMove(dst, src, emuWidth * emuHeight * 2);
        } else {
            for (y = 0; y < emuHeight; y++) {
                MemMove(dst, src, emuWidth * 2);
                src += emuWidth;
                dst += screenWidth;
            }
        }
    } else {
        for (y = 0; y < emuHeight; y++) {
            for (x = 0; x < emuWidth; x++) {
                c16 = src[x];
                dst[x * 2] = c16;
                dst[x * 2 + 1] = c16;
                dst[screenWidth + x * 2] = c16;
                dst[screenWidth + x * 2 + 1] = c16;
            }
            src += emuWidth;
            dst += screenWidth * 2;
        }
    }

    pumpkin_dirty_region_mode(dirtyRegionBegin);
    pumpkin_screen_dirty(wh, x0, TITLE_H, emuWidth * scale, emuHeight * scale);
    pumpkin_dirty_region_mode(dirtyRegionEnd);
}

void systemGbBorderOn(void) {
    gbBorderOn = true;
    emuWidth = gbBorderLineSkip = (int)kSGBWidth;
    emuHeight = (int)kSGBHeight;
    gbBorderColumnSkip = (int)(kSGBWidth - kGBWidth) >> 1;
    gbBorderRowSkip = (int)(kSGBHeight - kGBHeight) >> 1;
    updateScale();
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------

static bool extKeyDown(const uint64_t* ext, int key) {
    return (ext[key >> 6] >> (key & 63)) & 1;
}

static uint32_t readKeypad(void) {
    uint32_t keyMask, modMask, j = 0;
    uint64_t ext[2];

    pumpkin_status(NULL, NULL, &keyMask, &modMask, NULL, ext);

    // Bit layout is the GBA KEYINPUT register, active high:
    // 0 A, 1 B, 2 Select, 3 Start, 4 Right, 5 Left, 6 Up, 7 Down, 8 R, 9 L
    if (extKeyDown(ext, 'x') || extKeyDown(ext, 'X')) j |= 1 << 0;
    if (extKeyDown(ext, 'z') || extKeyDown(ext, 'Z')) j |= 1 << 1;
    if (extKeyDown(ext, 8) || extKeyDown(ext, '\t')) j |= 1 << 2;    // Backspace/Tab
    if (extKeyDown(ext, 13) || extKeyDown(ext, ' ')) j |= 1 << 3;    // Enter/Space
    if (keyMask & keyBitRight) j |= 1 << 4;
    if (keyMask & keyBitLeft) j |= 1 << 5;
    if (keyMask & keyBitPageUp) j |= 1 << 6;                         // Up arrow
    if (keyMask & keyBitPageDown) j |= 1 << 7;                       // Down arrow
    if (extKeyDown(ext, 's') || extKeyDown(ext, 'S')) j |= 1 << 8;   // R
    if (extKeyDown(ext, 'a') || extKeyDown(ext, 'A')) j |= 1 << 9;   // L

    // Opposing directions confuse some games.
    if ((j & 0x30) == 0x30) j &= ~0x30;
    if ((j & 0xC0) == 0xC0) j &= ~0xC0;

    return j;
}

uint32_t systemReadJoypad(int which) {
    (void)which;
    return joyMask;
}

bool systemReadJoypads(void) {
    return true;
}

// ---------------------------------------------------------------------------
// Battery saves and save states
// ---------------------------------------------------------------------------

static bool saveRamPtr(uint8_t** ptr, int* size) {
    if (romType == IMAGE_GBA) {
        switch (coreOptions.saveType) {
            case GBA_SAVE_EEPROM:
            case GBA_SAVE_EEPROM_SENSOR:
                *ptr = eepromData;
                *size = eepromSize;
                return true;
            case GBA_SAVE_FLASH:
                *ptr = flashSaveMemory;
                *size = g_flashSize;
                return true;
            case GBA_SAVE_SRAM:
                *ptr = flashSaveMemory;
                *size = SIZE_SRAM;
                return true;
            default:
                return false;
        }
    }

    if (g_gbCartData.has_battery() && gbRam) {
        *ptr = gbRam;
        *size = (int)g_gbCartData.ram_size();
        return true;
    }

    return false;
}

static void readBattery(void) {
    uint8_t *ptr, *buf;
    int size;
    UInt32 fsize;
    char path[MAX_PATH];

    if (!saveRamPtr(&ptr, &size)) return;

    romPath(path, sizeof(path), romBase, "sav");
    if ((buf = readFile(path, &fsize)) != NULL) {
        MemMove(ptr, buf, fsize < (UInt32)size ? fsize : (UInt32)size);
        free(buf);
        debug(DEBUG_INFO, VBAM_TAG, "loaded battery %s (%u bytes)", path, (unsigned)fsize);
    }
}

static void writeBattery(void) {
    uint8_t* ptr;
    int size;
    char path[MAX_PATH];

    if (!core || !saveRamPtr(&ptr, &size)) return;

    romPath(path, sizeof(path), romBase, "sav");
    if (writeFile(path, ptr, size)) {
        debug(DEBUG_INFO, VBAM_TAG, "wrote battery %s (%d bytes)", path, size);
    } else {
        debug(DEBUG_ERROR, VBAM_TAG, "failed writing battery %s", path);
    }
}

static void saveState(void) {
    unsigned len;
    char path[MAX_PATH];

    if (!core || !stateBuf) return;

    len = core->emuWriteState(stateBuf);
    romPath(path, sizeof(path), romBase, "ss0");
    if (len && writeFile(path, stateBuf, len)) {
        debug(DEBUG_INFO, VBAM_TAG, "wrote state %s (%u bytes)", path, len);
    } else {
        debug(DEBUG_ERROR, VBAM_TAG, "failed writing state %s", path);
    }
}

static void loadState(void) {
    uint8_t* buf;
    UInt32 fsize;
    char path[MAX_PATH];

    if (!core) return;

    romPath(path, sizeof(path), romBase, "ss0");
    if ((buf = readFile(path, &fsize)) != NULL) {
        if (core->emuReadState(buf)) {
            debug(DEBUG_INFO, VBAM_TAG, "loaded state %s", path);
        } else {
            debug(DEBUG_ERROR, VBAM_TAG, "state %s rejected by core", path);
        }
        free(buf);
    }
}

// ---------------------------------------------------------------------------
// GBA save type detection (adapted from the libretro port)
// ---------------------------------------------------------------------------

static void load_image_preferences(void) {
    bool found = false;
    int saveType = GBA_SAVE_AUTO;
    int saveSize = 0;
    bool hasRtc = false;
    bool hasRumble = false;
    char buffer[5];
    unsigned i, found_no = 0;

    coreOptions.saveType = GBA_SAVE_AUTO;
    g_flashSize = SIZE_FLASH1M;
    eepromSize = SIZE_EEPROM_512;
    coreOptions.rtcEnabled = false;
    coreOptions.mirroringEnable = false;

    buffer[0] = g_rom[0xac];
    buffer[1] = g_rom[0xad];
    buffer[2] = g_rom[0xae];
    buffer[3] = g_rom[0xaf];
    buffer[4] = 0;
    debug(DEBUG_INFO, VBAM_TAG, "game code %s", buffer);

    for (i = 0; i < 512; i++) {
        if (gbaover[i].romid[0] && !strcmp(gbaover[i].romid, buffer)) {
            found = true;
            found_no = i;
            break;
        }
    }

    if (found) {
        debug(DEBUG_INFO, VBAM_TAG, "found in gba-over: %s", gbaover[found_no].romtitle);
        saveType = gbaover[found_no].saveType;
        saveSize = gbaover[found_no].saveSize;
        hasRtc = gbaover[found_no].rtcEnabled;

        if ((saveType != GBA_SAVE_NONE) && !saveSize) {
            if (saveType == GBA_SAVE_EEPROM || saveType == GBA_SAVE_EEPROM_SENSOR)
                saveSize = SIZE_EEPROM_512;
            if (saveType == GBA_SAVE_FLASH)
                saveSize = SIZE_FLASH512;
        }
    }

    if (saveType == GBA_SAVE_AUTO) {
        if (find_string(g_rom, romSize, "FLASH1M_")) {
            saveType = GBA_SAVE_FLASH;
            saveSize = SIZE_FLASH1M;
        } else if (find_string(g_rom, romSize, "FLASH_") ||
                   find_string(g_rom, romSize, "FLASH512_")) {
            saveType = GBA_SAVE_FLASH;
            saveSize = SIZE_FLASH512;
        } else if (find_string(g_rom, romSize, "EEPROM_")) {
            saveType = GBA_SAVE_EEPROM;
            saveSize = SIZE_EEPROM_8K;
        } else if (find_string(g_rom, romSize, "SRAM_F") ||
                   find_string(g_rom, romSize, "SRAM_")) {
            saveType = GBA_SAVE_SRAM;
        } else {
            saveType = GBA_SAVE_NONE;
        }

        if (find_string(g_rom, romSize, "SIIRTC_V")) {
            hasRtc = true;
        }
    }

    switch (saveType) {
        case GBA_SAVE_SRAM:
            coreOptions.saveType = GBA_SAVE_SRAM;
            g_flashSize = SIZE_SRAM;
            break;
        case GBA_SAVE_FLASH:
            coreOptions.saveType = GBA_SAVE_FLASH;
            flashSetSize(saveSize);
            break;
        case GBA_SAVE_EEPROM:
        case GBA_SAVE_EEPROM_SENSOR:
            coreOptions.saveType = saveType;
            eepromSetSize(saveSize);
            break;
        case GBA_SAVE_NONE:
            coreOptions.saveType = GBA_SAVE_NONE;
            break;
        default:
        case GBA_SAVE_AUTO:
            coreOptions.saveType = GBA_SAVE_AUTO;
            break;
    }

    coreOptions.rtcEnabled = hasRtc;
    rtcEnable(hasRtc);

    // Game codes starting with 'R' or 'V' have rumble support.
    hasRumble = (buffer[0] == 'R') || (buffer[0] == 'V');
    rtcEnableRumble(!hasRtc && hasRumble);

    // Game codes starting with 'F' are classic/famicom games.
    coreOptions.mirroringEnable = (buffer[0] == 'F');
    doMirroring(coreOptions.mirroringEnable);

    debug(DEBUG_INFO, VBAM_TAG, "saveType %d rtc %d mirroring %d",
          coreOptions.saveType, coreOptions.rtcEnabled, coreOptions.mirroringEnable ? 1 : 0);
}

// ---------------------------------------------------------------------------
// ROM loading / unloading
// ---------------------------------------------------------------------------

static bool loadRom(const char* name) {
    uint8_t* data;
    UInt32 fsize;
    char path[MAX_PATH];
    const char* dot;

    romType = typeFromName(name);
    if (romType == IMAGE_UNKNOWN) return false;

    StrNCopy(romFile, name, sizeof(romFile) - 1);
    romFile[sizeof(romFile) - 1] = 0;
    StrNCopy(romBase, name, sizeof(romBase) - 1);
    romBase[sizeof(romBase) - 1] = 0;
    if ((dot = strrchr(romBase, '.')) != NULL) {
        romBase[dot - romBase] = 0;
    }

    romPath(path, sizeof(path), name, NULL);
    if ((data = readFile(path, &fsize)) == NULL) {
        debug(DEBUG_ERROR, VBAM_TAG, "cannot read %s", path);
        return false;
    }

    soundInit();

    if (romType == IMAGE_GBA) {
        const char* ext = getExt(name);
        coreOptions.cpuIsMultiBoot = (ext && !StrCaselessCompare(ext, "mb"));

        romSize = CPULoadRomData((const char*)data, (int)fsize);
        free(data);
        if (!romSize) return false;

        core = &GBASystem;
        load_image_preferences();
        soundSetSampleRate(SAMPLERATE);
        CPUInit(NULL, false);  // HLE BIOS
        emuWidth = 240;
        emuHeight = 160;
        CPUReset();
    } else {
        bool ok = gbLoadRomData((const char*)data, fsize);
        free(data);
        if (!ok) return false;

        romSize = (int)fsize;
        core = &GBSystem;
        gbBorderOn = false;  // systemGbBorderOn() re-enables it if the game asks
        gbGetHardwareType();
        gbCPUInit(NULL, false);

        if (gbBorderOn) {
            emuWidth = gbBorderLineSkip = (int)kSGBWidth;
            emuHeight = (int)kSGBHeight;
            gbBorderColumnSkip = (int)(kSGBWidth - kGBWidth) >> 1;
            gbBorderRowSkip = (int)(kSGBHeight - kGBHeight) >> 1;
        } else {
            emuWidth = gbBorderLineSkip = (int)kGBWidth;
            emuHeight = (int)kGBHeight;
            gbBorderColumnSkip = gbBorderRowSkip = 0;
        }

        gbSoundSetSampleRate(SAMPLERATE);
        gbSoundSetDeclicking(1);
        gbReset();
    }

    updateScale();
    readBattery();

    if (!stateBuf) stateBuf = (uint8_t*)malloc(STATE_BUF_SIZE);

    emulating = 1;
    return true;
}

static void unloadRom(void) {
    if (core) {
        writeBattery();
        emulating = 0;
        core->emuCleanUp();
        soundShutdown();
        core = NULL;
    }
    romType = IMAGE_UNKNOWN;
}

// ---------------------------------------------------------------------------
// ROM chooser
// ---------------------------------------------------------------------------

static void freeRomItems(void) {
    for (int i = 0; i < numRoms; i++) {
        if (romItems[i]) MemPtrFree(romItems[i]);
        romItems[i] = NULL;
    }
    numRoms = 0;
}

static int compareItems(const void* a, const void* b) {
    return StrCaselessCompare(*(char* const*)a, *(char* const*)b);
}

static void scanRoms(void) {
    FileRef fr;
    FileInfoType info;
    UInt32 iterator;
    char name[128];

    freeRomItems();

    if (VFSFileOpen(volref, (char*)ROM_DIR, vfsModeRead, &fr) != errNone) return;

    iterator = vfsIteratorStart;
    for (;;) {
        info.nameP = name;
        info.nameBufLen = sizeof(name);
        if (VFSDirEntryEnumerate(fr, &iterator, &info) != errNone) break;
        if (info.attributes & vfsFileAttrDirectory) continue;
        if (typeFromName(name) == IMAGE_UNKNOWN) continue;
        if (numRoms >= MAX_ROMS) break;

        romItems[numRoms] = (char*)MemPtrNew(StrLen(name) + 1);
        StrCopy(romItems[numRoms], name);
        numRoms++;
    }
    VFSFileClose(fr);

    qsort(romItems, numRoms, sizeof(char*), compareItems);
}

// ---------------------------------------------------------------------------
// Forms and events
// ---------------------------------------------------------------------------

static void resize(FormType* frm) {
    UInt32 sw, sh;
    WinHandle wh;
    RectangleType rect;

    WinScreenMode(winScreenModeGet, &sw, &sh, NULL, NULL);
    wh = FrmGetWindowHandle(frm);
    RctSetRectangle(&rect, 0, 0, sw, sh);
    WinSetBounds(wh, &rect);
}

static void MenuEvent(UInt16 id) {
    switch (id) {
        case menuOpen:
            backToChooser = true;
            break;
        case menuReset:
            doReset = true;
            break;
        case menuSaveState:
            doSaveState = true;
            break;
        case menuLoadState:
            doLoadState = true;
            break;
        case menuQuit:
            quitApp = true;
            break;
    }
}

static Boolean MainFormHandleEvent(EventType* event) {
    FormType* frm;
    Boolean handled = false;

    switch (event->eType) {
        case frmOpenEvent:
            frm = FrmGetActiveForm();
            resize(frm);
            FrmSetTitle(frm, romFile);
            FrmDrawForm(frm);
            ready = true;
            handled = true;
            break;
        case frmUpdateEvent:
            frm = FrmGetActiveForm();
            FrmDrawForm(frm);
            handled = true;
            break;
        case menuEvent:
            MenuEvent(event->data.menu.itemID);
            handled = true;
            break;
        default:
            break;
    }

    return handled;
}

static Boolean ChooseFormHandleEvent(EventType* event) {
    FormType* frm;
    ListType* lst;
    UInt16 index;
    Boolean handled = false;

    switch (event->eType) {
        case frmOpenEvent:
            frm = FrmGetActiveForm();
            resize(frm);
            index = FrmGetObjectIndex(frm, romList);
            lst = (ListType*)FrmGetObjectPtr(frm, index);
            if (numRoms > 0) {
                LstSetListChoices(lst, romItems, numRoms);
                LstSetSelection(lst, 0);
            } else {
                LstSetListChoices(lst, NULL, 0);
            }
            FrmDrawForm(frm);
            handled = true;
            break;
        case ctlSelectEvent:
            if (event->data.ctlSelect.controlID == runBtn && numRoms > 0) {
                frm = FrmGetActiveForm();
                index = FrmGetObjectIndex(frm, romList);
                lst = (ListType*)FrmGetObjectPtr(frm, index);
                romIndex = LstGetSelection(lst);
                handled = true;
            }
            break;
        case menuEvent:
            MenuEvent(event->data.menu.itemID);
            handled = true;
            break;
        default:
            break;
    }

    return handled;
}

static Boolean ApplicationHandleEvent(EventType* event) {
    FormPtr frm;
    UInt16 form;
    Boolean handled = false;

    switch (event->eType) {
        case frmLoadEvent:
            form = event->data.frmLoad.formID;
            frm = FrmInitForm(form);
            FrmSetActiveForm(frm);
            switch (form) {
                case MainForm:
                    FrmSetEventHandler(frm, MainFormHandleEvent);
                    break;
                case ChooseForm:
                    FrmSetEventHandler(frm, ChooseFormHandleEvent);
                    break;
            }
            handled = true;
            break;
        default:
            break;
    }

    return handled;
}

// Pump one event and drain the task mailbox; us is the maximum wait.
// Returns false when the app must exit.
static Boolean pumpEvents(uint32_t us) {
    EventType event;
    Err err;
    unsigned char* buf;
    unsigned int len;

    if (thread_must_end()) return false;

    EvtGetEventUs(&event, us);
    if (!SysHandleEvent(&event)) {
        if (!MenuHandleEvent(NULL, &event, &err)) {
            if (!ApplicationHandleEvent(&event)) {
                FrmDispatchEvent(&event);
            }
        }
    }

    if (thread_server_read_timeout(0, &buf, &len) == -1) return false;
    if (buf) sys_free(buf);

    if (event.eType == appStopEvent) {
        appStopped = true;
        return false;
    }

    return true;
}

// Chooser loop: runs until a ROM is picked, quit, or app stop.
static Boolean chooserLoop(void) {
    for (;;) {
        if (!pumpEvents(20000)) return false;
        if (quitApp || appStopped) return false;
        if (romIndex >= 0) return true;
    }
}

// ---------------------------------------------------------------------------
// Frame loop
// ---------------------------------------------------------------------------

void systemFrame(void) {
    frameDone = 1;
}

static void runEmulator(void) {
    int64_t tNext = sys_get_clock();
    Boolean audioDead = false;

    // temp perf stats
    int statFrames = 0;
    int64_t statEmu = 0, statPump = 0, statWait = 0;
    int64_t statT0 = sys_get_clock(), t1, t2, t3, t4;

    while (emulating) {
        t1 = sys_get_clock();
        if (!pumpEvents(0)) break;
        if (quitApp || backToChooser) break;

        if (doReset) {
            doReset = false;
            core->emuReset();
        }
        if (doSaveState) {
            doSaveState = false;
            saveState();
        }
        if (doLoadState) {
            doLoadState = false;
            loadState();
        }

        t2 = sys_get_clock();

        joyMask = readKeypad();

        frameDone = 0;
        while (!frameDone) {
            core->emuMain(core->emuCount);
        }
        t3 = sys_get_clock();

        // The core sets this to SYSTEM_SAVE_UPDATED whenever cart RAM is
        // written; flush to disk once it has been quiet for that many frames.
        if (systemSaveUpdateCounter > 0) {
            if (--systemSaveUpdateCounter == 0) {
                writeBattery();
            }
        }

        // Pacing: sync to the audio ring when a stream is running, else to
        // the wall clock.
        int queued = audioDead ? -1 : soundPumpkinQueuedFrames();
        if (queued >= 0) {
            int64_t waitStart = sys_get_clock();
            while (queued > AUDIO_HIGH_WATER) {
                if (!pumpEvents(2000)) return;
                if (quitApp || backToChooser) return;
                queued = soundPumpkinQueuedFrames();
                if (queued < 0) break;
                if (sys_get_clock() - waitStart > 400000) {
                    // nobody is draining the ring: the audio stream is dead,
                    // fall back to wall-clock pacing for this session
                    debug(DEBUG_ERROR, VBAM_TAG, "audio ring not draining, using clock pacing");
                    audioDead = true;
                    break;
                }
            }
            tNext = sys_get_clock();
        } else {
            tNext += FRAME_US;
            int64_t now = sys_get_clock();
            if (tNext > now) {
                if (!pumpEvents((uint32_t)(tNext - now))) break;
            } else if (tNext < now - 4 * FRAME_US) {
                tNext = now;  // fell behind; don't try to catch up
            }
        }

        t4 = sys_get_clock();
        statFrames++;
        statPump += t2 - t1;
        statEmu += t3 - t2;
        statWait += t4 - t3;
        if (t4 - statT0 >= 1000000) {
            debug(DEBUG_INFO, VBAM_TAG,
                  "perf: fps=%d pump=%dus emu=%dus wait=%dus queued=%d dead=%d",
                  statFrames, (int)(statPump / statFrames), (int)(statEmu / statFrames),
                  (int)(statWait / statFrames), soundPumpkinQueuedFrames(), audioDead ? 1 : 0);
            statFrames = 0;
            statEmu = statPump = statWait = 0;
            statT0 = t4;
        }
    }
}

// ---------------------------------------------------------------------------
// Remaining system callbacks
// ---------------------------------------------------------------------------

void systemMessage(int id, const char* fmt, ...) {
    sys_va_list ap;

    (void)id;
    sys_va_start(ap, fmt);
    debugva_full("vbam", "systemMessage", 0, DEBUG_ERROR, VBAM_TAG, fmt, ap);
    sys_va_end(ap);
}

void systemSendScreen(void) {}
void systemSetTitle(const char* title) { (void)title; }
void systemScreenCapture(int a) { (void)a; }
void systemScreenMessage(const char* msg) {
    debug(DEBUG_INFO, VBAM_TAG, "%s", msg);
}
void systemShowSpeed(int speed) { (void)speed; }
void system10Frames(void) {}
bool systemPauseOnFrame(void) { return false; }
void systemGbPrint(uint8_t* data, int len, int pages, int feed, int palette, int contrast) {
    (void)data; (void)len; (void)pages; (void)feed; (void)palette; (void)contrast;
}
uint32_t systemGetClock(void) {
    return (uint32_t)(sys_get_clock() / 1000);
}

std::unique_ptr<SoundDriver> systemSoundInit(void) {
    soundShutdown();
    return std::make_unique<SoundPumpkin>();
}
bool systemCanChangeSoundQuality(void) { return true; }
void systemOnWriteDataToSoundBuffer(const uint16_t* finalWave, int length) {
    (void)finalWave; (void)length;
}
void systemOnSoundShutdown(void) {}

void systemUpdateMotionSensor(void) {}
int systemGetSensorX(void) { return 2047; }
int systemGetSensorY(void) { return 2047; }
int systemGetSensorZ(void) { return 0; }
uint8_t systemGetSensorDarkness(void) { return 0xE8; }
void systemCartridgeRumble(bool e) { (void)e; }

// ---------------------------------------------------------------------------
// PilotMain
// ---------------------------------------------------------------------------

extern "C" UInt32 PilotMain(UInt16 cmd, MemPtr cmdPBP, UInt16 launchFlags) {
    UInt32 iterator;

    (void)cmdPBP;
    (void)launchFlags;

    if (cmd != sysAppLaunchCmdNormalLaunch) return 0;

    iterator = vfsIteratorStart;
    VFSVolumeEnumerate(&volref, &iterator);

    WinScreenGetAttribute(winScreenWidth, &screenWidth);
    WinScreenGetAttribute(winScreenHeight, &screenHeight);
    FrmCenterDialogs(true);

    ensureRomDir();
    buildColorMap16();

    coreOptions.mirroringEnable = false;
    coreOptions.parseDebug = true;
    coreOptions.cheatsEnabled = 0;
    coreOptions.skipSaveGameBattery = 0;
    coreOptions.gbPrinterEnabled = 0;
    coreOptions.skipBios = true;

    quitApp = false;
    appStopped = false;

    Boolean autoran = false;

    for (;;) {
        scanRoms();
        romIndex = -1;
        ready = false;
        backToChooser = false;
        doReset = doSaveState = doLoadState = false;

        if (numRoms == 1 && !autoran) {
            // exactly one ROM: skip the chooser, like the Doom port does
            romIndex = 0;
        } else {
            FrmGotoForm(ChooseForm);
            if (!chooserLoop()) break;
        }
        autoran = true;
        if (romIndex < 0 || romIndex >= numRoms) break;

        if (!loadRom(romItems[romIndex])) {
            debug(DEBUG_ERROR, VBAM_TAG, "failed to load %s", romItems[romIndex]);
            continue;
        }

        ready = false;
        FrmGotoForm(MainForm);
        runEmulator();
        unloadRom();

        if (quitApp || appStopped) break;
    }

    unloadRom();
    freeRomItems();
    if (stateBuf) {
        free(stateBuf);
        stateBuf = NULL;
    }
    FrmCloseAllForms();

    return 0;
}
