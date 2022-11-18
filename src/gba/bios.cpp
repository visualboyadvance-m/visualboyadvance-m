#include <math.h>
#include <memory.h>
#include <stdlib.h>

#include "GBA.h"
#include "GBAinline.h"
#include "Globals.h"
#include "bios.h"

uint16_t sineTable[256] = {
    (uint16_t)0x0000, (uint16_t)0x0192, (uint16_t)0x0323, (uint16_t)0x04B5, (uint16_t)0x0645, (uint16_t)0x07D5, (uint16_t)0x0964, (uint16_t)0x0AF1,
    (uint16_t)0x0C7C, (uint16_t)0x0E05, (uint16_t)0x0F8C, (uint16_t)0x1111, (uint16_t)0x1294, (uint16_t)0x1413, (uint16_t)0x158F, (uint16_t)0x1708,
    (uint16_t)0x187D, (uint16_t)0x19EF, (uint16_t)0x1B5D, (uint16_t)0x1CC6, (uint16_t)0x1E2B, (uint16_t)0x1F8B, (uint16_t)0x20E7, (uint16_t)0x223D,
    (uint16_t)0x238E, (uint16_t)0x24DA, (uint16_t)0x261F, (uint16_t)0x275F, (uint16_t)0x2899, (uint16_t)0x29CD, (uint16_t)0x2AFA, (uint16_t)0x2C21,
    (uint16_t)0x2D41, (uint16_t)0x2E5A, (uint16_t)0x2F6B, (uint16_t)0x3076, (uint16_t)0x3179, (uint16_t)0x3274, (uint16_t)0x3367, (uint16_t)0x3453,
    (uint16_t)0x3536, (uint16_t)0x3612, (uint16_t)0x36E5, (uint16_t)0x37AF, (uint16_t)0x3871, (uint16_t)0x392A, (uint16_t)0x39DA, (uint16_t)0x3A82,
    (uint16_t)0x3B20, (uint16_t)0x3BB6, (uint16_t)0x3C42, (uint16_t)0x3CC5, (uint16_t)0x3D3E, (uint16_t)0x3DAE, (uint16_t)0x3E14, (uint16_t)0x3E71,
    (uint16_t)0x3EC5, (uint16_t)0x3F0E, (uint16_t)0x3F4E, (uint16_t)0x3F84, (uint16_t)0x3FB1, (uint16_t)0x3FD3, (uint16_t)0x3FEC, (uint16_t)0x3FFB,
    (uint16_t)0x4000, (uint16_t)0x3FFB, (uint16_t)0x3FEC, (uint16_t)0x3FD3, (uint16_t)0x3FB1, (uint16_t)0x3F84, (uint16_t)0x3F4E, (uint16_t)0x3F0E,
    (uint16_t)0x3EC5, (uint16_t)0x3E71, (uint16_t)0x3E14, (uint16_t)0x3DAE, (uint16_t)0x3D3E, (uint16_t)0x3CC5, (uint16_t)0x3C42, (uint16_t)0x3BB6,
    (uint16_t)0x3B20, (uint16_t)0x3A82, (uint16_t)0x39DA, (uint16_t)0x392A, (uint16_t)0x3871, (uint16_t)0x37AF, (uint16_t)0x36E5, (uint16_t)0x3612,
    (uint16_t)0x3536, (uint16_t)0x3453, (uint16_t)0x3367, (uint16_t)0x3274, (uint16_t)0x3179, (uint16_t)0x3076, (uint16_t)0x2F6B, (uint16_t)0x2E5A,
    (uint16_t)0x2D41, (uint16_t)0x2C21, (uint16_t)0x2AFA, (uint16_t)0x29CD, (uint16_t)0x2899, (uint16_t)0x275F, (uint16_t)0x261F, (uint16_t)0x24DA,
    (uint16_t)0x238E, (uint16_t)0x223D, (uint16_t)0x20E7, (uint16_t)0x1F8B, (uint16_t)0x1E2B, (uint16_t)0x1CC6, (uint16_t)0x1B5D, (uint16_t)0x19EF,
    (uint16_t)0x187D, (uint16_t)0x1708, (uint16_t)0x158F, (uint16_t)0x1413, (uint16_t)0x1294, (uint16_t)0x1111, (uint16_t)0x0F8C, (uint16_t)0x0E05,
    (uint16_t)0x0C7C, (uint16_t)0x0AF1, (uint16_t)0x0964, (uint16_t)0x07D5, (uint16_t)0x0645, (uint16_t)0x04B5, (uint16_t)0x0323, (uint16_t)0x0192,
    (uint16_t)0x0000, (uint16_t)0xFE6E, (uint16_t)0xFCDD, (uint16_t)0xFB4B, (uint16_t)0xF9BB, (uint16_t)0xF82B, (uint16_t)0xF69C, (uint16_t)0xF50F,
    (uint16_t)0xF384, (uint16_t)0xF1FB, (uint16_t)0xF074, (uint16_t)0xEEEF, (uint16_t)0xED6C, (uint16_t)0xEBED, (uint16_t)0xEA71, (uint16_t)0xE8F8,
    (uint16_t)0xE783, (uint16_t)0xE611, (uint16_t)0xE4A3, (uint16_t)0xE33A, (uint16_t)0xE1D5, (uint16_t)0xE075, (uint16_t)0xDF19, (uint16_t)0xDDC3,
    (uint16_t)0xDC72, (uint16_t)0xDB26, (uint16_t)0xD9E1, (uint16_t)0xD8A1, (uint16_t)0xD767, (uint16_t)0xD633, (uint16_t)0xD506, (uint16_t)0xD3DF,
    (uint16_t)0xD2BF, (uint16_t)0xD1A6, (uint16_t)0xD095, (uint16_t)0xCF8A, (uint16_t)0xCE87, (uint16_t)0xCD8C, (uint16_t)0xCC99, (uint16_t)0xCBAD,
    (uint16_t)0xCACA, (uint16_t)0xC9EE, (uint16_t)0xC91B, (uint16_t)0xC851, (uint16_t)0xC78F, (uint16_t)0xC6D6, (uint16_t)0xC626, (uint16_t)0xC57E,
    (uint16_t)0xC4E0, (uint16_t)0xC44A, (uint16_t)0xC3BE, (uint16_t)0xC33B, (uint16_t)0xC2C2, (uint16_t)0xC252, (uint16_t)0xC1EC, (uint16_t)0xC18F,
    (uint16_t)0xC13B, (uint16_t)0xC0F2, (uint16_t)0xC0B2, (uint16_t)0xC07C, (uint16_t)0xC04F, (uint16_t)0xC02D, (uint16_t)0xC014, (uint16_t)0xC005,
    (uint16_t)0xC000, (uint16_t)0xC005, (uint16_t)0xC014, (uint16_t)0xC02D, (uint16_t)0xC04F, (uint16_t)0xC07C, (uint16_t)0xC0B2, (uint16_t)0xC0F2,
    (uint16_t)0xC13B, (uint16_t)0xC18F, (uint16_t)0xC1EC, (uint16_t)0xC252, (uint16_t)0xC2C2, (uint16_t)0xC33B, (uint16_t)0xC3BE, (uint16_t)0xC44A,
    (uint16_t)0xC4E0, (uint16_t)0xC57E, (uint16_t)0xC626, (uint16_t)0xC6D6, (uint16_t)0xC78F, (uint16_t)0xC851, (uint16_t)0xC91B, (uint16_t)0xC9EE,
    (uint16_t)0xCACA, (uint16_t)0xCBAD, (uint16_t)0xCC99, (uint16_t)0xCD8C, (uint16_t)0xCE87, (uint16_t)0xCF8A, (uint16_t)0xD095, (uint16_t)0xD1A6,
    (uint16_t)0xD2BF, (uint16_t)0xD3DF, (uint16_t)0xD506, (uint16_t)0xD633, (uint16_t)0xD767, (uint16_t)0xD8A1, (uint16_t)0xD9E1, (uint16_t)0xDB26,
    (uint16_t)0xDC72, (uint16_t)0xDDC3, (uint16_t)0xDF19, (uint16_t)0xE075, (uint16_t)0xE1D5, (uint16_t)0xE33A, (uint16_t)0xE4A3, (uint16_t)0xE611,
    (uint16_t)0xE783, (uint16_t)0xE8F8, (uint16_t)0xEA71, (uint16_t)0xEBED, (uint16_t)0xED6C, (uint16_t)0xEEEF, (uint16_t)0xF074, (uint16_t)0xF1FB,
    (uint16_t)0xF384, (uint16_t)0xF50F, (uint16_t)0xF69C, (uint16_t)0xF82B, (uint16_t)0xF9BB, (uint16_t)0xFB4B, (uint16_t)0xFCDD, (uint16_t)0xFE6E
};

