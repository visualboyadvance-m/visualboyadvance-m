#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

#include "SoundRetro.h"
#include "libretro.h"
#include "libretro_core_options.h"
#include "scrc32.h"

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

#include "../filters/interframe.hpp"

#define FRAMERATE  (16777216.0 / 280896.0) // 59.73
#define SAMPLERATE 32768.0

#ifndef VBAM_VERSION
#define VBAM_VERSION "2.1.3"
#endif

static retro_log_printf_t log_cb;
static retro_video_refresh_t video_cb;
static retro_input_poll_t poll_cb;
static retro_input_state_t input_cb;
static retro_environment_t environ_cb;
static retro_set_rumble_state_t rumble_cb;
static retro_audio_sample_t audio_cb;
retro_audio_sample_batch_t audio_batch_cb;

static char retro_system_directory[2048];
static char biosfile[4096];
static bool can_dupe = false;

// core options
static bool option_sndInterpolation = true;
static bool option_useBios = false;
static bool option_colorizerHack = false;
static bool option_forceRTCenable = false;
static double option_sndFiltering = 0.5;
static unsigned option_gbPalette = 0;
static bool option_lcdfilter = false;

// filters
static IFBFilterFunc ifb_filter_func = NULL;

static unsigned retropad_device[4] = {0};
static unsigned systemWidth = gbaWidth;
static unsigned systemHeight = gbaHeight;
static EmulatedSystem* core = NULL;
static IMAGE_TYPE type = IMAGE_UNKNOWN;
static bool libretro_supports_bitmasks = false;

// global vars
uint16_t systemColorMap16[0x10000];
uint32_t systemColorMap32[0x10000];
int RGB_LOW_BITS_MASK = 0x821; // used for 16bit inter-frame filters
int systemRedShift = 0;
int systemBlueShift = 0;
int systemGreenShift = 0;
int systemColorDepth = 32;
int systemVerbose = 0;
int systemFrameSkip = 0;
int systemSaveUpdateCounter = SYSTEM_SAVE_NOT_UPDATED;
int emulating = 0;

#ifdef BKPT_SUPPORT
void (*dbgOutput)(const char* s, uint32_t addr);
void (*dbgSignal)(int sig, int number);
#endif

#define GS555(x) (x | (x << 5) | (x << 10))
uint16_t systemGbPalette[24] = {
    GS555(0x1f), GS555(0x15), GS555(0x0c), 0,
    GS555(0x1f), GS555(0x15), GS555(0x0c), 0,
    GS555(0x1f), GS555(0x15), GS555(0x0c), 0,
    GS555(0x1f), GS555(0x15), GS555(0x0c), 0,
    GS555(0x1f), GS555(0x15), GS555(0x0c), 0,
    GS555(0x1f), GS555(0x15), GS555(0x0c), 0
};

struct palettes_t {
    char        name[40];
    uint16_t    data[8];
};

static struct palettes_t defaultGBPalettes[] = {
    {
        "Standard",
        { 0x7FFF, 0x56B5, 0x318C, 0x0000, 0x7FFF, 0x56B5, 0x318C, 0x0000 },
    },
    {
        "Blue Sea",
        { 0x6200, 0x7E10, 0x7C10, 0x5000, 0x6200, 0x7E10, 0x7C10, 0x5000 },
    },
    {
        "Dark Night",
        { 0x4008, 0x4000, 0x2000, 0x2008, 0x4008, 0x4000, 0x2000, 0x2008 },
    },
    {
        "Green Forest",
        { 0x43F0, 0x03E0, 0x4200, 0x2200, 0x43F0, 0x03E0, 0x4200, 0x2200 },
    },
    {
        "Hot Desert",
        { 0x43FF, 0x03FF, 0x221F, 0x021F, 0x43FF, 0x03FF, 0x221F, 0x021F },
    },
    {
        "Pink Dreams",
        { 0x621F, 0x7E1F, 0x7C1F, 0x2010, 0x621F, 0x7E1F, 0x7C1F, 0x2010 },
    },
    {
        "Weird Colors",
        { 0x621F, 0x401F, 0x001F, 0x2010, 0x621F, 0x401F, 0x001F, 0x2010 },
    },
    {
        "Real GB Colors",
        { 0x1B8E, 0x02C0, 0x0DA0, 0x1140, 0x1B8E, 0x02C0, 0x0DA0, 0x1140 },
    },
    {
        "Real GB on GBASP Colors",
        { 0x7BDE, 0x5778, 0x5640, 0x0000, 0x7BDE, 0x529C, 0x2990, 0x0000 },
    }
};

static void set_gbPalette(void)
{
    if (type != IMAGE_GB)
        return;

    if (gbCgbMode || gbSgbMode)
        return;

    const uint16_t *pal = defaultGBPalettes[option_gbPalette].data;
    for (int i = 0; i < 8; i++) {
        uint16_t val = pal[i];
        gbPalette[i] = val;
    }
}

static void* gb_rtcdata_prt(void)
{
    switch (gbRomType) {
    case 0x0f:
    case 0x10: // MBC3 + extended
        return &gbDataMBC3.mapperSeconds;
    case 0xfd: // TAMA5 + extended
        return &gbDataTAMA5.mapperSeconds;
    case 0xfe: // HuC3 + Clock
        return &gbRTCHuC3.mapperLastTime;
    }
    return NULL;
}

static size_t gb_rtcdata_size(void)
{
    switch (gbRomType) {
    case 0x0f:
    case 0x10: // MBC3 + extended
        return MBC3_RTC_DATA_SIZE;
    case 0xfd: // TAMA5 + extended
        return TAMA5_RTC_DATA_SIZE;
    case 0xfe: // HuC3 + Clock
        return sizeof(gbRTCHuC3.mapperLastTime);
    }
    return 0;
}

