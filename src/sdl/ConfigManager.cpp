// necessary to get portable strerror_r
#undef _GNU_SOURCE
#include <string.h>
#define _GNU_SOURCE 1

#include "ConfigManager.h"
extern "C" {
#include "../common/iniparser.h"
}

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <cmath>
#include <cerrno>

#include "../common/Patch.h"
#include "../gba/GBA.h"
#include "../gba/agbprint.h"
#include "../gba/Flash.h"
#include "../gba/Cheats.h"
#include "../gba/remote.h"
#include "../gba/RTC.h"
#include "../gba/Sound.h"
#include "../gb/gb.h"
#include "../gb/gbGlobals.h"
#include "../gb/gbCheats.h"
#include "../gb/gbSound.h"
#include "../Util.h"

#ifndef _WIN32
#define GETCWD getcwd
#else // _WIN32
#include <direct.h>
#define GETCWD _getcwd
#define stat _stat
#define mkdir(X,Y) (_mkdir(X))
// from: https://www.linuxquestions.org/questions/programming-9/porting-to-win32-429334/
#ifndef S_ISDIR
    #define S_ISDIR(mode)  (((mode) & _S_IFMT) == _S_IFDIR)
#endif
#endif // _WIN32

#ifndef __GNUC__
#define HAVE_DECL_GETOPT 0
#define __STDC__ 1
#include "getopt.h"
#else // ! __GNUC__
#define HAVE_DECL_GETOPT 1
#include <getopt.h>
#endif // ! __GNUC__
#include <string>
#include <sstream>

enum named_opts
{
	OPT_AGB_PRINT = 1000,
	OPT_AUTOFIRE,
	OPT_AUTO_FRAME_SKIP,
	OPT_BATTERY_DIR,
	OPT_BIOS_FILE_NAME_GB,
	OPT_BIOS_FILE_NAME_GBA,
	OPT_BIOS_FILE_NAME_GBC,
	OPT_CAPTURE_FORMAT,
	OPT_CHEAT,
	OPT_CPU_SAVE_TYPE,
	OPT_DOTCODE_FILE_NAME_LOAD,
	OPT_DOTCODE_FILE_NAME_SAVE,
	OPT_GB_BORDER_AUTOMATIC,
	OPT_GB_BORDER_ON,
	OPT_GB_COLOR_OPTION,
	OPT_GB_EMULATOR_TYPE,
	OPT_GB_FRAME_SKIP,
	OPT_GB_PALETTE_OPTION,
	OPT_IFB_TYPE,
	OPT_OPT_FLASH_SIZE,
	OPT_REWIND_TIMER,
	OPT_RTC_ENABLED,
	OPT_SAVE_DIR,
	OPT_SCREEN_SHOT_DIR,
	OPT_SHOW_SPEED,
	OPT_SHOW_SPEED_TRANSPARENT,
	OPT_SOUND_FILTERING,
	OPT_SPEEDUP_THROTTLE,
	OPT_SPEEDUP_FRAME_SKIP,
	OPT_NO_SPEEDUP_THROTTLE_FRAME_SKIP
};

#define SOUND_MAX_VOLUME 2.0
#define SOUND_ECHO       0.2
#define SOUND_STEREO     0.15

#define REWIND_NUM 8
#define REWIND_SIZE 400000

char path[2048];

dictionary* preferences;

const char* batteryDir;
const char* biosFileNameGB;
const char* biosFileNameGBA;
const char* biosFileNameGBC;
const char* saveDir;
const char* screenShotDir;
int agbPrint;
int autoFireMaxCount = 1;
int autoFrameSkip = 0;
int autoPatch;
int captureFormat = 0;
int disableStatusMessages = 0;
int filter = kStretch2x;
int frameSkip = 1;
int fullScreen;
int ifbType = kIFBNone;
int openGL;
int optFlashSize;
int optPrintUsage;
int pauseWhenInactive = 0;
int preparedCheats = 0;
int rewindTimer = 0;
int showSpeed;
int showSpeedTransparent;

const char* preparedCheatCodes[MAX_CHEATS];

// allow up to 100 IPS/UPS/PPF patches given on commandline
int	patchNum = 0;
char *patchNames[PATCH_MAX_NUM] = { NULL }; // and so on

