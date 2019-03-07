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

//OpenGL library
#if (defined _MSC_VER)
#pragma comment(lib, "OpenGL32")
#include <windows.h>
#endif

#include <cmath>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifdef __APPLE__
#include <OpenGL/OpenGL.h>
#include <OpenGL/glext.h>
#include <OpenGL/glu.h>
#else
#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/glu.h>
#endif

#include <time.h>

#include "version.h"

#include "SDL.h"

#include "../Util.h"
#include "../common/ConfigManager.h"
#include "../common/Patch.h"
#include "../gb/gb.h"
#include "../gb/gbCheats.h"
#include "../gb/gbGlobals.h"
#include "../gb/gbSound.h"
#include "../gba/Cheats.h"
#include "../gba/Flash.h"
#include "../gba/GBA.h"
#include "../gba/RTC.h"
#include "../gba/Sound.h"
#include "../gba/agbprint.h"

#include "../common/SoundSDL.h"
#include "filters.h"
#include "inputSDL.h"
#include "text.h"

#ifndef _WIN32
#include <unistd.h>
#define GETCWD getcwd
#else // _WIN32
#include <direct.h>
#define GETCWD _getcwd
#define snprintf sprintf
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

#if WITH_LIRC
#include <lirc/lirc_client.h>
#include <sys/poll.h>
#endif

extern void remoteInit();
extern void remoteCleanUp();
extern void remoteStubMain();
extern void remoteStubSignal(int, int);
extern void remoteOutput(const char*, uint32_t);
extern void remoteSetProtocol(int);
extern void remoteSetPort(int);

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
int systemSaveUpdateCounter = SYSTEM_SAVE_NOT_UPDATED;

int srcPitch = 0;
int destWidth = 0;
int destHeight = 0;
int desktopWidth = 0;
int desktopHeight = 0;

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

bool pauseNextFrame = false;
int sdlMirroringEnable = 1;

//static int ignore_first_resize_event = 0;

/* forward */
void systemConsoleMessage(const char*);

char* home;
char homeDataDir[2048];

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
        sprintf(tmp, "Volume: %i%%", (int)(newVolume * 100.0 + 0.5));
        systemScreenMessage(tmp);
        soundSetVolume(newVolume);
    }
}

#if WITH_LIRC
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
        sprintf(LIRCConfigLoc, "%s/%s", homeDataDir, "lircrc");
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
    bool res = false;

    if (!dir || !dir[0]) {
        return false;
    }

    struct stat buf;

    int len = strlen(dir);

    char* p = (char*)dir + len - 1;

    while (p != dir && (*p == '/' || *p == '\\')) {
        *p = 0;
        p--;
    }

    if (stat(dir, &buf) == 0) {
        if (!(buf.st_mode & S_IFDIR)) {
            fprintf(stderr, "Error: %s is not a directory\n", dir);
        }
        res = true;
    } else {
        fprintf(stderr, "Error: %s does not exist\n", dir);
    }

    return res;
}

char* sdlGetFilename(char* name)
{
    static char filebuffer[2048];

    int len = strlen(name);

    char* p = name + len - 1;

    while (true) {
        if (*p == '/' || *p == '\\') {
            p++;
            break;
        }
        len--;
        p--;
        if (len == 0)
            break;
    }

    if (len == 0)
        strcpy(filebuffer, name);
    else
        strcpy(filebuffer, p);
    return filebuffer;
}

