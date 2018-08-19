#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

#include "SoundRetro.h"
#include "libretro.h"

#include "../System.h"
#include "../Util.h"
#include "../apu/Blip_Buffer.h"
#include "../apu/Gb_Apu.h"
#include "../apu/Gb_Oscs.h"
#include "../common/Port.h"
#include "../common/ConfigManager.h"
#include "../gba/Cheats.h"
#include "../gba/EEprom.h"
#include "../gba/Flash.h"
#include "../gba/GBAGfx.h"
#include "../gba/Globals.h"
#include "../gba/RTC.h"
#include "../gba/Sound.h"
#include "../gba/bios.h"

#include "../gb/gb.h"
#include "../gb/gbCheats.h"
#include "../gb/gbGlobals.h"
#include "../gb/gbMemory.h"
#include "../gb/gbSGB.h"
#include "../gb/gbSound.h"

#define RETRO_DEVICE_GBA             RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 0)
#define RETRO_DEVICE_GBA_ALT1        RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 1)
#define RETRO_DEVICE_GBA_ALT2        RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 2)

#define ISHEXDEC ((codeLine[cursor]>='0') && (codeLine[cursor]<='9')) || ((codeLine[cursor]>='a') && (codeLine[cursor]<='f')) || ((codeLine[cursor]>='A') && (codeLine[cursor]<='F'))

static retro_log_printf_t log_cb;
static retro_video_refresh_t video_cb;
static retro_input_poll_t poll_cb;
static retro_input_state_t input_cb;
static retro_environment_t environ_cb;
retro_audio_sample_batch_t audio_batch_cb;

static char retro_system_directory[4096];
static char biosfile[4096];
static float sndFiltering = 0.5f;
static bool sndInterpolation = true;
static bool can_dupe = false;
static bool usebios = false;
static unsigned retropad_layout = 1;

static const double FramesPerSecond =  (16777216.0 / 280896.0); // 59.73
static const long SampleRate = 32768;
static const int GBWidth = 160;
static const int GBHeight = 144;
static const int SGBWidth = 256;
static const int SGBHeight = 224;
static const int GBAWidth = 240;
static const int GBAHeight = 160;
static unsigned width = 240;
static unsigned height = 160;
static EmulatedSystem* core = NULL;
static IMAGE_TYPE type = IMAGE_UNKNOWN;

uint16_t systemColorMap16[0x10000];
uint32_t systemColorMap32[0x10000];
int RGB_LOW_BITS_MASK = 0;
int systemRedShift = 0;
int systemBlueShift = 0;
int systemGreenShift = 0;
int systemColorDepth = 32;
int systemDebug = 0;
int systemVerbose = 0;
int systemFrameSkip = 0;
int systemSaveUpdateCounter = SYSTEM_SAVE_NOT_UPDATED;
int systemSpeed = 0;
int emulating = 0;

void (*dbgOutput)(const char* s, uint32_t addr);
void (*dbgSignal)(int sig, int number);

#define GS555(x) (x | (x << 5) | (x << 10))
uint16_t systemGbPalette[24] = {
    GS555(0x1f), GS555(0x15), GS555(0x0c), 0,
    GS555(0x1f), GS555(0x15), GS555(0x0c), 0,
    GS555(0x1f), GS555(0x15), GS555(0x0c), 0,
    GS555(0x1f), GS555(0x15), GS555(0x0c), 0,
    GS555(0x1f), GS555(0x15), GS555(0x0c), 0,
    GS555(0x1f), GS555(0x15), GS555(0x0c), 0
};

extern int gbRomType; // gets type from header 0x147
extern int gbBattery; // enabled when gbRamSize != 0

static bool gb_hasbattery(void)
{
    switch (gbRomType) {
    case 0x03: // MBC1
    case 0xff: // MBC1
    case 0x0d: // MM01
    case 0x0f:
    case 0x10: // MBC3 + extended
    case 0x13:
    case 0xfc: // MBC3
    case 0x1b:
    case 0x1e: // MBC5
    //incomplete, does not save gbTAMA5ram
    case 0xfd: // TAMA5 + extended
    case 0x06: // MBC2
    case 0x22: // MBC7
        return true;
    }
    return false;
}

static bool gb_hasrtc(void)
{
    switch (gbRomType) {
    case 0x0f:
    case 0x10: // MBC3 + extended
    case 0x13:
    case 0xfd: // TAMA5 + extended
        return true;
    }
    return false;
}

static void* gba_savedata_ptr(void)
{
    if ((saveType == 1) | (saveType == 4))
        return eepromData;
    if ((saveType == 2) | (saveType == 3))
        return flashSaveMemory;
    return 0;
}

static size_t gba_savedata_size(void)
{
    if ((saveType == 1) | (saveType == 4))
        return eepromSize;
    if ((saveType == 2) | (saveType == 3))
        return flashSize;
    return 0;
}

static void* gb_savedata_ptr(void)
{
    if (gb_hasbattery())
        return gbRam;
    return 0;
}

static size_t gb_savedata_size(void)
{
    if (gb_hasbattery())
        return gbRamSize;
    return 0;
}

static void* gb_rtcdata_prt(void)
{
    if (gb_hasrtc()) {
        switch (gbRomType) {
        case 0x0f:
        case 0x10: // MBC3 + extended
            return &gbDataMBC3.mapperSeconds;
        case 0x13:
        case 0xfd: // TAMA5 + extended
            return &gbDataTAMA5.mapperSeconds;
        }
    }
    return 0;
}

static size_t gb_rtcdata_size(void)
{
    if (gb_hasrtc()) {
        switch (gbRomType) {
        case 0x0f:
        case 0x10: // MBC3 + extended
            return (10 * sizeof(int) + sizeof(time_t));
            break;
        case 0x13:
        case 0xfd: // TAMA5 + extended
            return (14 * sizeof(int) + sizeof(time_t));
            break;
        }
    }
    return 0;
}

static void* savedata_ptr(void)
{
    if (type == IMAGE_GBA)
        return gba_savedata_ptr();
    if (type == IMAGE_GB)
        return gb_savedata_ptr();
    return 0;
}

