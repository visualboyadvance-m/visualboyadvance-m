#include "Flash.h"
#include "../Util.h"
#include "GBA.h"
#include "Globals.h"
#include "Sram.h"
#include <memory.h>
#include <stdio.h>
#include <string.h>

#define FLASH_READ_ARRAY 0
#define FLASH_CMD_1 1
#define FLASH_CMD_2 2
#define FLASH_AUTOSELECT 3
#define FLASH_CMD_3 4
#define FLASH_CMD_4 5
#define FLASH_CMD_5 6
#define FLASH_ERASE_COMPLETE 7
#define FLASH_PROGRAM 8
#define FLASH_SETBANK 9

uint8_t flashSaveMemory[SIZE_FLASH1M];

int flashState = FLASH_READ_ARRAY;
int flashReadState = FLASH_READ_ARRAY;
int flashSize = SIZE_FLASH512;
int flashDeviceID = 0x1b;
int flashManufacturerID = 0x32;
int flashBank = 0;

void flashInit()
{
    memset(flashSaveMemory, 0xff, sizeof(flashSaveMemory));
}

void flashReset()
{
    flashState = FLASH_READ_ARRAY;
    flashReadState = FLASH_READ_ARRAY;
    flashBank = 0;
}

void flashSetSize(int size)
{
    //  log("Setting flash size to %d\n", size);
    if (size == SIZE_FLASH512) {
        flashDeviceID = 0x1b;
        flashManufacturerID = 0x32;
    } else {
        flashDeviceID = 0x13; //0x09;
        flashManufacturerID = 0x62; //0xc2;
    }
    // Added to make 64k saves compatible with 128k ones
    // (allow wrongfuly set 64k saves to work for Pokemon games)
    if ((size == SIZE_FLASH1M) && (flashSize == SIZE_FLASH512))
        memcpy((uint8_t*)(flashSaveMemory + SIZE_FLASH512), (uint8_t*)(flashSaveMemory), SIZE_FLASH512);
    flashSize = size;
}

uint8_t flashRead(uint32_t address)
{
    //  log("Reading %08x from %08x\n", address, reg[15].I);
    //  log("Current read state is %d\n", flashReadState);
    address &= 0xFFFF;

    switch (flashReadState) {
    case FLASH_READ_ARRAY:
        return flashSaveMemory[(flashBank << 16) + address];
    case FLASH_AUTOSELECT:
        switch (address & 0xFF) {
        case 0:
            // manufacturer ID
            return flashManufacturerID;
        case 1:
            // device ID
            return flashDeviceID;
        }
        break;
    case FLASH_ERASE_COMPLETE:
        flashState = FLASH_READ_ARRAY;
        flashReadState = FLASH_READ_ARRAY;
        return 0xFF;
    };
    return 0;
}

void flashSaveDecide(uint32_t address, uint8_t byte)
{
    if (saveType == GBA_SAVE_EEPROM)
        return;

    if (cpuSramEnabled && cpuFlashEnabled) {
        if (address == 0x0e005555) {
            saveType = GBA_SAVE_FLASH;
            cpuSramEnabled = false;
            cpuSaveGameFunc = flashWrite;
        } else {
            saveType = GBA_SAVE_SRAM;
            cpuFlashEnabled = false;
            cpuSaveGameFunc = sramWrite;
        }

        log("%s emulation is enabled by writing to:  $%08x : %02x\n",
            cpuSramEnabled ? "SRAM" : "FLASH", address, byte);
    }

    (*cpuSaveGameFunc)(address, byte);
}

void flashDelayedWrite(uint32_t address, uint8_t byte)
{
    saveType = GBA_SAVE_FLASH;
    cpuSaveGameFunc = flashWrite;
    flashWrite(address, byte);
}

