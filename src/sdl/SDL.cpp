// VisualBoyAdvance - Nintendo Gameboy/GameboyAdvance (TM) emulator.
// Copyright (C) 1999-2003 Forgotten
// Copyright (C) 2005-2006 Forgotten and the VBA development team

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or(at your option)
// any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include <sys/stat.h>
#include <sys/types.h>

// System includes.
#ifdef _WIN32

#include <direct.h>
#include <io.h>

#define getcwd _getcwd
#define stat _stat
#define access _access

#ifndef W_OK
#define W_OK 2
#endif
#define mkdir(X,Y) (_mkdir(X))

// from: https://www.linuxquestions.org/questions/programming-9/porting-to-win32-429334/
#ifndef S_ISDIR
#define S_ISDIR(mode)  (((mode) & _S_IFMT) == _S_IFDIR)
#endif

#else

#include <unistd.h>

#endif // _WIN32

#ifndef __GNUC__

#define HAVE_DECL_GETOPT 0
#define __STDC__ 1
#include "getopt.h"

#else // ! __GNUC__

#define HAVE_DECL_GETOPT 1
#include <getopt.h>

#endif // ! __GNUC__

// OpenGL library.
#if defined(_WIN32)

#pragma comment(lib, "OpenGL32")
#include <Windows.h>

#define strdup _strdup

#endif  // defined(_WIN32)

#if !defined(CONFIG_IDF_TARGET) && !defined(NO_OPENGL)
#if defined(__APPLE__)
#define GL_SILENCE_DEPRECATION 1

#include <OpenGL/OpenGL.h>
#include <OpenGL/glext.h>
#include <OpenGL/glu.h>

#else  // !defined(__APPLE__)

#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/glu.h>

#endif  // defined(__APPLE__)
#endif

#ifdef ENABLE_SDL3
#include <SDL3/SDL.h>

#ifndef CONFIG_IDF_TARGET
#include <SDL3/SDL_main.h>
#endif
#else
#include <SDL.h>
#endif

#if defined(VBAM_ENABLE_LIRC)
#include <lirc/lirc_client.h>
#include <sys/poll.h>
#endif

#ifdef CONFIG_IDF_TARGET
#define CONFIG_RGB565 1
#define CONFIG_16BIT 1
#define NO_OPENGL 1
#endif

#include "components/draw_text/draw_text.h"
#include "components/filters_agb/filters_agb.h"
#include "components/filters_cgb/filters_cgb.h"
#include "components/user_config/user_config.h"
#include "core/base/file_util.h"
#include "core/base/message.h"
#include "core/base/patch.h"
#include "core/base/version.h"
#include "core/gb/gb.h"
#include "core/gb/gbCheats.h"
#include "core/gb/gbGlobals.h"
#include "core/gb/gbSound.h"
#include "core/gba/gba.h"
#include "core/gba/gbaCheats.h"
#include "core/gba/gbaFlash.h"
#include "core/gba/gbaGlobals.h"
#include "core/gba/gbaRtc.h"
#include "core/gba/gbaSound.h"
#include "sdl/ConfigManager.h"
#include "sdl/audio_sdl.h"
#include "sdl/filters.h"
#include "sdl/inputSDL.h"

#if __STDC_WANT_SECURE_LIB__
#define snprintf sprintf_s
#endif

// from: https://stackoverflow.com/questions/7608714/why-is-my-pointer-not-null-after-free
#define freeSafe(ptr) free(ptr); ptr = NULL;

extern void remoteInit();
extern void remoteCleanUp();
extern void remoteStubMain();
extern void remoteStubSignal(int, int);
extern void remoteOutput(const char*, uint32_t);
extern void remoteSetProtocol(int);
extern void remoteSetPort(int);
extern int userColorDepth;

struct CoreOptions coreOptions;

struct EmulatedSystem emulator = {
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    false,
    0
};

SDL_Surface* surface = NULL;
SDL_Window* window = NULL;
SDL_Renderer* renderer = NULL;
SDL_Texture* texture = NULL;
SDL_GLContext glcontext;

int systemSpeed = 0;
int systemRedShift = 0;
int systemBlueShift = 0;
int systemGreenShift = 0;
int systemColorDepth = 0;
int systemVerbose = 0;
int systemFrameSkip = 0;
int frameskipadjust = 0;
int systemSaveUpdateCounter = SYSTEM_SAVE_NOT_UPDATED;
int renderedFrames = 0;
int showRenderedFrames = 0;
int mouseCounter = 0;
uint32_t autoFrameSkipLastTime = 0;

char* rewindMemory = NULL;
int rewindCount;
int rewindCounter;
int rewindPos;
int rewindSaveNeeded = 0;
int rewindTopPos;
int* rewindSerials = NULL;

int srcPitch = 0;
int destWidth = 0;
int destHeight = 0;
int desktopWidth = 0;
int desktopHeight = 0;
int sizeX = 240;
int sizeY = 160;

FilterFunc filterFunction = 0;
IFBFilterFunc ifbFunction = 0;

uint8_t* delta = NULL;
static const int delta_size = 322 * 242 * 4;

int filter_enlarge = 2;

int cartridgeType = 3;

int textureSize = 256;
GLuint screenTexture = 0;
uint8_t* filterPix = 0;

int emulating = 0;
int RGB_LOW_BITS_MASK = 0x821;
uint32_t systemColorMap32[0x10000];
uint16_t systemColorMap16[0x10000];
uint8_t  systemColorMap8[0x10000];
uint16_t systemGbPalette[24];

char filename[2048];

static int rewindSerial = 0;

static int sdlSaveKeysSwitch = 0;
// if 0, then SHIFT+F# saves, F# loads (old VBA, ...)
// if 1, then SHIFT+F# loads, F# saves (linux snes9x, ...)
// if 2, then F5 decreases slot number, F6 increases, F7 saves, F8 loads

static int saveSlotPosition = 0; // default is the slot from normal F1
// internal slot number for undoing the last load
#define SLOT_POS_LOAD_BACKUP 8
// internal slot number for undoing the last save
#define SLOT_POS_SAVE_BACKUP 9

static int sdlOpenglScale = 1;
// will scale window on init by this much
static int sdlSoundToggledOff = 0;

extern int autoFireMaxCount;

#define REWIND_NUM 8
#define REWIND_SIZE 400000

enum VIDEO_SIZE {
    VIDEO_1X,
    VIDEO_2X,
    VIDEO_3X,
    VIDEO_4X,
    VIDEO_5X,
    VIDEO_6X,
    VIDEO_320x240,
    VIDEO_640x480,
    VIDEO_800x600,
    VIDEO_1024x768,
    VIDEO_1280x1024,
    VIDEO_OTHER
};

#define _stricmp strcasecmp

uint32_t throttleLastTime = 0;

bool paused = false;
bool wasPaused = false;
bool pauseNextFrame = false;
int sdlMirroringEnable = 1;

//static int ignore_first_resize_event = 0;

/* forward */
void systemConsoleMessage(const char*);

char* home;
char homeConfigDir[1024] = "";
char homeDataDir[1024] = "";

bool screenMessage = false;
char screenMessageBuffer[21];
uint32_t screenMessageTime = 0;

#define SOUND_MAX_VOLUME 2.0
#define SOUND_ECHO 0.2
#define SOUND_STEREO 0.15

static void sdlChangeVolume(float d)
{
    float oldVolume = soundGetVolume();
    float newVolume = oldVolume + d;

    if (newVolume < 0.0)
        newVolume = 0.0;
    if (newVolume > SOUND_MAX_VOLUME)
        newVolume = SOUND_MAX_VOLUME;

    if (fabs(newVolume - oldVolume) > 0.001) {
        char tmp[32];
        snprintf(tmp, sizeof(tmp), "Volume: %i%%", (int)(newVolume * 100.0 + 0.5));
        systemScreenMessage(tmp);
        soundSetVolume(newVolume);
    }
}

#if defined(VBAM_ENABLE_LIRC)
//LIRC code
bool LIRCEnabled = false;
int LIRCfd = 0;
static struct lirc_config* LIRCConfigInfo;

void StartLirc(void)
{
    fprintf(stdout, "Trying to start LIRC: ");
    //init LIRC and Record output
    LIRCfd = lirc_init("vbam", 1);
    if (LIRCfd == -1) {
        //it failed
        fprintf(stdout, "Failed\n");
    } else {
        fprintf(stdout, "Success\n");
        //read the config file
        char LIRCConfigLoc[2048];
        snprintf(LIRCConfigLoc, sizeof(LIRCConfigLoc), "%s%c%s", homeConfigDir, kFileSep, "lircrc");
        fprintf(stdout, "LIRC Config file:");
        if (lirc_readconfig(LIRCConfigLoc, &LIRCConfigInfo, NULL) == 0) {
            //check vbam dir for lircrc
            fprintf(stdout, "Loaded (%s)\n", LIRCConfigLoc);
        } else if (lirc_readconfig(NULL, &LIRCConfigInfo, NULL) == 0) {
            //check default lircrc location
            fprintf(stdout, "Loaded\n");
        } else {
            //it all failed
            fprintf(stdout, "Failed\n");
            LIRCEnabled = false;
        }
        LIRCEnabled = true;
    }
}

void StopLirc(void)
{
    //did we actually get lirc working at the start
    if (LIRCEnabled) {
        //if so free the config and deinit lirc
        fprintf(stdout, "Shuting down LIRC\n");
        lirc_freeconfig(LIRCConfigInfo);
        lirc_deinit();
        //set lirc enabled to false
        LIRCEnabled = false;
    }
}
#endif

#ifdef __MSC__
#define stat _stat
#define S_IFDIR _S_IFDIR
#endif

bool sdlCheckDirectory(const char* dir)
{
    struct stat buf;

    if (!dir || !dir[0])
        return false;

    if (stat(dir, &buf) == 0)
    {
        if (!(buf.st_mode & S_IFDIR))
        {
            fprintf(stderr, "Error: %s is not a directory\n", dir);
            return false;
        }
        return true;
    }
    else
    {
        fprintf(stderr, "Error: %s does not exist\n", dir);
        return false;
    }
}

char* sdlGetFilename(const char* name)
{
    char path[1024];
    const char *_filename = strrchr(name, kFileSep);
    if (_filename)
#if __STDC_WANT_SECURE_LIB__
        strcpy_s(path, sizeof(path), _filename + 1);
#else
        strcpy(path, _filename + 1);
#endif
    else
#if __STDC_WANT_SECURE_LIB__
        strcpy_s(path, sizeof(path), name);
#else
        strcpy(path, name);
#endif
    return strdup(path);
}

char* sdlGetFilePath(const char* name)
{
    char path[1024];
    const char *_filename = strrchr(name, kFileSep);
    if (_filename) {
        size_t length = strlen(name) - strlen(_filename);
        memcpy(path, name, length);
        path[length] = '\0';
    }
    else {
        path[0] = '.';
        path[1] = kFileSep;
        path[2] = '\0';
    }
    return strdup(path);
}