#ifndef NO_DEBUGGER
void(*dbgMain)() = remoteStubMain;
void(*dbgSignal)(int, int) = remoteStubSignal;
void(*dbgOutput)(const char *, uint32_t) = debuggerOutput;
#endif

char* arg0 = NULL;


struct option argOptions[] = {
	{ "agb-print", required_argument, 0, OPT_AGB_PRINT },
	{ "auto-frame-skip", required_argument, 0, OPT_AUTO_FRAME_SKIP },
	{ "auto-patch", no_argument, &autoPatch, 1 },
	{ "autofire", required_argument, 0, OPT_AUTOFIRE },
	{ "battery-dir", required_argument, 0, OPT_BATTERY_DIR },
	{ "bios", required_argument, 0, 'b' },
	{ "bios-file-name-gb", required_argument, 0, OPT_BIOS_FILE_NAME_GB },
	{ "bios-file-name-gba", required_argument, 0, OPT_BIOS_FILE_NAME_GBA },
	{ "bios-file-name-gbc", required_argument, 0, OPT_BIOS_FILE_NAME_GBC },
	{ "border-automatic", no_argument, 0, OPT_GB_BORDER_AUTOMATIC },
	{ "border-on", no_argument, 0, OPT_GB_BORDER_ON },
	{ "capture-format", required_argument, 0, OPT_CAPTURE_FORMAT },
	{ "cheat", required_argument, 0, OPT_CHEAT },
	{ "cheats-enabled", no_argument, &coreOptions.cheatsEnabled, 1 },
	{ "color-option", no_argument, 0, OPT_GB_COLOR_OPTION },
	{ "config", required_argument, 0, 'c' },
	{ "cpu-disable-sfx", no_argument, &coreOptions.cpuDisableSfx, 1 },
	{ "cpu-save-type", required_argument, 0, OPT_CPU_SAVE_TYPE },
	{ "debug", no_argument, 0, 'd' },
	{ "disable-sfx", no_argument, &coreOptions.cpuDisableSfx, 1 },
	{ "disable-status-messages", no_argument, &disableStatusMessages, 1 },
	{ "dotcode-file-name-load", required_argument, 0, OPT_DOTCODE_FILE_NAME_LOAD },
	{ "dotcode-file-name-save", required_argument, 0, OPT_DOTCODE_FILE_NAME_SAVE },
	{ "filter", required_argument, 0, 'f' },
	{ "flash-128k", no_argument, &optFlashSize, 1 },
	{ "flash-64k", no_argument, &optFlashSize, 0 },
	{ "flash-size", required_argument, 0, 'S' },
	{ "frameskip", required_argument, 0, 's' },
	{ "full-screen", no_argument, &fullScreen, 1 },
	{ "gb-border-automatic", no_argument, 0, OPT_GB_BORDER_AUTOMATIC },
	{ "gb-border-on", no_argument, 0, OPT_GB_BORDER_ON },
	{ "gb-color-option", no_argument, 0, OPT_GB_COLOR_OPTION },
	{ "gb-emulator-type", required_argument, 0, OPT_GB_EMULATOR_TYPE },
	{ "gb-frame-skip", required_argument, 0, OPT_GB_FRAME_SKIP },
	{ "gb-palette-option", required_argument, 0, OPT_GB_PALETTE_OPTION },
	{ "gb-printer", no_argument, &coreOptions.winGbPrinterEnabled, 1 },
	{ "gdb", required_argument, 0, 'G' },
	{ "help", no_argument, &optPrintUsage, 1 },
	{ "ifb-filter", required_argument, 0, 'I' },
	{ "ifb-type", required_argument, 0, OPT_IFB_TYPE },
	{ "no-agb-print", no_argument, &agbPrint, 0 },
	{ "no-auto-frameskip", no_argument, &autoFrameSkip, 0 },
	{ "no-debug", no_argument, 0, 'N' },
	{ "no-opengl", no_argument, &openGL, 0 },
	{ "no-patch", no_argument, &autoPatch, 0 },
	{ "no-pause-when-inactive", no_argument, &pauseWhenInactive, 0 },
	{ "no-rtc", no_argument, &coreOptions.rtcEnabled, 0 },
	{ "no-show-speed", no_argument, &showSpeed, 0 },
	{ "opengl", required_argument, 0, 'O' },
	{ "opengl-bilinear", no_argument, &openGL, 2 },
	{ "opengl-nearest", no_argument, &openGL, 1 },
	{ "opt-flash-size", required_argument, 0, OPT_OPT_FLASH_SIZE },
	{ "patch", required_argument, 0, 'i' },
	{ "pause-when-inactive", no_argument, &pauseWhenInactive, 1 },
	{ "profile", optional_argument, 0, 'p' },
	{ "rewind-timer", required_argument, 0, OPT_REWIND_TIMER },
	{ "rtc", no_argument, &coreOptions.rtcEnabled, 1 },
	{ "rtc-enabled", required_argument, 0, OPT_RTC_ENABLED },
	{ "save-auto", no_argument, &coreOptions.cpuSaveType, 0 },
	{ "save-dir", required_argument, 0, OPT_SAVE_DIR },
	{ "save-eeprom", no_argument, &coreOptions.cpuSaveType, 1 },
	{ "save-flash", no_argument, &coreOptions.cpuSaveType, 3 },
	{ "save-none", no_argument, &coreOptions.cpuSaveType, 5 },
	{ "save-sensor", no_argument, &coreOptions.cpuSaveType, 4 },
	{ "save-sram", no_argument, &coreOptions.cpuSaveType, 2 },
	{ "save-type", required_argument, 0, 't' },
	{ "screen-shot-dir", required_argument, 0, OPT_SCREEN_SHOT_DIR },
	{ "show-speed", required_argument, 0, OPT_SHOW_SPEED },
	{ "show-speed-detailed", no_argument, &showSpeed, 2 },
	{ "show-speed-normal", no_argument, &showSpeed, 1 },
	{ "show-speed-transparent", required_argument, 0, OPT_SHOW_SPEED_TRANSPARENT },
	{ "skip-bios", no_argument, &coreOptions.skipBios, 1 },
	{ "skip-save-game-battery", no_argument, &coreOptions.skipSaveGameBattery, 1 },
	{ "skip-save-game-cheats", no_argument, &coreOptions.skipSaveGameCheats, 1 },
	{ "sound-filtering", required_argument, 0, OPT_SOUND_FILTERING },
	{ "throttle", required_argument, 0, 'T' },
	{ "speedup-throttle", required_argument, 0, OPT_SPEEDUP_THROTTLE },
	{ "speedup-frame-skip", required_argument, 0, OPT_SPEEDUP_FRAME_SKIP },
	{ "no-speedup-throttle-frame-skip", no_argument, 0, OPT_NO_SPEEDUP_THROTTLE_FRAME_SKIP },
	{ "use-bios", no_argument, &coreOptions.useBios, 1 },
	{ "verbose", required_argument, 0, 'v' },
	{ "win-gb-printer-enabled", no_argument, &coreOptions.winGbPrinterEnabled, 1 },