static void gbInitRTC(void)
{
    struct tm* lt;
    time_t rawtime;
    time(&rawtime);
    lt = localtime(&rawtime);

    switch (gbRomType) {
    case 0x0f:
    case 0x10:
        gbDataMBC3.mapperSeconds = lt->tm_sec;
        gbDataMBC3.mapperMinutes = lt->tm_min;
        gbDataMBC3.mapperHours = lt->tm_hour;
        gbDataMBC3.mapperDays = lt->tm_yday & 255;
        gbDataMBC3.mapperControl = (gbDataMBC3.mapperControl & 0xfe) | (lt->tm_yday > 255 ? 1 : 0);
        gbDataMBC3.mapperLastTime = rawtime;
        break;
    case 0xfd: {
        uint8_t gbDaysinMonth[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
        int days = lt->tm_yday + 365 * 3;
        gbDataTAMA5.mapperSeconds = lt->tm_sec;
        gbDataTAMA5.mapperMinutes = lt->tm_min;
        gbDataTAMA5.mapperHours = lt->tm_hour;
        gbDataTAMA5.mapperDays = 1;
        gbDataTAMA5.mapperMonths = 1;
        gbDataTAMA5.mapperYears = 1970;
        gbDataTAMA5.mapperLastTime = rawtime;
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
    case 0xfe:
        gbRTCHuC3.mapperLastTime = rawtime;
        break;
    }
}

static void SetGBBorder(unsigned val)
{
    struct retro_system_av_info avinfo;
    unsigned _changed = 0;

    switch (val) {
        case 0:
            _changed = ((systemWidth != gbWidth) || (systemHeight != gbHeight)) ? 1 : 0;
            systemWidth = gbBorderLineSkip = gbWidth;
            systemHeight = gbHeight;
            gbBorderColumnSkip = gbBorderRowSkip = 0;
            break;
        case 1:
            _changed = ((systemWidth != sgbWidth) || (systemHeight != sgbHeight)) ? 1 : 0;
            systemWidth = gbBorderLineSkip = sgbWidth;
            systemHeight = sgbHeight;
            gbBorderColumnSkip = (sgbWidth - gbWidth) >> 1;
            gbBorderRowSkip = (sgbHeight - gbHeight) >> 1;
            break;
    }

    retro_get_system_av_info(&avinfo);

    if (!_changed)
        environ_cb(RETRO_ENVIRONMENT_SET_GEOMETRY, &avinfo);
    else
        environ_cb(RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO, &avinfo);
}

void* retro_get_memory_data(unsigned id)
{
    void *data = NULL;

    switch (type) {
    case IMAGE_GBA:
        switch (id) {
        case RETRO_MEMORY_SAVE_RAM:
            if ((saveType == GBA_SAVE_EEPROM) | (saveType == GBA_SAVE_EEPROM_SENSOR))
                data =  eepromData;
            else if ((saveType == GBA_SAVE_SRAM) | (saveType == GBA_SAVE_FLASH))
                data = flashSaveMemory;
            break;
        case RETRO_MEMORY_SYSTEM_RAM:
            data = workRAM;
            break;
        case RETRO_MEMORY_VIDEO_RAM:
            data = vram;
            break;
        }
        break;

    case IMAGE_GB:
        switch (id) {
        case RETRO_MEMORY_SAVE_RAM:
            if (gbBattery)
                data = gbRam;
            break;
        case RETRO_MEMORY_SYSTEM_RAM:
            data = (gbCgbMode ? gbWram : (gbMemory + 0xC000));
            break;
        case RETRO_MEMORY_VIDEO_RAM:
            data = (gbCgbMode ? gbVram : (gbMemory + 0x8000));
            break;
        case RETRO_MEMORY_RTC:
            if (gbRTCPresent)
                data = gb_rtcdata_prt();
            break;
        }
        break;

    default: break;
    }
    return data;
}

size_t retro_get_memory_size(unsigned id)
{
    size_t size = 0;

    switch (type) {
    case IMAGE_GBA:
        switch (id) {
        case RETRO_MEMORY_SAVE_RAM:
            if ((saveType == GBA_SAVE_EEPROM) | (saveType == GBA_SAVE_EEPROM_SENSOR))
                size = eepromSize;
            else if (saveType == GBA_SAVE_FLASH)
                size = flashSize;
            else if (saveType == GBA_SAVE_SRAM)
                size = SIZE_SRAM;
            break;
        case RETRO_MEMORY_SYSTEM_RAM:
            size = SIZE_WRAM;
            break;
        case RETRO_MEMORY_VIDEO_RAM:
            size = SIZE_VRAM - 0x2000; // usuable vram is only 0x18000
            break;
        }
        break;

    case IMAGE_GB:
        switch (id) {
        case RETRO_MEMORY_SAVE_RAM:
            if (gbBattery)
                size = gbRamSize;
            break;
        case RETRO_MEMORY_SYSTEM_RAM:
            size = gbCgbMode ? 0x8000 : 0x2000;
            break;
        case RETRO_MEMORY_VIDEO_RAM:
            size = gbCgbMode ? 0x4000 : 0x2000;
            break;
        case RETRO_MEMORY_RTC:
            if (gbRTCPresent)
                size = gb_rtcdata_size();
            break;
        }
        break;

    default: break;
    }
    return size;
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
    audio_cb = cb;
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

static struct retro_input_descriptor input_gba[] = {
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,  "D-Pad Left" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP, "D-Pad Up" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,  "D-Pad Down" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "D-Pad Right" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B,  "B" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A,  "A" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L,  "L" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R,  "R" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y,  "Turbo B" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X,  "Turbo A" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "Select" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "Start" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2, "Solar Sensor (Darker)" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2, "Solar Sensor (Lighter)" },
    { 0, 0, 0, 0, NULL },
};

static struct retro_input_descriptor input_gb[] = {
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,  "D-Pad Left" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP, "D-Pad Up" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,  "D-Pad Down" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "D-Pad Right" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A,  "B" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B,  "A" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X,  "Turbo B" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y,  "Turbo A" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "Select" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "Start" },
    { 0, 0, 0, 0, NULL },
};

static struct retro_input_descriptor input_sgb[] = {
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,  "D-Pad Left" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP, "D-Pad Up" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,  "D-Pad Down" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "D-Pad Right" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A,  "B" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B,  "A" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X,  "Turbo B" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y,  "Turbo A" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "Select" },
    { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "Start" },
    { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,  "D-Pad Left" },
    { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP, "D-Pad Up" },
    { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,  "D-Pad Down" },
    { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "D-Pad Right" },
    { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A,  "B" },
    { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B,  "A" },
    { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X,  "Turbo B" },
    { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y,  "Turbo A" },
    { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "Select" },
    { 1, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "Start" },
    { 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,  "D-Pad Left" },
    { 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP, "D-Pad Up" },
    { 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,  "D-Pad Down" },
    { 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "D-Pad Right" },
    { 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A,  "B" },
    { 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B,  "A" },
    { 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X,  "Turbo B" },
    { 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y,  "Turbo A" },
    { 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "Select" },
    { 2, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "Start" },
    { 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,  "D-Pad Left" },
    { 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP, "D-Pad Up" },
    { 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,  "D-Pad Down" },
    { 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "D-Pad Right" },
    { 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A,  "B" },
    { 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B,  "A" },
    { 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X,  "Turbo B" },
    { 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y,  "Turbo A" },
    { 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "Select" },
    { 3, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "Start" },
    { 0, 0, 0, 0, NULL },
};

