#include <locale>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "GBA.h"
#include "GBAinline.h"
#include "Globals.h"
#include "ereader.h"

char US_Ereader[19] = "CARDE READERPSAE01";
char JAP_Ereader[19] = "CARDE READERPEAJ01";
char JAP_Ereader_plus[19] = "CARDEREADER+PSAJ01";
char rom_info[19];

char Signature[0x29] = "E-Reader Dotcode -Created- by CaitSith2";

unsigned char ShortDotCodeHeader[0x30] = {
    0x00, 0x30, 0x01, 0x01,
    0x00, 0x01, 0x05, 0x10,
    0x00, 0x00, 0x10, 0x12, //Constant data

    0x00, 0x00, //Header First 2 bytes

    0x02, 0x00, //Constant data

    0x00, 0x00, //Header Second 2 bytes

    0x10, 0x47, 0xEF, //Global Checksum 1

    0x19, 0x00, 0x00, 0x00, 0x08, 0x4E, 0x49,
    0x4E, 0x54, 0x45, 0x4E, 0x44, 0x4F, 0x00, 0x22,
    0x00, 0x09, //Constant data

    0x00, 0x00, //Header, last 8 bytes
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00,
    0x00, //Header Checksum
    0x57 //Global Checksum 2
};

unsigned char LongDotCodeHeader[0x30] = {
    0x00, 0x30, 0x01, 0x02,
    0x00, 0x01, 0x08, 0x10,
    0x00, 0x00, 0x10, 0x12, //Constant Data

    0x00, 0x00, //Header, first 2 bytes

    0x01, 0x00, //Constant data

    0x00, 0x00, //Header, second 2 bytes
    0x10, 0x9A, 0x99, //Global Checksum 1

    0x19, 0x00, 0x00, 0x00, 0x08, 0x4E, 0x49,
    0x4E, 0x54, 0x45, 0x4E, 0x44, 0x4F, 0x00, 0x22,
    0x00, 0x09, //Constant data

    0x00, 0x00, //Header, last 8 bytes
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00,
    0x00, //Header Checksum
    0x57 //Global Checksum 2
};

unsigned char shortheader[0x18] = {
    0x00, 0x02, 0x00, 0x01, 0x40, 0x10, 0x00, 0x1C,
    0x10, 0x6F, 0x40, 0xDA, 0x39, 0x25, 0x8E, 0xE0,
    0x7B, 0xB5, 0x98, 0xB6, 0x5B, 0xCF, 0x7F, 0x72
};
unsigned char longheader[0x18] = {
    0x00, 0x03, 0x00, 0x19, 0x40, 0x10, 0x00, 0x2C,
    0x0E, 0x88, 0xED, 0x82, 0x50, 0x67, 0xFB, 0xD1,
    0x43, 0xEE, 0x03, 0xC6, 0xC6, 0x2B, 0x2C, 0x93
};

unsigned char dotcodeheader[0x48];
unsigned char dotcodedata[0xB38];
unsigned char dotcodetemp[0xB00];
int dotcodepointer;
int dotcodeinterleave;
int decodestate;

uint32_t GFpow;

unsigned char* DotCodeData;
char filebuffer[2048];

int dotcodesize;

#if (defined __WIN32__ || defined _WIN32)
#define strcasecmp _stricmp
#endif

int CheckEReaderRegion(void) //US = 1, JAP = 2, JAP+ = 3
{
    int i;
    for (i = 0; i < 18; i++)
        rom_info[i] = rom[0xA0 + i];
    rom_info[i] = 0;

    if (!strcasecmp(rom_info, US_Ereader))
        return 1;
    if (!strcasecmp(rom_info, JAP_Ereader))
        return 2;
    if (!strcasecmp(rom_info, JAP_Ereader_plus))
        return 3;

    return 0;
}

