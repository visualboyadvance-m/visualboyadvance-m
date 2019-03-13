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

#define VBA_CURRENT_VERSION "2.1.1"

static retro_log_printf_t log_cb;
static retro_video_refresh_t video_cb;
static retro_input_poll_t poll_cb;
static retro_input_state_t input_cb;
static retro_environment_t environ_cb;
retro_audio_sample_batch_t audio_batch_cb;
static retro_set_rumble_state_t rumble_cb;

static char retro_system_directory[4096];
static char biosfile[4096];
static float sndFiltering = 0.5f;
static bool sndInterpolation = true;
static bool can_dupe = false;
static bool usebios = false;
static unsigned retropad_device[4] = {0};

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
static unsigned current_gbPalette;

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

// Dummy vars/funcs for serial io emulation without LINK communication related stuff
#ifndef NO_LINK
#include "../gba/GBALink.h"
uint8_t gbSIO_SC;
bool LinkIsWaiting;
bool LinkFirstTime;
bool EmuReseted;
int winGbPrinterEnabled;
bool gba_joybus_active = false;

LinkMode GetLinkMode()
{
    return LINK_DISCONNECTED;
}

void StartGPLink(uint16_t value)
{
    WRITE16LE(((uint16_t*)&ioMem[COMM_RCNT]), value);
}

void LinkUpdate(int ticks)
{
}

void StartLink(uint16_t siocnt)
{
}

void CheckLinkConnection()
{
}

void gbInitLink()
{
   LinkIsWaiting = false;
   LinkFirstTime = true;
}

uint16_t gbLinkUpdate(uint8_t b, int gbSerialOn) //used on external clock
{
   return (b << 8);
}
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

static const uint16_t defaultGBPalettes[][8] = {
    {
        // Standard
        0x7FFF, 0x56B5, 0x318C, 0x0000, 0x7FFF, 0x56B5, 0x318C, 0x0000,
    },
    {
        // Blue Sea
        0x6200, 0x7E10, 0x7C10, 0x5000, 0x6200, 0x7E10, 0x7C10, 0x5000,
    },
    {
        // Dark Night
        0x4008, 0x4000, 0x2000, 0x2008, 0x4008, 0x4000, 0x2000, 0x2008,
    },
    {
        // Green Forest
        0x43F0, 0x03E0, 0x4200, 0x2200, 0x43F0, 0x03E0, 0x4200, 0x2200,
    },
    {
        // Hot Desert
        0x43FF, 0x03FF, 0x221F, 0x021F, 0x43FF, 0x03FF, 0x221F, 0x021F,
    },
    {
        // Pink Dreams
        0x621F, 0x7E1F, 0x7C1F, 0x2010, 0x621F, 0x7E1F, 0x7C1F, 0x2010,
    },
    {
        // Weird Colors
        0x621F, 0x401F, 0x001F, 0x2010, 0x621F, 0x401F, 0x001F, 0x2010,
    },
    {
        // Real GB Colors
        0x1B8E, 0x02C0, 0x0DA0, 0x1140, 0x1B8E, 0x02C0, 0x0DA0, 0x1140,
    },
    {
        // Real 'GB on GBASP' Colors
        0x7BDE, /*0x23F0*/ 0x5778, /*0x5DC0*/ 0x5640, 0x0000, 0x7BDE, /*0x3678*/ 0x529C, /*0x0980*/ 0x2990, 0x0000,
    }
};

static void set_gbPalette(void)
{
    const uint16_t *pal = defaultGBPalettes[current_gbPalette];

    if (type != IMAGE_GB)
        return;

    if (gbCgbMode || gbSgbMode)
        return;

    for (int i = 0; i < 8; i++)
        gbPalette[i] = pal[i];
}

extern int gbRomType; // gets type from header 0x147
extern int gbBattery; // enabled when gbRamSize != 0

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
    return NULL;
}