static const struct retro_controller_description port_gba[] = {
    { "GBA Joypad", RETRO_DEVICE_JOYPAD },
    { NULL, 0 },
};

static const struct retro_controller_description port_gb[] = {
    { "GB Joypad", RETRO_DEVICE_JOYPAD },
    { NULL, 0 },
};

static const struct retro_controller_info ports_gba[] = {
    { port_gba, 1},
    { NULL, 0 }
};

static const struct retro_controller_info ports_gb[] = {
    { port_gb, 1},
    { NULL, 0 }
};

static const struct retro_controller_info ports_sgb[] = {
    { port_gb, 1},
    { port_gb, 1},
    { port_gb, 1},
    { port_gb, 1},
    { NULL, 0 }
};

void retro_set_controller_port_device(unsigned port, unsigned device)
{
    if (port > 3) return;

    retropad_device[port] = device;
    log_cb(RETRO_LOG_INFO, "Controller %d device: %d\n", port + 1, device);
}

void retro_set_environment(retro_environment_t cb)
{
   environ_cb = cb;
   bool categoriesSupported;
   libretro_set_core_options(environ_cb, &categoriesSupported);
}

void retro_get_system_info(struct retro_system_info *info)
{
   info->need_fullpath = false;
   info->valid_extensions = "dmg|gb|gbc|cgb|sgb|gba";
#ifdef GIT_COMMIT
   info->library_version = VBAM_VERSION " " GIT_COMMIT;
#else
   info->library_version = VBAM_VERSION;
#endif
   info->library_name = "VBA-M";
   info->block_extract = false;
}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
   double aspect = (3.0f / 2.0f);
   unsigned maxWidth  = gbaWidth;
   unsigned maxHeight = gbaHeight;

   if (type == IMAGE_GB) {
      aspect = !gbBorderOn ? (10.0 / 9.0) : (8.0 / 7.0);
      maxWidth = !gbBorderOn ? gbWidth : sgbWidth;
      maxHeight = !gbBorderOn ? gbHeight : sgbHeight;
   }

   info->geometry.base_width = systemWidth;
   info->geometry.base_height = systemHeight;
   info->geometry.max_width = maxWidth;
   info->geometry.max_height = maxHeight;
   info->geometry.aspect_ratio = aspect;
   info->timing.fps = FRAMERATE;
   info->timing.sample_rate = SAMPLERATE;
}

void retro_init(void)
{
   struct retro_log_callback log;
   struct retro_rumble_interface rumble;

   environ_cb(RETRO_ENVIRONMENT_GET_CAN_DUPE, &can_dupe);
   if (environ_cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log))
      log_cb = log.log;
   else
      log_cb = NULL;

   const char* dir = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &dir) && dir)
       snprintf(retro_system_directory, sizeof(retro_system_directory), "%s", dir);

#ifdef FRONTEND_SUPPORTS_RGB565
    systemColorDepth = 16;
    systemRedShift = 11;
    systemGreenShift = 6;
    systemBlueShift = 0;
    enum retro_pixel_format rgb565 = RETRO_PIXEL_FORMAT_RGB565;
    if (environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &rgb565) && log_cb)
        log_cb(RETRO_LOG_INFO, "Frontend supports RGB565 - will use that instead of XRGB1555.\n");
#else
    systemColorDepth = 32;
    systemRedShift = 19;
    systemGreenShift = 11;
    systemBlueShift = 3;
    enum retro_pixel_format rgb8888 = RETRO_PIXEL_FORMAT_XRGB8888;
    if (environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &rgb8888) && log_cb)
        log_cb(RETRO_LOG_INFO, "Frontend supports XRGB8888 - will use that instead of XRGB1555.\n");
