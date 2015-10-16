#include <math.h>
#include <memory.h>
#include <stdlib.h>

#include "GBA.h"
#include "bios.h"
#include "GBAinline.h"
#include "Globals.h"

s16 sineTable[256] = {
  (s16)0x0000, (s16)0x0192, (s16)0x0323, (s16)0x04B5, (s16)0x0645, (s16)0x07D5, (s16)0x0964, (s16)0x0AF1,
  (s16)0x0C7C, (s16)0x0E05, (s16)0x0F8C, (s16)0x1111, (s16)0x1294, (s16)0x1413, (s16)0x158F, (s16)0x1708,
  (s16)0x187D, (s16)0x19EF, (s16)0x1B5D, (s16)0x1CC6, (s16)0x1E2B, (s16)0x1F8B, (s16)0x20E7, (s16)0x223D,
  (s16)0x238E, (s16)0x24DA, (s16)0x261F, (s16)0x275F, (s16)0x2899, (s16)0x29CD, (s16)0x2AFA, (s16)0x2C21,
  (s16)0x2D41, (s16)0x2E5A, (s16)0x2F6B, (s16)0x3076, (s16)0x3179, (s16)0x3274, (s16)0x3367, (s16)0x3453,
  (s16)0x3536, (s16)0x3612, (s16)0x36E5, (s16)0x37AF, (s16)0x3871, (s16)0x392A, (s16)0x39DA, (s16)0x3A82,
  (s16)0x3B20, (s16)0x3BB6, (s16)0x3C42, (s16)0x3CC5, (s16)0x3D3E, (s16)0x3DAE, (s16)0x3E14, (s16)0x3E71,
  (s16)0x3EC5, (s16)0x3F0E, (s16)0x3F4E, (s16)0x3F84, (s16)0x3FB1, (s16)0x3FD3, (s16)0x3FEC, (s16)0x3FFB,
  (s16)0x4000, (s16)0x3FFB, (s16)0x3FEC, (s16)0x3FD3, (s16)0x3FB1, (s16)0x3F84, (s16)0x3F4E, (s16)0x3F0E,
  (s16)0x3EC5, (s16)0x3E71, (s16)0x3E14, (s16)0x3DAE, (s16)0x3D3E, (s16)0x3CC5, (s16)0x3C42, (s16)0x3BB6,
  (s16)0x3B20, (s16)0x3A82, (s16)0x39DA, (s16)0x392A, (s16)0x3871, (s16)0x37AF, (s16)0x36E5, (s16)0x3612,
  (s16)0x3536, (s16)0x3453, (s16)0x3367, (s16)0x3274, (s16)0x3179, (s16)0x3076, (s16)0x2F6B, (s16)0x2E5A,
  (s16)0x2D41, (s16)0x2C21, (s16)0x2AFA, (s16)0x29CD, (s16)0x2899, (s16)0x275F, (s16)0x261F, (s16)0x24DA,
  (s16)0x238E, (s16)0x223D, (s16)0x20E7, (s16)0x1F8B, (s16)0x1E2B, (s16)0x1CC6, (s16)0x1B5D, (s16)0x19EF,
  (s16)0x187D, (s16)0x1708, (s16)0x158F, (s16)0x1413, (s16)0x1294, (s16)0x1111, (s16)0x0F8C, (s16)0x0E05,
  (s16)0x0C7C, (s16)0x0AF1, (s16)0x0964, (s16)0x07D5, (s16)0x0645, (s16)0x04B5, (s16)0x0323, (s16)0x0192,
  (s16)0x0000, (s16)0xFE6E, (s16)0xFCDD, (s16)0xFB4B, (s16)0xF9BB, (s16)0xF82B, (s16)0xF69C, (s16)0xF50F,
  (s16)0xF384, (s16)0xF1FB, (s16)0xF074, (s16)0xEEEF, (s16)0xED6C, (s16)0xEBED, (s16)0xEA71, (s16)0xE8F8,
  (s16)0xE783, (s16)0xE611, (s16)0xE4A3, (s16)0xE33A, (s16)0xE1D5, (s16)0xE075, (s16)0xDF19, (s16)0xDDC3,
  (s16)0xDC72, (s16)0xDB26, (s16)0xD9E1, (s16)0xD8A1, (s16)0xD767, (s16)0xD633, (s16)0xD506, (s16)0xD3DF,
  (s16)0xD2BF, (s16)0xD1A6, (s16)0xD095, (s16)0xCF8A, (s16)0xCE87, (s16)0xCD8C, (s16)0xCC99, (s16)0xCBAD,
  (s16)0xCACA, (s16)0xC9EE, (s16)0xC91B, (s16)0xC851, (s16)0xC78F, (s16)0xC6D6, (s16)0xC626, (s16)0xC57E,
  (s16)0xC4E0, (s16)0xC44A, (s16)0xC3BE, (s16)0xC33B, (s16)0xC2C2, (s16)0xC252, (s16)0xC1EC, (s16)0xC18F,
  (s16)0xC13B, (s16)0xC0F2, (s16)0xC0B2, (s16)0xC07C, (s16)0xC04F, (s16)0xC02D, (s16)0xC014, (s16)0xC005,
  (s16)0xC000, (s16)0xC005, (s16)0xC014, (s16)0xC02D, (s16)0xC04F, (s16)0xC07C, (s16)0xC0B2, (s16)0xC0F2,
  (s16)0xC13B, (s16)0xC18F, (s16)0xC1EC, (s16)0xC252, (s16)0xC2C2, (s16)0xC33B, (s16)0xC3BE, (s16)0xC44A,
  (s16)0xC4E0, (s16)0xC57E, (s16)0xC626, (s16)0xC6D6, (s16)0xC78F, (s16)0xC851, (s16)0xC91B, (s16)0xC9EE,
  (s16)0xCACA, (s16)0xCBAD, (s16)0xCC99, (s16)0xCD8C, (s16)0xCE87, (s16)0xCF8A, (s16)0xD095, (s16)0xD1A6,
  (s16)0xD2BF, (s16)0xD3DF, (s16)0xD506, (s16)0xD633, (s16)0xD767, (s16)0xD8A1, (s16)0xD9E1, (s16)0xDB26,
  (s16)0xDC72, (s16)0xDDC3, (s16)0xDF19, (s16)0xE075, (s16)0xE1D5, (s16)0xE33A, (s16)0xE4A3, (s16)0xE611,
  (s16)0xE783, (s16)0xE8F8, (s16)0xEA71, (s16)0xEBED, (s16)0xED6C, (s16)0xEEEF, (s16)0xF074, (s16)0xF1FB,
  (s16)0xF384, (s16)0xF50F, (s16)0xF69C, (s16)0xF82B, (s16)0xF9BB, (s16)0xFB4B, (s16)0xFCDD, (s16)0xFE6E
};

