#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "zconf.h"
#ifndef __LIBRETRO__
#include <zlib.h>
#endif

#include "Patch.h"

#if defined(__FreeBSD__) || defined(__NetBSD__)
#include <sys/param.h>
#include "BSD.h"
#endif

#ifndef __LIBRETRO__

#ifdef __GNUC__
#if defined(__APPLE__) || defined(BSD) || defined(__NetBSD__)
typedef off_t __off64_t; /* off_t is 64 bits on BSD. */
#define fseeko64 fseeko
#define ftello64 ftello
#else
typedef off64_t __off64_t;
#endif /* __APPLE__ || BSD */
#endif /* __GNUC__ */

#ifndef _MSC_VER
#define _stricmp strcasecmp
#endif // ! _MSC_VER

#ifdef _MSC_VER
#define fseeko64 _fseeki64
#define ftello64 _ftelli64
typedef __int64 __off64_t;
#endif

static int readInt2(FILE* f)
{
    int res = 0;
    int c = fgetc(f);
    if (c == EOF)
        return -1;
    res = c;
    c = fgetc(f);
    if (c == EOF)
        return -1;
    return c + (res << 8);
}

static int readInt3(FILE* f)
{
    int res = 0;
    int c = fgetc(f);
    if (c == EOF)
        return -1;
    res = c;
    c = fgetc(f);
    if (c == EOF)
        return -1;
    res = c + (res << 8);
    c = fgetc(f);
    if (c == EOF)
        return -1;
    return c + (res << 8);
}

static int64_t readInt4(FILE* f)
{
    int64_t tmp, res = 0;
    int c;

    for (int i = 0; i < 4; i++) {
        c = fgetc(f);
        if (c == EOF)
            return -1;
        tmp = c;
        res = res + (tmp << (i * 8));
    }

    return res;
}

static int64_t readInt8(FILE* f)
{
    int64_t tmp, res = 0;
    int c;

    for (int i = 0; i < 8; i++) {
        c = fgetc(f);
        if (c == EOF)
            return -1;
        tmp = c;
        res = res + (tmp << (i * 8));
    }

    return res;
}

static int64_t readVarPtr(FILE* f)
{
    int64_t offset = 0, shift = 1;
    for (;;) {
        int c = fgetc(f);
        if (c == EOF)
            return 0;
        offset += (c & 0x7F) * shift;
        if (c & 0x80)
            break;
        shift <<= 7;
        offset += shift;
    }
    return offset;
}

static uint32_t readSignVarPtr(FILE* f)
{
    int64_t offset = readVarPtr(f);
    bool sign =  offset & 1;

    offset = offset >> 1;
    if (sign) {
        offset = -offset;
    }
    return (uint32_t)(offset);
}

#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

static uLong computePatchCRC(FILE* f, unsigned int size)
{
    Bytef buf[4096];
    long readed;

    uLong crc = crc32(0L, Z_NULL, 0);
    do {
        readed = (long)(fread(buf, 1, MIN(size, sizeof(buf)), f));
        crc = crc32(crc, buf, readed);
        size -= readed;
    } while (readed > 0);
    return crc;
}

static bool patchApplyIPS(const char* patchname, uint8_t** r, int* s)
{
    // from the IPS spec at http://zerosoft.zophar.net/ips.htm
    FILE* f = utilOpenFile(patchname, "rb");
    if (!f)
        return false;

    bool result = false;

    uint8_t* rom = *r;
    int size = *s;
    if (fgetc(f) == 'P' && fgetc(f) == 'A' && fgetc(f) == 'T' && fgetc(f) == 'C' && fgetc(f) == 'H') {
        int b;
        int offset;
        int len;

        result = true;

        for (;;) {
            // read offset
            offset = readInt3(f);
            // if offset == EOF, end of patch
            if (offset == 0x454f46 || offset == -1)
                break;
            // read length
            len = readInt2(f);
            if (!len) {
                // len == 0, RLE block
                len = readInt2(f);
                // byte to fill
                int c = fgetc(f);
                if (c == -1)
                    break;
                b = (uint8_t)c;
            } else
                b = -1;
            // check if we need to reallocate our ROM
            if ((offset + len) >= size) {
                size *= 2;
                rom = (uint8_t*)realloc(rom, size);
                *r = rom;
                *s = size;
            }
            if (b == -1) {
                // normal block, just read the data
                if (fread(&rom[offset], 1, len, f) != (size_t)len)
                    break;
            } else {
                // fill the region with the given byte
                while (len--) {
                    rom[offset++] = (uint8_t)(b&0xff);
                }
            }
        }
    }
    // close the file
    fclose(f);

    return result;
}