	{ NULL, no_argument, NULL, 0 }
};


uint32_t fromHex(const char *s)
{
	if (!s)
		return 0;
	uint32_t value;
	sscanf(s, "%x", &value);
	return value;
}

uint32_t fromDec(const char *s)
{
	if (!s)
		return 0;
	uint32_t value = 0;
	sscanf(s, "%u", &value);
	return value;
}

void SetHome(char *_arg0)
{
	arg0 = _arg0;
}

void OpenPreferences(const char *name)
{
	if (!preferences && name)
		preferences = iniparser_load(name);
}

void ValidateConfig()
{
	if (gbEmulatorType > 5)
		gbEmulatorType = 1;
	if (gbPaletteOption > 2)
		gbPaletteOption = 0;
	if (frameSkip < 0 || frameSkip > 9)
		frameSkip = 2;
	if (gbFrameSkip < 0 || gbFrameSkip > 9)
		gbFrameSkip = 0;
	if (filter < kStretch1x || filter >= kInvalidFilter)
		filter = kStretch2x;

	if (coreOptions.cpuSaveType < 0 || coreOptions.cpuSaveType > 5)
		coreOptions.cpuSaveType = 0;
	if (optFlashSize != 0 && optFlashSize != 1)
		optFlashSize = 0;
	if (ifbType < kIFBNone || ifbType >= kInvalidIFBFilter)
		ifbType = kIFBNone;
	if (showSpeed < 0 || showSpeed > 2)
		showSpeed = 1;
	if (rewindTimer < 0 || rewindTimer > 600)
		rewindTimer = 0;
	if (autoFireMaxCount < 1)
		autoFireMaxCount = 1;
}