// 2020-08-12 - negativeExponent
// Fix ArcTan and ArcTan2 based on mgba's hle bios fixes
// https://github.com/mgba-emu/mgba/commit/14dc01409c9e971ea0697f5017b45d0db6a7faf5#diff-8f06a143a9fd912c83209f935d3aca25
// https://github.com/mgba-emu/mgba/commit/b154457857d3367a4c0196a4abadeeb6c850ffdf#diff-8f06a143a9fd912c83209f935d3aca25

void BIOS_ArcTan()
{
#ifdef GBA_LOGGING
    if (systemVerbose & VERBOSE_SWI) {
        log("ArcTan: %08x (VCOUNT=%2d)\n",
            reg[0].I,
            VCOUNT);
    }
#endif

    int32_t i = reg[0].I;
    int32_t a = -((i * i) >> 14);
    int32_t b = ((0xA9 * a) >> 14) + 0x390;
    b = ((b * a) >> 14) + 0x91C;
    b = ((b * a) >> 14) + 0xFB6;
    b = ((b * a) >> 14) + 0x16AA;
    b = ((b * a) >> 14) + 0x2081;
    b = ((b * a) >> 14) + 0x3651;
    b = ((b * a) >> 14) + 0xA2F9;
    reg[0].I = (i * b) >> 16;
    reg[1].I = a;
    reg[3].I = b;

#ifdef GBA_LOGGING
    if (systemVerbose & VERBOSE_SWI) {
        log("ArcTan: return=%08x\n",
            reg[0].I);
    }
#endif
}

void BIOS_ArcTan2()
{
#ifdef GBA_LOGGING
    if (systemVerbose & VERBOSE_SWI) {
        log("ArcTan2: %08x,%08x (VCOUNT=%2d)\n",
            reg[0].I,
            reg[1].I,
            VCOUNT);
    }
#endif

    int32_t x = reg[0].I;
    int32_t y = reg[1].I;
    int32_t res = 0;
    if (y == 0) {
        res = ((x >> 16) & 0x8000);
    } else {
        if (x == 0) {
            res = ((y >> 16) & 0x8000) + 0x4000;
        } else {
            if ((abs(x) > abs(y)) || ((abs(x) == abs(y)) && (!((x < 0) && (y < 0))))) {
                reg[1].I = x;
                reg[0].I = y << 14;
                BIOS_Div();
                BIOS_ArcTan();
                if (x < 0)
                    res = 0x8000 + reg[0].I;
                else
                    res = (((y >> 16) & 0x8000) << 1) + reg[0].I;
            } else {
                reg[0].I = x << 14;
                BIOS_Div();
                BIOS_ArcTan();
                res = (0x4000 + ((y >> 16) & 0x8000)) - reg[0].I;
            }
        }
    }
    reg[0].I = res;
    reg[3].I = 0x170;

#ifdef GBA_LOGGING
    if (systemVerbose & VERBOSE_SWI) {
        log("ArcTan2: return=%08x\n",
            reg[0].I);
    }
#endif
}

void BIOS_BitUnPack()
{
#ifdef GBA_LOGGING
    if (systemVerbose & VERBOSE_SWI) {
        log("BitUnPack: %08x,%08x,%08x (VCOUNT=%2d)\n",
            reg[0].I,
            reg[1].I,
            reg[2].I,
            VCOUNT);
    }
#endif

    uint32_t source = reg[0].I;
    uint32_t dest = reg[1].I;
    uint32_t header = reg[2].I;

    int len = CPUReadHalfWord(header);
    // check address
    if (((source & 0xe000000) == 0) || ((source + len) & 0xe000000) == 0)
        return;

    int bits = CPUReadByte(header + 2);
    int revbits = 8 - bits;
    // uint32_t value = 0;
    uint32_t base = CPUReadMemory(header + 4);
    bool addBase = (base & 0x80000000) ? true : false;
    base &= 0x7fffffff;
    int dataSize = CPUReadByte(header + 3);

    int data = 0;
    int bitwritecount = 0;
    while (1) {
        len -= 1;
        if (len < 0)
            break;
        int mask = 0xff >> revbits;
        uint8_t b = CPUReadByte(source);
        source++;
        int bitcount = 0;
        while (1) {
            if (bitcount >= 8)
                break;
            uint32_t d = b & mask;
            uint32_t temp = d >> bitcount;
            if (d || addBase) {
                temp += base;
            }
            data |= temp << bitwritecount;
            bitwritecount += dataSize;
            if (bitwritecount >= 32) {
                CPUWriteMemory(dest, data);
                dest += 4;
                data = 0;
                bitwritecount = 0;
            }
            mask <<= bits;
            bitcount += bits;
        }
    }
}

void BIOS_GetBiosChecksum()
{
    reg[0].I = 0xBAAE187F;
}

void BIOS_BgAffineSet()
{
#ifdef GBA_LOGGING
    if (systemVerbose & VERBOSE_SWI) {
        log("BgAffineSet: %08x,%08x,%08x (VCOUNT=%2d)\n",
            reg[0].I,
            reg[1].I,
            reg[2].I,
            VCOUNT);
    }
#endif

    uint32_t src = reg[0].I;
    uint32_t dest = reg[1].I;
    int num = reg[2].I;

    for (int i = 0; i < num; i++) {
        int32_t cx = CPUReadMemory(src);
        src += 4;
        int32_t cy = CPUReadMemory(src);
        src += 4;
        int16_t dispx = CPUReadHalfWordSigned(src);
        src += 2;
        int16_t dispy = CPUReadHalfWordSigned(src);
        src += 2;
        int16_t rx = CPUReadHalfWordSigned(src);
        src += 2;
        int16_t ry = CPUReadHalfWordSigned(src);
        src += 2;
        uint16_t theta = DowncastU16(CPUReadHalfWord(src) >> 8);
        src += 4; // keep structure alignment
        int32_t a = sineTable[(theta + 0x40) & 255];
        int32_t b = sineTable[theta];

        int16_t dx = Downcast16((rx * a) >> 14);
        int16_t dmx = Downcast16((rx * b) >> 14);
        int16_t dy = Downcast16((ry * b) >> 14);
        int16_t dmy = Downcast16((ry * a) >> 14);

        CPUWriteHalfWord(dest, dx);
        dest += 2;
        CPUWriteHalfWord(dest, -dmx);
        dest += 2;
        CPUWriteHalfWord(dest, dy);
        dest += 2;
        CPUWriteHalfWord(dest, dmy);
        dest += 2;

        int32_t startx = cx - dx * dispx + dmx * dispy;
        int32_t starty = cy - dy * dispx - dmy * dispy;

        CPUWriteMemory(dest, startx);
        dest += 4;
        CPUWriteMemory(dest, starty);
        dest += 4;
    }
}