static bool patchApplyUPS(const char* patchname, uint8_t** rom, int* size)
{
    int64_t srcCRC, dstCRC, patchCRC;

    FILE* f = utilOpenFile(patchname, "rb");
    if (!f)
        return false;

    fseeko64(f, 0, SEEK_END);
    __off64_t patchSize = ftello64(f);
    if (patchSize < 20) {
        fclose(f);
        return false;
    }

    fseeko64(f, 0, SEEK_SET);
    if (fgetc(f) != 'U' || fgetc(f) != 'P' || fgetc(f) != 'S' || fgetc(f) != '1') {
        fclose(f);
        return false;
    }

    fseeko64(f, -12, SEEK_END);
    srcCRC = readInt4(f);
    dstCRC = readInt4(f);
    patchCRC = readInt4(f);
    if (srcCRC == -1 || dstCRC == -1 || patchCRC == -1) {
        fclose(f);
        return false;
    }

    fseeko64(f, 0, SEEK_SET);
    uint32_t crc = computePatchCRC(f, (unsigned)(patchSize - 4));

    if (crc != patchCRC) {
        fclose(f);
        return false;
    }

    crc = crc32(0L, Z_NULL, 0);
    crc = crc32(crc, *rom, *size);

    fseeko64(f, 4, SEEK_SET);
    int64_t dataSize;
    int64_t srcSize = readVarPtr(f);
    int64_t dstSize = readVarPtr(f);

    if (crc == srcCRC) {
        if (srcSize != *size) {
            fclose(f);
            return false;
        }
        dataSize = dstSize;
    } else if (crc == dstCRC) {
        if (dstSize != *size) {
            fclose(f);
            return false;
        }
        dataSize = srcSize;
    } else {
        fclose(f);
        return false;
    }
    if (dataSize > *size) {
        *rom = (uint8_t*)realloc(*rom, dataSize);
        memset(*rom + *size, 0, dataSize - *size);
        *size = (int)(dataSize);
    }

    int64_t relative = 0;
    uint8_t* mem;
    while (ftello64(f) < patchSize - 12) {
        relative += readVarPtr(f);
        if (relative > dataSize)
            continue;
        mem = *rom + relative;
        for (int64_t i = relative; i < dataSize; i++) {
            int x = fgetc(f);
            relative++;
            if (!x)
                break;
            if (i < dataSize) {
                *mem++ ^= x;
            }
        }
    }

    fclose(f);
    return true;
}