FILE* sdlFindFile(const char* name)
{
    char buffer[4096];
    char path[2048];

#ifdef _WIN32
#define PATH_SEP ";"
#define kFileSep '\\'
#define EXE_NAME "vbam.exe"
#else // ! _WIN32
#define PATH_SEP ":"
#define kFileSep '/'
#define EXE_NAME "vbam"
#endif // ! _WIN32

    fprintf(stdout, "Searching for file %s\n", name);

    if (getcwd(buffer, sizeof(buffer))) {
        fprintf(stdout, "Searching current directory: %s\n", buffer);
    }

#if __STDC_WANT_SECURE_LIB__
    FILE* f = NULL;
    fopen_s(&f, name, "r");
#else
    FILE* f = fopen(name, "r");
#endif
    if (f != NULL) {
        return f;
    }

    if (strlen(homeDataDir)) {
        fprintf(stdout, "Searching home directory: %s\n", homeDataDir);
        snprintf(path, sizeof(path), "%s%c%s", homeDataDir, kFileSep, name);
#if __STDC_WANT_SECURE_LIB__
        fopen_s(&f, path, "r");
#else
        f = fopen(path, "r");
#endif
        if (f != NULL)
            return f;
    }

#ifdef _WIN32
#if __STDC_WANT_SECURE_LIB__
    char* profileDir = NULL;
    size_t profileDirSz = 0;
    _dupenv_s(&profileDir, &profileDirSz, "USERPROFILE");
#else
    char* profileDir = getenv("USERPROFILE");
#endif
    if (profileDir != NULL) {
        fprintf(stdout, "Searching user profile directory: %s\n", profileDir);
        snprintf(path, sizeof(path), "%s%c%s", profileDir, kFileSep, name);
#if __STDC_WANT_SECURE_LIB__
        fopen_s(&f, path, "r");
#else
        f = fopen(path, "r");
#endif
        if (f != NULL)
            return f;
    }

    if (!strchr(home, '/') && !strchr(home, '\\')) {
#if __STDC_WANT_SECURE_LIB__
        char* _path = NULL;
        size_t _pathSz = 0;
        _dupenv_s(&_path, &_pathSz, "PATH");
#else
        char* _path = getenv("PATH");
#endif

        if (_path != NULL) {
            fprintf(stdout, "Searching PATH\n");
#if __STDC_WANT_SECURE_LIB__
            strcpy_s(buffer, sizeof(buffer), _path);
#else
            strcpy(buffer, _path);
#endif
            buffer[sizeof(buffer) - 1] = 0;
#if __STDC_WANT_SECURE_LIB__
            char* tok_next1 = NULL;
            char* tok_next2 = NULL;
            char* tok = strtok_s(buffer, PATH_SEP, &tok_next1);
#else
            char* tok = strtok(buffer, PATH_SEP);
#endif

            while (tok) {
                snprintf(path, sizeof(path), "%s%c%s", tok, kFileSep, EXE_NAME);
#if __STDC_WANT_SECURE_LIB__
                fopen_s(&f, path, "r");
#else
                f = fopen(path, "r");
#endif
                if (f != NULL) {
                    char path2[2048];
                    fclose(f);
                    snprintf(path2, sizeof(path2), "%s%c%s", tok, kFileSep, name);
#if __STDC_WANT_SECURE_LIB__
                    fopen_s(&f, path2, "r");
#else
                    f = fopen(path2, "r");
#endif
                    if (f != NULL) {
                        fprintf(stdout, "Found at %s\n", path2);
                        return f;
                    }
                }
#if __STDC_WANT_SECURE_LIB__
                tok = strtok_s(NULL, PATH_SEP, &tok_next2);
#else
                tok = strtok(NULL, PATH_SEP);
#endif
            }
        }
    } else {
        // executable is relative to some directory
        fprintf(stdout, "Searching executable directory\n");
#if __STDC_WANT_SECURE_LIB__
        strcpy_s(buffer, sizeof(buffer), home);
#else
        strcpy(buffer, home);
#endif
        char* p = strrchr(buffer, kFileSep);
        if (p) {
            *p = 0;
            snprintf(path, sizeof(path), "%s%c%s", buffer, kFileSep, name);
#if __STDC_WANT_SECURE_LIB__
            fopen_s(&f, path, "r");
#else
            f = fopen(path, "r");
#endif
            if (f != NULL)
                return f;
        }
    }
#else // ! _WIN32
    fprintf(stdout, "Searching data directory: %s\n", PKGDATADIR);
    snprintf(path, sizeof(path), "%s%c%s", PKGDATADIR, kFileSep, name);
#if __STDC_WANT_SECURE_LIB__
    fopen_s(&f, path, "r");
#else
    f = fopen(path, "r");
#endif
    if (f != NULL)
        return f;

    fprintf(stdout, "Searching system config directory: %s\n", SYSCONF_INSTALL_DIR);
    snprintf(path, sizeof(path), "%s%c%s", SYSCONF_INSTALL_DIR, kFileSep, name);
#if __STDC_WANT_SECURE_LIB__
    fopen_s(&f, path, "r");
#else
    f = fopen(path, "r");
#endif
    if (f != NULL)
        return f;
#endif // ! _WIN32

    return NULL;
}

#if !defined(CONFIG_IDF_TARGET) && !defined(NO_OPENGL)
static void sdlOpenGLScaleWithAspect(int w, int h)
{
    float screenAspect = (float)sizeX / sizeY,
          windowAspect = (float)w / h;

    if (windowAspect == screenAspect)
        glViewport(0, 0, w, h);
    else if (windowAspect < screenAspect) {
        int height = (int)(w / screenAspect);
        glViewport(0, (h - height) / 2, w, height);
    } else {
        int width = (int)(h * screenAspect);
        glViewport((w - width) / 2, 0, width, h);
    }
}

static void sdlOpenGLVideoResize()
{
    if (glIsTexture(screenTexture))
        glDeleteTextures(1, &screenTexture);

    glGenTextures(1, &screenTexture);
    glBindTexture(GL_TEXTURE_2D, screenTexture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
        openGL == 2 ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
        openGL == 2 ? GL_LINEAR : GL_NEAREST);

    // Calculate texture size as the smallest working power of two
    float n1 = log10((float)destWidth) / log10(2.0f);
    float n2 = log10((float)destHeight) / log10(2.0f);
    float n = (n1 > n2) ? n1 : n2;

    // round up
    if (((float)((int)n)) != n)
        n = ((float)((int)n)) + 1.0f;

    textureSize = (int)pow(2.0f, n);

    if (systemColorDepth == 8)
    {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, textureSize, textureSize, 0,
                     GL_RGB, GL_UNSIGNED_BYTE_3_3_2, NULL);
    } else if (systemColorDepth == 16) {
#ifdef CONFIG_RGB565
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, textureSize, textureSize, 0,
                     GL_RGB, GL_UNSIGNED_SHORT_5_6_5, NULL);
#else
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, textureSize, textureSize, 0,
                     GL_BGRA, GL_UNSIGNED_SHORT_1_5_5_5_REV, NULL);
#endif
    } else if (systemColorDepth == 24) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, textureSize, textureSize, 0,
                     GL_RGB, GL_UNSIGNED_BYTE, NULL);
    } else {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, textureSize, textureSize, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    }

    glClear(GL_COLOR_BUFFER_BIT);

    sdlOpenGLScaleWithAspect(destWidth, destHeight);
}

void sdlOpenGLInit(int w, int h)
{
    (void)w; // unused params
    (void)h; // unused params

    glDisable(GL_CULL_FACE);
    glEnable(GL_TEXTURE_2D);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    glOrtho(0.0, 1.0, 1.0, 0.0, 0.0, 1.0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glClearColor(0.0, 0.0, 0.0, 1.0);

    sdlOpenGLVideoResize();
}
#endif

static void sdlApplyPerImagePreferences()
{
    FILE* f = sdlFindFile("vba-over.ini");
    if (!f) {
        fprintf(stdout, "vba-over.ini NOT FOUND (using emulator settings)\n");
        return;
    } else
        fprintf(stdout, "Reading vba-over.ini\n");

    char buffer[7];
    buffer[0] = '[';
    buffer[1] = g_rom[0xac];
    buffer[2] = g_rom[0xad];
    buffer[3] = g_rom[0xae];
    buffer[4] = g_rom[0xaf];
    buffer[5] = ']';
    buffer[6] = 0;

    char readBuffer[2048];

    bool found = false;

    while (1) {
        char* s = fgets(readBuffer, 2048, f);

        if (s == NULL)
            break;

        char* p = strchr(s, ';');

        if (p)
            *p = 0;

#if __STDC_WANT_SECURE_LIB__
        char* token_next = NULL;
        char* token = strtok_s(s, " \t\n\r=", &token_next);
#else
        char* token = strtok(s, " \t\n\r=");
#endif

        if (!token)
            continue;
        if (strlen(token) == 0)
            continue;

        if (!strcmp(token, buffer)) {
            found = true;
            break;
        }
    }

    if (found) {
        while (1) {
            char* s = fgets(readBuffer, 2048, f);

            if (s == NULL)
                break;

            char* p = strchr(s, ';');
            if (p)
                *p = 0;

#if __STDC_WANT_SECURE_LIB__
            char* token_next = NULL;
            char* token = strtok_s(s, " \t\n\r=", &token_next);
#else
            char* token = strtok(s, " \t\n\r=");
#endif
            if (!token)
                continue;
            if (strlen(token) == 0)
                continue;

            if (token[0] == '[') // starting another image settings
                break;
#if __STDC_WANT_SECURE_LIB__
            char* value_next = NULL;
            char* value = strtok_s(NULL, "\t\n\r=", &value_next);
#else
            char* value = strtok(NULL, "\t\n\r=");
#endif
            if (value == NULL)
                continue;

            if (!strcmp(token, "rtcEnabled"))
                rtcEnable(atoi(value) == 0 ? false : true);
            else if (!strcmp(token, "flashSize")) {
                int size = atoi(value);
                if (size == 0x10000 || size == 0x20000)
                    flashSetSize(size);
            } else if (!strcmp(token, "saveType")) {
                int save = atoi(value);
                if (save >= 0 && save <= 5)
                    coreOptions.cpuSaveType = save;
            } else if (!strcmp(token, "mirroringEnabled")) {
                coreOptions.mirroringEnable = (atoi(value) == 0 ? false : true);
            }
        }
    }
    fclose(f);
}

static int sdlCalculateShift(uint32_t mask)
{
    int m = 0;

    while (mask) {
        m++;
        mask >>= 1;
    }

    return m - 5;
}

/* returns filename of savestate num, in static buffer (not reentrant, no need to free,
 * but value won't survive much - so if you want to remember it, dup it)
 * You may use the buffer for something else though - until you call sdlStateName again
 */
static char* sdlStateName(int num)
{
    static char stateName[2048];
    char *gameDir = sdlGetFilePath(filename);
    char *gameFile = sdlGetFilename(filename);

    if (saveDir)
        snprintf(stateName, sizeof(stateName), "%s%c%s%d.sgm", saveDir, kFileSep, gameFile, num + 1);
    else if (access(gameDir, W_OK) == 0)
        snprintf(stateName, sizeof(stateName), "%s%c%s%d.sgm", gameDir, kFileSep, gameFile, num + 1);
    else
        snprintf(stateName, sizeof(stateName), "%s%c%s%d.sgm", homeDataDir, kFileSep, gameFile, num + 1);

    freeSafe(gameDir);
    freeSafe(gameFile);
    return stateName;
}

void sdlWriteState(int num)
{
    char* stateName;

    stateName = sdlStateName(num);

    if (emulator.emuWriteState)
        emulator.emuWriteState(stateName);

    // now we reuse the stateName buffer - 2048 bytes fit in a lot
    if (num == SLOT_POS_LOAD_BACKUP) {
        snprintf(stateName, 2048, "Current state backed up to %d", num + 1);
        systemScreenMessage(stateName);
    } else if (num >= 0) {
        snprintf(stateName, 2048, "Wrote state %d", num + 1);
        systemScreenMessage(stateName);
    }

    systemDrawScreen();
}

void sdlReadState(int num)
{
    char* stateName;

    stateName = sdlStateName(num);
    if (emulator.emuReadState)
        emulator.emuReadState(stateName);

    if (num == SLOT_POS_LOAD_BACKUP) {
        snprintf(stateName, 2048, "Last load UNDONE");
    } else if (num == SLOT_POS_SAVE_BACKUP) {
        snprintf(stateName, 2048, "Last save UNDONE");
    } else {
        snprintf(stateName, 2048, "Loaded state %d", num + 1);
    }
    systemScreenMessage(stateName);

    systemDrawScreen();
}

/*
 * perform savestate exchange
 * - put the savestate in slot "to" to slot "backup" (unless backup == to)
 * - put the savestate in slot "from" to slot "to" (unless from == to)
 */
void sdlWriteBackupStateExchange(int from, int to, int backup)
{
    char* dmp;
    char* stateNameOrig = NULL;
    char* stateNameDest = NULL;
    char* stateNameBack = NULL;

    dmp = sdlStateName(from);
    stateNameOrig = (char*)realloc(stateNameOrig, strlen(dmp) + 1);
#if __STDC_WANT_SECURE_LIB__
    strcpy_s(stateNameOrig, strlen(dmp) + 1, dmp);
#else
    strcpy(stateNameOrig, dmp);
#endif
    dmp = sdlStateName(to);
    stateNameDest = (char*)realloc(stateNameDest, strlen(dmp) + 1);
#if __STDC_WANT_SECURE_LIB__
    strcpy_s(stateNameDest, strlen(dmp) + 1, dmp);
#else
    strcpy(stateNameDest, dmp);
#endif
    dmp = sdlStateName(backup);
    stateNameBack = (char*)realloc(stateNameBack, strlen(dmp) + 1);
#if __STDC_WANT_SECURE_LIB__
    strcpy_s(stateNameBack, strlen(dmp) + 1, dmp);
#else
    strcpy(stateNameBack, dmp);
#endif

    /* on POSIX, rename would not do anything anyway for identical names, but let's check it ourselves anyway */
    if (to != backup) {
        if (-1 == rename(stateNameDest, stateNameBack)) {
            fprintf(stdout, "savestate backup: can't backup old state %s to %s", stateNameDest, stateNameBack);
            perror(": ");
        }
    }
    if (to != from) {
        if (-1 == rename(stateNameOrig, stateNameDest)) {
            fprintf(stdout, "savestate backup: can't move new state %s to %s", stateNameOrig, stateNameDest);
            perror(": ");
        }
    }

    systemConsoleMessage("Savestate store and backup committed"); // with timestamp and newline
    fprintf(stdout, "to slot %d, backup in %d, using temporary slot %d\n", to + 1, backup + 1, from + 1);

    free(stateNameOrig);
    free(stateNameDest);
    free(stateNameBack);
}

void sdlWriteBattery()
{
    char buffer[2048];
    char *gameDir = sdlGetFilePath(filename);
    char *gameFile = sdlGetFilename(filename);

    if (batteryDir)
        snprintf(buffer, sizeof(buffer), "%s%c%s.sav", batteryDir, kFileSep, gameFile);
    else if (access(gameDir, W_OK) == 0)
        snprintf(buffer, sizeof(buffer), "%s%c%s.sav", gameDir, kFileSep, gameFile);
    else
        snprintf(buffer, sizeof(buffer), "%s%c%s.sav", homeDataDir, kFileSep, gameFile);

    bool result = emulator.emuWriteBattery(buffer);

    if (result)
        systemMessage(0, "Wrote battery '%s'", buffer);

    freeSafe(gameFile);
    freeSafe(gameDir);
}

void sdlReadBattery()
{
    char buffer[2048];
    char *gameDir = sdlGetFilePath(filename);
    char *gameFile = sdlGetFilename(filename);

    if (batteryDir)
        snprintf(buffer, sizeof(buffer), "%s%c%s.sav", batteryDir, kFileSep, gameFile);
    else if (access(gameDir, W_OK) == 0)
        snprintf(buffer, sizeof(buffer), "%s%c%s.sav", gameDir, kFileSep, gameFile);
    else
        snprintf(buffer, sizeof(buffer), "%s%c%s.sav", homeDataDir, kFileSep, gameFile);

    bool result = emulator.emuReadBattery(buffer);

    if (result)
        systemMessage(0, "Loaded battery '%s'", buffer);

    freeSafe(gameFile);
    freeSafe(gameDir);
}

void sdlReadDesktopVideoMode()
{
    if (window) {
#ifdef ENABLE_SDL3
        const SDL_DisplayMode *dm = SDL_GetDesktopDisplayMode(SDL_GetDisplayForWindow(window));
        desktopWidth = dm->w;
        desktopHeight = dm->h;
#else
        SDL_DisplayMode dm;
        SDL_GetDesktopDisplayMode(SDL_GetWindowDisplayIndex(window), &dm);
        desktopWidth = dm.w;
        desktopHeight = dm.h;
#endif
    }
}

static void sdlResizeVideo()
{
    filter_enlarge = getFilterEnlargeFactor(filter);

    destWidth = filter_enlarge * sizeX;
    destHeight = filter_enlarge * sizeY;

#if !defined(CONFIG_IDF_TARGET) && !defined(NO_OPENGL)
    if (openGL) {
        free(filterPix);
        filterPix = (uint8_t*)calloc(1, (systemColorDepth >> 3) * destWidth * destHeight);
        sdlOpenGLVideoResize();
    }
#endif

#ifdef ENABLE_SDL3
    if (surface)
        SDL_DestroySurface(surface);
#else
    if (surface)
        SDL_FreeSurface(surface);
#endif

    if (texture)
        SDL_DestroyTexture(texture);

#if !defined(CONFIG_IDF_TARGET) && !defined(NO_OPENGL)
    if (!openGL) {
#endif
        if (systemColorDepth == 8)
        {
#ifdef ENABLE_SDL3
            surface = SDL_CreateSurface(destWidth, destHeight, SDL_GetPixelFormatForMasks(8, 0xE0, 0x1C, 0x03, 0x00));
            texture = SDL_CreateTexture(renderer, SDL_GetPixelFormatForMasks(8, 0xE0, 0x1C, 0x03, 0x00), SDL_TEXTUREACCESS_STREAMING, destWidth, destHeight);
#else
            surface = SDL_CreateRGBSurface(0, destWidth, destHeight, 8, 0xE0, 0x1C, 0x03, 0x00);
            texture = SDL_CreateTexture(renderer, SDL_MasksToPixelFormatEnum(8, 0xE0, 0x1C, 0x03, 0x00), SDL_TEXTUREACCESS_STREAMING, destWidth, destHeight);
#endif
        } else if (systemColorDepth == 16) {
#ifdef CONFIG_RGB565
#ifdef ENABLE_SDL3
            surface = SDL_CreateSurface(destWidth, destHeight, SDL_GetPixelFormatForMasks(16, 0xF800, 0x07E0, 0x001F, 0x0000));
            texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, destWidth, destHeight);
#else
            surface = SDL_CreateRGBSurface(0, destWidth, destHeight, 16, 0xF800, 0x07E0, 0x001F, 0x0000);
            texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, destWidth, destHeight);
#endif
#else
#ifdef ENABLE_SDL3
            surface = SDL_CreateSurface(destWidth, destHeight, SDL_GetPixelFormatForMasks(16, 0x7C00, 0x03E0, 0x001F, 0x0000));
            texture = SDL_CreateTexture(renderer, SDL_GetPixelFormatForMasks(16, 0x7C00, 0x03E0, 0x001F, 0x0000), SDL_TEXTUREACCESS_STREAMING, destWidth, destHeight);
#else
            surface = SDL_CreateRGBSurface(0, destWidth, destHeight, 16, 0x7C00, 0x03E0, 0x001F, 0x0000);
            texture = SDL_CreateTexture(renderer, SDL_MasksToPixelFormatEnum(16, 0x7C00, 0x03E0, 0x001F, 0x0000), SDL_TEXTUREACCESS_STREAMING, destWidth, destHeight);
#endif
#endif
        } else if (systemColorDepth == 24) {
#ifdef ENABLE_SDL3
            surface = SDL_CreateSurface(destWidth, destHeight, SDL_GetPixelFormatForMasks(24, 0x00FF0000, 0x0000FF00, 0x000000FF, 0x00000000));
            texture = SDL_CreateTexture(renderer, SDL_GetPixelFormatForMasks(24, 0x00FF0000, 0x0000FF00, 0x000000FF, 0x00000000), SDL_TEXTUREACCESS_STREAMING, destWidth, destHeight);
#else
            surface = SDL_CreateRGBSurface(0, destWidth, destHeight, 24, 0x00FF0000, 0x0000FF00, 0x000000FF, 0x00000000);
            texture = SDL_CreateTexture(renderer, SDL_MasksToPixelFormatEnum(24, 0x00FF0000, 0x0000FF00, 0x000000FF, 0x00000000), SDL_TEXTUREACCESS_STREAMING, destWidth, destHeight);
#endif
        } else {
#ifdef ENABLE_SDL3
            surface = SDL_CreateSurface(destWidth, destHeight, SDL_GetPixelFormatForMasks(32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0x00000000));
            texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING, destWidth, destHeight);
#else
            surface = SDL_CreateRGBSurface(0, destWidth, destHeight, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0x00000000);
            texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING, destWidth, destHeight);