FILE* sdlFindFile(const char* name)
{
    char buffer[4096];
    char path[2048];

#ifdef _WIN32
#define PATH_SEP ";"
#define FILE_SEP '\\'
#define EXE_NAME "vbam.exe"
#else // ! _WIN32
#define PATH_SEP ":"
#define FILE_SEP '/'
#define EXE_NAME "vbam"
#endif // ! _WIN32

    fprintf(stdout, "Searching for file %s\n", name);

    if (GETCWD(buffer, 2048)) {
        fprintf(stdout, "Searching current directory: %s\n", buffer);
    }

    FILE* f = fopen(name, "r");
    if (f != NULL) {
        return f;
    }

    if (homeDir) {
        fprintf(stdout, "Searching home directory: %s\n", homeDataDir);
        sprintf(path, "%s%c%s", homeDataDir, FILE_SEP, name);
        f = fopen(path, "r");
        if (f != NULL)
            return f;
    }

#ifdef _WIN32
    char* home = getenv("USERPROFILE");
    if (home != NULL) {
        fprintf(stdout, "Searching user profile directory: %s\n", home);
        sprintf(path, "%s%c%s", home, FILE_SEP, name);
        f = fopen(path, "r");
        if (f != NULL)
            return f;
    }

    if (!strchr(home, '/') && !strchr(home, '\\')) {
        char* path = getenv("PATH");

        if (path != NULL) {
            fprintf(stdout, "Searching PATH\n");
            strncpy(buffer, path, 4096);
            buffer[4095] = 0;
            char* tok = strtok(buffer, PATH_SEP);

            while (tok) {
                sprintf(path, "%s%c%s", tok, FILE_SEP, EXE_NAME);
                f = fopen(path, "r");
                if (f != NULL) {
                    char path2[2048];
                    fclose(f);
                    sprintf(path2, "%s%c%s", tok, FILE_SEP, name);
                    f = fopen(path2, "r");
                    if (f != NULL) {
                        fprintf(stdout, "Found at %s\n", path2);
                        return f;
                    }
                }
                tok = strtok(NULL, PATH_SEP);
            }
        }
    } else {
        // executable is relative to some directory
        fprintf(stdout, "Searching executable directory\n");
        strcpy(buffer, home);
        char* p = strrchr(buffer, FILE_SEP);
        if (p) {
            *p = 0;
            sprintf(path, "%s%c%s", buffer, FILE_SEP, name);
            f = fopen(path, "r");
            if (f != NULL)
                return f;
        }
    }
#else // ! _WIN32
    fprintf(stdout, "Searching data directory: %s\n", PKGDATADIR);
    sprintf(path, "%s%c%s", PKGDATADIR, FILE_SEP, name);
    f = fopen(path, "r");
    if (f != NULL)
        return f;

    fprintf(stdout, "Searching system config directory: %s\n", SYSCONF_INSTALL_DIR);
    sprintf(path, "%s%c%s", SYSCONF_INSTALL_DIR, FILE_SEP, name);
    f = fopen(path, "r");
    if (f != NULL)
        return f;
#endif // ! _WIN32

    return NULL;
}

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

    // Calculate texture size as a the smallest working power of two
    float n1 = log10((float)destWidth) / log10(2.0f);
    float n2 = log10((float)destHeight) / log10(2.0f);
    float n = (n1 > n2) ? n1 : n2;

    // round up
    if (((float)((int)n)) != n)
        n = ((float)((int)n)) + 1.0f;

    textureSize = (int)pow(2.0f, n);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, textureSize, textureSize, 0,
        GL_RGBA, GL_UNSIGNED_BYTE, NULL);

    glClear(GL_COLOR_BUFFER_BIT);

    sdlOpenGLScaleWithAspect(destWidth, destHeight);
}

void sdlOpenGLInit(int w, int h)
{

#if 0
  float screenAspect = (float) sizeX / sizeY,
        windowAspect = (float) w / h;

  if(glIsTexture(screenTexture))
  glDeleteTextures(1, &screenTexture);
#endif
    glDisable(GL_CULL_FACE);
    glEnable(GL_TEXTURE_2D);

#if 0
  if(windowAspect == screenAspect)
    glViewport(0, 0, w, h);
  else if (windowAspect < screenAspect) {
    int height = (int)(w / screenAspect);
    glViewport(0, (h - height) / 2, w, height);
  } else {
    int width = (int)(h * screenAspect);
    glViewport((w - width) / 2, 0, width, h);
  }
#endif

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    glOrtho(0.0, 1.0, 1.0, 0.0, 0.0, 1.0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

#if 0
  glGenTextures(1, &screenTexture);
  glBindTexture(GL_TEXTURE_2D, screenTexture);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                  openGL == 2 ? GL_LINEAR : GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                  openGL == 2 ? GL_LINEAR : GL_NEAREST);

  // Calculate texture size as a the smallest working power of two
  float n1 = log10((float)destWidth ) / log10( 2.0f);
  float n2 = log10((float)destHeight ) / log10( 2.0f);
  float n = (n1 > n2)? n1 : n2;

    // round up
  if (((float)((int)n)) != n)
    n = ((float)((int)n)) + 1.0f;

  textureSize = (int)pow(2.0f, n);

  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, textureSize, textureSize, 0,
               GL_RGBA, GL_UNSIGNED_BYTE, NULL);