#endif

   bool yes = true;
   environ_cb(RETRO_ENVIRONMENT_SET_SUPPORT_ACHIEVEMENTS, &yes);

   if (environ_cb(RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE, &rumble))
      rumble_cb = rumble.set_rumble_state;
   else
      rumble_cb = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_INPUT_BITMASKS, NULL))
   {
      libretro_supports_bitmasks = true;
      log_cb(RETRO_LOG_INFO, "SET_SUPPORT_INPUT_BITMASK: yes\n");
   }
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

    bool found = false;
    bool hasRumble = false;
    char buffer[12 + 1];
    unsigned i = 0, found_no = 0;
    unsigned long romCrc32 = crc32(0, rom, romSize);

    cpuSaveType = GBA_SAVE_AUTO;
    flashSize = SIZE_FLASH512;
    eepromSize = SIZE_EEPROM_512;
    rtcEnabled = false;
    mirroringEnable = false;

    log("File CRC32      : 0x%08X\n", romCrc32);

    buffer[0] = 0;
    for (i = 0; i < 12; i++) {
        if (rom[0xa0 + i] == 0)
            break;
        buffer[i] = rom[0xa0 + i];
    }

    buffer[i] = 0;
    log("Game Title      : %s\n", buffer);

    buffer[0] = rom[0xac];
    buffer[1] = rom[0xad];
    buffer[2] = rom[0xae];
    buffer[3] = rom[0xaf];
    buffer[4] = 0;
    log("Game Code       : %s\n", buffer);

    for (i = 0; i < 512; i++) {
        if (!strcmp(gbaover[i].romid, buffer)) {
            found = true;
            found_no = i;
            break;
        }
    }

    if (found) {
        log("Name            : %s\n", gbaover[found_no].romtitle);

        rtcEnabled = gbaover[found_no].rtcEnabled;
        cpuSaveType = gbaover[found_no].saveType;

        unsigned size = gbaover[found_no].saveSize;
        if (cpuSaveType == GBA_SAVE_SRAM)
            flashSize = SIZE_SRAM;
        else if (cpuSaveType == GBA_SAVE_FLASH)
            flashSize = (size == SIZE_FLASH1M) ? SIZE_FLASH1M : SIZE_FLASH512;
        else if ((cpuSaveType == GBA_SAVE_EEPROM) || (cpuSaveType == GBA_SAVE_EEPROM_SENSOR))
            eepromSize = (size == SIZE_EEPROM_8K) ? SIZE_EEPROM_8K : SIZE_EEPROM_512;
    }

    // gameID that starts with 'F' are classic/famicom games
    mirroringEnable = (buffer[0] == 'F') ? true : false;

    if (!cpuSaveType)
        utilGBAFindSave(romSize);

    saveType = cpuSaveType;

    if (flashSize == SIZE_FLASH512 || flashSize == SIZE_FLASH1M)
        flashSetSize(flashSize);

    if (option_forceRTCenable)
        rtcEnabled = true;

    rtcEnable(rtcEnabled);

    // game code starting with 'R' or 'V' has rumble support
    if ((buffer[0] == 'R') || (buffer[0] == 'V'))
        hasRumble = true;

    rtcEnableRumble(!rtcEnabled && hasRumble);

    doMirroring(mirroringEnable);

    log("romSize         : %dKB\n", (romSize + 1023) / 1024);
    log("has RTC         : %s.\n", rtcEnabled ? "Yes" : "No");
    log("cpuSaveType     : %s.\n", savetype[cpuSaveType]);
    if (cpuSaveType == 3)
        log("flashSize       : %d.\n", flashSize);
    else if (cpuSaveType == 1)
        log("eepromSize      : %d.\n", eepromSize);
    log("mirroringEnable : %s.\n", mirroringEnable ? "Yes" : "No");
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
    soundSetSampleRate(SAMPLERATE);

    if (option_useBios) {
        snprintf(biosfile, sizeof(biosfile), "%s%c%s", retro_system_directory, SLASH, "gba_bios.bin");
        log("Loading bios: %s\n", biosfile);
    }
    CPUInit(biosfile, option_useBios);

    systemWidth = gbaWidth;
    systemHeight = gbaHeight;

    CPUReset();
}

static void gb_init(void)
{
    const char *biosname[] = {"gb_bios.bin", "gbc_bios.bin"};

    log("Loading VBA-M Core (GB/GBC)...\n");

    gbGetHardwareType();

    setColorizerHack(option_colorizerHack);

    // Disable bios loading when using Colorizer hack
    if (option_colorizerHack)
        option_useBios = false;

    if (option_useBios) {
        snprintf(biosfile, sizeof(biosfile), "%s%c%s",
            retro_system_directory, SLASH, biosname[gbCgbMode]);
        log("Loading bios: %s\n", biosfile);
    }

    gbCPUInit(biosfile, option_useBios);

    if (gbBorderOn) {
        systemWidth = gbBorderLineSkip = sgbWidth;
        systemHeight = sgbHeight;
        gbBorderColumnSkip = (sgbWidth - gbWidth) >> 1;
        gbBorderRowSkip = (sgbHeight - gbHeight) >> 1;
    } else {
        systemWidth = gbBorderLineSkip = gbWidth;
        systemHeight = gbHeight;
        gbBorderColumnSkip = gbBorderRowSkip = 0;
    }

    gbSoundSetSampleRate(SAMPLERATE);
    gbSoundSetDeclicking(1);

    gbReset(); // also resets sound;
    set_gbPalette();

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

    if (gbBattery)
        log("Game supports battery save ram.\n");
    if (gbRom[0x143] == 0xc0)
        log("Game works on CGB only\n");
    else if (gbRom[0x143] == 0x80)
        log("Game supports GBC functions, GB compatible.\n");
    if (gbRom[0x146] == 0x03)
        log("Game supports SGB functions\n");
}

void retro_deinit(void)
{
    emulating = 0;
    core->emuCleanUp();
    soundShutdown();
    libretro_supports_bitmasks = false;
}

void retro_reset(void)
{
    core->emuReset();
    set_gbPalette();
}

#define MAX_PLAYERS 4
#define MAX_BUTTONS 10
#define TURBO_BUTTONS 2
static bool option_turboEnable = false;
static unsigned option_turboDelay = 3;
static unsigned turbo_delay_counter[MAX_PLAYERS][TURBO_BUTTONS] = {{0}, {0}};
static const unsigned binds[MAX_BUTTONS] = {
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
};

static const unsigned turbo_binds[TURBO_BUTTONS] = {
    RETRO_DEVICE_ID_JOYPAD_X,
    RETRO_DEVICE_ID_JOYPAD_Y
};