static size_t gb_rtcdata_size(void)
{
    if (gb_hasrtc()) {
        switch (gbRomType) {
        case 0x0f:
        case 0x10: // MBC3 + extended
            return MBC3_RTC_DATA_SIZE;
            break;
        case 0x13:
        case 0xfd: // TAMA5 + extended
            return TAMA5_RTC_DATA_SIZE;
            break;
        }
    }
    return 0;
}

static void gbUpdateRTC(void)
{
    if (gb_hasrtc()) {
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
    if (type == IMAGE_GBA) {
        switch (id) {
        case RETRO_MEMORY_SAVE_RAM:
            if ((saveType == GBA_SAVE_EEPROM) | (saveType == GBA_SAVE_EEPROM_SENSOR))
                return eepromData;
            if ((saveType == GBA_SAVE_SRAM) | (saveType == GBA_SAVE_FLASH))
                return flashSaveMemory;
            return NULL;
        case RETRO_MEMORY_SYSTEM_RAM:
            return workRAM;
        case RETRO_MEMORY_VIDEO_RAM:
            return vram;
        }
    }
    else if (type == IMAGE_GB) {
        switch (id) {
        case RETRO_MEMORY_SAVE_RAM:
            if (gbBattery)
                return gbRam;
            return NULL;
        case RETRO_MEMORY_SYSTEM_RAM:
            return gbMemoryMap[0x0c];
        case RETRO_MEMORY_VIDEO_RAM:
            return gbMemoryMap[0x08] ;
        }
    }

    return NULL;
}

size_t retro_get_memory_size(unsigned id)
{
    if (type == IMAGE_GBA) {
        switch (id) {
        case RETRO_MEMORY_SAVE_RAM:
            if ((saveType == GBA_SAVE_EEPROM) | (saveType == GBA_SAVE_EEPROM_SENSOR))
                return eepromSize;
            if (saveType == GBA_SAVE_FLASH)
                return flashSize;
            if (saveType == GBA_SAVE_SRAM)
                return SIZE_SRAM;
            return 0;
        case RETRO_MEMORY_SYSTEM_RAM:
            return SIZE_WRAM;
        case RETRO_MEMORY_VIDEO_RAM:
            return SIZE_VRAM - 0x2000; // usuable vram is only 0x18000
        }
    }
    else if (type == IMAGE_GB) {
        switch (id) {
        case RETRO_MEMORY_SAVE_RAM:
            if (gbBattery)
                return gbRamSize;
            return 0;
        case RETRO_MEMORY_SYSTEM_RAM:
            return 0x2000;
        case RETRO_MEMORY_VIDEO_RAM:
            return 0x2000;
        }
    }

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

   struct retro_variable variables[] = {
      { "vbam_solarsensor", "Solar Sensor Level; 0|1|2|3|4|5|6|7|8|9|10" },
      { "vbam_usebios", "Use BIOS file (Restart); disabled|enabled" },
      { "vbam_soundinterpolation", "Sound Interpolation; enabled|disabled" },
      { "vbam_soundfiltering", "Sound Filtering; 5|6|7|8|9|10|0|1|2|3|4" },
      { "vbam_gbHardware", "(GB) Emulated Hardware; Game Boy Color|Automatic|Super Game Boy|Game Boy|Game Boy Advance|Super Game Boy 2" },
      { "vbam_palettes", "(GB) Color Palette; Standard|Blue Sea|Dark Knight|Green Forest|Hot Desert|Pink Dreams|Wierd Colors|Original|GBA SP" },
      { "vbam_showborders", "(GB) Show Borders; disabled|enabled|auto" },
      { "vbam_turboenable", "Enable Turbo Buttons; disabled|enabled" },
      { "vbam_turbodelay", "Turbo Delay (in frames); 3|4|5|6|7|8|9|10|11|12|13|14|15|1|2" },
      { "vbam_astick_deadzone", "Sensors Deadzone (%); 15|20|25|30|0|5|10"},
      { "vbam_gyro_sensitivity", "Sensor Sensitivity (Gyroscope) (%); 100|105|110|115|120|10|15|20|25|30|35|40|45|50|55|60|65|70|75|80|85|90|95"},
      { "vbam_tilt_sensitivity", "Sensor Sensitivity (Tilt) (%); 100|105|110|115|120|10|15|20|25|30|35|40|45|50|55|60|65|70|75|80|85|90|95"},
      { "vbam_swap_astick", "Swap Left/Right Analog; disabled|enabled" },
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

   cb(RETRO_ENVIRONMENT_SET_VARIABLES, variables);
}

void retro_get_system_info(struct retro_system_info *info)
{
   info->need_fullpath = false;
   info->valid_extensions = "dmg|gb|gbc|cgb|sgb|gba";
#ifdef GIT_VERSION
   info->library_version = VBA_CURRENT_VERSION GIT_VERSION;
#else
   info->library_version = VBA_CURRENT_VERSION;
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
    enum retro_pixel_format rgb565 = RETRO_PIXEL_FORMAT_RGB565;
    if (environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &rgb565) && log_cb)
        log_cb(RETRO_LOG_INFO, "Frontend supports RGB565 - will use that instead of XRGB1555.\n");
#else
    enum retro_pixel_format rgb8888 = RETRO_PIXEL_FORMAT_XRGB8888;
    if (environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &rgb8888) && log_cb)
        log_cb(RETRO_LOG_INFO, "Frontend supports XRGB8888 - will use that instead of XRGB1555.\n");
#endif

   bool yes = true;
   environ_cb(RETRO_ENVIRONMENT_SET_SUPPORT_ACHIEVEMENTS, &yes);

   if (environ_cb(RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE, &rumble)) {
      rumble_cb = rumble.set_rumble_state;
   } else
      rumble_cb = NULL;

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

    cpuSaveType = GBA_SAVE_AUTO;
    flashSize = SIZE_FLASH512;
    eepromSize = SIZE_EEPROM_512;
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
    mirroringEnable = (buffer[0] == 0x46) ? true : false;

    if (!cpuSaveType) {
        log("Scrapping ROM for save type.\n");
        utilGBAFindSave(romSize);
    }

    saveType = cpuSaveType;

    if (flashSize == SIZE_FLASH512 || flashSize == SIZE_FLASH1M)
        flashSetSize(flashSize);

    rtcEnable(rtcEnabled);
    rtcEnableRumble(!rtcEnabled);

    doMirroring(mirroringEnable);

    log("romSize         : %dKB\n", (romSize + 1023) / 1024);
    log("has RTC         : %s.\n", rtcEnabled ? "Yes" : "No");
    log("cpuSaveType     : %s.\n", savetype[cpuSaveType]);
    if (cpuSaveType == 3)
        log("flashSize       : %d.\n", flashSize);
    if (cpuSaveType == 1)
        log("eepromSize      : %d.\n", eepromSize);
    log("mirroringEnable : %s.\n", mirroringEnable ? "Yes" : "No");
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
    soundSetSampleRate(SampleRate);

    if (usebios) {
        snprintf(biosfile, sizeof(biosfile), "%s%c%s", retro_system_directory, SLASH, "gba_bios.bin");
        log("Loading bios: %s\n", biosfile);
    }
    CPUInit(biosfile, usebios);

    width = GBAWidth;
    height = GBAHeight;

    CPUReset();
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

    if (gbBattery)
        log("Game supports battery save ram.\n");
    if (gbRom[0x143] == 0xc0)
        log("Game works on CGB only\n");
    else if (gbRom[0x143] == 0x80)
        log("Game supports GBC functions, GB compatible.\n");
    if (gbRom[0x146] == 0x03)
        log("Game supports SGB functions\n");
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
    core->emuReset();
    set_gbPalette();
}

