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

#define RETRO_DEVICE_GBA             RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 0)
#define RETRO_DEVICE_GBA_ALT1        RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 1)
#define RETRO_DEVICE_GBA_ALT2        RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 2)

#define SAMPLERATE                   32768
#define FRAMERATE                    (16777216.0 / 280896.0)

#define ISHEXDEC ((codeLine[cursor]>='0') && (codeLine[cursor]<='9')) || ((codeLine[cursor]>='a') && (codeLine[cursor]<='f')) || ((codeLine[cursor]>='A') && (codeLine[cursor]<='F'))

static retro_log_printf_t log_cb;
static retro_video_refresh_t video_cb;
static retro_input_poll_t poll_cb;
static retro_input_state_t input_cb;
retro_audio_sample_batch_t audio_batch_cb;
static retro_environment_t environ_cb;

static float sndFiltering = 0.5f;
static bool sndInterpolation = true;
static bool enableRtc = false;
static bool can_dupe = false;
int emulating = 0;
static unsigned controller_layout = 0;

uint8_t libretro_save_buf[0x20000 + 0x2000]; /* Workaround for broken-by-design GBA save semantics. */

static unsigned libretro_save_size = sizeof(libretro_save_buf);
static char biosfile[1024] = {0};
static bool usebios = false;

int RGB_LOW_BITS_MASK = 0;

uint16_t systemColorMap16[0x10000];
uint32_t systemColorMap32[0x10000];
uint16_t systemGbPalette[24];
int systemRedShift = 0;
int systemBlueShift = 0;
int systemGreenShift = 0;
int systemColorDepth = 32;
int systemDebug = 0;
int systemVerbose = 0;
int systemFrameSkip = 0;
int systemSaveUpdateCounter = SYSTEM_SAVE_NOT_UPDATED;
int systemSpeed = 0;

void (*dbgOutput)(const char* s, uint32_t addr);
void (*dbgSignal)(int sig, int number);

void* retro_get_memory_data(unsigned id)
{
    if (id == RETRO_MEMORY_SAVE_RAM)
        return libretro_save_buf;
    if (id == RETRO_MEMORY_SYSTEM_RAM)
        return workRAM;
    if (id == RETRO_MEMORY_VIDEO_RAM)
        return vram;

    return NULL;
}

size_t retro_get_memory_size(unsigned id)
{
    if (id == RETRO_MEMORY_SAVE_RAM)
        return libretro_save_size;
    if (id == RETRO_MEMORY_SYSTEM_RAM)
        return 0x40000;
    if (id == RETRO_MEMORY_VIDEO_RAM)
        return 0x20000;

    return 0;
}

static bool scan_area(const uint8_t* data, unsigned size)
{
    for (unsigned i = 0; i < size; i++)
        if (data[i] != 0xff)
            return true;

    return false;
}

