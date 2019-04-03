#ifndef __LIBRETRO__
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <iosfwd>
#include <sstream>
#include <vector>

#ifndef _WIN32
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif // HAVE_NETINET_IN_H
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#else // ! HAVE_ARPA_INET_H
#define socklen_t int
#endif // ! HAVE_ARPA_INET_H
#define SOCKET int
#else // _WIN32
#include <io.h>
#include <winsock.h>
#define socklen_t int
#define close closesocket
#define read _read
#define write _write
#define strdup _strdup
#endif // _WIN32

#include "BreakpointStructures.h"
#include "GBA.h"
#include "elf.h"
#include "remote.h"
#include <iomanip>
#include <iostream>

extern bool debugger;
extern int emulating;
extern void CPUUpdateCPSR();

int remotePort = 0;
int remoteSignal = 5;
SOCKET remoteSocket = -1;
SOCKET remoteListenSocket = -1;
bool remoteConnected = false;
bool remoteResumed = false;

int (*remoteSendFnc)(char*, int) = NULL;
int (*remoteRecvFnc)(char*, int) = NULL;
bool (*remoteInitFnc)() = NULL;
void (*remoteCleanUpFnc)() = NULL;

#ifndef SDL
void remoteSetSockets(SOCKET l, SOCKET r)
{
    remoteSocket = r;
    remoteListenSocket = l;
}
#endif

#define debuggerReadMemory(addr) \
    (*(uint32_t*)&map[(addr) >> 24].address[(addr)&map[(addr) >> 24].mask])

#define debuggerReadHalfWord(addr) \
    (*(uint16_t*)&map[(addr) >> 24].address[(addr)&map[(addr) >> 24].mask])

#define debuggerReadByte(addr) \
    map[(addr) >> 24].address[(addr)&map[(addr) >> 24].mask]

#define debuggerWriteMemory(addr, value) \
    *(uint32_t*)&map[(addr) >> 24].address[(addr)&map[(addr) >> 24].mask] = (value)

#define debuggerWriteHalfWord(addr, value) \
    *(uint16_t*)&map[(addr) >> 24].address[(addr)&map[(addr) >> 24].mask] = (value)

#define debuggerWriteByte(addr, value) \
    map[(addr) >> 24].address[(addr)&map[(addr) >> 24].mask] = (value)

bool dontBreakNow = false;
int debuggerNumOfDontBreak = 0;
int debuggerRadix = 0;

#define NUMBEROFDB 1000
uint32_t debuggerNoBreakpointList[NUMBEROFDB];

const char* cmdAliasTable[] = { "help", "?", "h", "?", "continue", "c", "next", "n",
    "cpyb", "copyb", "cpyh", "copyh", "cpyw", "copyw",
    "exe", "execute", "exec", "execute",
    NULL, NULL };

struct DebuggerCommand {
    const char* name;
    void (*function)(int, char**);
    const char* help;
    const char* syntax;
};

char monbuf[1000];
void monprintf(std::string line);
std::string StringToHex(std::string& cmd);
std::string HexToString(char* p);
void debuggerUsage(const char* cmd);
void debuggerHelp(int n, char** args);
void printFlagHelp();
void dbgExecute(std::string& cmd);

extern bool debuggerBreakOnWrite(uint32_t, uint32_t, int);
extern bool debuggerBreakOnRegisterCondition(uint8_t, uint32_t, uint32_t, uint8_t);
extern bool debuggerBreakOnExecution(uint32_t, uint8_t);

regBreak* breakRegList[16];
uint8_t lowRegBreakCounter[4]; //(r0-r3)
uint8_t medRegBreakCounter[4]; //(r4-r7)
uint8_t highRegBreakCounter[4]; //(r8-r11)
uint8_t statusRegBreakCounter[4]; //(r12-r15)
uint8_t* regBreakCounter[4] = {
    &lowRegBreakCounter[0],
    &medRegBreakCounter[0],
    &highRegBreakCounter[0],
    &statusRegBreakCounter[0]
};
uint32_t lastWasBranch = 0;

struct regBreak* getFromBreakRegList(uint8_t regnum, int location)
{
    if (location > regBreakCounter[regnum >> 2][regnum & 3])
        return NULL;

    struct regBreak* ans = breakRegList[regnum];
    for (int i = 0; i < location && ans; i++) {
        ans = ans->next;
    }
    return ans;
}

bool enableRegBreak = false;
reg_pair oldReg[16];
uint32_t regDiff[16];

void breakReg_check(int i)
{
    struct regBreak* brkR = breakRegList[i];
    bool notFound = true;
    uint8_t counter = regBreakCounter[i >> 2][i & 3];
    for (int bri = 0; (bri < counter) && notFound; bri++) {
        if (!brkR) {
            regBreakCounter[i >> 2][i & 3] = (uint8_t)bri;
            break;
        } else {
            if (brkR->flags != 0) {
                uint32_t regVal = (i == 15 ? (armState ? reg[15].I - 4 : reg[15].I - 2) : reg[i].I);
                if ((brkR->flags & 0x1) && (regVal == brkR->intVal)) {
                    debuggerBreakOnRegisterCondition(i, brkR->intVal, regVal, 1);
                    notFound = false;
                }
                if ((brkR->flags & 0x8)) {
                    if ((brkR->flags & 0x4) && ((int)regVal < (int)brkR->intVal)) {
                        debuggerBreakOnRegisterCondition(i, brkR->intVal, regVal, 4);
                        notFound = false;
                    }
                    if ((brkR->flags & 0x2) && ((int)regVal > (int)brkR->intVal)) {
                        debuggerBreakOnRegisterCondition(i, brkR->intVal, regVal, 5);
                        notFound = false;
                    }
                }
                if ((brkR->flags & 0x4) && (regVal < brkR->intVal)) {
                    debuggerBreakOnRegisterCondition(i, brkR->intVal, regVal, 2);
                    notFound = false;
                }
                if ((brkR->flags & 0x2) && (regVal > brkR->intVal)) {
                    debuggerBreakOnRegisterCondition(i, brkR->intVal, regVal, 3);
                    notFound = false;
                }
            }
            brkR = brkR->next;
        }
    }
    if (!notFound) {
        //CPU_BREAK_LOOP_2;
    }
}

void clearParticularRegListBreaks(int regNum)
{

    while (breakRegList[regNum]) {
        struct regBreak* ans = breakRegList[regNum]->next;
        free(breakRegList[regNum]);
        breakRegList[regNum] = ans;
    }
    regBreakCounter[regNum >> 2][regNum & 3] = 0;
}

void clearBreakRegList()
{
    for (int i = 0; i < 16; i++) {
        clearParticularRegListBreaks(i);
    }
}

void deleteFromBreakRegList(uint8_t regNum, int num)
{
    int counter = regBreakCounter[regNum >> 2][regNum & 3];
    if (num >= counter) {
        return;
    }
    struct regBreak* ans = breakRegList[regNum];
    struct regBreak* prev = NULL;
    for (int i = 0; i < num; i++) {
        prev = ans;
        ans = ans->next;
    }
    if (prev) {
        prev->next = ans->next;
    } else {
        breakRegList[regNum] = ans->next;
    }
    free(ans);
    regBreakCounter[regNum >> 2][regNum & 3]--;
}

void addBreakRegToList(uint8_t regnum, uint8_t flags, uint32_t value)
{
    struct regBreak* ans = (struct regBreak*)malloc(sizeof(struct regBreak));
    ans->flags = flags;
    ans->intVal = value;
    ans->next = breakRegList[regnum];
    breakRegList[regnum] = ans;
    regBreakCounter[regnum >> 2][regnum & 3]++;
}

void printBreakRegList(bool verbose)
{
    const char* flagsToOP[] = { "never", "==", ">", ">=", "<", "<=", "!=", "always" };
    bool anyPrint = false;
    for (int i = 0; i < 4; i++) {
        for (int k = 0; k < 4; k++) {
            if (regBreakCounter[i][k]) {
                if (!anyPrint) {
                    {
                        sprintf(monbuf, "Register breakpoint list:\n");
                        monprintf(monbuf);
                    }
                    {
                        sprintf(monbuf, "-------------------------\n");
                        monprintf(monbuf);
                    }
                    anyPrint = true;
                }
                struct regBreak* tmp = breakRegList[i * 4 + k];
                for (int j = 0; j < regBreakCounter[i][k]; j++) {
                    if (tmp->flags & 8) {
                        sprintf(monbuf, "No. %d:\tBreak if (signed)%s %08x\n", j, flagsToOP[tmp->flags & 7], tmp->intVal);
                        monprintf(monbuf);
                    } else {
                        sprintf(monbuf, "No. %d:\tBreak if %s %08x\n", j, flagsToOP[tmp->flags], tmp->intVal);
                        monprintf(monbuf);
                    }
                    tmp = tmp->next;
                }
                {
                    sprintf(monbuf, "-------------------------\n");
                    monprintf(monbuf);
                }
            } else {
                if (verbose) {
                    if (!anyPrint) {
                        {
                            sprintf(monbuf, "Register breakpoint list:\n");
                            monprintf(monbuf);
                        }
                        {
                            sprintf(monbuf, "-------------------------\n");
                            monprintf(monbuf);
                        }
                        anyPrint = true;
                    }
                    {
                        sprintf(monbuf, "No breaks on r%d.\n", i);
                        monprintf(monbuf);
                    }
                    {
                        sprintf(monbuf, "-------------------------\n");
                        monprintf(monbuf);
                    }
                }
            }
        }
    }
    if (!verbose && !anyPrint) {
        {
            sprintf(monbuf, "No Register breaks found.\n");
            monprintf(monbuf);
        }
    }
}

void debuggerOutput(const char* s, uint32_t addr)
{
    if (s)
        printf("%s", s);
    else {
        char c;

        c = debuggerReadByte(addr);
        addr++;
        while (c) {
            putchar(c);
            c = debuggerReadByte(addr);
            addr++;
        }
    }
}

// checks that the given address is in the DB list
bool debuggerInDB(uint32_t address)
{

    for (int i = 0; i < debuggerNumOfDontBreak; i++) {
        if (debuggerNoBreakpointList[i] == address)
            return true;
    }

    return false;
}

void debuggerDontBreak(int n, char** args)
{
    if (n == 2) {
        uint32_t address = 0;
        sscanf(args[1], "%x", &address);
        int i = debuggerNumOfDontBreak;
        if (i > NUMBEROFDB) {
            monprintf("Can't have this many DB entries");
            return;
        }
        debuggerNoBreakpointList[i] = address;
        debuggerNumOfDontBreak++;
        {
            sprintf(monbuf, "Added Don't Break at %08x\n", address);
            monprintf(monbuf);
        }
    } else
        debuggerUsage("db");
}

void debuggerDontBreakClear(int n, char** args)
{
    (void)args; // unused params
    if (n == 1) {
        debuggerNumOfDontBreak = 0;
        {
            sprintf(monbuf, "Cleared Don't Break list.\n");
            monprintf(monbuf);
        }
    } else
        debuggerUsage("dbc");
}

void debuggerDumpLoad(int n, char** args)
{
    uint32_t address;
    char* file;
    FILE* f;
    int c;

    if (n == 3) {
        file = args[1];

        if (!dexp_eval(args[2], &address)) {
            {
                sprintf(monbuf, "Invalid expression in address.\n");
                monprintf(monbuf);
            }
            return;
        }

        f = fopen(file, "rb");
        if (f == NULL) {
            {
                sprintf(monbuf, "Error opening file.\n");
                monprintf(monbuf);
            }
            return;
        }

        fseek(f, 0, SEEK_END);
        int size = ftell(f);
        fseek(f, 0, SEEK_SET);

        for (int i = 0; i < size; i++) {
            c = fgetc(f);
            if (c == -1)
                break;
            debuggerWriteByte(address, c);
            address++;
        }

        fclose(f);
    } else
        debuggerUsage("dload");
}

void debuggerDumpSave(int n, char** args)
{
    uint32_t address;
    uint32_t size;
    char* file;
    FILE* f;

    if (n == 4) {
        file = args[1];
        if (!dexp_eval(args[2], &address)) {
            {
                sprintf(monbuf, "Invalid expression in address.\n");
                monprintf(monbuf);
            }
            return;
        }
        if (!dexp_eval(args[3], &size)) {
            {
                sprintf(monbuf, "Invalid expression in size");
                monprintf(monbuf);
            }
            return;
        }

        f = fopen(file, "wb");
        if (f == NULL) {
            {
                sprintf(monbuf, "Error opening file.\n");
                monprintf(monbuf);
            }
            return;
        }

        for (uint32_t i = 0; i < size; i++) {
            fputc(debuggerReadByte(address), f);
            address++;
        }

        fclose(f);
    } else
        debuggerUsage("dsave");
}

void debuggerEditByte(int n, char** args)
{
    if (n >= 3) {
        uint32_t address;
        uint32_t value;
        if (!dexp_eval(args[1], &address)) {
            {
                sprintf(monbuf, "Invalid expression in address.\n");
                monprintf(monbuf);
            }
            return;
        }
        for (int i = 2; i < n; i++) {
            if (!dexp_eval(args[i], &value)) {
                {
                    sprintf(monbuf, "Invalid expression in %d value.Ignored.\n", (i - 1));
                    monprintf(monbuf);
                }
            }
            debuggerWriteByte(address, (uint16_t)value);
            address++;
        }
    } else
        debuggerUsage("eb");
}

void debuggerEditHalfWord(int n, char** args)
{
    if (n >= 3) {
        uint32_t address;
        uint32_t value;
        if (!dexp_eval(args[1], &address)) {
            {
                sprintf(monbuf, "Invalid expression in address.\n");
                monprintf(monbuf);
            }
            return;
        }
        if (address & 1) {
            {
                sprintf(monbuf, "Error: address must be half-word aligned\n");
                monprintf(monbuf);
            }
            return;
        }
        for (int i = 2; i < n; i++) {
            if (!dexp_eval(args[i], &value)) {
                {
                    sprintf(monbuf, "Invalid expression in %d value.Ignored.\n", (i - 1));
                    monprintf(monbuf);
                }
            }
            debuggerWriteHalfWord(address, (uint16_t)value);
            address += 2;
        }
    } else
        debuggerUsage("eh");
}

void debuggerEditWord(int n, char** args)
{
    if (n >= 3) {
        uint32_t address;
        uint32_t value;
        if (!dexp_eval(args[1], &address)) {
            {
                sprintf(monbuf, "Invalid expression in address.\n");
                monprintf(monbuf);
            }
            return;
        }
        if (address & 3) {
            {
                sprintf(monbuf, "Error: address must be word aligned\n");
                monprintf(monbuf);
            }
            return;
        }
        for (int i = 2; i < n; i++) {
            if (!dexp_eval(args[i], &value)) {
                {
                    sprintf(monbuf, "Invalid expression in %d value.Ignored.\n", (i - 1));
                    monprintf(monbuf);
                }
            }
            debuggerWriteMemory(address, (uint32_t)value);
            address += 4;
        }
    } else
        debuggerUsage("ew");
}

