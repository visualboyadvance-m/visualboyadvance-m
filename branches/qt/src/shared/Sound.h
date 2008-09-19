#ifndef VBA_SOUND_H
#define VBA_SOUND_H

#include "System.h"

#define NR10 0x60
#define NR11 0x62
#define NR12 0x63
#define NR13 0x64
#define NR14 0x65
#define NR21 0x68
#define NR22 0x69
#define NR23 0x6c
#define NR24 0x6d
#define NR30 0x70
#define NR31 0x72
#define NR32 0x73
#define NR33 0x74
#define NR34 0x75
#define NR41 0x78
#define NR42 0x79
#define NR43 0x7c
#define NR44 0x7d
#define NR50 0x80
#define NR51 0x81
#define NR52 0x84
#define SGCNT0_H 0x82
#define FIFOA_L 0xa0
#define FIFOA_H 0xa2
#define FIFOB_L 0xa4
#define FIFOB_H 0xa6

extern void (*psoundTickfn)();
void soundShutdown();
bool soundInit();
void soundPause();
void soundResume();
void soundEnable(int);
void soundDisable(int);
int  soundGetEnable();
void soundReset();
void soundSaveGame(gzFile);
void soundReadGame(gzFile, int);
void soundEvent(u32, u8);
void soundEvent(u32, u16);
void soundTimerOverflow(int);
void soundSetQuality(int);
void setsystemSoundOn(bool value);
void setsoundPaused(bool value);
void interp_rate();

extern int SOUND_CLOCK_TICKS;   // Number of 16.8 MHz clocks between calls to soundTick()
extern int soundTicks;          // Number of 16.8 MHz clocks until soundTick() will be called
extern int soundQuality;        // sample rate = 44100 / soundQuality
extern int soundBufferLen;      // size of sound buffer in BYTES
extern u16 soundFinalWave[1470];// 16-bit SIGNED stereo sample buffer
extern int soundVolume;         // emulator volume code (not linear)

extern int soundInterpolation;  // 1 if PCM should have low-pass filtering
extern float soundFiltering;    // 0.0 = none, 1.0 = max (only if soundInterpolation!=0)

extern bool soundEcho;          // enables echo for GB, not GBA

// Not used anymore
extern bool soundLowPass;
extern bool soundReverse;

// Unknown purpose
extern int soundBufferTotalLen;
extern u32 soundNextPosition;
extern bool soundPaused;

#endif // VBA_SOUND_H