static void adjust_save_ram()
{
    if (scan_area(libretro_save_buf, 512) && !scan_area(libretro_save_buf + 512, sizeof(libretro_save_buf) - 512)) {
        libretro_save_size = 512;
        if (log_cb)
            log_cb(RETRO_LOG_INFO, "Detecting EEprom 8kbit\n");
    } else if (scan_area(libretro_save_buf, 0x2000) && !scan_area(libretro_save_buf + 0x2000, sizeof(libretro_save_buf) - 0x2000)) {
        libretro_save_size = 0x2000;
        if (log_cb)
            log_cb(RETRO_LOG_INFO, "Detecting EEprom 64kbit\n");
    }

    else if (scan_area(libretro_save_buf, 0x10000) && !scan_area(libretro_save_buf + 0x10000, sizeof(libretro_save_buf) - 0x10000)) {
        libretro_save_size = 0x10000;
        if (log_cb)
            log_cb(RETRO_LOG_INFO, "Detecting Flash 512kbit\n");
    } else if (scan_area(libretro_save_buf, 0x20000) && !scan_area(libretro_save_buf + 0x20000, sizeof(libretro_save_buf) - 0x20000)) {
        libretro_save_size = 0x20000;
        if (log_cb)
            log_cb(RETRO_LOG_INFO, "Detecting Flash 1Mbit\n");
    } else if (log_cb)
        log_cb(RETRO_LOG_INFO, "Did not detect any particular SRAM type.\n");

    if (libretro_save_size == 512 || libretro_save_size == 0x2000)
        eepromData = libretro_save_buf;
    else if (libretro_save_size == 0x10000 || libretro_save_size == 0x20000)
        flashSaveMemory = libretro_save_buf;
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

void retro_set_controller_port_device(unsigned port, unsigned device)
{
    if (port > 0) return; // GBA Only supports 1 controller

    log_cb(RETRO_LOG_INFO, "Controller %d'\n", device);
    switch (device) {

    case RETRO_DEVICE_JOYPAD:
    case RETRO_DEVICE_GBA:
    default:
        controller_layout = 0;
        break;
    case RETRO_DEVICE_GBA_ALT1:
        controller_layout = 1;
        break;
    case RETRO_DEVICE_GBA_ALT2:
        controller_layout = 2;
        break;
    case RETRO_DEVICE_NONE:
        controller_layout = -1;
        break;
    }
}

void retro_set_environment(retro_environment_t cb)
{
   environ_cb = cb;

   struct retro_variable variables[] = {
      { "vbam_solarsensor", "Solar Sensor Level; 0|1|2|3|4|5|6|7|8|9|10" },
      { "vbam_usebios", "Use BIOS file (Restart); disabled|enabled" },
      { "vbam_soundinterpolation", "Sound Interpolation; disabled|enabled" },
      { "vbam_soundfiltering", "Sound Filtering; 5|6|7|8|9|10|0|1|2|3|4" },
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

   static const struct retro_controller_info ports[] = {{ port_1, 3 },{ NULL,0 }};

   cb(RETRO_ENVIRONMENT_SET_VARIABLES, variables);
   cb(RETRO_ENVIRONMENT_SET_CONTROLLER_INFO, (void*)ports);
}

void retro_get_system_info(struct retro_system_info *info)
{
   info->need_fullpath = false;
   info->valid_extensions = "gba";
#ifdef GIT_VERSION
   info->library_version = "2.0.2" GIT_VERSION;
#else
   info->library_version = "2.0.2-GIT";
#endif
   info->library_name = "VBA-M";
   info->block_extract = false;
}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
   info->geometry.base_width   = 240;
   info->geometry.base_height  = 160;
   info->geometry.max_width    = 240;
   info->geometry.max_height   = 160;
   info->geometry.aspect_ratio = 3.0 / 2.0;
   info->timing.fps            = (double)FRAMERATE;
   info->timing.sample_rate    = (double)SAMPLERATE;
}

void retro_init(void)
{
#ifdef _WIN32
   char slash = '\\';
#else
   char slash = '/';
#endif

   struct retro_log_callback log;
   memset(libretro_save_buf, 0xff, sizeof(libretro_save_buf));
   adjust_save_ram();
   environ_cb(RETRO_ENVIRONMENT_GET_CAN_DUPE, &can_dupe);
   if (environ_cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log))
      log_cb = log.log;
   else
      log_cb = NULL;

   const char* dir = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &dir) && dir)
      snprintf(biosfile, sizeof(biosfile), "%s%c%s", dir, slash, "gba_bios.bin");

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

static unsigned serialize_size = 0;

typedef struct {
    char romtitle[256];
    char romid[5];
    int flashSize;
    int saveType;
    int rtcEnabled;
    int mirroringEnabled;
    int useBios;
} ini_t;

