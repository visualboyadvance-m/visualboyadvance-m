#include "core/base/system.h"

void systemMessage(int, const char*, ...) {}

void log(const char*, ...) {}

bool systemPauseOnFrame() {
    return false;
}

void systemGbPrint(uint8_t*, int, int, int, int, int) {}

void systemScreenCapture(int) {}

void systemDrawScreen() {}

void systemSendScreen() {}

bool systemReadJoypads() {
    return false;
}

uint32_t systemReadJoypad(int) {
    return 0;
}

uint32_t systemGetClock() {
    return 0;
}

void systemSetTitle(const char*) {}

std::unique_ptr<SoundDriver> systemSoundInit() {
    return NULL;
}

void systemOnWriteDataToSoundBuffer(const uint16_t* /*finalWave*/, int /*length*/) {}

void systemOnSoundShutdown() {}

void systemScreenMessage(const char*) {}

void systemUpdateMotionSensor() {}

int systemGetSensorX() {
    return 0;
}

int systemGetSensorY() {
    return 0;
}

int systemGetSensorZ() {
    return 0;
}

uint8_t systemGetSensorDarkness() {
    return 0;
}

void systemCartridgeRumble(bool) {}

void systemPossibleCartridgeRumble(bool) {}

void updateRumbleFrame() {}

bool systemCanChangeSoundQuality() {
    return false;
}

void systemShowSpeed(int) {}

void system10Frames() {}

void systemFrame() {}

void systemGbBorderOn() {}

void (*dbgOutput)(const char* s, uint32_t addr);
void (*dbgSignal)(int sig, int number);

uint8_t  systemColorMap8[0x10000];
uint16_t systemColorMap16[0x10000];
uint32_t systemColorMap32[0x10000];
uint16_t systemGbPalette[24];
int systemRedShift;
int systemGreenShift;
int systemBlueShift;
int systemColorDepth;
int systemVerbose;
int systemFrameSkip;
int systemSaveUpdateCounter;
int systemSpeed;

int emulating = 0;