bool debuggerBreakOnRegisterCondition(uint8_t registerName, uint32_t compareVal, uint32_t regVal, uint8_t type)
{
    const char* typeName;
    switch (type) {
    case 1:
        typeName = "equal to";
        break;
    case 2:
        typeName = "greater (unsigned) than";
        break;
    case 3:
        typeName = "smaller (unsigned) than";
        break;
    case 4:
        typeName = "greater (signed) than";
        break;
    case 5:
        typeName = "smaller (signed) than";
        break;
    default:
        typeName = "unknown";
    }
    {
        sprintf(monbuf, "Breakpoint on R%02d : %08x is %s register content (%08x)\n", registerName, compareVal, typeName, regVal);
        monprintf(monbuf);
    }
    if (debuggerInDB(armState ? reg[15].I - 4 : reg[15].I - 2)) {
        {
            sprintf(monbuf, "But this address is marked not to break, so skipped\n");
            monprintf(monbuf);
        }
        return false;
    }
    debugger = true;
    return true;
}

void debuggerBreakRegisterList(bool verbose)
{
    printBreakRegList(verbose);
}

int getRegisterNumber(char* regName)
{
    int r = -1;
    if (toupper(regName[0]) == 'P' && toupper(regName[1]) == 'C') {
        r = 15;
    } else if (toupper(regName[0]) == 'L' && toupper(regName[1]) == 'R') {
        r = 14;
    } else if (toupper(regName[0]) == 'S' && toupper(regName[1]) == 'P') {
        r = 13;
    } else if (toupper(regName[0]) == 'R') {
        sscanf((char*)(regName + 1), "%d", &r);
    } else {
        sscanf(regName, "%d", &r);
    }

    return r;
}

void debuggerEditRegister(int n, char** args)
{
    if (n == 3) {
        int r = getRegisterNumber(args[1]);
        uint32_t val;
        if (r > 16) {
            {
                sprintf(monbuf, "Error: Register must be valid (0-16)\n");
                monprintf(monbuf);
            }
            return;
        }
        if (!dexp_eval(args[2], &val)) {
            {
                sprintf(monbuf, "Invalid expression in value.\n");
                monprintf(monbuf);
            }
            return;
        }
        reg[r].I = val;
        {
            sprintf(monbuf, "R%02d=%08X\n", r, val);
            monprintf(monbuf);
        }
    } else
        debuggerUsage("er");
}

void debuggerEval(int n, char** args)
{
    if (n == 2) {
        uint32_t result = 0;
        if (dexp_eval(args[1], &result)) {
            {
                sprintf(monbuf, " =$%08X\n", result);
                monprintf(monbuf);
            }
        } else {
            {
                sprintf(monbuf, "Invalid expression\n");
                monprintf(monbuf);
            }
        }
    } else
        debuggerUsage("eval");
}

void debuggerFillByte(int n, char** args)
{
    if (n == 4) {
        uint32_t address;
        uint32_t value;
        uint32_t reps;
        if (!dexp_eval(args[1], &address)) {
            {
                sprintf(monbuf, "Invalid expression in address.\n");
                monprintf(monbuf);
            }
            return;
        }
        if (!dexp_eval(args[2], &value)) {
            {
                sprintf(monbuf, "Invalid expression in value.\n");
                monprintf(monbuf);
            }
        }
        if (!dexp_eval(args[3], &reps)) {
            {
                sprintf(monbuf, "Invalid expression in repetition number.\n");
                monprintf(monbuf);
            }
        }
        for (uint32_t i = 0; i < reps; i++) {
            debuggerWriteByte(address, (uint8_t)value);
            address++;
        }
    } else
        debuggerUsage("fillb");
}

void debuggerFillHalfWord(int n, char** args)
{
    if (n == 4) {
        uint32_t address;
        uint32_t value;
        uint32_t reps;
        if (!dexp_eval(args[1], &address)) {
            {
                sprintf(monbuf, "Invalid expression in address.\n");
                monprintf(monbuf);
            }
            return;
        } /*
		 if(address & 1) {
		 { sprintf(monbuf, "Error: address must be halfword aligned\n"); monprintf(monbuf); }
		 return;
		 }*/
        if (!dexp_eval(args[2], &value)) {
            {
                sprintf(monbuf, "Invalid expression in value.\n");
                monprintf(monbuf);
            }
        }
        if (!dexp_eval(args[3], &reps)) {
            {
                sprintf(monbuf, "Invalid expression in repetition number.\n");
                monprintf(monbuf);
            }
        }
        for (uint32_t i = 0; i < reps; i++) {
            debuggerWriteHalfWord(address, (uint16_t)value);
            address += 2;
        }
    } else
        debuggerUsage("fillh");
}

void debuggerFillWord(int n, char** args)
{
    if (n == 4) {
        uint32_t address;
        uint32_t value;
        uint32_t reps;
        if (!dexp_eval(args[1], &address)) {
            {
                sprintf(monbuf, "Invalid expression in address.\n");
                monprintf(monbuf);
            }
            return;
        } /*
		 if(address & 3) {
		 { sprintf(monbuf, "Error: address must be word aligned\n"); monprintf(monbuf); }
		 return;
		 }*/
        if (!dexp_eval(args[2], &value)) {
            {
                sprintf(monbuf, "Invalid expression in value.\n");
                monprintf(monbuf);
            }
        }
        if (!dexp_eval(args[3], &reps)) {
            {
                sprintf(monbuf, "Invalid expression in repetition number.\n");
                monprintf(monbuf);
            }
        }
        for (uint32_t i = 0; i < reps; i++) {
            debuggerWriteMemory(address, (uint32_t)value);
            address += 4;
        }
    } else
        debuggerUsage("fillw");
}

unsigned int SearchStart = 0xFFFFFFFF;
unsigned int SearchMaxMatches = 5;
uint8_t SearchData[64]; // It actually doesn't make much sense to search for more than 64 bytes, does it?
unsigned int SearchLength = 0;
unsigned int SearchResults;

unsigned int AddressToGBA(uint8_t* mem)
{
    if (mem >= &bios[0] && mem <= &bios[0x3fff])
        return 0x00000000 + (mem - &bios[0]);
    else if (mem >= &workRAM[0] && mem <= &workRAM[0x3ffff])
        return 0x02000000 + (mem - &workRAM[0]);
    else if (mem >= &internalRAM[0] && mem <= &internalRAM[0x7fff])
        return 0x03000000 + (mem - &internalRAM[0]);
    else if (mem >= &ioMem[0] && mem <= &ioMem[0x3ff])
        return 0x04000000 + (mem - &ioMem[0]);
    else if (mem >= &paletteRAM[0] && mem <= &paletteRAM[0x3ff])
        return 0x05000000 + (mem - &paletteRAM[0]);
    else if (mem >= &vram[0] && mem <= &vram[0x1ffff])
        return 0x06000000 + (mem - &vram[0]);
    else if (mem >= &oam[0] && mem <= &oam[0x3ff])
        return 0x07000000 + (mem - &oam[0]);
    else if (mem >= &rom[0] && mem <= &rom[0x1ffffff])
        return 0x08000000 + (mem - &rom[0]);
    else
        return 0xFFFFFFFF;
};

void debuggerDoSearch()
{
    unsigned int count = 0;

    while (true) {
        unsigned int final = SearchStart + SearchLength - 1;
        uint8_t* end;
        uint8_t* start;

        switch (SearchStart >> 24) {
        case 0:
            if (final > 0x00003FFF) {
                SearchStart = 0x02000000;
                continue;
            } else {
                start = bios + (SearchStart & 0x3FFF);
                end = bios + 0x3FFF;
                break;
            };
        case 2:
            if (final > 0x0203FFFF) {
                SearchStart = 0x03000000;
                continue;
            } else {
                start = workRAM + (SearchStart & 0x3FFFF);
                end = workRAM + 0x3FFFF;
                break;
            };
        case 3:
            if (final > 0x03007FFF) {
                SearchStart = 0x04000000;
                continue;
            } else {
                start = internalRAM + (SearchStart & 0x7FFF);
                end = internalRAM + 0x7FFF;
                break;
            };
        case 4:
            if (final > 0x040003FF) {
                SearchStart = 0x05000000;
                continue;
            } else {
                start = ioMem + (SearchStart & 0x3FF);
                end = ioMem + 0x3FF;
                break;
            };
        case 5:
            if (final > 0x050003FF) {
                SearchStart = 0x06000000;
                continue;
            } else {
                start = paletteRAM + (SearchStart & 0x3FF);
                end = paletteRAM + 0x3FF;
                break;
            };
        case 6:
            if (final > 0x0601FFFF) {
                SearchStart = 0x07000000;
                continue;
            } else {
                start = vram + (SearchStart & 0x1FFFF);
                end = vram + 0x1FFFF;
                break;
            };
        case 7:
            if (final > 0x070003FF) {
                SearchStart = 0x08000000;
                continue;
            } else {
                start = oam + (SearchStart & 0x3FF);
                end = oam + 0x3FF;
                break;
            };
        case 8:
        case 9:
        case 10:
        case 11:
        case 12:
        case 13:
            if (final <= 0x09FFFFFF) {
                start = rom + (SearchStart & 0x01FFFFFF);
                end = rom + 0x01FFFFFF;
                break;
            };
        default: {
            sprintf(monbuf, "Search completed.\n");
            monprintf(monbuf);
        }
            SearchLength = 0;
            return;
        };

        end -= SearchLength - 1;
        uint8_t firstbyte = SearchData[0];
        while (start <= end) {
            while ((start <= end) && (*start != firstbyte))
                start++;

            if (start > end)
                break;

            unsigned int p = 1;
            while ((start[p] == SearchData[p]) && (p < SearchLength))
                p++;

            if (p == SearchLength) {
                {
                    sprintf(monbuf, "Search result (%d): %08x\n", count + SearchResults, AddressToGBA(start));
                    monprintf(monbuf);
                }
                count++;
                if (count == SearchMaxMatches) {
                    SearchStart = AddressToGBA(start + p);
                    SearchResults += count;
                    return;
                };

                start += p; // assume areas don't overlap; alternative: start++;
            } else
                start++;
        };

        SearchStart = AddressToGBA(end + SearchLength - 1) + 1;
    };
};

void debuggerFindText(int n, char** args)
{
    if ((n == 4) || (n == 3)) {
        SearchResults = 0;
        if (!dexp_eval(args[1], &SearchStart)) {
            {
                sprintf(monbuf, "Invalid expression.\n");
                monprintf(monbuf);
            }
            return;
        }

        if (n == 4) {
            sscanf(args[2], "%u", &SearchMaxMatches);
            strncpy((char*)SearchData, args[3], 64);
            SearchLength = strlen(args[3]);
        } else if (n == 3) {
            strncpy((char*)SearchData, args[2], 64);
            SearchLength = strlen(args[2]);
        };

        if (SearchLength > 64) {
            {
                sprintf(monbuf, "Entered string (length: %d) is longer than 64 bytes and was cut.\n", SearchLength);
                monprintf(monbuf);
            }
            SearchLength = 64;
        };

        debuggerDoSearch();

    } else
        debuggerUsage("ft");
};

void debuggerFindHex(int n, char** args)
{
    if ((n == 4) || (n == 3)) {
        SearchResults = 0;
        if (!dexp_eval(args[1], &SearchStart)) {
            {
                sprintf(monbuf, "Invalid expression.\n");
                monprintf(monbuf);
            }
            return;
        }

        char SearchHex[128];
        if (n == 4) {
            sscanf(args[2], "%u", &SearchMaxMatches);
            strncpy(SearchHex, args[3], 128);
            SearchLength = strlen(args[3]);
        } else if (n == 3) {
            strncpy(SearchHex, args[2], 128);
            SearchLength = strlen(args[2]);
        };

        if (SearchLength & 1) {
            sprintf(monbuf, "Unaligned bytecount: %d,5. Last digit (%c) cut.\n", SearchLength / 2, SearchHex[SearchLength - 1]);
            monprintf(monbuf);
        }

        SearchLength /= 2;

        if (SearchLength > 64) {
            {
                sprintf(monbuf, "Entered string (length: %d) is longer than 64 bytes and was cut.\n", SearchLength);
                monprintf(monbuf);
            }
            SearchLength = 64;
        };

        for (unsigned int i = 0; i < SearchLength; i++) {
            unsigned int cbuf = 0;
            sscanf(&SearchHex[i << 1], "%02x", &cbuf);
            SearchData[i] = cbuf;
        };

        debuggerDoSearch();

    } else
        debuggerUsage("fh");
};

void debuggerFindResume(int n, char** args)
{
    if ((n == 1) || (n == 2)) {
        if (SearchLength == 0) {
            {
                sprintf(monbuf, "Error: No search in progress. Start a search with ft or fh.\n");
                monprintf(monbuf);
            }
            debuggerUsage("fr");
            return;
        };

        if (n == 2)
            sscanf(args[1], "%u", &SearchMaxMatches);

        debuggerDoSearch();

    } else
        debuggerUsage("fr");
};

void debuggerCopyByte(int n, char** args)
{
    uint32_t source;
    uint32_t dest;
    uint32_t number = 1;
    uint32_t reps = 1;
    if (n > 5 || n < 3) {
        debuggerUsage("copyb");
    }

    if (n == 5) {
        if (!dexp_eval(args[4], &reps)) {
            {
                sprintf(monbuf, "Invalid expression in repetition number.\n");
                monprintf(monbuf);
            }
        }
    }
    if (n > 3) {
        if (!dexp_eval(args[3], &number)) {
            {
                sprintf(monbuf, "Invalid expression in number of copy units.\n");
                monprintf(monbuf);
            }
        }
    }
    if (!dexp_eval(args[1], &source)) {
        {
            sprintf(monbuf, "Invalid expression in source address.\n");
            monprintf(monbuf);
        }
        return;
    }
    if (!dexp_eval(args[2], &dest)) {
        {
            sprintf(monbuf, "Invalid expression in destination address.\n");
            monprintf(monbuf);
        }
    }

    for (uint32_t j = 0; j < reps; j++) {
        for (uint32_t i = 0; i < number; i++) {
            debuggerWriteByte(dest + i, debuggerReadByte(source + i));
        }
        dest += number;
    }
}

void debuggerCopyHalfWord(int n, char** args)
{
    uint32_t source;
    uint32_t dest;
    uint32_t number = 2;
    uint32_t reps = 1;
    if (n > 5 || n < 3) {
        debuggerUsage("copyh");
    }

    if (n == 5) {
        if (!dexp_eval(args[4], &reps)) {
            {
                sprintf(monbuf, "Invalid expression in repetition number.\n");
                monprintf(monbuf);
            }
        }
    }
    if (n > 3) {
        if (!dexp_eval(args[3], &number)) {
            {
                sprintf(monbuf, "Invalid expression in number of copy units.\n");
                monprintf(monbuf);
            }
        }
        number = number << 1;
    }
    if (!dexp_eval(args[1], &source)) {
        {
            sprintf(monbuf, "Invalid expression in source address.\n");
            monprintf(monbuf);
        }
        return;
    }
    if (!dexp_eval(args[2], &dest)) {
        {
            sprintf(monbuf, "Invalid expression in destination address.\n");
            monprintf(monbuf);
        }
    }

    for (uint32_t j = 0; j < reps; j++) {
        for (uint32_t i = 0; i < number; i += 2) {
            debuggerWriteHalfWord(dest + i, debuggerReadHalfWord(source + i));
        }
        dest += number;
    }
}