void LoadConfig()
{
	agbPrint = ReadPrefHex("agbPrint");
	autoFireMaxCount = fromDec(ReadPrefString("autoFireMaxCount"));
	autoFrameSkip = ReadPref("autoFrameSkip", 0);
	autoPatch = ReadPref("autoPatch", 1);
	batteryDir = ReadPrefString("batteryDir");
	biosFileNameGB = ReadPrefString("biosFileGB");
	biosFileNameGBA = ReadPrefString("biosFileGBA");
	biosFileNameGBC = ReadPrefString("biosFileGBC");
	captureFormat = ReadPref("captureFormat", 0);
	coreOptions.cheatsEnabled = ReadPref("cheatsEnabled", 0);
	coreOptions.cpuDisableSfx = ReadPref("disableSfx", 0);
	coreOptions.cpuSaveType = ReadPrefHex("saveType");
	disableStatusMessages = ReadPrefHex("disableStatus");
	filter = ReadPref("filter", 0);
	frameSkip = ReadPref("frameSkip", 0);
	fullScreen = ReadPrefHex("fullScreen");
	gbBorderAutomatic = ReadPref("borderAutomatic", 1);
	gbBorderOn = ReadPrefHex("borderOn");
	gbColorOption = ReadPref("colorOption", 0);
	gbEmulatorType = ReadPref("emulatorType", 0);
	gbFrameSkip = ReadPref("gbFrameSkip", 0);
	gbPaletteOption = ReadPref("gbPaletteOption", 0);
	gbSoundSetDeclicking(ReadPref("gbSoundDeclicking", 1));
	gb_effects_config.echo = (float)ReadPref("gbSoundEffectsEcho", 20) / 100.0f;
	gb_effects_config.enabled = ReadPref("gbSoundEffectsEnabled", 0);
	gb_effects_config.stereo = (float)ReadPref("gbSoundEffectsStereo", 15) / 100.0f;
	gb_effects_config.surround = ReadPref("gbSoundEffectsSurround", 0);
	ifbType = ReadPref("ifbType", 0);
	coreOptions.loadDotCodeFile = ReadPrefString("loadDotCodeFile");
	openGL = ReadPrefHex("openGL");
	optFlashSize = ReadPref("flashSize", 0);
	pauseWhenInactive = ReadPref("pauseWhenInactive", 1);
	rewindTimer = ReadPref("rewindTimer", 0);
	coreOptions.rtcEnabled = ReadPref("rtcEnabled", 0);
	saveDir = ReadPrefString("saveDir");
	coreOptions.saveDotCodeFile = ReadPrefString("saveDotCodeFile");
	screenShotDir = ReadPrefString("screenShotDir");
	showSpeed = ReadPref("showSpeed", 0);
	showSpeedTransparent = ReadPref("showSpeedTransparent", 1);
	coreOptions.skipBios = ReadPref("skipBios", 0);
	coreOptions.skipSaveGameBattery = ReadPref("skipSaveGameBattery", 1);
	coreOptions.skipSaveGameCheats = ReadPref("skipSaveGameCheats", 0);
	soundFiltering = (float)ReadPref("gbaSoundFiltering", 50) / 100.0f;
	soundInterpolation = ReadPref("gbaSoundInterpolation", 1);
	coreOptions.throttle = ReadPref("throttle", 100);
	coreOptions.speedup_throttle = ReadPref("speedupThrottle", 100);
	coreOptions.speedup_frame_skip = ReadPref("speedupFrameSkip", 9);
	coreOptions.speedup_throttle_frame_skip = ReadPref("speedupThrottleFrameSkip", 0);
	coreOptions.useBios = ReadPrefHex("useBiosGBA");
	coreOptions.winGbPrinterEnabled = ReadPref("gbPrinter", 0);

	int soundQuality = (ReadPrefHex("soundQuality", 1));
	switch (soundQuality) {
	case 1:
	case 2:
	case 4:
		break;
	default:
		log("Unknown sound quality %d. Defaulting to 44Khz\n", soundQuality);
		soundQuality = 1;
		break;
	}
	soundSetSampleRate(44100 / soundQuality);
	int volume = ReadPref("soundVolume", 100);
	float volume_percent = volume / 100.0f;
	if (volume_percent < 0.0 || volume_percent > SOUND_MAX_VOLUME)
		volume_percent = 1.0;
	soundSetVolume(volume_percent);

	soundSetEnable((ReadPrefHex("soundEnable", 0x30f)) & 0x30f);
	if ((ReadPrefHex("soundStereo"))) {
		gb_effects_config.enabled = true;
	}
	if ((ReadPrefHex("soundEcho"))) {
		gb_effects_config.enabled = true;
	}
	if ((ReadPrefHex("soundSurround"))) {
		gb_effects_config.surround = true;
		gb_effects_config.enabled = true;
	}

	if (optFlashSize == 0)
		flashSetSize(0x10000);
	else
		flashSetSize(0x20000);

	rtcEnable(coreOptions.rtcEnabled ? true : false);
	agbPrintEnable(agbPrint ? true : false);

	for (int i = 0; i < 24;) {
		systemGbPalette[i++] = (0x1f) | (0x1f << 5) | (0x1f << 10);
		systemGbPalette[i++] = (0x15) | (0x15 << 5) | (0x15 << 10);
		systemGbPalette[i++] = (0x0c) | (0x0c << 5) | (0x0c << 10);
		systemGbPalette[i++] = 0;
	}

	systemSaveUpdateCounter = SYSTEM_SAVE_NOT_UPDATED;

	optPrintUsage = 0;

	// TODO
	//int s = ReadPref("mapViewStretch", 0);
	//m_size = ReadPref("memViewerDataSize", 0);
	//m_stretch = ReadPref("GBOamViewStretch", 0);
	//m_stretch = ReadPref("tileViewStretch", 0);
	//numberType = ReadPref("cheatsNumberType", 2);
	//numberType = ReadPref("gbCheatsNumberType", 2);
	//restoreValues = ReadPref("cheatsRestore", 0) ?
	//scale = ReadPref("printerScale", 0);
	//searchType = ReadPref("cheatsSearchType", SEARCH_EQ);
	//searchType = ReadPref("gbCheatsSearchType",
	//selectedFilter = ReadPref(("selectedFilter"), 0);
	//sizeType = ReadPref("cheatsSizeType", 0);
	//sizeType = ReadPref("gbCheatsSizeType", 0);
	//updateValues = ReadPref("cheatsUpdate", 0);
	//updateValues = ReadPref("gbCheatsUpdate", 0);
	//valueType = ReadPref("cheatsValueType", 0);
	//valueType = ReadPref("gbCheatsValueType", 0);

	ValidateConfig();
}

