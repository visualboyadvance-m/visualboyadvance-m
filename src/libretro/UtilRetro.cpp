#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libretro.h>

#include "NLS.h"
#include "System.h"
#include "Util.h"
#include "common/Port.h"
#include "common/ConfigManager.h"
#include "gba/Flash.h"
#include "gba/GBA.h"
#include "gba/Globals.h"
#include "gba/RTC.h"

#include "gb/gbGlobals.h"
#include "gba/gbafilter.h"

#ifndef _MSC_VER
#include <strings.h>
#define _stricmp strcasecmp
#endif // ! _MSC_VER

// Because Configmanager was introduced, this has to be done.
int  rtcEnabled          = 0;
int  cpuDisableSfx       = 0;
int  skipBios            = 0;
int  saveType            = 0;
int  cpuSaveType         = 0;
int  skipSaveGameBattery = 0;
int  skipSaveGameCheats  = 0;
int  useBios             = 0;
int  cheatsEnabled       = 0;
int  layerSettings       = 0xff00;
int  layerEnable         = 0xff00;
bool speedup             = false;
bool parseDebug          = false;
bool speedHack           = false;
bool mirroringEnable     = false;
bool cpuIsMultiBoot      = false;

const char* loadDotCodeFile;
const char* saveDotCodeFile;

void utilPutDword(uint8_t* p, uint32_t value)
{
    *p++ = value & 255;
    *p++ = (value >> 8) & 255;
    *p++ = (value >> 16) & 255;
    *p = (value >> 24) & 255;
}

void utilPutWord(uint8_t* p, uint16_t value)
{
    *p++ = value & 255;
    *p = (value >> 8) & 255;
}

extern bool cpuIsMultiBoot;

bool utilIsGBAImage(const char* file)
{
    cpuIsMultiBoot = false;
    if (strlen(file) > 4) {
        const char* p = strrchr(file, '.');

        if (p != NULL) {
            if ((_stricmp(p, ".agb") == 0) || (_stricmp(p, ".gba") == 0) || (_stricmp(p, ".bin") == 0) || (_stricmp(p, ".elf") == 0))
                return true;
            if (_stricmp(p, ".mb") == 0) {
                cpuIsMultiBoot = true;
                return true;
            }
        }
    }

    return false;
}

bool utilIsGBImage(const char* file)
{
    if (strlen(file) > 4) {
        const char *p = strrchr(file, '.');

        if (p != NULL) {
            if ((_stricmp(p, ".dmg") == 0) || (_stricmp(p, ".gb") == 0)
            || (_stricmp(p, ".gbc") == 0) || (_stricmp(p, ".cgb") == 0)
            || (_stricmp(p, ".sgb") == 0)) {
                return true;
            }
        }
    }

    return false;
}

IMAGE_TYPE utilFindType(const char* file)
{
    if (utilIsGBAImage(file))
        return IMAGE_GBA;

    if (utilIsGBImage(file))
        return IMAGE_GB;

    return IMAGE_UNKNOWN;
}

static int utilGetSize(int size)
{
    int res = 1;
    while(res < size)
        res <<= 1;
    return res;
}

uint8_t *utilLoad(const char *file, bool (*accept)(const char *), uint8_t *data, int &size)
{
    FILE *fp = NULL;

    fp = fopen(file,"rb");
    if (!fp)
    {
        log("Failed to open file %s", file);
        return NULL;
    }
    fseek(fp, 0, SEEK_END); //go to end

    size = ftell(fp); // get position at end (length)
    rewind(fp);

    uint8_t *image = data;
    if(image == NULL)
    {
        image = (uint8_t *)malloc(utilGetSize(size));
        if(image == NULL)
        {
            log("Failed to allocate memory for %s", file);
            return NULL;
        }
    }

    if (fread(image, 1, size, fp) != (size_t)size) {
        log("Failed to read from %s", file);
        fclose(fp);
        return NULL;
    }

    fclose(fp);
    return image;
}