void debuggerCopyWord(int n, char** args)
{
    uint32_t source;
    uint32_t dest;
    uint32_t number = 4;
    uint32_t reps = 1;
    if (n > 5 || n < 3) {
        debuggerUsage("copyw");
    }

    if (n == 5) {
        if (!dexp_eval(args[4], &reps)) {
            {
                sprintf(monbuf, "Invalid expression in repetition number.\n");
                monprintf(monbuf);
            }
        }
    }
    if (n > 3) {
        if (!dexp_eval(args[3], &number)) {
            {
                sprintf(monbuf, "Invalid expression in number of copy units.\n");
                monprintf(monbuf);
            }
        }
        number = number << 2;
    }
    if (!dexp_eval(args[1], &source)) {
        {
            sprintf(monbuf, "Invalid expression in source address.\n");
            monprintf(monbuf);
        }
        return;
    }
    if (!dexp_eval(args[2], &dest)) {
        {
            sprintf(monbuf, "Invalid expression in destination address.\n");
            monprintf(monbuf);
        }
    }

    for (uint32_t j = 0; j < reps; j++) {
        for (uint32_t i = 0; i < number; i += 4) {
            debuggerWriteMemory(dest + i, debuggerReadMemory(source + i));
        }
        dest += number;
    }
}

void debuggerIoVideo()
{
    {
        sprintf(monbuf, "DISPCNT  = %04x\n", DISPCNT);
        monprintf(monbuf);
    }
    {
        sprintf(monbuf, "DISPSTAT = %04x\n", DISPSTAT);
        monprintf(monbuf);
    }
    {
        sprintf(monbuf, "VCOUNT   = %04x\n", VCOUNT);
        monprintf(monbuf);
    }
    {
        sprintf(monbuf, "BG0CNT   = %04x\n", BG0CNT);
        monprintf(monbuf);
    }
    {
        sprintf(monbuf, "BG1CNT   = %04x\n", BG1CNT);
        monprintf(monbuf);
    }
    {
        sprintf(monbuf, "BG2CNT   = %04x\n", BG2CNT);
        monprintf(monbuf);
    }
    {
        sprintf(monbuf, "BG3CNT   = %04x\n", BG3CNT);
        monprintf(monbuf);
    }
    {
        sprintf(monbuf, "WIN0H    = %04x\n", WIN0H);
        monprintf(monbuf);
    }
    {
        sprintf(monbuf, "WIN0V    = %04x\n", WIN0V);
        monprintf(monbuf);
    }
    {
        sprintf(monbuf, "WIN1H    = %04x\n", WIN1H);
        monprintf(monbuf);
    }
    {
        sprintf(monbuf, "WIN1V    = %04x\n", WIN1V);
        monprintf(monbuf);
    }
    {
        sprintf(monbuf, "WININ    = %04x\n", WININ);
        monprintf(monbuf);
    }
    {
        sprintf(monbuf, "WINOUT   = %04x\n", WINOUT);
        monprintf(monbuf);
    }
    {
        sprintf(monbuf, "MOSAIC   = %04x\n", MOSAIC);
        monprintf(monbuf);
    }
    {
        sprintf(monbuf, "BLDMOD   = %04x\n", BLDMOD);
        monprintf(monbuf);
    }
    {
        sprintf(monbuf, "COLEV    = %04x\n", COLEV);
        monprintf(monbuf);
    }
    {
        sprintf(monbuf, "COLY     = %04x\n", COLY);
        monprintf(monbuf);
    }
}

void debuggerIoVideo2()
{
    {
        sprintf(monbuf, "BG0HOFS  = %04x\n", BG0HOFS);
        monprintf(monbuf);
    }
    {
        sprintf(monbuf, "BG0VOFS  = %04x\n", BG0VOFS);
        monprintf(monbuf);
    }
    {
        sprintf(monbuf, "BG1HOFS  = %04x\n", BG1HOFS);
        monprintf(monbuf);
    }
    {
        sprintf(monbuf, "BG1VOFS  = %04x\n", BG1VOFS);
        monprintf(monbuf);
    }
    {
        sprintf(monbuf, "BG2HOFS  = %04x\n", BG2HOFS);
        monprintf(monbuf);
    }
    {
        sprintf(monbuf, "BG2VOFS  = %04x\n", BG2VOFS);
        monprintf(monbuf);
    }
    {
        sprintf(monbuf, "BG3HOFS  = %04x\n", BG3HOFS);
        monprintf(monbuf);
    }
    {
        sprintf(monbuf, "BG3VOFS  = %04x\n", BG3VOFS);
        monprintf(monbuf);
    }
    {
        sprintf(monbuf, "BG2PA    = %04x\n", BG2PA);
        monprintf(monbuf);
    }
    {
        sprintf(monbuf, "BG2PB    = %04x\n", BG2PB);
        monprintf(monbuf);
    }
    {
        sprintf(monbuf, "BG2PC    = %04x\n", BG2PC);
        monprintf(monbuf);
    }
    {
        sprintf(monbuf, "BG2PD    = %04x\n", BG2PD);
        monprintf(monbuf);
    }
    {
        sprintf(monbuf, "BG2X     = %08x\n", (BG2X_H << 16) | BG2X_L);
        monprintf(monbuf);
    }
    {
        sprintf(monbuf, "BG2Y     = %08x\n", (BG2Y_H << 16) | BG2Y_L);
        monprintf(monbuf);
    }
    {
        sprintf(monbuf, "BG3PA    = %04x\n", BG3PA);
        monprintf(monbuf);
    }
    {
        sprintf(monbuf, "BG3PB    = %04x\n", BG3PB);
        monprintf(monbuf);
    }
    {
        sprintf(monbuf, "BG3PC    = %04x\n", BG3PC);
        monprintf(monbuf);
    }
    {
        sprintf(monbuf, "BG3PD    = %04x\n", BG3PD);
        monprintf(monbuf);
    }
    {
        sprintf(monbuf, "BG3X     = %08x\n", (BG3X_H << 16) | BG3X_L);
        monprintf(monbuf);
    }
    {
        sprintf(monbuf, "BG3Y     = %08x\n", (BG3Y_H << 16) | BG3Y_L);
        monprintf(monbuf);
    }
}

void debuggerIoDMA()
{
    {
        sprintf(monbuf, "DM0SAD   = %08x\n", (DM0SAD_H << 16) | DM0SAD_L);
        monprintf(monbuf);
    }
    {
        sprintf(monbuf, "DM0DAD   = %08x\n", (DM0DAD_H << 16) | DM0DAD_L);
        monprintf(monbuf);
    }
    {
        sprintf(monbuf, "DM0CNT   = %08x\n", (DM0CNT_H << 16) | DM0CNT_L);
        monprintf(monbuf);
    }
    {
        sprintf(monbuf, "DM1SAD   = %08x\n", (DM1SAD_H << 16) | DM1SAD_L);
        monprintf(monbuf);
    }
    {
        sprintf(monbuf, "DM1DAD   = %08x\n", (DM1DAD_H << 16) | DM1DAD_L);
        monprintf(monbuf);
    }
    {
        sprintf(monbuf, "DM1CNT   = %08x\n", (DM1CNT_H << 16) | DM1CNT_L);
        monprintf(monbuf);
    }
    {
        sprintf(monbuf, "DM2SAD   = %08x\n", (DM2SAD_H << 16) | DM2SAD_L);
        monprintf(monbuf);
    }
    {
        sprintf(monbuf, "DM2DAD   = %08x\n", (DM2DAD_H << 16) | DM2DAD_L);
        monprintf(monbuf);
    }
    {
        sprintf(monbuf, "DM2CNT   = %08x\n", (DM2CNT_H << 16) | DM2CNT_L);
        monprintf(monbuf);
    }
    {
        sprintf(monbuf, "DM3SAD   = %08x\n", (DM3SAD_H << 16) | DM3SAD_L);
        monprintf(monbuf);
    }
    {
        sprintf(monbuf, "DM3DAD   = %08x\n", (DM3DAD_H << 16) | DM3DAD_L);
        monprintf(monbuf);
    }
    {
        sprintf(monbuf, "DM3CNT   = %08x\n", (DM3CNT_H << 16) | DM3CNT_L);
        monprintf(monbuf);
    }
}

void debuggerIoTimer()
{
    {
        sprintf(monbuf, "TM0D     = %04x\n", TM0D);
        monprintf(monbuf);
    }
    {
        sprintf(monbuf, "TM0CNT   = %04x\n", TM0CNT);
        monprintf(monbuf);
    }
    {
        sprintf(monbuf, "TM1D     = %04x\n", TM1D);
        monprintf(monbuf);
    }
    {
        sprintf(monbuf, "TM1CNT   = %04x\n", TM1CNT);
        monprintf(monbuf);
    }
    {
        sprintf(monbuf, "TM2D     = %04x\n", TM2D);
        monprintf(monbuf);
    }
    {
        sprintf(monbuf, "TM2CNT   = %04x\n", TM2CNT);
        monprintf(monbuf);
    }
    {
        sprintf(monbuf, "TM3D     = %04x\n", TM3D);
        monprintf(monbuf);
    }
    {
        sprintf(monbuf, "TM3CNT   = %04x\n", TM3CNT);
        monprintf(monbuf);
    }
}

void debuggerIoMisc()
{
    {
        sprintf(monbuf, "P1       = %04x\n", P1);
        monprintf(monbuf);
    }
    {
        sprintf(monbuf, "IE       = %04x\n", IE);
        monprintf(monbuf);
    }
    {
        sprintf(monbuf, "IF       = %04x\n", IF);
        monprintf(monbuf);
    }
    {
        sprintf(monbuf, "IME      = %04x\n", IME);
        monprintf(monbuf);
    }
}

void debuggerIo(int n, char** args)
{
    if (n == 1) {
        debuggerIoVideo();
        return;
    }
    if (!strcmp(args[1], "video"))
        debuggerIoVideo();
    else if (!strcmp(args[1], "video2"))
        debuggerIoVideo2();
    else if (!strcmp(args[1], "dma"))
        debuggerIoDMA();
    else if (!strcmp(args[1], "timer"))
        debuggerIoTimer();
    else if (!strcmp(args[1], "misc"))
        debuggerIoMisc();
    else {
        sprintf(monbuf, "Unrecognized option %s\n", args[1]);
        monprintf(monbuf);
    }
}

#define ASCII(c) (c)<32 ? '.' : (c)> 127 ? '.' : (c)

bool canUseTbl = true;
bool useWordSymbol = false;
bool thereIsATable = false;
char** wordSymbol;
bool isTerminator[256];
bool isNewline[256];
bool isTab[256];
uint8_t largestSymbol = 1;

void freeWordSymbolContents()
{
    for (int i = 0; i < 256; i++) {
        if (wordSymbol[i])
            free(wordSymbol[i]);
        wordSymbol[i] = NULL;
        isTerminator[i] = false;
        isNewline[i] = false;
        isTab[i] = false;
    }
}

void freeWordSymbol()
{
    useWordSymbol = false;
    thereIsATable = false;
    free(wordSymbol);
    largestSymbol = 1;
}

void debuggerReadCharTable(int n, char** args)
{
    if (n == 2) {
        if (!canUseTbl) {
            {
                sprintf(monbuf, "Cannot operate over character table, as it was disabled.\n");
                monprintf(monbuf);
            }
            return;
        }
        if (strcmp(args[1], "none") == 0) {
            freeWordSymbol();
            {
                sprintf(monbuf, "Cleared table. Reverted to ASCII.\n");
                monprintf(monbuf);
            }
            return;
        }
        FILE* tlb = fopen(args[1], "r");
        if (!tlb) {
            {
                sprintf(monbuf, "Could not open specified file. Abort.\n");
                monprintf(monbuf);
            }
            return;
        }
        char buffer[30];
        uint32_t slot;
        char* character = (char*)calloc(10, sizeof(char));
        wordSymbol = (char**)calloc(256, sizeof(char*));
        while (fgets(buffer, 30, tlb)) {

            sscanf(buffer, "%02x=%s", &slot, character);

            if (character[0]) {
                if (strlen(character) == 4) {
                    if ((character[0] == '<') && (character[1] == '\\') && (character[3] == '>')) {
                        if (character[2] == '0') {
                            isTerminator[slot] = true;
                        }
                        if (character[2] == 'n') {
                            isNewline[slot] = true;
                        }
                        if (character[2] == 't') {
                            isTab[slot] = true;
                        }
                        continue;
                    } else
                        wordSymbol[slot] = character;
                } else
                    wordSymbol[slot] = character;
            } else
                wordSymbol[slot] = (char*)' ';

            if (largestSymbol < strlen(character))
                largestSymbol = strlen(character);

            character = (char*)malloc(10);
        }
        useWordSymbol = true;
        thereIsATable = true;

    } else {
        debuggerUsage("tbl");
    }
}

void printCharGroup(uint32_t addr, bool useAscii)
{
    for (int i = 0; i < 16; i++) {
        if (useWordSymbol && !useAscii) {
            char* c = wordSymbol[debuggerReadByte(addr + i)];
            int j;
            if (c) {
                {
                    sprintf(monbuf, "%s", c);
                    monprintf(monbuf);
                }
                j = strlen(c);
            } else {
                j = 0;
            }
            while (j < largestSymbol) {
                {
                    sprintf(monbuf, " ");
                    monprintf(monbuf);
                }
                j++;
            }
        } else {
            {
                sprintf(monbuf, "%c", ASCII(debuggerReadByte(addr + i)));
                monprintf(monbuf);
            }
        }
    }
}

void debuggerMemoryByte(int n, char** args)
{
    if (n == 2) {
        uint32_t addr = 0;

        if (!dexp_eval(args[1], &addr)) {
            {
                sprintf(monbuf, "Invalid expression\n");
                monprintf(monbuf);
            }
            return;
        }
        for (int loop = 0; loop < 16; loop++) {
            {
                sprintf(monbuf, "%08x ", addr);
                monprintf(monbuf);
            }
            for (int j = 0; j < 16; j++) {
                {
                    sprintf(monbuf, "%02x ", debuggerReadByte(addr + j));
                    monprintf(monbuf);
                }
            }
            printCharGroup(addr, true);
            {
                sprintf(monbuf, "\n");
                monprintf(monbuf);
            }
            addr += 16;
        }
    } else
        debuggerUsage("mb");
}

void debuggerMemoryHalfWord(int n, char** args)
{
    if (n == 2) {
        uint32_t addr = 0;

        if (!dexp_eval(args[1], &addr)) {
            {
                sprintf(monbuf, "Invalid expression\n");
                monprintf(monbuf);
            }
            return;
        }

        addr = addr & 0xfffffffe;

        for (int loop = 0; loop < 16; loop++) {
            {
                sprintf(monbuf, "%08x ", addr);
                monprintf(monbuf);
            }
            for (int j = 0; j < 16; j += 2) {
                {
                    sprintf(monbuf, "%02x%02x ", debuggerReadByte(addr + j + 1), debuggerReadByte(addr + j));
                    monprintf(monbuf);
                }
            }
            printCharGroup(addr, true);
            {
                sprintf(monbuf, "\n");
                monprintf(monbuf);
            }
            addr += 16;
        }
    } else
        debuggerUsage("mh");
}

void debuggerMemoryWord(int n, char** args)
{
    if (n == 2) {
        uint32_t addr = 0;
        if (!dexp_eval(args[1], &addr)) {
            {
                sprintf(monbuf, "Invalid expression\n");
                monprintf(monbuf);
            }
            return;
        }
        addr = addr & 0xfffffffc;
        for (int loop = 0; loop < 16; loop++) {
            {
                sprintf(monbuf, "%08x ", addr);
                monprintf(monbuf);
            }
            for (int j = 0; j < 16; j += 4) {
                {
                    sprintf(monbuf, "%02x%02x%02x%02x ", debuggerReadByte(addr + j + 3), debuggerReadByte(addr + j + 2), debuggerReadByte(addr + j + 1), debuggerReadByte(addr + j));
                    monprintf(monbuf);
                }
            }
            printCharGroup(addr, true);
            {
                sprintf(monbuf, "\n");
                monprintf(monbuf);
            }
            addr += 16;
        }
    } else
        debuggerUsage("mw");
}