static size_t savedata_size(void)
{
    if (type == IMAGE_GBA)
        return gba_savedata_size();
    if (type == IMAGE_GB)
        return gb_savedata_size();
    return 0;
}

static void* rtcdata_ptr(void)
{
    if (type == IMAGE_GB)
        return gb_rtcdata_prt();
    return 0;
}

static size_t rtcdata_size(void)
{
    if (type == IMAGE_GB)
        return gb_rtcdata_size();
    return 0;
}

static void* wram_ptr(void)
{
    if (type == IMAGE_GBA)
        return workRAM;
    if (type == IMAGE_GB)
        return gbMemoryMap[0x0c];
    return 0;
}

static size_t wram_size(void)
{
    if (type == IMAGE_GBA)
        return 0x40000;
    if (type == IMAGE_GB)
        // only use 1st bank of wram,  libretro doesnt seem to handle
        // the switching bank properly in GBC mode. This is to avoid possible incorrect reads.
        // For cheevos purposes, this bank is accessed using retro_memory_descriptor instead.
        return gbCgbMode ? 0x1000 : 0x2000;
}

static void* vram_ptr(void)
{
    if (type == IMAGE_GBA)
        return vram;
    if (type == IMAGE_GB)
        return gbMemoryMap[0x08] ;
    return 0;
}

static size_t vram_size(void)
{
    if (type == IMAGE_GBA)
        return 0x20000;
    if (type == IMAGE_GB)
        return 0x2000;;
    return 0;
}

static void gbUpdateRTC(void)
{
    if (gb_hasbattery()) {
        struct tm* lt;
        time_t rawtime;
        time(&rawtime);
        lt = localtime(&rawtime);

        switch (gbRomType) {
        case 0x0f:
        case 0x10: {
                gbDataMBC3.mapperSeconds = lt->tm_sec;
                gbDataMBC3.mapperMinutes = lt->tm_min;
                gbDataMBC3.mapperHours = lt->tm_hour;
                gbDataMBC3.mapperDays = lt->tm_yday & 255;
                gbDataMBC3.mapperControl = (gbDataMBC3.mapperControl & 0xfe) | (lt->tm_yday > 255 ? 1 : 0);
                gbDataMBC3.mapperLastTime = rawtime;
            }
            break;
        case 0xfd: {
                uint8_t gbDaysinMonth[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
                gbDataTAMA5.mapperSeconds = lt->tm_sec;
                gbDataTAMA5.mapperMinutes = lt->tm_min;
                gbDataTAMA5.mapperHours = lt->tm_hour;
                gbDataTAMA5.mapperDays = 1;
                gbDataTAMA5.mapperMonths = 1;
                gbDataTAMA5.mapperYears = 1970;
                gbDataTAMA5.mapperLastTime = rawtime;
                int days = lt->tm_yday + 365 * 3;
                while (days) {
                    gbDataTAMA5.mapperDays++;
                    days--;
                    if (gbDataTAMA5.mapperDays > gbDaysinMonth[gbDataTAMA5.mapperMonths - 1]) {
                        gbDataTAMA5.mapperDays = 1;
                        gbDataTAMA5.mapperMonths++;
                        if (gbDataTAMA5.mapperMonths > 12) {
                            gbDataTAMA5.mapperMonths = 1;
                            gbDataTAMA5.mapperYears++;
                            if ((gbDataTAMA5.mapperYears & 3) == 0)
                                gbDaysinMonth[1] = 29;
                            else
                                gbDaysinMonth[1] = 28;
                        }
                    }
                }
                gbDataTAMA5.mapperControl = (gbDataTAMA5.mapperControl & 0xfe) | (lt->tm_yday > 255 ? 1 : 0);
            }
            break;
        }
    }
}

void* retro_get_memory_data(unsigned id)
{
    if (id == RETRO_MEMORY_SAVE_RAM)
        return savedata_ptr();
    //if (id == RETRO_MEMORY_RTC)
        //return rtcdata_ptr();
    if (id == RETRO_MEMORY_SYSTEM_RAM)
        return wram_ptr();
    if (id == RETRO_MEMORY_VIDEO_RAM)
        return vram_ptr();
    return 0;
}

size_t retro_get_memory_size(unsigned id)
{
    if (id == RETRO_MEMORY_SAVE_RAM)
        return savedata_size();
    //if (id == RETRO_MEMORY_RTC)
        //return rtcdata_size();
    if (id == RETRO_MEMORY_SYSTEM_RAM)
        return wram_size();
    if (id == RETRO_MEMORY_VIDEO_RAM)
        return vram_size();
    return 0;
}

unsigned retro_api_version(void)
{
    return RETRO_API_VERSION;
}

void retro_set_video_refresh(retro_video_refresh_t cb)
{
    video_cb = cb;
}

void retro_set_audio_sample(retro_audio_sample_t cb)
{
}

void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb)
{
    audio_batch_cb = cb;
}

void retro_set_input_poll(retro_input_poll_t cb)
{
    poll_cb = cb;
}

void retro_set_input_state(retro_input_state_t cb)
{
    input_cb = cb;
}

static void update_input_descriptors(void);

void retro_set_controller_port_device(unsigned port, unsigned device)
{
    if (port > 0) return; // GBA Only supports 1 controller

    log_cb(RETRO_LOG_INFO, "Controller %d'\n", device);
    switch (device) {

    case RETRO_DEVICE_JOYPAD:
    case RETRO_DEVICE_GBA:
    default:
        retropad_layout = 1;
        break;
    case RETRO_DEVICE_GBA_ALT1:
        retropad_layout = 2;
        break;
    case RETRO_DEVICE_GBA_ALT2:
        retropad_layout = 3;
        break;
    case RETRO_DEVICE_NONE:
        retropad_layout = 0;
        break;
    }

    update_input_descriptors();
}