static void systemUpdateSolarSensor(int level);
static uint8_t sensorDarkness = 0xE8;
static uint8_t sensorDarknessLevel = 0; // so we can adjust sensor from gamepad
static int option_analogDeadzone;
static int option_gyroSensitivity, option_tiltSensitivity;
static bool option_swapAnalogSticks;

static void update_variables(bool startup)
{
    struct retro_variable var = { NULL, NULL };
    char key[256] = {0};
    int disabled_layers = 0;
    int sound_enabled = 0x30F;
    bool sound_changed = false;

    var.key = key;
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

    strcpy(key, "vbam_sound_x");
    for (unsigned i = 0; i < 6; i++) {
        key[strlen("vbam_sound_")] = '1' + i;
        var.value = NULL;
        if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value && var.value[0] == 'd') {
            int which = (i < 4) ? (1 << i) : (0x100 << (i - 4));
            sound_enabled &= ~(which);
        }
    }

    if (soundGetEnable() != sound_enabled)
        soundSetEnable(sound_enabled & 0x30F);

    var.key = "vbam_soundinterpolation";
    var.value = NULL;

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
        bool newval = (!strcmp(var.value, "enabled"));
        if (option_sndInterpolation != newval) {
            option_sndInterpolation = newval;
            sound_changed = true;
        }
    }

    var.key = "vbam_soundfiltering";
    var.value = NULL;

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
        double newval = atof(var.value) * 0.1f;
        if (option_sndFiltering != newval) {
            option_sndFiltering = newval;
            sound_changed = true;
        }
    }

    if (sound_changed) {
        soundInterpolation = option_sndInterpolation;
        soundFiltering     = option_sndFiltering;
    }

    var.key = "vbam_usebios";
    var.value = NULL;

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
        option_useBios = (!strcmp(var.value, "enabled")) ? true : false;
    }

    var.key = "vbam_forceRTCenable";
    var.value = NULL;

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
        option_forceRTCenable = (!strcmp(var.value, "enabled")) ? true : false;
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
        if (strcmp(var.value, "auto") == 0) {
            gbBorderAutomatic = 1;
        }
        else if (!strcmp(var.value, "enabled")) {
            gbBorderAutomatic = 0;
            gbBorderOn = 1;
        } else { // disabled
            gbBorderOn = 0;
            gbBorderAutomatic = 0;
        }

        if ((type == IMAGE_GB) && !startup)
            SetGBBorder(gbBorderOn);
    }

    var.key = "vbam_gbHardware";
    var.value = NULL;

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value && startup) {
        if (strcmp(var.value, "auto") == 0)
            gbEmulatorType = 0;
        else if (strcmp(var.value, "gbc") == 0)
            gbEmulatorType = 1;
        else if (strcmp(var.value, "sgb") == 0)
            gbEmulatorType = 2;
        else if (strcmp(var.value, "gb") == 0)
            gbEmulatorType = 3;
        else if (strcmp(var.value, "gba") == 0)
            gbEmulatorType = 4;
        else if (strcmp(var.value, "sgb2") == 0)
            gbEmulatorType = 5;
    }

    var.key = "vbam_allowcolorizerhack";
    var.value = NULL;

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
        option_colorizerHack = (!strcmp(var.value, "enabled")) ? true : false;
    }

    var.key = "vbam_turboenable";

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
        option_turboEnable = (!strcmp(var.value, "enabled")) ? true : false;
    }

    var.key = "vbam_turbodelay";

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
        option_turboDelay = atoi(var.value);
    }

    var.key = "vbam_astick_deadzone";
    var.value = NULL;

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
        option_analogDeadzone = (int)(atof(var.value) * 0.01 * 0x8000);
    }

    var.key = "vbam_tilt_sensitivity";
    var.value = NULL;

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
        option_tiltSensitivity = atoi(var.value);
    }

    var.key = "vbam_gyro_sensitivity";
    var.value = NULL;

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
        option_gyroSensitivity = atoi(var.value);
    }

    var.key = "vbam_swap_astick";
    var.value = NULL;

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
        option_swapAnalogSticks = (!strcmp(var.value, "enabled")) ? true : false;
    }

    var.key = "vbam_palettes";
    var.value = NULL;

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
        unsigned lastpal = option_gbPalette;

        if (!strcmp(var.value, "black and white"))
            option_gbPalette = 0;
        else if (!strcmp(var.value, "blue sea"))
            option_gbPalette = 1;
        else if (!strcmp(var.value, "dark knight"))
            option_gbPalette = 2;
        else if (!strcmp(var.value, "green forest"))
            option_gbPalette = 3;
        else if (!strcmp(var.value, "hot desert"))
            option_gbPalette = 4;
        else if (!strcmp(var.value, "pink dreams"))
            option_gbPalette = 5;
        else if (!strcmp(var.value, "wierd colors"))
            option_gbPalette = 6;
        else if (!strcmp(var.value, "original gameboy"))
            option_gbPalette = 7;
        else if (!strcmp(var.value, "gba sp"))
            option_gbPalette = 8;

        if (lastpal != option_gbPalette)
            set_gbPalette();
    }

    var.key = "vbam_gbcoloroption";
    var.value = NULL;

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
        gbColorOption = (!strcmp(var.value, "enabled")) ? 1 : 0;
    }

    var.key = "vbam_lcdfilter";
    var.value = NULL;

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
        bool prev_lcdfilter = option_lcdfilter;
        option_lcdfilter = (!strcmp(var.value, "enabled")) ? true : false;
        if (prev_lcdfilter != option_lcdfilter)
            utilUpdateSystemColorMaps(option_lcdfilter);
    }

    var.key = "vbam_interframeblending";
    var.value = NULL;

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
        if (!strcmp(var.value, "smart"))
        {
            if (systemColorDepth == 16)
                ifb_filter_func = SmartIB;
            else
                ifb_filter_func = SmartIB32;
        }
        else if (!strcmp(var.value, "motion blur"))
        {
            if (systemColorDepth == 16)
                ifb_filter_func = MotionBlurIB;
            else
                ifb_filter_func = MotionBlurIB32;
        }
        else
            ifb_filter_func = NULL;
    }
    else
        ifb_filter_func = NULL;

    // Hide some core options depending on rom image type
    if (startup) {
        unsigned i;
        struct retro_core_option_display option_display;
        char gb_options[5][25] = {
            "vbam_palettes",
            "vbam_gbHardware",
            "vbam_allowcolorizerhack",
            "vbam_showborders",
            "vbam_gbcoloroption"
        };
        char gba_options[3][22] = {
            "vbam_solarsensor",
            "vbam_gyro_sensitivity",
            "vbam_forceRTCenable"
        };

        // Show or hide GB/GBC only options
        option_display.visible = (type == IMAGE_GB) ? 1 : 0;
        for (i = 0; i < 5; i++)
        {
            option_display.key = gb_options[i];
            environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
        }

        // Show or hide GBA only options
        option_display.visible = (type == IMAGE_GBA) ? 1 : 0;
        for (i = 0; i < 3; i++)
        {
            option_display.key = gba_options[i];
            environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY, &option_display);
        }
    }
}

