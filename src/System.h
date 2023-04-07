#ifndef SYSTEM_H
#define SYSTEM_H

#include "common/Types.h"

#define winlog log

#ifdef __LIBRETRO__
#define utilOpenFile fopen
#endif

class SoundDriver;

struct EmulatedSystem {
        // main emulation function
        void (*emuMain)(int);
        // reset emulator
        void (*emuReset)();
        // clean up memory
        void (*emuCleanUp)();
        // load battery file
        bool (*emuReadBattery)(const char *);
        // write battery file
        bool (*emuWriteBattery)(const char *);
#ifdef __LIBRETRO__
        // load state
        bool (*emuReadState)(const uint8_t *);
        // load state
        unsigned (*emuWriteState)(uint8_t *);
#else
        // load state
        bool (*emuReadState)(const char *);
        // save state
        bool (*emuWriteState)(const char *);
#endif
        // load memory state (rewind)
        bool (*emuReadMemState)(char *, int);
        // write memory state (rewind)
        bool (*emuWriteMemState)(char *, int, long &);
        // write PNG file
        bool (*emuWritePNG)(const char *);
        // write BMP file
        bool (*emuWriteBMP)(const char *);
        // emulator update CPSR (ARM only)
        void (*emuUpdateCPSR)();
        // emulator has debugger
        bool emuHasDebugger;
        // clock ticks to emulate
        int emuCount;
};

extern struct CoreOptions {
    bool cpuIsMultiBoot = false;
    bool mirroringEnable = true;
    bool skipBios = false;
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

extern void log(const char *, ...);
extern bool systemPauseOnFrame();
extern void systemGbPrint(uint8_t *, int, int, int, int, int);
extern void systemScreenCapture(int);
extern void systemDrawScreen();
extern void systemSendScreen();
// updates the joystick data
extern bool systemReadJoypads();
// return information about the given joystick, -1 for default joystick
extern uint32_t systemReadJoypad(int);
extern uint32_t systemGetClock();
extern void systemMessage(int, const char *, ...);
extern void systemSetTitle(const char *);
extern SoundDriver *systemSoundInit();
extern void systemOnWriteDataToSoundBuffer(const uint16_t *finalWave, int length);
extern void systemOnSoundShutdown();
extern void systemScreenMessage(const char *);
extern void systemUpdateMotionSensor();
extern int systemGetSensorX();
extern int systemGetSensorY();
extern int systemGetSensorZ();
extern uint8_t systemGetSensorDarkness();
extern void systemCartridgeRumble(bool);
extern void systemPossibleCartridgeRumble(bool);
extern void updateRumbleFrame();
extern bool systemCanChangeSoundQuality();
extern void systemShowSpeed(int);
extern void system10Frames();
extern void systemFrame();
extern void systemGbBorderOn();
extern void Sm60FPS_Init();
extern bool Sm60FPS_CanSkipFrame();
extern void Sm60FPS_Sleep();
extern void DbgMsg(const char *msg, ...);
extern void (*dbgOutput)(const char *s, uint32_t addr);
extern void (*dbgSignal)(int sig, int number);
extern uint16_t systemColorMap16[0x10000];
extern uint32_t systemColorMap32[0x10000];
extern uint16_t systemGbPalette[24];
extern int systemRedShift;
extern int systemGreenShift;
extern int systemBlueShift;
extern int systemColorDepth;
extern int systemVerbose;
extern int systemFrameSkip;
extern int systemSaveUpdateCounter;
extern int systemSpeed;
#define MAX_CHEATS 16384
#define SYSTEM_SAVE_UPDATED 30
#define SYSTEM_SAVE_NOT_UPDATED 0
#endif // SYSTEM_H
