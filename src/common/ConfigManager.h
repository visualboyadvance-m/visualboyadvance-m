#ifndef _CONFIGMANAGER_H
#define _CONFIGMANAGER_H

#pragma once
#include "../sdl/filters.h"
#include <stdio.h>

#ifndef __GNUC__
#define HAVE_DECL_GETOPT 0
#define __STDC__ 1
#ifndef __LIBRETRO__
#include "getopt.h"
#endif
#else // ! __GNUC__
#define HAVE_DECL_GETOPT 1
#ifndef __LIBRETRO__
#include <getopt.h>
#endif
#endif // ! __GNUC__

#define MAX_CHEATS 16384

extern bool cpuIsMultiBoot;
extern bool mirroringEnable;
extern bool parseDebug;
extern bool speedHack;
extern bool speedup;
extern const char *biosFileNameGB;
extern const char *biosFileNameGBA;
extern const char *biosFileNameGBC;
extern const char *loadDotCodeFile;
extern const char *saveDotCodeFile;
extern int agbPrint;
extern int autoFireMaxCount;
extern int autoFrameSkip;
extern int autoPatch;
extern int captureFormat;
extern int cheatsEnabled;
extern int colorizerHack;
extern int cpuDisableSfx;
extern int cpuSaveType;
extern int disableStatusMessages;
extern int filter;
extern int frameSkip;
extern int fullScreen;
extern int ifbType;
extern int layerEnable;
extern int layerSettings;
extern int openGL;
extern int optFlashSize;
extern int optPrintUsage;
extern int pauseWhenInactive;
extern int rewindTimer;
extern int rtcEnabled;
extern int saveType;
extern int showSpeed;
extern int showSpeedTransparent;
extern int skipBios;
extern int skipSaveGameBattery;
extern int skipSaveGameCheats;
extern int useBios;
extern int winGbPrinterEnabled;
extern uint32_t throttle;
extern uint32_t speedup_throttle;
extern uint32_t speedup_frame_skip;
extern bool speedup_throttle_frame_skip;
extern bool allowKeyboardBackgroundInput;
extern bool allowJoystickBackgroundInput;

extern int preparedCheats;
extern const char *preparedCheatCodes[MAX_CHEATS];

// allow up to 100 IPS/UPS/PPF patches given on commandline
#define PATCH_MAX_NUM 100
extern int patchNum;
extern char *patchNames[PATCH_MAX_NUM]; // and so on

extern const char *screenShotDir;
extern const char *saveDir;
extern const char *batteryDir;

// Directory within homedir to use for default save location.
#define DOT_DIR "visualboyadvance-m"

void SetHome(char *_arg0);
void SaveConfigFile();
void CloseConfig();
uint32_t ReadPrefHex(const char *pref_key, int default_value);
uint32_t ReadPrefHex(const char *pref_key);
uint32_t ReadPref(const char *pref_key, int default_value);
uint32_t ReadPref(const char *pref_key);
const char *ReadPrefString(const char *pref_key, const char *default_value);
const char *ReadPrefString(const char *pref_key);
void LoadConfigFile(int argc, char **argv);
void LoadConfig();
int ReadOpts(int argc, char **argv);
#endif