static bool patchApplyBPS(const char* patchname, uint8_t** rom, int* size)
{
    int64_t srcCRC, dstCRC, patchCRC;

    FILE* f = utilOpenFile(patchname, "rb");
    if (!f)
        return false;

    fseeko64(f, 0, SEEK_END);
    __off64_t patchSize = ftello64(f);
    if (patchSize < 20) {
        fclose(f);
        return false;
    }

    fseeko64(f, 0, SEEK_SET);
    if (fgetc(f) != 'B' || fgetc(f) != 'P' || fgetc(f) != 'S' || fgetc(f) != '1') {
        fclose(f);
        return false;
    }

    fseeko64(f, -12, SEEK_END);
    srcCRC = readInt4(f);
    dstCRC = readInt4(f);
    patchCRC = readInt4(f);
    if (srcCRC == -1 || dstCRC == -1 || patchCRC == -1) {
        fclose(f);
        return false;
    }

    fseeko64(f, 0, SEEK_SET);
    uint32_t crc = computePatchCRC(f, (unsigned)(patchSize - 4));

    if (crc != patchCRC) {
        fclose(f);
        return false;
    }

    crc = crc32(0L, Z_NULL, 0);
    crc = crc32(crc, *rom, *size);

    fseeko64(f, 4, SEEK_SET);
    int dataSize;
    const int64_t srcSize = readVarPtr(f);
    const int64_t dstSize = readVarPtr(f);
    const int64_t mtdSize = readVarPtr(f);
    fseeko64(f, mtdSize, SEEK_CUR);

    if (crc == srcCRC) {
        if (srcSize != *size) {
            fclose(f);
            return false;
        }
        dataSize = (int)(dstSize);
    } else if (crc == dstCRC) {
        if (dstSize != *size) {
            fclose(f);
            return false;
        }
        dataSize = (int)(srcSize);
    } else {
        fclose(f);
        return false;
    }

    uint8_t* new_rom = (uint8_t*)calloc(1, dataSize);

    int64_t length = 0;
    uint8_t action = 0;
    uint32_t outputOffset = 0, sourceRelativeOffset = 0, targetRelativeOffset = 0;

    while (ftello64(f) < patchSize - 12) {
        length = readVarPtr(f);
        action = length & 3 ;
        length = (length>>2) + 1;
        switch(action){
        case 0: // sourceRead
            while(length--) {
                new_rom[outputOffset] = rom[0][outputOffset];
                outputOffset++;
            }
            break;
        case 1: // patchRead
            while(length--) {
                new_rom[outputOffset++] = (uint8_t)(fgetc(f) & 0xff);
            }
            break;
        case 2: // sourceCopy
            sourceRelativeOffset += readSignVarPtr(f);
            while(length--) {
                new_rom[outputOffset++] = rom[0][sourceRelativeOffset++];
            }
            break;
        case 3: // targetCopy
            targetRelativeOffset += readSignVarPtr(f);
            while(length--) { // yes, copy from alredy patched rom, and only 1 byte at time (pseudo-rle)
                new_rom[outputOffset++] = new_rom[targetRelativeOffset++];
            }
            break;
        }
    }

    crc = crc32(0L, Z_NULL, 0);
    crc = crc32(crc, new_rom, dataSize);

    if(crc == dstCRC)
    {
        if (dataSize > *size) {
            *rom = (uint8_t*)realloc(*rom, dataSize);
        }
        memcpy(*rom, new_rom, dataSize);
        *size = dataSize;
        free(new_rom);
    }

    fclose(f);
    return true;
}

static int ppfVersion(FILE* f)
{
    fseeko64(f, 0, SEEK_SET);
    if (fgetc(f) != 'P' || fgetc(f) != 'P' || fgetc(f) != 'F') //-V501
        return 0;
    switch (fgetc(f)) {
    case '1':
        return 1;
    case '2':
        return 2;
    case '3':
        return 3;
    default:
        return 0;
    }
}

static int ppfFileIdLen(FILE* f, int version)
{
    if (version == 2) {
        fseeko64(f, -8, SEEK_END);
    } else {
        fseeko64(f, -6, SEEK_END);
    }

    if (fgetc(f) != '.' || fgetc(f) != 'D' || fgetc(f) != 'I' || fgetc(f) != 'Z')
        return 0;

    return (version == 2) ? int(readInt4(f)) : readInt2(f);
}

static bool patchApplyPPF1(FILE* f, uint8_t** rom, int* size)
{
    fseek(f, 0, SEEK_END);
    int count = ftell(f);
    if (count < 56)
        return false;
    count -= 56;

    fseek(f, 56, SEEK_SET);

    uint8_t* mem = *rom;

    while (count > 0) {
        int64_t offset_read = readInt4(f);
        if (offset_read == -1)
            break;
        int offset = (int)(offset_read);
        int len = fgetc(f);
        if (len == EOF)
            break;
        if (offset + len > *size)
            break;
        if (fread(&mem[offset], 1, len, f) != (size_t)len)
            break;
        count -= 4 + 1 + len;
    }

    return (count == 0);
}