void retro_set_environment(retro_environment_t cb)
{
   environ_cb = cb;

   struct retro_variable variables[] = {
      { "vbam_solarsensor", "Solar Sensor Level; 0|1|2|3|4|5|6|7|8|9|10" },
      { "vbam_usebios", "Use BIOS file (Restart); disabled|enabled" },
      { "vbam_soundinterpolation", "Sound Interpolation; disabled|enabled" },
      { "vbam_soundfiltering", "Sound Filtering; 5|6|7|8|9|10|0|1|2|3|4" },
      { "vbam_gbHardware", "(GB) Emulated Hardware; Automatic|Game Boy Color|Super Game Boy|Game Boy|Game Boy Advance|Super Game Boy 2" },
      { "vbam_showborders", "(GB) Show Borders; disabled|enabled|auto" },
      { "vbam_layer_1", "Show layer 1; enabled|disabled" },
      { "vbam_layer_2", "Show layer 2; enabled|disabled" },
      { "vbam_layer_3", "Show layer 3; enabled|disabled" },
      { "vbam_layer_4", "Show layer 4; enabled|disabled" },
      { "vbam_layer_5", "Show sprite layer; enabled|disabled" },
      { "vbam_layer_6", "Show window layer 1; enabled|disabled" },
      { "vbam_layer_7", "Show window layer 2; enabled|disabled" },
      { "vbam_layer_8", "Show sprite window layer; enabled|disabled" },
      { "vbam_sound_1", "Sound channel 1; enabled|disabled" },
      { "vbam_sound_2", "Sound channel 2; enabled|disabled" },
      { "vbam_sound_3", "Sound channel 3; enabled|disabled" },
      { "vbam_sound_4", "Sound channel 4; enabled|disabled" },
      { "vbam_sound_5", "Direct Sound A; enabled|disabled" },
      { "vbam_sound_6", "Direct Sound B; enabled|disabled" },
      { NULL, NULL },
   };

   static const struct retro_controller_description port_1[] = {
      { "GBA Joypad", RETRO_DEVICE_GBA },
      { "Alt Joypad YB", RETRO_DEVICE_GBA_ALT1 },
      { "Alt Joypad AB", RETRO_DEVICE_GBA_ALT2 },
      { NULL, 0 },
   };

   static const struct retro_controller_info ports[] = {
        {port_1, 3},
        { NULL, 0 }
    };

   cb(RETRO_ENVIRONMENT_SET_VARIABLES, variables);
   cb(RETRO_ENVIRONMENT_SET_CONTROLLER_INFO, (void*)ports);
}

void retro_get_system_info(struct retro_system_info *info)
{
   info->need_fullpath = false;
   info->valid_extensions = "dmg|gb|gbc|cgb|sgb|gba";
#ifdef GIT_VERSION
   info->library_version = "2.1.0" GIT_VERSION;
#else
   info->library_version = "2.1.0-GIT";
#endif
   info->library_name = "VBA-M";
   info->block_extract = false;
}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
   float aspect = (3.0f / 2.0f);
   if (type == IMAGE_GB)
      aspect = !gbBorderOn ? (10.0 / 9.0) : (8.0 / 7.0);

   info->geometry.base_width = width;
   info->geometry.base_height = height;
   info->geometry.max_width = width;
   info->geometry.max_height = height;
   info->geometry.aspect_ratio = aspect;
   info->timing.fps = FramesPerSecond;
   info->timing.sample_rate = (double)SampleRate;
}

void retro_init(void)
{
   struct retro_log_callback log;
   environ_cb(RETRO_ENVIRONMENT_GET_CAN_DUPE, &can_dupe);
   if (environ_cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log))
      log_cb = log.log;
   else
      log_cb = NULL;

   const char* dir = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &dir) && dir)
       snprintf(retro_system_directory, sizeof(retro_system_directory), "%s", dir);

#ifdef FRONTEND_SUPPORTS_RGB565
    enum retro_pixel_format rgb565 = RETRO_PIXEL_FORMAT_RGB565;
    if (environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &rgb565) && log_cb)
        log_cb(RETRO_LOG_INFO, "Frontend supports RGB565 - will use that instead of XRGB1555.\n");
#else
    enum retro_pixel_format rgb8888 = RETRO_PIXEL_FORMAT_XRGB8888;
    if (environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &rgb8888) && log_cb)
        log_cb(RETRO_LOG_INFO, "Frontend supports XRGB8888 - will use that instead of XRGB1555.\n");
#endif
}

static const char *gbGetCartridgeType(void)
{
    const char *type = "Unknown";

    switch (gbRom[0x147]) {
    case 0x00:
        type = "ROM";
        break;
    case 0x01:
        type = "ROM+MBC1";
        break;
    case 0x02:
        type = "ROM+MBC1+RAM";
        break;
    case 0x03:
        type = "ROM+MBC1+RAM+BATTERY";
        break;
    case 0x05:
        type = "ROM+MBC2";
        break;
    case 0x06:
        type = "ROM+MBC2+BATTERY";
        break;
    case 0x0b:
        type = "ROM+MMM01";
        break;
    case 0x0c:
        type = "ROM+MMM01+RAM";
        break;
    case 0x0d:
        type = "ROM+MMM01+RAM+BATTERY";
        break;
    case 0x0f:
        type = "ROM+MBC3+TIMER+BATTERY";
        break;
    case 0x10:
        type = "ROM+MBC3+TIMER+RAM+BATTERY";
        break;
    case 0x11:
        type = "ROM+MBC3";
        break;
    case 0x12:
        type = "ROM+MBC3+RAM";
        break;
    case 0x13:
        type = "ROM+MBC3+RAM+BATTERY";
        break;
    case 0x19:
        type = "ROM+MBC5";
        break;
    case 0x1a:
        type = "ROM+MBC5+RAM";
        break;
    case 0x1b:
        type = "ROM+MBC5+RAM+BATTERY";
        break;
    case 0x1c:
        type = "ROM+MBC5+RUMBLE";
        break;
    case 0x1d:
        type = "ROM+MBC5+RUMBLE+RAM";
        break;
    case 0x1e:
        type = "ROM+MBC5+RUMBLE+RAM+BATTERY";
        break;
    case 0x22:
        type = "ROM+MBC7+BATTERY";
        break;
    case 0x55:
        type = "GameGenie";
        break;
    case 0x56:
        type = "GameShark V3.0";
        break;
    case 0xfc:
        type = "ROM+POCKET CAMERA";
        break;
    case 0xfd:
        type = "ROM+BANDAI TAMA5";
        break;
    case 0xfe:
        type = "ROM+HuC-3";
        break;
    case 0xff:
        type = "ROM+HuC-1";
        break;
    }

    return (type);
}

