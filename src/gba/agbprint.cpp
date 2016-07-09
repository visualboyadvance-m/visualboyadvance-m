#include <stdio.h>
#include <string.h>

#include "../System.h"
#include "../common/Port.h"
#include "GBA.h"
#include "Globals.h"

#define debuggerWriteHalfWord(addr, value) \
    WRITE16LE((uint16_t*)&map[(addr) >> 24].address[(addr)&map[(addr) >> 24].mask], (value))

#define debuggerReadHalfWord(addr) \
    READ16LE(((uint16_t*)&map[(addr) >> 24].address[(addr)&map[(addr) >> 24].mask]))

static bool agbPrintEnabled = false;
static bool agbPrintProtect = false;

bool agbPrintWrite(uint32_t address, uint16_t value)
{
    if (agbPrintEnabled) {
        if (address == 0x9fe2ffe) { // protect
            agbPrintProtect = (value != 0);
            debuggerWriteHalfWord(address, value);
            return true;
        } else {
            if (agbPrintProtect && ((address >= 0x9fe20f8 && address <= 0x9fe20ff) // control structure
                                       || (address >= 0x8fd0000 && address <= 0x8fdffff)
                                       || (address >= 0x9fd0000 && address <= 0x9fdffff))) {
                debuggerWriteHalfWord(address, value);
                return true;
            }
        }
    }
    return false;
}

void agbPrintReset()
{
    agbPrintProtect = false;
}

void agbPrintEnable(bool enable)
{
    agbPrintEnabled = enable;
}

bool agbPrintIsEnabled()
{
    return agbPrintEnabled;
}

void agbPrintFlush()
{
    uint16_t get = debuggerReadHalfWord(0x9fe20fc);
    uint16_t put = debuggerReadHalfWord(0x9fe20fe);

    uint32_t address = (debuggerReadHalfWord(0x9fe20fa) << 16);
    if (address != 0xfd0000 && address != 0x1fd0000) {
        dbgOutput("Did you forget to call AGBPrintInit?\n", 0);
        // get rid of the text otherwise we will continue to be called
        debuggerWriteHalfWord(0x9fe20fc, put);
        return;
    }

    uint8_t* data = &rom[address];

    while (get != put) {
        char c = data[get++];
        char s[2];
        s[0] = c;
        s[1] = 0;

        if (systemVerbose & VERBOSE_AGBPRINT)
            dbgOutput(s, 0);
        if (c == '\n')
            break;
    }
    debuggerWriteHalfWord(0x9fe20fc, get);
}