void BIOS_ArcTan()
{
#ifdef GBA_LOGGING
  if(systemVerbose & VERBOSE_SWI) {
    log("ArcTan: %08x (VCOUNT=%2d)\n",
        reg[0].I,
        VCOUNT);
  }
#endif

  s32 a =  -(((s32)(reg[0].I*reg[0].I)) >> 14);
  s32 b = ((0xA9 * a) >> 14) + 0x390;
  b = ((b * a) >> 14) + 0x91C;
  b = ((b * a) >> 14) + 0xFB6;
  b = ((b * a) >> 14) + 0x16AA;
  b = ((b * a) >> 14) + 0x2081;
  b = ((b * a) >> 14) + 0x3651;
  b = ((b * a) >> 14) + 0xA2F9;
  a = ((s32)reg[0].I * b) >> 16;
  reg[0].I = a;

#ifdef GBA_LOGGING
  if(systemVerbose & VERBOSE_SWI) {
    log("ArcTan: return=%08x\n",
        reg[0].I);
  }
#endif
}

void BIOS_ArcTan2()
{
#ifdef GBA_LOGGING
  if(systemVerbose & VERBOSE_SWI) {
    log("ArcTan2: %08x,%08x (VCOUNT=%2d)\n",
        reg[0].I,
        reg[1].I,
        VCOUNT);
  }
#endif

  s32 x = reg[0].I;
  s32 y = reg[1].I;
  u32 res = 0;
  if (y == 0) {
    res = ((x>>16) & 0x8000);
  } else {
    if (x == 0) {
      res = ((y>>16) & 0x8000) + 0x4000;
    } else {
		if ((abs(x) > abs(y)) || ((abs(x) == abs(y)) && (!((x<0) && (y<0))))) {
        reg[1].I = x;
        reg[0].I = y << 14;
        BIOS_Div();
        BIOS_ArcTan();
        if (x < 0)
          res = 0x8000 + reg[0].I;
        else
          res = (((y>>16) & 0x8000)<<1) + reg[0].I;
      } else {
        reg[0].I = x << 14;
        BIOS_Div();
        BIOS_ArcTan();
        res = (0x4000 + ((y>>16) & 0x8000)) - reg[0].I;
      }
    }
  }
  reg[0].I = res;

#ifdef GBA_LOGGING
  if(systemVerbose & VERBOSE_SWI) {
    log("ArcTan2: return=%08x\n",
        reg[0].I);
  }
#endif
}