void BIOS_CpuSet()
{
#ifdef GBA_LOGGING
    if (systemVerbose & VERBOSE_SWI) {
        log("CpuSet: 0x%08x,0x%08x,0x%08x (VCOUNT=%d)\n", reg[0].I, reg[1].I,
            reg[2].I, VCOUNT);
    }
#endif

    uint32_t source = reg[0].I;
    uint32_t dest = reg[1].I;
    uint32_t cnt = reg[2].I;

    if (((source & 0xe000000) == 0) || ((source + (((cnt << 11) >> 9) & 0x1fffff)) & 0xe000000) == 0)
        return;

    int count = cnt & 0x1FFFFF;

    // 32-bit ?
    if ((cnt >> 26) & 1) {
        // needed for 32-bit mode!
        source &= 0xFFFFFFFC;
        dest &= 0xFFFFFFFC;
        // fill ?
        if ((cnt >> 24) & 1) {
            uint32_t value = (source > 0x0EFFFFFF ? 0x1CAD1CAD : CPUReadMemory(source));
            while (count) {
                CPUWriteMemory(dest, value);
                dest += 4;
                count--;
            }
        } else {
            // copy
            while (count) {
                CPUWriteMemory(dest, (source > 0x0EFFFFFF ? 0x1CAD1CAD : CPUReadMemory(source)));
                source += 4;
                dest += 4;
                count--;
            }
        }
    } else {
        // 16-bit fill?
        if ((cnt >> 24) & 1) {
            uint16_t value = (source > 0x0EFFFFFF ? 0x1CAD : DowncastU16(CPUReadHalfWord(source)));
            while (count) {
                CPUWriteHalfWord(dest, value);
                dest += 2;
                count--;
            }
        } else {
            // copy
            while (count) {
                CPUWriteHalfWord(dest, (source > 0x0EFFFFFF ? 0x1CAD : DowncastU16(CPUReadHalfWord(source))));
                source += 2;
                dest += 2;
                count--;
            }
        }
    }
}

void BIOS_CpuFastSet()
{
#ifdef GBA_LOGGING
    if (systemVerbose & VERBOSE_SWI) {
        log("CpuFastSet: 0x%08x,0x%08x,0x%08x (VCOUNT=%d)\n", reg[0].I, reg[1].I,
            reg[2].I, VCOUNT);
    }
#endif

    uint32_t source = reg[0].I;
    uint32_t dest = reg[1].I;
    uint32_t cnt = reg[2].I;

    if (((source & 0xe000000) == 0) || ((source + (((cnt << 11) >> 9) & 0x1fffff)) & 0xe000000) == 0)
        return;

    // needed for 32-bit mode!
    source &= 0xFFFFFFFC;
    dest &= 0xFFFFFFFC;

    int count = cnt & 0x1FFFFF;

    // fill?
    if ((cnt >> 24) & 1) {
        while (count > 0) {
            // BIOS always transfers 32 bytes at a time
            uint32_t value = (source > 0x0EFFFFFF ? 0xBAFFFFFB : CPUReadMemory(source));
            for (int i = 0; i < 8; i++) {
                CPUWriteMemory(dest, value);
                dest += 4;
            }
            count -= 8;
        }
    } else {
        // copy
        while (count > 0) {
            // BIOS always transfers 32 bytes at a time
            for (int i = 0; i < 8; i++) {
                CPUWriteMemory(dest, (source > 0x0EFFFFFF ? 0xBAFFFFFB : CPUReadMemory(source)));
                source += 4;
                dest += 4;
            }
            count -= 8;
        }
    }
}

void BIOS_Diff8bitUnFilterWram()
{
#ifdef GBA_LOGGING
    if (systemVerbose & VERBOSE_SWI) {
        log("Diff8bitUnFilterWram: 0x%08x,0x%08x (VCOUNT=%d)\n", reg[0].I,
            reg[1].I, VCOUNT);
    }
#endif

    uint32_t source = reg[0].I;
    uint32_t dest = reg[1].I;

    uint32_t header = CPUReadMemory(source);
    source += 4;

    if (((source & 0xe000000) == 0) || (((source + ((header >> 8) & 0x1fffff)) & 0xe000000) == 0))
        return;

    int len = header >> 8;

    uint8_t data = CPUReadByte(source++);
    CPUWriteByte(dest++, data);
    len--;

    while (len > 0) {
        uint8_t diff = CPUReadByte(source++);
        data += diff;
        CPUWriteByte(dest++, data);
        len--;
    }
}

void BIOS_Diff8bitUnFilterVram()
{
#ifdef GBA_LOGGING
    if (systemVerbose & VERBOSE_SWI) {
        log("Diff8bitUnFilterVram: 0x%08x,0x%08x (VCOUNT=%d)\n", reg[0].I,
            reg[1].I, VCOUNT);
    }
#endif

    uint32_t source = reg[0].I;
    uint32_t dest = reg[1].I;

    uint32_t header = CPUReadMemory(source);
    source += 4;

    if (((source & 0xe000000) == 0) || ((source + ((header >> 8) & 0x1fffff)) & 0xe000000) == 0)
        return;

    int len = header >> 8;

    uint8_t data = CPUReadByte(source++);
    uint16_t writeData = data;
    int shift = 8;
    int bytes = 1;

    while (len >= 2) {
        uint8_t diff = CPUReadByte(source++);
        data += diff;
        writeData |= (data << shift);
        bytes++;
        shift += 8;
        if (bytes == 2) {
            CPUWriteHalfWord(dest, writeData);
            dest += 2;
            len -= 2;
            bytes = 0;
            writeData = 0;
            shift = 0;
        }
    }
}

void BIOS_Diff16bitUnFilter()
{
#ifdef GBA_LOGGING
    if (systemVerbose & VERBOSE_SWI) {
        log("Diff16bitUnFilter: 0x%08x,0x%08x (VCOUNT=%d)\n", reg[0].I,
            reg[1].I, VCOUNT);
    }
#endif

    uint32_t source = reg[0].I;
    uint32_t dest = reg[1].I;

    uint32_t header = CPUReadMemory(source);
    source += 4;

    if (((source & 0xe000000) == 0) || ((source + ((header >> 8) & 0x1fffff)) & 0xe000000) == 0)
        return;

    int len = header >> 8;

    uint16_t data = DowncastU16(CPUReadHalfWord(source));
    source += 2;
    CPUWriteHalfWord(dest, data);
    dest += 2;
    len -= 2;

    while (len >= 2) {
        uint16_t diff = DowncastU16(CPUReadHalfWord(source));
        source += 2;
        data += diff;
        CPUWriteHalfWord(dest, data);
        dest += 2;
        len -= 2;
    }
}

void BIOS_Div()
{
#ifdef GBA_LOGGING
    if (systemVerbose & VERBOSE_SWI) {
        log("Div: 0x%08x,0x%08x (VCOUNT=%d)\n",
            reg[0].I,
            reg[1].I,
            VCOUNT);
    }
#endif

    int number = reg[0].I;
    int denom = reg[1].I;

    if (denom != 0) {
        reg[0].I = number / denom;
        reg[1].I = number % denom;
        int32_t temp = (int32_t)reg[0].I;
        reg[3].I = temp < 0 ? (uint32_t)-temp : (uint32_t)temp;
    }
#ifdef GBA_LOGGING
    if (systemVerbose & VERBOSE_SWI) {
        log("Div: return=0x%08x,0x%08x,0x%08x\n",
            reg[0].I,
            reg[1].I,
            reg[3].I);
    }
#endif
}

void BIOS_DivARM()
{
#ifdef GBA_LOGGING
    if (systemVerbose & VERBOSE_SWI) {
        log("DivARM: 0x%08x, (VCOUNT=%d)\n",
            reg[0].I,
            VCOUNT);
    }
#endif

    uint32_t temp = reg[0].I;
    reg[0].I = reg[1].I;
    reg[1].I = temp;
    BIOS_Div();
}