#endif
        }
#if !defined(CONFIG_IDF_TARGET) && !defined(NO_OPENGL)
    }
#endif

#if !defined(CONFIG_IDF_TARGET) && !defined(NO_OPENGL)
    if (!openGL && surface == NULL) {
#else
    if (surface == NULL) {
#endif
        systemMessage(0, "Failed to set video mode");
        SDL_Quit();
        exit(-1);
    }
}

void sdlInitVideo()
{
    int flags;
    int screenWidth;
    int screenHeight;
    int window_width, window_height, render_width, render_height;
#ifdef ENABLE_SDL3
    SDL_RendererLogicalPresentation representation;
    bool makes_sense = false;
#else
    SDL_bool makes_sense = SDL_FALSE;
#endif
    uint32_t rmask, gmask, bmask;
#ifndef ENABLE_SDL3
    SDL_RendererInfo render_info;
#endif

    filter_enlarge = getFilterEnlargeFactor(filter);

    destWidth = filter_enlarge * sizeX;
    destHeight = filter_enlarge * sizeY;

    flags = fullScreen ? SDL_WINDOW_FULLSCREEN : SDL_WINDOW_RESIZABLE;

#if !defined(CONFIG_IDF_TARGET) && !defined(NO_OPENGL)
    if (openGL) {
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        flags |= SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;
    }
#endif

    screenWidth = destWidth;
    screenHeight = destHeight;

    if (window)
        SDL_DestroyWindow(window);
    if (renderer)
        SDL_DestroyRenderer(renderer);

#ifndef ENABLE_SDL3
    window = SDL_CreateWindow("VBA-M", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, screenWidth, screenHeight, flags);

    if (!openGL) {
        renderer = SDL_CreateRenderer(window, -1, 0);
    }

#if !defined(CONFIG_IDF_TARGET) && !defined(NO_OPENGL)
    if (openGL)
    {
        systemMessage(0, "Renderer: OpenGL (%s)", openGL == 2 ? "bilinear" : "no filter");
    } else {
#endif
        SDL_GetRendererInfo(renderer, &render_info);

        systemMessage(0, "Renderer: SDL %d.%d (%s)", SDL_MAJOR_VERSION, SDL_MINOR_VERSION, render_info.name);
#if !defined(CONFIG_IDF_TARGET) && !defined(NO_OPENGL)
    }
#endif

    SDL_RenderSetLogicalSize(renderer, screenWidth, screenHeight);
    SDL_RenderGetLogicalSize(renderer, &render_width, &render_height);

    makes_sense = (SDL_bool)(screenWidth >= render_width && screenHeight >= render_height);
    SDL_RenderSetIntegerScale(renderer, makes_sense);
#else
    window = SDL_CreateWindow("VBA-M", screenWidth, screenHeight, flags);
#if !defined(CONFIG_IDF_TARGET) && !defined(NO_OPENGL)
    if (!openGL) {
#endif
        renderer = SDL_CreateRenderer(window, NULL);
#if !defined(CONFIG_IDF_TARGET) && !defined(NO_OPENGL)
    }
#endif

#if !defined(CONFIG_IDF_TARGET) && !defined(NO_OPENGL)
    if (openGL)
    {
        systemMessage(0, "Renderer: OpenGL (%s)", openGL == 2 ? "bilinear" : "nearest");
    } else {
#endif
    	systemMessage(0, "Renderer: SDL %d.%d (%s)", SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_GetRendererName(renderer));
#if !defined(CONFIG_IDF_TARGET) && !defined(NO_OPENGL)
    }
#endif

    SDL_GetCurrentRenderOutputSize(renderer, &window_width, &window_height);
    SDL_GetRenderLogicalPresentation(renderer, &render_width, &render_height, &representation);

    makes_sense = (window_width >= render_width && window_height >= render_height);

    if (makes_sense == true)
    {
        SDL_SetRenderLogicalPresentation(renderer, screenWidth, screenHeight, SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);
    } else {
        SDL_SetRenderLogicalPresentation(renderer, screenWidth, screenHeight, SDL_LOGICAL_PRESENTATION_DISABLED);
    }

    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_LINEAR);
#endif

    if (window == NULL) {
        systemMessage(0, "Failed to set video mode");
        SDL_Quit();
        exit(-1);
    }

    if (userColorDepth != 0)
    {
        systemColorDepth = userColorDepth;

        switch (systemColorDepth)
        {
            case  8:
                srcPitch = sizeX * (systemColorDepth >> 3) + 4;
                break;

            case 16:
                srcPitch = sizeX * (systemColorDepth >> 3) + 4;
                break;
                
            case 24:
                srcPitch = sizeX * (systemColorDepth >> 3);
                break;
                
            case 32:
                srcPitch = sizeX * (systemColorDepth >> 3) + 4;
                break;

            default:
                fprintf(stderr, "Wrong color depth %d\n", systemColorDepth);
                exit(-1);
                break;
        }
    } else {
#ifdef CONFIG_8BIT
        systemColorDepth = 8;
        srcPitch = sizeX * (systemColorDepth >> 3) + 4;
#elif defined(CONFIG_16BIT)
        systemColorDepth = 16;
        srcPitch = sizeX * (systemColorDepth >> 3) + 4;
#elif defined(CONFIG_24BIT)
        systemColorDepth = 24;
        srcPitch = sizeX * (systemColorDepth >> 3);
#else /* defined(CONFIG_32BIT) */
        systemColorDepth = 32;
        srcPitch = sizeX * (systemColorDepth >> 3) + 4;
#endif
    }

    if (systemColorDepth == 8)
    {
        rmask = 0x000000E0;
        gmask = 0x0000001C;
        bmask = 0x00000003;
    } else if (systemColorDepth == 16) {
#ifdef CONFIG_RGB565
        rmask = 0x0000F800;
        gmask = 0x000007E0;
        bmask = 0x0000001F;
#else
        rmask = 0x00007C00;
        gmask = 0x000003E0;
        bmask = 0x0000001F;
#endif
    } else {
        rmask = 0x00FF0000;
        gmask = 0x0000FF00;
        bmask = 0x000000FF;
    }

    systemRedShift = sdlCalculateShift(rmask);
    systemGreenShift = sdlCalculateShift(gmask);
    systemBlueShift = sdlCalculateShift(bmask);

    //printf("systemRedShift %d, systemGreenShift %d, systemBlueShift %d\n",
    //   systemRedShift, systemGreenShift, systemBlueShift);
    //  originally 3, 11, 19 -> 27, 19, 11

