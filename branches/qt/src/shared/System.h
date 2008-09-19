#ifndef SYSTEM_H
#define SYSTEM_H

#include <stdint.h>

#include <zlib.h>

#ifndef NULL
#define NULL 0
#endif


typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;



typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

struct EmulatedSystem {
    // main emulation function
    void ( *emuMain )( int );
    // reset emulator
    void ( *emuReset )();
    // clean up memory
    void ( *emuCleanUp )();
    // load battery file
    bool ( *emuReadBattery )( const char * );
    // write battery file
    bool ( *emuWriteBattery )( const char * );
    // load state
    bool ( *emuReadState )( const char * );
    // save state
    bool ( *emuWriteState )( const char * );
    // load memory state (rewind)
    bool ( *emuReadMemState )( char *, int );
    // write memory state (rewind)
    bool ( *emuWriteMemState )( char *, int );
    // write PNG file
    bool ( *emuWritePNG )( const char * );
    // write BMP file
    bool ( *emuWriteBMP )( const char * );
    // emulator update CPSR (ARM only)
    void ( *emuUpdateCPSR )();
    // emulator has debugger
    bool emuHasDebugger;
    // clock ticks to emulate
    int emuCount;
};

void log( const char *, ... );

bool systemPauseOnFrame();
void systemGbPrint( u8 *, int, int, int, int );
void systemScreenCapture( int );
void systemDrawScreen();
bool systemReadJoypads(); // updates the joystick data
u32 systemReadJoypad( int ); // return information about the given joystick, -1 for default joystick
u32 systemGetClock();
void systemMessage( int, const char *, ... );
void systemSetTitle( const char * );
void systemWriteDataToSoundBuffer();
void systemSoundShutdown();
void systemSoundPause();
void systemSoundResume();
void systemSoundReset();
bool systemSoundInit();
void systemScreenMessage( const char * );
void systemUpdateMotionSensor();
int  systemGetSensorX();
int  systemGetSensorY();
bool systemCanChangeSoundQuality();
void systemShowSpeed( int );
void system10Frames( int );
void systemFrame();
void systemGbBorderOn();

extern void Sm60FPS_Init();
extern bool Sm60FPS_CanSkipFrame();
extern void Sm60FPS_Sleep();
extern void DbgMsg( const char *msg, ... );
extern void winlog( const char *, ... );

extern void ( *dbgOutput )( const char *s, u32 addr );
extern void ( *dbgSignal )( int sig, int number );

extern bool systemSoundOn;
extern u16 systemColorMap16[0x10000];
extern u32 systemColorMap32[0x10000];
extern u16 systemGbPalette[24];
extern int systemRedShift;
extern int systemGreenShift;
extern int systemBlueShift;
extern int systemColorDepth;
extern int systemDebug;
extern int systemVerbose;
extern int systemFrameSkip;
extern int systemSaveUpdateCounter;
extern int systemSpeed;
extern int systemThrottle;

#define SYSTEM_SAVE_UPDATED 30
#define SYSTEM_SAVE_NOT_UPDATED 0

#endif