void BIOS_HuffUnComp()
{
#ifdef GBA_LOGGING
    if (systemVerbose & VERBOSE_SWI) {
        log("HuffUnComp: 0x%08x,0x%08x (VCOUNT=%d)\n",
            reg[0].I,
            reg[1].I,
            VCOUNT);
    }
#endif

    uint32_t source = reg[0].I;
    uint32_t dest = reg[1].I;

    uint32_t header = CPUReadMemory(source);
    source += 4;

    if (((source & 0xe000000) == 0) || ((source + ((header >> 8) & 0x1fffff)) & 0xe000000) == 0)
        return;

    uint8_t treeSize = CPUReadByte(source++);

    uint32_t treeStart = source;

    source += ((treeSize + 1) << 1) - 1; // minus because we already skipped one byte

    int len = header >> 8;

    uint32_t mask = 0x80000000;
    uint32_t data = CPUReadMemory(source);
    source += 4;

    int pos = 0;
    uint8_t rootNode = CPUReadByte(treeStart);
    uint8_t currentNode = rootNode;
    bool writeData = false;
    int byteShift = 0;
    int byteCount = 0;
    uint32_t writeValue = 0;

    if ((header & 0x0F) == 8) {
        while (len > 0) {
            // take left
            if (pos == 0)
                pos++;
            else
                pos += (((currentNode & 0x3F) + 1) << 1);

            if (data & mask) {
                // right
                if (currentNode & 0x40)
                    writeData = true;
                currentNode = CPUReadByte(treeStart + pos + 1);
            } else {
                // left
                if (currentNode & 0x80)
                    writeData = true;
                currentNode = CPUReadByte(treeStart + pos);
            }

            if (writeData) {
                writeValue |= (currentNode << byteShift);
                byteCount++;
                byteShift += 8;

                pos = 0;
                currentNode = rootNode;
                writeData = false;

                if (byteCount == 4) {
                    byteCount = 0;
                    byteShift = 0;
                    CPUWriteMemory(dest, writeValue);
                    writeValue = 0;
                    dest += 4;
                    len -= 4;
                }
            }
            mask >>= 1;
            if (mask == 0) {
                mask = 0x80000000;
                data = CPUReadMemory(source);
                source += 4;
            }
        }
    } else {
        int halfLen = 0;
        int value = 0;
        while (len > 0) {
            // take left
            if (pos == 0)
                pos++;
            else
                pos += (((currentNode & 0x3F) + 1) << 1);

            if ((data & mask)) {
                // right
                if (currentNode & 0x40)
                    writeData = true;
                currentNode = CPUReadByte(treeStart + pos + 1);
            } else {
                // left
                if (currentNode & 0x80)
                    writeData = true;
                currentNode = CPUReadByte(treeStart + pos);
            }

            if (writeData) {
                if (halfLen == 0)
                    value |= currentNode;
                else
                    value |= (currentNode << 4);

                halfLen += 4;
                if (halfLen == 8) {
                    writeValue |= (value << byteShift);
                    byteCount++;
                    byteShift += 8;

                    halfLen = 0;
                    value = 0;

                    if (byteCount == 4) {
                        byteCount = 0;
                        byteShift = 0;
                        CPUWriteMemory(dest, writeValue);
                        dest += 4;
                        writeValue = 0;
                        len -= 4;
                    }
                }
                pos = 0;
                currentNode = rootNode;
                writeData = false;
            }
            mask >>= 1;
            if (mask == 0) {
                mask = 0x80000000;
                data = CPUReadMemory(source);
                source += 4;
            }
        }
    }
}

void BIOS_LZ77UnCompVram()
{
#ifdef GBA_LOGGING
    if (systemVerbose & VERBOSE_SWI) {
        log("LZ77UnCompVram: 0x%08x,0x%08x (VCOUNT=%d)\n",
            reg[0].I,
            reg[1].I,
            VCOUNT);
    }
#endif

    uint32_t source = reg[0].I;
    uint32_t dest = reg[1].I;

    uint32_t header = CPUReadMemory(source);
    source += 4;

    if (((source & 0xe000000) == 0) || ((source + ((header >> 8) & 0x1fffff)) & 0xe000000) == 0)
        return;

    int byteCount = 0;
    int byteShift = 0;
    uint32_t writeValue = 0;

    int len = header >> 8;

    while (len > 0) {
        uint8_t d = CPUReadByte(source++);

        if (d) {
            for (int i = 0; i < 8; i++) {
                if (d & 0x80) {
                    uint16_t data = CPUReadByte(source++) << 8;
                    data |= CPUReadByte(source++);
                    int length = (data >> 12) + 3;
                    int offset = (data & 0x0FFF);
                    uint32_t windowOffset = dest + byteCount - offset - 1;
                    for (int i2 = 0; i2 < length; i2++) {
                        writeValue |= (CPUReadByte(windowOffset++) << byteShift);
                        byteShift += 8;
                        byteCount++;

                        if (byteCount == 2) {
                            CPUWriteHalfWord(dest, DowncastU16(writeValue));
                            dest += 2;
                            byteCount = 0;
                            byteShift = 0;
                            writeValue = 0;
                        }
                        len--;
                        if (len == 0)
                            return;
                    }
                } else {
                    writeValue |= (CPUReadByte(source++) << byteShift);
                    byteShift += 8;
                    byteCount++;
                    if (byteCount == 2) {
                        CPUWriteHalfWord(dest, DowncastU16(writeValue));
                        dest += 2;
                        byteCount = 0;
                        byteShift = 0;
                        writeValue = 0;
                    }
                    len--;
                    if (len == 0)
                        return;
                }
                d <<= 1;
            }
        } else {
            for (int i = 0; i < 8; i++) {
                writeValue |= (CPUReadByte(source++) << byteShift);
                byteShift += 8;
                byteCount++;
                if (byteCount == 2) {
                    CPUWriteHalfWord(dest, DowncastU16(writeValue));
                    dest += 2;
                    byteShift = 0;
                    byteCount = 0;
                    writeValue = 0;
                }
                len--;
                if (len == 0)
                    return;
            }
        }
    }
}

void BIOS_LZ77UnCompWram()
{
#ifdef GBA_LOGGING
    if (systemVerbose & VERBOSE_SWI) {
        log("LZ77UnCompWram: 0x%08x,0x%08x (VCOUNT=%d)\n", reg[0].I, reg[1].I,
            VCOUNT);
    }
#endif

    uint32_t source = reg[0].I;
    uint32_t dest = reg[1].I;

    uint32_t header = CPUReadMemory(source);
    source += 4;

    if (((source & 0xe000000) == 0) || ((source + ((header >> 8) & 0x1fffff)) & 0xe000000) == 0)
        return;

    int len = header >> 8;

    while (len > 0) {
        uint8_t d = CPUReadByte(source++);

        if (d) {
            for (int i = 0; i < 8; i++) {
                if (d & 0x80) {
                    uint16_t data = CPUReadByte(source++) << 8;
                    data |= CPUReadByte(source++);
                    int length = (data >> 12) + 3;
                    int offset = (data & 0x0FFF);
                    uint32_t windowOffset = dest - offset - 1;
                    for (int i2 = 0; i2 < length; i2++) {
                        CPUWriteByte(dest++, CPUReadByte(windowOffset++));
                        len--;
                        if (len == 0)
                            return;
                    }
                } else {
                    CPUWriteByte(dest++, CPUReadByte(source++));
                    len--;
                    if (len == 0)
                        return;
                }
                d <<= 1;
            }
        } else {
            for (int i = 0; i < 8; i++) {
                CPUWriteByte(dest++, CPUReadByte(source++));
                len--;
                if (len == 0)
                    return;
            }
        }
    }
}

void BIOS_ObjAffineSet()
{
#ifdef GBA_LOGGING
    if (systemVerbose & VERBOSE_SWI) {
        log("ObjAffineSet: 0x%08x,0x%08x,0x%08x,0x%08x (VCOUNT=%d)\n",
            reg[0].I,
            reg[1].I,
            reg[2].I,
            reg[3].I,
            VCOUNT);
    }
#endif

    uint32_t src = reg[0].I;
    uint32_t dest = reg[1].I;
    int num = reg[2].I;
    int offset = reg[3].I;

    for (int i = 0; i < num; i++) {
        int16_t rx = CPUReadHalfWordSigned(src);
        src += 2;
        int16_t ry = CPUReadHalfWordSigned(src);
        src += 2;
        uint16_t theta = DowncastU16(CPUReadHalfWord(src) >> 8);
        src += 4; // keep structure alignment

        int32_t a = (int32_t)sineTable[(theta + 0x40) & 255];
        int32_t b = (int32_t)sineTable[theta];

        int16_t dx = Downcast16((((int32_t)rx * a) >> 14));
        int16_t dmx = Downcast16(((int32_t)rx * b) >> 14);
        int16_t dy = Downcast16(((int32_t)ry * b) >> 14);
        int16_t dmy = Downcast16(((int32_t)ry * a) >> 14);

        CPUWriteHalfWord(dest, dx);
        dest += offset;
        CPUWriteHalfWord(dest, -dmx);
        dest += offset;
        CPUWriteHalfWord(dest, dy);
        dest += offset;
        CPUWriteHalfWord(dest, dmy);
        dest += offset;
    }
}