static const ini_t gbaover[256] = {
			//romtitle,							    	romid	flash	save	rtc	mirror	bios
			{"2 Games in 1 - Dragon Ball Z - The Legacy of Goku I & II (USA)",	"BLFE",	0,	1,	0,	0,	0},
			{"2 Games in 1 - Dragon Ball Z - Buu's Fury + Dragon Ball GT - Transformation (USA)", "BUFE", 0, 1, 0, 0, 0},
			{"Boktai - The Sun Is in Your Hand (Europe)(En,Fr,De,Es,It)",		"U3IP",	0,	0,	1,	0,	0},
			{"Boktai - The Sun Is in Your Hand (USA)",				"U3IE",	0,	0,	1,	0,	0},
			{"Boktai 2 - Solar Boy Django (USA)",					"U32E",	0,	0,	1,	0,	0},
			{"Boktai 2 - Solar Boy Django (Europe)(En,Fr,De,Es,It)",		"U32P",	0,	0,	1,	0,	0},
			{"Bokura no Taiyou - Taiyou Action RPG (Japan)",			"U3IJ",	0,	0,	1,	0,	0},
			{"Card e-Reader+ (Japan)",						"PSAJ",	131072,	0,	0,	0,	0},
			{"Classic NES Series - Bomberman (USA, Europe)",			"FBME",	0,	1,	0,	1,	0},
			{"Classic NES Series - Castlevania (USA, Europe)",			"FADE",	0,	1,	0,	1,	0},
			{"Classic NES Series - Donkey Kong (USA, Europe)",			"FDKE",	0,	1,	0,	1,	0},
			{"Classic NES Series - Dr. Mario (USA, Europe)",			"FDME",	0,	1,	0,	1,	0},
			{"Classic NES Series - Excitebike (USA, Europe)",			"FEBE",	0,	1,	0,	1,	0},
			{"Classic NES Series - Legend of Zelda (USA, Europe)",			"FZLE",	0,	1,	0,	1,	0},
			{"Classic NES Series - Ice Climber (USA, Europe)",			"FICE",	0,	1,	0,	1,	0},
			{"Classic NES Series - Metroid (USA, Europe)",				"FMRE",	0,	1,	0,	1,	0},
			{"Classic NES Series - Pac-Man (USA, Europe)",				"FP7E",	0,	1,	0,	1,	0},
			{"Classic NES Series - Super Mario Bros. (USA, Europe)",		"FSME",	0,	1,	0,	1,	0},
			{"Classic NES Series - Xevious (USA, Europe)",				"FXVE",	0,	1,	0,	1,	0},
			{"Classic NES Series - Zelda II - The Adventure of Link (USA, Europe)",	"FLBE",	0,	1,	0,	1,	0},
			{"Digi Communication 2 - Datou! Black Gemagema Dan (Japan)",		"BDKJ",	0,	1,	0,	0,	0},
			{"e-Reader (USA)",							"PSAE",	131072,	0,	0,	0,	0},
			{"Dragon Ball GT - Transformation (USA)",				"BT4E",	0,	1,	0,	0,	0},
			{"Dragon Ball Z - Buu's Fury (USA)",					"BG3E",	0,	1,	0,	0,	0},
			{"Dragon Ball Z - Taiketsu (Europe)(En,Fr,De,Es,It)",			"BDBP",	0,	1,	0,	0,	0},
			{"Dragon Ball Z - Taiketsu (USA)",					"BDBE",	0,	1,	0,	0,	0},
			{"Dragon Ball Z - The Legacy of Goku II International (Japan)",		"ALFJ",	0,	1,	0,	0,	0},
			{"Dragon Ball Z - The Legacy of Goku II (Europe)(En,Fr,De,Es,It)",	"ALFP", 0,	1,	0,	0,	0},
			{"Dragon Ball Z - The Legacy of Goku II (USA)",				"ALFE",	0,	1,	0,	0,	0},
			{"Dragon Ball Z - The Legacy Of Goku (Europe)(En,Fr,De,Es,It)",		"ALGP",	0,	1,	0,	0,	0},
			{"Dragon Ball Z - The Legacy of Goku (USA)",				"ALGE",	131072,	1,	0,	0,	0},
			{"F-Zero - Climax (Japan)",						"BFTJ",	131072,	0,	0,	0,	0},
			{"Famicom Mini Vol. 01 - Super Mario Bros. (Japan)",			"FMBJ",	0,	1,	0,	1,	0},
			{"Famicom Mini Vol. 12 - Clu Clu Land (Japan)",				"FCLJ",	0,	1,	0,	1,	0},
			{"Famicom Mini Vol. 13 - Balloon Fight (Japan)",			"FBFJ",	0,	1,	0,	1,	0},
			{"Famicom Mini Vol. 14 - Wrecking Crew (Japan)",			"FWCJ",	0,	1,	0,	1,	0},
			{"Famicom Mini Vol. 15 - Dr. Mario (Japan)",				"FDMJ",	0,	1,	0,	1,	0},
			{"Famicom Mini Vol. 16 - Dig Dug (Japan)",				"FTBJ",	0,	1,	0,	1,	0},
			{"Famicom Mini Vol. 17 - Takahashi Meijin no Boukenjima (Japan)",	"FTBJ",	0,	1,	0,	1,	0},
			{"Famicom Mini Vol. 18 - Makaimura (Japan)",				"FMKJ",	0,	1,	0,	1,	0},
			{"Famicom Mini Vol. 19 - Twin Bee (Japan)",				"FTWJ",	0,	1,	0,	1,	0},
			{"Famicom Mini Vol. 20 - Ganbare Goemon! Karakuri Douchuu (Japan)",	"FGGJ",	0,	1,	0,	1,	0},
			{"Famicom Mini Vol. 21 - Super Mario Bros. 2 (Japan)",			"FM2J",	0,	1,	0,	1,	0},
			{"Famicom Mini Vol. 22 - Nazo no Murasame Jou (Japan)",			"FNMJ",	0,	1,	0,	1,	0},
			{"Famicom Mini Vol. 23 - Metroid (Japan)",				"FMRJ",	0,	1,	0,	1,	0},
			{"Famicom Mini Vol. 24 - Hikari Shinwa - Palthena no Kagami (Japan)",	"FPTJ",	0,	1,	0,	1,	0},
			{"Famicom Mini Vol. 25 - The Legend of Zelda 2 - Link no Bouken (Japan)","FLBJ",0,	1,	0,	1,	0},
			{"Famicom Mini Vol. 26 - Famicom Mukashi Banashi - Shin Onigashima - Zen Kou Hen (Japan)","FFMJ",0,1,0,	1,	0},
			{"Famicom Mini Vol. 27 - Famicom Tantei Club - Kieta Koukeisha - Zen Kou Hen (Japan)","FTKJ",0,1,0,	1,	0},
			{"Famicom Mini Vol. 28 - Famicom Tantei Club Part II - Ushiro ni Tatsu Shoujo - Zen Kou Hen (Japan)","FTUJ",0,1,0,1,0},
			{"Famicom Mini Vol. 29 - Akumajou Dracula (Japan)",			"FADJ",	0,	1,	0,	1,	0},
			{"Famicom Mini Vol. 30 - SD Gundam World - Gachapon Senshi Scramble Wars (Japan)","FSDJ",0,1,	0,	1,	0},
			{"Game Boy Wars Advance 1+2 (Japan)",					"BGWJ",	131072,	0,	0,	0,	0},
			{"Golden Sun - The Lost Age (USA)",					"AGFE",	65536,	0,	0,	1,	0},
			{"Golden Sun (USA)",							"AGSE",	65536,	0,	0,	1,	0},
			{"Iridion II (Europe) (En,Fr,De)",							"AI2P",	0,	5,	0,	0,	0},
			{"Iridion II (USA)",							"AI2E",	0,	5,	0,	0,	0},
			{"Koro Koro Puzzle - Happy Panechu! (Japan)",				"KHPJ",	0,	4,	0,	0,	0},
			{"Mario vs. Donkey Kong (Europe)",					"BM5P",	0,	3,	0,	0,	0},
			{"Pocket Monsters - Emerald (Japan)",					"BPEJ",	131072,	0,	1,	0,	0},
			{"Pocket Monsters - Fire Red (Japan)",					"BPRJ",	131072,	0,	0,	0,	0},
			{"Pocket Monsters - Leaf Green (Japan)",				"BPGJ",	131072,	0,	0,	0,	0},
			{"Pocket Monsters - Ruby (Japan)",					"AXVJ",	131072,	0,	1,	0,	0},
			{"Pocket Monsters - Sapphire (Japan)",					"AXPJ",	131072,	0,	1,	0,	0},
			{"Pokemon Mystery Dungeon - Red Rescue Team (USA, Australia)",		"B24E",	131072,	0,	0,	0,	0},
			{"Pokemon Mystery Dungeon - Red Rescue Team (En,Fr,De,Es,It)",		"B24P",	131072,	0,	0,	0,	0},
			{"Pokemon - Blattgruene Edition (Germany)",				"BPGD",	131072,	0,	0,	0,	0},
			{"Pokemon - Edicion Rubi (Spain)",					"AXVS",	131072,	0,	1,	0,	0},
			{"Pokemon - Edicion Esmeralda (Spain)",					"BPES",	131072,	0,	1,	0,	0},
			{"Pokemon - Edicion Rojo Fuego (Spain)",				"BPRS",	131072,	1,	0,	0,	0},
			{"Pokemon - Edicion Verde Hoja (Spain)",				"BPGS",	131072,	1,	0,	0,	0},
			{"Pokemon - Eidicion Zafiro (Spain)",					"AXPS",	131072,	0,	1,	0,	0},
			{"Pokemon - Emerald Version (USA, Europe)",				"BPEE",	131072,	0,	1,	0,	0},
			{"Pokemon - Feuerrote Edition (Germany)",				"BPRD",	131072,	0,	0,	0,	0},
			{"Pokemon - Fire Red Version (USA, Europe)",				"BPRE",	131072,	0,	0,	0,	0},
			{"Pokemon - Leaf Green Version (USA, Europe)",				"BPGE",	131072,	0,	0,	0,	0},
			{"Pokemon - Rubin Edition (Germany)",					"AXVD",	131072,	0,	1,	0,	0},
			{"Pokemon - Ruby Version (USA, Europe)",				"AXVE",	131072,	0,	1,	0,	0},
			{"Pokemon - Sapphire Version (USA, Europe)",				"AXPE",	131072,	0,	1,	0,	0},
			{"Pokemon - Saphir Edition (Germany)",					"AXPD",	131072,	0,	1,	0,	0},
			{"Pokemon - Smaragd Edition (Germany)",					"BPED",	131072,	0,	1,	0,	0},
			{"Pokemon - Version Emeraude (France)",					"BPEF",	131072,	0,	1,	0,	0},
			{"Pokemon - Version Rouge Feu (France)",				"BPRF",	131072,	0,	0,	0,	0},
			{"Pokemon - Version Rubis (France)",					"AXVF",	131072,	0,	1,	0,	0},
			{"Pokemon - Version Saphir (France)",					"AXPF",	131072,	0,	1,	0,	0},
			{"Pokemon - Version Vert Feuille (France)",				"BPGF",	131072,	0,	0,	0,	0},
			{"Pokemon - Versione Rubino (Italy)",					"AXVI",	131072,	0,	1,	0,	0},
			{"Pokemon - Versione Rosso Fuoco (Italy)",				"BPRI",	131072,	0,	0,	0,	0},
			{"Pokemon - Versione Smeraldo (Italy)",					"BPEI",	131072,	0,	1,	0,	0},
			{"Pokemon - Versione Verde Foglia (Italy)",				"BPGI",	131072,	0,	0,	0,	0},
			{"Pokemon - Versione Zaffiro (Italy)",					"AXPI",	131072,	0,	1,	0,	0},
			{"Rockman EXE 4.5 - Real Operation (Japan)",				"BR4J",	0,	0,	1,	0,	0},
			{"Rocky (Europe)(En,Fr,De,Es,It)",					"AROP",	0,	1,	0,	0,	0},
			{"Rocky (USA)(En,Fr,De,Es,It)",						"AR8e",	0,	1,	0,	0,	0},
			{"Sennen Kazoku (Japan)",						"BKAJ",	131072,	0,	1,	0,	0},
			{"Shin Bokura no Taiyou - Gyakushuu no Sabata (Japan)",			"U33J",	0,	1,	1,	0,	0},
			{"Super Mario Advance 4 (Japan)",					"AX4J",	131072,	0,	0,	0,	0},
			{"Super Mario Advance 4 - Super Mario Bros. 3 (Europe)(En,Fr,De,Es,It)","AX4P",	131072,	0,	0,	0,	0},
			{"Super Mario Advance 4 - Super Mario Bros 3 - Super Mario Advance 4 v1.1 (USA)","AX4E",131072,0,0,0,0},
			{"Top Gun - Combat Zones (USA)(En,Fr,De,Es,It)",			"A2YE",	0,	5,	0,	0,	0},
			{"Yoshi's Universal Gravitation (Europe)(En,Fr,De,Es,It)",		"KYGP",	0,	4,	0,	0,	0},
			{"Yoshi no Banyuuinryoku (Japan)",					"KYGJ",	0,	4,	0,	0,	0},
			{"Yoshi - Topsy-Turvy (USA)",						"KYGE",	0,	1,	0,	0,	0},
			{"Yu-Gi-Oh! GX - Duel Academy (USA)",					"BYGE",	0,	2,	0,	0,	1},
			{"Yu-Gi-Oh! - Ultimate Masters - 2006 (Europe)(En,Jp,Fr,De,Es,It)",	"BY6P",	0,	2,	0,	0,	0},
			{"Zoku Bokura no Taiyou - Taiyou Shounen Django (Japan)",		"U32J",	0,	0,	1,	0,	0}
};