// System analog stick range is -0x7fff to 0x7fff
// Implementation from mupen64plus-libretro
#include <math.h>
#define ROUND(x)    floor((x) + 0.5)
#define ASTICK_MAX  0x8000
static int analog_x, analog_y, analog_z;
static uint32_t input_buf[MAX_PLAYERS] = { 0 };

static void updateInput_MotionSensors(void)
{
    int16_t analog[3], astick_data[3];
    double scaled_range, radius, angle;
    unsigned tilt_retro_device_index =
        option_swapAnalogSticks ? RETRO_DEVICE_INDEX_ANALOG_LEFT : RETRO_DEVICE_INDEX_ANALOG_RIGHT;
    unsigned gyro_retro_device_index =
        option_swapAnalogSticks ? RETRO_DEVICE_INDEX_ANALOG_RIGHT : RETRO_DEVICE_INDEX_ANALOG_LEFT;

    // Tilt sensor section
    analog[0] = input_cb(0, RETRO_DEVICE_ANALOG,
        tilt_retro_device_index, RETRO_DEVICE_ID_ANALOG_X);
    analog[1] = input_cb(0, RETRO_DEVICE_ANALOG,
        tilt_retro_device_index, RETRO_DEVICE_ID_ANALOG_Y);

    // Convert cartesian coordinate analog stick to polar coordinates
    radius = sqrt(analog[0] * analog[0] + analog[1] * analog[1]);
    angle = atan2(analog[1], analog[0]);

    if (radius > option_analogDeadzone) {
        // Re-scale analog stick range to negate deadzone (makes slow movements possible)
        radius = (radius - option_analogDeadzone) *
            ((float)ASTICK_MAX/(ASTICK_MAX - option_analogDeadzone));
        // Tilt sensor range is from  from 1897 to 2197
        radius *= 150.0 / ASTICK_MAX * (option_tiltSensitivity / 100.0);
        // Convert back to cartesian coordinates
        astick_data[0] = +(int16_t)ROUND(radius * cos(angle));
        astick_data[1] = -(int16_t)ROUND(radius * sin(angle));
    } else
        astick_data[0] = astick_data[1] = 0;

    analog_x = astick_data[0];
    analog_y = astick_data[1];

    // Gyro sensor section
    analog[2] = input_cb(0, RETRO_DEVICE_ANALOG,
        gyro_retro_device_index, RETRO_DEVICE_ID_ANALOG_X);

    if ( analog[2] < -option_analogDeadzone ) {
        // Re-scale analog stick range
        scaled_range = (-analog[2] - option_analogDeadzone) *
            ((float)ASTICK_MAX / (ASTICK_MAX - option_analogDeadzone));
        // Gyro sensor range is +/- 1800
        scaled_range *= 1800.0 / ASTICK_MAX * (option_gyroSensitivity / 100.0);
        astick_data[2] = -(int16_t)ROUND(scaled_range);
    } else if ( analog[2] > option_analogDeadzone ) {
        scaled_range = (analog[2] - option_analogDeadzone) *
            ((float)ASTICK_MAX / (ASTICK_MAX - option_analogDeadzone));
        scaled_range *= (1800.0 / ASTICK_MAX * (option_gyroSensitivity / 100.0));
        astick_data[2] = +(int16_t)ROUND(scaled_range);
    } else
        astick_data[2] = 0;

    analog_z = astick_data[2];
}

// Update solar sensor level by gamepad buttons, default L2/R2
void updateInput_SolarSensor(void)
{
    static bool buttonpressed = false;

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
    }
}

// Update joypads
static void updateInput_Joypad(void)
{
    unsigned max_buttons = MAX_BUTTONS - ((type == IMAGE_GB) ? 2 : 0); // gb only has 8 buttons
    int16_t inbuf = 0;

    for (unsigned port = 0; port < MAX_PLAYERS; port++)
    {
        // Reset input states
        input_buf[port] = 0;

        if (retropad_device[port] == RETRO_DEVICE_JOYPAD) {
            if (libretro_supports_bitmasks)
                inbuf = input_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_MASK);
            else
            {
                for (int i = 0; i < (RETRO_DEVICE_ID_JOYPAD_R3 + 1); i++)
                    inbuf |= input_cb(0, RETRO_DEVICE_JOYPAD, 0, i) ? (1 << i) : 0;
            }

            for (unsigned button = 0; button < max_buttons; button++)
                input_buf[port] |= (inbuf & (1 << binds[button])) ? (1 << button) : 0;

            if (option_turboEnable) {
                /* Handle Turbo A & B buttons */
                for (unsigned tbutton = 0; tbutton < TURBO_BUTTONS; tbutton++) {
                    if (input_cb(port, RETRO_DEVICE_JOYPAD, 0, turbo_binds[tbutton])) {
                        if (!turbo_delay_counter[port][tbutton])
                            input_buf[port] |= 1 << tbutton;
                        turbo_delay_counter[port][tbutton]++;
                        if (turbo_delay_counter[port][tbutton] > option_turboDelay)
                            /* Reset the toggle if delay value is reached */
                            turbo_delay_counter[port][tbutton] = 0;
                    }
                    else
                        /* If the button is not pressed, just reset the toggle */
                        turbo_delay_counter[port][tbutton] = 0;
                }
            }
            // Do not allow opposing directions
            if ((input_buf[port] & 0x30) == 0x30)
                input_buf[port] &= ~(0x30);
            else if ((input_buf[port] & 0xC0) == 0xC0)
                input_buf[port] &= ~(0xC0);
        }
    }
}