void debuggerStringRead(int n, char** args)
{
    if (n == 2) {
        uint32_t addr = 0;

        if (!dexp_eval(args[1], &addr)) {
            {
                sprintf(monbuf, "Invalid expression\n");
                monprintf(monbuf);
            }
            return;
        }
        for (int i = 0; i < 512; i++) {
            uint8_t slot = debuggerReadByte(addr + i);

            if (useWordSymbol) {
                if (isTerminator[slot]) {
                    {
                        sprintf(monbuf, "\n");
                        monprintf(monbuf);
                    }
                    return;
                } else if (isNewline[slot]) {
                    {
                        sprintf(monbuf, "\n");
                        monprintf(monbuf);
                    }
                } else if (isTab[slot]) {
                    {
                        sprintf(monbuf, "\t");
                        monprintf(monbuf);
                    }
                } else {
                    if (wordSymbol[slot]) {
                        {
                            sprintf(monbuf, "%s", wordSymbol[slot]);
                            monprintf(monbuf);
                        }
                    }
                }
            } else {
                {
                    sprintf(monbuf, "%c", ASCII(slot));
                    monprintf(monbuf);
                }
            }
        }
    } else
        debuggerUsage("ms");
}

void debuggerRegisters(int, char**)
{
    {
        sprintf(monbuf, "R00=%08x R04=%08x R08=%08x R12=%08x\n", reg[0].I, reg[4].I, reg[8].I, reg[12].I);
        monprintf(monbuf);
    }
    {
        sprintf(monbuf, "R01=%08x R05=%08x R09=%08x R13=%08x\n", reg[1].I, reg[5].I, reg[9].I, reg[13].I);
        monprintf(monbuf);
    }
    {
        sprintf(monbuf, "R02=%08x R06=%08x R10=%08x R14=%08x\n", reg[2].I, reg[6].I, reg[10].I, reg[14].I);
        monprintf(monbuf);
    }
    {
        sprintf(monbuf, "R03=%08x R07=%08x R11=%08x R15=%08x\n", reg[3].I, reg[7].I, reg[11].I, reg[15].I);
        monprintf(monbuf);
    }
    {
        sprintf(monbuf, "CPSR=%08x (%c%c%c%c%c%c%c Mode: %02x)\n",
            reg[16].I,
            (N_FLAG ? 'N' : '.'),
            (Z_FLAG ? 'Z' : '.'),
            (C_FLAG ? 'C' : '.'),
            (V_FLAG ? 'V' : '.'),
            (armIrqEnable ? '.' : 'I'),
            ((!(reg[16].I & 0x40)) ? '.' : 'F'),
            (armState ? '.' : 'T'),
            armMode);
        monprintf(monbuf);
    }
}

void debuggerExecuteCommands(int n, char** args)
{
    if (n == 1) {
        {
            sprintf(monbuf, "%s requires at least one pathname to execute.", args[0]);
            monprintf(monbuf);
        }
        return;
    } else {
        char buffer[4096];
        n--;
        args++;
        while (n) {
            FILE* toExec = fopen(args[0], "r");
            if (toExec) {
                while (fgets(buffer, 4096, toExec)) {
                    std::string buf(buffer);
                    dbgExecute(buf);
                    if (!debugger || !emulating) {
                        return;
                    }
                }
            } else {
                sprintf(monbuf, "Could not open %s. Will not be executed.\n", args[0]);
                monprintf(monbuf);
            }

            args++;
            n--;
        }
    }
}

void debuggerSetRadix(int argc, char** argv)
{
    if (argc != 2)
        debuggerUsage(argv[0]);
    else {
        int r = atoi(argv[1]);

        bool error = false;
        switch (r) {
        case 10:
            debuggerRadix = 0;
            break;
        case 8:
            debuggerRadix = 2;
            break;
        case 16:
            debuggerRadix = 1;
            break;
        default:
            error = true;
            {
                sprintf(monbuf, "Unknown radix %d. Valid values are 8, 10 and 16.\n", r);
                monprintf(monbuf);
            }
            break;
        }
        if (!error) {
            sprintf(monbuf, "Radix set to %d\n", r);
            monprintf(monbuf);
        }
    }
}

void debuggerSymbols(int argc, char** argv)
{
    int i = 0;
    uint32_t value;
    uint32_t size;
    int type;
    bool match = false;
    int matchSize = 0;
    char* matchStr = NULL;

    if (argc == 2) {
        match = true;
        matchSize = strlen(argv[1]);
        matchStr = argv[1];
    }
    {
        sprintf(monbuf, "Symbol               Value    Size     Type   \n");
        monprintf(monbuf);
    }
    {
        sprintf(monbuf, "-------------------- -------  -------- -------\n");
        monprintf(monbuf);
    }
    const char* s = NULL;
    while ((s = elfGetSymbol(i, &value, &size, &type))) {
        if (*s) {
            if (match) {
                if (strncmp(s, matchStr, matchSize) != 0) {
                    i++;
                    continue;
                }
            }
            const char* ts = "?";
            switch (type) {
            case 2:
                ts = "ARM";
                break;
            case 0x0d:
                ts = "THUMB";
                break;
            case 1:
                ts = "DATA";
                break;
            }
            {
                sprintf(monbuf, "%-20s %08x %08x %-7s\n", s, value, size, ts);
                monprintf(monbuf);
            }
        }
        i++;
    }
}

void debuggerWhere(int n, char** args)
{
    (void)n; // unused params
    (void)args; // unused params
    void elfPrintCallChain(uint32_t);
    elfPrintCallChain(armNextPC);
}

void debuggerVar(int n, char** args)
{
    uint32_t val;

    if (n < 2) {
        dexp_listVars();
        return;
    }

    if (strcmp(args[1], "set") == 0) {

        if (n < 4) {
            {
                sprintf(monbuf, "No expression specified.\n");
                monprintf(monbuf);
            }
            return;
        }

        if (!dexp_eval(args[3], &val)) {
            {
                sprintf(monbuf, "Invalid expression.\n");
                monprintf(monbuf);
            }
            return;
        }

        dexp_setVar(args[2], val);
        {
            sprintf(monbuf, "%s = $%08x\n", args[2], val);
            monprintf(monbuf);
        }
        return;
    }

    if (strcmp(args[1], "list") == 0) {
        dexp_listVars();
        return;
    }

    if (strcmp(args[1], "save") == 0) {
        if (n < 3) {
            {
                sprintf(monbuf, "No file specified.\n");
                monprintf(monbuf);
            }
            return;
        }
        dexp_saveVars(args[2]);
        return;
    }

    if (strcmp(args[1], "load") == 0) {
        if (n < 3) {
            {
                sprintf(monbuf, "No file specified.\n");
                monprintf(monbuf);
            }
            return;
        }
        dexp_loadVars(args[2]);
        return;
    }

    {
        sprintf(monbuf, "Unrecognized sub-command.\n");
        monprintf(monbuf);
    }
}

bool debuggerBreakOnExecution(uint32_t address, uint8_t state)
{
    (void)state; // unused params
    if (dontBreakNow)
        return false;
    if (debuggerInDB(address))
        return false;
    if (!doesBreak(address, armState ? 0x44 : 0x88))
        return false;

    {
        sprintf(monbuf, "Breakpoint (on %s) address %08x\n", (armState ? "ARM" : "Thumb"), address);
        monprintf(monbuf);
    }
    debugger = true;
    return true;
}

bool debuggerBreakOnRead(uint32_t address, int size)
{
    (void)size; // unused params
    if (dontBreakNow)
        return false;
    if (debuggerInDB(armState ? reg[15].I - 4 : reg[15].I - 2))
        return false;
    if (!doesBreak(address, 0x22))
        return false;
    //if (size == 2)
    //	monprintf("Breakpoint (on read) address %08x value:%08x\n",
    //	address, debuggerReadMemory(address));
    //else if (size == 1)
    //	monprintf("Breakpoint (on read) address %08x value:%04x\n",
    //	address, debuggerReadHalfWord(address));
    //else
    //	monprintf("Breakpoint (on read) address %08x value:%02x\n",
    //	address, debuggerReadByte(address));
    debugger = true;
    return true;
}

bool debuggerBreakOnWrite(uint32_t address, uint32_t value, int size)
{
    (void)value; // unused params
    (void)size; // unused params
    if (dontBreakNow)
        return false;
    if (debuggerInDB(armState ? reg[15].I - 4 : reg[15].I - 2))
        return false;
    if (!doesBreak(address, 0x11))
        return false;
    //uint32_t lastValue;
    //dexp_eval("old_value", &lastValue);
    //if (size == 2)
    //	monprintf("Breakpoint (on write) address %08x old:%08x new:%08x\n",
    //	address, lastValue, value);
    //else if (size == 1)
    //	monprintf("Breakpoint (on write) address %08x old:%04x new:%04x\n",
    //	address, (uint16_t)lastValue, (uint16_t)value);
    //else
    //	monprintf("Breakpoint (on write) address %08x old:%02x new:%02x\n",
    //	address, (uint8_t)lastValue, (uint8_t)value);
    debugger = true;
    return true;
}

void debuggerBreakOnWrite(uint32_t address, uint32_t oldvalue, uint32_t value, int size, int t)
{
    (void)oldvalue; // unused params
    (void)t; // unused params
    debuggerBreakOnWrite(address, value, size);
    //uint32_t lastValue;
    //dexp_eval("old_value", &lastValue);

    //const char *type = "write";
    //if (t == 2)
    //	type = "change";

    //if (size == 2)
    //	monprintf("Breakpoint (on %s) address %08x old:%08x new:%08x\n",
    //	type, address, oldvalue, value);
    //else if (size == 1)
    //	monprintf("Breakpoint (on %s) address %08x old:%04x new:%04x\n",
    //	type, address, (uint16_t)oldvalue, (uint16_t)value);
    //else
    //	monprintf("Breakpoint (on %s) address %08x old:%02x new:%02x\n",
    //	type, address, (uint8_t)oldvalue, (uint8_t)value);
    //debugger = true;
}

uint8_t getFlags(char* flagName)
{

    for (int i = 0; flagName[i] != '\0'; i++) {
        flagName[i] = toupper(flagName[i]);
    }

    if (strcmp(flagName, "ALWAYS") == 0) {
        return 0x7;
    }

    if (strcmp(flagName, "NEVER") == 0) {
        return 0x0;
    }

    uint8_t flag = 0;

    bool negate_flag = false;

    for (int i = 0; flagName[i] != '\0'; i++) {
        switch (flagName[i]) {
        case 'E':
            flag |= 1;
            break;
        case 'G':
            flag |= 2;
            break;
        case 'L':
            flag |= 4;
            break;
        case 'S':
            flag |= 8;
            break;
        case 'U':
            flag &= 7;
            break;
        case 'N':
            negate_flag = (!negate_flag);
            break;
        }
    }
    if (negate_flag) {
        flag = ((flag & 8) | ((~flag) & 7));
    }
    return flag;
}

void debuggerBreakRegister(int n, char** args)
{
    if (n != 3) {
        {
            sprintf(monbuf, "Incorrect usage of breg. Correct usage is breg <register> {flag} {value}\n");
            monprintf(monbuf);
        }
        printFlagHelp();
        return;
    }
    uint8_t reg = (uint8_t)getRegisterNumber(args[0]);
    uint8_t flag = getFlags(args[1]);
    uint32_t value;
    if (!dexp_eval(args[2], &value)) {
        {
            sprintf(monbuf, "Invalid expression.\n");
            monprintf(monbuf);
        }
        return;
    }
    if (flag != 0) {
        addBreakRegToList(reg, flag, value);
        {
            sprintf(monbuf, "Added breakpoint on register R%02d, value %08x\n", reg, value);
            monprintf(monbuf);
        }
    }
    return;
}

void debuggerBreakRegisterClear(int n, char** args)
{
    if (n > 0) {
        int r = getRegisterNumber(args[0]);
        if (r >= 0) {
            clearParticularRegListBreaks(r);
            {
                sprintf(monbuf, "Cleared all Register breakpoints for %s.\n", args[0]);
                monprintf(monbuf);
            }
        }
    } else {
        clearBreakRegList();
        {
            sprintf(monbuf, "Cleared all Register breakpoints.\n");
            monprintf(monbuf);
        }
    }
}

void debuggerBreakRegisterDelete(int n, char** args)
{
    if (n < 2) {
        {
            sprintf(monbuf, "Illegal use of Break register delete:\n Correct usage requires <register> <breakpointNo>.\n");
            monprintf(monbuf);
        }
        return;
    }
    int r = getRegisterNumber(args[0]);
    if ((r < 0) || (r > 16)) {
        {
            sprintf(monbuf, "Could not find a correct register number:\n Correct usage requires <register> <breakpointNo>.\n");
            monprintf(monbuf);
        }
        return;
    }
    uint32_t num;
    if (!dexp_eval(args[1], &num)) {
        {
            sprintf(monbuf, "Could not parse the breakpoint number:\n Correct usage requires <register> <breakpointNo>.\n");
            monprintf(monbuf);
        }
        return;
    }
    deleteFromBreakRegList(r, num);
    {
        sprintf(monbuf, "Deleted Breakpoint %d of regsiter %s.\n", num, args[0]);
        monprintf(monbuf);
    }
}

//WARNING: Some old particle to new code conversion may convert a single command
//into two or more words. Such words are separated by space, so a new tokenizer can
//find them.
const char* replaceAlias(const char* lower_cmd, const char** aliasTable)
{
    for (int i = 0; aliasTable[i]; i = i + 2) {
        if (strcmp(lower_cmd, aliasTable[i]) == 0) {
            return aliasTable[i + 1];
        }
    }
    return lower_cmd;
}

const char* breakAliasTable[] = {

    //actual beginning
    "break", "b 0 0",
    "breakpoint", "b 0 0",
    "bp", "b 0 0",
    "b", "b 0 0",

    //break types
    "thumb", "t",
    "arm", "a",
    "execution", "x",
    "exec", "x",
    "e", "x",
    "exe", "x",
    "x", "x",
    "read", "r",
    "write", "w",
    "access", "i",
    "acc", "i",
    "io", "i",
    "register", "g",
    "reg", "g",
    "any", "*",

    //code modifiers
    "clear", "c",
    "clean", "c",
    "cls", "c",
    "list", "l",
    "lst", "l",
    "delete", "d",
    "del", "d",
    "make", "m",
    /*
	//old parts made to look like the new code parts
	"bt", "b t m",
	"ba", "b a m",
	"bd", "b * d",
	"bl", "b * l",
	"bpr","b r m",
	"bprc","b r c",
	"bpw", "b w m",
	"bpwc", "b w c",
	"bt", "b t m",
	*/
    //and new parts made to look like old parts
    "breg", "b g m",
    "bregc", "b g c",
    "bregd", "b g d",
    "bregl", "b g l",

    "blist", "b * l",
    /*
	"btc", "b t c",
	"btd", "b t d",
	"btl", "b t l",

	"bac", "b a c",
	"bad", "b a d",
	"bal", "b a l",

	"bx", "b x m",
	"bxc", "b x c",
	"bxd", "b x d",
	"bxl", "b x l",

	"bw", "b w m",
	"bwc", "b w c",
	"bwd", "b w d",
	"bwl", "b w l",

	"br", "b r m",
	"brc", "b r c",
	"brd", "b r d",
	"brl", "b r l",
	*/
    "bio", "b i m",
    "bioc", "b i c",
    "biod", "b i d",
    "biol", "b i l",

    "bpio", "b i m",
    "bpioc", "b i c",
    "bpiod", "b i d",
    "bpiol", "b i l",
    /*
	"bprd", "b r d",
	"bprl", "b r l",

	"bpwd", "b w d",
	"bpwl", "b w l",
	*/
    NULL, NULL

};