static void load_image_preferences(void)
{
    char buffer[5];
    buffer[0] = rom[0xac];
    buffer[1] = rom[0xad];
    buffer[2] = rom[0xae];
    buffer[3] = rom[0xaf];
    buffer[4] = 0;

    if (log_cb)
        log_cb(RETRO_LOG_INFO, "GameID in ROM is: %s\n", buffer);

    bool found = false;
    int found_no = 0;

    for (int i = 0; i < 256; i++) {
        if (!strcmp(gbaover[i].romid, buffer)) {
            found = true;
            found_no = i;
            break;
        }
    }

    if (found) {
        if (log_cb)
            log_cb(RETRO_LOG_INFO, "Found ROM in vba-over list.\n");

        enableRtc = gbaover[found_no].rtcEnabled;

        if (gbaover[found_no].flashSize != 0)
            flashSize = gbaover[found_no].flashSize;
        else
            flashSize = 65536;

        cpuSaveType = gbaover[found_no].saveType;

        mirroringEnable = gbaover[found_no].mirroringEnabled;
    }

    if (log_cb) {
        log_cb(RETRO_LOG_INFO, "RTC = %d.\n", enableRtc);
        log_cb(RETRO_LOG_INFO, "flashSize = %d.\n", flashSize);
        log_cb(RETRO_LOG_INFO, "cpuSaveType = %d.\n", cpuSaveType);
        log_cb(RETRO_LOG_INFO, "mirroringEnable = %d.\n", mirroringEnable);
    }
}