void CloseConfig()
{
	if (preferences)
		iniparser_freedict(preferences);
}

const char* FindConfigFile(const char *name)
{
	char buffer[4096];

#ifdef _WIN32
#define PATH_SEP ";"
#define EXE_NAME "vbam.exe"
#else // ! _WIN32
#define PATH_SEP ":"
#define EXE_NAME "vbam"
#endif // ! _WIN32

	if (GETCWD(buffer, 2048)) {
	}

	if (FileExists(name))
	{
		return name;
	}

	struct stat s;
	std::string homeDirTmp = get_xdg_user_config_home() + DOT_DIR;
	char *fullDir = (char *)homeDirTmp.c_str();
	if (stat(fullDir, &s) == -1 || !S_ISDIR(s.st_mode))
		mkdir(fullDir, 0755);

	if (fullDir) {
		sprintf(path, "%s%c%s", fullDir, FILE_SEP, name);
		if (FileExists(path))
		{
			return path;
		}
	}

#ifdef _WIN32
	char *home = getenv("USERPROFILE");
	if (home != NULL) {
		sprintf(path, "%s%c%s", home, FILE_SEP, name);
		if (FileExists(path))
		{
			return path;
		}
	}

	if (!strchr(arg0, '/') &&
		!strchr(arg0, '\\')) {
		char *env_path = getenv("PATH");

		if (env_path != NULL) {
			strncpy(buffer, env_path, 4096);
			buffer[4095] = 0;
			char *tok = strtok(buffer, PATH_SEP);

			while (tok) {
				sprintf(env_path, "%s%c%s", tok, FILE_SEP, EXE_NAME);
				if (FileExists(env_path)) {
					static char path2[2048];
					sprintf(path2, "%s%c%s", tok, FILE_SEP, name);
					if (FileExists(path2)) {
						return path2;
					}
				}
				tok = strtok(NULL, PATH_SEP);
			}
		}
	}
	else {
		// executable is relative to some directory
		strcpy(buffer, arg0);
		char *p = strrchr(buffer, FILE_SEP);
		if (p) {
			*p = 0;
			sprintf(path, "%s%c%s", buffer, FILE_SEP, name);
			if (FileExists(path))
			{
				return path;
			}
		}
	}
#else // ! _WIN32
	sprintf(path, "%s%c%s", PKGDATADIR, FILE_SEP, name);
	if (FileExists(path))
	{
		return path;
	}

	sprintf(path, "%s%c%s", SYSCONF_INSTALL_DIR, FILE_SEP, name);
	if (FileExists(path))
	{
		return path;
	}
#endif // ! _WIN32
	return 0;
}