static const char *gbGetSaveRamSize(void)
{
    const char *type = "Unknown";

    switch (gbRom[0x149]) {
    case 0:
        type = "None";
        break;
    case 1:
        type = "2K";
        break;
    case 2:
        type = "8K";
        break;
    case 3:
        type = "32K";
        break;
    case 4:
        type = "128K";
        break;
    case 5:
        type = "64K";
        break;
    }

    return (type);
}

typedef struct {
    char romtitle[256];
    char romid[5];
    int saveSize; // also can override eeprom size
    int saveType;  // 0auto 1eeprom 2sram 3flash 4sensor+eeprom 5none
    int rtcEnabled;
    int mirroringEnabled;
    int useBios;
} ini_t;

static const ini_t gbaover[512] = {
    #include "gba-over.inc"
};

static int romSize = 0;

static void load_image_preferences(void)
{
    const char *savetype[] = {
        "AUTO",
        "EEPROM",
        "SRAM",
        "FLASH",
        "SENSOR + EEPROM",
        "NONE"
    };

    char buffer[5];

    buffer[0] = rom[0xac];
    buffer[1] = rom[0xad];
    buffer[2] = rom[0xae];
    buffer[3] = rom[0xaf];
    buffer[4] = 0;

    cpuSaveType = 0;
    flashSize = 0x10000;
    eepromSize = 512;
    rtcEnabled = false;
    mirroringEnable = false;

    log("GameID in ROM is: %s\n", buffer);

    bool found = false;
    int found_no = 0;

    for (int i = 0; i < 512; i++) {
        if (!strcmp(gbaover[i].romid, buffer)) {
            found = true;
            found_no = i;
            break;
        }
    }

    if (found) {
        log("Found ROM in vba-over list.\n");
        log("Name            : %s\n", gbaover[found_no].romtitle);

        cpuSaveType = gbaover[found_no].saveType;

        if (gbaover[found_no].saveSize != 0) {
            unsigned size = gbaover[found_no].saveSize;
            if ((cpuSaveType == 3) && ((size == 65536) || (size == 131072)))
                flashSize = size;
            else if ((cpuSaveType == 1) && (size == 8192))
                eepromSize = 0x2000;
        }

        rtcEnabled = gbaover[found_no].rtcEnabled;

        mirroringEnable = gbaover[found_no].mirroringEnabled;
    }

    if (!cpuSaveType) {
        utilGBAFindSave(romSize);
    }

    log("romSize         : %dKB)\n", (romSize + 1023) / 1024);
    log("has RTC         : %s.\n", rtcEnabled ? "Yes" : "No");
    log("cpuSaveType     : %s.\n", savetype[cpuSaveType]);
    if (cpuSaveType == 3)
        log("flashSize       : %d.\n", flashSize);
    if (cpuSaveType == 1)
        log("eepromSize      : %d.\n", eepromSize);
    log("mirroringEnable : %d.\n", mirroringEnable);
}

static void update_colormaps(void)
{
#ifdef FRONTEND_SUPPORTS_RGB565
    systemColorDepth = 16;
    systemRedShift = 11;
    systemGreenShift = 6;
    systemBlueShift = 0;
#else
    systemColorDepth = 32;
    systemRedShift = 19;
    systemGreenShift = 11;
    systemBlueShift = 3;
#endif

    utilUpdateSystemColorMaps(false);

    log("Color Depth = %d\n", systemColorDepth);
}


#ifdef _WIN32
static const char SLASH = '\\';
#else
static const char SLASH = '/';
#endif

static void gba_init(void)
{
    log("Loading VBA-M Core (GBA)...\n");

    load_image_preferences();

    saveType = cpuSaveType;

    if (flashSize == 0x10000 || flashSize == 0x20000)
        flashSetSize(flashSize);

    rtcEnable(rtcEnabled);
    rtcEnableRumble(!rtcEnabled);

    doMirroring(mirroringEnable);
    soundSetSampleRate(SampleRate);

    if (usebios) {
        snprintf(biosfile, sizeof(biosfile), "%s%c%s", retro_system_directory, SLASH, "gba_bios.bin");
        log("Loading bios: %s\n", biosfile);
    }
    CPUInit(biosfile, usebios);

    width = GBAWidth;
    height = GBAHeight;

    // CPUReset() will reset eepromSize to 512.
    // Save current eepromSize override then restore after CPUReset()
    int tmp = eepromSize;
    CPUReset();
    eepromSize = tmp;
}

static void gb_init(void)
{
    const char *biosname[] = {"gb_bios.bin", "gbc_bios.bin"};

    log("Loading VBA-M Core (GB/GBC)...\n");

    gbGetHardwareType();

    if (usebios) {
        snprintf(biosfile, sizeof(biosfile), "%s%c%s",
            retro_system_directory, SLASH, biosname[gbCgbMode]);
        log("Loading bios: %s\n", biosfile);
    }

    gbCPUInit(biosfile, usebios);

    if (gbBorderOn) {
        width = gbBorderLineSkip = SGBWidth;
        height = SGBHeight;
        gbBorderColumnSkip = (SGBWidth - GBWidth) >> 1;
        gbBorderRowSkip = (SGBHeight - GBHeight) >> 1;
    } else {
        width = gbBorderLineSkip = GBWidth;
        height = GBHeight;
        gbBorderColumnSkip = gbBorderRowSkip = 0;
    }

    gbSoundSetSampleRate(SampleRate);
    gbSoundSetDeclicking(1);

    gbReset(); // also resets sound;

    // VBA-M always updates time based on current time and not in-game time.
    // No need to add RTC data to RETRO_MEMORY_RTC, so its safe to place this here.
    gbUpdateRTC();

    log("Rom size       : %02x (%dK)\n", gbRom[0x148], (romSize + 1023) / 1024);
    log("Cartridge type : %02x (%s)\n", gbRom[0x147], gbGetCartridgeType());
    log("Ram size       : %02x (%s)\n", gbRom[0x149], gbGetSaveRamSize());

    int i = 0;
    uint8_t crc = 25;

    for (i = 0x134; i < 0x14d; i++)
        crc += gbRom[i];
    crc = 256 - crc;
    log("CRC            : %02x (%02x)\n", crc, gbRom[0x14d]);

    uint16_t crc16 = 0;

    for (i = 0; i < gbRomSize; i++)
        crc16 += gbRom[i];

    crc16 -= gbRom[0x14e] + gbRom[0x14f];
    log("Checksum       : %04x (%04x)\n", crc16, gbRom[0x14e] * 256 + gbRom[0x14f]);

    if (gb_hasbattery)
        log(": Game supports battery save ram.\n");
    if (gbRom[0x143] == 0xc0)
        log(": Game works on CGB only\n");
    else if (gbRom[0x143] == 0x80)
        log(": Game supports GBC functions, GB compatible.\n");
    if (gbRom[0x146] == 0x03)
        log(": Game supports SGB functions\n");
}