char* breakSymbolCombo(char* command, int* length)
{
    char* res = (char*)malloc(6);
    res[0] = 'b';
    res[1] = ' ';
    res[2] = '0';
    res[3] = ' ';
    res[4] = '0';
    int i = 1;
    if (command[1] == 'p') {
        i++;
    }
    while (i < *length) {
        switch (command[i]) {
        case 'l':
        case 'c':
        case 'd':
        case 'm':
            if (res[4] == '0')
                res[4] = command[i];
            else {
                free(res);
                return command;
            }
            break;
        case '*':
        case 't':
        case 'a':
        case 'x':
        case 'r':
        case 'w':
        case 'i':
            if (res[2] == '0')
                res[2] = command[i];
            else {
                free(res);
                return command;
            }
            break;
        default:
            free(res);
            return command;
        }
        i++;
    }
    if (res[2] == '0')
        res[2] = '*';
    if (res[4] == '0')
        res[4] = 'm';
    *length = 5;
    return res;
}

const char* typeMapping[] = { "'uint8_t", "'uint16_t", "'uint32_t", "'uint32_t", "'int8_t", "'int16_t", "'int32_t", "'int32_t" };

const char* compareFlagMapping[] = { "Never", "==", ">", ">=", "<", "<=", "!=", "<=>" };

struct intToString {
    int value;
    const char mapping[20];
};

struct intToString breakFlagMapping[] = {
    { 0x80, "Thumb" },
    { 0x40, "ARM" },
    { 0x20, "Read" },
    { 0x10, "Write" },
    { 0x8, "Thumb" },
    { 0x4, "ARM" },
    { 0x2, "Read" },
    { 0x1, "Write" },
    { 0x0, "None" }
};

//printers
void printCondition(struct ConditionalBreakNode* toPrint)
{
    if (toPrint) {
        const char* firstType = typeMapping[toPrint->exp_type_flags & 0x7];
        const char* secondType = typeMapping[(toPrint->exp_type_flags >> 4) & 0x7];
        const char* operand = compareFlagMapping[toPrint->cond_flags & 0x7];
        {
            sprintf(monbuf, "%s %s %s%s %s %s", firstType, toPrint->address,
                ((toPrint->cond_flags & 8) ? "s" : ""), operand,
                secondType, toPrint->value);
            monprintf(monbuf);
        }
        if (toPrint->next) {
            {
                sprintf(monbuf, " &&\n\t\t");
                monprintf(monbuf);
            }
            printCondition(toPrint->next);
        } else {
            {
                sprintf(monbuf, "\n");
                monprintf(monbuf);
            }
            return;
        }
    }
}

void printConditionalBreak(struct ConditionalBreak* toPrint, bool printAddress)
{
    if (toPrint) {
        if (printAddress) {
            sprintf(monbuf, "At %08x, ", toPrint->break_address);
            monprintf(monbuf);
        }
        if (toPrint->type_flags & 0xf0) {
            sprintf(monbuf, "Break Always on");
            monprintf(monbuf);
        }
        bool hasPrevCond = false;
        uint8_t flgs = 0x80;
        while (flgs != 0) {
            if (toPrint->type_flags & flgs) {
                if (hasPrevCond) {
                    sprintf(monbuf, ",");
                    monprintf(monbuf);
                }
                for (int i = 0; i < 9; i++) {
                    if (breakFlagMapping[i].value == flgs) {
                        {
                            sprintf(monbuf, "\t%s", breakFlagMapping[i].mapping);
                            monprintf(monbuf);
                        }
                        hasPrevCond = true;
                    }
                }
            }
            flgs = flgs >> 1;
            if ((flgs == 0x8) && (toPrint->type_flags & 0xf)) {
                {
                    sprintf(monbuf, "\n\t\tBreak conditional on");
                    monprintf(monbuf);
                }
                hasPrevCond = false;
            }
        }
        {
            sprintf(monbuf, "\n");
            monprintf(monbuf);
        }
        if (toPrint->type_flags & 0xf && toPrint->firstCond) {
            {
                sprintf(monbuf, "With conditions:\n\t\t");
                monprintf(monbuf);
            }
            printCondition(toPrint->firstCond);
        } else if (toPrint->type_flags & 0xf) {
            //should not happen
            {
                sprintf(monbuf, "No conditions detected, but conditional. Assumed always by default.\n");
                monprintf(monbuf);
            }
        }
    }
}

void printAllConditionals()
{

    for (int i = 0; i < 16; i++) {

        if (conditionals[i] != NULL) {
            {
                sprintf(monbuf, "Address range 0x%02x000000 breaks:\n", i);
                monprintf(monbuf);
            }
            {
                sprintf(monbuf, "-------------------------\n");
                monprintf(monbuf);
            }
            struct ConditionalBreak* base = conditionals[i];
            int count = 1;
            uint32_t lastAddress = base->break_address;
            {
                sprintf(monbuf, "Address %08x\n-------------------------\n", lastAddress);
                monprintf(monbuf);
            }
            while (base) {
                if (lastAddress != base->break_address) {
                    lastAddress = base->break_address;
                    count = 1;
                    {
                        sprintf(monbuf, "-------------------------\n");
                        monprintf(monbuf);
                    }
                    {
                        sprintf(monbuf, "Address %08x\n-------------------------\n", lastAddress);
                        monprintf(monbuf);
                    }
                }
                {
                    sprintf(monbuf, "No.%d\t-->\t", count);
                    monprintf(monbuf);
                }
                printConditionalBreak(base, false);
                count++;
                base = base->next;
            }
        }
    }
}

uint8_t printConditionalsFromAddress(uint32_t address)
{
    uint8_t count = 1;
    if (conditionals[address >> 24] != NULL) {
        struct ConditionalBreak* base = conditionals[address >> 24];
        while (base) {
            if (address == base->break_address) {
                if (count == 1) {
                    {
                        sprintf(monbuf, "Address %08x\n-------------------------\n", address);
                        monprintf(monbuf);
                    }
                }
                {
                    sprintf(monbuf, "No.%d\t-->\t", count);
                    monprintf(monbuf);
                }
                printConditionalBreak(base, false);
                count++;
            }
            if (address < base->break_address)
                break;
            base = base->next;
        }
    }
    if (count == 1) {
        {
            sprintf(monbuf, "None\n");
            monprintf(monbuf);
        }
    }
    return count;
}

void printAllFlagConditionals(uint8_t flag, bool orMode)
{
    int count = 1;
    int actualCount = 1;
    for (int i = 0; i < 16; i++) {
        if (conditionals[i] != NULL) {
            bool isCondStart = true;
            struct ConditionalBreak* base = conditionals[i];

            uint32_t lastAddress = base->break_address;

            while (base) {
                if (lastAddress != base->break_address) {
                    lastAddress = base->break_address;
                    count = 1;
                    actualCount = 1;
                }
                if (((base->type_flags & flag) == base->type_flags) || (orMode && (base->type_flags & flag))) {
                    if (actualCount == 1) {
                        if (isCondStart) {
                            {
                                sprintf(monbuf, "Address range 0x%02x000000 breaks:\n", i);
                                monprintf(monbuf);
                            }
                            {
                                sprintf(monbuf, "-------------------------\n");
                                monprintf(monbuf);
                            }
                            isCondStart = false;
                        }
                        {
                            sprintf(monbuf, "Address %08x\n-------------------------\n", lastAddress);
                            monprintf(monbuf);
                        }
                    }
                    {
                        sprintf(monbuf, "No.%d\t-->\t", count);
                        monprintf(monbuf);
                    }
                    printConditionalBreak(base, false);
                    actualCount++;
                }
                base = base->next;
                count++;
            }
        }
    }
}

void printAllFlagConditionalsWithAddress(uint32_t address, uint8_t flag, bool orMode)
{
    int count = 1;
    int actualCount = 1;
    for (int i = 0; i < 16; i++) {
        if (conditionals[i] != NULL) {
            bool isCondStart = true;
            struct ConditionalBreak* base = conditionals[i];

            uint32_t lastAddress = base->break_address;

            while (base) {
                if (lastAddress != base->break_address) {
                    lastAddress = base->break_address;
                    count = 1;
                    actualCount = 1;
                }
                if ((lastAddress == address) && (((base->type_flags & flag) == base->type_flags) || (orMode && (base->type_flags & flag)))) {
                    if (actualCount == 1) {
                        if (isCondStart) {
                            {
                                sprintf(monbuf, "Address range 0x%02x000000 breaks:\n", i);
                                monprintf(monbuf);
                            }
                            {
                                sprintf(monbuf, "-------------------------\n");
                                monprintf(monbuf);
                            }
                            isCondStart = false;
                        }
                        {
                            sprintf(monbuf, "Address %08x\n-------------------------\n", lastAddress);
                            monprintf(monbuf);
                        }
                    }
                    {
                        sprintf(monbuf, "No.%d\t-->\t", count);
                        monprintf(monbuf);
                    }
                    printConditionalBreak(base, false);
                    actualCount++;
                }
                base = base->next;
                count++;
            }
        }
    }
}

void makeBreak(uint32_t address, uint8_t flags, char** expression, int n)
{
    if (n >= 1) {
        if (tolower(expression[0][0]) == 'i' && tolower(expression[0][1]) == 'f') {
            expression = expression + 1;
            n--;
            if (n != 0) {
                parseAndCreateConditionalBreaks(address, flags, expression, n);
                return;
            }
        }
    } else {
        flags = flags << 0x4;
        printConditionalBreak(addConditionalBreak(address, flags), true);
        return;
    }
}
void deleteBreak(uint32_t address, uint8_t flags, char** expression, int howToDelete)
{
    bool applyOr = true;
    if (howToDelete > 0) {
        if (((expression[0][0] == '&') && !expression[0][1]) || ((tolower(expression[0][0]) == 'o') && (tolower(expression[0][1]) == 'n')) || ((tolower(expression[0][0]) == 'l') && (tolower(expression[0][1]) == 'y'))) {
            applyOr = false;
            howToDelete--;
            expression++;
        }
        if (howToDelete > 0) {
            uint32_t number = 0;
            if (!dexp_eval(expression[0], &number)) {
                {
                    sprintf(monbuf, "Invalid expression for number format.\n");
                    monprintf(monbuf);
                }
                return;
            }
            removeFlagFromConditionalBreakNo(address, (uint8_t)number, (flags | (flags >> 4)));
            {
                sprintf(monbuf, "Removed all specified breaks from %08x.\n", address);
                monprintf(monbuf);
            }
            return;
        }
        removeConditionalWithAddressAndFlag(address, flags, applyOr);
        removeConditionalWithAddressAndFlag(address, flags << 4, applyOr);
        {
            sprintf(monbuf, "Removed all specified breaks from %08x.\n", address);
            monprintf(monbuf);
        }
    } else {
        removeConditionalWithAddressAndFlag(address, flags, applyOr);
        removeConditionalWithAddressAndFlag(address, flags << 4, applyOr);
        {
            sprintf(monbuf, "Removed all specified breaks from %08x.\n", address);
            monprintf(monbuf);
        }
    }
    return;
}
void clearBreaks(uint32_t address, uint8_t flags, char** expression, int howToClear)
{
    (void)address; // unused params
    (void)expression; // unused params
    if (howToClear == 2) {
        removeConditionalWithFlag(flags, true);
        removeConditionalWithFlag(flags << 4, true);
    } else {
        removeConditionalWithFlag(flags, false);
        removeConditionalWithFlag(flags << 4, false);
    }
    {
        sprintf(monbuf, "Cleared all requested breaks.\n");
        monprintf(monbuf);
    }
}

void listBreaks(uint32_t address, uint8_t flags, char** expression, int howToList)
{
    (void)expression; // unused params
    flags |= (flags << 4);
    if (howToList) {
        printAllFlagConditionalsWithAddress(address, flags, true);
    } else {
        printAllFlagConditionals(flags, true);
    }
    {
        sprintf(monbuf, "\n");
        monprintf(monbuf);
    }
}

void executeBreakCommands(int n, char** cmd)
{
    char* command = cmd[0];
    int len = strlen(command);
    bool changed = false;
    if (len <= 4) {
        command = breakSymbolCombo(command, &len);
        changed = (len == 5);
    }
    if (!changed) {
        command = strdup(replaceAlias(cmd[0], breakAliasTable));
        changed = (strcmp(cmd[0], command));
    }
    if (!changed) {
        cmd[0][0] = '!';
        return;
    }
    cmd++;
    n--;
    void (*operation)(uint32_t, uint8_t, char**, int) = &makeBreak; //the function to be called

    uint8_t flag = 0;
    uint32_t address = 0;
    //if(strlen(command) == 1){
    //Cannot happen, that would mean cmd[0] != b
    //}
    char target;
    char ope;

    if (command[2] == '0') {
        if (n <= 0) {
            {
                sprintf(monbuf, "Invalid break command.\n");
                monprintf(monbuf);
            }
            free(command);
            return;
        }

        for (int i = 0; cmd[0][i]; i++) {
            cmd[0][i] = tolower(cmd[0][i]);
        }
        const char* replaced = replaceAlias(cmd[0], breakAliasTable);
        if (replaced == cmd[0]) {
            target = '*';
        } else {
            target = replaced[0];
            if ((target == 'c') || (target == 'd') || (target == 'l') || (target == 'm')) {
                command[4] = target;
                target = '*';
            }
            cmd++;
            n--;
        }
        command[2] = target;
    }

    if (command[4] == '0') {
        if (n <= 0) {
            {
                sprintf(monbuf, "Invalid break command.\n");
                monprintf(monbuf);
            }
            free(command);
            return;
        }

        for (int i = 0; cmd[0][i]; i++) {
            cmd[0][i] = tolower(cmd[0][i]);
        }
        ope = replaceAlias(cmd[0], breakAliasTable)[0];
        if ((ope == 'c') || (ope == 'd') || (ope == 'l') || (ope == 'm')) {
            command[4] = ope;
            cmd++;
            n--;
        } else {
            command[4] = 'm';
        }
    }

    switch (command[4]) {
    case 'l':
        operation = &listBreaks;
        break;
    case 'c':
        operation = &clearBreaks;
        break;
    case 'd':
        operation = &deleteBreak;
        break;

    case 'm':
    default:
        operation = &makeBreak;
    };

    switch (command[2]) {
    case 'g':
        switch (command[4]) {
        case 'l':
            debuggerBreakRegisterList((n > 0) && (tolower(cmd[0][0]) == 'v'));
            return;
        case 'c':
            debuggerBreakRegisterClear(n, cmd);
            return;
        case 'd':
            debuggerBreakRegisterDelete(n, cmd);
            return;

        case 'm':
            debuggerBreakRegister(n, cmd);
        default:
            return;
        };
        return;
    case '*':
        flag = 0xf;
        break;
    case 't':
        flag = 0x8;
        break;
    case 'a':
        flag = 0x4;
        break;
    case 'x':
        flag = 0xC;
        break;
    case 'r':
        flag = 0x2;
        break;
    case 'w':
        flag = 0x1;
        break;
    case 'i':
        flag = 0x3;
        break;
    default:
        free(command);
        return;
    };

    free(command);
    bool hasAddress = false;
    if ((n >= 1) && (operation != clearBreaks)) {
        if (!dexp_eval(cmd[0], &address)) {
            {
                sprintf(monbuf, "Invalid expression for address format.\n");
                monprintf(monbuf);
            }
            return;
        }
        hasAddress = true;
    }
    if (operation == listBreaks) {
        operation(address, flag, NULL, hasAddress);
        return;
    } else if (operation == clearBreaks) {
        if (!hasAddress && (n >= 1)) {
            if ((cmd[0][0] == '|' && cmd[0][1] == '|') || ((cmd[0][0] == 'O' || cmd[0][0] == 'o') && (cmd[0][1] == 'R' || cmd[0][1] == 'r'))) {
                operation(address, flag, NULL, 2);
            } else {
                operation(address, flag, NULL, 0);
            }
        } else {
            operation(address, flag, NULL, 0);
        }
    } else if (!hasAddress && (operation == deleteBreak)) {
        {
            sprintf(monbuf, "Delete breakpoint operation requires at least one address;\n");
            monprintf(monbuf);
        }
        {
            sprintf(monbuf, "Usage: break [type] delete [address] no.[number] --> Deletes breakpoint [number] of [address].\n");
            monprintf(monbuf);
        }
        //{ sprintf(monbuf, "Usage: [delete Operand] [address] End [address] --> Deletes range between [address] and [end]\n"); monprintf(monbuf); }
        {
            sprintf(monbuf, "Usage: break [type] delete [address]\n --> Deletes all breakpoints of [type] on [address].");
            monprintf(monbuf);
        }
        return;
    } else if (!hasAddress && (operation == makeBreak)) {
        {
            sprintf(monbuf, "Can only create breakpoints if an address is provided");
            monprintf(monbuf);
        }
        //print usage here
        return;
    } else {
        operation(address, flag, cmd + 1, n - 1);
        return;
    }
//brkcmd_special_register:
    switch (command[4]) {
    case 'l':
        debuggerBreakRegisterList((n > 0) && (tolower(cmd[0][0]) == 'v'));
        return;
    case 'c':
        debuggerBreakRegisterClear(n, cmd);
        return;
    case 'd':
        debuggerBreakRegisterDelete(n, cmd);
        return;

    case 'm':
        debuggerBreakRegister(n, cmd);
    default:
        return;
    };
    return;
}

