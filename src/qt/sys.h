#ifndef VBAM_QT_SYS_H_
#define VBAM_QT_SYS_H_

// Frontend-side declarations shared between sys.cpp (core callbacks), the
// game area and the command handlers. Equivalent of the tail of wxvbam.h.

#include <cstdint>
#include <vector>

#include <QString>

#include "core/base/system.h"

#if defined(VBAM_ENABLE_DEBUGGER)
extern bool debugger;
extern void (*dbgMain)();
extern void (*dbgSignal)(int, int);
extern void (*dbgOutput)(const char*, uint32_t);
extern void remoteStubMain();
extern void remoteCleanUp();
extern void remoteStubSignal(int, int);
extern void remoteOutput(const char*, uint32_t);

// GDB stub transports (sys.cpp): pseudo terminal and TCP (QTcpServer).
extern bool debugOpenPty();
extern const QString& debugGetSlavePty();
extern bool debugWaitPty();
extern bool debugStartListen(int port);
extern bool debugWaitSocket();
#endif  // defined(VBAM_ENABLE_DEBUGGER)

// supported movie format for game recording
enum MVFormatID {
    MV_FORMAT_ID_NONE,

    /* movie formats */
    MV_FORMAT_ID_VMV,
    MV_FORMAT_ID_VMV1,
    MV_FORMAT_ID_VMV2,
};
std::vector<MVFormatID> getSupMovFormatsToRecord();
std::vector<char*> getSupMovNamesToRecord();
std::vector<char*> getSupMovExtsToRecord();
std::vector<MVFormatID> getSupMovFormatsToPlayback();
std::vector<char*> getSupMovNamesToPlayback();
std::vector<char*> getSupMovExtsToPlayback();

// Game (input) recording / playback; these integrate with systemReadJoypad.
void systemStartGameRecording(const QString& fname, MVFormatID format);
// Path of the movie being recorded, empty when none.
QString systemGameRecordingFile();
void systemStopGameRecording();
void systemStartGamePlayback(const QString& fname, MVFormatID format);
void systemStopGamePlayback();

// Updates the solar sensor from the current key state (called per frame).
void systemUpdateSolarSensor();

// true if turbo mode (like pressing turbo button constantly)
extern bool turbo;

extern int autofire, autohold;

// Joypad bit masks.
#define KEYM_A (1 << 0)
#define KEYM_B (1 << 1)
#define KEYM_SELECT (1 << 2)
#define KEYM_START (1 << 3)
#define KEYM_RIGHT (1 << 4)
#define KEYM_LEFT (1 << 5)
#define KEYM_UP (1 << 6)
#define KEYM_DOWN (1 << 7)
#define KEYM_R (1 << 8)
#define KEYM_L (1 << 9)
#define KEYM_SPEED (1 << 10)
#define KEYM_CAPTURE (1 << 11)
#define KEYM_GS (1 << 12)

#define REALKEY_MASK ((1 << 13) - 1)

#define KEYM_AUTO_A (1 << 13)
#define KEYM_AUTO_B (1 << 14)
#define KEYM_MOTION_UP (1 << 15)
#define KEYM_MOTION_DOWN (1 << 16)
#define KEYM_MOTION_LEFT (1 << 17)
#define KEYM_MOTION_RIGHT (1 << 18)
#define KEYM_MOTION_IN (1 << 19)
#define KEYM_MOTION_OUT (1 << 20)

#endif  // VBAM_QT_SYS_H_