static void gba_init(void)
{
    cpuSaveType = 0;
    flashSize = 0x10000;
    enableRtc = false;
    mirroringEnable = false;

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

    load_image_preferences();

    saveType = cpuSaveType;

    if (flashSize == 0x10000 || flashSize == 0x20000)
        flashSetSize(flashSize);

    rtcEnabled = enableRtc;
    if (enableRtc)
        rtcEnable(enableRtc);

    doMirroring(mirroringEnable);

    soundInit();
    soundSetSampleRate(32000);

    CPUInit(biosfile, usebios);

    CPUReset();

    soundReset();

    uint8_t* state_buf = (uint8_t*)malloc(2000000);
    serialize_size = CPUWriteState(state_buf, 2000000);
    free(state_buf);

    emulating = 1;
}

static void gba_soundchanged(void)
{
    soundInterpolation = sndInterpolation;
    soundFiltering     = sndFiltering;
}

void retro_deinit(void)
{
    emulating = 0;
    CPUCleanUp();
    soundShutdown();
}

void retro_reset(void)
{
    CPUReset();
}

#define MAX_BUTTONS 10
static const unsigned binds[3][MAX_BUTTONS] = {
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

static void systemUpdateSolarSensor(int level);

static unsigned has_frame;
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
    for (int i = 0; i < 8; i++)
    {
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
    for (unsigned i = 0; i < 6; i++)
    {
        key[strlen("vbam_sound_")] = '1' + i;
        var.value = NULL;
        if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value && var.value[0] == 'd')
        {
            unsigned which = (i < 4) ? (1 << i) : (0x100 << (i - 4));
            sound_enabled &= ~(which);
        }
    }

    if (soundGetEnable() != sound_enabled)
        soundSetEnable(sound_enabled & 0x30F);

    var.key = "vbam_soundinterpolation";
    var.value = NULL;

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
    {
        bool newval = (strcmp(var.value, "enabled") == 0);
        if (sndInterpolation != newval)
        {
            sndInterpolation = newval;
            sound_changed = true;
        }
    }

    var.key = "vbam_soundfiltering";
    var.value = NULL;

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
    {
        float newval = atof(var.value) * 0.1f;
        if (sndFiltering != newval)
        {
            sndFiltering = newval;
            sound_changed = true;
        }
    }

    if (sound_changed)
        //Update interpolation and filtering values
        gba_soundchanged();

    var.key = "vbam_usebios";
    var.value = NULL;

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
    {
        bool newval = (strcmp(var.value, "enabled") == 0);
        usebios = newval;
    }

    var.key = "vbam_solarsensor";
    var.value = NULL;

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
    {
        sensorDarknessLevel = atoi(var.value);
        systemUpdateSolarSensor(sensorDarknessLevel);
    }
}