static void gba_soundchanged(void)
{
    soundInterpolation = sndInterpolation;
    soundFiltering     = sndFiltering;
}

void retro_deinit(void)
{
    emulating = 0;
    core->emuCleanUp();
    soundShutdown();
}

void retro_reset(void)
{
    // save current eepromSize
    int tmp = eepromSize;
    core->emuReset();
    eepromSize = tmp;
}

#define MAX_BUTTONS 10
static const unsigned binds[4][MAX_BUTTONS] = {
    {  0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, // placeholder for no input
    {
        RETRO_DEVICE_ID_JOYPAD_A,
        RETRO_DEVICE_ID_JOYPAD_B,
        RETRO_DEVICE_ID_JOYPAD_SELECT,
        RETRO_DEVICE_ID_JOYPAD_START,
        RETRO_DEVICE_ID_JOYPAD_RIGHT,
        RETRO_DEVICE_ID_JOYPAD_LEFT,
        RETRO_DEVICE_ID_JOYPAD_UP,
        RETRO_DEVICE_ID_JOYPAD_DOWN,
        RETRO_DEVICE_ID_JOYPAD_R,
        RETRO_DEVICE_ID_JOYPAD_L
    },
    {
        RETRO_DEVICE_ID_JOYPAD_B,
        RETRO_DEVICE_ID_JOYPAD_Y,
        RETRO_DEVICE_ID_JOYPAD_SELECT,
        RETRO_DEVICE_ID_JOYPAD_START,
        RETRO_DEVICE_ID_JOYPAD_RIGHT,
        RETRO_DEVICE_ID_JOYPAD_LEFT,
        RETRO_DEVICE_ID_JOYPAD_UP,
        RETRO_DEVICE_ID_JOYPAD_DOWN,
        RETRO_DEVICE_ID_JOYPAD_R,
        RETRO_DEVICE_ID_JOYPAD_L
    },
    {
        RETRO_DEVICE_ID_JOYPAD_B,
        RETRO_DEVICE_ID_JOYPAD_A,
        RETRO_DEVICE_ID_JOYPAD_SELECT,
        RETRO_DEVICE_ID_JOYPAD_START,
        RETRO_DEVICE_ID_JOYPAD_RIGHT,
        RETRO_DEVICE_ID_JOYPAD_LEFT,
        RETRO_DEVICE_ID_JOYPAD_UP,
        RETRO_DEVICE_ID_JOYPAD_DOWN,
        RETRO_DEVICE_ID_JOYPAD_R,
        RETRO_DEVICE_ID_JOYPAD_L
    }
};

static void systemGbBorderOff(void);
static void systemUpdateSolarSensor(int level);
static uint8_t sensorDarkness = 0xE8;
static uint8_t sensorDarknessLevel = 0; // so we can adjust sensor from gamepad

static void update_variables(void)
{
    bool sound_changed = false;
    char key[256];
    struct retro_variable var;
    var.key = key;

    int disabled_layers=0;

    strcpy(key, "vbam_layer_x");
    for (int i = 0; i < 8; i++) {
        key[strlen("vbam_layer_")] = '1' + i;
        var.value = NULL;
        if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value && var.value[0] == 'd')
            disabled_layers |= 0x100 << i;
    }

    layerSettings = 0xFF00 ^ disabled_layers;
    layerEnable = DISPCNT & layerSettings;
    CPUUpdateRenderBuffers(false);

    int sound_enabled = 0x30F;
    strcpy(key, "vbam_sound_x");
    for (unsigned i = 0; i < 6; i++) {
        key[strlen("vbam_sound_")] = '1' + i;
        var.value = NULL;
        if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value && var.value[0] == 'd') {
            unsigned which = (i < 4) ? (1 << i) : (0x100 << (i - 4));
            sound_enabled &= ~(which);
        }
    }

    if (soundGetEnable() != sound_enabled)
        soundSetEnable(sound_enabled & 0x30F);

    var.key = "vbam_soundinterpolation";
    var.value = NULL;

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
        bool newval = (strcmp(var.value, "enabled") == 0);
        if (sndInterpolation != newval) {
            sndInterpolation = newval;
            sound_changed = true;
        }
    }

    var.key = "vbam_soundfiltering";
    var.value = NULL;

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
        float newval = atof(var.value) * 0.1f;
        if (sndFiltering != newval) {
            sndFiltering = newval;
            sound_changed = true;
        }
    }

    if (sound_changed) {
        //Update interpolation and filtering values
        gba_soundchanged();
    }

    var.key = "vbam_usebios";
    var.value = NULL;

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
        bool newval = (strcmp(var.value, "enabled") == 0);
        usebios = newval;
    }

    var.key = "vbam_solarsensor";
    var.value = NULL;

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
        sensorDarknessLevel = atoi(var.value);
        systemUpdateSolarSensor(sensorDarknessLevel);
    }

    var.key = "vbam_showborders";
    var.value = NULL;

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
        int oldval = (gbBorderOn << 1) | gbBorderAutomatic;
        if (strcmp(var.value, "auto") == 0) {
            gbBorderOn = 0;
            gbBorderAutomatic = 1;
        }
        else if (strcmp(var.value, "enabled") == 0) {
            gbBorderAutomatic = 0;
            gbBorderOn = 1;
        }
        else { // disabled
            gbBorderOn = 0;
            gbBorderAutomatic = 0;
        }

        if ((type == IMAGE_GB) && (oldval != ((gbBorderOn << 1) | gbBorderAutomatic))) {
            if (gbBorderOn) {
                systemGbBorderOn();
                gbSgbRenderBorder();
            }
            else
                systemGbBorderOff();
        }
    }

    var.key = "vbam_gbHardware";
    var.value = NULL;

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
        if (strcmp(var.value, "Automatic") == 0)
            gbEmulatorType = 0;
        else if (strcmp(var.value, "Game Boy Color") == 0)
            gbEmulatorType = 1;
        else if (strcmp(var.value, "Super Game Boy") == 0)
            gbEmulatorType = 2;
        else if (strcmp(var.value, "Game Boy") == 0)
            gbEmulatorType = 3;
        else if (strcmp(var.value, "Game Boy Advance") == 0)
            gbEmulatorType = 4;
        else if (strcmp(var.value, "Super Game Boy 2") == 0)
            gbEmulatorType = 5;
    }
}