void BIOS_RegisterRamReset(uint32_t flags)
{
    // no need to trace here. this is only called directly from GBA.cpp
    // to emulate bios initialization

    CPUUpdateRegister(0x0, 0x80);

    if (flags) {
        if (flags & 0x01) {
            // clear work RAM
            memset(workRAM, 0, SIZE_WRAM);
        }
        if (flags & 0x02) {
            // clear internal RAM
            memset(internalRAM, 0, 0x7e00); // don't clear 0x7e00-0x7fff
        }
        if (flags & 0x04) {
            // clear palette RAM
            memset(paletteRAM, 0, 0x400);
        }
        if (flags & 0x08) {
            // clear VRAM
            memset(vram, 0, 0x18000);
        }
        if (flags & 0x10) {
            // clean OAM
            memset(oam, 0, 0x400);
        }

        if (flags & 0x80) {
            int i;
            for (i = 0; i < 0x10; i++)
                CPUUpdateRegister(0x200 + i * 2, 0);

            for (i = 0; i < 0xF; i++)
                CPUUpdateRegister(0x4 + i * 2, 0);

            for (i = 0; i < 0x20; i++)
                CPUUpdateRegister(0x20 + i * 2, 0);

            for (i = 0; i < 0x18; i++)
                CPUUpdateRegister(0xb0 + i * 2, 0);

            CPUUpdateRegister(0x130, 0);
            CPUUpdateRegister(0x20, 0x100);
            CPUUpdateRegister(0x30, 0x100);
            CPUUpdateRegister(0x26, 0x100);
            CPUUpdateRegister(0x36, 0x100);
        }

        if (flags & 0x20) {
            int i;
            for (i = 0; i < 8; i++)
                CPUUpdateRegister(0x110 + i * 2, 0);
            CPUUpdateRegister(0x134, 0x8000);
            for (i = 0; i < 7; i++)
                CPUUpdateRegister(0x140 + i * 2, 0);
        }

        if (flags & 0x40) {
            int i;
            CPUWriteByte(0x4000084, 0);
            CPUWriteByte(0x4000084, 0x80);
            CPUWriteMemory(0x4000080, 0x880e0000);
            CPUUpdateRegister(0x88, CPUReadHalfWord(0x4000088) & 0x3ff);
            CPUWriteByte(0x4000070, 0x70);
            for (i = 0; i < 8; i++)
                CPUUpdateRegister(0x90 + i * 2, 0);
            CPUWriteByte(0x4000070, 0);
            for (i = 0; i < 8; i++)
                CPUUpdateRegister(0x90 + i * 2, 0);
            CPUWriteByte(0x4000084, 0);
        }
    }
}

void BIOS_RegisterRamReset()
{
#ifdef GBA_LOGGING
    if (systemVerbose & VERBOSE_SWI) {
        log("RegisterRamReset: 0x%08x (VCOUNT=%d)\n",
            reg[0].I,
            VCOUNT);
    }
#endif

    BIOS_RegisterRamReset(reg[0].I);
}

void BIOS_RLUnCompVram()
{
#ifdef GBA_LOGGING
    if (systemVerbose & VERBOSE_SWI) {
        log("RLUnCompVram: 0x%08x,0x%08x (VCOUNT=%d)\n",
            reg[0].I,
            reg[1].I,
            VCOUNT);
    }
#endif

    uint32_t source = reg[0].I;
    uint32_t dest = reg[1].I;

    uint32_t header = CPUReadMemory(source & 0xFFFFFFFC);
    source += 4;

    if (((source & 0xe000000) == 0) || ((source + ((header >> 8) & 0x1fffff)) & 0xe000000) == 0)
        return;

    int len = header >> 8;
    int byteCount = 0;
    int byteShift = 0;
    uint32_t writeValue = 0;

    while (len > 0) {
        uint8_t d = CPUReadByte(source++);
        int l = d & 0x7F;
        if (d & 0x80) {
            uint8_t data = CPUReadByte(source++);
            l += 3;
            for (int i = 0; i < l; i++) {
                writeValue |= (data << byteShift);
                byteShift += 8;
                byteCount++;

                if (byteCount == 2) {
                    CPUWriteHalfWord(dest, DowncastU16(writeValue));
                    dest += 2;
                    byteCount = 0;
                    byteShift = 0;
                    writeValue = 0;
                }
                len--;
                if (len == 0)
                    return;
            }
        } else {
            l++;
            for (int i = 0; i < l; i++) {
                writeValue |= (CPUReadByte(source++) << byteShift);
                byteShift += 8;
                byteCount++;
                if (byteCount == 2) {
                    CPUWriteHalfWord(dest, DowncastU16(writeValue));
                    dest += 2;
                    byteCount = 0;
                    byteShift = 0;
                    writeValue = 0;
                }
                len--;
                if (len == 0)
                    return;
            }
        }
    }
}

void BIOS_RLUnCompWram()
{
#ifdef GBA_LOGGING
    if (systemVerbose & VERBOSE_SWI) {
        log("RLUnCompWram: 0x%08x,0x%08x (VCOUNT=%d)\n",
            reg[0].I,
            reg[1].I,
            VCOUNT);
    }
#endif

    uint32_t source = reg[0].I;
    uint32_t dest = reg[1].I;

    uint32_t header = CPUReadMemory(source & 0xFFFFFFFC);
    source += 4;

    if (((source & 0xe000000) == 0) || ((source + ((header >> 8) & 0x1fffff)) & 0xe000000) == 0)
        return;

    int len = header >> 8;

    while (len > 0) {
        uint8_t d = CPUReadByte(source++);
        int l = d & 0x7F;
        if (d & 0x80) {
            uint8_t data = CPUReadByte(source++);
            l += 3;
            for (int i = 0; i < l; i++) {
                CPUWriteByte(dest++, data);
                len--;
                if (len == 0)
                    return;
            }
        } else {
            l++;
            for (int i = 0; i < l; i++) {
                CPUWriteByte(dest++, CPUReadByte(source++));
                len--;
                if (len == 0)
                    return;
            }
        }
    }
}

void BIOS_SoftReset()
{
#ifdef GBA_LOGGING
    if (systemVerbose & VERBOSE_SWI) {
        log("SoftReset: (VCOUNT=%d)\n", VCOUNT);
    }
#endif

    armState = true;
    armMode = 0x1F;
    armIrqEnable = false;
    C_FLAG = V_FLAG = N_FLAG = Z_FLAG = false;
    reg[13].I = 0x03007F00;
    reg[14].I = 0x00000000;
    reg[16].I = 0x00000000;
    reg[R13_IRQ].I = 0x03007FA0;
    reg[R14_IRQ].I = 0x00000000;
    reg[SPSR_IRQ].I = 0x00000000;
    reg[R13_SVC].I = 0x03007FE0;
    reg[R14_SVC].I = 0x00000000;
    reg[SPSR_SVC].I = 0x00000000;
    uint8_t b = internalRAM[0x7ffa];

    memset(&internalRAM[0x7e00], 0, 0x200);

    if (b) {
        armNextPC = 0x02000000;
        reg[15].I = 0x02000004;
    } else {
        armNextPC = 0x08000000;
        reg[15].I = 0x08000004;
    }
}

