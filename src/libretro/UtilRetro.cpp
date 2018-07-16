#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

extern int systemColorDepth;
extern int systemRedShift;
extern int systemGreenShift;
extern int systemBlueShift;

extern uint16_t systemColorMap16[0x10000];
extern uint32_t systemColorMap32[0x10000];

const char gb_image_header[] =
{
   static_cast<const char>
   (
      0xce, 0xed, 0x66, 0x66, 0xcc, 0x0d, 0x00, 0x0b, 0x03, 0x73, 0x00,
      0x83, 0x00, 0x0c, 0x00, 0x0d, 0x00, 0x08, 0x11, 0x1f, 0x88, 0x89,
      0x00, 0x0e, 0xdc, 0xcc, 0x6e, 0xe6, 0xdd, 0xdd, 0xd9, 0x99, 0xbb,
      0xbb, 0x67, 0x63, 0x6e, 0x0e, 0xec, 0xcc, 0xdd, 0xdc, 0x99, 0x9f,
      0xbb, 0xb9, 0x33, 0x3e
   )
};

bool utilWritePNGFile(const char* fileName, int w, int h, uint8_t* pix)
{
    return false;
}

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

bool utilWriteBMPFile(const char* fileName, int w, int h, uint8_t* pix)
{
    return false;
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
/*
	FILE *fp;
	bool ret = false;
	char buffer[47];
	if (!file || !(fp = fopen (file, "r")))		//TODO more checks here (does file exist, is it a file, a symlink or a blockdevice)
		return ret;
	fseek (fp, 0, SEEK_END);
	if (ftell (fp) >= 0x8000) {			//afaik there can be no gb-rom smaller than this
		fseek (fp, 0x104, SEEK_SET);
		fread (buffer, sizeof (char), 47, fp);
		ret = !memcmp (buffer, gb_image_header, 47);
	}
	fclose (fp);
	return ret;
*/
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

// strip .gz or .z off end
void utilStripDoubleExtension(const char* file, char* buffer)
{
    if (buffer != file) // allows conversion in place
        strcpy(buffer, file);
}

static bool utilIsImage(const char* file)
{
    return utilIsGBAImage(file) || utilIsGBImage(file);
}

IMAGE_TYPE utilFindType(const char* file)
{
    //char buffer[2048];
    if (!utilIsImage(file)) // TODO: utilIsArchive() instead?
    {
        return IMAGE_UNKNOWN;
    }
    return utilIsGBAImage(file) ? IMAGE_GBA : IMAGE_GB;
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
	//char *buf = NULL;

	fp = fopen(file,"rb");
	if(!fp) return NULL;
	fseek(fp, 0, SEEK_END); //go to end
	size = ftell(fp); // get position at end (length)
	rewind(fp);

	uint8_t *image = data;
	if(image == NULL)
	{
		//allocate buffer memory if none was passed to the function
		image = (uint8_t *)malloc(utilGetSize(size));
		if(image == NULL)
		{
			systemMessage(MSG_OUT_OF_MEMORY, N_("Failed to allocate memory for %s"),
					"data");
			return NULL;
		}
	}

   fread(image, 1, size, fp); // read into buffer
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
    if (detectedSaveType == 0) {
        detectedSaveType = 5;
    }
    if (detectedSaveType == 4) {
        detectedSaveType = 3;
    }

    cpuSaveType = detectedSaveType;
    rtcEnabled = rtcFound_;
    flashSize = flashSize_;
}

void utilUpdateSystemColorMaps(bool lcd)
{
    switch (systemColorDepth) {
    case 16: {
        for (int i = 0; i < 0x10000; i++) {
            systemColorMap16[i] = ((i & 0x1f) << systemRedShift) | (((i & 0x3e0) >> 5) << systemGreenShift) | (((i & 0x7c00) >> 10) << systemBlueShift);
        }
    } break;
    case 24:
    case 32: {
        for (int i = 0; i < 0x10000; i++) {
            systemColorMap32[i] = ((i & 0x1f) << systemRedShift) | (((i & 0x3e0) >> 5) << systemGreenShift) | (((i & 0x7c00) >> 10) << systemBlueShift);
        }
    } break;
    }
}

// Check for existence of file.
bool utilFileExists(const char* filename)
{
    FILE* f = fopen(filename, "r");
    if (f == NULL) {
        return false;
    } else {
        fclose(f);
        return true;
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