static unsigned has_frame;

void retro_run(void)
{
    bool updated = false;
    static bool buttonpressed = false;
    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated)
        update_variables();

    poll_cb();

    // Update solar sensor level by gamepad buttons, default L2/R2
    if (buttonpressed) {
        buttonpressed = input_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2) ||
            input_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2);
    } else {
        if (input_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2)) {
            sensorDarknessLevel++;
            if (sensorDarknessLevel > 10)
                sensorDarknessLevel = 10;
            systemUpdateSolarSensor(sensorDarknessLevel);
            buttonpressed = true;
        } else if (input_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2)) {
            if (sensorDarknessLevel)
                sensorDarknessLevel--;
            systemUpdateSolarSensor(sensorDarknessLevel);
            buttonpressed = true;
        }
    } // end of solar sensor update

    has_frame = 0;

    do {
        core->emuMain(core->emuCount);
    } while (!has_frame);
}

static unsigned serialize_size = 0;

size_t retro_serialize_size(void)
{
    return serialize_size;
}

bool retro_serialize(void* data, size_t size)
{
    if (size == serialize_size)
        return core->emuWriteState((uint8_t*)data, size);
    return false;
}

bool retro_unserialize(const void* data, size_t size)
{
    if (size == serialize_size)
        return core->emuReadState((uint8_t*)data, size);
    return false;
}

void retro_cheat_reset(void)
{
    cheatsEnabled = 1;
    if (type == IMAGE_GBA)
		cheatsDeleteAll(false);
	else if (type == IMAGE_GB)
        gbCheatRemoveAll();
}

void retro_cheat_set(unsigned index, bool enabled, const char* code)
{
    /*
    const char *begin, *c;

    begin = c = code;

   if (!code)
      return;

   do {
      if (*c != '+' && *c != '\0')
         continue;

      char buf[32] = {0};
      int len = c - begin;
      int i;

      // make sure it's using uppercase letters
      for (i = 0; i < len; i++)
         buf[i] = toupper(begin[i]);
      buf[i] = 0;

      begin = ++c;

      if (len == 16)
         cheatsAddGSACode(buf, "", false);
      else {
         char *space = strrchr(buf, ' ');
         if (space != NULL) {
            if ((buf + len - space - 1) == 4)
               cheatsAddCBACode(buf, "");
            else {
               memmove(space, space+1, strlen(space+1)+1);
               cheatsAddGSACode(buf, "", true);
            }
         } else if (log_cb)
            log_cb(RETRO_LOG_ERROR, "[VBA] Invalid cheat code '%s'\n", buf);
      }

   } while (*c++);
   */
   
    std::string codeLine = code;
    std::string name = "cheat_" + index;
    int matchLength = 0;
    std::vector<std::string> codeParts;
    int cursor;
   
    if (type == IMAGE_GBA) {
        //Break the code into Parts
        for (cursor = 0;; cursor++) {
            if  (ISHEXDEC)
                matchLength++;
            else {
                if (matchLength) {
                    if (matchLength > 8) {
                    codeParts.push_back(codeLine.substr(cursor - matchLength, 8));
                    codeParts.push_back(codeLine.substr(cursor - matchLength + 8, matchLength - 8));
                    } else
                        codeParts.push_back(codeLine.substr(cursor - matchLength, matchLength));
                    matchLength = 0;
                }
            }
            if (!codeLine[cursor])
                break;
        }

        //Add to core
        for (cursor = 0; cursor < codeParts.size(); cursor += 2) {
            std::string codeString;
            codeString += codeParts[cursor];

            if (codeParts[cursor + 1].length() == 8) {
                codeString += codeParts[cursor + 1];
                cheatsAddGSACode(codeString.c_str(), name.c_str(), true);
            } else if (codeParts[cursor + 1].length() == 4) {
                codeString += " ";
                codeString += codeParts[cursor + 1];
                cheatsAddCBACode(codeString.c_str(), name.c_str());
            } else {
                codeString += " ";
                codeString += codeParts[cursor + 1];
                log_cb(RETRO_LOG_ERROR, "Invalid cheat code '%s'\n", codeString.c_str());
            }
            log_cb(RETRO_LOG_INFO, "Cheat code added: '%s'\n", codeString.c_str());
        }
    } else if (type == IMAGE_GB) {
        if (codeLine.find("-") != std::string::npos) {
            if (gbAddGgCheat(code, ""))
                log_cb(RETRO_LOG_INFO, "Cheat code added: '%s'\n", codeLine.c_str());
        } else {
            if (gbAddGsCheat(code, ""))
                log_cb(RETRO_LOG_INFO, "Cheat code added: '%s'\n", codeLine.c_str());
        }
    }
}