#endif

    glClearColor(0.0, 0.0, 0.0, 1.0);

    sdlOpenGLVideoResize();
}

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
    buffer[1] = rom[0xac];
    buffer[2] = rom[0xad];
    buffer[3] = rom[0xae];
    buffer[4] = rom[0xaf];
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

        char* token = strtok(s, " \t\n\r=");

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

            char* token = strtok(s, " \t\n\r=");
            if (!token)
                continue;
            if (strlen(token) == 0)
                continue;

            if (token[0] == '[') // starting another image settings
                break;
            char* value = strtok(NULL, "\t\n\r=");
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
                    cpuSaveType = save;
            } else if (!strcmp(token, "mirroringEnabled")) {
                mirroringEnable = (atoi(value) == 0 ? false : true);
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

    if (saveDir)
        sprintf(stateName, "%s/%s%d.sgm", saveDir, sdlGetFilename(filename),
            num + 1);
    else if (homeDir)
        sprintf(stateName, "%s/%s%d.sgm", homeDataDir, sdlGetFilename(filename), num + 1);
    else
        sprintf(stateName, "%s%d.sgm", filename, num + 1);

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
        sprintf(stateName, "Current state backed up to %d", num + 1);
        systemScreenMessage(stateName);
    } else if (num >= 0) {
        sprintf(stateName, "Wrote state %d", num + 1);
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
        sprintf(stateName, "Last load UNDONE");
    } else if (num == SLOT_POS_SAVE_BACKUP) {
        sprintf(stateName, "Last save UNDONE");
    } else {
        sprintf(stateName, "Loaded state %d", num + 1);
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
    strcpy(stateNameOrig, dmp);
    dmp = sdlStateName(to);
    stateNameDest = (char*)realloc(stateNameDest, strlen(dmp) + 1);
    strcpy(stateNameDest, dmp);
    dmp = sdlStateName(backup);
    stateNameBack = (char*)realloc(stateNameBack, strlen(dmp) + 1);
    strcpy(stateNameBack, dmp);

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
    char buffer[1048];

    if (batteryDir)
        sprintf(buffer, "%s/%s.sav", batteryDir, sdlGetFilename(filename));
    else if (homeDir)
        sprintf(buffer, "%s/%s.sav", homeDataDir, sdlGetFilename(filename));
    else
        sprintf(buffer, "%s.sav", filename);

    emulator.emuWriteBattery(buffer);

    systemScreenMessage("Wrote battery");
}

void sdlReadBattery()
{
    char buffer[1048];

    if (batteryDir)
        sprintf(buffer, "%s/%s.sav", batteryDir, sdlGetFilename(filename));
    else if (homeDir)
        sprintf(buffer, "%s/%s.sav", homeDataDir, sdlGetFilename(filename));
    else
        sprintf(buffer, "%s.sav", filename);

    bool res = false;

    res = emulator.emuReadBattery(buffer);

    if (res)
        systemScreenMessage("Loaded battery");
}

void sdlReadDesktopVideoMode()
{
    SDL_DisplayMode dm;
    SDL_GetDesktopDisplayMode(SDL_GetWindowDisplayIndex(window), &dm);
    desktopWidth = dm.w;
    desktopHeight = dm.h;
}

