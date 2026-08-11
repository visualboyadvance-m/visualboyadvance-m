#include "core/gba/gbaEeprom.h"

#include <cstring>

#include "core/base/file_util.h"
#include "core/gba/gba.h"
#include "gbaEeprom.h"

extern int cpuDmaCount;

int eepromMode = EEPROM_IDLE;
int eepromByte = 0;
int eepromBits = 0;
int eepromAddress = 0;

uint8_t eepromData[SIZE_EEPROM_8K];

uint8_t eepromBuffer[16];
bool eepromInUse = false;
int eepromSize = SIZE_EEPROM_512;
uint32_t eepromMask = 0;

variable_desc eepromSaveData[] = {
    { &eepromMode, sizeof(int) },
    { &eepromByte, sizeof(int) },
    { &eepromBits, sizeof(int) },
    { &eepromAddress, sizeof(int) },
    { &eepromInUse, sizeof(bool) },
    { &eepromData[0], SIZE_EEPROM_512 },
    { &eepromBuffer[0], 16 },
    { NULL, 0 }
};

void eepromInit()
{
    eepromInUse = false;
    eepromSize = SIZE_EEPROM_512;
    memset(eepromData, 255, sizeof(eepromData));
}

void eepromReset()
{
    eepromMode = EEPROM_IDLE;
    eepromByte = 0;
    eepromBits = 0;
    eepromAddress = 0;
}

void eepromSetSize(int size) {
    eepromSize = size;
    eepromMask = (gbaGetRomSize() > (16 * 1024 * 1024)) ? 0x01FFFF00 : 0x01000000;
}

// Bring the EEPROM state machine back into the range its own protocol can
// produce after a save state has been loaded.
//
// eepromMode, eepromAddress, eepromByte and eepromBits are all raw ints off
// disk, and the first three end up as array indices:
//
//     eepromData[(eepromAddress << 3) + eepromByte]   (read)
//     eepromData[(eepromAddress << 3) + i]            (write)
//     eepromBuffer[eepromByte]                        (both)
//
// During emulation these are derived from incoming command bits with explicit
// masking (see the eepromAddress assignments in eepromWrite), so the values
// are structurally bounded. Loading a state skipped all of that, and because
// eepromMode is restored too, a crafted state can land directly in
// EEPROM_READDATA2 or EEPROM_WRITEDATA and hit the access on the first EEPROM
// operation after the load.
static void eepromSanitizeSaveState()
{
    if (eepromMode < EEPROM_IDLE || eepromMode > EEPROM_WRITEDATA)
        eepromMode = EEPROM_IDLE;

    // eepromData holds 1024 eight-byte words.
    eepromAddress &= (SIZE_EEPROM_8K >> 3) - 1;

    // Byte offset within the current word. The protocol takes it up to 8, not
    // 7: eepromWrite advances it on every eighth bit, so the final step of a
    // 64-bit transfer -- the one that commits eepromBuffer to eepromData --
    // runs with eepromByte == 8, and eepromBuffer[8] is written at 0x41.
    // Resetting that to 0 would corrupt an in-flight battery write across a
    // save and reload. eepromBuffer has 16 bytes, so 8 is in range there; the
    // eepromData index it also feeds is bounded at the point of use.
    if (eepromByte < 0 || eepromByte > 8)
        eepromByte = 0;

    // Bit counter. The state machine compares it against 0x40 and 0x41 for
    // termination, so a value past those would never finish a transfer, and
    // incrementing it from INT_MAX is undefined behaviour besides.
    if (eepromBits < 0 || eepromBits > 0x41)
        eepromBits = 0;
}

#ifdef __LIBRETRO__
void eepromSaveGame(uint8_t*& data)
{
    utilWriteDataMem(data, eepromSaveData);
    utilWriteIntMem(data, eepromSize);
    utilWriteMem(data, eepromData, SIZE_EEPROM_8K);
}

void eepromReadGame(const uint8_t*& data)
{
    utilReadDataMem(data, eepromSaveData);
    eepromSize = utilReadIntMem(data);
    utilReadMem(eepromData, data, SIZE_EEPROM_8K);
    eepromSanitizeSaveState();
}

#else // !__LIBRETRO__

void eepromSaveGame(gzFile gzFile)
{
    utilWriteData(gzFile, eepromSaveData);
    utilWriteInt(gzFile, eepromSize);
    utilGzWrite(gzFile, eepromData, SIZE_EEPROM_8K);
}

void eepromReadGame(gzFile gzFile, int version)
{
    utilReadData(gzFile, eepromSaveData);
    if (version >= SAVE_GAME_VERSION_3) {
        eepromSize = utilReadInt(gzFile);
        utilGzRead(gzFile, eepromData, SIZE_EEPROM_8K);
    } else {
        // prior to 0.7.1, only 4K EEPROM was supported
        eepromSize = SIZE_EEPROM_512;
    }

    eepromSanitizeSaveState();
}