void debuggerDisable(int n, char** args)
{
    if (n >= 3) {
        debuggerUsage("disable");
        return;
    }
    while (n > 1) {
        int i = 0;
        while (args[3 - n][i]) {
            args[3 - n][i] = tolower(args[2 - n][i]);
            i++;
        }
        if (strcmp(args[3 - n], "breg")) {
            enableRegBreak = false;
            {
                sprintf(monbuf, "Break on register disabled.\n");
                monprintf(monbuf);
            }
        } else if (strcmp(args[3 - n], "tbl")) {
            canUseTbl = false;
            useWordSymbol = false;
            {
                sprintf(monbuf, "Symbol table disabled.\n");
                monprintf(monbuf);
            }
        } else {
            {
                sprintf(monbuf, "Invalid command. Only tbl and breg are accepted as commands\n");
                monprintf(monbuf);
            }
            return;
        }
        n--;
    }
}

void debuggerEnable(int n, char** args)
{
    if (n >= 3) {
        debuggerUsage("enable");
        return;
    }
    while (n > 1) {
        int i = 0;
        while (args[3 - n][i]) {
            args[3 - n][i] = tolower(args[2 - n][i]);
            i++;
        }
        if (strcmp(args[3 - n], "breg")) {
            enableRegBreak = true;
            {
                sprintf(monbuf, "Break on register enabled.\n");
                monprintf(monbuf);
            }
        } else if (strcmp(args[3 - n], "tbl")) {
            canUseTbl = true;
            useWordSymbol = thereIsATable;
            {
                sprintf(monbuf, "Symbol table enabled.\n");
                monprintf(monbuf);
            }
        } else {
            {
                sprintf(monbuf, "Invalid command. Only tbl and breg are accepted as commands\n");
                monprintf(monbuf);
            }
            return;
        }
        n--;
    }
}

DebuggerCommand debuggerCommands[] = {
    //simple commands
    { "?", debuggerHelp, "Shows this help information. Type ? <command> for command help. Alias 'help', 'h'.", "[<command>]" },
    //	{ "n", debuggerNext, "Executes the next instruction.", "[<count>]" },
    //	{ "c", debuggerContinue, "Continues execution", NULL },
    //	// Hello command, shows Hello on the board

    //{ "br", debuggerBreakRead, "Break on read", "{address} {size}" },
    //{ "bw", debuggerBreakWrite, "Break on write", "{address} {size}" },
    //{ "bt", debuggerBreakWrite, "Break on write", "{address} {size}" },

    //{ "ba", debuggerBreakArm, "Adds an ARM breakpoint", "{address}" },
    //{ "bd", debuggerBreakDelete, "Deletes a breakpoint", "<number>" },
    //{ "bl", debuggerBreakList, "Lists breakpoints" },
    //{ "bpr", debuggerBreakRead, "Break on read", "{address} {size}" },
    //{ "bprc", debuggerBreakReadClear, "Clear break on read", NULL },
    //{ "bpw", debuggerBreakWrite, "Break on write", "{address} {size}" },
    //{ "bpwc", debuggerBreakWriteClear, "Clear break on write", NULL },
    { "breg", debuggerBreakRegister, "Breaks on a register specified value", "<register_number> {flag} {value}" },
    { "bregc", debuggerBreakRegisterClear, "Clears all break on register", "<register_number> {flag} {value}" },
    //{ "bt", debuggerBreakThumb, "Adds a THUMB breakpoint", "{address}" }

    //	//diassemble commands
    //	{ "d", debuggerDisassemble, "Disassembles instructions", "[<address> [<number>]]" },
    //	{ "da", debuggerDisassembleArm, "Disassembles ARM instructions", "[{address} [{number}]]" },
    //	{ "dt", debuggerDisassembleThumb, "Disassembles Thumb instructions", "[{address} [{number}]]" },

    { "db", debuggerDontBreak, "Don't break at the following address.", "[{address} [{number}]]" },
    { "dbc", debuggerDontBreakClear, "Clear the Don't Break list.", NULL },
    { "dload", debuggerDumpLoad, "Load raw data dump from file", "<file> {address}" },
    { "dsave", debuggerDumpSave, "Dump raw data to file", "<file> {address} {size}" },
    //	{ "dn", debuggerDisassembleNear, "Disassembles instructions near PC", "[{number}]" },

    { "disable", debuggerDisable, "Disables operations.", "tbl|breg" },
    { "enable", debuggerEnable, "Enables operations.", "tbl|breg" },

    { "eb", debuggerEditByte, "Modify memory location (byte)", "{address} {value}*" },
    { "eh", debuggerEditHalfWord, "Modify memory location (half-word)", "{address} {value}*" },
    { "ew", debuggerEditWord, "Modify memory location (word)", "{address} {value}*" },
    { "er", debuggerEditRegister, "Modify register", "<register number> {value}" },

    { "eval", debuggerEval, "Evaluate expression", "{expression}" },

    { "fillb", debuggerFillByte, "Fills memory location (byte)", "{address} {value} {number of times}" },
    { "fillh", debuggerFillHalfWord, "Fills memory location (half-word)", "{address} {value} {number of times}" },
    { "fillw", debuggerFillWord, "Fills memory location (word)", "{address} {value} {number of times}" },

    { "copyb", debuggerCopyByte, "Copies memory content (byte)", "{address} {second address} {size} optional{repeat}" },
    { "copyh", debuggerCopyHalfWord, "Copies memory content (half-word)", "{address} {second address} {size} optional{repeat}" },
    { "copyw", debuggerCopyWord, "Copies memory content (word)", "{address} {second address} {size} optional{repeat}" },

    { "ft", debuggerFindText, "Search memory for ASCII-string.", "<start> [<max-result>] <string>" },
    { "fh", debuggerFindHex, "Search memory for hex-string.", "<start> [<max-result>] <hex-string>" },
    { "fr", debuggerFindResume, "Resume current search.", "[<max-result>]" },

    { "io", debuggerIo, "Show I/O registers status", "[video|video2|dma|timer|misc]" },
    //	{ "load", debuggerReadState, "Loads a Fx type savegame", "<number>" },

    { "mb", debuggerMemoryByte, "Shows memory contents (bytes)", "{address}" },
    { "mh", debuggerMemoryHalfWord, "Shows memory contents (half-words)", "{address}" },
    { "mw", debuggerMemoryWord, "Shows memory contents (words)", "{address}" },
    { "ms", debuggerStringRead, "Shows memory contents (table string)", "{address}" },

    { "r", debuggerRegisters, "Shows ARM registers", NULL },
    //	{ "rt", debuggerRunTo, "Run to address", "{address}" },
    //	{ "rta", debuggerRunToArm, "Run to address (ARM)", "{address}" },
    //	{ "rtt", debuggerRunToThumb, "Run to address (Thumb)", "{address}" },

    //	{ "reset", debuggerResetSystem, "Resets the system", NULL },
    //	{ "reload", debuggerReloadRom, "Reloads the ROM", "optional {rom path}" },
    { "execute", debuggerExecuteCommands, "Executes commands from a text file", "{file path}" },

    //	{ "save", debuggerWriteState, "Creates a Fx type savegame", "<number>" },
    //	{ "sbreak", debuggerBreak, "Adds a breakpoint on the given function", "<function>|<line>|<file:line>" },
    { "sradix", debuggerSetRadix, "Sets the print radix", "<radix>" },
    //	{ "sprint", debuggerPrint, "Print the value of a expression (if known)", "[/x|/o|/d] <expression>" },
    { "ssymbols", debuggerSymbols, "List symbols", "[<symbol>]" },
    //#ifndef FINAL_VERSION
    //	{ "strace", debuggerDebug, "Sets the trace level", "<value>" },
    //#endif
    //#ifdef DEV_VERSION
    //	{ "sverbose", debuggerVerbose, "Change verbose setting", "<value>" },
    //#endif
    { "swhere", debuggerWhere, "Shows call chain", NULL },

    { "tbl", debuggerReadCharTable, "Loads a character table", "<file>" },

    //	{ "trace", debuggerTrace, "Control tracer", "start|stop|file <file>" },
    { "var", debuggerVar, "Define variables", "<name> {variable}" },
    { NULL, NULL, NULL, NULL } // end marker
};

void printFlagHelp()
{
    monprintf("Flags are combinations of six distinct characters:\n");
    monprintf("\t\te --> Equal to;\n");
    monprintf("\t\tg --> Greater than;\n");
    monprintf("\t\tl --> Less than;\n");
    monprintf("\t\ts --> signed;\n");
    monprintf("\t\tu --> unsigned (assumed by ommision);\n");
    monprintf("\t\tn --> not;\n");
    monprintf("Ex: ge -> greater or equal; ne -> not equal; lg --> less or greater (same as not equal);\n");
    monprintf("s and u parts cannot be used in the same line, and are not negated by n;\n");
    monprintf("Special flags: always(all true), never(all false).\n");
}

void debuggerUsage(const char* cmd)
{
    if (!strcmp(cmd, "break")) {
        monprintf("Break command, composed of three parts:\n");
        monprintf("Break (b, bp or break): Indicates a break command;\n");
        monprintf("Type of break: Indicates the type of break the command applies to;\n");
        monprintf("Command: Indicates the type of command to be applied.\n");
        monprintf("Type Flags:\n\tt (thumb): The Thumb execution mode.\n");
        monprintf("\ta (ARM): The ARM execution mode.\n");
        monprintf("\tx (execution, exe, exec, e): Any execution mode.\n");
        monprintf("\tr (read): When a read occurs.\n");
        monprintf("\tw (write): When a write occurs.\n");
        monprintf("\ti (io, access,acc): When memory access (read or write) occurs.\n");
        monprintf("\tg (register, reg): Special On Register value change break.\n");
        monprintf("\t* (any): On any occasion (except register change).Omission value.\n");
        monprintf("Cmd Flags:\n\tm (make): Create a breakpoint.Default omission value.\n");
        monprintf("\tl (list,lst): Lists all existing breakpoints of the specified type.\n");
        monprintf("\td (delete,del): Deletes a specific breakpoint of the specified type.\n");
        monprintf("\tc (clear, clean, cls): Erases all breakpoints of the specified type.\n");
        monprintf("\n");
        monprintf("All those flags can be combined in order to access the several break functions\n");
        monprintf("EX: btc clears all breaks; bx, bxm creates a breakpoint on any type of execution.\n");
        monprintf("All commands can be built by using [b|bp][TypeFlag][CommandFlag];\n");
        monprintf("All commands can be built by using [b|bp|break] [TypeFlag|alias] [CommandFlag|alias];\n");
        monprintf("Each command has separate arguments from each other.\nFor more details, use help b[reg|m|d|c|l]\n");
        return;
    }
    if (!strcmp(cmd, "breg")) {
        monprintf("Break on register command, special case of the break command.\n");
        monprintf("It allows the user to break when a certain value is inside a register.\n");
        monprintf("All register breaks are conditional.\n");
        monprintf("Usage: breg [regName] [condition] [Expression].\n");
        monprintf("regName is between r0 and r15 (PC, LR and SP included);\n");
        monprintf("expression is an evaluatable expression whose value determines when to break;\n");
        monprintf("condition is the condition to be evaluated in typeFlags.\n");
        printFlagHelp();
        monprintf("---------!!!WARNING!!!---------\n");
        monprintf("Register checking and breaking is extremely expensive for the computer.\n");
        monprintf("On one of the test machines, a maximum value of 600% for speedup collapsed\n");
        monprintf("to 350% just from having them enabled.\n");
        monprintf("If (or while) not needed, you can have a speedup by disabling them, using\n");
        monprintf("disable breg.\n");
        monprintf("Breg is disabled by default. Re-enable them using enable breg.\n");
        monprintf("Use example: breg r0 ne 0x0 --> Breaks as soon as r0 is not 0.\n");
        return;
    }
    if (!strcmp(cmd, "bm")) {
        monprintf("Create breakpoint command. Used to place a breakpoint on a given address.\n");
        monprintf("It allows for breaks on execution(any processor mode) and on access(r/w).\n");
        monprintf("Breaks can be Conditional or Inconditional.\n\n");
        monprintf("Inconditional breaks:\nUsage: [breakTag] [address]\n");
        monprintf("Simplest of the two, the old type of breaks. Creates a breakpoint that, when\n");
        monprintf("the given type flag occurs (like a read, or a run when in thumb mode), halts;\n\n");
        monprintf("Conditional breaks:\n");
        monprintf("Usage:\n\t[breakTag] [address] if {'<type> [expr] [cond] '<type> [expr] <&&,||>}\n");
        monprintf("Where <> elements are optional, {} are repeateable;\n");
        monprintf("[expression] are evaluatable expressions, in the usual VBA format\n(that is, eval acceptable);\n");
        monprintf("type is the type of that expression. Uses C-like names. Omission means integer.\n");
        monprintf("cond is the condition to be evaluated.\n");
        monprintf("If && or || are not present, the chain of evaluation stops.\n");
        monprintf("&& states the next condition must happen with the previous one, or the break\nfails.\n");
        monprintf("|| states the next condition is independent from the last one, and break\nseparately.\n\n");
        monprintf("Type can be:\n");
        monprintf("   [uint8_t, b, byte],[uint16_t, h, hword, halfword],[uint32_t,w, word]\n");
        monprintf("   [int8_t, sb, sbyte],[int16_t, sh, shword, short, shalfword],[int32_t, int, sw, word]\n");
        monprintf("Types have to be preceded by a ' ex: 'int, 'uint8_t\n\n");
        monprintf("Conditions may be:\n");
        monprintf("C-like:\t\t[<], [<=], [>], [>=] , [==], [!= or <>]\n");
        monprintf("ASM-like:\t[lt], [le], [gt], [ge] , [eq], [ne]\n\n");
        monprintf("EX:	bw 0x03005008 if old_value == 'uint32_t [0x03005008]\n");
        monprintf("Breaks on write from 0x03005008, when the old_value variable, that is assigned\n");
        monprintf("as the previous memory value when a write is performed, is equal to the new\ncontents of 0x03005008.\n\n");
        monprintf("EX:	bx 0x08000500 if r0 == 1 || r0 > 1 && r2 == 0 || 'uint8_t [r7] == 5\n");
        monprintf("Breaks in either thumb or arm execution of 0x08000500, if r0's contents are 1,\n");
        monprintf("or if r0's contents are bigger than 1 and r2 is equal to 0, or the content of\nthe address at r7(as byte) is equal to 5.\n");
        monprintf("It will not break if r0 > 1 and r2 != 0.\n");
        return;
    }
    if (!strcmp(cmd, "bl")) {
        monprintf("List breakpoints command. Used to view breakpoints.\n");
        monprintf("Usage: [breakTag] <address> <v>\n");
        monprintf("It will list all breaks on the specified type (read, write..).\n");
        monprintf("If (optional) address is included, it will try and list all breaks of that type\n");
        monprintf("for that address.\n");
        monprintf("The numbers shown on that list (No.) are the ones needed to delete it directly.\n");
        monprintf("v option lists all requested values, even if empty.\n");
        return;
    }
    if (!strcmp(cmd, "bc")) {
        monprintf("Clear breakpoints command. Clears all specified breakpoints.\n");
        monprintf("Usage: [breakTag] <or,||>\n");
        monprintf("It will delete all breaks on all addresses for the specified type.\n");
        monprintf("If (optional) or is included, it will try and delete all breaks associated with\n");
        monprintf("the flags. EX: bic or --> Deletes all breaks on read and all on write.\n");
        return;
    }
    if (!strcmp(cmd, "bd")) {
        monprintf("Delete breakpoint command. Clears the specified breakpoint.\n");
        monprintf("Usage: [breakTag] [address] <only> [number]\n");
        monprintf("It will delete the numbered break on that addresses for the specified type.\n");
        monprintf("If only is included, it will delete only breaks with the specified flag.\n");
        monprintf("EX: bxd 0x8000000 only -->Deletes all breaks on 0x08000000 that break on both\n");
        monprintf("arm and thumb modes. Thumb only or ARM only are unnafected.\n");
        monprintf("EX: btd 0x8000000 5 -->Deletes the thumb break from the 5th break on 0x8000000.\n");
        monprintf("---------!!!WARNING!!!---------\n");
        monprintf("Break numbers are volatile, and may change at any time. before deleting any one\n");
        monprintf("breakpoint, list them to see if the number hasn't changed. The numbers may\n");
        monprintf("change only when you add or delete a breakpoint to that address. Numbers are \n");
        monprintf("internal to each address.\n");
        return;
    }

    for (int i = 0;; i++) {
        if (debuggerCommands[i].name) {
            if (!strcmp(debuggerCommands[i].name, cmd)) {
                sprintf(monbuf, "%s %s\t%s\n",
                    debuggerCommands[i].name,
                    debuggerCommands[i].syntax ? debuggerCommands[i].syntax : "",
                    debuggerCommands[i].help);
                monprintf(monbuf);
                break;
            }
        } else {
            {
                sprintf(monbuf, "Unrecognized command '%s'.", cmd);
                monprintf(monbuf);
            }
            break;
        }
    }
}