void BIOS_Sqrt()
{
#ifdef GBA_LOGGING
    if (systemVerbose & VERBOSE_SWI) {
        log("Sqrt: %08x (VCOUNT=%2d)\n",
            reg[0].I,
            VCOUNT);
    }
#endif
    reg[0].I = (uint32_t)sqrt((double)reg[0].I);
#ifdef GBA_LOGGING
    if (systemVerbose & VERBOSE_SWI) {
        log("Sqrt: return=%08x\n",
            reg[0].I);
    }
#endif
}

#define ADBITS_MASK 0x3FU
uint32_t const base1 = 0x040000C0;
uint32_t const base2 = 0x04000080;

static bool BIOS_SndDriver_ba4(uint32_t r0, uint32_t r4r12) // 0xba4
{
    if (r4r12) {
        r4r12 = r4r12 & ~0xFE000000;
        r4r12 += r0;
        if (!((r0 & 0x0E000000) && (r4r12 & 0x0E000000)))
            return true;
    }

    return false;
}

static void BIOS_SndDriver_b4c(uint32_t r0, uint32_t r1, uint32_t r2) // 0xb4c
{
    // @r4
    uint32_t r4 = 4 * r2 & 0x7FFFFF;
    bool ok = BIOS_SndDriver_ba4(r0, r4); // aka b9c

#if 0
    int v3; // r4@1
    int v4; // r0@1
    int v5; // r1@1
    int v6; // r2@1
    char v7; // zf@1
    signed int v8; // r5@2
    int v9; // r5@4
    int v10; // r3@6
    int v11; // r3@10
    unsigned int v12; // r4@11
    unsigned short v13; // r3@13
#endif

    // 0b56
    if (!ok) {
        uint32_t r5 = 0; //v8
        if (r2 >= (1 << (27 - 1))) //v6
        {
            r5 = r1 + r4;
            if (r2 >= (1 << (25 - 1))) {
                uint32_t r3 = CPUReadMemory(r0);
                while (r1 < r5) {
                    CPUWriteMemory(r1, r3);
                    r1 += 4;
                }
            } else // @todo 0b6e
            {
#if 0
          while ( v5 < v9 )
          {
            v11 = *(_DWORD *)v4;
            v4 += 4;
            *(_DWORD *)v5 = v11;
            v5 += 4;
          }
#endif
            }
        } else // @todo 0b78
        {
#if 0
        v12 = (unsigned int)v3 >> 1;
        if ( __CFSHR__(v6, 25) )
        {
          v13 = *(_WORD *)v4;
          while ( v8 < (signed int)v12 )
          {
            *(_WORD *)(v5 + v8) = v13;
            v8 += 2;
          }
        }
        else
        {
          while ( v8 < (signed int)v12 )
          {
            *(_WORD *)(v5 + v8) = *(_WORD *)(v4 + v8);
            v8 += 2;
          }
        }
#endif
        }
    }
    // 0x0b96
}

static int32_t BIOS_SndDriver_3e4(uint32_t const r0a, uint32_t const r1a) // 0x3e4
{
    int32_t r0 = (int32_t)r1a;
    int32_t r1 = (int32_t)r0a;
    uint32_t v5 = r0 & 0x80000000;
    int32_t r12;
    int32_t r2;
    bool gtr;

    if ((r1 & 0x80000000) != 0)
        r1 = -r1;
    r12 = r0; //v5 ^ (r0 >> 32);
    if (r0 < 0)
        r0 = -r0;
    r2 = r1;

    do {
        int32_t r0_rsh = ((uint32_t)r0 >> 1);
        gtr = (r2 >= r0_rsh);
        if (r2 <= r0_rsh)
            r2 *= 2;
    } while (!gtr);

    while (1) {
        v5 += (r0 >= r2) + v5;
        if (r0 >= r2)
            r0 -= r2;
        if (r2 == r1)
            break;
        r2 = ((uint32_t)r2 >> 1);
    }

    if ((r12 << 1) == 0)
        return -v5;

    return v5;
}

static void BIOS_SndDriverSub1(uint32_t p1) // 0x170a
{
    uint8_t local1 = DowncastU8((p1 & 0x000F0000) >> 16); // param is r0
    uint32_t const puser1 = CPUReadMemory(0x3007FF0); // 7FC0 + 0x30

    // Store something
    CPUWriteByte(puser1 + 8, local1);

    uint32_t r0 = (0x31e8 + (local1 << 1)) - 0x20;

    // @todo read from bios region
    if (r0 == 0x31D0) {
        r0 = 0xE0;
    } else if (r0 == 0x31E0) {
        r0 = 0x2C0;
    } else
        r0 = CPUReadHalfWord(r0 + 0x1E);
    CPUWriteMemory(puser1 + 0x10, r0);

    uint32_t r1 = 0x630;
    uint32_t const r4 = r0;

    // 0x172c
    r0 = BIOS_SndDriver_3e4(r0, r1);
    CPUWriteByte(puser1 + 0xB, DowncastU8(r0));

    uint32_t x = 0x91d1b * r4;
    r1 = x + 0x1388;
    r0 = 0x1388 << 1;
    r0 = BIOS_SndDriver_3e4(r0, r1);
    CPUWriteMemory(puser1 + 0x14, r0);

    r1 = 1 << 24;
    r0 = BIOS_SndDriver_3e4(r0, r1) + 1;
    r0 /= 2;
    CPUWriteMemory(puser1 + 0x18, r0);

    // 0x1750
    uint32_t r4basesnd = 0x4000100;
    r0 = r4;
    r1 = 0x44940;
    CPUWriteHalfWord(r4basesnd + 2, 0);
    r0 = BIOS_SndDriver_3e4(r0, r1);
    r0 = (1 << 16) - r0;
    CPUWriteHalfWord(r4basesnd + 0, DowncastU16(r0));

    // sub 0x18c8 is unrolled here
    r1 = 0x5b << 9;
    CPUWriteHalfWord(base1 + 6, DowncastU16(r1));
    CPUWriteHalfWord(base1 + 12, DowncastU16(r1));

    // 0x176a, @todo busy loop here
    r0 = 0x4000000;
    //do
    {
        r1 = CPUReadByte(r0 + 6);
    } //while (r1 != 0x9F);

    CPUWriteHalfWord(r4basesnd + 2, 0x80);
}

void BIOS_SndDriverInit() // 0x166a
{
#ifdef GBA_LOGGING
    if (systemVerbose & VERBOSE_SWI) {
        log("SndDriverInit: WaveData=%08x mk=%08x fp=%08x\n",
            reg[0].I,
            reg[1].I,
            reg[2].I);
    }
#endif

    // 7FC0 + 0x30
    uint32_t const puser1 = 0x3007FF0;
    uint32_t const user1 = reg[0].I;

    uint32_t base3 = 0x040000BC;
    //uint32_t base4 = 0x03007FF0;

    CPUWriteHalfWord(base1 + 6, 0);
    CPUWriteHalfWord(base1 + 12, 0);

    CPUWriteHalfWord(base2 + 4, 0x8F);
    CPUWriteHalfWord(base2 + 2, 0xA90E);

    uint16_t val9 = DowncastU16(CPUReadHalfWord(base2 + 9));
    CPUWriteHalfWord(base2 + 9, val9 & ADBITS_MASK); // DA?

    CPUWriteMemory(base3 + 0, (user1 + 0x350)); //0x350, 640int
    CPUWriteMemory(base1 + 0, 0x40000A0);
    CPUWriteMemory(base1 + 8, 2224); //0x980
    CPUWriteMemory(base1 + 12, 0x40000A4);
    CPUWriteMemory(puser1, user1);

    uint32_t const r2 = 0x050003EC;
    uint32_t const sp = reg[13].I; // 0x03003c98;
    CPUWriteMemory(sp, 0);
    BIOS_SndDriver_b4c(sp, user1, r2);

    // 0x16b0
    CPUWriteByte(user1 + 0x6, 0x8);
    CPUWriteByte(user1 + 0x7, 0xF);
    CPUWriteMemory(user1 + 0x38, 0x2425);
    CPUWriteMemory(user1 + 0x28, 0x1709);
    CPUWriteMemory(user1 + 0x2C, 0x1709);
    CPUWriteMemory(user1 + 0x30, 0x1709);
    CPUWriteMemory(user1 + 0x3C, 0x1709);
    CPUWriteMemory(user1 + 0x34, 0x3738);

    BIOS_SndDriverSub1(0x40000);

    CPUWriteMemory(user1, 0x68736D53); // this ret is common among funcs
}