void LoadConfigFile()
{
	if (preferences == NULL)
	{
		const char* configFile = FindConfigFile("vbam.ini");
		OpenPreferences(configFile);
	}
}

void SaveConfigFile()
{
	const char* configFile = FindConfigFile("vbam.ini");

	if (configFile != NULL)
	{
		FILE *f = utilOpenFile(configFile, "w");
		if (f == NULL) {
                        char err_msg[4096] = "unknown error";
                        strncpy(err_msg, strerror(errno), 4096);
			fprintf(stderr, "Configuration file '%s' could not be written to: %s\n", configFile, err_msg);
			return;
		}
		// Needs mixed case version of the option name to add new options into the ini
		//for (int i = 0; i < (sizeof(argOptions) / sizeof(option)); i++)
		//{
		//	std::string pref = "preferences:";
		//	pref.append(argOptions[i].name);
		//	if (!iniparser_find_entry(preferences, pref.c_str()))
		//	{
		//		iniparser_set(preferences, pref.c_str(), NULL);
		//	}
		//}
		iniparser_dump_ini(preferences, f);
		fclose(f);
	}
}

uint32_t ReadPrefHex(const char* pref_key, int default_value)
{
    std::stringstream ss;
    std::string default_string;
    ss.setf(std::ios::hex|std::ios::showbase, std::ios::basefield);
    ss << default_value;
    ss >> default_string;
    LoadConfigFile();
    std::string pref = "preferences:";
    pref.append(pref_key);
    return fromHex(iniparser_getstring(preferences, pref.c_str(), default_string.c_str()));
}

uint32_t ReadPrefHex(const char* pref_key)
{
	LoadConfigFile();
	std::string pref = "preferences:";
	pref.append(pref_key);
	return fromHex(iniparser_getstring(preferences, pref.c_str(), 0));
}

uint32_t ReadPref(const char* pref_key, int default_value)
{
	LoadConfigFile();
	std::string pref = "preferences:";
	pref.append(pref_key);
	return iniparser_getint(preferences, pref.c_str(), default_value);
}

uint32_t ReadPref(const char* pref_key)
{
	return ReadPref(pref_key, 0);
}

const char* ReadPrefString(const char* pref_key, const char* default_value)
{
	LoadConfigFile();
	std::string pref = "preferences:";
	pref.append(pref_key);
	return iniparser_getstring(preferences, pref.c_str(), default_value);
}

const char* ReadPrefString(const char* pref_key)
{
	return ReadPrefString(pref_key, "");
}

/*-------------------------------------------------------------------------*/
/**
  @brief    Duplicate a string
  @param    s String to duplicate
  @return   Pointer to a newly allocated string, to be freed with free()

  This is a replacement for strdup(). This implementation is provided
  for systems that do not have it.
 */
/*--------------------------------------------------------------------------*/
static char *xstrdup(const char *s)
{
        char *t;
        if (!s)
                return NULL;
        t = (char *)malloc(strlen(s) + 1);
        if (t) {
                strcpy(t, s);
        }
        return t;
}