int LoadDotCodeData(int size, uint32_t* DCdata, unsigned long MEM1, unsigned long MEM2, int loadraw)
{
    uint32_t temp1;
    int i, j;

    unsigned char scanmap[28];
    int scantotal = 0;

    for (i = 0; i < 28; i++)
        scanmap[i] = 0;

    unsigned char longdotcodescan[28] = {
        0xF1, 0xF1, 0xF1, 0xF1, 0xF1, 0xF1, 0xF1, 0xF1,
        0xF1, 0xF1, 0xF1, 0xF1, 0xF1, 0xF1, 0xF1, 0xF1,
        0xF1, 0xF1, 0xF1, 0xF1, 0xF1, 0xF1, 0xF1, 0xF1,
        0xF1, 0xF2, 0xB1, 0xB1
    };

    temp1 = CPUReadMemory(MEM1 - 4);
    for (i = 0; i < 0x60; i += 4)
        CPUWriteMemory((MEM2 - 8) + i, 0);
    for (i = 0; i < 0x1860; i += 4)
        CPUWriteMemory(temp1 + i, 0);
    if (DCdata != NULL) {
        if (size == 0xB60) {
            for (i = 0; i < 28; i++) {
                for (j = 0, scantotal = 0; j < 0x68; j += 4) {
                    scantotal += DCdata[((i * 0x68) + j) >> 2];
                }
                if (scantotal)
                    scanmap[i] = longdotcodescan[i];
            }
            for (i = 0; i < size; i += 4) {
                CPUWriteMemory(temp1 + i + 0x9C0, DCdata[i >> 2]);
            }
        } else if (size == 0x750) {
            for (i = 0; i < 18; i++) {
                if ((DCdata[0] == 0x02011394) && (DCdata[1] == 0x0203E110) && (i == 0))
                    continue;
                for (j = 0, scantotal = 0; j < 0x68; j += 4) {
                    scantotal += DCdata[((i * 0x68) + j) >> 2];
                }
                if (scantotal)
                    scanmap[i] = longdotcodescan[i];
            }
            for (i = 0; i < size; i += 4) {
                CPUWriteMemory(temp1 + i, DCdata[i >> 2]);
            }
        }
    }
    CPUWriteMemory(MEM2 - 8, 0x1860);
    CPUWriteMemory(MEM2 - 4, temp1);

    if (size == 0xB60) {
        if (loadraw) {
            for (i = 0; i < 28; i++)
                CPUWriteByte(MEM2 + 0x18 + i, scanmap[i]);
        } else {
            CPUWriteMemory(MEM2 + 0x18, 0xF1F1F1F1);
            CPUWriteMemory(MEM2 + 0x18 + 4, 0xF1F1F1F1);
            CPUWriteMemory(MEM2 + 0x18 + 8, 0xF1F1F1F1);
            CPUWriteMemory(MEM2 + 0x18 + 12, 0xF1F1F1F1);
            CPUWriteMemory(MEM2 + 0x18 + 16, 0xF1F1F1F1);
            CPUWriteMemory(MEM2 + 0x18 + 20, 0xF1F1F1F1);
            CPUWriteMemory(MEM2 + 0x18 + 24, 0xB1B1F2F1);
        }
        CPUWriteMemory(MEM2 + 0x40, 0x19);
        CPUWriteMemory(MEM2 + 0x44, 0x34);
    } else if (size == 0x750) {
        if (loadraw) {
            for (i = 0; i < 18; i++)
                CPUWriteByte(MEM2 + i, scanmap[i]);
        } else {
            CPUWriteMemory(MEM2, 0xF1F1F1F1);
            CPUWriteMemory(MEM2 + 4, 0xF1F1F1F1);
            CPUWriteMemory(MEM2 + 8, 0xF1F1F1F1);
            CPUWriteMemory(MEM2 + 12, 0xF2F1F1F1);
            CPUWriteMemory(MEM2 + 16, 0xB1B1);
        }
        CPUWriteMemory(MEM2 + 0x40, 0x01);
        CPUWriteMemory(MEM2 + 0x44, 0x12);
    }
    CPUWriteMemory(MEM2 + 0x48, 0x3C);
    CPUWriteMemory(MEM2 + 0x4C, MEM2);

    return 0;
}