void BIOS_SndDriverMode() //0x179c
{
    uint32_t input = reg[0].I;
    uint32_t const puser1 = CPUReadMemory(0x3007FF0); // 7FC0 + 0x30
    uint32_t user1 = CPUReadMemory(puser1);

    if (user1 == 0x68736D53) {
        CPUWriteMemory(puser1, (++user1)); // this guard is common for funcs, unify

        // reverb values at bits 0...7
        uint8_t revb = (input & 0xFF);
        if (revb) {
            revb >>= 1; // shift out the 7th enable bit
            CPUWriteByte(puser1 + 5, revb);
        }
        // direct sound multi channels at bits 8...11
        uint8_t chc = (input & 0xF00) >> 8;
        if (chc > 0) {
            CPUWriteByte(puser1 + 6, chc);
            uint32_t puser2 = puser1 + 7 + 0x49;
            int count = 12;
            while (count--) {
                CPUWriteByte(puser2, 0);
                puser2 += 0x40;
            }
        }
        // direct sound master volume at bits 12...15
        uint8_t chv = (input & 0xF000) >> 12;
        if (chv > 0) {
            CPUWriteByte(puser1 + 7, chv);
        }
        // DA converter bits at bits 20...23
        uint32_t dab = (input & 0x00B00000);
        if (dab) {
            dab &= 0x00300000;
            dab >>= 0xE;
            uint8_t adv = CPUReadByte(puser1 + 9) & ADBITS_MASK; // @todo verify offset
            dab |= adv;
            CPUWriteByte(puser1 + 9, DowncastU8(dab));
        }
        // Playback frequency at bits 16...19
        uint32_t pbf = (input & 0x000F0000);
        if (pbf) {
            // Modifies puser1/user1
            BIOS_SndDriverVSyncOff();

            // another sub at 180c
            BIOS_SndDriverSub1(pbf);
        }
        CPUWriteMemory(puser1, (--user1));
    }
}

void BIOS_SndDriverMain() // 0x1dc4 -> 0x08004024 phantasy star
{
    //// Usually addr 0x2010928
    uint32_t const puser1 = CPUReadMemory(0x3007FF0); // 7FC0 + 0x30, //@+sp14
    uint32_t user1 = CPUReadMemory(puser1);

    if (user1 != 0x68736D53)
        return;

    // main
    CPUWriteMemory(puser1, (++user1)); // this guard is common for funcs, unify

    int const user2 = CPUReadMemory(puser1 + 0x20);
    if (user2) {
        // Call 0x2102  sub_16A8 - -> param r1
    }

    int const userfunc = CPUReadMemory(puser1 + 0x28);
    switch (userfunc) {
    case 0x1709: //phantasy star
    default:
        break;
    }

    uint32_t const v2 = CPUReadMemory(puser1 + 0x10); //r8
    uint8_t const user4 = CPUReadByte(puser1 + 4) - 1;
    uint32_t user41 = 0;

    if (user4 > 0) {
        user41 = CPUReadByte(puser1 + 0x0B);
        user41 -= user4;
        user41 *= v2;
    }

    uint32_t r5;
    uint32_t const r5c = puser1 + 0x350 + user41; //r5 @sp+8
    int user6 = r5c + 0x630; //r6
    int user5 = CPUReadByte(puser1 + 0x5); //r3

    if (user5) {
        // @todo 0x1e1a
    } else // 0x1e74
    {
        r5 = r5c;
        int count = v2 >> 4; //r1...v13
        if (!(v2 >> 3)) {
            CPUWriteMemory(r5, 0);
            CPUWriteMemory(user6, 0);
            r5 += 4;
            user6 += 4;
        }
        if (!(v2 >> 1)) //0x1e7c
        {
            CPUWriteMemory(r5, 0);
            CPUWriteMemory(user6, 0);
            r5 += 4;
            user6 += 4;

            CPUWriteMemory(r5, 0);
            CPUWriteMemory(user6, 0);
            r5 += 4;
            user6 += 4;
        }
        do // 0x1e8e
        {
            // @todo optimize this memset
            CPUWriteMemory(r5, 0);
            CPUWriteMemory(user6, 0);
            r5 += 4;
            user6 += 4;

            CPUWriteMemory(r5, 0);
            CPUWriteMemory(user6, 0);
            r5 += 4;
            user6 += 4;

            CPUWriteMemory(r5, 0);
            CPUWriteMemory(user6, 0);
            r5 += 4;
            user6 += 4;

            CPUWriteMemory(r5, 0);
            CPUWriteMemory(user6, 0);
            r5 += 4;
            user6 += 4;

            count -= 1;
        } while (count > 0);
    }

    //0x1ea2
    uint32_t r4 = puser1; // apparenty ch ptr?
    uint32_t r9 = CPUReadMemory(r4 + 0x14);
    uint32_t r12 = CPUReadMemory(r4 + 0x18);
    uint32_t i = CPUReadByte(r4 + 0x6);

    for (r4 += 0x10; i > 0; i--) {
        r4 += 0x40;
        /*lbl_0x1eb0:*/
        uint32_t r3 = CPUReadMemory(r4 + 0x24);
        uint8_t r6 = CPUReadByte(r4);

        if ((r6 & 0xC7) == 0) // 0x1ebc
            continue; //goto lbl_20e4;
        if ((r6 & 0x80) && ((r6 & 0x40) == 0)) // 0x1ec4
        {
            r6 = 0x3;
            CPUWriteByte(r4, r6);
            CPUWriteMemory(r4 + 0x28, r3 + 0x10);

            int r0t1 = CPUReadMemory(r3 + 0xC);
            CPUWriteMemory(r4 + 0x18, r0t1);

            r5 = 0;
            CPUWriteByte(r4 + 0x9, 0);
            CPUWriteMemory(r4 + 0x1c, 0);

            uint8_t r2a = CPUReadByte(r3 + 0x3); // seems to be LABEL_20e4
            if ((r2a & 0xC0)) // 1ee4
            {
            }
            goto lbl_0x1f46;
        } else {
            //lbl_0x1eee:
            r5 = CPUReadByte(r4 + 0x9); //
            if ((r6 & 0x4) != 0) // @todo 0x1ef4
            {
            }

            if ((r6 & 0x40) != 0) // @todo 0x1f08
            {
            }

            if ((r6 & 0x03) == 2) // 0x1f2a
            {
                uint8_t mul1 = CPUReadByte(r4 + 0x5);
                r5 *= mul1;
                r5 >>= 8;

                uint8_t cmp1 = CPUReadByte(r4 + 0x6);
                if (r5 <= cmp1) {
                    r5 = cmp1;
                    // @todo beq @ 0x1f3a -> ??
                    r6--;
                    CPUWriteByte(r4, r6);
                }
            } else if ((r6 & 0x03) == 3) // 0x1f44
            {
            lbl_0x1f46: //@todo check if there is really another path to here
                uint8_t add1 = CPUReadByte(r4 + 0x4);
                r5 += add1;

                if (r5 >= 0xff) {
                    r6--;
                    r5 = 0xff;
                    CPUWriteByte(r4, r6);
                }
            }
        }
        {
            //lbl_0x1f54:
            CPUWriteByte(r4 + 0x9, DowncastU8(r5));

            uint32_t user0 = CPUReadByte(puser1 + 0x7); // @sp+10
            user0++;
            user0 *= r5;
            r5 = user0 >> 4;

            user0 = CPUReadByte(r4 + 0x2);
            user0 *= r5;
            user0 >>= 8;
            CPUWriteByte(r4 + 0xA, DowncastU8(user0));

            user0 = CPUReadByte(r4 + 0x3);
            user0 *= r5;
            user0 >>= 8;
            CPUWriteByte(r4 + 0xB, DowncastU8(user0));

            user0 = r6 & 0x10;
            if (user0 != 0) // @todo 0x1f76
            {
            }

            r5 = r5c; // @todo below r5 is used and updated
            uint32_t r2 = CPUReadMemory(r4 + 0x18);
            r3 = CPUReadMemory(r4 + 0x28);

            uint32_t r8 = v2;

            uint8_t r10 = CPUReadByte(r4 + 0xA);
            uint8_t r11 = CPUReadByte(r4 + 0xB);
            uint8_t r0a = CPUReadByte(r4 + 0x1);
            if ((r0a & 8)) //@todo 0x1fa8
            {
            }

            uint32_t r7 = CPUReadMemory(r4 + 0x1c);
            uint32_t r14 = CPUReadMemory(r4 + 0x20);

        lbl_0x2004: //	LABEL_52:
            while (r7 >= 4 * r9) {
                if (r2 <= 4) // @todo 0x2008, no phant
                    goto lbl_204c;
                r2 -= 4;
                r3 += 4;
                r7 -= 4 * r9;
            }
            if (r7 >= 2 * r9) {
                if (r2 <= 2) // @todo 0x2008, no phant
                    goto lbl_204c;
                r2 -= 2;
                r3 += 2;
                r7 -= 2 * r9;
            }
            if (r7 < r9)
                goto lbl_207c;
            do {
            lbl_204c: //	LABEL_59:
                --r2;
                if (r2) {
                    ++r3;
                } else {
                    r2 = user0; //0x2054
                    if (r2) {
                        r3 = CPUReadMemory(reg[13].I + 0xC); // @todo stack pull 0x205c
                    } else {
                        CPUWriteByte(r4, DowncastU8(r2));
                        goto lbl_20e4;
                    }
                }
                r7 -= r9;
            } while (r7 >= r9);
        lbl_207c:
            while (1) {
                int32_t r0b = CPUReadByte(DowncastU8(r3));
                int32_t r1a = CPUReadByte(r3 + 0x1);

                r1a -= r0b;
                int32_t r6a = r1a * (int32_t)r7;
                r1a = r6a * r12; // 208c
                r6a = (r0b + ((int8_t)(r1a >> 23)));

                r1a = r6a * (int32_t)r11;

                r0b = CPUReadByte(r5 + 0x630);
                r0b = (r0b + ((int8_t)(r1a >> 8)));
                CPUWriteByte(r5 + 0x630, DowncastU8(r0b));
                r1a = r6a * (int32_t)r10;
                r0b = CPUReadByte(r5);
                r0b = (r0b + ((int8_t)(r1a >> 8)));
                CPUWriteByte(r5++, DowncastU8(r0b)); //ptr inc +1 not +4

                r7 += r14;
                --r8;
                if (!r8)
                    break;
                if (r7 >= r9)
                    goto lbl_0x2004;
            }

            CPUWriteMemory(r4 + 0x1c, r7);
            //lbl_20cc:
            CPUWriteMemory(r4 + 0x18, r2);
            CPUWriteMemory(r4 + 0x28, r3);
        }
    lbl_20e4:
        (void)r5;
    }

    // 0x20EE
    CPUWriteMemory(puser1, 0x68736D53); // this guard is common for funcs, unify
}

