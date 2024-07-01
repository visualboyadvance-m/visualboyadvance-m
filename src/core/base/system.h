#ifndef VBAM_CORE_BASE_SYSTEM_H_
#define VBAM_CORE_BASE_SYSTEM_H_

#include <cstdint>
#include <memory>

#include "core/base/sound_driver.h"

enum IMAGE_TYPE {
    IMAGE_UNKNOWN = -1,
    IMAGE_GBA = 0, 
    IMAGE_GB = 1
};

struct EmulatedSystem {
    // main emulation function
    void (*emuMain)(int);
    // reset emulator
    void (*emuReset)();
    // clean up memory
    void (*emuCleanUp)();
    // load battery file
    bool (*emuReadBattery)(const char*);
    // write battery file
    bool (*emuWriteBattery)(const char*);
#ifdef __LIBRETRO__
    // load state
    bool (*emuReadState)(const uint8_t*);
    // load state
    unsigned (*emuWriteState)(uint8_t*);
#else
    // load state
    bool (*emuReadState)(const char*);
    // save state
    bool (*emuWriteState)(const char*);
#endif
    // load memory state (rewind)
    bool (*emuReadMemState)(char*, int);
    // write memory state (rewind)
    bool (*emuWriteMemState)(char*, int, long&);
    // write PNG file
    bool (*emuWritePNG)(const char*);
    // write BMP file
    bool (*emuWriteBMP)(const char*);
    // emulator update CPSR (ARM only)
    void (*emuUpdateCPSR)();
    // emulator has debugger
    bool emuHasDebugger;
    // clock ticks to emulate
    int emuCount;
};

// The `coreOptions` object must be instantiated by the embedder.
extern struct CoreOptions {
    bool cpuIsMultiBoot = false;
    bool mirroringEnable = true;
    bool skipBios = false;
    bool parseDebug = true;
    bool speedHack = false;
    bool speedup = false;
    bool speedup_throttle_frame_skip = false;
    bool speedup_mute = true;
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
    const char* loadDotCodeFile = nullptr;
    const char* saveDotCodeFile = nullptr;
} coreOptions;

// The following functions must be implemented by the emulator.
extern void log(const char*, ...);
extern bool systemPauseOnFrame();
extern void systemGbPrint(uint8_t*, int, int, int, int, int);
extern void systemScreenCapture(int);
extern void systemDrawScreen();
extern void systemSendScreen();
// updates the joystick data
extern bool systemReadJoypads();
// return information about the given joystick, -1 for default joystick
extern uint32_t systemReadJoypad(int);
extern uint32_t systemGetClock();
extern void systemSetTitle(const char*);
extern std::unique_ptr<SoundDriver> systemSoundInit();
extern void systemOnWriteDataToSoundBuffer(const uint16_t* finalWave, int length);
extern void systemOnSoundShutdown();
extern void systemScreenMessage(const char*);
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
extern void (*dbgOutput)(const char* s, uint32_t addr);
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

#endif  // VBAM_CORE_BASE_SYSTEM_H_