static bool firstrun = true;
static unsigned has_frame;

void retro_run(void)
{
    bool updated = false;

    if (firstrun) {
        bool initRTC = false;
        firstrun = false;
        /* Check if GB game has RTC data. Has to be check here since this is where the data will be
         * available when using libretro api. */
        if ((type == IMAGE_GB) && gbRTCPresent) {
            switch (gbRomType) {
            case 0x0f:
            case 0x10:
                /* Check if any RTC has been loaded, zero value means nothing has been loaded. */
                if (!gbDataMBC3.mapperLastTime)
                    initRTC = true;
                break;
            case 0xfd:
                if (!gbDataTAMA5.mapperLastTime)
                    initRTC = true;
                break;
            case 0xfe:
                if (!gbRTCHuC3.mapperLastTime)
                    initRTC = true;
                break;
            }
            /* Initialize RTC using local time if needed */
            if (initRTC)
                gbInitRTC();
        }
    }

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated)
        update_variables(false);

    poll_cb();

    updateInput_Joypad();
    updateInput_SolarSensor();
    updateInput_MotionSensors();

    has_frame = 0;

    while (!has_frame)
        core->emuMain(core->emuCount);
}

static unsigned serialize_size = 0;

size_t retro_serialize_size(void)
{
    return serialize_size;
}

bool retro_serialize(void* data, size_t size)
{
    if (size == serialize_size)
        return core->emuWriteState((uint8_t*)data);
    return false;
}

bool retro_unserialize(const void* data, size_t size)
{
    if (size == serialize_size)
        return core->emuReadState((uint8_t*)data);
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

#define ISHEXDEC \
    ((code[cursor] >= '0') && (code[cursor] <= '9')) || \
    ((code[cursor] >= 'a') && (code[cursor] <= 'f')) || \
    ((code[cursor] >= 'A') && (code[cursor] <= 'F')) || \
    (code[cursor] == '-') \

void retro_cheat_set(unsigned index, bool enabled, const char* code)
{
    // 2018-11-30 - retrowertz
    // added support GB/GBC multiline 6/9 digit Game Genie codes and Game Shark

    char name[128] = {0};
    unsigned cursor = 0;
    char *codeLine = NULL;
    int codeLineSize = strlen(code) + 5;
    int codePos = 0;
    int i = 0;

    codeLine = (char *)calloc(codeLineSize, sizeof(char));
    sprintf(name, "cheat_%d", index);
    for (cursor = 0;; cursor++) {
        if (ISHEXDEC) {
            codeLine[codePos++] = toupper(code[cursor]);
        } else {
            switch (type) {
            case IMAGE_GB:
                if (codePos >= 7) {
                    if (codePos == 7 || codePos == 11) {
                        codeLine[codePos] = '\0';
                        if (gbAddGgCheat(codeLine, name))
                            log("Cheat code added: '%s'\n", codeLine);
                    } else if (codePos == 8) {
                        codeLine[codePos] = '\0';
                        if (gbAddGsCheat(codeLine, name))
                            log("Cheat code added: '%s'\n", codeLine);
                    } else {
                        codeLine[codePos] = '\0';
                        log("Invalid cheat code '%s'\n", codeLine);
                    }
                    codePos = 0;
                    memset(codeLine, 0, codeLineSize);
                }
                break;
            case IMAGE_GBA:
                if (codePos >= 12) {
                    if (codePos == 12 ) {
                        for (i = 0; i < 4 ; i++)
                            codeLine[codePos - i] = codeLine[(codePos - i) - 1];
                        codeLine[8] = ' ';
                        codeLine[13] = '\0';
                        cheatsAddCBACode(codeLine, name);
                        log("Cheat code added: '%s'\n", codeLine);
                    } else if (codePos == 16) {
                        codeLine[16] = '\0';
                        cheatsAddGSACode(codeLine, name, true);
                        log("Cheat code added: '%s'\n", codeLine);
                    } else {
                        codeLine[codePos] = '\0';
                        log("Invalid cheat code '%s'\n", codeLine);
                    }
                    codePos = 0;
                    memset(codeLine, 0, codeLineSize);
                }
                break;
            default: break;
            }
            if (!code[cursor])
                break;
        }
    }
    free(codeLine);
}

static void update_input_descriptors(void)
{
    if (type == IMAGE_GB) {
        if (gbSgbMode) {
            environ_cb(RETRO_ENVIRONMENT_SET_CONTROLLER_INFO, (void*)ports_sgb);
            environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, input_sgb);
        } else {
            environ_cb(RETRO_ENVIRONMENT_SET_CONTROLLER_INFO, (void*)ports_gb);
            environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, input_gb);
        }
    } else if (type == IMAGE_GBA) {
        environ_cb(RETRO_ENVIRONMENT_SET_CONTROLLER_INFO, (void*)ports_gba);
        environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, input_gba);
    }
}