void flashWrite(uint32_t address, uint8_t byte)
{
    //  log("Writing %02x at %08x\n", byte, address);
    //  log("Current state is %d\n", flashState);
    address &= 0xFFFF;
    switch (flashState) {
    case FLASH_READ_ARRAY:
        if (address == 0x5555 && byte == 0xAA)
            flashState = FLASH_CMD_1;
        break;
    case FLASH_CMD_1:
        if (address == 0x2AAA && byte == 0x55)
            flashState = FLASH_CMD_2;
        else
            flashState = FLASH_READ_ARRAY;
        break;
    case FLASH_CMD_2:
        if (address == 0x5555) {
            if (byte == 0x90) {
                flashState = FLASH_AUTOSELECT;
                flashReadState = FLASH_AUTOSELECT;
            } else if (byte == 0x80) {
                flashState = FLASH_CMD_3;
            } else if (byte == 0xF0) {
                flashState = FLASH_READ_ARRAY;
                flashReadState = FLASH_READ_ARRAY;
            } else if (byte == 0xA0) {
                flashState = FLASH_PROGRAM;
            } else if (byte == 0xB0 && flashSize == SIZE_FLASH1M) {
                flashState = FLASH_SETBANK;
            } else {
                flashState = FLASH_READ_ARRAY;
                flashReadState = FLASH_READ_ARRAY;
            }
        } else {
            flashState = FLASH_READ_ARRAY;
            flashReadState = FLASH_READ_ARRAY;
        }
        break;
    case FLASH_CMD_3:
        if (address == 0x5555 && byte == 0xAA) {
            flashState = FLASH_CMD_4;
        } else {
            flashState = FLASH_READ_ARRAY;
            flashReadState = FLASH_READ_ARRAY;
        }
        break;
    case FLASH_CMD_4:
        if (address == 0x2AAA && byte == 0x55) {
            flashState = FLASH_CMD_5;
        } else {
            flashState = FLASH_READ_ARRAY;
            flashReadState = FLASH_READ_ARRAY;
        }
        break;
    case FLASH_CMD_5:
        if (byte == 0x30) {
            // SECTOR ERASE
            memset(&flashSaveMemory[(flashBank << 16) + (address & 0xF000)],
                0xff,
                0x1000);
            systemSaveUpdateCounter = SYSTEM_SAVE_UPDATED;
            flashReadState = FLASH_ERASE_COMPLETE;
        } else if (byte == 0x10) {
            // CHIP ERASE
            memset(flashSaveMemory, 0xff, flashSize);
            systemSaveUpdateCounter = SYSTEM_SAVE_UPDATED;
            flashReadState = FLASH_ERASE_COMPLETE;
        } else {
            flashState = FLASH_READ_ARRAY;
            flashReadState = FLASH_READ_ARRAY;
        }
        break;
    case FLASH_AUTOSELECT:
        if (byte == 0xF0) {
            flashState = FLASH_READ_ARRAY;
            flashReadState = FLASH_READ_ARRAY;
        } else if (address == 0x5555 && byte == 0xAA)
            flashState = FLASH_CMD_1;
        else {
            flashState = FLASH_READ_ARRAY;
            flashReadState = FLASH_READ_ARRAY;
        }
        break;
    case FLASH_PROGRAM:
        flashSaveMemory[(flashBank << 16) + address] = byte;
        systemSaveUpdateCounter = SYSTEM_SAVE_UPDATED;
        flashState = FLASH_READ_ARRAY;
        flashReadState = FLASH_READ_ARRAY;
        break;
    case FLASH_SETBANK:
        if (address == 0) {
            flashBank = (byte & 1);
        }
        flashState = FLASH_READ_ARRAY;
        flashReadState = FLASH_READ_ARRAY;
        break;
    }
}

static variable_desc flashSaveData3[] = {
    { &flashState, sizeof(int) },
    { &flashReadState, sizeof(int) },
    { &flashSize, sizeof(int) },
    { &flashBank, sizeof(int) },
    { &flashSaveMemory[0], SIZE_FLASH1M },
    { NULL, 0 }
};

#ifdef __LIBRETRO__
void flashSaveGame(uint8_t*& data)
{
    utilWriteDataMem(data, flashSaveData3);
}

void flashReadGame(const uint8_t*& data)
{
    utilReadDataMem(data, flashSaveData3);
}

#else // !__LIBRETRO__
static variable_desc flashSaveData[] = {
    { &flashState, sizeof(int) },
    { &flashReadState, sizeof(int) },
    { &flashSaveMemory[0], SIZE_FLASH512 },
    { NULL, 0 }
};

static variable_desc flashSaveData2[] = {
    { &flashState, sizeof(int) },
    { &flashReadState, sizeof(int) },
    { &flashSize, sizeof(int) },
    { &flashSaveMemory[0], SIZE_FLASH1M },
    { NULL, 0 }
};

void flashSaveGame(gzFile gzFile)
{
    utilWriteData(gzFile, flashSaveData3);
}

void flashReadGame(gzFile gzFile, int version)
{
    if (version < SAVE_GAME_VERSION_5)
        utilReadData(gzFile, flashSaveData);
    else if (version < SAVE_GAME_VERSION_7) {
        utilReadData(gzFile, flashSaveData2);
        flashBank = 0;
        flashSetSize(flashSize);
    } else {
        utilReadData(gzFile, flashSaveData3);
    }
}

void flashReadGameSkip(gzFile gzFile, int version)
{
    // skip the flash data in a save game
    if (version < SAVE_GAME_VERSION_5)
        utilReadDataSkip(gzFile, flashSaveData);
    else if (version < SAVE_GAME_VERSION_7) {
        utilReadDataSkip(gzFile, flashSaveData2);
    } else {
        utilReadDataSkip(gzFile, flashSaveData3);
    }
}
#endif