#if !defined(CONFIG_IDF_TARGET) && !defined(NO_OPENGL)
    if (openGL) {
        if (systemColorDepth == 32)
        {
            // Align to BGRA instead of ABGR
            systemRedShift += 8;
            systemGreenShift += 8;
            systemBlueShift += 8;
        }
    }
#endif

#if !defined(CONFIG_IDF_TARGET) && !defined(NO_OPENGL)
    if (openGL) {
        glcontext = SDL_GL_CreateContext(window);
        sdlOpenGLInit(screenWidth, screenHeight);
    }
#endif

    sdlResizeVideo();
}

#ifdef ENABLE_SDL3
#ifndef SDL_KMOD_META
#define SDL_KMOD_META SDL_KMOD_GUI
#endif

#define MOD_KEYS (SDL_KMOD_CTRL | SDL_KMOD_SHIFT | SDL_KMOD_ALT | SDL_KMOD_META)
#define MOD_NOCTRL (SDL_KMOD_SHIFT | SDL_KMOD_ALT | SDL_KMOD_META)
#define MOD_NOALT (SDL_KMOD_CTRL | SDL_KMOD_SHIFT | SDL_KMOD_META)
#define MOD_NOSHIFT (SDL_KMOD_CTRL | SDL_KMOD_ALT | SDL_KMOD_META)
#else
#ifndef KMOD_META
#define KMOD_META KMOD_GUI
#endif

#define MOD_KEYS (KMOD_CTRL | KMOD_SHIFT | KMOD_ALT | KMOD_META)
#define MOD_NOCTRL (KMOD_SHIFT | KMOD_ALT | KMOD_META)
#define MOD_NOALT (KMOD_CTRL | KMOD_SHIFT | KMOD_META)
#define MOD_NOSHIFT (KMOD_CTRL | KMOD_ALT | KMOD_META)
#endif

/*
 * 04.02.2008 (xKiv): factored out from sdlPollEvents
 *
 */
void change_rewind(int howmuch)
{
    if (emulating && emulator.emuReadMemState && rewindMemory
        && rewindCount) {
        rewindPos = (rewindPos + rewindCount + howmuch) % rewindCount;
        emulator.emuReadMemState(
            &rewindMemory[REWIND_SIZE * rewindPos],
            REWIND_SIZE);
        rewindCounter = 0;
        {
            char rewindMsgBuffer[50];
            snprintf(rewindMsgBuffer, sizeof(rewindMsgBuffer), "Rewind to %1d [%d]", rewindPos + 1, rewindSerials[rewindPos]);
            rewindMsgBuffer[49] = 0;
            systemConsoleMessage(rewindMsgBuffer);
        }
    }
}

/*
 * handle the F* keys (for savestates)
 * given the slot number and state of the SHIFT modifier, save or restore
 * (in savemode 3, saveslot is stored in saveSlotPosition and num means:
 *  4 .. F5: decrease slot number (down to 0)
 *  5 .. F6: increase slot number (up to 7, because 8 and 9 are reserved for backups)
 *  6 .. F7: save state
 *  7 .. F8: load state
 *  (these *should* be configurable)
 *  other keys are ignored
 * )
 */
static void sdlHandleSavestateKey(int num, int shifted)
{
    int action = -1;
    // 0: load
    // 1: save
    int backuping = 1; // controls whether we are doing savestate backups

    if (sdlSaveKeysSwitch == 2) {
        // ignore "shifted"
        switch (num) {
        // nb.: saveSlotPosition is base 0, but to the user, we show base 1 indexes (F## numbers)!
        case 4:
            if (saveSlotPosition > 0) {
                saveSlotPosition--;
                fprintf(stdout, "Changed savestate slot to %d.\n", saveSlotPosition + 1);
            } else
                fprintf(stderr, "Can't decrease slotnumber below 1.\n");
            return; // handled
        case 5:
            if (saveSlotPosition < 7) {
                saveSlotPosition++;
                fprintf(stdout, "Changed savestate slot to %d.\n", saveSlotPosition + 1);
            } else
                fprintf(stderr, "Can't increase slotnumber above 8.\n");
            return; // handled
        case 6:
            action = 1; // save
            break;
        case 7:
            action = 0; // load
            break;
        default:
            // explicitly ignore
            return; // handled
        }
    }

    if (sdlSaveKeysSwitch == 0) /* "classic" VBA: shifted is save */
    {
        if (shifted)
            action = 1; // save
        else
            action = 0; // load
        saveSlotPosition = num;
    }
    if (sdlSaveKeysSwitch == 1) /* "xKiv" VBA: shifted is load */
    {
        if (!shifted)
            action = 1; // save
        else
            action = 0; // load
        saveSlotPosition = num;
    }

    if (action < 0 || action > 1) {
        fprintf(
            stderr,
            "sdlHandleSavestateKey(%d,%d), mode %d: unexpected action %d.\n",
            num,
            shifted,
            sdlSaveKeysSwitch,
            action);
    }

    if (action) { /* save */
        if (backuping) {
            sdlWriteState(-1); // save to a special slot
            sdlWriteBackupStateExchange(-1, saveSlotPosition, SLOT_POS_SAVE_BACKUP); // F10
        } else {
            sdlWriteState(saveSlotPosition);
        }
    } else { /* load */
        if (backuping) {
            /* first back up where we are now */
            sdlWriteState(SLOT_POS_LOAD_BACKUP); // F9
        }
        sdlReadState(saveSlotPosition);
    }

} // sdlHandleSavestateKey

