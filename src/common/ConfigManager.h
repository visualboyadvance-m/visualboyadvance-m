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

extern struct CoreOptions {
    bool cpuIsMultiBoot = false;
    bool mirroringEnable = true;
    bool parseDebug = true;
    bool speedHack = false;
    bool speedup = false;
    bool speedup_throttle_frame_skip = false;
    int cheatsEnabled = 1;
    int cpuDisableSfx = 0;
    int cpuSaveType = 0;
    int layerSettings = 0xff00;
    int layerEnable = 0xff00;
    int rtcEnabled = 0;
    int saveType = 0;
    int skipBios = 0;
    int skipSaveGameBattery = 1;
    int skipSaveGameCheats = 0;
    int useBios = 0;
    int winGbPrinterEnabled = 1;
    uint32_t speedup_throttle = 100;
    uint32_t speedup_frame_skip = 9;
    uint32_t throttle = 100;
    const char *loadDotCodeFile = nullptr;
    const char *saveDotCodeFile = nullptr;
} coreOptions;

extern const char *biosFileNameGB;
extern const char *biosFileNameGBA;
extern const char *biosFileNameGBC;
extern int agbPrint;
extern int autoFireMaxCount;
extern int autoFrameSkip;
extern int autoPatch;
extern int captureFormat;
extern int colorizerHack;
extern int disableStatusMessages;
extern int filter;
extern int frameSkip;
extern int fullScreen;
extern int ifbType;
extern int openGL;
extern int optFlashSize;
extern int optPrintUsage;
extern int pauseWhenInactive;
extern int rewindTimer;
extern int showSpeed;
extern int showSpeedTransparent;
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