void BIOS_BitUnPack()
{
#ifdef GBA_LOGGING
  if(systemVerbose & VERBOSE_SWI) {
    log("BitUnPack: %08x,%08x,%08x (VCOUNT=%2d)\n",
        reg[0].I,
        reg[1].I,
        reg[2].I,
        VCOUNT);
  }
#endif

  u32 source = reg[0].I;
  u32 dest = reg[1].I;
  u32 header = reg[2].I;

  int len = CPUReadHalfWord(header);
    // check address
  if(((source & 0xe000000) == 0) ||
     ((source + len) & 0xe000000) == 0)
    return;

  int bits = CPUReadByte(header+2);
  int revbits = 8 - bits;
  // u32 value = 0;
  u32 base = CPUReadMemory(header+4);
  bool addBase = (base & 0x80000000) ? true : false;
  base &= 0x7fffffff;
  int dataSize = CPUReadByte(header+3);

  int data = 0;
  int bitwritecount = 0;
  while(1) {
    len -= 1;
    if(len < 0)
      break;
    int mask = 0xff >> revbits;
    u8 b = CPUReadByte(source);
    source++;
    int bitcount = 0;
    while(1) {
      if(bitcount >= 8)
        break;
      u32 d = b & mask;
      u32 temp = d >> bitcount;
      if(d || addBase) {
        temp += base;
      }
      data |= temp << bitwritecount;
      bitwritecount += dataSize;
      if(bitwritecount >= 32) {
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
  reg[0].I=0xBAAE187F;
}

void BIOS_BgAffineSet()
{
#ifdef GBA_LOGGING
  if(systemVerbose & VERBOSE_SWI) {
    log("BgAffineSet: %08x,%08x,%08x (VCOUNT=%2d)\n",
        reg[0].I,
        reg[1].I,
        reg[2].I,
        VCOUNT);
  }
#endif

  u32 src = reg[0].I;
  u32 dest = reg[1].I;
  int num = reg[2].I;

  for(int i = 0; i < num; i++) {
    s32 cx = CPUReadMemory(src);
    src+=4;
    s32 cy = CPUReadMemory(src);
    src+=4;
    s16 dispx = CPUReadHalfWord(src);
    src+=2;
    s16 dispy = CPUReadHalfWord(src);
    src+=2;
    s16 rx = CPUReadHalfWord(src);
    src+=2;
    s16 ry = CPUReadHalfWord(src);
    src+=2;
    u16 theta = CPUReadHalfWord(src)>>8;
    src+=4; // keep structure alignment
    s32 a = sineTable[(theta+0x40)&255];
    s32 b = sineTable[theta];

    s16 dx =  (rx * a)>>14;
    s16 dmx = (rx * b)>>14;
    s16 dy =  (ry * b)>>14;
    s16 dmy = (ry * a)>>14;

    CPUWriteHalfWord(dest, dx);
    dest += 2;
    CPUWriteHalfWord(dest, -dmx);
    dest += 2;
    CPUWriteHalfWord(dest, dy);
    dest += 2;
    CPUWriteHalfWord(dest, dmy);
    dest += 2;

    s32 startx = cx - dx * dispx + dmx * dispy;
    s32 starty = cy - dy * dispx - dmy * dispy;

    CPUWriteMemory(dest, startx);
    dest += 4;
    CPUWriteMemory(dest, starty);
    dest += 4;
  }
}

void BIOS_CpuSet()
{
#ifdef GBA_LOGGING
  if(systemVerbose & VERBOSE_SWI) {
    log("CpuSet: 0x%08x,0x%08x,0x%08x (VCOUNT=%d)\n", reg[0].I, reg[1].I,
        reg[2].I, VCOUNT);
  }
#endif

  u32 source = reg[0].I;
  u32 dest = reg[1].I;
  u32 cnt = reg[2].I;

  if(((source & 0xe000000) == 0) ||
     ((source + (((cnt << 11)>>9) & 0x1fffff)) & 0xe000000) == 0)
    return;

  int count = cnt & 0x1FFFFF;

  // 32-bit ?
  if((cnt >> 26) & 1) {
    // needed for 32-bit mode!
    source &= 0xFFFFFFFC;
    dest &= 0xFFFFFFFC;
    // fill ?
    if((cnt >> 24) & 1) {
        u32 value = (source>0x0EFFFFFF ? 0x1CAD1CAD : CPUReadMemory(source));
      while(count) {
        CPUWriteMemory(dest, value);
        dest += 4;
        count--;
      }
    } else {
      // copy
      while(count) {
        CPUWriteMemory(dest, (source>0x0EFFFFFF ? 0x1CAD1CAD : CPUReadMemory(source)));
        source += 4;
        dest += 4;
        count--;
      }
    }
  } else {
    // 16-bit fill?
    if((cnt >> 24) & 1) {
      u16 value = (source>0x0EFFFFFF ? 0x1CAD : CPUReadHalfWord(source));
      while(count) {
        CPUWriteHalfWord(dest, value);
        dest += 2;
        count--;
      }
    } else {
      // copy
      while(count) {
        CPUWriteHalfWord(dest, (source>0x0EFFFFFF ? 0x1CAD : CPUReadHalfWord(source)));
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
  if(systemVerbose & VERBOSE_SWI) {
    log("CpuFastSet: 0x%08x,0x%08x,0x%08x (VCOUNT=%d)\n", reg[0].I, reg[1].I,
        reg[2].I, VCOUNT);
  }
#endif

  u32 source = reg[0].I;
  u32 dest = reg[1].I;
  u32 cnt = reg[2].I;

  if(((source & 0xe000000) == 0) ||
     ((source + (((cnt << 11)>>9) & 0x1fffff)) & 0xe000000) == 0)
    return;

  // needed for 32-bit mode!
  source &= 0xFFFFFFFC;
  dest &= 0xFFFFFFFC;

  int count = cnt & 0x1FFFFF;

  // fill?
  if((cnt >> 24) & 1) {
    while(count > 0) {
      // BIOS always transfers 32 bytes at a time
      u32 value = (source>0x0EFFFFFF ? 0xBAFFFFFB : CPUReadMemory(source));
      for(int i = 0; i < 8; i++) {
        CPUWriteMemory(dest, value);
        dest += 4;
      }
      count -= 8;
    }
  } else {
    // copy
    while(count > 0) {
      // BIOS always transfers 32 bytes at a time
      for(int i = 0; i < 8; i++) {
        CPUWriteMemory(dest, (source>0x0EFFFFFF ? 0xBAFFFFFB :CPUReadMemory(source)));
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
  if(systemVerbose & VERBOSE_SWI) {
    log("Diff8bitUnFilterWram: 0x%08x,0x%08x (VCOUNT=%d)\n", reg[0].I,
        reg[1].I, VCOUNT);
  }
#endif

  u32 source = reg[0].I;
  u32 dest = reg[1].I;

  u32 header = CPUReadMemory(source);
  source += 4;

  if(((source & 0xe000000) == 0) ||
     (((source + ((header >> 8) & 0x1fffff)) & 0xe000000) == 0))
    return;

  int len = header >> 8;

  u8 data = CPUReadByte(source++);
  CPUWriteByte(dest++, data);
  len--;

  while(len > 0) {
    u8 diff = CPUReadByte(source++);
    data += diff;
    CPUWriteByte(dest++, data);
    len--;
  }
}

void BIOS_Diff8bitUnFilterVram()
{
#ifdef GBA_LOGGING
  if(systemVerbose & VERBOSE_SWI) {
    log("Diff8bitUnFilterVram: 0x%08x,0x%08x (VCOUNT=%d)\n", reg[0].I,
        reg[1].I, VCOUNT);
  }
#endif

  u32 source = reg[0].I;
  u32 dest = reg[1].I;

  u32 header = CPUReadMemory(source);
  source += 4;

  if(((source & 0xe000000) == 0) ||
     ((source + ((header >> 8) & 0x1fffff)) & 0xe000000) == 0)
    return;

  int len = header >> 8;

  u8 data = CPUReadByte(source++);
  u16 writeData = data;
  int shift = 8;
  int bytes = 1;

  while(len >= 2) {
    u8 diff = CPUReadByte(source++);
    data += diff;
    writeData |= (data << shift);
    bytes++;
    shift += 8;
    if(bytes == 2) {
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
  if(systemVerbose & VERBOSE_SWI) {
    log("Diff16bitUnFilter: 0x%08x,0x%08x (VCOUNT=%d)\n", reg[0].I,
        reg[1].I, VCOUNT);
  }
#endif

  u32 source = reg[0].I;
  u32 dest = reg[1].I;

  u32 header = CPUReadMemory(source);
  source += 4;

  if(((source & 0xe000000) == 0) ||
     ((source + ((header >> 8) & 0x1fffff)) & 0xe000000) == 0)
    return;

  int len = header >> 8;

  u16 data = CPUReadHalfWord(source);
  source += 2;
  CPUWriteHalfWord(dest, data);
  dest += 2;
  len -= 2;

  while(len >= 2) {
    u16 diff = CPUReadHalfWord(source);
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
  if(systemVerbose & VERBOSE_SWI) {
    log("Div: 0x%08x,0x%08x (VCOUNT=%d)\n",
        reg[0].I,
        reg[1].I,
        VCOUNT);
  }
#endif

  int number = reg[0].I;
  int denom = reg[1].I;

  if(denom != 0) {
    reg[0].I = number / denom;
    reg[1].I = number % denom;
    s32 temp = (s32)reg[0].I;
    reg[3].I = temp < 0 ? (u32)-temp : (u32)temp;
  }
#ifdef GBA_LOGGING
  if(systemVerbose & VERBOSE_SWI) {
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
  if(systemVerbose & VERBOSE_SWI) {
    log("DivARM: 0x%08x, (VCOUNT=%d)\n",
        reg[0].I,
        VCOUNT);
  }
#endif

  u32 temp = reg[0].I;
  reg[0].I = reg[1].I;
  reg[1].I = temp;
  BIOS_Div();
}

void BIOS_HuffUnComp()
{
#ifdef GBA_LOGGING
  if(systemVerbose & VERBOSE_SWI) {
    log("HuffUnComp: 0x%08x,0x%08x (VCOUNT=%d)\n",
        reg[0].I,
        reg[1].I,
        VCOUNT);
  }
#endif

  u32 source = reg[0].I;
  u32 dest = reg[1].I;

  u32 header = CPUReadMemory(source);
  source += 4;

  if(((source & 0xe000000) == 0) ||
     ((source + ((header >> 8) & 0x1fffff)) & 0xe000000) == 0)
    return;

  u8 treeSize = CPUReadByte(source++);

  u32 treeStart = source;

  source += ((treeSize+1)<<1)-1; // minus because we already skipped one byte

  int len = header >> 8;

  u32 mask = 0x80000000;
  u32 data = CPUReadMemory(source);
  source += 4;

  int pos = 0;
  u8 rootNode = CPUReadByte(treeStart);
  u8 currentNode = rootNode;
  bool writeData = false;
  int byteShift = 0;
  int byteCount = 0;
  u32 writeValue = 0;

  if((header & 0x0F) == 8) {
    while(len > 0) {
      // take left
      if(pos == 0)
        pos++;
      else
        pos += (((currentNode & 0x3F)+1)<<1);

      if(data & mask) {
        // right
        if(currentNode & 0x40)
          writeData = true;
        currentNode = CPUReadByte(treeStart+pos+1);
      } else {
        // left
        if(currentNode & 0x80)
          writeData = true;
        currentNode = CPUReadByte(treeStart+pos);
      }

      if(writeData) {
        writeValue |= (currentNode << byteShift);
        byteCount++;
        byteShift += 8;

        pos = 0;
        currentNode = rootNode;
        writeData = false;

        if(byteCount == 4) {
          byteCount = 0;
          byteShift = 0;
          CPUWriteMemory(dest, writeValue);
          writeValue = 0;
          dest += 4;
          len -= 4;
        }
      }
      mask >>= 1;
      if(mask == 0) {
        mask = 0x80000000;
        data = CPUReadMemory(source);
        source += 4;
      }
    }
  } else {
    int halfLen = 0;
    int value = 0;
    while(len > 0) {
      // take left
      if(pos == 0)
        pos++;
      else
        pos += (((currentNode & 0x3F)+1)<<1);

      if((data & mask)) {
        // right
        if(currentNode & 0x40)
          writeData = true;
        currentNode = CPUReadByte(treeStart+pos+1);
      } else {
        // left
        if(currentNode & 0x80)
          writeData = true;
        currentNode = CPUReadByte(treeStart+pos);
      }

      if(writeData) {
        if(halfLen == 0)
          value |= currentNode;
        else
          value |= (currentNode<<4);

        halfLen += 4;
        if(halfLen == 8) {
          writeValue |= (value << byteShift);
          byteCount++;
          byteShift += 8;

          halfLen = 0;
          value = 0;

          if(byteCount == 4) {
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
      if(mask == 0) {
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
  if(systemVerbose & VERBOSE_SWI) {
    log("LZ77UnCompVram: 0x%08x,0x%08x (VCOUNT=%d)\n",
        reg[0].I,
        reg[1].I,
        VCOUNT);
  }
#endif

  u32 source = reg[0].I;
  u32 dest = reg[1].I;

  u32 header = CPUReadMemory(source);
  source += 4;

  if(((source & 0xe000000) == 0) ||
     ((source + ((header >> 8) & 0x1fffff)) & 0xe000000) == 0)
    return;

  int byteCount = 0;
  int byteShift = 0;
  u32 writeValue = 0;

  int len = header >> 8;

  while(len > 0) {
    u8 d = CPUReadByte(source++);

    if(d) {
      for(int i = 0; i < 8; i++) {
        if(d & 0x80) {
          u16 data = CPUReadByte(source++) << 8;
          data |= CPUReadByte(source++);
          int length = (data >> 12) + 3;
          int offset = (data & 0x0FFF);
          u32 windowOffset = dest + byteCount - offset - 1;
          for(int i2 = 0; i2 < length; i2++) {
            writeValue |= (CPUReadByte(windowOffset++) << byteShift);
            byteShift += 8;
            byteCount++;

            if(byteCount == 2) {
              CPUWriteHalfWord(dest, writeValue);
              dest += 2;
              byteCount = 0;
              byteShift = 0;
              writeValue = 0;
            }
            len--;
            if(len == 0)
              return;
          }
        } else {
          writeValue |= (CPUReadByte(source++) << byteShift);
          byteShift += 8;
          byteCount++;
          if(byteCount == 2) {
            CPUWriteHalfWord(dest, writeValue);
            dest += 2;
            byteCount = 0;
            byteShift = 0;
            writeValue = 0;
          }
          len--;
          if(len == 0)
            return;
        }
        d <<= 1;
      }
    } else {
      for(int i = 0; i < 8; i++) {
        writeValue |= (CPUReadByte(source++) << byteShift);
        byteShift += 8;
        byteCount++;
        if(byteCount == 2) {
          CPUWriteHalfWord(dest, writeValue);
          dest += 2;
          byteShift = 0;
          byteCount = 0;
          writeValue = 0;
        }
        len--;
        if(len == 0)
          return;
      }
    }
  }
}

void BIOS_LZ77UnCompWram()
{
#ifdef GBA_LOGGING
  if(systemVerbose & VERBOSE_SWI) {
    log("LZ77UnCompWram: 0x%08x,0x%08x (VCOUNT=%d)\n", reg[0].I, reg[1].I,
        VCOUNT);
  }
#endif

  u32 source = reg[0].I;
  u32 dest = reg[1].I;

  u32 header = CPUReadMemory(source);
  source += 4;

  if(((source & 0xe000000) == 0) ||
     ((source + ((header >> 8) & 0x1fffff)) & 0xe000000) == 0)
    return;

  int len = header >> 8;

  while(len > 0) {
    u8 d = CPUReadByte(source++);

    if(d) {
      for(int i = 0; i < 8; i++) {
        if(d & 0x80) {
          u16 data = CPUReadByte(source++) << 8;
          data |= CPUReadByte(source++);
          int length = (data >> 12) + 3;
          int offset = (data & 0x0FFF);
          u32 windowOffset = dest - offset - 1;
          for(int i2 = 0; i2 < length; i2++) {
            CPUWriteByte(dest++, CPUReadByte(windowOffset++));
            len--;
            if(len == 0)
              return;
          }
        } else {
          CPUWriteByte(dest++, CPUReadByte(source++));
          len--;
          if(len == 0)
            return;
        }
        d <<= 1;
      }
    } else {
      for(int i = 0; i < 8; i++) {
        CPUWriteByte(dest++, CPUReadByte(source++));
        len--;
        if(len == 0)
          return;
      }
    }
  }
}

void BIOS_ObjAffineSet()
{
#ifdef GBA_LOGGING
  if(systemVerbose & VERBOSE_SWI) {
    log("ObjAffineSet: 0x%08x,0x%08x,0x%08x,0x%08x (VCOUNT=%d)\n",
        reg[0].I,
        reg[1].I,
        reg[2].I,
        reg[3].I,
        VCOUNT);
  }
#endif

  u32 src = reg[0].I;
  u32 dest = reg[1].I;
  int num = reg[2].I;
  int offset = reg[3].I;

  for(int i = 0; i < num; i++) {
    s16 rx = CPUReadHalfWord(src);
    src+=2;
    s16 ry = CPUReadHalfWord(src);
    src+=2;
    u16 theta = CPUReadHalfWord(src)>>8;
    src+=4; // keep structure alignment

    s32 a = (s32)sineTable[(theta+0x40)&255];
    s32 b = (s32)sineTable[theta];

    s16 dx =  ((s32)rx * a)>>14;
    s16 dmx = ((s32)rx * b)>>14;
    s16 dy =  ((s32)ry * b)>>14;
    s16 dmy = ((s32)ry * a)>>14;

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

void BIOS_RegisterRamReset(u32 flags)
{
  // no need to trace here. this is only called directly from GBA.cpp
  // to emulate bios initialization

  CPUUpdateRegister(0x0, 0x80);

  if(flags) {
    if(flags & 0x01) {
      // clear work RAM
      memset(workRAM, 0, 0x40000);
    }
    if(flags & 0x02) {
      // clear internal RAM
      memset(internalRAM, 0, 0x7e00); // don't clear 0x7e00-0x7fff
    }
    if(flags & 0x04) {
      // clear palette RAM
      memset(paletteRAM, 0, 0x400);
    }
    if(flags & 0x08) {
      // clear VRAM
      memset(vram, 0, 0x18000);
    }
    if(flags & 0x10) {
      // clean OAM
      memset(oam, 0, 0x400);
    }

    if(flags & 0x80) {
      int i;
      for(i = 0; i < 0x10; i++)
        CPUUpdateRegister(0x200+i*2, 0);

      for(i = 0; i < 0xF; i++)
        CPUUpdateRegister(0x4+i*2, 0);

      for(i = 0; i < 0x20; i++)
        CPUUpdateRegister(0x20+i*2, 0);

      for(i = 0; i < 0x18; i++)
        CPUUpdateRegister(0xb0+i*2, 0);

      CPUUpdateRegister(0x130, 0);
      CPUUpdateRegister(0x20, 0x100);
      CPUUpdateRegister(0x30, 0x100);
      CPUUpdateRegister(0x26, 0x100);
      CPUUpdateRegister(0x36, 0x100);
    }

    if(flags & 0x20) {
      int i;
      for(i = 0; i < 8; i++)
        CPUUpdateRegister(0x110+i*2, 0);
      CPUUpdateRegister(0x134, 0x8000);
      for(i = 0; i < 7; i++)
        CPUUpdateRegister(0x140+i*2, 0);
    }

    if(flags & 0x40) {
      int i;
      CPUWriteByte(0x4000084, 0);
      CPUWriteByte(0x4000084, 0x80);
      CPUWriteMemory(0x4000080, 0x880e0000);
      CPUUpdateRegister(0x88, CPUReadHalfWord(0x4000088)&0x3ff);
      CPUWriteByte(0x4000070, 0x70);
      for(i = 0; i < 8; i++)
        CPUUpdateRegister(0x90+i*2, 0);
      CPUWriteByte(0x4000070, 0);
      for(i = 0; i < 8; i++)
        CPUUpdateRegister(0x90+i*2, 0);
      CPUWriteByte(0x4000084, 0);
    }
  }
}

void BIOS_RegisterRamReset()
{
#ifdef GBA_LOGGING
  if(systemVerbose & VERBOSE_SWI) {
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
  if(systemVerbose & VERBOSE_SWI) {
    log("RLUnCompVram: 0x%08x,0x%08x (VCOUNT=%d)\n",
        reg[0].I,
        reg[1].I,
        VCOUNT);
  }
#endif

  u32 source = reg[0].I;
  u32 dest = reg[1].I;

  u32 header = CPUReadMemory(source & 0xFFFFFFFC);
  source += 4;

  if(((source & 0xe000000) == 0) ||
     ((source + ((header >> 8) & 0x1fffff)) & 0xe000000) == 0)
    return;

  int len = header >> 8;
  int byteCount = 0;
  int byteShift = 0;
  u32 writeValue = 0;

  while(len > 0) {
    u8 d = CPUReadByte(source++);
    int l = d & 0x7F;
    if(d & 0x80) {
      u8 data = CPUReadByte(source++);
      l += 3;
      for(int i = 0;i < l; i++) {
        writeValue |= (data << byteShift);
        byteShift += 8;
        byteCount++;

        if(byteCount == 2) {
          CPUWriteHalfWord(dest, writeValue);
          dest += 2;
          byteCount = 0;
          byteShift = 0;
          writeValue = 0;
        }
        len--;
        if(len == 0)
          return;
      }
    } else {
      l++;
      for(int i = 0; i < l; i++) {
        writeValue |= (CPUReadByte(source++) << byteShift);
        byteShift += 8;
        byteCount++;
        if(byteCount == 2) {
          CPUWriteHalfWord(dest, writeValue);
          dest += 2;
          byteCount = 0;
          byteShift = 0;
          writeValue = 0;
        }
        len--;
        if(len == 0)
          return;
      }
    }
  }
}

void BIOS_RLUnCompWram()
{
#ifdef GBA_LOGGING
  if(systemVerbose & VERBOSE_SWI) {
    log("RLUnCompWram: 0x%08x,0x%08x (VCOUNT=%d)\n",
        reg[0].I,
        reg[1].I,
        VCOUNT);
  }
#endif

  u32 source = reg[0].I;
  u32 dest = reg[1].I;

  u32 header = CPUReadMemory(source & 0xFFFFFFFC);
  source += 4;

  if(((source & 0xe000000) == 0) ||
     ((source + ((header >> 8) & 0x1fffff)) & 0xe000000) == 0)
    return;

  int len = header >> 8;

  while(len > 0) {
    u8 d = CPUReadByte(source++);
    int l = d & 0x7F;
    if(d & 0x80) {
      u8 data = CPUReadByte(source++);
      l += 3;
      for(int i = 0;i < l; i++) {
        CPUWriteByte(dest++, data);
        len--;
        if(len == 0)
          return;
      }
    } else {
      l++;
      for(int i = 0; i < l; i++) {
        CPUWriteByte(dest++,  CPUReadByte(source++));
        len--;
        if(len == 0)
          return;
      }
    }
  }
}

void BIOS_SoftReset()
{
#ifdef GBA_LOGGING
  if(systemVerbose & VERBOSE_SWI) {
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
  u8 b = internalRAM[0x7ffa];

  memset(&internalRAM[0x7e00], 0, 0x200);

  if(b) {
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
  if(systemVerbose & VERBOSE_SWI) {
    log("Sqrt: %08x (VCOUNT=%2d)\n",
        reg[0].I,
        VCOUNT);
  }
#endif
  reg[0].I = (u32)sqrt((double)reg[0].I);
#ifdef GBA_LOGGING
  if(systemVerbose & VERBOSE_SWI) {
    log("Sqrt: return=%08x\n",
        reg[0].I);
  }
#endif
}

#define ADBITS_MASK 0x3FU
u32 const base1 = 0x040000C0;
u32 const base2 = 0x04000080;

static bool BIOS_SndDriver_ba4(u32 r0, u32 r4r12) // 0xba4
{
    if (r4r12)
    {
        r4r12 = r4r12 & ~0xFE000000;
        r4r12 += r0;
        if (!((r0 & 0x0E000000) && (r4r12 & 0x0E000000)))
            return true;
    }

    return false;
}

static void BIOS_SndDriver_b4c(u32 r0, u32 r1, u32 r2) // 0xb4c
{
    // @r4
    u32 r4 = 4 * r2 & 0x7FFFFF;
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
    if ( !ok )
    {
      u32 r5 = 0; //v8
      if ( r2 >= (1<<(27-1)) ) //v6
      {
        r5 = r1 + r4;
        if ( r2 >= (1<<(25-1)) )
        {
          u32 r3 = CPUReadMemory(r0);
          while ( r1 < r5 )
          {
            CPUWriteMemory(r1, r3);
            r1 += 4;
          }
        }
        else // @todo 0b6e
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
      }
      else // @todo 0b78
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

static s32 BIOS_SndDriver_3e4(u32 const r0a, u32 const r1a) // 0x3e4
{
    s32 r0 = r1a;
    s32 r1 = r0a;
    u32 v5 = r0 & 0x80000000;
    s32 r12;
    s32 r2;
    bool gtr;

    if ( (r1 & 0x80000000 & 0x80000000) != 0 )
        r1 = -r1;
    r12 = r0; //v5 ^ (r0 >> 32);
    if ( r0 < 0 )
        r0 = -r0;
    r2 = r1;

    do
    {
        gtr = (r2 >= r0 >> 1);
        if ( r2 <= r0 >> 1 )
            r2 *= 2;
    } while ( !gtr );

    while ( 1 )
    {
        v5 += (r0 >= (u32)r2) + v5;
        if ( r0 >= (u32)r2 )
            r0 -= r2;
        if ( r2 == r1 )
            break;
        r2 = (u32)r2 >> 1;
    }

    if ( !(r12 << 1) )
        return -v5;
    else
        return v5;
}

static void BIOS_SndDriverSub1(u32 p1) // 0x170a
{
    u8 local1 = (p1 & 0x000F0000) >> 16; // param is r0
    u32 const puser1 = CPUReadMemory(0x3007FF0); // 7FC0 + 0x30

    // Store something
    CPUWriteByte(puser1+8, local1);

    u32 r0 = (0x31e8 + (local1 << 1)) - 0x20;

    // @todo read from bios region
    if (r0 == 0x31D0)
    {
        r0 = 0xE0;
    }
    else if (r0 == 0x31E0)
    {
        r0 = 0x2C0;
    }
    else r0 = CPUReadHalfWord(r0+0x1E);
    CPUWriteMemory(puser1+0x10, r0);

    u32 r1 = 0x630;
    u32 const r4 = r0;

	// 0x172c
    r0 = BIOS_SndDriver_3e4(r0, r1);
	CPUWriteByte(puser1+0xB, r0);

	u32 x = 0x91d1b * r4;
	r1 = x + 0x1388;
	r0 = 0x1388 << 1;
	r0 = BIOS_SndDriver_3e4(r0, r1);
	CPUWriteMemory(puser1+0x14, r0);

	r1 = 1 << 24;
	r0 = BIOS_SndDriver_3e4(r0, r1) + 1;
	r0 /= 2;
	CPUWriteMemory(puser1+0x18, r0);

	// 0x1750
	u32 r4basesnd = 0x4000100;
	r0 = r4;
	r1 = 0x44940;
	CPUWriteHalfWord(r4basesnd+2, 0);
	r0 = BIOS_SndDriver_3e4(r0, r1);
	r0 = (1<<16)-r0;
	CPUWriteHalfWord(r4basesnd+0, r0);

	// sub 0x18c8 is unrolled here
	r1 = 0x5b << 9;
	CPUWriteHalfWord(base1+6 , r1);
	CPUWriteHalfWord(base1+12 , r1);

	// 0x176a, @todo busy loop here
	r0 = 0x4000000;
	//do
	{
		r1 = CPUReadByte(r0+6);
	} //while (r1 != 0x9F);

	CPUWriteHalfWord(r4basesnd+2, 0x80);
}

void BIOS_SndDriverInit() // 0x166a
{
#ifdef GBA_LOGGING
  if(systemVerbose & VERBOSE_SWI) {
    log("SndDriverInit: WaveData=%08x mk=%08x fp=%08x\n",
        reg[0].I,
        reg[1].I,
        reg[2].I);
  }
#endif

	// 7FC0 + 0x30
    u32 const puser1 = 0x3007FF0;
    u32 const user1 = reg[0].I;

    u32 base3 = 0x040000BC;
    //u32 base4 = 0x03007FF0;

    CPUWriteHalfWord(base1+6 , 0);
    CPUWriteHalfWord(base1+12, 0);

    CPUWriteHalfWord(base2+4, 0x8F);
    CPUWriteHalfWord(base2+2, 0xA90E);

    u16 val9 = CPUReadHalfWord(base2+9);
    CPUWriteHalfWord(base2+9, val9&ADBITS_MASK); // DA?

    CPUWriteMemory(base3+0, (user1 + 0x350)); //0x350, 640int
    CPUWriteMemory(base1+0, 0x40000A0);
    CPUWriteMemory(base1+8, 2224); //0x980
    CPUWriteMemory(base1+12, 0x40000A4);
    CPUWriteMemory(puser1, user1);

    u32 const r2 = 0x050003EC;
    u32 const sp = reg[13].I; // 0x03003c98;
    CPUWriteMemory(sp, 0);
	BIOS_SndDriver_b4c(sp, user1, r2);

	// 0x16b0
	CPUWriteByte(user1+0x6, 0x8);
	CPUWriteByte(user1+0x7, 0xF);
	CPUWriteMemory(user1+0x38, 0x2425);
	CPUWriteMemory(user1+0x28, 0x1709);
	CPUWriteMemory(user1+0x2C, 0x1709);
	CPUWriteMemory(user1+0x30, 0x1709);
	CPUWriteMemory(user1+0x3C, 0x1709);
	CPUWriteMemory(user1+0x34, 0x3738);

	BIOS_SndDriverSub1(0x40000);

    CPUWriteMemory(user1, 0x68736D53); // this ret is common among funcs
}

void BIOS_SndDriverMode() //0x179c
{
    u32 input = reg[0].I;
    u32 const puser1 = CPUReadMemory(0x3007FF0); // 7FC0 + 0x30
    u32 user1 = CPUReadMemory(puser1);

    if ( user1 == 0x68736D53 )
    {
        CPUWriteMemory(puser1, (++user1)); // this guard is common for funcs, unify

        // reverb values at bits 0...7
        u8 revb = (input & 0xFF);
        if ( revb )
        {
            revb>>=1;  // shift out the 7th enable bit
            CPUWriteByte(puser1+5, revb);
        }
        // direct sound multi channels at bits 8...11
        u8 chc = (input & 0xF00)>>8;
        if ( chc > 0 )
        {
            CPUWriteByte(puser1+6, chc);
            u32 puser2 = puser1 + 7 + 0x49;
            int count=12;
            while (count--)
            {
                CPUWriteByte(puser2, 0);
                puser2+=0x40;
            }
        }
        // direct sound master volume at bits 12...15
        u8 chv = (input & 0xF000)>>12;
        if ( chv > 0 )
        {
            CPUWriteByte(puser1+7, chv);
        }
        // DA converter bits at bits 20...23
        u32 dab = (input & 0x00B00000);
        if ( dab )
        {
            dab &= 0x00300000;
            dab >>= 0xE;
            u8 adv = CPUReadByte(puser1+9) & ADBITS_MASK; // @todo verify offset
            dab |= adv;
            CPUWriteByte(puser1+9, dab);
        }
        // Playback frequency at bits 16...19
        u32 pbf = (input & 0x000F0000);
        if ( pbf )
        {
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
    u32 const puser1 = CPUReadMemory(0x3007FF0); // 7FC0 + 0x30, //@+sp14
    u32 user1 = CPUReadMemory(puser1);

    if ( user1 != 0x68736D53 )
        return;

    // main
    CPUWriteMemory(puser1, (++user1)); // this guard is common for funcs, unify

    int const user2 = CPUReadMemory(puser1 + 0x20);
    if ( user2 )
    {
        int const par1 = CPUReadMemory(puser1 + 0x24);
        // Call 0x2102  sub_16A8 - -> param r1
    }

	int const userfunc = CPUReadMemory(puser1 + 0x28);
	switch (userfunc)
	{
		case 0x1709: //phantasy star
        default:
			break;
	}

    u32 const v2 = CPUReadMemory(puser1 + 0x10); //r8
    u8 const user4 = CPUReadByte(puser1+4) - 1;
	u32 user41 = 0;

    if ( user4 > 0 )
    {
        user41 = CPUReadByte(puser1 + 0x0B);
        user41 -= user4;
        user41 *= v2;
    }

    u32 r5;
	u32 const r5c = puser1 + 0x350 + user41; //r5 @sp+8
	int user6 = r5c + 0x630; //r6
    int user5 = CPUReadByte(puser1 + 0x5); //r3

    if ( user5 )
    {
        // @todo 0x1e1a
    }
    else // 0x1e74
    {
		r5 = r5c;
        int count = v2>>4; //r1...v13
        if ( !(v2 >> 3) )
        {
            CPUWriteMemory(r5, 0);
            CPUWriteMemory(user6, 0);
            r5+=4; user6+=4;
        }
        if ( !(v2 >> 1)  ) //0x1e7c
        {
            CPUWriteMemory(r5, 0);
            CPUWriteMemory(user6, 0);
            r5+=4; user6+=4;

            CPUWriteMemory(r5, 0);
            CPUWriteMemory(user6, 0);
            r5+=4; user6+=4;
        }
        do // 0x1e8e
        {
            // @todo optimize this memset
            CPUWriteMemory(r5, 0);
            CPUWriteMemory(user6, 0);
            r5+=4; user6+=4;

            CPUWriteMemory(r5, 0);
            CPUWriteMemory(user6, 0);
            r5+=4; user6+=4;

            CPUWriteMemory(r5, 0);
            CPUWriteMemory(user6, 0);
            r5+=4; user6+=4;

            CPUWriteMemory(r5, 0);
            CPUWriteMemory(user6, 0);
            r5+=4; user6+=4;

            count -= 1;
        } while (count > 0);
    }

    //0x1ea2
    u32 r4 = puser1; // apparenty ch ptr?
    int r9 = CPUReadMemory(r4+0x14);
    int r12 = CPUReadMemory(r4+0x18);
    u32 i = CPUReadByte(r4+0x6);

    for (r4+=0x10; i > 0; i-- )
    {
        r4+=0x40;
/*lbl_0x1eb0:*/
        u32 r3 = CPUReadMemory(r4+0x24);
        u8 r6 = CPUReadByte(r4);

        if ( (r6 & 0xC7) == 0 ) // 0x1ebc
            continue; //goto lbl_20e4;
		if ( (r6 & 0x80) && ((r6 & 0x40) == 0) ) // 0x1ec4
        {
            r6 = 0x3;
			CPUWriteByte(r4, r6);
			CPUWriteMemory(r4+0x28, r3+0x10);

			int r0t1 = CPUReadMemory(r3+0xC);
			CPUWriteMemory(r4+0x18, r0t1);

			r5=0;
			CPUWriteByte(r4+0x9, 0);
			CPUWriteMemory(r4+0x1c, 0);

			u8 r2a = CPUReadByte(r3+0x3); // seems to be LABEL_20e4
			if ((r2a & 0xC0)) // 1ee4
			{

			}
            goto lbl_0x1f46;
        }
		else
		{
//lbl_0x1eee:
			r5 = CPUReadByte(r4+0x9); //
			if ( (r6 & 0x4) != 0) // @todo 0x1ef4
			{

			}

			if ( (r6 & 0x40) != 0) // @todo 0x1f08
			{

			}

			if ( (r6 & 0x03) == 2) // 0x1f2a
			{
				u8 mul1 = CPUReadByte(r4+0x5);
				r5*=mul1;
				r5>>=8;

				u8 cmp1 = CPUReadByte(r4+0x6);
				if (r5 <= cmp1)
				{
                    r5=cmp1;
                    // @todo beq @ 0x1f3a -> ??
					r6--;
					CPUWriteByte(r4, r6);
				}
			}
			else if ( (r6 & 0x03) == 3) // 0x1f44
			{
lbl_0x1f46:		//@todo check if there is really another path to here
				u8 add1 = CPUReadByte(r4+0x4);
				r5+=add1;

				if (r5>=0xff)
				{
					r6--;
					r5=0xff;
					CPUWriteByte(r4, r6);
				}
			}
		}
		{
//lbl_0x1f54:
			CPUWriteByte(r4+0x9, r5);

			u32 user0 = CPUReadByte(puser1+0x7); // @sp+10
			user0++;
			user0*=r5;
			r5=user0>>4;

			user0 = CPUReadByte(r4+0x2);
			user0*=r5;
			user0>>=8;
			CPUWriteByte(r4+0xA, user0);

			user0 = CPUReadByte(r4+0x3);
			user0*=r5;
			user0>>=8;
			CPUWriteByte(r4+0xB, user0);

            user0 = r6&0x10;
			if ( user0 != 0) // @todo 0x1f76
			{

			}

			r5 = r5c; // @todo below r5 is used and updated
			u32 r2 = CPUReadMemory(r4+0x18);
			r3 = CPUReadMemory(r4+0x28);

			u32 r8 = v2;

			u8 r10 = CPUReadByte(r4+0xA);
			u8 r11 = CPUReadByte(r4+0xB);
			u8 r0a = CPUReadByte(r4+0x1);
			if ((r0a & 8)) //@todo 0x1fa8
			{

			}

			u32 r7 = CPUReadMemory(r4+0x1c);
			u32 r14 = CPUReadMemory(r4+0x20);

lbl_0x2004://	LABEL_52:
			while ( r7 >= 4 * r9 )
			{
				if ( r2 <= 4 )// @todo 0x2008, no phant
					goto lbl_204c;
				r2 -= 4;
				r3 += 4;
				r7 -= 4 * r9;
			}
			if ( r7 >= 2 * r9 )
			{
				if ( r2 <= 2 ) // @todo 0x2008, no phant
					goto lbl_204c;
				r2 -= 2;
				r3 += 2;
				r7 -= 2 * r9;
			}
			if ( r7 < r9 )
				goto lbl_207c;
			do
			{
lbl_204c:	//	LABEL_59:
				--r2;
				if ( r2 )
				{
					++r3;
				}
				else
				{
					r2 = user0; //0x2054
					if ( r2 )
					{
                        r3 = CPUReadMemory(reg[13].I+0xC); // @todo stack pull 0x205c
					}
					else
                    {
                        CPUWriteByte(r4, r2);
                        goto lbl_20e4;
                    }
				}
				r7 -= r9;
			} while ( r7 >= r9 );
lbl_207c:
			while ( 1 )
			{
				s32 r0a = CPUReadByte(r3);
				s32 r1a = CPUReadByte(r3+0x1);

				r1a-=r0a;
				s32 r6a = r1a*(s32)r7;
				r1a = r6a * r12; // 208c
				r6a = (r0a + ((s8)(r1a>>23)));

				r1a = r6a * (s32)r11;

				r0a = CPUReadByte(r5 + 0x630);
                r0a = (r0a + ((s8)(r1a>>8)));
                CPUWriteByte(r5 + 0x630, r0a);
                r1a = r6a * (s32)r10;
                r0a = CPUReadByte(r5);
                r0a = (r0a + ((s8)(r1a>>8)));
                CPUWriteByte(r5++, r0a); //ptr inc +1 not +4

                r7+=r14;
				--r8;
				if ( !r8 )
					break;
				if ( r7 >= r9 )
					goto lbl_0x2004;
			}

			CPUWriteMemory(r4+0x1c, r7);
//lbl_20cc:
			CPUWriteMemory(r4+0x18, r2);
			CPUWriteMemory(r4+0x28, r3);
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
    u32 const puser1 = CPUReadMemory(0x3007FF0); // @todo some sound area, make it common.
    u32 const user1 = CPUReadMemory(puser1);

    if ( user1 == 0x68736D53 )
    {
        u8 v1 = CPUReadByte(puser1+4);
        s8 v1i = v1-1;
        CPUWriteByte(puser1+4, v1i);
        if ( v1 <= 1 )
        {
            u8 v2 = CPUReadByte(puser1+0xB); //11
            u32 base2 = 0x040000D2;
            CPUWriteByte(puser1+4, v2);

            CPUWriteHalfWord(base1+0x6 , 0);
            CPUWriteHalfWord(base2, 0);
            CPUWriteHalfWord(base1+0x6 , 0xB600);
            CPUWriteHalfWord(base2, 0xB600); //-18944
        }
    }
}

void BIOS_SndDriverVSyncOff() // 0x1878
{
    u32 const puser1 = CPUReadMemory(0x3007FF0); // 7FC0 + 0x30
    u32 user1 = CPUReadMemory(puser1);

    if ( user1 == 0x68736D53 || user1 == 0x68736D54 )
    {
        CPUWriteMemory(puser1, (++user1)); // this guard is common for funcs, unify

        CPUWriteHalfWord(base1+0x6 , 0);
        CPUWriteHalfWord(base1+0x12 , 0);
        CPUWriteByte(puser1+4, 0);

        u32 r1 = puser1+0x350;
        u32 r2 = 0x05000318;
        u32 sp = reg[13].I; //0x03003c94;

        CPUWriteMemory(sp, 0);
        BIOS_SndDriver_b4c(sp, r1, r2);

        user1 = CPUReadMemory(puser1);  // 0x18aa
        CPUWriteMemory(puser1, (--user1)); // this ret is common among funcs
    }
    //0x18b0
}

// This functions is verified but lacks proper register settings before calling user func
// it might be that user func modifies or uses those?
// r7 should be puser1, r6 should be flags, ....
void BIOS_SndChannelClear() //0x1824
{
    u32 const puser1 = CPUReadMemory(0x3007FF0); // 7FC0 + 0x30
    u32 user1 = CPUReadMemory(puser1);

    if ( user1 == 0x68736D53 )
    {
        CPUWriteMemory(puser1, (++user1));
        u32 puser2 = puser1 + 0x7 + 0x49;

        int count=12;
        while (count--)
        {
            CPUWriteByte(puser2, 0);
            puser2+=0x40;
        }

        reg[4].I = CPUReadMemory(puser1 + 0x1c); //r5 -> some user thing
        if ( reg[4].I != 0 )
        {
            reg[3].I = 1; // r4 -> channel counter?
            int puser4 = puser1 + 0x2c;
            //reg[0].I = reg[3].I = 1; // r0 & r4 => 1

            while (reg[3].I <= 4)
            {
                 // @todo does user func modify these?
                 reg[0].I = reg[3].I << 24;
                 reg[0].I >>= 24;

                 // Get ptr to user func
                 reg[1].I =  CPUReadMemory(puser4);

                 // @todo here we jump where r1 points; user func?
                 // @todo might modify r6 also?

                 // After call routines
                 reg[3].I += 1; // r4 -> channel counter?
                 reg[4].I += 0x40;// r5 -> some user thing
            }
            CPUWriteByte(reg[4].I, 0); // terminating record?
        }
        CPUWriteMemory(puser1, 0x68736D53);
    }
}

void BIOS_MidiKey2Freq()
{
#ifdef GBA_LOGGING
  if(systemVerbose & VERBOSE_SWI) {
    log("MidiKey2Freq: WaveData=%08x mk=%08x fp=%08x\n",
        reg[0].I,
        reg[1].I,
        reg[2].I);
  }
#endif
  int freq = CPUReadMemory(reg[0].I+4);
  double tmp;
  tmp = ((double)(180 - reg[1].I)) - ((double)reg[2].I / 256.f);
  tmp = pow((double)2.f, tmp / 12.f);
  reg[0].I = (int)((double)freq / tmp);

#ifdef GBA_LOGGING
  if(systemVerbose & VERBOSE_SWI) {
    log("MidiKey2Freq: return %08x\n",
        reg[0].I);
  }
#endif
}

void BIOS_SndDriverJmpTableCopy()
{
#ifdef GBA_LOGGING
  if(systemVerbose & VERBOSE_SWI) {
    log("SndDriverJmpTableCopy: dest=%08x\n",
        reg[0].I);
  }
#endif
  for(int i = 0; i < 0x24; i++) {
    CPUWriteMemory(reg[0].I, 0x9c);
    reg[0].I += 4;
  }
}