void sdlPollEvents()
{
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
#ifndef ENABLE_SDL3
		case SDL_QUIT:
#else
        case SDL_EVENT_QUIT:
#endif
            emulating = 0;
            break;
#ifdef ENABLE_SDL3
        case SDL_EVENT_WINDOW_FOCUS_GAINED:
            if (pauseWhenInactive)
                if (paused) {
                    if (emulating) {
                        paused = false;
                        soundResume();
                    }
                }
            break;
        case SDL_EVENT_WINDOW_FOCUS_LOST:
            if (pauseWhenInactive) {
                wasPaused = true;
                if (emulating) {
                    paused = true;
                    soundPause();
                }

                memset(delta, 255, delta_size);
            }
            break;
        case SDL_EVENT_WINDOW_RESIZED:
#if !defined(CONFIG_IDF_TARGET) && !defined(NO_OPENGL)
            if (openGL)
                sdlOpenGLScaleWithAspect(event.window.data1, event.window.data2);
#endif
            break;
		case SDL_EVENT_MOUSE_MOTION:
		case SDL_EVENT_MOUSE_BUTTON_UP:
		case SDL_EVENT_MOUSE_BUTTON_DOWN:
#else
		case SDL_WINDOWEVENT:
			switch (event.window.event) {
			case SDL_WINDOWEVENT_FOCUS_GAINED:
				if (pauseWhenInactive)
					if (paused) {
						if (emulating) {
							paused = false;
							soundResume();
						}
					}
				break;
			case SDL_WINDOWEVENT_FOCUS_LOST:
				if (pauseWhenInactive) {
					wasPaused = true;
					if (emulating) {
						paused = true;
						soundPause();
					}

					memset(delta, 255, delta_size);
				}
				break;
			case SDL_WINDOWEVENT_RESIZED:
#if !defined(CONFIG_IDF_TARGET) && !defined(NO_OPENGL)
				if (openGL)
					sdlOpenGLScaleWithAspect(event.window.data1, event.window.data2);
#endif
				break;
			}
			break;
		case SDL_MOUSEMOTION:
		case SDL_MOUSEBUTTONUP:
		case SDL_MOUSEBUTTONDOWN:
#endif
            if (fullScreen) {
#ifdef ENABLE_SDL3
                SDL_ShowCursor();
#else
                SDL_ShowCursor(true);
#endif
                mouseCounter = 120;
            }
            break;
#ifndef ENABLE_SDL3
		case SDL_JOYHATMOTION:
		case SDL_JOYBUTTONDOWN:
		case SDL_JOYBUTTONUP:
		case SDL_JOYAXISMOTION:
		case SDL_KEYDOWN:
#else
        case SDL_EVENT_JOYSTICK_HAT_MOTION:
        case SDL_EVENT_JOYSTICK_BUTTON_DOWN:
        case SDL_EVENT_JOYSTICK_BUTTON_UP:
        case SDL_EVENT_JOYSTICK_AXIS_MOTION:
        case SDL_EVENT_KEY_DOWN:
#endif
            inputProcessSDLEvent(event);
            break;
#ifndef ENABLE_SDL3
		case SDL_KEYUP:
			switch (event.key.keysym.sym) {
			case SDLK_r:
				if (!(event.key.keysym.mod & MOD_NOCTRL) && (event.key.keysym.mod & KMOD_CTRL)) {
#else
        case SDL_EVENT_KEY_UP:
            switch (event.key.key) {
            case SDLK_R:
				if (!(event.key.mod & MOD_NOCTRL) && (event.key.mod & SDL_KMOD_CTRL)) {
#endif
                    if (emulating) {
                        emulator.emuReset();

                        systemScreenMessage("Reset");
                    }
                }
                break;
#ifndef ENABLE_SDL3
			case SDLK_b:
				if (!(event.key.keysym.mod & MOD_NOCTRL) && (event.key.keysym.mod & KMOD_CTRL))
#else
            case SDLK_B:
                if (!(event.key.mod & MOD_NOCTRL) && (event.key.mod & SDL_KMOD_CTRL))
#endif
                    change_rewind(-1);
                break;
#ifndef ENABLE_SDL3
			case SDLK_v:
				if (!(event.key.keysym.mod & MOD_NOCTRL) && (event.key.keysym.mod & KMOD_CTRL))
#else
            case SDLK_V:
                if (!(event.key.mod & MOD_NOCTRL) && (event.key.mod & SDL_KMOD_CTRL))
#endif
                    change_rewind(+1);
                break;
#ifndef ENABLE_SDL3
			case SDLK_h:
				if (!(event.key.keysym.mod & MOD_NOCTRL) && (event.key.keysym.mod & KMOD_CTRL))
#else
			case SDLK_H:
				if (!(event.key.mod & MOD_NOCTRL) && (event.key.mod & SDL_KMOD_CTRL))
#endif
                    change_rewind(0);
                break;
#ifndef ENABLE_SDL3
			case SDLK_j:
				if (!(event.key.keysym.mod & MOD_NOCTRL) && (event.key.keysym.mod & KMOD_CTRL))
#else
            case SDLK_J:
                if (!(event.key.mod & MOD_NOCTRL) && (event.key.mod & SDL_KMOD_CTRL))
#endif
                    change_rewind((rewindTopPos - rewindPos) * ((rewindTopPos > rewindPos) ? +1 : -1));
                break;
#ifndef ENABLE_SDL3
			case SDLK_e:
				if (!(event.key.keysym.mod & MOD_NOCTRL) && (event.key.keysym.mod & KMOD_CTRL)) {
#else
            case SDLK_E:
                if (!(event.key.mod & MOD_NOCTRL) && (event.key.mod & SDL_KMOD_CTRL)) {
#endif
                    coreOptions.cheatsEnabled = !coreOptions.cheatsEnabled;
                    systemConsoleMessage(coreOptions.cheatsEnabled ? "Cheats on" : "Cheats off");
                }
                break;

#ifndef ENABLE_SDL3
			case SDLK_s:
				if (!(event.key.keysym.mod & MOD_NOCTRL) && (event.key.keysym.mod & KMOD_CTRL)) {
#else
            case SDLK_S:
                if (!(event.key.mod & MOD_NOCTRL) && (event.key.mod & SDL_KMOD_CTRL)) {
#endif
                    if (sdlSoundToggledOff) { // was off
                        // restore saved state
                        soundSetEnable(sdlSoundToggledOff);
                        sdlSoundToggledOff = 0;
                        systemConsoleMessage("Sound toggled on");
                    } else { // was on
                        sdlSoundToggledOff = soundGetEnable();
                        soundSetEnable(0);
                        systemConsoleMessage("Sound toggled off");
                        if (!sdlSoundToggledOff) {
                            sdlSoundToggledOff = 0x3ff;
                        }
                    }
                }
                break;
            case SDLK_KP_DIVIDE:
                sdlChangeVolume((float)-0.1);
                break;
            case SDLK_KP_MULTIPLY:
                sdlChangeVolume((float)0.1);
                break;
            case SDLK_KP_MINUS:
                if (gb_effects_config.stereo > 0.0) {
                    gb_effects_config.stereo = 0.0;
                    if (gb_effects_config.echo == 0.0 && !gb_effects_config.surround) {
                        gb_effects_config.enabled = 0;
                    }
                    systemScreenMessage("Stereo off");
                } else {
                    gb_effects_config.stereo = (float)SOUND_STEREO;
                    gb_effects_config.enabled = true;
                    systemScreenMessage("Stereo on");
                }
                break;
            case SDLK_KP_PLUS:
                if (gb_effects_config.echo > 0.0) {
                    gb_effects_config.echo = 0.0;
                    if (gb_effects_config.stereo == 0.0 && !gb_effects_config.surround) {
                        gb_effects_config.enabled = false;
                    }
                    systemScreenMessage("Echo off");
                } else {
                    gb_effects_config.echo = (float)SOUND_ECHO;
                    gb_effects_config.enabled = true;
                    systemScreenMessage("Echo on");
                }
                break;
            case SDLK_KP_ENTER:
                if (gb_effects_config.surround) {
                    gb_effects_config.surround = false;
                    if (gb_effects_config.stereo == 0.0 && gb_effects_config.echo == 0.0) {
                        gb_effects_config.enabled = false;
                    }
                    systemScreenMessage("Surround off");
                } else {
                    gb_effects_config.surround = true;
                    gb_effects_config.enabled = true;
                    systemScreenMessage("Surround on");
                }
                break;

#ifndef ENABLE_SDL3
			case SDLK_p:
				if (!(event.key.keysym.mod & MOD_NOCTRL) && (event.key.keysym.mod & KMOD_CTRL)) {
#else
            case SDLK_P:
                if (!(event.key.mod & MOD_NOCTRL) && (event.key.mod & SDL_KMOD_CTRL)) {
#endif
                    paused = !paused;
                    if (paused)
                        soundPause();
                    else
                        soundResume();
                    if (paused)
                        wasPaused = true;
                    systemConsoleMessage(paused ? "Pause on" : "Pause off");
                }
                break;
            case SDLK_ESCAPE:
                emulating = 0;
                break;
#ifndef ENABLE_SDL3
			case SDLK_f:
				if (!(event.key.keysym.mod & MOD_NOCTRL) && (event.key.keysym.mod & KMOD_CTRL)) {
					fullScreen = !fullScreen;
					SDL_SetWindowFullscreen(window, fullScreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
#else
            case SDLK_F:
                if (!(event.key.mod & MOD_NOCTRL) && (event.key.mod & SDL_KMOD_CTRL)) {
                    fullScreen = !fullScreen;
                    SDL_SetWindowFullscreen(window, fullScreen ? true : false);
#endif
#if !defined(CONFIG_IDF_TARGET) && !defined(NO_OPENGL)
                    if (openGL) {
                        if (fullScreen)
                            sdlOpenGLScaleWithAspect(desktopWidth, desktopHeight);
                        else
                            sdlOpenGLScaleWithAspect(destWidth, destHeight);
                    }
#endif
                    //sdlInitVideo();
                }
                break;
#ifndef ENABLE_SDL3
			case SDLK_g:
				if (!(event.key.keysym.mod & MOD_NOCTRL) && (event.key.keysym.mod & KMOD_CTRL)) {
#else
            case SDLK_G:
                if (!(event.key.mod & MOD_NOCTRL) && (event.key.mod & SDL_KMOD_CTRL)) {
#endif
                    filterFunction = 0;
                    while (!filterFunction) {
                        filter = (Filter)((filter + 1) % kInvalidFilter);
                        filterFunction = initFilter(filter, systemColorDepth, sizeX);
                    }
                    if (getFilterEnlargeFactor(filter) != filter_enlarge) {
                        sdlResizeVideo();
                        if (!fullScreen)
                            SDL_SetWindowSize(window, destWidth, destHeight);
                    }
                    systemScreenMessage(getFilterName(filter));
                }
                break;
            case SDLK_F11:
                if (armState) {
                    armNextPC -= 4;
                    reg[15].I -= 4;
                } else {
                    armNextPC -= 2;
                    reg[15].I -= 2;
                }
                debugger = true;
                break;
            case SDLK_F1:
            case SDLK_F2:
            case SDLK_F3:
            case SDLK_F4:
            case SDLK_F5:
            case SDLK_F6:
            case SDLK_F7:
            case SDLK_F8:
#ifndef ENABLE_SDL3
				if (!(event.key.keysym.mod & MOD_NOSHIFT) && (event.key.keysym.mod & KMOD_SHIFT)) {
					sdlHandleSavestateKey(event.key.keysym.sym - SDLK_F1, 1); // with SHIFT
				} else if (!(event.key.keysym.mod & MOD_KEYS)) {
                    sdlHandleSavestateKey(event.key.keysym.sym - SDLK_F1, 0); // without SHIFT
#else
                if (!(event.key.mod & MOD_NOSHIFT) && (event.key.mod & SDL_KMOD_SHIFT)) {
                    sdlHandleSavestateKey(event.key.key - SDLK_F1, 1); // with SHIFT
                } else if (!(event.key.mod & MOD_KEYS)) {
                    sdlHandleSavestateKey(event.key.key - SDLK_F1, 0); // without SHIFT
#endif
                }
                break;
            /* backups - only load */
            case SDLK_F9:
                /* F9 is "load backup" - saved state from *just before* the last restore */
#ifndef ENABLE_SDL3
				if (!(event.key.keysym.mod & MOD_NOSHIFT)) /* must work with or without shift, but only without other modifiers*/
#else
				if (!(event.key.mod & MOD_NOSHIFT)) /* must work with or without shift, but only without other modifiers*/
#endif
                {
                    sdlReadState(SLOT_POS_LOAD_BACKUP);
                }
                break;
            case SDLK_F10:
                /* F10 is "save backup" - what was in the last overwritten savestate before we overwrote it*/
#ifndef ENABLE_SDL3
				if (!(event.key.keysym.mod & MOD_NOSHIFT)) /* must work with or without shift, but only without other modifiers*/
#else
				if (!(event.key.mod & MOD_NOSHIFT)) /* must work with or without shift, but only without other modifiers*/
#endif
                {
                    sdlReadState(SLOT_POS_SAVE_BACKUP);
                }
                break;
            case SDLK_1:
            case SDLK_2:
            case SDLK_3:
            case SDLK_4:
#ifndef ENABLE_SDL3
				if (!(event.key.keysym.mod & MOD_NOALT) && (event.key.keysym.mod & KMOD_ALT)) {
#else
				if (!(event.key.mod & MOD_NOALT) && (event.key.mod & SDL_KMOD_ALT)) {
#endif
                    const char* disableMessages[4] = { "autofire A disabled",
                        "autofire B disabled",
                        "autofire R disabled",
                        "autofire L disabled" };
                    const char* enableMessages[4] = { "autofire A",
                        "autofire B",
                        "autofire R",
                        "autofire L" };

                    EKey k = KEY_BUTTON_A;
#ifdef ENABLE_SDL3
                    if (event.key.key == SDLK_1)
                        k = KEY_BUTTON_A;
                    else if (event.key.key == SDLK_2)
                        k = KEY_BUTTON_B;
                    else if (event.key.key == SDLK_3)
                        k = KEY_BUTTON_R;
                    else if (event.key.key == SDLK_4)
                        k = KEY_BUTTON_L;

                    if (inputToggleAutoFire(k)) {
                        systemScreenMessage(enableMessages[event.key.key - SDLK_1]);
                    } else {
                        systemScreenMessage(disableMessages[event.key.key - SDLK_1]);
                    }
                } else if (!(event.key.mod & MOD_NOCTRL) && (event.key.mod & SDL_KMOD_CTRL)) {
                    int mask = 0x0100 << (event.key.key - SDLK_1);
#else
                    if (event.key.keysym.sym == SDLK_1)
                        k = KEY_BUTTON_A;
                    else if (event.key.keysym.sym == SDLK_2)
                        k = KEY_BUTTON_B;
                    else if (event.key.keysym.sym == SDLK_3)
                        k = KEY_BUTTON_R;
                    else if (event.key.keysym.sym == SDLK_4)
                        k = KEY_BUTTON_L;

                    if (inputToggleAutoFire(k)) {
                        systemScreenMessage(enableMessages[event.key.keysym.sym - SDLK_1]);
                    } else {
                        systemScreenMessage(disableMessages[event.key.keysym.sym - SDLK_1]);
                    }
                } else if (!(event.key.keysym.mod & MOD_NOCTRL) && (event.key.keysym.mod & KMOD_CTRL)) {
                    int mask = 0x0100 << (event.key.keysym.sym - SDLK_1);
#endif
                    coreOptions.layerSettings ^= mask;
                    coreOptions.layerEnable = DISPCNT & coreOptions.layerSettings;
                    CPUUpdateRenderBuffers(false);
                }
                break;
            case SDLK_5:
            case SDLK_6:
            case SDLK_7:
            case SDLK_8:
#ifndef ENABLE_SDL3
				if (!(event.key.keysym.mod & MOD_NOCTRL) && (event.key.keysym.mod & KMOD_CTRL)) {
                    int mask = 0x0100 << (event.key.keysym.sym - SDLK_1);
#else
				if (!(event.key.mod & MOD_NOCTRL) && (event.key.mod & SDL_KMOD_CTRL)) {
                    int mask = 0x0100 << (event.key.key - SDLK_1);
#endif
                    coreOptions.layerSettings ^= mask;
                    coreOptions.layerEnable = DISPCNT & coreOptions.layerSettings;
                }
                break;
#ifndef ENABLE_SDL3
			case SDLK_n:
				if (!(event.key.keysym.mod & MOD_NOCTRL) && (event.key.keysym.mod & KMOD_CTRL)) {
#else
            case SDLK_N:
                if (!(event.key.mod & MOD_NOCTRL) && (event.key.mod & SDL_KMOD_CTRL)) {
#endif
                    if (paused)
                        paused = false;
                    pauseNextFrame = true;
                }
                break;
            default:
                break;
            }
            inputProcessSDLEvent(event);
            break;
        }
    }
}

#if defined(VBAM_ENABLE_LIRC)
void lircCheckInput(void)
{
    if (LIRCEnabled) {
        //setup a poll (poll.h)
        struct pollfd pollLIRC;
        //values fd is the pointer gotten from lircinit and events is what way
        pollLIRC.fd = LIRCfd;
        pollLIRC.events = POLLIN;
        //run the poll
        if (poll(&pollLIRC, 1, 0) > 0) {
            //poll retrieved something
            char* CodeLIRC;
            char* CmdLIRC;
            int ret; //dunno???
            if (lirc_nextcode(&CodeLIRC) == 0 && CodeLIRC != NULL) {
                //retrieve the commands
                while ((ret = lirc_code2char(LIRCConfigInfo, CodeLIRC, &CmdLIRC)) == 0 && CmdLIRC != NULL) {
                    //change the text to uppercase
                    char* CmdLIRC_Pointer = CmdLIRC;
                    while (*CmdLIRC_Pointer != '\0') {
                        *CmdLIRC_Pointer = toupper(*CmdLIRC_Pointer);
                        CmdLIRC_Pointer++;
                    }

                    if (strcmp(CmdLIRC, "QUIT") == 0) {
                        emulating = 0;
                    } else if (strcmp(CmdLIRC, "PAUSE") == 0) {
                        paused = !paused;
                        if (paused)
                            soundPause();
                        else
                            soundResume();
                        if (paused)
                            wasPaused = true;
                        systemConsoleMessage(paused ? "Pause on" : "Pause off");
                        systemScreenMessage(paused ? "Pause on" : "Pause off");
                    } else if (strcmp(CmdLIRC, "RESET") == 0) {
                        if (emulating) {
                            emulator.emuReset();
                            systemScreenMessage("Reset");
                        }
                    } else if (strcmp(CmdLIRC, "MUTE") == 0) {
                        if (sdlSoundToggledOff) { // was off
                            // restore saved state
                            soundSetEnable(sdlSoundToggledOff);
                            sdlSoundToggledOff = 0;
                            systemConsoleMessage("Sound toggled on");
                        } else { // was on
                            sdlSoundToggledOff = soundGetEnable();
                            soundSetEnable(0);
                            systemConsoleMessage("Sound toggled off");
                            if (!sdlSoundToggledOff) {
                                sdlSoundToggledOff = 0x3ff;
                            }
                        }
                    } else if (strcmp(CmdLIRC, "VOLUP") == 0) {
                        sdlChangeVolume(0.1);
                    } else if (strcmp(CmdLIRC, "VOLDOWN") == 0) {
                        sdlChangeVolume(-0.1);
                    } else if (strcmp(CmdLIRC, "LOADSTATE") == 0) {
                        sdlReadState(saveSlotPosition);
                    } else if (strcmp(CmdLIRC, "SAVESTATE") == 0) {
                        sdlWriteState(saveSlotPosition);
                    } else if (strcmp(CmdLIRC, "1") == 0) {
                        saveSlotPosition = 0;
                        systemScreenMessage("Selected State 1");
                    } else if (strcmp(CmdLIRC, "2") == 0) {
                        saveSlotPosition = 1;
                        systemScreenMessage("Selected State 2");
                    } else if (strcmp(CmdLIRC, "3") == 0) {
                        saveSlotPosition = 2;
                        systemScreenMessage("Selected State 3");
                    } else if (strcmp(CmdLIRC, "4") == 0) {
                        saveSlotPosition = 3;
                        systemScreenMessage("Selected State 4");
                    } else if (strcmp(CmdLIRC, "5") == 0) {
                        saveSlotPosition = 4;
                        systemScreenMessage("Selected State 5");
                    } else if (strcmp(CmdLIRC, "6") == 0) {
                        saveSlotPosition = 5;
                        systemScreenMessage("Selected State 6");
                    } else if (strcmp(CmdLIRC, "7") == 0) {
                        saveSlotPosition = 6;
                        systemScreenMessage("Selected State 7");
                    } else if (strcmp(CmdLIRC, "8") == 0) {
                        saveSlotPosition = 7;
                        systemScreenMessage("Selected State 8");
                    } else {
                        //do nothing
                    }
                }
                //we dont need this code nomore
                free(CodeLIRC);
            }
        }
    }
}
#endif

void usage(char* cmd)
{
    printf("%s [option ...] file\n", cmd);
    printf("\
\n\
Options:\n\
  -O, --opengl=MODE   Set OpenGL texture filter\n\
      --no-opengl       0 - Disable OpenGL\n\
      --opengl-nearest  1 - No filtering\n\
      --opengl-bilinear   2 - Bilinear filtering\n\
  -F, --fullscreen   Full screen\n\
  -G, --gdb=PROTOCOL           GNU Remote Stub mode:\n\
                                tcp   - use TCP at port 55555\n\
                                tcp:PORT - use TCP at port PORT\n\
                                pipe     - use pipe transport\n\
  -I, --ifb-filter=FILTER   Select interframe blending filter:\n\
");
    for (int i = 0; i < (int)kInvalidIFBFilter; i++)
        printf("                                %d - %s\n", i, getIFBFilterName((IFBFilter)i));
    printf("\
  -N, --no-debug               Don't parse debug information\n\
  -S, --flash-size=SIZE  Set the Flash size\n\
      --flash-64k       0 -  64K Flash\n\
      --flash-128k  1 - 128K Flash\n\
  -T, --throttle=THROTTLE   Set the desired throttle (5...1000)\n\
  -b, --bios=BIOS       Use given bios file\n\
  -c, --config=FILE   Read the given configuration file\n\
  -f, --filter=FILTER     Select filter:\n\
");
    for (int i = 0; i < (int)kInvalidFilter; i++)
        printf("                                %d - %s\n", i, getFilterName((Filter)i));
    printf("\
  -h, --help                   Print this help\n\
  -i, --patch=PATCH   Apply given patch\n\
  -p, --profile=[HERTZ]  Enable profiling\n\
  -s, --frameskip=FRAMESKIP Set frame skip (0...9)\n\
  -t, --save-type=TYPE   Set the available save type\n\
      --save-auto       0 - Automatic (EEPROM, SRAM, FLASH)\n\
      --save-eeprom 1 - EEPROM\n\
      --save-sram       2 - SRAM\n\
      --save-flash  3 - FLASH\n\
      --save-sensor 4 - EEPROM+Sensor\n\
      --save-none       5 - NONE\n\
  -v, --verbose=VERBOSE  Set verbose logging (trace.log)\n\
                                  1 - SWI\n\
                                  2 - Unaligned memory access\n\
                                  4 - Illegal memory write\n\
                                  8 - Illegal memory read\n\
                                 16 - DMA 0\n\
                                 32 - DMA 1\n\
                                 64 - DMA 2\n\
                                128 - DMA 3\n\
                                256 - Undefined instruction\n\
                                512 - AGBPrint messages\n\
\n\
Long options only:\n\
      --agb-print       Enable AGBPrint support\n\
      --auto-frameskip   Enable auto frameskipping\n\
      --no-agb-print           Disable AGBPrint support\n\
      --no-auto-frameskip   Disable auto frameskipping\n\
      --color-depth   Set color depth (8, 16, 24 or 32)\n\
      --no-patch               Do not automatically apply patch\n\
      --no-pause-when-inactive Don't pause when inactive\n\
      --no-rtc   Disable RTC support\n\
      --no-show-speed     Don't show emulation speed\n\
      --no-throttle   Disable throttle\n\
      --pause-when-inactive Pause when inactive\n\
      --rtc  Enable RTC support\n\
      --show-speed-normal   Show emulation speed\n\
      --show-speed-detailed Show detailed speed data\n\
      --cheat 'CHEAT'     Add a cheat\n\
");
}

/*
 * 04.02.2008 (xKiv) factored out, reformatted, more usefuler rewinds browsing scheme
 */
void handleRewinds()
{
    int curSavePos; // where we are saving today [1]

    rewindCount++; // how many rewinds will be stored after this store
    if (rewindCount > REWIND_NUM)
        rewindCount = REWIND_NUM;

    curSavePos = (rewindTopPos + 1) % rewindCount; // [1] depends on previous
    long resize;
    if (
        emulator.emuWriteMemState
        && emulator.emuWriteMemState(
               &rewindMemory[curSavePos * REWIND_SIZE],
               REWIND_SIZE, /* available*/
               resize /* actual size */
               )) {
        char rewMsgBuf[100];
        snprintf(rewMsgBuf, sizeof(rewMsgBuf), "Remembered rewind %1d (of %1d), serial %d.", curSavePos + 1, rewindCount, rewindSerial);
        rewMsgBuf[99] = 0;
        systemConsoleMessage(rewMsgBuf);
        rewindSerials[curSavePos] = rewindSerial;

        // set up next rewind save
        // - don't clobber the current rewind position, unless it is the original top
        if (rewindPos == rewindTopPos) {
            rewindPos = curSavePos;
        }
        // - new identification and top
        rewindSerial++;
        rewindTopPos = curSavePos;
        // for the rest of the code, rewindTopPos will be where the newest rewind got stored
    }
}

void SetHomeConfigDir()
{
    snprintf(homeConfigDir, sizeof(homeConfigDir), "%s%s", get_xdg_user_config_home().c_str(), DOT_DIR);
    struct stat s;
    if (stat(homeDataDir, &s) == -1 || !S_ISDIR(s.st_mode))
        mkdir(homeDataDir, 0755);
}

void SetHomeDataDir()
{
    snprintf(homeDataDir, sizeof(homeConfigDir), "%s%s", get_xdg_user_data_home().c_str(), DOT_DIR);
    struct stat s;
    if (stat(homeDataDir, &s) == -1 || !S_ISDIR(s.st_mode))
        mkdir(homeDataDir, 0755);
}

int main(int argc, char** argv)
{
    fprintf(stdout, "%s\n", kVbamNameAndSubversion.c_str());

    home = argv[0];
    SetHome(home);
    SetHomeConfigDir();
    SetHomeDataDir();

    frameSkip = 2;
    gbBorderOn = false;

    coreOptions.parseDebug = true;

    gb_effects_config.stereo = 0.0;
    gb_effects_config.echo = 0.0;
    gb_effects_config.surround = false;
    gb_effects_config.enabled = false;

    LoadConfig(); // Parse command line arguments (overrides ini)

    // Additional configuration.
	if (rewindTimer) {
		rewindMemory = (char *)malloc(REWIND_NUM*REWIND_SIZE);
		rewindSerials = (int *)calloc(REWIND_NUM, sizeof(int)); // init to zeroes
	}


    ReadOpts(argc, argv);

    inputSetKeymap(PAD_1, KEY_LEFT, ReadPrefHex("Joy0_Left"));
    inputSetKeymap(PAD_1, KEY_RIGHT, ReadPrefHex("Joy0_Right"));
    inputSetKeymap(PAD_1, KEY_UP, ReadPrefHex("Joy0_Up"));
    inputSetKeymap(PAD_1, KEY_DOWN, ReadPrefHex("Joy0_Down"));
    inputSetKeymap(PAD_1, KEY_BUTTON_A, ReadPrefHex("Joy0_A"));
    inputSetKeymap(PAD_1, KEY_BUTTON_B, ReadPrefHex("Joy0_B"));
    inputSetKeymap(PAD_1, KEY_BUTTON_L, ReadPrefHex("Joy0_L"));
    inputSetKeymap(PAD_1, KEY_BUTTON_R, ReadPrefHex("Joy0_R"));
    inputSetKeymap(PAD_1, KEY_BUTTON_START, ReadPrefHex("Joy0_Start"));
    inputSetKeymap(PAD_1, KEY_BUTTON_SELECT, ReadPrefHex("Joy0_Select"));
    inputSetKeymap(PAD_1, KEY_BUTTON_SPEED, ReadPrefHex("Joy0_Speed"));
    inputSetKeymap(PAD_1, KEY_BUTTON_CAPTURE, ReadPrefHex("Joy0_Capture"));
    inputSetKeymap(PAD_2, KEY_LEFT, ReadPrefHex("Joy1_Left"));
    inputSetKeymap(PAD_2, KEY_RIGHT, ReadPrefHex("Joy1_Right"));
    inputSetKeymap(PAD_2, KEY_UP, ReadPrefHex("Joy1_Up"));
    inputSetKeymap(PAD_2, KEY_DOWN, ReadPrefHex("Joy1_Down"));
    inputSetKeymap(PAD_2, KEY_BUTTON_A, ReadPrefHex("Joy1_A"));
    inputSetKeymap(PAD_2, KEY_BUTTON_B, ReadPrefHex("Joy1_B"));
    inputSetKeymap(PAD_2, KEY_BUTTON_L, ReadPrefHex("Joy1_L"));
    inputSetKeymap(PAD_2, KEY_BUTTON_R, ReadPrefHex("Joy1_R"));
    inputSetKeymap(PAD_2, KEY_BUTTON_START, ReadPrefHex("Joy1_Start"));
    inputSetKeymap(PAD_2, KEY_BUTTON_SELECT, ReadPrefHex("Joy1_Select"));
    inputSetKeymap(PAD_2, KEY_BUTTON_SPEED, ReadPrefHex("Joy1_Speed"));
    inputSetKeymap(PAD_2, KEY_BUTTON_CAPTURE, ReadPrefHex("Joy1_Capture"));
    inputSetKeymap(PAD_3, KEY_LEFT, ReadPrefHex("Joy2_Left"));
    inputSetKeymap(PAD_3, KEY_RIGHT, ReadPrefHex("Joy2_Right"));
    inputSetKeymap(PAD_3, KEY_UP, ReadPrefHex("Joy2_Up"));
    inputSetKeymap(PAD_3, KEY_DOWN, ReadPrefHex("Joy2_Down"));
    inputSetKeymap(PAD_3, KEY_BUTTON_A, ReadPrefHex("Joy2_A"));
    inputSetKeymap(PAD_3, KEY_BUTTON_B, ReadPrefHex("Joy2_B"));
    inputSetKeymap(PAD_3, KEY_BUTTON_L, ReadPrefHex("Joy2_L"));
    inputSetKeymap(PAD_3, KEY_BUTTON_R, ReadPrefHex("Joy2_R"));
    inputSetKeymap(PAD_3, KEY_BUTTON_START, ReadPrefHex("Joy2_Start"));
    inputSetKeymap(PAD_3, KEY_BUTTON_SELECT, ReadPrefHex("Joy2_Select"));
    inputSetKeymap(PAD_3, KEY_BUTTON_SPEED, ReadPrefHex("Joy2_Speed"));
    inputSetKeymap(PAD_3, KEY_BUTTON_CAPTURE, ReadPrefHex("Joy2_Capture"));
    inputSetKeymap(PAD_4, KEY_LEFT, ReadPrefHex("Joy3_Left"));
    inputSetKeymap(PAD_4, KEY_RIGHT, ReadPrefHex("Joy3_Right"));
    inputSetKeymap(PAD_4, KEY_UP, ReadPrefHex("Joy3_Up"));
    inputSetKeymap(PAD_4, KEY_DOWN, ReadPrefHex("Joy3_Down"));
    inputSetKeymap(PAD_4, KEY_BUTTON_A, ReadPrefHex("Joy3_A"));
    inputSetKeymap(PAD_4, KEY_BUTTON_B, ReadPrefHex("Joy3_B"));
    inputSetKeymap(PAD_4, KEY_BUTTON_L, ReadPrefHex("Joy3_L"));
    inputSetKeymap(PAD_4, KEY_BUTTON_R, ReadPrefHex("Joy3_R"));
    inputSetKeymap(PAD_4, KEY_BUTTON_START, ReadPrefHex("Joy3_Start"));
    inputSetKeymap(PAD_4, KEY_BUTTON_SELECT, ReadPrefHex("Joy3_Select"));
    inputSetKeymap(PAD_4, KEY_BUTTON_SPEED, ReadPrefHex("Joy3_Speed"));
    inputSetKeymap(PAD_4, KEY_BUTTON_CAPTURE, ReadPrefHex("Joy3_Capture"));
    inputSetKeymap(PAD_1, KEY_BUTTON_AUTO_A, ReadPrefHex("Joy0_AutoA"));
    inputSetKeymap(PAD_1, KEY_BUTTON_AUTO_B, ReadPrefHex("Joy0_AutoB"));
    inputSetKeymap(PAD_2, KEY_BUTTON_AUTO_A, ReadPrefHex("Joy1_AutoA"));
    inputSetKeymap(PAD_2, KEY_BUTTON_AUTO_B, ReadPrefHex("Joy1_AutoB"));
    inputSetKeymap(PAD_3, KEY_BUTTON_AUTO_A, ReadPrefHex("Joy2_AutoA"));
    inputSetKeymap(PAD_3, KEY_BUTTON_AUTO_B, ReadPrefHex("Joy2_AutoB"));
    inputSetKeymap(PAD_4, KEY_BUTTON_AUTO_A, ReadPrefHex("Joy3_AutoA"));
    inputSetKeymap(PAD_4, KEY_BUTTON_AUTO_B, ReadPrefHex("Joy3_AutoB"));
    inputSetMotionKeymap(KEY_LEFT, ReadPrefHex("Motion_Left"));
    inputSetMotionKeymap(KEY_RIGHT, ReadPrefHex("Motion_Right"));
    inputSetMotionKeymap(KEY_UP, ReadPrefHex("Motion_Up"));
    inputSetMotionKeymap(KEY_DOWN, ReadPrefHex("Motion_Down"));

    if (!sdlCheckDirectory(screenShotDir))
        screenShotDir = NULL;
    if (!sdlCheckDirectory(saveDir))
        saveDir = NULL;
    if (!sdlCheckDirectory(batteryDir))
        batteryDir = NULL;

    sdlSaveKeysSwitch = (ReadPrefHex("saveKeysSwitch"));
    sdlOpenglScale = (ReadPrefHex("openGLscale"));

    if (optPrintUsage) {
        usage(argv[0]);
        exit(-1);
    }

    if (!debugger) {
        if (optind >= argc) {
            systemMessage(0, "Missing image name");
            usage(argv[0]);
            exit(-1);
        }
    }

    if (optind < argc) {
        char* szFile = argv[optind];

        utilStripDoubleExtension(szFile, filename, sizeof(filename));
        char* p = strrchr(filename, '.');

        if (p)
            *p = 0;

        if (autoPatch && patchNum == 0) {
            char* tmp;
            // no patch given yet - look for ROMBASENAME.ips
            tmp = (char*)malloc(strlen(filename) + 4 + 2);
            snprintf(tmp, strlen(filename) + 4 + 1, "%s.ips", filename);
            patchNames[patchNum] = tmp;
            patchNum++;

            // no patch given yet - look for ROMBASENAME.ups
            tmp = (char*)malloc(strlen(filename) + 4 + 2);
            snprintf(tmp, strlen(filename) + 4 + 1, "%s.ups", filename);
            patchNames[patchNum] = tmp;
            patchNum++;

            // no patch given yet - look for ROMBASENAME.bps
            tmp = (char*)malloc(strlen(filename) + 4 + 1);
            snprintf(tmp, strlen(filename) + 4 + 1, "%s.bps", filename);
            patchNames[patchNum] = tmp;
            patchNum++;

            // no patch given yet - look for ROMBASENAME.ppf
            tmp = (char*)malloc(strlen(filename) + 4 + 1);
            snprintf(tmp, strlen(filename) + 4 + 1, "%s.ppf", filename);
            patchNames[patchNum] = tmp;
            patchNum++;
        }

        soundInit();

        bool failed = false;

        IMAGE_TYPE type = utilFindType(szFile);

        if (type == IMAGE_UNKNOWN) {
            systemMessage(0, "Unknown file type %s", szFile);
            exit(-1);
        }
        cartridgeType = (int)type;

        if (type == IMAGE_GB) {
            failed = !gbLoadRom(szFile);
            if (!failed) {
                gbGetHardwareType();

                // Load BIOS for the actual system being emulated
                // gbHardware: 1=GB, 2=GBC, 4=SGB/SGB2, 8=GBA
                // Only load BIOS for GB and GBC modes
                if (gbHardware == 2) {
                    // Game Boy Color
                    gbCPUInit(biosFileNameGBC, coreOptions.useBios);
                } else if (gbHardware == 1) {
                    // Game Boy
                    gbCPUInit(biosFileNameGB, coreOptions.useBios);
                } else {
                    // SGB/SGB2 (4) or GBA (8) - no BIOS loaded
                    gbCPUInit(NULL, false);
                }

                cartridgeType = IMAGE_GB;
                emulator = GBSystem;
                for (int patchnum = 0; patchnum < patchNum; patchnum++) {
                    fprintf(stdout, "Trying patch %s%s\n", patchNames[patchnum],
                        gbApplyPatch(patchNames[patchnum]) ? " [success]" : "");
                }
                gbReset();
            }
        } else if (type == IMAGE_GBA) {
            int size = CPULoadRom(szFile);
            failed = (size == 0);
            if (!failed) {
                if (coreOptions.cpuSaveType == 0)
                    flashDetectSaveType(size);
                else
                    coreOptions.saveType = coreOptions.cpuSaveType;

                sdlApplyPerImagePreferences();

                doMirroring(coreOptions.mirroringEnable);

                cartridgeType = 0;
                emulator = GBASystem;

                CPUInit(biosFileNameGBA, coreOptions.useBios);
                int patchnum;
                for (patchnum = 0; patchnum < patchNum; patchnum++) {
                    fprintf(stdout, "Trying patch %s%s\n", patchNames[patchnum],
                        applyPatch(patchNames[patchnum], &g_rom, &size) ? " [success]" : "");
                }
                CPUReset();
            }
        }

        if (failed) {
            systemMessage(0, "Failed to load file %s", szFile);
            exit(-1);
        }
    } else {
        soundInit();
        cartridgeType = 0;
#if __STDC_WANT_SECURE_LIB__
        strcpy_s(filename, sizeof(filename), "gnu_stub");
#else
        strcpy(filename, "gnu_stub");
#endif
        g_rom = (uint8_t*)malloc(0x2000000);
        g_workRAM = (uint8_t*)calloc(1, 0x40000);
        g_bios = (uint8_t*)calloc(1, 0x4000);
        g_internalRAM = (uint8_t*)calloc(1, 0x8000);
        g_paletteRAM = (uint8_t*)calloc(1, 0x400);
        g_vram = (uint8_t*)calloc(1, 0x20000);
        g_oam = (uint8_t*)calloc(1, 0x400);
        g_pix = (uint8_t*)calloc(1, 4 * 241 * 162);
        g_ioMem = (uint8_t*)calloc(1, 0x400);

        emulator = GBASystem;

        CPUInit(biosFileNameGBA, coreOptions.useBios);
        CPUReset();
    }

    sdlReadBattery();

    if (debugger)
        remoteInit();

    int flags = SDL_INIT_VIDEO;

#ifdef ENABLE_SDL3
    if (SDL_InitSubSystem(flags) == false) {
#else
    if (SDL_InitSubSystem(flags) < 0) {
#endif
        systemMessage(0, "Failed to init SDL subsystem: %s", SDL_GetError());
        exit(-1);
    }

#ifdef ENABLE_SDL3
    if (SDL_Init(flags) == false) {
#else
    if (SDL_Init(flags) < 0) {
#endif
        systemMessage(0, "Failed to init SDL: %s", SDL_GetError());
        exit(-1);
    }

#ifdef ENABLE_SDL3
    if (SDL_InitSubSystem(SDL_INIT_JOYSTICK) == false) {
#else
    if (SDL_InitSubSystem(SDL_INIT_JOYSTICK) < 0) {
#endif
        systemMessage(0, "Failed to init joystick support: %s", SDL_GetError());
    }

#if defined(VBAM_ENABLE_LIRC)
    StartLirc();
#endif

    inputInitJoysticks();

    if (cartridgeType == IMAGE_GBA) {
        sizeX = 240;
        sizeY = 160;
        systemFrameSkip = frameSkip;
    } else if (cartridgeType == IMAGE_GB) {
        if (gbBorderOn) {
            sizeX = 256;
            sizeY = 224;
            gbBorderLineSkip = 256;
            gbBorderColumnSkip = 48;
            gbBorderRowSkip = 40;
        } else {
            sizeX = 160;
            sizeY = 144;
            gbBorderLineSkip = 160;
            gbBorderColumnSkip = 0;
            gbBorderRowSkip = 0;
        }
        systemFrameSkip = gbFrameSkip;
    } else {
        sizeX = 320;
        sizeY = 240;
    }

    sdlReadDesktopVideoMode();

    sdlInitVideo();

    filterFunction = initFilter(filter, systemColorDepth, sizeX);
    if (!filterFunction) {
        fprintf(stderr, "Unable to init filter '%s'\n", getFilterName(filter));
        exit(-1);
    }

    if (systemColorDepth == 15)
        systemColorDepth = 16;

    if (systemColorDepth != 8 && systemColorDepth != 16 && systemColorDepth != 24 && systemColorDepth != 32) {
        fprintf(stderr, "Unsupported color depth '%d'.\nOnly 8, 16, 24 and 32 bit color depths are supported\n", systemColorDepth);
        exit(-1);
    }

    fprintf(stdout, "Color depth: %d\n", systemColorDepth);

    gbafilter_update_colors();
    gbcfilter_update_colors();

    if (delta == NULL) {
        delta = (uint8_t*)malloc(delta_size);
        memset(delta, 255, delta_size);
    }

    ifbFunction = initIFBFilter(ifbType, systemColorDepth);

    emulating = 1;
    renderedFrames = 0;

    autoFrameSkipLastTime = throttleLastTime = systemGetClock();

    // now we can enable cheats?
    {
        int i;
        for (i = 0; i < preparedCheats; i++) {
            const char* p;
            int l;
            p = preparedCheatCodes[i];
            l = (int)strlen(p);
            if (l == 17 && p[8] == ':') {
                fprintf(stdout, "Adding cheat code %s\n", p);
                cheatsAddCheatCode(p, p);
            } else if (l == 13 && p[8] == ' ') {
                fprintf(stdout, "Adding CBA cheat code %s\n", p);
                cheatsAddCBACode(p, p);
            } else if (l == 8) {
                fprintf(stdout, "Adding GB(GS) cheat code %s\n", p);
                gbAddGsCheat(p, p);
            } else {
                fprintf(stderr, "Unknown format for cheat code %s\n", p);
            }
        }
    }

    while (emulating) {
        if (!paused) {
            if (debugger && emulator.emuHasDebugger)
                remoteStubMain();
            else {
                emulator.emuMain(emulator.emuCount);
                if (rewindSaveNeeded && rewindMemory && emulator.emuWriteMemState) {
                    handleRewinds();
                }

                rewindSaveNeeded = false;
            }
        } else {
            SDL_Delay(500);
        }
        sdlPollEvents();
#if defined(VBAM_ENABLE_LIRC)
        lircCheckInput();
#endif
        if (mouseCounter) {
            mouseCounter--;
            if (mouseCounter == 0)
#ifdef ENABLE_SDL3
                SDL_HideCursor();
#else
                SDL_ShowCursor(false);
#endif
        }
    }

    emulating = 0;
    fprintf(stdout, "Shutting down\n");
    remoteCleanUp();
    soundShutdown();

#if !defined(CONFIG_IDF_TARGET) && !defined(NO_OPENGL)
    if (openGL) {
#ifdef ENABLE_SDL3
        SDL_GL_DestroyContext(glcontext);
#else
        SDL_GL_DeleteContext(glcontext);
#endif
    }
#endif

    if (gbRom != NULL || g_rom != NULL) {
        sdlWriteBattery();
        emulator.emuCleanUp();
    }

    if (delta) {
        free(delta);
        delta = NULL;
    }

    if (filterPix) {
        free(filterPix);
        filterPix = NULL;
    }

    for (int i = 0; i < patchNum; i++) {
        free(patchNames[i]);
    }

#if defined(VBAM_ENABLE_LIRC)
    StopLirc();
#endif

    SaveConfigFile();
    CloseConfig();
    SDL_Quit();
    return 0;
}

void systemMessage(int num, const char* msg, ...)
{
    (void)num; // unused params
    va_list valist;

    va_start(valist, msg);
    vfprintf(stderr, msg, valist);
    fprintf(stderr, "\n");
    va_end(valist);
}

void drawScreenMessage(uint8_t* screen, int pitch, int x, int y, unsigned int duration)
{
    if (screenMessage) {
        if (cartridgeType == 1 && gbBorderOn) {
            gbSgbRenderBorder();
        }
        if (((systemGetClock() - screenMessageTime) < duration) && !disableStatusMessages) {
            drawText(screen, pitch, x, y,
                screenMessageBuffer, false);
        } else {
            screenMessage = false;
        }
    }
}

void drawSpeed(uint8_t* screen, int pitch, int x, int y)
{
    char buffer[50];
    if (showSpeed == 1)
        snprintf(buffer, sizeof(buffer), "%d%%", systemSpeed);
    else
        snprintf(buffer, sizeof(buffer), "%3d%%(%d, %d fps)", systemSpeed,
            systemFrameSkip,
            showRenderedFrames);

    drawText(screen, pitch, x, y, buffer, showSpeedTransparent);
}

void systemDrawScreen()
{
    unsigned int destPitch = destWidth * (systemColorDepth >> 3);
    uint8_t* screen;

    renderedFrames++;

#if !defined(CONFIG_IDF_TARGET) && !defined(NO_OPENGL)
    if (openGL)
        screen = filterPix;
    else {
#endif
        screen = (uint8_t*)surface->pixels;
        SDL_LockSurface(surface);
#if !defined(CONFIG_IDF_TARGET) && !defined(NO_OPENGL)
    }
#endif

    if (ifbFunction)
        ifbFunction(g_pix + srcPitch, srcPitch, sizeX, sizeY);

    filterFunction(g_pix + srcPitch, srcPitch, delta, screen, destPitch, sizeX, sizeY);

#if !defined(CONFIG_IDF_TARGET) && !defined(NO_OPENGL)
    if (openGL) {
        int bytes = (systemColorDepth >> 3);
        for (int i = 0; i < destWidth; i++)
            for (int j = 0; j < destHeight; j++) {
                uint8_t k = 0;
                uint8_t l = 0;

                if (systemColorDepth == 24)
                {
                    k = filterPix[i * bytes + j * destPitch + 2];
                    filterPix[i * bytes + j * destPitch + 2] = filterPix[i * bytes + j * destPitch];
                    filterPix[i * bytes + j * destPitch] = k;
                } else if (systemColorDepth == 32) {
                    k = filterPix[i * bytes + j * destPitch + 3];
                    l = filterPix[i * bytes + j * destPitch + 2];
                    filterPix[i * bytes + j * destPitch + 3] = 0;
                    filterPix[i * bytes + j * destPitch + 2] = filterPix[i * bytes + j * destPitch + 1];
                    filterPix[i * bytes + j * destPitch + 1] = l;
                    filterPix[i * bytes + j * destPitch] = k;
                } else {
                    k = filterPix[i * bytes + j * destPitch + 3];
                    filterPix[i * bytes + j * destPitch + 3] = filterPix[i * bytes + j * destPitch + 1];
                    filterPix[i * bytes + j * destPitch + 1] = k;
                }
            }
    }
#endif

    drawScreenMessage(screen, destPitch, 10, destHeight - 20, 3000);

    if (showSpeed && fullScreen)
        drawSpeed(screen, destPitch, 10, 20);

#if !defined(CONFIG_IDF_TARGET) && !defined(NO_OPENGL)
    if (openGL) {
        glClear(GL_COLOR_BUFFER_BIT);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, destWidth);
        if (systemColorDepth == 8)
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, destWidth, destHeight,
                GL_RGB, GL_UNSIGNED_BYTE_3_3_2, screen);
        else if (systemColorDepth == 16)
#ifdef CONFIG_RGB565
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, destWidth, destHeight,
                GL_RGB, GL_UNSIGNED_SHORT_5_6_5, screen);
#else
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, destWidth, destHeight,
                GL_BGRA, GL_UNSIGNED_SHORT_1_5_5_5_REV, screen);
#endif
        else if (systemColorDepth == 24)
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, destWidth, destHeight,
                GL_RGB, GL_UNSIGNED_BYTE, screen);
        else
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, destWidth, destHeight,
                //GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, screen);
                GL_RGBA, GL_UNSIGNED_BYTE, screen);

        glBegin(GL_TRIANGLE_STRIP);
        glTexCoord2f(0.0f, 0.0f);
        glVertex3i(0, 0, 0);
        glTexCoord2f(destWidth / (GLfloat)textureSize, 0.0f);
        glVertex3i(1, 0, 0);
        glTexCoord2f(0.0f, destHeight / (GLfloat)textureSize);
        glVertex3i(0, 1, 0);
        glTexCoord2f(destWidth / (GLfloat)textureSize,
            destHeight / (GLfloat)textureSize);
        glVertex3i(1, 1, 0);
        glEnd();
        SDL_GL_SwapWindow(window);
    } else {
#endif
        SDL_UnlockSurface(surface);
        SDL_UpdateTexture(texture, NULL, surface->pixels, surface->pitch);
#ifdef ENABLE_SDL3
        SDL_RenderTexture(renderer, texture, NULL, NULL);
#else
        SDL_RenderCopy(renderer, texture, NULL, NULL);
#endif
        SDL_RenderPresent(renderer);
#if !defined(CONFIG_IDF_TARGET) && !defined(NO_OPENGL)
    }
#endif
}

void systemSendScreen()
{
}

void systemSetTitle(const char* title)
{
    SDL_SetWindowTitle(window, title);
}

void systemShowSpeed(int speed)
{
    systemSpeed = speed;

    showRenderedFrames = renderedFrames;
    renderedFrames = 0;

    if (!fullScreen && showSpeed) {
        char buffer[80];
        if (showSpeed == 1)
            snprintf(buffer, sizeof(buffer), "VBA-M - %d%%", systemSpeed);
        else
            snprintf(buffer, sizeof(buffer), "VBA-M - %d%%(%d, %d fps)", systemSpeed,
                systemFrameSkip,
                showRenderedFrames);

        systemSetTitle(buffer);
    }
}

void systemFrame()
{
}

void system10Frames()
{
    uint32_t time = systemGetClock();
    if (!wasPaused && autoFrameSkip) {
        uint32_t diff = time - autoFrameSkipLastTime;
        int speed = 100;

        if (diff)
            speed = (1000000 / 60) / diff;

        if (speed >= 98) {
            frameskipadjust++;

            if (frameskipadjust >= 3) {
                frameskipadjust = 0;
                if (systemFrameSkip > 0)
                    systemFrameSkip--;
            }
        } else {
            if (speed < 80)
                frameskipadjust -= (90 - speed) / 5;
            else if (systemFrameSkip < 9)
                frameskipadjust--;

            if (frameskipadjust <= -2) {
                frameskipadjust += 2;
                if (systemFrameSkip < 9)
                    systemFrameSkip++;
            }
        }
    }
    if (rewindMemory) {
        if (++rewindCounter >= rewindTimer) {
            rewindSaveNeeded = true;
            rewindCounter = 0;
        }
    }

    if (systemSaveUpdateCounter) {
        if (--systemSaveUpdateCounter <= SYSTEM_SAVE_NOT_UPDATED) {
            sdlWriteBattery();
            systemSaveUpdateCounter = SYSTEM_SAVE_NOT_UPDATED;
        }
    }

    wasPaused = false;
    autoFrameSkipLastTime = time;
}

void systemScreenCapture(int a)
{
    char buffer[2048];
    bool result = false;
    char *gameDir = sdlGetFilePath(filename);
    char *gameFile = sdlGetFilename(filename);

    if (captureFormat) {
        if (screenShotDir)
            snprintf(buffer, sizeof(buffer), "%s%c%s%02d.bmp", screenShotDir, kFileSep, gameFile, a);
        else if (access(gameDir, W_OK) == 0)
            snprintf(buffer, sizeof(buffer), "%s%c%s%02d.bmp", gameDir, kFileSep, gameFile, a);
        else
            snprintf(buffer, sizeof(buffer), "%s%c%s%02d.bmp", homeDataDir, kFileSep, gameFile, a);

        result = emulator.emuWriteBMP(buffer);
    } else {
        if (screenShotDir)
            snprintf(buffer, sizeof(buffer), "%s%c%s%02d.png", screenShotDir, kFileSep, gameFile, a);
        else if (access(gameDir, W_OK) == 0)
            snprintf(buffer, sizeof(buffer), "%s%c%s%02d.png", gameDir, kFileSep, gameFile, a);
        else
            snprintf(buffer, sizeof(buffer), "%s%c%s%02d.png", homeDataDir, kFileSep, gameFile, a);

        result = emulator.emuWritePNG(buffer);
    }

    if (result)
        systemScreenMessage("Screen capture");

    freeSafe(gameFile);
    freeSafe(gameDir);
}

void systemSaveOldest()
{
    // I need to be implemented
}

void systemLoadRecent()
{
    // I need to be implemented
}

uint32_t systemGetClock()
{
    return (uint32_t)SDL_GetTicks();
}

void systemGbPrint(uint8_t* data, int len, int pages, int feed, int palette, int contrast)
{
    (void)data; // unused params
    (void)len; // unused params
    (void)pages; // unused params
    (void)feed; // unused params
    (void)palette; // unused params
    (void)contrast; // unused params
}

/* xKiv: added timestamp */
void systemConsoleMessage(const char* msg)
{
    time_t now_time;
    struct tm now_time_broken;

    now_time = time(NULL);
#if __STDC_WANT_SECURE_LIB__
    localtime_s(&now_time_broken, &now_time);
#else
    now_time_broken = *(localtime(&now_time));
#endif
    fprintf(
        stdout,
        "%02d:%02d:%02d %02d.%02d.%4d: %s\n",
        now_time_broken.tm_hour,
        now_time_broken.tm_min,
        now_time_broken.tm_sec,
        now_time_broken.tm_mday,
        now_time_broken.tm_mon + 1,
        now_time_broken.tm_year + 1900,
        msg);
}

void systemScreenMessage(const char* msg)
{

    screenMessage = true;
    screenMessageTime = systemGetClock();
    if (strlen(msg) > 20) {
#if __STDC_WANT_SECURE_LIB__
        strncpy_s(screenMessageBuffer, sizeof(screenMessageBuffer), msg, 20);
#else
        strncpy(screenMessageBuffer, msg, 20);
#endif
        screenMessageBuffer[20] = 0;
    } else
#if __STDC_WANT_SECURE_LIB__
        strcpy_s(screenMessageBuffer, sizeof(screenMessageBuffer), msg);
#else
        strcpy(screenMessageBuffer, msg);
#endif

    systemConsoleMessage(msg);
}

bool systemCanChangeSoundQuality()
{
    return false;
}

bool systemPauseOnFrame()
{
    if (pauseNextFrame) {
        paused = true;
        pauseNextFrame = false;
        return true;
    }
    return false;
}

void systemGbBorderOn()
{
    sizeX = 256;
    sizeY = 224;
    gbBorderLineSkip = 256;
    gbBorderColumnSkip = 48;
    gbBorderRowSkip = 40;

    sdlInitVideo();

    filterFunction = initFilter(filter, systemColorDepth, sizeX);
}

bool systemReadJoypads()
{
    return true;
}

uint32_t systemReadJoypad(int which)
{
    return inputReadJoypad(which);
}
//static uint8_t sensorDarkness = 0xE8; // total darkness (including daylight on rainy days)

void systemUpdateSolarSensor()
{
}

void systemCartridgeRumble(bool)
{
}

void systemUpdateMotionSensor()
{
    inputUpdateMotionSensor();
    systemUpdateSolarSensor();
}

int systemGetSensorX()
{
    return inputGetSensorX();
}

int systemGetSensorY()
{
    return inputGetSensorY();
}

int systemGetSensorZ()
{
    return 0;
}

uint8_t systemGetSensorDarkness()
{
    return 0xE8;
}

std::unique_ptr<SoundDriver> systemSoundInit() {
    soundShutdown();

    return std::make_unique<SoundSDL>();
}

void systemOnSoundShutdown()
{
}

void systemOnWriteDataToSoundBuffer(const uint16_t* finalWave, int length)
{
    (void)finalWave; // unused params
    (void)length; // unused params
}

void log(const char* defaultMsg, ...)
{
    static FILE* out = NULL;

    if (out == NULL) {
#if __STDC_WANT_SECURE_LIB__
        fopen_s(&out, "trace.log", "w");
#else
        out = fopen("trace.log", "w");
#endif
    }

    va_list valist;

    va_start(valist, defaultMsg);
    vfprintf(out, defaultMsg, valist);
    va_end(valist);
}