static void update_input_descriptors(void)
{
   struct retro_input_descriptor input_gba[] = {
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,  "D-Pad Left" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,    "D-Pad Up" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,  "D-Pad Down" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "D-Pad Right" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B,     "B" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A,     "A" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L,     "L" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R,     "R" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "Select" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "Start" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2,    "Solar Sensor (Darker)" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2,    "Solar Sensor (Lighter)" },

      { 0, 0, 0, 0, NULL },
   };

   struct retro_input_descriptor input_gba_alt1[] = {
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,  "D-Pad Left" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,    "D-Pad Up" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,  "D-Pad Down" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "D-Pad Right" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y,     "B" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B,     "A" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L,     "L" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R,     "R" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "Select" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "Start" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2,    "Solar Sensor (Darker)" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2,    "Solar Sensor (Lighter)" },

      { 0, 0, 0, 0, NULL },
   };

   struct retro_input_descriptor input_gba_alt2[] = {
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,  "D-Pad Left" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,    "D-Pad Up" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,  "D-Pad Down" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "D-Pad Right" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A,     "B" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B,     "A" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L,     "L" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R,     "R" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "Select" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "Start" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2,    "Solar Sensor (Darker)" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2,    "Solar Sensor (Lighter)" },

      { 0, 0, 0, 0, NULL },
   };

   switch (retropad_layout)
   {
   case 1:
      environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, input_gba);
      break;
   case 2:
      environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, input_gba_alt1);
      break;
   case 3:
      environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, input_gba_alt2);
      break;
   }
}

bool retro_load_game(const struct retro_game_info *game)
{
   update_variables();
   update_input_descriptors();

   type = utilFindType((const char *)game->path);

   if (type == IMAGE_UNKNOWN) {
      log("Unsupported image %s!\n", game->path);
      return false;
   }

   update_colormaps();
   soundInit();

   if (type == IMAGE_GBA) {
      romSize = CPULoadRomData((const char*)game->data, game->size);

      if (!romSize)
         return false;

      core = &GBASystem;

      gba_init();

      struct retro_memory_descriptor desc[11];
      memset(desc, 0, sizeof(desc));

      desc[0].start=0x03000000; desc[0].select=0xFF000000; desc[0].len=0x8000;    desc[0].ptr=internalRAM;//fast WRAM
      desc[1].start=0x02000000; desc[1].select=0xFF000000; desc[1].len=0x40000;   desc[1].ptr=workRAM;//slow WRAM
      /* TODO: if SRAM is flash, use start=0 addrspace="S" instead */
      desc[2].start=0x0E000000; desc[2].select=0;          desc[2].len=flashSize; desc[2].ptr=flashSaveMemory;//SRAM
      desc[3].start=0x08000000; desc[3].select=0;          desc[3].len=romSize;   desc[3].ptr=rom;//ROM
      desc[3].flags=RETRO_MEMDESC_CONST;
      desc[4].start=0x0A000000; desc[4].select=0;          desc[4].len=romSize;   desc[4].ptr=rom;//ROM mirror 1
      desc[4].flags=RETRO_MEMDESC_CONST;
      desc[5].start=0x0C000000; desc[5].select=0;          desc[5].len=romSize;   desc[5].ptr=rom;//ROM mirror 2
      desc[5].flags=RETRO_MEMDESC_CONST;
      desc[6].start=0x00000000; desc[6].select=0;          desc[6].len=0x4000;    desc[6].ptr=bios;//BIOS
      desc[6].flags=RETRO_MEMDESC_CONST;
      desc[7].start=0x06000000; desc[7].select=0xFF000000; desc[7].len=0x18000;   desc[7].ptr=vram;//VRAM
      desc[8].start=0x05000000; desc[8].select=0xFF000000; desc[8].len=0x400;     desc[8].ptr=paletteRAM;//palettes
      desc[9].start=0x07000000; desc[9].select=0xFF000000; desc[9].len=0x400;     desc[9].ptr=oam;//OAM
      desc[10].start=0x04000000;desc[10].select=0;         desc[10].len=0x400;    desc[10].ptr=ioMem;//bunch of registers

      struct retro_memory_map retromap = {
          desc,
          sizeof(desc)/sizeof(desc[0])
      };

      environ_cb(RETRO_ENVIRONMENT_SET_MEMORY_MAPS, &retromap);
   }

   else if (type == IMAGE_GB) {
      if (!gbLoadRomData((const char *)game->data, game->size))
         return false;

      romSize = game->size;

      core = &GBSystem;

      gb_init();

      struct retro_memory_descriptor desc[8];
      memset(desc, 0, sizeof(desc));

      // workram bank 0 0xc000-0xcfff / bank 1-7 0xd000-0xdfff
      desc[0].ptr   = &gbMemory[0xc000];
      desc[0].start = 0xc000;
      desc[0].len   = 0x1000;

      // workram bank 1-7 switchable (switchable-CGB only)
      desc[1].ptr   = gbCgbMode ? &gbWram[0x1000] : &gbMemory[0xd000];
      desc[1].start = 0xd000;
      desc[1].len   = 0x1000;

      // high ram area
      desc[2].ptr   = &gbMemory[0xff80];
      desc[2].start = 0xff80;
      desc[2].len   = 0x0080;

      // save ram or cartridge ram area
      desc[3].ptr   = gbRamSize ? &gbRam[0] : 0;
      desc[3].start = 0xa000;
      desc[3].len   = gbRamSize ? gbRamSize : 0;

      // vram (chr ram, gb map data 1 - 2)
      desc[4].ptr   = gbCgbMode ? &gbVram[0] : &gbMemory[0x8000];
      desc[4].start = 0x8000;
      desc[4].len   = 0x2000;

      // oam
      desc[5].ptr   = &gbMemory[0xfe00];
      desc[5].start = 0xfe00;
      desc[5].len   = 0x00a0;

      // http://gameboy.mongenel.com/dmg/asmmemmap.html
      // $4000-$7FFF 	Cartridge ROM - Switchable Banks 1-xx
      // $0150-$3FFF 	Cartridge ROM - Bank 0 (fixed)
      // $0100-$014F 	Cartridge Header Area
      // $0000-$00FF 	Restart and Interrupt Vectors
      desc[6].ptr   = gbMemoryMap[0x00];
      desc[6].start = 0x0000;
      desc[6].len   = 0x4000;
      desc[6].flags = RETRO_MEMDESC_CONST;

      desc[7].ptr   = gbMemoryMap[0x04];
      desc[7].start = 0x4000;
      desc[7].len   = 0x4000;
      desc[7].flags = RETRO_MEMDESC_CONST;

      struct retro_memory_map retromap = {
          desc,
          sizeof(desc) / sizeof(desc[0])
      };

      environ_cb(RETRO_ENVIRONMENT_SET_MEMORY_MAPS, &retromap);
   }

   if (!core)
       return false;

   bool yes = true;
   environ_cb(RETRO_ENVIRONMENT_SET_SUPPORT_ACHIEVEMENTS, &yes);

   uint8_t* state_buf = (uint8_t*)malloc(2000000);
   serialize_size = core->emuWriteState(state_buf, 2000000);
   free(state_buf);

   emulating = 1;

   return emulating;
}