void debuggerHelp(int n, char** args)
{
    if (n == 2) {
        debuggerUsage(args[1]);
    } else {
        for (int i = 0;; i++) {
            if (debuggerCommands[i].name) {
                {
                    sprintf(monbuf, "%-10s%s\n", debuggerCommands[i].name, debuggerCommands[i].help);
                    monprintf(monbuf);
                }
            } else
                break;
        }
        {
            sprintf(monbuf, "%-10s%s\n", "break", "Breakpoint commands");
            monprintf(monbuf);
        }
    }
}

char* strqtok(char* string, const char* ctrl)
{
    static char* nexttoken = NULL;
    char* str;

    if (string != NULL)
        str = string;
    else {
        if (nexttoken == NULL)
            return NULL;
        str = nexttoken;
    };

    char deli[32];
    memset(deli, 0, 32 * sizeof(char));
    while (*ctrl) {
        deli[*ctrl >> 3] |= (1 << (*ctrl & 7));
        ctrl++;
    };
    // can't allow to be set
    deli['"' >> 3] &= ~(1 << ('"' & 7));

    // jump over leading delimiters
    while ((deli[*str >> 3] & (1 << (*str & 7))) && *str)
        str++;

    if (*str == '"') {
        string = ++str;

        // only break if another quote or end of string is found
        while ((*str != '"') && *str)
            str++;
    } else {
        string = str;

        // break on delimiter
        while (!(deli[*str >> 3] & (1 << (*str & 7))) && *str)
            str++;
    };

    if (string == str) {
        nexttoken = NULL;
        return NULL;
    } else {
        if (*str) {
            *str = 0;
            nexttoken = str + 1;
        } else
            nexttoken = NULL;

        return string;
    };
};

void dbgExecute(char* toRun)
{
    char* commands[40];
    int commandCount = 0;
    commands[0] = strqtok(toRun, " \t\n");
    if (commands[0] == NULL)
        return;
    commandCount++;
    while ((commands[commandCount] = strqtok(NULL, " \t\n"))) {
        commandCount++;
        if (commandCount == 40)
            break;
    }

    //from here on, new algorithm.
    // due to the division of functions, some steps have to be made

    //first, convert the command name to a standart lowercase form
    //if more lowercasing needed, do it on the caller.
    for (int i = 0; commands[0][i]; i++) {
        commands[0][i] = tolower(commands[0][i]);
    }

    // checks if it is a quit command, if so quits.
    //if (isQuitCommand(commands[0])){
    //	if (quitConfirm()){
    //		debugger = false;
    //		emulating = false;
    //	}
    //	return;
    //}

    commands[0] = (char*)replaceAlias(commands[0], cmdAliasTable);

    if (commands[0][0] == 'b') {
        executeBreakCommands(commandCount, commands);
        if (commands[0][0] == '!')
            commands[0][0] = 'b';
        else
            return;
    }

    //although it mights seem weird, the old step is the last one to be executed.
    for (int j = 0;; j++) {
        if (debuggerCommands[j].name == NULL) {
            {
                sprintf(monbuf, "Unrecognized command %s. Type h for help.\n", commands[0]);
                monprintf(monbuf);
            }
            return;
        }
        if (!strcmp(commands[0], debuggerCommands[j].name)) {
            debuggerCommands[j].function(commandCount, commands);
            return;
        }
    }
}

void dbgExecute(std::string& cmd)
{
    char* dbgCmd = new char[cmd.length() + 1];
    strcpy(dbgCmd, cmd.c_str());
    dbgExecute(dbgCmd);
    delete[] dbgCmd;
}

int remoteTcpSend(char* data, int len)
{
    return send(remoteSocket, data, len, 0);
}

int remoteTcpRecv(char* data, int len)
{
    return recv(remoteSocket, data, len, 0);
}

bool remoteTcpInit()
{
    if (remoteSocket == -1) {
#ifdef _WIN32
        WSADATA wsaData;
        int error = WSAStartup(MAKEWORD(1, 1), &wsaData);
#endif // _WIN32
        SOCKET s = socket(PF_INET, SOCK_STREAM, 0);

        remoteListenSocket = s;

        if (s < 0) {
            fprintf(stderr, "Error opening socket\n");
            exit(-1);
        }
        int tmp = 1;
        setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char*)&tmp, sizeof(tmp));

        //    char hostname[256];
        //    gethostname(hostname, 256);

        //    hostent *ent = gethostbyname(hostname);
        //    unsigned long a = *((unsigned long *)ent->h_addr);

        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(remotePort);
        addr.sin_addr.s_addr = htonl(0);
        int count = 0;
        while (count < 3) {
            if (bind(s, (sockaddr*)&addr, sizeof(addr))) {
                addr.sin_port = htons(ntohs(addr.sin_port) + 1);
            } else
                break;
        }
        if (count == 3) {
            fprintf(stderr, "Error binding \n");
            exit(-1);
        }

        fprintf(stderr, "Listening for a connection at port %d\n",
            ntohs(addr.sin_port));

        if (listen(s, 1)) {
            fprintf(stderr, "Error listening\n");
            exit(-1);
        }
        socklen_t len = sizeof(addr);

#ifdef _WIN32
        int flag = 0;
        ioctlsocket(s, FIONBIO, (unsigned long*)&flag);
#endif // _WIN32
        SOCKET s2 = accept(s, (sockaddr*)&addr, &len);
        if (s2 > 0) {
            fprintf(stderr, "Got a connection from %s %d\n",
                inet_ntoa((in_addr)addr.sin_addr),
                ntohs(addr.sin_port));
        } else {
#ifdef _WIN32
            int error = WSAGetLastError();
#endif // _WIN32
        }
        //char dummy;
        //recv(s2, &dummy, 1, 0);
        //if(dummy != '+') {
        //  fprintf(stderr, "ACK not received\n");
        //  exit(-1);
        //}
        remoteSocket = s2;
        //    close(s);
    }
    return true;
}

void remoteTcpCleanUp()
{
    if (remoteSocket > 0) {
        fprintf(stderr, "Closing remote socket\n");
        close(remoteSocket);
        remoteSocket = -1;
    }
    if (remoteListenSocket > 0) {
        fprintf(stderr, "Closing listen socket\n");
        close(remoteListenSocket);
        remoteListenSocket = -1;
    }
}

int remotePipeSend(char* data, int len)
{
    int res = write(1, data, len);
    return res;
}

int remotePipeRecv(char* data, int len)
{
    int res = read(0, data, len);
    return res;
}

bool remotePipeInit()
{
    //  char dummy;
    //  if (read(0, &dummy, 1) == 1)
    //  {
    //    if(dummy != '+') {
    //      fprintf(stderr, "ACK not received\n");
    //      exit(-1);
    //    }
    //  }

    return true;
}

void remotePipeCleanUp()
{
}

void remoteSetPort(int port)
{
    remotePort = port;
}

void remoteSetProtocol(int p)
{
    if (p == 0) {
        remoteSendFnc = remoteTcpSend;
        remoteRecvFnc = remoteTcpRecv;
        remoteInitFnc = remoteTcpInit;
        remoteCleanUpFnc = remoteTcpCleanUp;
    } else {
        remoteSendFnc = remotePipeSend;
        remoteRecvFnc = remotePipeRecv;
        remoteInitFnc = remotePipeInit;
        remoteCleanUpFnc = remotePipeCleanUp;
    }
}

void remoteInit()
{
    if (remoteInitFnc)
        remoteInitFnc();
}

void remotePutPacket(const char* packet)
{
    const char* hex = "0123456789abcdef";

    size_t count = strlen(packet);
    char* buffer = new char[count + 5];

    unsigned char csum = 0;

    char* p = buffer;
    *p++ = '$';

    for (size_t i = 0; i < count; i++) {
        csum += packet[i];
        *p++ = packet[i];
    }
    *p++ = '#';
    *p++ = hex[csum >> 4];
    *p++ = hex[csum & 15];
    *p++ = 0;
    //log("send: %s\n", buffer);

    char c = 0;
    while (c != '+') {
        remoteSendFnc(buffer, (int)count + 4);

        if (remoteRecvFnc(&c, 1) < 0) {
            delete[] buffer;
            return;
        }
        //    fprintf(stderr,"sent:%s recieved:%c\n",buffer,c);
    }

    delete[] buffer;
}

void remoteOutput(const char* s, uint32_t addr)
{
    char buffer[16384];

    char* d = buffer;
    *d++ = 'O';

    if (s) {
        char c = *s++;
        while (c) {
            sprintf(d, "%02x", c);
            d += 2;
            c = *s++;
        }
    } else {
        char c = debuggerReadByte(addr);
        addr++;
        while (c) {
            sprintf(d, "%02x", c);
            d += 2;
            c = debuggerReadByte(addr);
            addr++;
        }
    }
    remotePutPacket(buffer);
    //  fprintf(stderr, "Output sent %s\n", buffer);
}

void remoteSendSignal()
{
    char buffer[1024];
    sprintf(buffer, "S%02x", remoteSignal);
    remotePutPacket(buffer);
}

void remoteSendStatus()
{
    char buffer[1024];
    sprintf(buffer, "T%02x", remoteSignal);
    char* s = buffer;
    s += 3;
    for (int i = 0; i < 15; i++) {
        uint32_t v = reg[i].I;
        sprintf(s, "%02x:%02x%02x%02x%02x;", i,
            (v & 255),
            (v >> 8) & 255,
            (v >> 16) & 255,
            (v >> 24) & 255);
        s += 12;
    }
    uint32_t v = armNextPC;
    sprintf(s, "0f:%02x%02x%02x%02x;", (v & 255),
        (v >> 8) & 255,
        (v >> 16) & 255,
        (v >> 24) & 255);
    s += 12;
    CPUUpdateCPSR();
    v = reg[16].I;
    sprintf(s, "19:%02x%02x%02x%02x;", (v & 255),
        (v >> 8) & 255,
        (v >> 16) & 255,
        (v >> 24) & 255);
    s += 12;
    *s = 0;
    //log("Sending %s\n", buffer);
    remotePutPacket(buffer);
}

void remoteBinaryWrite(char* p)
{
    uint32_t address;
    int count;
    sscanf(p, "%x,%x:", &address, &count);
    //  monprintf("Binary write for %08x %d\n", address, count);

    p = strchr(p, ':');
    p++;
    for (int i = 0; i < count; i++) {
        uint8_t b = *p++;
        switch (b) {
        case 0x7d:
            b = *p++;
            debuggerWriteByte(address, (b ^ 0x20));
            address++;
            break;
        default:
            debuggerWriteByte(address, b);
            address++;
            break;
        }
    }
    //  monprintf("ROM is %08x\n", debuggerReadMemory(0x8000254));
    remotePutPacket("OK");
}

void remoteMemoryWrite(char* p)
{
    uint32_t address;
    int count;
    sscanf(p, "%x,%x:", &address, &count);
    //  monprintf("Memory write for %08x %d\n", address, count);

    p = strchr(p, ':');
    p++;
    for (int i = 0; i < count; i++) {
        uint8_t v = 0;
        char c = *p++;
        if (c <= '9')
            v = (c - '0') << 4;
        else
            v = (c + 10 - 'a') << 4;
        c = *p++;
        if (c <= '9')
            v += (c - '0');
        else
            v += (c + 10 - 'a');
        debuggerWriteByte(address, v);
        address++;
    }
    //  monprintf("ROM is %08x\n", debuggerReadMemory(0x8000254));
    remotePutPacket("OK");
}