#define MAX_PLAYERS 4
#define MAX_BUTTONS 10
#define TURBO_BUTTONS 2
static bool turbo_enable = false;
static unsigned turbo_delay = 3;
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

static void systemGbBorderOff(void);
static void systemUpdateSolarSensor(int level);
static uint8_t sensorDarkness = 0xE8;
static uint8_t sensorDarknessLevel = 0; // so we can adjust sensor from gamepad
static int astick_deadzone;
static int gyro_sensitivity, tilt_sensitivity;
static bool swap_astick;

static void update_variables(bool startup)
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
            int which = (i < 4) ? (1 << i) : (0x100 << (i - 4));
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

        if ((type == IMAGE_GB) &&
            (oldval != ((gbBorderOn << 1) | gbBorderAutomatic)) && !startup) {
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

    var.key = "vbam_turboenable";

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
    {
        bool val = !strcmp(var.value, "enabled");
        turbo_enable = val;
    }

    var.key = "vbam_turbodelay";

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
    {
        turbo_delay = atoi(var.value);
    }

    var.key = "vbam_astick_deadzone";
    var.value = NULL;

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
    {
        astick_deadzone = (int)(atoi(var.value) * 0.01f * 0x8000);
    }

    var.key = "vbam_tilt_sensitivity";
    var.value = NULL;

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
    {
        tilt_sensitivity = atoi(var.value);
    }

    var.key = "vbam_gyro_sensitivity";
    var.value = NULL;

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
    {
        gyro_sensitivity = atoi(var.value);
    }

    var.key = "vbam_swap_astick";
    var.value = NULL;

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
    {
        swap_astick = (bool)(!strcmp(var.value, "enabled"));
    }

    var.key = "vbam_palettes";
    var.value = NULL;

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
    {
        int lastpal = current_gbPalette;

        if (!strcmp(var.value, "Standard"))
            current_gbPalette = 0;
        else if (!strcmp(var.value, "Blue Sea"))
            current_gbPalette = 1;
        else if (!strcmp(var.value, "Dark Knight"))
            current_gbPalette = 2;
        else if (!strcmp(var.value, "Green Forest"))
            current_gbPalette = 3;
        else if (!strcmp(var.value, "Hot Desert"))
            current_gbPalette = 4;
        else if (!strcmp(var.value, "Pink Dreams"))
            current_gbPalette = 5;
        else if (!strcmp(var.value, "Wierd Colors"))
            current_gbPalette = 6;
        else if (!strcmp(var.value, "Original"))
            current_gbPalette = 7;
        else if (!strcmp(var.value, "GBA SP"))
            current_gbPalette = 8;

        if (lastpal != current_gbPalette)
            set_gbPalette();
    }
}