static void sdlResizeVideo()
{
    filter_enlarge = getFilterEnlargeFactor(filter);

    destWidth = filter_enlarge * sizeX;
    destHeight = filter_enlarge * sizeY;

    if (openGL) {
        free(filterPix);
        filterPix = (uint8_t*)calloc(1, (systemColorDepth >> 3) * destWidth * destHeight);
        sdlOpenGLVideoResize();
    }

    if (surface)
        SDL_FreeSurface(surface);
    if (texture)
        SDL_DestroyTexture(texture);

    if (!openGL) {
        surface = SDL_CreateRGBSurface(0, destWidth, destHeight, 32,
            0x00FF0000, 0x0000FF00,
            0x000000FF, 0xFF000000);
        texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
            SDL_TEXTUREACCESS_STREAMING,
            destWidth, destHeight);
    }

    if (!openGL && surface == NULL) {
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

    filter_enlarge = getFilterEnlargeFactor(filter);

    destWidth = filter_enlarge * sizeX;
    destHeight = filter_enlarge * sizeY;

    flags = fullScreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0;
    if (openGL) {
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        flags |= SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;
    }

    screenWidth = destWidth;
    screenHeight = destHeight;

    if (window)
        SDL_DestroyWindow(window);
    if (renderer)
        SDL_DestroyRenderer(renderer);
    window = SDL_CreateWindow("VBA-M", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        screenWidth, screenHeight, flags);
    if (!openGL) {
        renderer = SDL_CreateRenderer(window, -1, 0);
    }

    if (window == NULL) {
        systemMessage(0, "Failed to set video mode");
        SDL_Quit();
        exit(-1);
    }

    uint32_t rmask, gmask, bmask;

#if 0
  if(openGL) {
#if SDL_BYTEORDER == SDL_LIL_ENDIAN /* OpenGL RGBA masks */
      rmask = 0x000000FF;
      gmask = 0x0000FF00;
      bmask = 0x00FF0000;
#else
      rmask = 0xFF000000;
      gmask = 0x00FF0000;
      bmask = 0x0000FF00;
#endif
  } else {
      rmask = surface->format->Rmask;
      gmask = surface->format->Gmask;
      bmask = surface->format->Bmask;
  }
#endif

    if (openGL) {
        rmask = 0xFF000000;
        gmask = 0x00FF0000;
        bmask = 0x0000FF00;
    } else {
        rmask = 0x00FF0000;
        gmask = 0x0000FF00;
        bmask = 0x000000FF;
    }

    systemRedShift = sdlCalculateShift(rmask);
    systemGreenShift = sdlCalculateShift(gmask);
    systemBlueShift = sdlCalculateShift(bmask);

    //printf("systemRedShift %d, systemGreenShift %d, systemBlueShift %d\n",
    //         systemRedShift, systemGreenShift, systemBlueShift);
    //  originally 3, 11, 19 -> 27, 19, 11

    if (openGL) {
        // Align to BGRA instead of ABGR
        systemRedShift += 8;
        systemGreenShift += 8;
        systemBlueShift += 8;
    }

#if 0
  if (openGL) {
    systemColorDepth = 0;
    int i;
    glcontext = SDL_GL_CreateContext(window);
    SDL_GL_GetAttribute(SDL_GL_RED_SIZE, &i);
    systemColorDepth += i;
    SDL_GL_GetAttribute(SDL_GL_GREEN_SIZE, &i);
    systemColorDepth += i;
    SDL_GL_GetAttribute(SDL_GL_BLUE_SIZE, &i);
    systemColorDepth += i;
    printf("color depth (without alpha) is %d\n", systemColorDepth);
    SDL_GL_GetAttribute(SDL_GL_ALPHA_SIZE, &i);
    systemColorDepth += i;
    printf("color depth is %d\n", systemColorDepth);
  }
  else
    systemColorDepth = 32;

  if(systemColorDepth == 16) {
    srcPitch = sizeX*2 + 4;
  } else {
    if(systemColorDepth == 32)
      srcPitch = sizeX*4 + 4;
    else
      srcPitch = sizeX*3;
  }

#endif

    systemColorDepth = 32;
    srcPitch = sizeX * 4 + 4;

    if (openGL) {
        glcontext = SDL_GL_CreateContext(window);
        sdlOpenGLInit(screenWidth, screenHeight);
    }

    sdlResizeVideo();

#if 0
  if(openGL) {
    int scaledWidth = screenWidth * sdlOpenglScale;
    int scaledHeight = screenHeight * sdlOpenglScale;

    free(filterPix);
    filterPix = (uint8_t *)calloc(1, (systemColorDepth >> 3) * destWidth * destHeight);
    sdlOpenGLInit(screenWidth, screenHeight);

    if (	(!fullScreen)
	&&	sdlOpenglScale	> 1
	&&	scaledWidth	< desktopWidth
	&&	scaledHeight	< desktopHeight
    ) {
        SDL_SetVideoMode(scaledWidth, scaledHeight, 0,
                       SDL_OPENGL | SDL_RESIZABLE |
                       (fullScreen ? SDL_FULLSCREEN : 0));
        sdlOpenGLInit(scaledWidth, scaledHeight);
	/* xKiv: it would seem that SDL_RESIZABLE causes the *previous* dimensions to be immediately
	 * reported back via the SDL_VIDEORESIZE event
	 */
	ignore_first_resize_event	= 1;
    }
  }
#endif
}
#if defined(KMOD_GUI)
#define KMOD_META KMOD_GUI
#endif

#define MOD_KEYS (KMOD_CTRL | KMOD_SHIFT | KMOD_ALT | KMOD_META)
#define MOD_NOCTRL (KMOD_SHIFT | KMOD_ALT | KMOD_META)
#define MOD_NOALT (KMOD_CTRL | KMOD_SHIFT | KMOD_META)
#define MOD_NOSHIFT (KMOD_CTRL | KMOD_ALT | KMOD_META)

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
            sprintf(rewindMsgBuffer, "Rewind to %1d [%d]", rewindPos + 1, rewindSerials[rewindPos]);
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
        case SDL_QUIT:
            emulating = 0;
            break;
#if 0
    case SDL_VIDEORESIZE:
      if (ignore_first_resize_event)
      {
	      ignore_first_resize_event	= 0;
	      break;
      }
      if (openGL)
      {
        SDL_SetVideoMode(event.resize.w, event.resize.h, 0,
                       SDL_OPENGL | SDL_RESIZABLE |
					   (fullScreen ? SDL_FULLSCREEN : 0));
        sdlOpenGLInit(event.resize.w, event.resize.h);
      }
      break;
    case SDL_ACTIVEEVENT:
      if(pauseWhenInactive && (event.active.state & SDL_APPINPUTFOCUS)) {
        active = event.active.gain;
        if(active) {
          if(!paused) {
            if(emulating)
              soundResume();
          }
        } else {
          wasPaused = true;
          if(pauseWhenInactive) {
            if(emulating)
              soundPause();
          }

          memset(delta,255,delta_size);
        }
      }
      break;