bool retro_load_game(const struct retro_game_info *game)
{
   type = utilFindType((const char *)game->path);

   if (type == IMAGE_UNKNOWN) {
      log("Unsupported image %s!\n", game->path);
      return false;
   }

   utilUpdateSystemColorMaps(option_lcdfilter);
   update_variables(true);
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

      unsigned addr, i;
      struct retro_memory_descriptor desc[18];
      struct retro_memory_map retromap;

      memset(desc, 0, sizeof(desc));
      // the GB's memory is divided into 16 parts with 4096 bytes each == 65536
      // $FFFF        Interrupt Enable Flag
      // $FF80-$FFFE  Zero Page - 127 bytes
      // $FF00-$FF7F  Hardware I/O Registers
      // $FEA0-$FEFF  Unusable Memory
      // $FE00-$FE9F  OAM - Object Attribute Memory
      // $E000-$FDFF  Echo RAM - Reserved, Do Not Use
      // $D000-$DFFF  Internal RAM - Bank 1-7 (switchable - CGB only)
      // $C000-$CFFF  Internal RAM - Bank 0 (fixed)
      // $A000-$BFFF  Cartridge RAM (If Available)
      // $9C00-$9FFF  BG Map Data 2
      // $9800-$9BFF  BG Map Data 1
      // $8000-$97FF  Character RAM
      // $4000-$7FFF  Cartridge ROM - Switchable Banks 1-xx
      // $0150-$3FFF  Cartridge ROM - Bank 0 (fixed)
      // $0100-$014F  Cartridge Header Area
      // $0000-$00FF  Restart and Interrupt Vectors
      // http://gameboy.mongenel.com/dmg/asmmemmap.html
      for (addr = 0, i = 0; addr < 16; addr++) {
         if (addr == 13) continue;
         if (addr == 14) continue;
         if (addr == 12) {                               // WRAM, bank 0-1
            if (!gbCgbMode) {                            // WRAM-GB
               if (!gbMemory) continue;
               desc[i].ptr      = gbMemory + 0xC000;
            } else {                                     // WRAM GBC
               if (!gbWram) continue;
               desc[i].ptr      = gbWram;
            }
            desc[i].start       = addr * 0x1000;
            desc[i].len         = 0x2000;
            i++;
            continue;
         } else {                                        // Everything else map
            if (gbMemoryMap[addr]) {
               desc[i].ptr      = gbMemoryMap[addr];
               desc[i].start    = addr * 0x1000;
               desc[i].len      = 0x1000;
               if (addr < 4)
                  desc[i].flags = RETRO_MEMDESC_CONST;
               i++;
            }
         }
      }

      if (gbWram && gbCgbMode) { // banks 2-7 of GBC work ram banks at $10000
         desc[i].ptr    = gbWram;
         desc[i].offset = 0x2000;
         desc[i].start  = 0x10000;
         desc[i].select = 0xFFFFA000;
         desc[i].len    = 0x6000;
         i++;
      }

      retromap.descriptors = desc;
      retromap.num_descriptors = i;
      environ_cb(RETRO_ENVIRONMENT_SET_MEMORY_MAPS, &retromap);
   }

   if (!core)
       return false;

   update_input_descriptors();    // Initialize input descriptors and info
   update_variables(false);
   uint8_t* state_buf = (uint8_t*)malloc(2000000);
   serialize_size = core->emuWriteState(state_buf);
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

// system callbacks

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
    unsigned pitch = systemWidth * (systemColorDepth >> 3);
    if (ifb_filter_func)
        ifb_filter_func(pix, pitch, systemWidth, systemHeight);
    video_cb(pix, systemWidth, systemHeight, pitch);
}

void systemSendScreen(void)
{
}

void systemFrame(void)
{
    has_frame = 1;
}

void systemGbBorderOn(void)
{
    SetGBBorder(1);
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

uint32_t systemReadJoypad(int which)
{
    if (which == -1)
        which = 0;
    return input_buf[which];
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

static int sensor_tilt[2], sensor_gyro;

int systemGetSensorX()
{
    return sensor_tilt[0];
}

int systemGetSensorY()
{
    return sensor_tilt[1];
}

int systemGetSensorZ()
{
    return sensor_gyro / 10;
}

uint8_t systemGetSensorDarkness(void)
{
    return sensorDarkness;
}

void systemUpdateMotionSensor(void)
{
    // Max ranges as set in VBAM
    static const int tilt_max    = 2197;
    static const int tilt_min    = 1897;
    static const int tilt_center = 2047;
    static const int gyro_thresh = 1800;

    if (!sensor_tilt[0])
        sensor_tilt[0] = tilt_center;

    if (!sensor_tilt[1])
        sensor_tilt[1] = tilt_center;

    sensor_tilt[0] = (-analog_x) + tilt_center;
    if (sensor_tilt[0] > tilt_max)
        sensor_tilt[0] = tilt_max;
    else if (sensor_tilt[0] < tilt_min)
        sensor_tilt[0] = tilt_min;

    sensor_tilt[1] = analog_y + tilt_center;
    if (sensor_tilt[1] > tilt_max)
        sensor_tilt[1] = tilt_max;
    else if (sensor_tilt[1] < tilt_min)
        sensor_tilt[1] = tilt_min;

    sensor_gyro = analog_z;
    if (sensor_gyro > gyro_thresh)
        sensor_gyro = gyro_thresh;
    else if (sensor_gyro < -gyro_thresh)
        sensor_gyro = -gyro_thresh;
}

void systemCartridgeRumble(bool e)
{
    if (!rumble_cb)
        return;

    if (e) {
        rumble_cb(0, RETRO_RUMBLE_WEAK, 0xffff);
        rumble_cb(0, RETRO_RUMBLE_STRONG, 0xffff);
    } else {
        rumble_cb(0, RETRO_RUMBLE_WEAK, 0);
        rumble_cb(0, RETRO_RUMBLE_STRONG, 0);
    }
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