#ifdef FINAL_VERSION
#define TICKS 250000
#else
#define TICKS 5000
#endif

void retro_run(void)
{
    bool updated = false;
    static bool buttonpressed = false;
    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated)
        update_variables();

    poll_cb();

    // Update solar sensor level by gamepad buttons, default L2/R2
    if (buttonpressed)
    {
        buttonpressed = input_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2) ||
            input_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2);
    }
    else
    {
        if (input_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2))
        {
            sensorDarknessLevel++;
            if (sensorDarknessLevel > 10)
                sensorDarknessLevel = 10;
            systemUpdateSolarSensor(sensorDarknessLevel);
            buttonpressed = true;
        }
        else if (input_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2))
        {
            if (sensorDarknessLevel)
                sensorDarknessLevel--;
            systemUpdateSolarSensor(sensorDarknessLevel);
            buttonpressed = true;
        }
    } // end of solar sensor update

    has_frame = 0;

    do {
        CPULoop(TICKS);
    } while (!has_frame);
}

size_t retro_serialize_size(void)
{
    return serialize_size;
}

bool retro_serialize(void* data, size_t size)
{
    return CPUWriteState((uint8_t*)data, size);
}

bool retro_unserialize(const void* data, size_t size)
{
    return CPUReadState((uint8_t*)data, size);
}