void eepromReadGameSkip(gzFile gzFile, int version)
{
    // skip the eeprom data in a save game
    utilReadDataSkip(gzFile, eepromSaveData);
    if (version >= SAVE_GAME_VERSION_3) {
        utilGzSeek(gzFile, sizeof(int), SEEK_CUR);
        utilGzSeek(gzFile, SIZE_EEPROM_8K, SEEK_CUR);
    }
}
#endif

int eepromRead(uint32_t /* address */)
{
    switch (eepromMode) {
    case EEPROM_IDLE:
    case EEPROM_READADDRESS:
    case EEPROM_WRITEDATA:
        return 1;
    case EEPROM_READDATA: {
        eepromBits++;
        if (eepromBits == 4) {
            eepromMode = EEPROM_READDATA2;
            eepromBits = 0;
            eepromByte = 0;
        }
        return 0;
    }
    case EEPROM_READDATA2: {
        int address = eepromAddress << 3;
        int mask = 1 << (7 - (eepromBits & 7));
        // eepromAddress is bounded to the buffer's 1024 words and eepromByte
        // to 8, so the sum reaches 8192 -- one past the end -- for the last
        // word. Wrap rather than read off the end; the state machine leaves
        // READDATA2 before a legitimate transfer ever gets there.
        int index = (address + eepromByte) & (SIZE_EEPROM_8K - 1);
        int data = (eepromData[index] & mask) ? 1 : 0;
        eepromBits++;
        if ((eepromBits & 7) == 0)
            eepromByte++;
        if (eepromBits == 0x40)
            eepromMode = EEPROM_IDLE;
        return data;
    }
    default:
        break;
    }
    return 0;
}

void eepromWrite(uint32_t /* address */, uint8_t value)
{
    if (cpuDmaCount == 0)
        return;
    int bit = value & 1;
    switch (eepromMode) {
    case EEPROM_IDLE:
        eepromByte = 0;
        eepromBits = 1;
        eepromBuffer[eepromByte] = (uint8_t)bit;
        eepromMode = EEPROM_READADDRESS;
        break;
    case EEPROM_READADDRESS:
        eepromBuffer[eepromByte] <<= 1;
        eepromBuffer[eepromByte] |= bit;
        eepromBits++;
        if ((eepromBits & 7) == 0) {
            eepromByte++;
        }
        if (cpuDmaCount == 0x11 || cpuDmaCount == 0x51) {
            if (eepromBits == 0x11) {
                eepromSetSize(SIZE_EEPROM_8K);
                eepromInUse = true;
                eepromAddress = ((eepromBuffer[0] & 0x3F) << 8) | ((eepromBuffer[1] & 0xFF));
                if (!(eepromBuffer[0] & 0x40)) {
                    eepromBuffer[0] = (uint8_t)bit;
                    eepromBits = 1;
                    eepromByte = 0;
                    eepromMode = EEPROM_WRITEDATA;
                } else {
                    eepromMode = EEPROM_READDATA;
                    eepromByte = 0;
                    eepromBits = 0;
                }
            }
        } else {
            if (eepromBits == 9) {
                eepromSetSize(SIZE_EEPROM_512);
                eepromInUse = true;
                eepromAddress = (eepromBuffer[0] & 0x3F);
                if (!(eepromBuffer[0] & 0x40)) {
                    eepromBuffer[0] = (uint8_t)bit;
                    eepromBits = 1;
                    eepromByte = 0;
                    eepromMode = EEPROM_WRITEDATA;
                } else {
                    eepromMode = EEPROM_READDATA;
                    eepromByte = 0;
                    eepromBits = 0;
                }
            }
        }
        break;
    case EEPROM_READDATA:
    case EEPROM_READDATA2:
        // should we reset here?
        eepromMode = EEPROM_IDLE;
        break;
    case EEPROM_WRITEDATA:
        eepromBuffer[eepromByte] <<= 1;
        eepromBuffer[eepromByte] |= bit;
        eepromBits++;
        if ((eepromBits & 7) == 0) {
            eepromByte++;
        }
        if (eepromBits == 0x40) {
            eepromInUse = true;
            // write data;
            for (int i = 0; i < 8; i++) {
                eepromData[(eepromAddress << 3) + i] = eepromBuffer[i];
            }
            systemSaveUpdateCounter = SYSTEM_SAVE_UPDATED;
        } else if (eepromBits == 0x41) {
            eepromMode = EEPROM_IDLE;
            eepromByte = 0;
            eepromBits = 0;
        }
        break;
    }
}