int ReadOpts(int argc, char ** argv)
{
	int op = -1;
	while ((op = getopt_long(argc,
		argv,
		"FNO:T:Y:G:I:D:b:c:df:hi:p::s:t:v:",
		argOptions,
		NULL)) != -1) {
		switch (op) {
		case 0:
			// long option already processed by getopt_long
			break;
		case OPT_CHEAT:
			// --cheat
			if (preparedCheats >= MAX_CHEATS) {
				log("Warning: cannot add more than %d cheats.\n", MAX_CHEATS);
				break;
			}
		  {
			  //char* cpy;
			  //cpy = (char *)malloc(1 + strlen(optarg));
			  //strcpy(cpy, optarg);
			  //preparedCheatCodes[preparedCheats++] = cpy;
			std::string cpy = optarg;
			preparedCheatCodes[preparedCheats++] = cpy.c_str();
		  }
		  break;
		case OPT_AUTOFIRE:
			// --autofire
			autoFireMaxCount = fromDec(optarg);
			if (autoFireMaxCount < 1)
				autoFireMaxCount = 1;
			break;
		case 'b':
			coreOptions.useBios = true;
			if (optarg == NULL) {
				log("Missing BIOS file name\n");
				break;
			}
			biosFileNameGBA = xstrdup(optarg);
			break;
		case 'c':
		{
			if (optarg == NULL) {
				log("Missing config file name\n");
				break;
			}
			FILE *f = utilOpenFile(optarg, "r");
			if (f == NULL) {
				log("File not found %s\n", optarg);
				break;
			}
			preferences = NULL;
			OpenPreferences(optarg);
			fclose(f);
			LoadConfig();
		}
		break;
		case 'd':
			debugger = true;
			break;
		case 'h':
			optPrintUsage = 1;
			break;
		case 'i':
			if (optarg == NULL) {
				log("Missing patch name\n");
				break;
			}
			if (patchNum >= PATCH_MAX_NUM) {
				log("Too many patches given at %s (max is %d). Ignoring.\n", optarg, PATCH_MAX_NUM);
			}
			else {
				patchNames[patchNum] = (char *)malloc(1 + strlen(optarg));
				strcpy(patchNames[patchNum], optarg);
				patchNum++;
			}
			break;
#ifndef NO_DEBUGGER
		case 'G':
			dbgMain = remoteStubMain;
			dbgSignal = remoteStubSignal;
			dbgOutput = remoteOutput;
			debugger = true;
			if (optarg) {
				char *s = optarg;
				if (strncmp(s, "tcp:", 4) == 0) {
					s += 4;
					int port = atoi(s);
					remoteSetProtocol(0);
					remoteSetPort(port);
				}
				else if (strcmp(s, "tcp") == 0) {
					remoteSetProtocol(0);
				}
				else if (strcmp(s, "pipe") == 0) {
					remoteSetProtocol(1);
				}
				else {
					log("Unknown protocol %s\n", s);
					break;
				}
			}
			else {
				remoteSetProtocol(0);
			}
			break;
#endif
		case 'N':
			coreOptions.parseDebug = false;
			break;
		case 'F':
			fullScreen = 1;
			break;
		case 'f':
			if (optarg) {
				filter = (Filter)atoi(optarg);
			}
			else {
				filter = kStretch2x;
			}
			break;
		case 'T':
			if (optarg)
				coreOptions.throttle = atoi(optarg);
			break;
		case 'I':
			if (optarg) {
				ifbType = (IFBFilter)atoi(optarg);
			}
			else {
				ifbType = kIFBNone;
			}
			break;
		case 'p':
#ifdef PROFILING
			if (optarg) {
				cpuEnableProfiling(atoi(optarg));
			}
			else
				cpuEnableProfiling(100);
#endif
			break;
		case 'S':
			optFlashSize = atoi(optarg);
			if (optFlashSize < 0 || optFlashSize > 1)
				optFlashSize = 0;
			break;
		case 's':
			if (optarg) {
				int a = atoi(optarg);
				if (a >= 0 && a <= 9) {
					gbFrameSkip = a;
					frameSkip = a;
				}
			}
			else {
				frameSkip = 2;
				gbFrameSkip = 0;
			}
			break;
		case 't':
			if (optarg) {
				int a = atoi(optarg);
				if (a < 0 || a > 5)
					a = 0;
				coreOptions.cpuSaveType = a;
			}
			break;
		case 'v':
			if (optarg) {
				systemVerbose = atoi(optarg);
			}
			else
				systemVerbose = 0;
			break;
		case '?':
			optPrintUsage = 1;
			break;
		case 'O':
			if (optarg) {
				openGL = atoi(optarg);
				if (openGL < 0 || openGL > 2)
					openGL = 1;
			}
			else
				openGL = 0;
			break;

		case OPT_CAPTURE_FORMAT:
			// --capture-format
			if (optarg) {
				captureFormat = atoi(optarg);
			}
			break;

		case OPT_SHOW_SPEED_TRANSPARENT:
			// --show-speed-transparent
			if (optarg) {
				showSpeedTransparent = atoi(optarg);
			}
			break;

		case OPT_AUTO_FRAME_SKIP:
			// --auto-frame-skip
			if (optarg) {
				autoFrameSkip = atoi(optarg);
			}
			break;

		case OPT_AGB_PRINT:
			// --agb-print
			if (optarg) {
				agbPrint = atoi(optarg);
			}
			break;

		case OPT_RTC_ENABLED:
			// --rtc-enabled
			if (optarg) {
				coreOptions.rtcEnabled = atoi(optarg);
			}
			break;

		case OPT_GB_FRAME_SKIP:
			// --gb-frame-skip
			if (optarg) {
				gbFrameSkip = atoi(optarg);
			}
			break;

		case OPT_SOUND_FILTERING:
			// --sound-filtering
			if (optarg) {
				soundFiltering = (float)(atoi(optarg));
			}
			break;

		case OPT_SHOW_SPEED:
			// --show-speed
			if (optarg) {
				showSpeed = atoi(optarg);
			}
			break;

		case OPT_IFB_TYPE:
			// --ifb-type
			if (optarg) {
				ifbType = atoi(optarg);
			}
			break;

		case OPT_GB_BORDER_AUTOMATIC:
			// --border-automatic
			// --gb-border-automatic
			gbBorderAutomatic = true;
			break;

		case OPT_GB_BORDER_ON:
			// --border-on
			// --gb-border-on
			gbBorderOn = true;
			break;

		case OPT_GB_COLOR_OPTION:
			// --color-option
			// --gb-color-option
			gbColorOption = true;
			break;

		case OPT_GB_EMULATOR_TYPE:
			// --gb-emulator-type
			if (optarg) {
				gbEmulatorType = atoi(optarg);
			}
			break;

		case OPT_GB_PALETTE_OPTION:
			// --gb-palette-option
			if (optarg) {
				gbPaletteOption = atoi(optarg);
			}
			break;

		case OPT_REWIND_TIMER:
			// --rewind-timer
			if (optarg) {
				rewindTimer = atoi(optarg);
			}
			break;

		case OPT_BIOS_FILE_NAME_GB:
			// --bios-file-name-gb
			biosFileNameGB = optarg;
			break;

		case OPT_BIOS_FILE_NAME_GBA:
			// --bios-file-name-gba
			biosFileNameGBA = optarg;
			break;

		case OPT_BIOS_FILE_NAME_GBC:
			// --bios-file-name-gbc
			biosFileNameGBC = optarg;
			break;

		case OPT_SCREEN_SHOT_DIR:
			// --screen-shot-dir
			screenShotDir = optarg;
			break;

		case OPT_SAVE_DIR:
			// --save-dir
			saveDir = optarg;
			break;

		case OPT_BATTERY_DIR:
			// --battery-dir
			batteryDir = optarg;
			break;

		case OPT_CPU_SAVE_TYPE:
			// --cpu-save-type
			if (optarg) {
				coreOptions.cpuSaveType = atoi(optarg);
			}
			break;

		case OPT_OPT_FLASH_SIZE:
			// --opt-flash-size
			if (optarg) {
				optFlashSize = atoi(optarg);
				if (optFlashSize < 0 || optFlashSize > 1)
					optFlashSize = 0;
			}
			break;

		case OPT_DOTCODE_FILE_NAME_LOAD:
			// --dotcode-file-name-load
			coreOptions.loadDotCodeFile = optarg;
			break;

		case OPT_DOTCODE_FILE_NAME_SAVE:
			// --dotcode-file-name-save
			coreOptions.saveDotCodeFile = optarg;
			break;
		case OPT_SPEEDUP_THROTTLE:
			if (optarg)
				coreOptions.speedup_throttle = atoi(optarg);
			break;
		case OPT_SPEEDUP_FRAME_SKIP:
			if (optarg)
				coreOptions.speedup_frame_skip = atoi(optarg);
			break;
		case OPT_NO_SPEEDUP_THROTTLE_FRAME_SKIP:
			coreOptions.speedup_throttle_frame_skip = false;
			break;
		}
	}
	return op;
}