void remoteMemoryRead(char* p)
{
    uint32_t address;
    int count;
    sscanf(p, "%x,%x:", &address, &count);
    //  monprintf("Memory read for %08x %d\n", address, count);

    char* buffer = new char[(count*2)+1];

    char* s = buffer;
    for (int i = 0; i < count; i++) {
        uint8_t b = debuggerReadByte(address);
        sprintf(s, "%02x", b);
        address++;
        s += 2;
    }
    *s = 0;
    remotePutPacket(buffer);

    delete[] buffer;
}

void remoteQuery(char* p)
{
    if (!strncmp(p, "fThreadInfo", 11)) {
        remotePutPacket("m1");
    } else if (!strncmp(p, "sThreadInfo", 11)) {
        remotePutPacket("l");
    } else if (!strncmp(p, "Supported", 9)) {
        remotePutPacket("PacketSize=1000");
    } else if (!strncmp(p, "HostInfo", 8)) {
        remotePutPacket("cputype:12;cpusubtype:5;ostype:unknown;vendor:nintendo;endian:little;ptrsize:4;");
    } else if (!strncmp(p, "C", 1)) {
        remotePutPacket("QC1");
    } else if (!strncmp(p, "Attached", 8)) {
        remotePutPacket("1");
    } else if (!strncmp(p, "Symbol", 6)) {
        remotePutPacket("OK");
    } else if (!strncmp(p, "Rcmd,", 5)) {
        p += 5;
        std::string cmd = HexToString(p);
        dbgExecute(cmd);
        remotePutPacket("OK");
    } else {
        fprintf(stderr, "Unknown packet %s\n", --p);
        remotePutPacket("");
    }
}

void remoteStepOverRange(char* p)
{
    uint32_t address;
    uint32_t final;
    sscanf(p, "%x,%x", &address, & final);

    remotePutPacket("OK");

    remoteResumed = true;
    do {
        CPULoop(1);
        if (debugger)
            break;
    } while (armNextPC >= address && armNextPC < final);

    remoteResumed = false;

    remoteSendStatus();
}

void remoteSetBreakPoint(char* p)
{
    uint32_t address;
    int count;
    sscanf(p, ",%x,%x#", &address, &count);

    for (int n = 0; n < count; n += 4)
        addConditionalBreak(address + n, armState ? 0x04 : 0x08);

    // Out of bounds memory checks
    //if (address < 0x2000000 || address > 0x3007fff) {
    //	remotePutPacket("E01");
    //	return;
    //}

    //if (address > 0x203ffff && address < 0x3000000) {
    //	remotePutPacket("E01");
    //	return;
    //}

    //uint32_t final = address + count;

    //if (address < 0x2040000 && final > 0x2040000) {
    //	remotePutPacket("E01");
    //	return;
    //}
    //else if (address < 0x3008000 && final > 0x3008000) {
    //	remotePutPacket("E01");
    //	return;
    //}
    remotePutPacket("OK");
}

void remoteClearBreakPoint(char* p)
{
    int result = 0;
    uint32_t address;
    int count;
    sscanf(p, ",%x,%x#", &address, &count);

    for (int n = 0; n < count; n += 4)
        result = removeConditionalWithAddressAndFlag(address + n, armState ? 0x04 : 0x08, true);

    if (result != -2)
        remotePutPacket("OK");
    else
        remotePutPacket("");
}

void remoteSetMemoryReadBreakPoint(char* p)
{
    uint32_t address;
    int count;
    sscanf(p, ",%x,%x#", &address, &count);

    for (int n = 0; n < count; n++)
        addConditionalBreak(address + n, 0x02);

    // Out of bounds memory checks
    //if (address < 0x2000000 || address > 0x3007fff) {
    //	remotePutPacket("E01");
    //	return;
    //}

    //if (address > 0x203ffff && address < 0x3000000) {
    //	remotePutPacket("E01");
    //	return;
    //}

    //uint32_t final = address + count;

    //if (address < 0x2040000 && final > 0x2040000) {
    //	remotePutPacket("E01");
    //	return;
    //}
    //else if (address < 0x3008000 && final > 0x3008000) {
    //	remotePutPacket("E01");
    //	return;
    //}
    remotePutPacket("OK");
}

void remoteClearMemoryReadBreakPoint(char* p)
{
    bool error = false;
    int result;
    uint32_t address;
    int count;
    sscanf(p, ",%x,%x#", &address, &count);

    for (int n = 0; n < count; n++) {
        result = removeConditionalWithAddressAndFlag(address + n, 0x02, true);
        if (result == -2)
            error = true;
    }

    if (!error)
        remotePutPacket("OK");
    else
        remotePutPacket("");
}

void remoteSetMemoryAccessBreakPoint(char* p)
{
    uint32_t address;
    int count;
    sscanf(p, ",%x,%x#", &address, &count);

    for (int n = 0; n < count; n++)
        addConditionalBreak(address + n, 0x03);

    // Out of bounds memory checks
    //if (address < 0x2000000 || address > 0x3007fff) {
    //	remotePutPacket("E01");
    //	return;
    //}

    //if (address > 0x203ffff && address < 0x3000000) {
    //	remotePutPacket("E01");
    //	return;
    //}

    //uint32_t final = address + count;

    //if (address < 0x2040000 && final > 0x2040000) {
    //	remotePutPacket("E01");
    //	return;
    //}
    //else if (address < 0x3008000 && final > 0x3008000) {
    //	remotePutPacket("E01");
    //	return;
    //}
    remotePutPacket("OK");
}

void remoteClearMemoryAccessBreakPoint(char* p)
{
    bool error = false;
    int result;
    uint32_t address;
    int count;
    sscanf(p, ",%x,%x#", &address, &count);

    for (int n = 0; n < count; n++) {
        result = removeConditionalWithAddressAndFlag(address + n, 0x03, true);
        if (result == -2)
            error = true;
    }

    if (!error)
        remotePutPacket("OK");
    else
        remotePutPacket("");
}

void remoteWriteWatch(char* p, bool active)
{
    uint32_t address;
    int count;
    sscanf(p, ",%x,%x#", &address, &count);

    if (active) {
        for (int n = 0; n < count; n++)
            addConditionalBreak(address + n, 0x01);
    } else {
        for (int n = 0; n < count; n++)
            removeConditionalWithAddressAndFlag(address + n, 0x01, true);
    }

    // Out of bounds memory check
    //fprintf(stderr, "Write watch for %08x %d\n", address, count);

    //if(address < 0x2000000 || address > 0x3007fff) {
    //  remotePutPacket("E01");
    //  return;
    //}

    //if(address > 0x203ffff && address < 0x3000000) {
    //  remotePutPacket("E01");
    //  return;
    //}

    // uint32_t final = address + count;

//if(address < 0x2040000 && final > 0x2040000) {
//  remotePutPacket("E01");
//  return;
//} else if(address < 0x3008000 && final > 0x3008000) {
//  remotePutPacket("E01");
//  return;
//}

#ifdef BKPT_SUPPORT
    for (int i = 0; i < count; i++) {
        if ((address >> 24) == 2)
            freezeWorkRAM[address & 0x3ffff] = active;
        else
            freezeInternalRAM[address & 0x7fff] = active;
        address++;
    }
#endif

    remotePutPacket("OK");
}

void remoteReadRegister(char* p)
{
    int r;
    sscanf(p, "%x", &r);
    if(r < 0 || r > 15)
    {
        remotePutPacket("E 00");
        return;
    }
    char buffer[1024];
    char* s = buffer;
    uint32_t v = reg[r].I;
    sprintf(s, "%02x%02x%02x%02x", v & 255, (v >> 8) & 255,
        (v >> 16) & 255, (v >> 24) & 255);
    remotePutPacket(buffer);
}

void remoteReadRegisters(char* p)
{
    (void)p; // unused params
    char buffer[1024];

    char* s = buffer;
    int i;
    // regular registers
    for (i = 0; i < 15; i++) {
        uint32_t v = reg[i].I;
        sprintf(s, "%02x%02x%02x%02x", v & 255, (v >> 8) & 255,
            (v >> 16) & 255, (v >> 24) & 255);
        s += 8;
    }
    // PC
    uint32_t pc = armNextPC;
    sprintf(s, "%02x%02x%02x%02x", pc & 255, (pc >> 8) & 255,
        (pc >> 16) & 255, (pc >> 24) & 255);
    s += 8;

    // floating point registers (24-bit)
    for (i = 0; i < 8; i++) {
        sprintf(s, "000000000000000000000000");
        s += 24;
    }

    // FP status register
    sprintf(s, "00000000");
    s += 8;
    // CPSR
    CPUUpdateCPSR();
    uint32_t v = reg[16].I;
    sprintf(s, "%02x%02x%02x%02x", v & 255, (v >> 8) & 255,
        (v >> 16) & 255, (v >> 24) & 255);
    s += 8;
    *s = 0;
    remotePutPacket(buffer);
}

void remoteWriteRegister(char* p)
{
    int r;

    sscanf(p, "%x=", &r);

    if(r < 0 || r > 15)
    {
        remotePutPacket("E 00");
        return;
    }

    p = strchr(p, '=');
    p++;

    char c = *p++;

    uint32_t v = 0;

    uint8_t data[4] = { 0, 0, 0, 0 };

    int i = 0;

    while (i < 4) {
        uint8_t b = 0;
        if (c <= '9')
            b = (c - '0') << 4;
        else
            b = (c + 10 - 'a') << 4;
        c = *p++;
        if (c <= '9')
            b += (c - '0');
        else
            b += (c + 10 - 'a');
        data[i++] = b;
        c = *p++;
    }

    v = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);

    //  monprintf("Write register %d=%08x\n", r, v);
    reg[r].I = v;
    if (r == 15) {
        armNextPC = v;
        if (armState)
            reg[15].I = v + 4;
        else
            reg[15].I = v + 2;
    }
    remotePutPacket("OK");
}

void remoteStubMain()
{
    if (!debugger)
        return;

    if (remoteResumed) {
        remoteSendStatus();
        remoteResumed = false;
    }

    const char* hex = "0123456789abcdef";
    while (1) {
        char ack;
        char buffer[1024];
        int res = remoteRecvFnc(buffer, 1024);

        if (res == -1) {
            fprintf(stderr, "GDB connection lost\n");
            debugger = false;
            break;
        } else if (res == -2)
            break;
        if (res < 1024) {
            buffer[res] = 0;
        } else {
            fprintf(stderr, "res=%d\n", res);
        }

        //    fprintf(stderr, "res=%d Received %s\n",res, buffer);
        char c = buffer[0];
        char* p = &buffer[0];
        int i = 0;
        unsigned char csum = 0;
        while (i < res) {
            if (buffer[i] == '$') {
                i++;
                csum = 0;
                c = buffer[i];
                p = &buffer[i + 1];
                while ((i < res) && (buffer[i] != '#')) {
                    csum += buffer[i];
                    i++;
                }
            } else if (buffer[i] == '#') {
                buffer[i] = 0;
                if ((i + 2) < res) {
                    if ((buffer[i + 1] == hex[csum >> 4]) && (buffer[i + 2] == hex[csum & 0xf])) {
                        ack = '+';
                        remoteSendFnc(&ack, 1);
                        //fprintf(stderr, "SentACK c=%c\n",c);
                        //process message...
                        char type;
                        switch (c) {
                        case '?':
                            remoteSendSignal();
                            break;
                        case 'D':
                            remotePutPacket("OK");
                            remoteResumed = true;
                            debugger = false;
                            return;
                        case 'e':
                            remoteStepOverRange(p);
                            break;
                        case 'k':
                            remotePutPacket("OK");
                            debugger = false;
                            emulating = false;
                            return;
                        case 'C':
                            remoteResumed = true;
                            debugger = false;
                            return;
                        case 'c':
                            remoteResumed = true;
                            debugger = false;
                            return;
                        case 's':
                            remoteResumed = true;
                            remoteSignal = 5;
                            CPULoop(1);
                            if (remoteResumed) {
                                remoteResumed = false;
                                remoteSendStatus();
                            }
                            break;
                        case 'g':
                            remoteReadRegisters(p);
                            break;
                        case 'p':
                            remoteReadRegister(p);
                            break;
                        case 'P':
                            remoteWriteRegister(p);
                            break;
                        case 'M':
                            remoteMemoryWrite(p);
                            break;
                        case 'm':
                            remoteMemoryRead(p);
                            break;
                        case 'X':
                            remoteBinaryWrite(p);
                            break;
                        case 'H':
                            remotePutPacket("OK");
                            break;
                        case 'q':
                            remoteQuery(p);
                            break;
                        case 'Z':
                            type = *p++;
                            if (type == '0') {
                                remoteSetBreakPoint(p);
                            } else if (type == '1') {
                                remoteSetBreakPoint(p);
                            } else if (type == '2') {
                                remoteWriteWatch(p, true);
                            } else if (type == '3') {
                                remoteSetMemoryReadBreakPoint(p);
                            } else if (type == '4') {
                                remoteSetMemoryAccessBreakPoint(p);
                            } else {
                                remotePutPacket("");
                            }
                            break;
                        case 'z':
                            type = *p++;
                            if (type == '0') {
                                remoteClearBreakPoint(p);
                            } else if (type == '1') {
                                remoteClearBreakPoint(p);
                            } else if (type == '2') {
                                remoteWriteWatch(p, false);
                            } else if (type == '3') {
                                remoteClearMemoryReadBreakPoint(p);
                            } else if (type == '4') {
                                remoteClearMemoryAccessBreakPoint(p);
                            } else {
                                remotePutPacket("");
                            }
                            break;
                        default: {
                            fprintf(stderr, "Unknown packet %s\n", --p);
                            remotePutPacket("");
                        } break;
                        }
                    } else {
                        fprintf(stderr, "bad chksum csum=%x msg=%c%c\n", csum, buffer[i + 1], buffer[i + 2]);
                        ack = '-';
                        remoteSendFnc(&ack, 1);
                        fprintf(stderr, "SentNACK\n");
                    } //if
                    i += 3;
                } else {
                    fprintf(stderr, "didn't receive chksum i=%d res=%d\n", i, res);
                    i++;
                } //if
            } else {
                if (buffer[i] != '+') { //ingnore ACKs
                    fprintf(stderr, "not sure what to do with:%c i=%d res=%d\n", buffer[i], i, res);
                }
                i++;
            } //if
        } //while
    }
}

void remoteStubSignal(int sig, int number)
{
    (void)number; // unused params
    remoteSignal = sig;
    remoteResumed = false;
    remoteSendStatus();
    debugger = true;
}

void remoteCleanUp()
{
    if (remoteCleanUpFnc)
        remoteCleanUpFnc();
}

std::string HexToString(char* p)
{
    std::string hex(p);
    std::string cmd;
    std::stringstream ss;
    uint32_t offset = 0;
    while (offset < hex.length()) {
        unsigned int buffer = 0;
        ss.clear();
        ss << std::hex << hex.substr(offset, 2);
        ss >> std::hex >> buffer;
        cmd.push_back(static_cast<unsigned char>(buffer));
        offset += 2;
    }
    return cmd;
}

std::string StringToHex(std::string& cmd)
{
    std::stringstream ss;
    ss << std::hex;
    for (uint32_t i = 0; i < cmd.length(); ++i)
        ss << std::setw(2) << std::setfill('0') << (int)cmd.c_str()[i];
    return ss.str();
}

void monprintf(std::string line)
{
    std::string output = "O";
    line = StringToHex(line);
    output += line;

    if (output.length() <= 1000) {
        char dbgReply[1000];
        strcpy(dbgReply, output.c_str());
        remotePutPacket(dbgReply);
    }
}

#endif