void retro_cheat_reset(void)
{
    cheatsEnabled = 1;
    cheatsDeleteAll(false);
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
   std::string codeLine=code;
   std::string name="cheat_"+index;
   int matchLength=0;
   std::vector<std::string> codeParts;
   int cursor;

    //Break the code into Parts
    for (cursor=0;;cursor++)
    {
      if (ISHEXDEC){
         matchLength++;
      } else {
         if (matchLength){
            if (matchLength>8){
               codeParts.push_back(codeLine.substr(cursor-matchLength,8));
               codeParts.push_back(codeLine.substr(cursor-matchLength+8,matchLength-8));

            } else {
               codeParts.push_back(codeLine.substr(cursor-matchLength,matchLength));
            }
            matchLength=0;
         }
      }
      if (!codeLine[cursor]){
         break;
      }
    }

   //Add to core
   for (cursor=0;cursor<codeParts.size();cursor+=2){
      std::string codeString;
      codeString+=codeParts[cursor];

      if (codeParts[cursor+1].length()==8){
         codeString+=codeParts[cursor+1];
         cheatsAddGSACode(codeString.c_str(),name.c_str(),true);
      } else if (codeParts[cursor+1].length()==4) {
         codeString+=" ";
         codeString+=codeParts[cursor+1];
         cheatsAddCBACode(codeString.c_str(),name.c_str());
      } else {
         codeString+=" ";
         codeString+=codeParts[cursor+1];
         log_cb(RETRO_LOG_ERROR, "[VBA] Invalid cheat code '%s'\n", codeString.c_str());
      }
      log_cb(RETRO_LOG_INFO, "[VBA] Cheat code added: '%s'\n", codeString.c_str());
   }
}