void EReaderWriteMemory(uint32_t address, uint32_t value)
{
    switch (address >> 24) {
    case 2:
        WRITE32LE(((uint32_t*)&workRAM[address & 0x3FFFF]), value);
        break;
    case 3:
        WRITE32LE(((uint32_t*)&internalRAM[address & 0x7FFF]), value);
        break;
    default:
        WRITE32LE(((uint32_t*)&rom[address & 0x1FFFFFF]), value);
        //rom[address & 0x1FFFFFF] = data;
        break;
    }
}

void BIOS_EReader_ScanCard(int swi_num)
{

    int i, j, k;
    int dotcodetype;

    int global1, global2;

    FILE* f;

    i = j = k = 0;
    //Open dotcode bin/raw

    if (swi_num == 0xE0) {
        dotcodepointer = 0;
        dotcodeinterleave = 0;
        decodestate = 0;

        const char* loadDotCodeFile = GetLoadDotCodeFile();

        if (!*loadDotCodeFile) {
            reg[0].I = 0x301;
            return;
        }
        f = utilOpenFile(loadDotCodeFile, "rb");
        //f=utilOpenFile(filebuffer,"rb");
        //f=utilOpenFile("dotcode4.raw","rb");
        if (f == NULL) {
            reg[0].I = 0x303;
            return;
        }
        fseek(f, 0, SEEK_END);
        i = ftell(f);
        fseek(f, 0, SEEK_SET);
        if ((i == 0xB60) || (i == 0x750)) {
            dotcodetype = 0;
        } else if ((i == 0x81C) || (i == 0x51C)) {
            dotcodetype = 1;
        } else {
            fclose(f);
            reg[0].I = 0x303;
            return;
        }
        DotCodeData = (unsigned char*)malloc(i);
        if (DotCodeData == NULL) {
            reg[0].I = 0x303;
            return;
        }
        FREAD_UNCHECKED(DotCodeData, 1, i, f);
        fclose(f);

        if (dotcodetype == 0) {

            switch (CheckEReaderRegion()) {
            case 1: //US
                LoadDotCodeData(i, (uint32_t*)DotCodeData, 0x2032D14, 0x2028B28, 1);
                EReaderWriteMemory(0x80091BA, 0x46C0DFE2);
                break;
            case 2:
                LoadDotCodeData(i, (uint32_t*)DotCodeData, 0x2006EC4, 0x2002478, 1);
                EReaderWriteMemory(0x8008B12, 0x46C0DFE2);
                break;
            case 3:
                LoadDotCodeData(i, (uint32_t*)DotCodeData, 0x202F8A4, 0x2031034, 1);
                EReaderWriteMemory(0x800922E, 0x46C0DFE2);
                break;
            }
            reg[0].I = 0;
            free(DotCodeData);
        } else {
            //dotcodesize = i;
            if (i == 0x81C)
                dotcodesize = 0xB60;
            else
                dotcodesize = 0x750;

            switch (CheckEReaderRegion()) {
            case 1: //US
                LoadDotCodeData(dotcodesize, (uint32_t*)NULL, 0x2032D14, 0x2028B28, 0);
                EReaderWriteMemory(0x80091BA, 0x46C0DFE1);
                break;
            case 2:
                LoadDotCodeData(dotcodesize, (uint32_t*)NULL, 0x2006EC4, 0x2002478, 0);
                EReaderWriteMemory(0x8008B12, 0x46C0DFE1);
                break;
            case 3:
                LoadDotCodeData(dotcodesize, (uint32_t*)NULL, 0x202F8A4, 0x2031034, 0);
                EReaderWriteMemory(0x800922E, 0x46C0DFE1);
                break;
            }
            reg[0].I = 0;
            dotcodesize = i;
        }
    } else if (swi_num == 0xE1) {

        switch (CheckEReaderRegion()) {
        case 1: //US
            EReaderWriteMemory(0x80091BA, 0xF8A5F03B);
            EReaderWriteMemory(0x3002F7C, 0xEFE40000); //Beginning of Reed-Solomon decoder
            EReaderWriteMemory(0x3003144, 0xCA00002F); //Fix required to Correct 16 "Erasures"
            EReaderWriteMemory(0x300338C, 0xEFE50000); //End of Reed-Solomon decoder
            GFpow = 0x3000A6C;
            break;
        case 2:
            EReaderWriteMemory(0x8008B12, 0xFB0BF035);
            EReaderWriteMemory(0x3002F88, 0xEFE40000);
            EReaderWriteMemory(0x3003150, 0xCA00002F);
            EReaderWriteMemory(0x3003398, 0xEFE50000);
            GFpow = 0x3000A78;
            break;
        case 3:
            EReaderWriteMemory(0x800922E, 0xF94BF04B);
            EReaderWriteMemory(0x3002F7C, 0xEFE40000);
            EReaderWriteMemory(0x3003144, 0xCA00002F);
            EReaderWriteMemory(0x300338C, 0xEFE50000);
            GFpow = 0x3000A6C;
            break;
        }

        armNextPC -= 2;
        reg[15].I -= 2;
        if (armState)
            ARM_PREFETCH
        else
            THUMB_PREFETCH

        for (i = 0, j = 0; i < 12; i++)
            j ^= DotCodeData[i];
        if (dotcodesize == 0x81C) {
            LongDotCodeHeader[0x2E] = j;
            LongDotCodeHeader[0x0D] = DotCodeData[0];
            LongDotCodeHeader[0x0C] = DotCodeData[1];
            LongDotCodeHeader[0x11] = DotCodeData[2];
            LongDotCodeHeader[0x10] = DotCodeData[3];

            LongDotCodeHeader[0x26] = DotCodeData[4];
            LongDotCodeHeader[0x27] = DotCodeData[5];
            LongDotCodeHeader[0x28] = DotCodeData[6];
            LongDotCodeHeader[0x29] = DotCodeData[7];
            LongDotCodeHeader[0x2A] = DotCodeData[8];
            LongDotCodeHeader[0x2B] = DotCodeData[9];
            LongDotCodeHeader[0x2C] = DotCodeData[10];
            LongDotCodeHeader[0x2D] = DotCodeData[11];

            LongDotCodeHeader[0x12] = 0x10; //calculate Global Checksum 1
            LongDotCodeHeader[0x02] = 1; //Do not calculate Global Checksum 2

            for (i = 0x0C, j = 0; i < 0x81C; i++) {
                if (i & 1)
                    j += DotCodeData[i];
                else
                    j += (DotCodeData[i] << 8);
            }
            j &= 0xFFFF;
            j ^= 0xFFFF;
            LongDotCodeHeader[0x13] = (j & 0xFF00) >> 8;
            LongDotCodeHeader[0x14] = (j & 0x00FF);

            for (i = 0, j = 0; i < 0x2F; i++)
                j += LongDotCodeHeader[i];
            j &= 0xFF;
            for (i = 1, global2 = 0; i < 0x2C; i++) {
                for (k = 0, global1 = 0; k < 0x30; k++) {
                    global1 ^= DotCodeData[((i - 1) * 0x30) + k + 0x0C];
                }
                global2 += global1;
            }
            global2 += j;
            global2 &= 0xFF;
            global2 ^= 0xFF;
            LongDotCodeHeader[0x2F] = global2;

        } else {
            ShortDotCodeHeader[0x2E] = j;
            ShortDotCodeHeader[0x0D] = DotCodeData[0];
            ShortDotCodeHeader[0x0C] = DotCodeData[1];
            ShortDotCodeHeader[0x11] = DotCodeData[2];
            ShortDotCodeHeader[0x10] = DotCodeData[3];

            ShortDotCodeHeader[0x26] = DotCodeData[4];
            ShortDotCodeHeader[0x27] = DotCodeData[5];
            ShortDotCodeHeader[0x28] = DotCodeData[6];
            ShortDotCodeHeader[0x29] = DotCodeData[7];
            ShortDotCodeHeader[0x2A] = DotCodeData[8];
            ShortDotCodeHeader[0x2B] = DotCodeData[9];
            ShortDotCodeHeader[0x2C] = DotCodeData[10];
            ShortDotCodeHeader[0x2D] = DotCodeData[11];

            ShortDotCodeHeader[0x12] = 0x10; //calculate Global Checksum 1
            ShortDotCodeHeader[0x02] = 1; //Do not calculate Global Checksum 2

            for (i = 0x0C, j = 0; i < 0x51C; i++) {
                if (i & 1)
                    j += DotCodeData[i];
                else
                    j += (DotCodeData[i] << 8);
            }
            j &= 0xFFFF;
            j ^= 0xFFFF;
            ShortDotCodeHeader[0x13] = (j & 0xFF00) >> 8;
            ShortDotCodeHeader[0x14] = (j & 0x00FF);

            for (i = 0, j = 0; i < 0x2F; i++)
                j += ShortDotCodeHeader[i];
            j &= 0xFF;
            for (i = 1, global2 = 0; i < 0x1C; i++) {
                for (k = 0, global1 = 0; k < 0x30; k++) {
                    global1 ^= DotCodeData[((i - 1) * 0x30) + k + 0x0C];
                }
                global2 += global1;
            }
            global2 += j;
            global2 &= 0xFF;
            global2 ^= 0xFF;
            ShortDotCodeHeader[0x2F] = global2;
        }

    } else if (swi_num == 0xE2) //Header
    {
        switch (CheckEReaderRegion()) {
        case 1: //US
            EReaderWriteMemory(0x80091BA, 0xF8A5F03B);
            EReaderWriteMemory(0x300338C, 0xEFE30000);
            GFpow = 0x3000A6C;
            break;
        case 2:
            EReaderWriteMemory(0x8008B12, 0xFB0BF035);
            EReaderWriteMemory(0x3003398, 0xEFE30000);
            GFpow = 0x3000A78;
            break;
        case 3:
            EReaderWriteMemory(0x800922E, 0xF94BF04B);
            EReaderWriteMemory(0x300338C, 0xEFE30000);
            GFpow = 0x3000A6C;
            break;
        }
        armNextPC -= 2;
        reg[15].I -= 2;
        if (armState)
            ARM_PREFETCH
        else
            THUMB_PREFETCH
    } else if ((swi_num == 0xE3) || (swi_num == 0xE5)) //Dotcode data
    {
        if (((int)reg[0].I >= 0) && (reg[0].I <= 0x10)) {
            if (decodestate == 0) {
                for (i = 0x17; i >= 0; i--) {
                    if ((0x17 - i) < 8)
                        j = CPUReadByte(GFpow + CPUReadByte(GFpow + 0x200 + i));
                    else
                        j = CPUReadByte(GFpow + CPUReadByte(GFpow + 0x200 + i)) ^ 0xFF;

                    dotcodeheader[(0x17 - i)] = j;
                    dotcodeheader[(0x17 - i) + 0x18] = j;
                    dotcodeheader[(0x17 - i) + 0x30] = j;
                }
                for (i = 0; i < 28; i++)
                    for (j = 0; j < 2; j++)
                        dotcodedata[(i * 0x68) + j] = dotcodeheader[(i * 2) + j];
                dotcodeinterleave = dotcodeheader[7];
                decodestate = 1;
            } else {
                for (i = 0x3F; i >= 0; i--) {
                    if ((0x3F - i) < 0x30)
                        j = CPUReadByte(GFpow + CPUReadByte(GFpow + 0x200 + i));
                    else
                        j = CPUReadByte(GFpow + CPUReadByte(GFpow + 0x200 + i)) ^ 0xFF;
                    dotcodetemp[((0x3F - i) * dotcodeinterleave) + dotcodepointer] = j;
                }
                dotcodepointer++;

                if (dotcodepointer == dotcodeinterleave) {
                    switch (dotcodeinterleave) {
                    case 0x1C:
                        j = 0x724;
                        k = 0x750 - j;
                        break;
                    case 0x2C:
                        j = 0xB38;
                        k = 0xB60 - j;
                        break;
                    }
                    dotcodepointer = 0;
                    for (i = 2; i < j; i++) {
                        if ((i % 0x68) == 0)
                            i += 2;
                        dotcodedata[i] = dotcodetemp[dotcodepointer++];
                    }
                    if (swi_num == 0xE3) {
                        const char* loadDotCodeFile = GetLoadDotCodeFile();
                        f = utilOpenFile(loadDotCodeFile, "rb+");
                        if (f != NULL) {
                            fwrite(dotcodedata, 1, j, f);
                            fclose(f);
                        }
                    } else {
                        const char* saveDotCodeFile = GetSaveDotCodeFile();
                        if (saveDotCodeFile) {
                            f = utilOpenFile(saveDotCodeFile, "wb");
                            if (f != NULL) {
                                fwrite(dotcodedata, 1, j, f);
                                fwrite(Signature, 1, 0x28, f);
                                if (j == 0x724) {
                                    fputc(0x65, f);
                                    fputc(0x02, f);
                                    fputc(0x71, f);
                                    fputc(0x10, f);
                                }
                                fclose(f);
                            }
                        }
                        free(DotCodeData);
                    }
                }
            }
        }

        int base = 14;
        armState = reg[base].I & 1 ? false : true;
        if (armState) {
            reg[15].I = reg[base].I & 0xFFFFFFFC;
            armNextPC = reg[15].I;
            reg[15].I += 4;
            ARM_PREFETCH
        } else {
            reg[15].I = reg[base].I & 0xFFFFFFFE;
            armNextPC = reg[15].I;
            reg[15].I += 2;
            THUMB_PREFETCH
        }
    } else if (swi_num == 0xE4) {
        reg[12].I = reg[13].I;
        if (decodestate == 0) {
            for (i = 0; i < 0x18; i++) {
                if (dotcodesize == 0x81C)
                    j = longheader[i];
                else
                    j = shortheader[i];

                if (i < 8)
                    j = CPUReadByte(GFpow + 0x100 + j);
                else
                    j = CPUReadByte(GFpow + 0x100 + (j ^ 0xFF));

                CPUWriteByte(GFpow + 0x200 + (0x17 - i), j);
            }
        } else {
            if (dotcodepointer == 0) {
                for (i = 0; i < 0x30; i++) {
                    if (dotcodesize == 0x81C)
                        j = LongDotCodeHeader[i];
                    else
                        j = ShortDotCodeHeader[i];

                    j = CPUReadByte(GFpow + 0x100 + j);
                    CPUWriteByte(GFpow + 0x200 + (0x3F - i), j);
                }

            } else {
                for (i = 0; i < 0x30; i++) {

                    j = DotCodeData[((dotcodepointer - 1) * 0x30) + 0x0C + i];
                    j = CPUReadByte(GFpow + 0x100 + j);
                    CPUWriteByte(GFpow + 0x200 + (0x3F - i), j);
                }
            }
            for (i = 0; i < 16; i++)
                CPUWriteByte(GFpow + 0x258 + i, 1); //16 Erasures on the parity bytes, to have them calculated.
        }
    }
}