bool retro_load_game_special(unsigned,  const struct retro_game_info *, size_t)
{
    return false;
}

void retro_unload_game(void)
{
}

unsigned retro_get_region(void)
{
    return RETRO_REGION_NTSC;
}

void systemOnWriteDataToSoundBuffer(const uint16_t*, int)
{
}

void systemOnSoundShutdown(void)
{
}

bool systemCanChangeSoundQuality(void)
{
    return true;
}

void systemDrawScreen(void)
{
    unsigned pitch = width * (systemColorDepth >> 3);
    video_cb(pix, width, height, pitch);
}

void systemFrame(void)
{
    has_frame = 1;
}

void systemGbBorderOn(void)
{
    bool changed = ((width != SGBWidth) || (height != SGBHeight));
    width = gbBorderLineSkip = SGBWidth;
    height = SGBHeight;
    gbBorderColumnSkip = (SGBWidth - GBWidth) >> 1;
    gbBorderRowSkip = (SGBHeight - GBHeight) >> 1;

    struct retro_system_av_info avinfo;
    retro_get_system_av_info(&avinfo);

    if (!changed)
        environ_cb(RETRO_ENVIRONMENT_SET_GEOMETRY, &avinfo);
    else
        environ_cb(RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO, &avinfo);
}

static void systemGbBorderOff(void)
{
    bool changed = ((width != GBWidth) || (height != GBHeight));
    width = gbBorderLineSkip = GBWidth;
    height = GBHeight;
    gbBorderColumnSkip = gbBorderRowSkip = 0;

    struct retro_system_av_info avinfo;
    retro_get_system_av_info(&avinfo);

    if (!changed)
        environ_cb(RETRO_ENVIRONMENT_SET_GEOMETRY, &avinfo);
    else
        environ_cb(RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO, &avinfo);
}

void systemMessage(const char* fmt, ...)
{
    char buffer[256];
    va_list ap;
    va_start(ap, fmt);
    vsprintf(buffer, fmt, ap);
    if (log_cb)
        log_cb(RETRO_LOG_INFO, "%s\n", buffer);
    va_end(ap);
}

void systemMessage(int, const char* fmt, ...)
{
    char buffer[256];
    va_list ap;
    va_start(ap, fmt);
    vsprintf(buffer, fmt, ap);
    if (log_cb)
        log_cb(RETRO_LOG_INFO, "%s\n", buffer);
    va_end(ap);
}

int systemGetSensorX(void)
{
    return 0;
}

int systemGetSensorY(void)
{
    return 0;
}

int systemGetSensorZ(void)
{
    return 0;
}

uint32_t systemReadJoypad(int which)
{
    if (which == -1)
        which = 0;

    uint32_t J = 0;

    if (retropad_layout)
        for (unsigned i = 0; i < MAX_BUTTONS; i++)
            J |= input_cb(which, RETRO_DEVICE_JOYPAD, 0, binds[retropad_layout][i]) << i;

    // Do not allow opposing directions
    if ((J & 0x30) == 0x30)
        J &= ~(0x30);
    else if ((J & 0xC0) == 0xC0)
        J &= ~(0xC0);

    return J;
}

static void systemUpdateSolarSensor(int v)
{
    int value = 0;
    switch (v) {
    case 1:  value = 0x06; break;
    case 2:  value = 0x0E; break;
    case 3:  value = 0x18; break;
    case 4:  value = 0x20; break;
    case 5:  value = 0x28; break;
    case 6:  value = 0x38; break;
    case 7:  value = 0x48; break;
    case 8:  value = 0x60; break;
    case 9:  value = 0x78; break;
    case 10: value = 0x98; break;
    default: break;
    }

    sensorDarkness = 0xE8 - value;
}

bool systemReadJoypads(void)
{
    return true;
}

void systemUpdateMotionSensor(void)
{
}

uint8_t systemGetSensorDarkness(void)
{
    return sensorDarkness;
}

void systemCartridgeRumble(bool)
{
}

bool systemPauseOnFrame(void)
{
    return false;
}

void systemGbPrint(uint8_t*, int, int, int, int)
{
}

void systemScreenCapture(int)
{
}

void systemScreenMessage(const char* msg)
{
    if (log_cb)
        log_cb(RETRO_LOG_INFO, "%s\n", msg);
}

void systemSetTitle(const char*)
{
}

void systemShowSpeed(int)
{
}

void system10Frames(int)
{
}

uint32_t systemGetClock(void)
{
    return 0;
}

SoundDriver* systemSoundInit(void)
{
    soundShutdown();
    return new SoundRetro();
}

void log(const char* defaultMsg, ...)
{
    va_list valist;
    char buf[2048];
    va_start(valist, defaultMsg);
    vsnprintf(buf, 2048, defaultMsg, valist);
    va_end(valist);

    if (log_cb)
        log_cb(RETRO_LOG_INFO, "%s", buf);
}