void utilGBAFindSave(const int size)
{
    bool rtcFound_ = false;
    int detectedSaveType = 0;
    int flashSize_ = 0x10000;
    uint32_t *p = (uint32_t *)&rom[0];
    uint32_t *end = (uint32_t *)(&rom[0] + size);

    while (p < end) {
        uint32_t d = READ32LE(p);

        if (d == 0x52504545) {
            if (memcmp(p, "EEPROM_", 7) == 0) {
                if (detectedSaveType == 0 || detectedSaveType == 4) {
                    detectedSaveType = 1;
                }
            }
        } else if (d == 0x4D415253) {
            if (memcmp(p, "SRAM_", 5) == 0) {
                if (detectedSaveType == 0 || detectedSaveType == 1
                    || detectedSaveType == 4) {
                    detectedSaveType = 2;
                    flashSize_ = 0x8000;
                }
            }
        } else if (d == 0x53414C46) {
            if (memcmp(p, "FLASH1M_", 8) == 0) {
                if (detectedSaveType == 0) {
                    detectedSaveType = 3;
                    flashSize_ = 0x20000;
                }
            } else if (memcmp(p, "FLASH512_", 9) == 0) {
                if (detectedSaveType == 0) {
                    detectedSaveType = 3;
                    flashSize_ = 0x10000;
                }
            } else if (memcmp(p, "FLASH", 5) == 0) {
                if (detectedSaveType == 0) {
                    detectedSaveType = 4;
                    flashSize_ = 0x10000;
                }
            }
        } else if (d == 0x52494953) {
            if (memcmp(p, "SIIRTC_V", 8) == 0) {
                rtcFound_ = true;
            }
        }
        p++;
    }
    // if no matches found, then set it to NONE
    if (detectedSaveType == 0)
        detectedSaveType = 5;
    if (detectedSaveType == 4)
        detectedSaveType = 3;

    cpuSaveType = detectedSaveType;
    rtcEnabled = rtcFound_;
    flashSize = flashSize_;
}

void utilUpdateSystemColorMaps(bool lcd)
{
    int i = 0;

    switch (systemColorDepth) {
        case 16:
            for (i = 0; i < 0x10000; i++) {
                systemColorMap16[i] = ((i & 0x1f) << systemRedShift) |
                    (((i & 0x3e0) >> 5) << systemGreenShift) |
                    (((i & 0x7c00) >> 10) << systemBlueShift);
                }
                if (lcd)
                    gbafilter_pal(systemColorMap16, 0x10000);
            break;
        case 24:
        case 32:
            for (i = 0; i < 0x10000; i++) {
                systemColorMap32[i] = ((i & 0x1f) << systemRedShift) |
                    (((i & 0x3e0) >> 5) << systemGreenShift) |
                    (((i & 0x7c00) >> 10) << systemBlueShift);
                }
                if (lcd)
                    gbafilter_pal32(systemColorMap32, 0x10000);
            break;
    }
}

// Not endian safe, but VBA itself doesn't seem to care, so hey <_<
void utilWriteIntMem(uint8_t*& data, int val)
{
    memcpy(data, &val, sizeof(int));
    data += sizeof(int);
}

void utilWriteMem(uint8_t*& data, const void* in_data, unsigned size)
{
    memcpy(data, in_data, size);
    data += size;
}

void utilWriteDataMem(uint8_t*& data, variable_desc* desc)
{
    while (desc->address) {
        utilWriteMem(data, desc->address, desc->size);
        desc++;
    }
}

int utilReadIntMem(const uint8_t*& data)
{
    int res;
    memcpy(&res, data, sizeof(int));
    data += sizeof(int);
    return res;
}

void utilReadMem(void* buf, const uint8_t*& data, unsigned size)
{
    memcpy(buf, data, size);
    data += size;
}

void utilReadDataMem(const uint8_t*& data, variable_desc* desc)
{
    while (desc->address) {
        utilReadMem(desc->address, data, desc->size);
        desc++;
    }
}