#endif
        case SDL_WINDOWEVENT:
            switch (event.window.event) {
            case SDL_WINDOWEVENT_FOCUS_GAINED:
                if (pauseWhenInactive)
                    if (paused) {
                        if (emulating) {
                            paused = 0;
                            soundResume();
                        }
                    }
                break;
            case SDL_WINDOWEVENT_FOCUS_LOST:
                if (pauseWhenInactive) {
                    wasPaused = true;
                    if (emulating) {
                        paused = 1;
                        soundPause();
                    }

                    memset(delta, 255, delta_size);
                }
                break;
            case SDL_WINDOWEVENT_RESIZED:
                if (openGL)
                    sdlOpenGLScaleWithAspect(event.window.data1, event.window.data2);
                break;
            }
            break;
        case SDL_MOUSEMOTION:
        case SDL_MOUSEBUTTONUP:
        case SDL_MOUSEBUTTONDOWN:
            if (fullScreen) {
                SDL_ShowCursor(SDL_ENABLE);
                mouseCounter = 120;
            }
            break;
        case SDL_JOYHATMOTION:
        case SDL_JOYBUTTONDOWN:
        case SDL_JOYBUTTONUP:
        case SDL_JOYAXISMOTION:
        case SDL_KEYDOWN:
            inputProcessSDLEvent(event);
            break;
        case SDL_KEYUP:
            switch (event.key.keysym.sym) {
            case SDLK_r:
                if (!(event.key.keysym.mod & MOD_NOCTRL) && (event.key.keysym.mod & KMOD_CTRL)) {
                    if (emulating) {
                        emulator.emuReset();

                        systemScreenMessage("Reset");
                    }
                }
                break;
            case SDLK_b:
                if (!(event.key.keysym.mod & MOD_NOCTRL) && (event.key.keysym.mod & KMOD_CTRL))
                    change_rewind(-1);
                break;
            case SDLK_v:
                if (!(event.key.keysym.mod & MOD_NOCTRL) && (event.key.keysym.mod & KMOD_CTRL))
                    change_rewind(+1);
                break;
            case SDLK_h:
                if (!(event.key.keysym.mod & MOD_NOCTRL) && (event.key.keysym.mod & KMOD_CTRL))
                    change_rewind(0);
                break;
            case SDLK_j:
                if (!(event.key.keysym.mod & MOD_NOCTRL) && (event.key.keysym.mod & KMOD_CTRL))
                    change_rewind((rewindTopPos - rewindPos) * ((rewindTopPos > rewindPos) ? +1 : -1));
                break;
            case SDLK_e:
                if (!(event.key.keysym.mod & MOD_NOCTRL) && (event.key.keysym.mod & KMOD_CTRL)) {
                    cheatsEnabled = !cheatsEnabled;
                    systemConsoleMessage(cheatsEnabled ? "Cheats on" : "Cheats off");
                }
                break;

            case SDLK_s:
                if (!(event.key.keysym.mod & MOD_NOCTRL) && (event.key.keysym.mod & KMOD_CTRL)) {
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
                sdlChangeVolume(-0.1);
                break;
            case SDLK_KP_MULTIPLY:
                sdlChangeVolume(0.1);
                break;
            case SDLK_KP_MINUS:
                if (gb_effects_config.stereo > 0.0) {
                    gb_effects_config.stereo = 0.0;
                    if (gb_effects_config.echo == 0.0 && !gb_effects_config.surround) {
                        gb_effects_config.enabled = 0;
                    }
                    systemScreenMessage("Stereo off");
                } else {
                    gb_effects_config.stereo = SOUND_STEREO;
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
                    gb_effects_config.echo = SOUND_ECHO;
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

            case SDLK_p:
                if (!(event.key.keysym.mod & MOD_NOCTRL) && (event.key.keysym.mod & KMOD_CTRL)) {
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
            case SDLK_f:
                if (!(event.key.keysym.mod & MOD_NOCTRL) && (event.key.keysym.mod & KMOD_CTRL)) {
                    fullScreen = !fullScreen;
                    SDL_SetWindowFullscreen(window, fullScreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
                    if (openGL) {
                        if (fullScreen)
                            sdlOpenGLScaleWithAspect(desktopWidth, desktopHeight);
                        else
                            sdlOpenGLScaleWithAspect(destWidth, destHeight);
                    }
                    //sdlInitVideo();
                }
                break;
            case SDLK_g:
                if (!(event.key.keysym.mod & MOD_NOCTRL) && (event.key.keysym.mod & KMOD_CTRL)) {
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
                if (!(event.key.keysym.mod & MOD_NOSHIFT) && (event.key.keysym.mod & KMOD_SHIFT)) {
                    sdlHandleSavestateKey(event.key.keysym.sym - SDLK_F1, 1); // with SHIFT
                } else if (!(event.key.keysym.mod & MOD_KEYS)) {
                    sdlHandleSavestateKey(event.key.keysym.sym - SDLK_F1, 0); // without SHIFT
                }
                break;
            /* backups - only load */
            case SDLK_F9:
                /* F9 is "load backup" - saved state from *just before* the last restore */
                if (!(event.key.keysym.mod & MOD_NOSHIFT)) /* must work with or without shift, but only without other modifiers*/
                {
                    sdlReadState(SLOT_POS_LOAD_BACKUP);
                }
                break;
            case SDLK_F10:
                /* F10 is "save backup" - what was in the last overwritten savestate before we overwrote it*/
                if (!(event.key.keysym.mod & MOD_NOSHIFT)) /* must work with or without shift, but only without other modifiers*/
                {
                    sdlReadState(SLOT_POS_SAVE_BACKUP);
                }
                break;
            case SDLK_1:
            case SDLK_2:
            case SDLK_3:
            case SDLK_4:
                if (!(event.key.keysym.mod & MOD_NOALT) && (event.key.keysym.mod & KMOD_ALT)) {
                    const char* disableMessages[4] = { "autofire A disabled",
                        "autofire B disabled",
                        "autofire R disabled",
                        "autofire L disabled" };
                    const char* enableMessages[4] = { "autofire A",
                        "autofire B",
                        "autofire R",
                        "autofire L" };

                    EKey k = KEY_BUTTON_A;
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
                    layerSettings ^= mask;
                    layerEnable = DISPCNT & layerSettings;
                    CPUUpdateRenderBuffers(false);
                }
                break;
            case SDLK_5:
            case SDLK_6:
            case SDLK_7:
            case SDLK_8:
                if (!(event.key.keysym.mod & MOD_NOCTRL) && (event.key.keysym.mod & KMOD_CTRL)) {
                    int mask = 0x0100 << (event.key.keysym.sym - SDLK_1);
                    layerSettings ^= mask;
                    layerEnable = DISPCNT & layerSettings;
                }
                break;
            case SDLK_n:
                if (!(event.key.keysym.mod & MOD_NOCTRL) && (event.key.keysym.mod & KMOD_CTRL)) {
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

#if WITH_LIRC
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
  -O, --opengl=MODE            Set OpenGL texture filter\n\
      --no-opengl               0 - Disable OpenGL\n\
      --opengl-nearest          1 - No filtering\n\
      --opengl-bilinear         2 - Bilinear filtering\n\
  -F, --fullscreen             Full screen\n\
  -G, --gdb=PROTOCOL           GNU Remote Stub mode:\n\
                                tcp      - use TCP at port 55555\n\
                                tcp:PORT - use TCP at port PORT\n\
                                pipe     - use pipe transport\n\
  -I, --ifb-filter=FILTER      Select interframe blending filter:\n\
");
    for (int i = 0; i < (int)kInvalidIFBFilter; i++)
        printf("                                %d - %s\n", i, getIFBFilterName((IFBFilter)i));
    printf("\
  -N, --no-debug               Don't parse debug information\n\
  -S, --flash-size=SIZE        Set the Flash size\n\
      --flash-64k               0 -  64K Flash\n\
      --flash-128k              1 - 128K Flash\n\
  -T, --throttle=THROTTLE      Set the desired throttle (5...1000)\n\
  -b, --bios=BIOS              Use given bios file\n\
  -c, --config=FILE            Read the given configuration file\n\
  -f, --filter=FILTER          Select filter:\n\
");
    for (int i = 0; i < (int)kInvalidFilter; i++)
        printf("                                %d - %s\n", i, getFilterName((Filter)i));
    printf("\
  -h, --help                   Print this help\n\
  -i, --patch=PATCH            Apply given patch\n\
  -p, --profile=[HERTZ]        Enable profiling\n\
  -s, --frameskip=FRAMESKIP    Set frame skip (0...9)\n\
  -t, --save-type=TYPE         Set the available save type\n\
      --save-auto               0 - Automatic (EEPROM, SRAM, FLASH)\n\
      --save-eeprom             1 - EEPROM\n\
      --save-sram               2 - SRAM\n\
      --save-flash              3 - FLASH\n\
      --save-sensor             4 - EEPROM+Sensor\n\
      --save-none               5 - NONE\n\
  -v, --verbose=VERBOSE        Set verbose logging (trace.log)\n\
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
      --agb-print              Enable AGBPrint support\n\
      --auto-frameskip         Enable auto frameskipping\n\
      --no-agb-print           Disable AGBPrint support\n\
      --no-auto-frameskip      Disable auto frameskipping\n\
      --no-patch               Do not automatically apply patch\n\
      --no-pause-when-inactive Don't pause when inactive\n\
      --no-rtc                 Disable RTC support\n\
      --no-show-speed          Don't show emulation speed\n\
      --no-throttle            Disable throttle\n\
      --pause-when-inactive    Pause when inactive\n\
      --rtc                    Enable RTC support\n\
      --show-speed-normal      Show emulation speed\n\
      --show-speed-detailed    Show detailed speed data\n\
      --cheat 'CHEAT'          Add a cheat\n\
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
        sprintf(rewMsgBuf, "Remembered rewind %1d (of %1d), serial %d.", curSavePos + 1, rewindCount, rewindSerial);
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

void SetHomeDataDir()
{
    sprintf(homeDataDir, "%s%s", get_xdg_user_data_home().c_str(), DOT_DIR);
    struct stat s;
    if (stat(homeDataDir, &s) == -1 || !S_ISDIR(s.st_mode))
	mkdir(homeDataDir, 0755);
}

int main(int argc, char** argv)
{
    fprintf(stdout, "%s\n", VBA_NAME_AND_SUBVERSION);

    home = argv[0];
    SetHome(home);
    SetHomeDataDir();

    frameSkip = 2;
    gbBorderOn = 0;

    parseDebug = true;

    gb_effects_config.stereo = 0.0;
    gb_effects_config.echo = 0.0;
    gb_effects_config.surround = false;
    gb_effects_config.enabled = false;

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

    LoadConfig(); // Parse command line arguments (overrides ini)
    ReadOpts(argc, argv);

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

        utilStripDoubleExtension(szFile, filename);
        char* p = strrchr(filename, '.');

        if (p)
            *p = 0;

        if (autoPatch && patchNum == 0) {
            char* tmp;
            // no patch given yet - look for ROMBASENAME.ips
            tmp = (char*)malloc(strlen(filename) + 4 + 1);
            sprintf(tmp, "%s.ips", filename);
            patchNames[patchNum] = tmp;
            patchNum++;

            // no patch given yet - look for ROMBASENAME.ups
            tmp = (char*)malloc(strlen(filename) + 4 + 1);
            sprintf(tmp, "%s.ups", filename);
            patchNames[patchNum] = tmp;
            patchNum++;

            // no patch given yet - look for ROMBASENAME.ppf
            tmp = (char*)malloc(strlen(filename) + 4 + 1);
            sprintf(tmp, "%s.ppf", filename);
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

                // used for the handling of the gb Boot Rom
                if (gbHardware & 7)
                    gbCPUInit(biosFileNameGB, useBios);

                cartridgeType = IMAGE_GB;
                emulator = GBSystem;
                int size = gbRomSize, patchnum;
                for (patchnum = 0; patchnum < patchNum; patchnum++) {
                    fprintf(stdout, "Trying patch %s%s\n", patchNames[patchnum],
                        applyPatch(patchNames[patchnum], &gbRom, &size) ? " [success]" : "");
                }
                if (size != gbRomSize) {
                    extern bool gbUpdateSizes();
                    gbUpdateSizes();
                    gbReset();
                }
                gbReset();
            }
        } else if (type == IMAGE_GBA) {
            int size = CPULoadRom(szFile);
            failed = (size == 0);
            if (!failed) {
                if (cpuSaveType == 0)
                    utilGBAFindSave(size);
                else
                    saveType = cpuSaveType;

                sdlApplyPerImagePreferences();

                doMirroring(mirroringEnable);

                cartridgeType = 0;
                emulator = GBASystem;

                CPUInit(biosFileNameGBA, useBios);
                int patchnum;
                for (patchnum = 0; patchnum < patchNum; patchnum++) {
                    fprintf(stdout, "Trying patch %s%s\n", patchNames[patchnum],
                        applyPatch(patchNames[patchnum], &rom, &size) ? " [success]" : "");
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
        strcpy(filename, "gnu_stub");
        rom = (uint8_t*)malloc(0x2000000);
        workRAM = (uint8_t*)calloc(1, 0x40000);
        bios = (uint8_t*)calloc(1, 0x4000);
        internalRAM = (uint8_t*)calloc(1, 0x8000);
        paletteRAM = (uint8_t*)calloc(1, 0x400);
        vram = (uint8_t*)calloc(1, 0x20000);
        oam = (uint8_t*)calloc(1, 0x400);
        pix = (uint8_t*)calloc(1, 4 * 241 * 162);
        ioMem = (uint8_t*)calloc(1, 0x400);

        emulator = GBASystem;

        CPUInit(biosFileNameGBA, useBios);
        CPUReset();
    }

    sdlReadBattery();

    if (debugger)
        remoteInit();

    int flags = SDL_INIT_VIDEO | SDL_INIT_TIMER;

    if (SDL_Init(flags) < 0) {
        systemMessage(0, "Failed to init SDL: %s", SDL_GetError());
        exit(-1);
    }

    if (SDL_InitSubSystem(SDL_INIT_JOYSTICK) < 0) {
        systemMessage(0, "Failed to init joystick support: %s", SDL_GetError());
    }

#if WITH_LIRC
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

    if (systemColorDepth != 16 && systemColorDepth != 24 && systemColorDepth != 32) {
        fprintf(stderr, "Unsupported color depth '%d'.\nOnly 16, 24 and 32 bit color depths are supported\n", systemColorDepth);
        exit(-1);
    }

    fprintf(stdout, "Color depth: %d\n", systemColorDepth);

    utilUpdateSystemColorMaps();

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
            l = strlen(p);
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
        if (!paused && active) {
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
#if WITH_LIRC
        lircCheckInput();
#endif
        if (mouseCounter) {
            mouseCounter--;
            if (mouseCounter == 0)
                SDL_ShowCursor(SDL_DISABLE);
        }
    }

    emulating = 0;
    fprintf(stdout, "Shutting down\n");
    remoteCleanUp();
    soundShutdown();

    if (openGL) {
        SDL_GL_DeleteContext(glcontext);
    }

    if (gbRom != NULL || rom != NULL) {
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

#if WITH_LIRC
    StopLirc();
#endif

    SaveConfigFile();
    CloseConfig();
    SDL_Quit();
    return 0;
}

void systemMessage(int num, const char* msg, ...)
{
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
        sprintf(buffer, "%d%%", systemSpeed);
    else
        sprintf(buffer, "%3d%%(%d, %d fps)", systemSpeed,
            systemFrameSkip,
            showRenderedFrames);

    drawText(screen, pitch, x, y, buffer, showSpeedTransparent);
}

void systemDrawScreen()
{
    unsigned int destPitch = destWidth * (systemColorDepth >> 3);
    uint8_t* screen;

    renderedFrames++;

    if (openGL)
        screen = filterPix;
    else {
        screen = (uint8_t*)surface->pixels;
        SDL_LockSurface(surface);
    }

    if (ifbFunction)
        ifbFunction(pix + srcPitch, srcPitch, sizeX, sizeY);

    filterFunction(pix + srcPitch, srcPitch, delta, screen,
        destPitch, sizeX, sizeY);

    if (openGL) {
        int bytes = (systemColorDepth >> 3);
        for (int i = 0; i < destWidth; i++)
            for (int j = 0; j < destHeight; j++) {
                uint8_t k;
                k = filterPix[i * bytes + j * destPitch + 3];
                filterPix[i * bytes + j * destPitch + 3] = filterPix[i * bytes + j * destPitch + 1];
                filterPix[i * bytes + j * destPitch + 1] = k;
            }
    }

    drawScreenMessage(screen, destPitch, 10, destHeight - 20, 3000);

    if (showSpeed && fullScreen)
        drawSpeed(screen, destPitch, 10, 20);

    if (openGL) {
        glClear(GL_COLOR_BUFFER_BIT);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, destWidth);
        if (systemColorDepth == 16)
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, destWidth, destHeight,
                GL_RGB, GL_UNSIGNED_SHORT_5_6_5, screen);
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
        SDL_UnlockSurface(surface);
        SDL_UpdateTexture(texture, NULL, surface->pixels, surface->pitch);
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);
    }
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
            sprintf(buffer, "VBA-M - %d%%", systemSpeed);
        else
            sprintf(buffer, "VBA-M - %d%%(%d, %d fps)", systemSpeed,
                systemFrameSkip,
                showRenderedFrames);

        systemSetTitle(buffer);
    }
}

void systemFrame()
{
}

void system10Frames(int rate)
{
    uint32_t time = systemGetClock();
    if (!wasPaused && autoFrameSkip) {
        uint32_t diff = time - autoFrameSkipLastTime;
        int speed = 100;

        if (diff)
            speed = (1000000 / rate) / diff;

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

    if (captureFormat) {
        if (screenShotDir)
            sprintf(buffer, "%s/%s%02d.bmp", screenShotDir, sdlGetFilename(filename), a);
        else if (homeDir)
            sprintf(buffer, "%s/%s%02d.bmp", homeDataDir, sdlGetFilename(filename), a);
        else
            sprintf(buffer, "%s%02d.bmp", filename, a);

        emulator.emuWriteBMP(buffer);
    } else {
        if (screenShotDir)
            sprintf(buffer, "%s/%s%02d.png", screenShotDir, sdlGetFilename(filename), a);
        else if (homeDir)
            sprintf(buffer, "%s/%s%02d.png", homeDataDir, sdlGetFilename(filename), a);
        else
            sprintf(buffer, "%s%02d.png", filename, a);
        emulator.emuWritePNG(buffer);
    }

    systemScreenMessage("Screen capture");
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
    return SDL_GetTicks();
}

void systemGbPrint(uint8_t* data, int len, int pages, int feed, int palette, int contrast)
{
}

/* xKiv: added timestamp */
void systemConsoleMessage(const char* msg)
{
    time_t now_time;
    struct tm now_time_broken;

    now_time = time(NULL);
    now_time_broken = *(localtime(&now_time));
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
        strncpy(screenMessageBuffer, msg, 20);
        screenMessageBuffer[20] = 0;
    } else
        strcpy(screenMessageBuffer, msg);

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

SoundDriver* systemSoundInit()
{
    soundShutdown();

    return new SoundSDL();
}

void systemOnSoundShutdown()
{
}

void systemOnWriteDataToSoundBuffer(const uint16_t* finalWave, int length)
{
}

void log(const char* defaultMsg, ...)
{
    static FILE* out = NULL;

    if (out == NULL) {
        out = fopen("trace.log", "w");
    }

    va_list valist;

    va_start(valist, defaultMsg);
    vfprintf(out, defaultMsg, valist);
    va_end(valist);
}
