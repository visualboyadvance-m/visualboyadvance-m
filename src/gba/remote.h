#ifndef REMOTE_H
#define REMOTE_H

#include "../common/Types.h"
#include "GBA.h"

#define BitSet(array, bit) \
  ((u8 *)(array))[(bit) >> 3] |= (1 << ((bit) & 7))

#define BitClear(array, bit) \
  ((u8 *)(array))[(bit) >> 3] &= ~(1 << ((bit) & 7))

#define BitGet(array, bit) \
  ((u8)((array)[(bit) >> 3]) & (u8)(1 <<((bit) & 7)))

#define BreakSet(array, addr, flag) \
  ((u8 *)(array))[(addr)>>1] |= ((addr&1) ? (flag<<4) : (flag&0xf))

#define BreakClear(array, addr, flag) \
  ((u8 *)(array))[(addr)>>1] &= ~((addr&1) ? (flag<<4) : (flag&0xf))

//check
#define BreakThumbCheck(array, addr) \
	((u8 *)(array))[(addr)>>1] &((addr&1) ? 0x80 : 0x8 )

#define BreakARMCheck(array, addr) \
  ((u8 *)(array))[(addr)>>1] & ((addr&1) ? 0x40 : 0x4 )

#define BreakReadCheck(array, addr) \
  ((u8 *)(array))[(addr)>>1] & ((addr&1) ? 0x20 : 0x2 )

#define BreakWriteCheck(array, addr) \
  ((u8 *)(array))[(addr)>>1] & ((addr&1) ? 0x10 : 0x1 )

#define BreakCheck(array, addr, flag) \
  ((u8 *)(array))[(addr)>>1] & ((addr&1) ? (flag<<4) : (flag&0xf))

extern bool debugger;

extern bool dexp_eval(char *, u32*);
extern void dexp_setVar(char *, u32);
extern void dexp_listVars();
extern void dexp_saveVars(char *);
extern void dexp_loadVars(char *);

void debuggerOutput(const char *s, u32 addr);

bool debuggerBreakOnExecution(u32 address, u8 state);
bool debuggerBreakOnWrite(u32 address, u32 value, int size);
void debuggerBreakOnWrite(u32 address, u32 oldvalue, u32 value, int size, int t);
bool debuggerBreakOnRead(u32 address, int size);

struct regBreak{
	//u8 regNum; /No longer needed
	// bit 0 = equal
	// bit 1 = greater
	// bit 2 = smaller
	// bit 3 = signed
	u8 flags;
	u32 intVal;
	struct regBreak* next;
};
extern u8 lowRegBreakCounter[4]; //(r0-r3)
extern u8 medRegBreakCounter[4]; //(r4-r7)
extern u8 highRegBreakCounter[4]; //(r8-r11)
extern u8 statusRegBreakCounter[4]; //(r12-r15)

extern bool enableRegBreak;
extern regBreak* breakRegList[16];
extern void breakReg_check(int i);

struct regBreak* getFromBreakRegList(u8 regnum, int location);

void clearBreakRegList();
void clearParticularRegListBreaks(int reg);
void deleteFromBreakRegList(u8 regnum, int location);

void addBreakRegToList(u8 regnum, u8 flags, u32 value);
void printBreakRegList(bool verbose);

void remoteStubMain();
void remoteStubSignal(int sig, int number);
void remoteOutput(const char *s, u32 addr);
void remoteSetProtocol(int p);
void remoteSetPort(int port);

#endif // REMOTE_H