// fully implemented @ 0x210c
void BIOS_SndDriverVSync()
{
    uint32_t const puser1 = CPUReadMemory(0x3007FF0); // @todo some sound area, make it common.
    uint32_t const user1 = CPUReadMemory(puser1);

    if (user1 == 0x68736D53) {
        uint8_t v1 = CPUReadByte(puser1 + 4);
        int8_t v1i = v1 - 1;
        CPUWriteByte(puser1 + 4, v1i);
        if (v1 <= 1) {
            uint8_t v2 = CPUReadByte(puser1 + 0xB); //11
            uint32_t base3 = 0x040000D2;
            CPUWriteByte(puser1 + 4, v2);

            CPUWriteHalfWord(base1 + 0x6, 0);
            CPUWriteHalfWord(base3, 0);
            CPUWriteHalfWord(base1 + 0x6, 0xB600);
            CPUWriteHalfWord(base3, 0xB600); //-18944
        }
    }
}

void BIOS_SndDriverVSyncOff() // 0x1878
{
    uint32_t const puser1 = CPUReadMemory(0x3007FF0); // 7FC0 + 0x30
    uint32_t user1 = CPUReadMemory(puser1);

    if (user1 == 0x68736D53 || user1 == 0x68736D54) {
        CPUWriteMemory(puser1, (++user1)); // this guard is common for funcs, unify

        CPUWriteHalfWord(base1 + 0x6, 0);
        CPUWriteHalfWord(base1 + 0x12, 0);
        CPUWriteByte(puser1 + 4, 0);

        uint32_t r1 = puser1 + 0x350;
        uint32_t r2 = 0x05000318;
        uint32_t sp = reg[13].I; //0x03003c94;

        CPUWriteMemory(sp, 0);
        BIOS_SndDriver_b4c(sp, r1, r2);

        user1 = CPUReadMemory(puser1); // 0x18aa
        CPUWriteMemory(puser1, (--user1)); // this ret is common among funcs
    }
    //0x18b0
}

// This functions is verified but lacks proper register settings before calling user func
// it might be that user func modifies or uses those?
// r7 should be puser1, r6 should be flags, ....
void BIOS_SndChannelClear() //0x1824
{
    uint32_t const puser1 = CPUReadMemory(0x3007FF0); // 7FC0 + 0x30
    uint32_t user1 = CPUReadMemory(puser1);

    if (user1 == 0x68736D53) {
        CPUWriteMemory(puser1, (++user1));
        uint32_t puser2 = puser1 + 0x7 + 0x49;

        int count = 12;
        while (count--) {
            CPUWriteByte(puser2, 0);
            puser2 += 0x40;
        }

        reg[4].I = CPUReadMemory(puser1 + 0x1c); //r5 -> some user thing
        if (reg[4].I != 0) {
            reg[3].I = 1; // r4 -> channel counter?
            int puser4 = puser1 + 0x2c;
            //reg[0].I = reg[3].I = 1; // r0 & r4 => 1

            while (reg[3].I <= 4) {
                // @todo does user func modify these?
                reg[0].I = reg[3].I << 24;
                reg[0].I >>= 24;

                // Get ptr to user func
                reg[1].I = CPUReadMemory(puser4);

                // @todo here we jump where r1 points; user func?
                // @todo might modify r6 also?

                // After call routines
                reg[3].I += 1; // r4 -> channel counter?
                reg[4].I += 0x40; // r5 -> some user thing
            }
            CPUWriteByte(reg[4].I, 0); // terminating record?
        }
        CPUWriteMemory(puser1, 0x68736D53);
    }
}

void BIOS_MidiKey2Freq()
{
#ifdef GBA_LOGGING
    if (systemVerbose & VERBOSE_SWI) {
        log("MidiKey2Freq: WaveData=%08x mk=%08x fp=%08x\n",
            reg[0].I,
            reg[1].I,
            reg[2].I);
    }
#endif
    int freq = CPUReadMemory(reg[0].I + 4);
    double tmp;
    tmp = ((double)(180 - reg[1].I)) - ((double)reg[2].I / 256.f);
    tmp = pow((double)2.f, tmp / 12.f);
    reg[0].I = (int)((double)freq / tmp);

#ifdef GBA_LOGGING
    if (systemVerbose & VERBOSE_SWI) {
        log("MidiKey2Freq: return %08x\n",
            reg[0].I);
    }
#endif
}

void BIOS_SndDriverJmpTableCopy()
{
#ifdef GBA_LOGGING
    if (systemVerbose & VERBOSE_SWI) {
        log("SndDriverJmpTableCopy: dest=%08x\n",
            reg[0].I);
    }
#endif
    for (int i = 0; i < 0x24; i++) {
        CPUWriteMemory(reg[0].I, 0x9c);
        reg[0].I += 4;
    }
}
