#ifndef VBAM_SDL_CONFIGMANAGER_H_
#define VBAM_SDL_CONFIGMANAGER_H_

#pragma once

#include "core/base/system.h"

extern const char *biosFileNameGB;
extern const char *biosFileNameGBA;
extern const char *biosFileNameGBC;
extern int agbPrint;
extern int autoFireMaxCount;
extern int autoFrameSkip;
extern int autoPatch;
extern int captureFormat;
extern int disableStatusMessages;
extern int filter;
extern int frameSkip;
extern int fullScreen;
// When set, use the null sound driver (no audio output; paces emulation with a
// timer). Also selected automatically if the normal driver fails to init.
extern int useNullSoundDriver;
extern int ifbType;
extern int openGL;
extern int optFlashSize;
extern int optPrintUsage;
extern int pauseWhenInactive;
extern int rewindTimer;
extern int showSpeed;
extern int showSpeedTransparent;

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

#endif  // VBAM_SDL_CONFIGMANAGER_H_