// System analog stick range is -0x7fff to 0x7fff
// Implementation from mupen64plus-libretro
#include <math.h>
#define ROUND(x)    floor((x) + 0.5)
#define ASTICK_MAX  0x8000
static int analog_x, analog_y, analog_z;

static void updateInput_MotionSensors(void)
{
    int16_t analog[3], astick_data[3];
    double scaled_range, radius, angle;
    unsigned tilt_retro_device_index =
        swap_astick ? RETRO_DEVICE_INDEX_ANALOG_LEFT : RETRO_DEVICE_INDEX_ANALOG_RIGHT;
    unsigned gyro_retro_device_index =
        swap_astick ? RETRO_DEVICE_INDEX_ANALOG_RIGHT : RETRO_DEVICE_INDEX_ANALOG_LEFT;

    // Tilt sensor section
    analog[0] = input_cb(0, RETRO_DEVICE_ANALOG,
        tilt_retro_device_index, RETRO_DEVICE_ID_ANALOG_X);
    analog[1] = input_cb(0, RETRO_DEVICE_ANALOG,
        tilt_retro_device_index, RETRO_DEVICE_ID_ANALOG_Y);

    // Convert cartesian coordinate analog stick to polar coordinates
    radius = sqrt(analog[0] * analog[0] + analog[1] * analog[1]);
    angle = atan2(analog[1], analog[0]);

    if (radius > astick_deadzone) {
        // Re-scale analog stick range to negate deadzone (makes slow movements possible)
        radius = (radius - astick_deadzone) *
            ((float)ASTICK_MAX/(ASTICK_MAX - astick_deadzone));
        // Tilt sensor range is from  from 1897 to 2197
        radius *= 150.0 / ASTICK_MAX * (tilt_sensitivity / 100.0);
        // Convert back to cartesian coordinates
        astick_data[0] = +(int16_t)ROUND(radius * cos(angle));
        astick_data[1] = -(int16_t)ROUND(radius * sin(angle));
    } else
        astick_data[0] = astick_data[1] = 0;

    analog_x = astick_data[0];
    analog_y = astick_data[1];

    // Gyro sensor section
    analog[3] = input_cb(0, RETRO_DEVICE_ANALOG,
        gyro_retro_device_index, RETRO_DEVICE_ID_ANALOG_X);

    if ( analog[3] < -astick_deadzone ) {
        // Re-scale analog stick range
        scaled_range = (-analog[3] - astick_deadzone) *
            ((float)ASTICK_MAX / (ASTICK_MAX - astick_deadzone));
        // Gyro sensor range is +/- 1800
        scaled_range *= 1800.0 / ASTICK_MAX * (gyro_sensitivity / 100.0);
        astick_data[3] = -(int16_t)ROUND(scaled_range);
    } else if ( analog[3] > astick_deadzone ) {
        scaled_range = (analog[3] - astick_deadzone) *
            ((float)ASTICK_MAX / (ASTICK_MAX - astick_deadzone));
        scaled_range *= (1800.0 / ASTICK_MAX * (gyro_sensitivity / 100.0));
        astick_data[3] = +(int16_t)ROUND(scaled_range);
    } else
        astick_data[3] = 0;

    analog_z = astick_data[3];
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

static unsigned has_frame;

void retro_run(void)
{
    bool updated = false;

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated)
        update_variables(false);

    poll_cb();

    updateInput_SolarSensor();
    updateInput_MotionSensors();

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

   update_variables(true);
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

      unsigned addr, i;
      struct retro_memory_descriptor desc[16];
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
         if (gbMemoryMap[addr] != NULL) {
            desc[i].ptr    = gbMemoryMap[addr];
            desc[i].start  = addr * 0x1000;
            desc[i].len    = 4096;
            if (addr < 4)  desc[i].flags  = RETRO_MEMDESC_CONST;
            i++;
         }
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

static int rumble_state, rumble_down;

uint32_t systemReadJoypad(int which)
{
    uint32_t J = 0;
    unsigned i, buttons = MAX_BUTTONS - ((type == IMAGE_GB) ? 2 : 0); // gb only has 8 buttons

    if (which == -1)
        which = 0;

    if (retropad_device[which] == RETRO_DEVICE_JOYPAD) {
        for (i = 0; i < buttons; i++)
            J |= input_cb(which, RETRO_DEVICE_JOYPAD, 0, binds[i]) << i;

        if (turbo_enable) {
            /* Handle Turbo A & B buttons */
            for (i = 0; i < TURBO_BUTTONS; i++) {
                if (input_cb(which, RETRO_DEVICE_JOYPAD, 0, turbo_binds[i])) {
                    if (!turbo_delay_counter[which][i])
                        J |= 1 << i;
                    turbo_delay_counter[which][i]++;
                    if (turbo_delay_counter[which][i] > turbo_delay)
                        /* Reset the toggle if delay value is reached */
                        turbo_delay_counter[which][i] = 0;
                } else
                    /* If the button is not pressed, just reset the toggle */
                    turbo_delay_counter[which][i] = 0;
            }
        }
    }

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