static bool patchApplyPPF2(FILE* f, uint8_t** rom, int* size)
{
    fseek(f, 0, SEEK_END);
    int count = ftell(f);
    if (count < 56 + 4 + 1024)
        return false;
    count -= 56 + 4 + 1024;

    fseek(f, 56, SEEK_SET);

    int64_t datalen_read = readInt4(f);
    if (datalen_read == -1)
        return false;

    int datalen = (int)(datalen_read);
    if (datalen != *size)
        return false;

    uint8_t* mem = *rom;

    uint8_t block[1024];
    if (fread(&block, 1, 1024, f) == 0 ||
            memcmp(&mem[0x9320], &block, 1024) != 0)
        return false;

    int idlen = ppfFileIdLen(f, 2);
    if (idlen > 0)
        count -= 16 + 16 + idlen;

    fseek(f, 56 + 4 + 1024, SEEK_SET);

    while (count > 0) {
        int64_t offset_read = readInt4(f);
        if (offset_read == -1)
            break;
        int offset = (int)(offset_read);
        int len = fgetc(f);
        if (len == EOF)
            break;
        if (offset + len > *size)
            break;
        if (fread(&mem[offset], 1, len, f) != (size_t)len)
            break;
        count -= 4 + 1 + len;
    }

    return (count == 0);
}

static bool patchApplyPPF3(FILE* f, uint8_t** rom, int* size)
{
    fseek(f, 0, SEEK_END);
    int count = ftell(f);
    if (count < 56 + 4 + 1024)
        return false;
    count -= 56 + 4;

    fseek(f, 56, SEEK_SET);

    int imagetype = fgetc(f);
    int blockcheck = fgetc(f);
    int undo = fgetc(f);
    fgetc(f);

    uint8_t* mem = *rom;

    if (blockcheck) {
        uint8_t block[1024];
        if (fread(&block, 1, 1024, f) == 0 ||
                memcmp(&mem[(imagetype == 0) ? 0x9320 : 0x80A0], &block, 1024) != 0)
            return false;
        count -= 1024;
    }

    int idlen = ppfFileIdLen(f, 2);
    if (idlen > 0)
        count -= 16 + 16 + idlen;

    fseek(f, 56 + 4 + (blockcheck ? 1024 : 0), SEEK_SET);

    while (count > 0) {
        __off64_t offset = readInt8(f);
        if (offset == -1)
            break;
        int len = fgetc(f);
        if (len == EOF)
            break;
        if (offset + len > *size)
            break;
        if (fread(&mem[offset], 1, len, f) != (size_t)len)
            break;
        if (undo)
            fseeko64(f, len, SEEK_CUR);
        count -= 8 + 1 + len;
        if (undo)
            count -= len;
    }

    return (count == 0);
}

static bool patchApplyPPF(const char* patchname, uint8_t** rom, int* size)
{
    FILE* f = utilOpenFile(patchname, "rb");
    if (!f)
        return false;

    bool res = false;

    int version = ppfVersion(f);
    switch (version) {
    case 1:
        res = patchApplyPPF1(f, rom, size);
        break;
    case 2:
        res = patchApplyPPF2(f, rom, size);
        break;
    case 3:
        res = patchApplyPPF3(f, rom, size);
        break;
    }

    fclose(f);
    return res;
}

#endif

bool applyPatch(const char* patchname, uint8_t** rom, int* size)
{
#ifndef __LIBRETRO__
    if (strlen(patchname) < 5)
        return false;
    const char* p = strrchr(patchname, '.');
    if (p == NULL)
        return false;
    if (_stricmp(p, ".ips") == 0)
        return patchApplyIPS(patchname, rom, size);
    if (_stricmp(p, ".ups") == 0)
        return patchApplyUPS(patchname, rom, size);
    if (_stricmp(p, ".bps") == 0)
        return patchApplyBPS(patchname, rom, size);
    if (_stricmp(p, ".ppf") == 0)
        return patchApplyPPF(patchname, rom, size);
#endif
    return false;
}