bool retro_load_game(const struct retro_game_info *game)
{
   update_variables();

   struct retro_input_descriptor input_desc[] = {
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

      { 0 },
   };

   environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, input_desc);

   int romSize = CPULoadRomData((const char*)game->data, game->size);
   if (!romSize)
      return false;

   gba_init();

   struct retro_memory_descriptor desc[11];
   memset(desc, 0, sizeof(desc));

   desc[0].start=0x03000000; desc[0].select=0xFF000000; desc[0].len=0x8000;    desc[0].ptr=internalRAM;//fast WRAM
   desc[1].start=0x02000000; desc[1].select=0xFF000000; desc[1].len=0x40000;   desc[1].ptr=workRAM;//slow WRAM
   /* TODO: if SRAM is flash, use start=0 addrspace="S" instead */
   desc[2].start=0x0E000000; desc[2].select=0;          desc[2].len=libretro_save_size; desc[2].ptr=flashSaveMemory;//SRAM
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
   struct retro_memory_map retromap={ desc, sizeof(desc)/sizeof(*desc) };
   environ_cb(RETRO_ENVIRONMENT_SET_MEMORY_MAPS, &retromap);

   bool yes = true;
   environ_cb(RETRO_ENVIRONMENT_SET_SUPPORT_ACHIEVEMENTS, &yes);

   return true;
}

bool retro_load_game_special(
    unsigned game_type,
    const struct retro_game_info *info, size_t num_info)
{
    return false;
}

extern unsigned g_audio_frames;
static unsigned g_video_frames;

void retro_unload_game(void)
{
    if (log_cb)
        log_cb(RETRO_LOG_INFO, "[VBA] Sync stats: Audio frames: %u, Video frames: %u, AF/VF: %.2f\n",
            g_audio_frames, g_video_frames, (float)g_audio_frames / g_video_frames);
    g_audio_frames = 0;
    g_video_frames = 0;
}

unsigned retro_get_region(void)
{
    return RETRO_REGION_NTSC;
}

void systemOnWriteDataToSoundBuffer(const uint16_t* finalWave, int length)
{
}

void systemOnSoundShutdown()
{

}

bool systemCanChangeSoundQuality()
{
    return true;
}

#ifdef FRONTEND_SUPPORTS_RGB565
#define BPP 2
#else
#define BPP 4
#endif

void systemDrawScreen()
{
    video_cb(pix, 240, 160, 240 * BPP);
    g_video_frames++;
}

void systemFrame()
{
    has_frame = 1;
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

    unsigned ControllerLayout = controller_layout;
    uint32_t J = 0;

    for (unsigned i = 0; i < MAX_BUTTONS; i++)
        J |= input_cb(which, RETRO_DEVICE_JOYPAD, 0, binds[ControllerLayout][i]) << i;

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
    switch (v)
    {
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

bool systemReadJoypads()
{
    return true;
}

void systemUpdateMotionSensor()
{
}

uint8_t systemGetSensorDarkness()
{
    return sensorDarkness;
}

void systemCartridgeRumble(bool)
{
}

bool systemPauseOnFrame() { return false; }
void systemGbPrint(uint8_t* data, int pages, int feed, int palette, int contrast) {}
void systemScreenCapture(int a) {}
void systemScreenMessage(const char* msg)
{
    if (log_cb)
        log_cb(RETRO_LOG_INFO, "%s\n", msg);
}

void systemSetTitle(const char* title) {}
void systemShowSpeed(int speed) {}
void system10Frames(int rate) {}

uint32_t systemGetClock()
{
    return 0;
}

SoundDriver* systemSoundInit()
{
    soundShutdown();
    return new SoundRetro();
}
