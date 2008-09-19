#ifndef GB_H
#define GB_H

#include "../shared/System.h"

#define C_FLAG 0x10
#define H_FLAG 0x20
#define N_FLAG 0x40
#define Z_FLAG 0x80

typedef union {
  struct {
#ifdef WORDS_BIGENDIAN
    u8 B1, B0;
#else
    u8 B0,B1;
#endif
  } B;
  u16 W;
} gbRegister;

bool gbLoadRom(const char *);
bool gbLoadRomFromMemory( unsigned char *memory, const int size );
void gbEmulate(int);
void gbWriteMemory(register u16, register u8);
void gbDrawLine();
bool gbIsGameboyRom(const char *);
void gbSoundReset();
void gbSoundSetQuality(int);
void gbGetHardwareType();
void gbReset();
void gbCleanUp();
void gbCPUInit(const char *,bool);
bool gbWriteBatteryFile(const char *);
bool gbWriteBatteryFile(const char *, bool);
bool gbReadBatteryFile(const char *);
bool gbWriteSaveState(const char *);
bool gbWriteMemSaveState(char *, int);
bool gbReadSaveState(const char *);
bool gbReadMemSaveState(char *, int);
void gbSgbRenderBorder();
bool gbWritePNGFile(const char *);
bool gbWriteBMPFile(const char *);
bool gbReadGSASnapshot(const char *);

extern struct EmulatedSystem GBSystem;

#endif
