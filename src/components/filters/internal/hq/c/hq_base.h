/*
    VisualBoyAdvance - a Game Boy & Game Boy Advance emulator

    Copyright (C) 2008 VBA-M development team


    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

        hq filter by Maxim Stepin ( http://hiend3d.com )
*/

#ifdef _16BIT
#ifdef _32BIT
#error _16BIT and _32BIT defined at the same time!
#endif
#endif

#ifdef _16BIT
#define SIZE_PIXEL 2 // 16bit = 2 bytes
#define COLORTYPE unsigned short
#define RGBtoYUV RGBtoYUV_16
#define Interp1 Interp1_16
#define Interp2 Interp2_16
#define Interp3 Interp3_16
#define Interp4 Interp4_16
#define Interp5 Interp5_16
#define Interp6 Interp6_16
#define Interp7 Interp7_16
#define Interp8 Interp8_16
#endif

#ifdef _32BIT
#define SIZE_PIXEL 4 // 32bit = 4 bytes
#define COLORTYPE unsigned int
#define RGBtoYUV RGBtoYUV_32
#define Interp1 Interp1_32
#define Interp2 Interp2_32
#define Interp3 Interp3_32
#define Interp4 Interp4_32
#define Interp5 Interp5_32
#define Interp6 Interp6_32
#define Interp7 Interp7_32
#define Interp8 Interp8_32
#endif

#ifdef _HQ3X

#define _MAGNIFICATION 3

#define PIXEL00_1M Interp1(pOut, c[5], c[1]);
#define PIXEL00_1U Interp1(pOut, c[5], c[2]);
#define PIXEL00_1L Interp1(pOut, c[5], c[4]);
#define PIXEL00_2 Interp2(pOut, c[5], c[4], c[2]);
#define PIXEL00_4 Interp4(pOut, c[5], c[4], c[2]);
#define PIXEL00_5 Interp5(pOut, c[4], c[2]);
#define PIXEL00_C *((COLORTYPE *)(pOut)) = c[5];

#define PIXEL01_1 Interp1(pOut + SIZE_PIXEL, c[5], c[2]);
#define PIXEL01_3 Interp3(pOut + SIZE_PIXEL, c[5], c[2]);
#define PIXEL01_6 Interp1(pOut + SIZE_PIXEL, c[2], c[5]);
#define PIXEL01_C *((COLORTYPE *)(pOut + SIZE_PIXEL)) = c[5];

#define PIXEL02_1M Interp1(pOut + SIZE_PIXEL + SIZE_PIXEL, c[5], c[3]);
#define PIXEL02_1U Interp1(pOut + SIZE_PIXEL + SIZE_PIXEL, c[5], c[2]);
#define PIXEL02_1R Interp1(pOut + SIZE_PIXEL + SIZE_PIXEL, c[5], c[6]);
#define PIXEL02_2 Interp2(pOut + SIZE_PIXEL + SIZE_PIXEL, c[5], c[2], c[6]);
#define PIXEL02_4 Interp4(pOut + SIZE_PIXEL + SIZE_PIXEL, c[5], c[2], c[6]);
#define PIXEL02_5 Interp5(pOut + SIZE_PIXEL + SIZE_PIXEL, c[2], c[6]);
#define PIXEL02_C *((COLORTYPE *)(pOut + SIZE_PIXEL + SIZE_PIXEL)) = c[5];

#define PIXEL10_1 Interp1(pOut + dstPitch, c[5], c[4]);
#define PIXEL10_3 Interp3(pOut + dstPitch, c[5], c[4]);
#define PIXEL10_6 Interp1(pOut + dstPitch, c[4], c[5]);
#define PIXEL10_C *((COLORTYPE *)(pOut + dstPitch)) = c[5];

#define PIXEL11 *((COLORTYPE *)(pOut + dstPitch + SIZE_PIXEL)) = c[5];

#define PIXEL12_1 Interp1(pOut + dstPitch + SIZE_PIXEL + SIZE_PIXEL, c[5], c[6]);
#define PIXEL12_3 Interp3(pOut + dstPitch + SIZE_PIXEL + SIZE_PIXEL, c[5], c[6]);
#define PIXEL12_6 Interp1(pOut + dstPitch + SIZE_PIXEL + SIZE_PIXEL, c[6], c[5]);
#define PIXEL12_C *((COLORTYPE *)(pOut + dstPitch + SIZE_PIXEL + SIZE_PIXEL)) = c[5];

#define PIXEL20_1M Interp1(pOut + dstPitch + dstPitch, c[5], c[7]);
#define PIXEL20_1D Interp1(pOut + dstPitch + dstPitch, c[5], c[8]);
#define PIXEL20_1L Interp1(pOut + dstPitch + dstPitch, c[5], c[4]);
#define PIXEL20_2 Interp2(pOut + dstPitch + dstPitch, c[5], c[8], c[4]);
#define PIXEL20_4 Interp4(pOut + dstPitch + dstPitch, c[5], c[8], c[4]);
#define PIXEL20_5 Interp5(pOut + dstPitch + dstPitch, c[8], c[4]);
#define PIXEL20_C *((COLORTYPE *)(pOut + dstPitch + dstPitch)) = c[5];

#define PIXEL21_1 Interp1(pOut + dstPitch + dstPitch + SIZE_PIXEL, c[5], c[8]);
#define PIXEL21_3 Interp3(pOut + dstPitch + dstPitch + SIZE_PIXEL, c[5], c[8]);
#define PIXEL21_6 Interp1(pOut + dstPitch + dstPitch + SIZE_PIXEL, c[8], c[5]);
#define PIXEL21_C *((COLORTYPE *)(pOut + dstPitch + dstPitch + SIZE_PIXEL)) = c[5];

#define PIXEL22_1M Interp1(pOut + dstPitch + dstPitch + SIZE_PIXEL + SIZE_PIXEL, c[5], c[9]);
#define PIXEL22_1D Interp1(pOut + dstPitch + dstPitch + SIZE_PIXEL + SIZE_PIXEL, c[5], c[8]);
#define PIXEL22_1R Interp1(pOut + dstPitch + dstPitch + SIZE_PIXEL + SIZE_PIXEL, c[5], c[6]);
#define PIXEL22_2 Interp2(pOut + dstPitch + dstPitch + SIZE_PIXEL + SIZE_PIXEL, c[5], c[6], c[8]);
#define PIXEL22_4 Interp4(pOut + dstPitch + dstPitch + SIZE_PIXEL + SIZE_PIXEL, c[5], c[6], c[8]);
#define PIXEL22_5 Interp5(pOut + dstPitch + dstPitch + SIZE_PIXEL + SIZE_PIXEL, c[6], c[8]);
#define PIXEL22_C *((COLORTYPE *)(pOut + dstPitch + dstPitch + SIZE_PIXEL + SIZE_PIXEL)) = c[5];

#endif // #ifdef _HQ3X

#ifdef _HQ4X

#define _MAGNIFICATION 4

#define PIXEL00_0 *((COLORTYPE *)(pOut)) = c[5];
#define PIXEL00_11 Interp1(pOut, c[5], c[4]);
#define PIXEL00_12 Interp1(pOut, c[5], c[2]);
#define PIXEL00_20 Interp2(pOut, c[5], c[2], c[4]);
#define PIXEL00_50 Interp5(pOut, c[2], c[4]);
#define PIXEL00_80 Interp8(pOut, c[5], c[1]);
#define PIXEL00_81 Interp8(pOut, c[5], c[4]);
#define PIXEL00_82 Interp8(pOut, c[5], c[2]);

#define PIXEL01_0 *((COLORTYPE *)(pOut + SIZE_PIXEL)) = c[5];
#define PIXEL01_10 Interp1(pOut + SIZE_PIXEL, c[5], c[1]);
#define PIXEL01_12 Interp1(pOut + SIZE_PIXEL, c[5], c[2]);
#define PIXEL01_14 Interp1(pOut + SIZE_PIXEL, c[2], c[5]);
#define PIXEL01_21 Interp2(pOut + SIZE_PIXEL, c[2], c[5], c[4]);
#define PIXEL01_31 Interp3(pOut + SIZE_PIXEL, c[5], c[4]);
#define PIXEL01_50 Interp5(pOut + SIZE_PIXEL, c[2], c[5]);
#define PIXEL01_60 Interp6(pOut + SIZE_PIXEL, c[5], c[2], c[4]);
#define PIXEL01_61 Interp6(pOut + SIZE_PIXEL, c[5], c[2], c[1]);
#define PIXEL01_82 Interp8(pOut + SIZE_PIXEL, c[5], c[2]);
#define PIXEL01_83 Interp8(pOut + SIZE_PIXEL, c[2], c[4]);

#define PIXEL02_0 *((COLORTYPE *)(pOut + SIZE_PIXEL + SIZE_PIXEL)) = c[5];
#define PIXEL02_10 Interp1(pOut + SIZE_PIXEL + SIZE_PIXEL, c[5], c[3]);
#define PIXEL02_11 Interp1(pOut + SIZE_PIXEL + SIZE_PIXEL, c[5], c[2]);
#define PIXEL02_13 Interp1(pOut + SIZE_PIXEL + SIZE_PIXEL, c[2], c[5]);
#define PIXEL02_21 Interp2(pOut + SIZE_PIXEL + SIZE_PIXEL, c[2], c[5], c[6]);
#define PIXEL02_32 Interp3(pOut + SIZE_PIXEL + SIZE_PIXEL, c[5], c[6]);
#define PIXEL02_50 Interp5(pOut + SIZE_PIXEL + SIZE_PIXEL, c[2], c[5]);
#define PIXEL02_60 Interp6(pOut + SIZE_PIXEL + SIZE_PIXEL, c[5], c[2], c[6]);
#define PIXEL02_61 Interp6(pOut + SIZE_PIXEL + SIZE_PIXEL, c[5], c[2], c[3]);
#define PIXEL02_81 Interp8(pOut + SIZE_PIXEL + SIZE_PIXEL, c[5], c[2]);
#define PIXEL02_83 Interp8(pOut + SIZE_PIXEL + SIZE_PIXEL, c[2], c[6]);

#define PIXEL03_0 *((COLORTYPE *)(pOut + SIZE_PIXEL + SIZE_PIXEL + SIZE_PIXEL)) = c[5];
#define PIXEL03_11 Interp1(pOut + SIZE_PIXEL + SIZE_PIXEL + SIZE_PIXEL, c[5], c[2]);
#define PIXEL03_12 Interp1(pOut + SIZE_PIXEL + SIZE_PIXEL + SIZE_PIXEL, c[5], c[6]);
#define PIXEL03_20 Interp2(pOut + SIZE_PIXEL + SIZE_PIXEL + SIZE_PIXEL, c[5], c[2], c[6]);
#define PIXEL03_50 Interp5(pOut + SIZE_PIXEL + SIZE_PIXEL + SIZE_PIXEL, c[2], c[6]);
#define PIXEL03_80 Interp8(pOut + SIZE_PIXEL + SIZE_PIXEL + SIZE_PIXEL, c[5], c[3]);
#define PIXEL03_81 Interp8(pOut + SIZE_PIXEL + SIZE_PIXEL + SIZE_PIXEL, c[5], c[2]);
#define PIXEL03_82 Interp8(pOut + SIZE_PIXEL + SIZE_PIXEL + SIZE_PIXEL, c[5], c[6]);

#define PIXEL10_0 *((COLORTYPE *)(pOut + dstPitch)) = c[5];
#define PIXEL10_10 Interp1(pOut + dstPitch, c[5], c[1]);
#define PIXEL10_11 Interp1(pOut + dstPitch, c[5], c[4]);
#define PIXEL10_13 Interp1(pOut + dstPitch, c[4], c[5]);
#define PIXEL10_21 Interp2(pOut + dstPitch, c[4], c[5], c[2]);
#define PIXEL10_32 Interp3(pOut + dstPitch, c[5], c[2]);
#define PIXEL10_50 Interp5(pOut + dstPitch, c[4], c[5]);
#define PIXEL10_60 Interp6(pOut + dstPitch, c[5], c[4], c[2]);
#define PIXEL10_61 Interp6(pOut + dstPitch, c[5], c[4], c[1]);
#define PIXEL10_81 Interp8(pOut + dstPitch, c[5], c[4]);
#define PIXEL10_83 Interp8(pOut + dstPitch, c[4], c[2]);

#define PIXEL11_0 *((COLORTYPE *)(pOut + dstPitch + SIZE_PIXEL)) = c[5];
#define PIXEL11_30 Interp3(pOut + dstPitch + SIZE_PIXEL, c[5], c[1]);
#define PIXEL11_31 Interp3(pOut + dstPitch + SIZE_PIXEL, c[5], c[4]);
#define PIXEL11_32 Interp3(pOut + dstPitch + SIZE_PIXEL, c[5], c[2]);
#define PIXEL11_70 Interp7(pOut + dstPitch + SIZE_PIXEL, c[5], c[4], c[2]);

#define PIXEL12_0 *((COLORTYPE *)(pOut + dstPitch + SIZE_PIXEL + SIZE_PIXEL)) = c[5];
#define PIXEL12_30 Interp3(pOut + dstPitch + SIZE_PIXEL + SIZE_PIXEL, c[5], c[3]);
#define PIXEL12_31 Interp3(pOut + dstPitch + SIZE_PIXEL + SIZE_PIXEL, c[5], c[2]);
#define PIXEL12_32 Interp3(pOut + dstPitch + SIZE_PIXEL + SIZE_PIXEL, c[5], c[6]);
#define PIXEL12_70 Interp7(pOut + dstPitch + SIZE_PIXEL + SIZE_PIXEL, c[5], c[6], c[2]);

#define PIXEL13_0 *((COLORTYPE *)(pOut + dstPitch + SIZE_PIXEL + SIZE_PIXEL + SIZE_PIXEL)) = c[5];
#define PIXEL13_10 Interp1(pOut + dstPitch + SIZE_PIXEL + SIZE_PIXEL + SIZE_PIXEL, c[5], c[3]);
#define PIXEL13_12 Interp1(pOut + dstPitch + SIZE_PIXEL + SIZE_PIXEL + SIZE_PIXEL, c[5], c[6]);
#define PIXEL13_14 Interp1(pOut + dstPitch + SIZE_PIXEL + SIZE_PIXEL + SIZE_PIXEL, c[6], c[5]);
#define PIXEL13_21                                                                                 \
        Interp2(pOut + dstPitch + SIZE_PIXEL + SIZE_PIXEL + SIZE_PIXEL, c[6], c[5], c[2]);
#define PIXEL13_31 Interp3(pOut + dstPitch + SIZE_PIXEL + SIZE_PIXEL + SIZE_PIXEL, c[5], c[2]);
#define PIXEL13_50 Interp5(pOut + dstPitch + SIZE_PIXEL + SIZE_PIXEL + SIZE_PIXEL, c[6], c[5]);
#define PIXEL13_60                                                                                 \
        Interp6(pOut + dstPitch + SIZE_PIXEL + SIZE_PIXEL + SIZE_PIXEL, c[5], c[6], c[2]);
#define PIXEL13_61                                                                                 \
        Interp6(pOut + dstPitch + SIZE_PIXEL + SIZE_PIXEL + SIZE_PIXEL, c[5], c[6], c[3]);
#define PIXEL13_82 Interp8(pOut + dstPitch + SIZE_PIXEL + SIZE_PIXEL + SIZE_PIXEL, c[5], c[6]);
#define PIXEL13_83 Interp8(pOut + dstPitch + SIZE_PIXEL + SIZE_PIXEL + SIZE_PIXEL, c[6], c[2]);

#define PIXEL20_0 *((COLORTYPE *)(pOut + dstPitch + dstPitch)) = c[5];
#define PIXEL20_10 Interp1(pOut + dstPitch + dstPitch, c[5], c[7]);
#define PIXEL20_12 Interp1(pOut + dstPitch + dstPitch, c[5], c[4]);
#define PIXEL20_14 Interp1(pOut + dstPitch + dstPitch, c[4], c[5]);
#define PIXEL20_21 Interp2(pOut + dstPitch + dstPitch, c[4], c[5], c[8]);
#define PIXEL20_31 Interp3(pOut + dstPitch + dstPitch, c[5], c[8]);
#define PIXEL20_50 Interp5(pOut + dstPitch + dstPitch, c[4], c[5]);
#define PIXEL20_60 Interp6(pOut + dstPitch + dstPitch, c[5], c[4], c[8]);
#define PIXEL20_61 Interp6(pOut + dstPitch + dstPitch, c[5], c[4], c[7]);
#define PIXEL20_82 Interp8(pOut + dstPitch + dstPitch, c[5], c[4]);
#define PIXEL20_83 Interp8(pOut + dstPitch + dstPitch, c[4], c[8]);

#define PIXEL21_0 *((COLORTYPE *)(pOut + dstPitch + dstPitch + SIZE_PIXEL)) = c[5];
#define PIXEL21_30 Interp3(pOut + dstPitch + dstPitch + SIZE_PIXEL, c[5], c[7]);
#define PIXEL21_31 Interp3(pOut + dstPitch + dstPitch + SIZE_PIXEL, c[5], c[8]);
#define PIXEL21_32 Interp3(pOut + dstPitch + dstPitch + SIZE_PIXEL, c[5], c[4]);
#define PIXEL21_70 Interp7(pOut + dstPitch + dstPitch + SIZE_PIXEL, c[5], c[4], c[8]);
#define PIXEL22_0 *((COLORTYPE *)(pOut + dstPitch + dstPitch + SIZE_PIXEL + SIZE_PIXEL)) = c[5];
#define PIXEL22_30 Interp3(pOut + dstPitch + dstPitch + SIZE_PIXEL + SIZE_PIXEL, c[5], c[9]);
#define PIXEL22_31 Interp3(pOut + dstPitch + dstPitch + SIZE_PIXEL + SIZE_PIXEL, c[5], c[6]);
#define PIXEL22_32 Interp3(pOut + dstPitch + dstPitch + SIZE_PIXEL + SIZE_PIXEL, c[5], c[8]);
#define PIXEL22_70 Interp7(pOut + dstPitch + dstPitch + SIZE_PIXEL + SIZE_PIXEL, c[5], c[6], c[8]);

#define PIXEL23_0                                                                                  \
        *((COLORTYPE *)(pOut + dstPitch + dstPitch + SIZE_PIXEL + SIZE_PIXEL + SIZE_PIXEL)) = c[5];
#define PIXEL23_10                                                                                 \
        Interp1(pOut + dstPitch + dstPitch + SIZE_PIXEL + SIZE_PIXEL + SIZE_PIXEL, c[5], c[9]);
#define PIXEL23_11                                                                                 \
        Interp1(pOut + dstPitch + dstPitch + SIZE_PIXEL + SIZE_PIXEL + SIZE_PIXEL, c[5], c[6]);
#define PIXEL23_13                                                                                 \
        Interp1(pOut + dstPitch + dstPitch + SIZE_PIXEL + SIZE_PIXEL + SIZE_PIXEL, c[6], c[5]);
#define PIXEL23_21                                                                                 \
        Interp2(pOut + dstPitch + dstPitch + SIZE_PIXEL + SIZE_PIXEL + SIZE_PIXEL,                 \
                c[6],                                                                              \
                c[5],                                                                              \
                c[8]);
#define PIXEL23_32                                                                                 \
        Interp3(pOut + dstPitch + dstPitch + SIZE_PIXEL + SIZE_PIXEL + SIZE_PIXEL, c[5], c[8]);
#define PIXEL23_50                                                                                 \
        Interp5(pOut + dstPitch + dstPitch + SIZE_PIXEL + SIZE_PIXEL + SIZE_PIXEL, c[6], c[5]);
#define PIXEL23_60                                                                                 \
        Interp6(pOut + dstPitch + dstPitch + SIZE_PIXEL + SIZE_PIXEL + SIZE_PIXEL,                 \
                c[5],                                                                              \
                c[6],                                                                              \
                c[8]);
#define PIXEL23_61                                                                                 \
        Interp6(pOut + dstPitch + dstPitch + SIZE_PIXEL + SIZE_PIXEL + SIZE_PIXEL,                 \
                c[5],                                                                              \
                c[6],                                                                              \
                c[9]);
#define PIXEL23_81                                                                                 \
        Interp8(pOut + dstPitch + dstPitch + SIZE_PIXEL + SIZE_PIXEL + SIZE_PIXEL, c[5], c[6]);
#define PIXEL23_83                                                                                 \
        Interp8(pOut + dstPitch + dstPitch + SIZE_PIXEL + SIZE_PIXEL + SIZE_PIXEL, c[6], c[8]);

#define PIXEL30_0 *((COLORTYPE *)(pOut + dstPitch + dstPitch + dstPitch)) = c[5];
#define PIXEL30_11 Interp1(pOut + dstPitch + dstPitch + dstPitch, c[5], c[8]);
#define PIXEL30_12 Interp1(pOut + dstPitch + dstPitch + dstPitch, c[5], c[4]);
#define PIXEL30_20 Interp2(pOut + dstPitch + dstPitch + dstPitch, c[5], c[8], c[4]);
#define PIXEL30_50 Interp5(pOut + dstPitch + dstPitch + dstPitch, c[8], c[4]);
#define PIXEL30_80 Interp8(pOut + dstPitch + dstPitch + dstPitch, c[5], c[7]);
#define PIXEL30_81 Interp8(pOut + dstPitch + dstPitch + dstPitch, c[5], c[8]);
#define PIXEL30_82 Interp8(pOut + dstPitch + dstPitch + dstPitch, c[5], c[4]);

#define PIXEL31_0 *((COLORTYPE *)(pOut + dstPitch + dstPitch + dstPitch + SIZE_PIXEL)) = c[5];
#define PIXEL31_10 Interp1(pOut + dstPitch + dstPitch + dstPitch + SIZE_PIXEL, c[5], c[7]);
#define PIXEL31_11 Interp1(pOut + dstPitch + dstPitch + dstPitch + SIZE_PIXEL, c[5], c[8]);
#define PIXEL31_13 Interp1(pOut + dstPitch + dstPitch + dstPitch + SIZE_PIXEL, c[8], c[5]);
#define PIXEL31_21 Interp2(pOut + dstPitch + dstPitch + dstPitch + SIZE_PIXEL, c[8], c[5], c[4]);
#define PIXEL31_32 Interp3(pOut + dstPitch + dstPitch + dstPitch + SIZE_PIXEL, c[5], c[4]);
#define PIXEL31_50 Interp5(pOut + dstPitch + dstPitch + dstPitch + SIZE_PIXEL, c[8], c[5]);
#define PIXEL31_60 Interp6(pOut + dstPitch + dstPitch + dstPitch + SIZE_PIXEL, c[5], c[8], c[4]);
#define PIXEL31_61 Interp6(pOut + dstPitch + dstPitch + dstPitch + SIZE_PIXEL, c[5], c[8], c[7]);
#define PIXEL31_81 Interp8(pOut + dstPitch + dstPitch + dstPitch + SIZE_PIXEL, c[5], c[8]);
#define PIXEL31_83 Interp8(pOut + dstPitch + dstPitch + dstPitch + SIZE_PIXEL, c[8], c[4]);

#define PIXEL32_0                                                                                  \
        *((COLORTYPE *)(pOut + dstPitch + dstPitch + dstPitch + SIZE_PIXEL + SIZE_PIXEL)) = c[5];
#define PIXEL32_10                                                                                 \
        Interp1(pOut + dstPitch + dstPitch + dstPitch + SIZE_PIXEL + SIZE_PIXEL, c[5], c[9]);
#define PIXEL32_12                                                                                 \
        Interp1(pOut + dstPitch + dstPitch + dstPitch + SIZE_PIXEL + SIZE_PIXEL, c[5], c[8]);
#define PIXEL32_14                                                                                 \
        Interp1(pOut + dstPitch + dstPitch + dstPitch + SIZE_PIXEL + SIZE_PIXEL, c[8], c[5]);
#define PIXEL32_21                                                                                 \
        Interp2(pOut + dstPitch + dstPitch + dstPitch + SIZE_PIXEL + SIZE_PIXEL, c[8], c[5], c[6]);
#define PIXEL32_31                                                                                 \
        Interp3(pOut + dstPitch + dstPitch + dstPitch + SIZE_PIXEL + SIZE_PIXEL, c[5], c[6]);
#define PIXEL32_50                                                                                 \
        Interp5(pOut + dstPitch + dstPitch + dstPitch + SIZE_PIXEL + SIZE_PIXEL, c[8], c[5]);
#define PIXEL32_60                                                                                 \
        Interp6(pOut + dstPitch + dstPitch + dstPitch + SIZE_PIXEL + SIZE_PIXEL, c[5], c[8], c[6]);
#define PIXEL32_61                                                                                 \
        Interp6(pOut + dstPitch + dstPitch + dstPitch + SIZE_PIXEL + SIZE_PIXEL, c[5], c[8], c[9]);
#define PIXEL32_82                                                                                 \
        Interp8(pOut + dstPitch + dstPitch + dstPitch + SIZE_PIXEL + SIZE_PIXEL, c[5], c[8]);
#define PIXEL32_83                                                                                 \
        Interp8(pOut + dstPitch + dstPitch + dstPitch + SIZE_PIXEL + SIZE_PIXEL, c[8], c[6]);

#define PIXEL33_0                                                                                  \
        *((COLORTYPE *)(pOut + dstPitch + dstPitch + dstPitch + SIZE_PIXEL + SIZE_PIXEL +          \
                        SIZE_PIXEL)) = c[5];
#define PIXEL33_11                                                                                 \
        Interp1(pOut + dstPitch + dstPitch + dstPitch + SIZE_PIXEL + SIZE_PIXEL + SIZE_PIXEL,      \
                c[5],                                                                              \
                c[6]);
#define PIXEL33_12                                                                                 \
        Interp1(pOut + dstPitch + dstPitch + dstPitch + SIZE_PIXEL + SIZE_PIXEL + SIZE_PIXEL,      \
                c[5],                                                                              \
                c[8]);
#define PIXEL33_20                                                                                 \
        Interp2(pOut + dstPitch + dstPitch + dstPitch + SIZE_PIXEL + SIZE_PIXEL + SIZE_PIXEL,      \
                c[5],                                                                              \
                c[8],                                                                              \
                c[6]);
#define PIXEL33_50                                                                                 \
        Interp5(pOut + dstPitch + dstPitch + dstPitch + SIZE_PIXEL + SIZE_PIXEL + SIZE_PIXEL,      \
                c[8],                                                                              \
                c[6]);
#define PIXEL33_80                                                                                 \
        Interp8(pOut + dstPitch + dstPitch + dstPitch + SIZE_PIXEL + SIZE_PIXEL + SIZE_PIXEL,      \
                c[5],                                                                              \
                c[9]);
#define PIXEL33_81                                                                                 \
        Interp8(pOut + dstPitch + dstPitch + dstPitch + SIZE_PIXEL + SIZE_PIXEL + SIZE_PIXEL,      \
                c[5],                                                                              \
                c[6]);
#define PIXEL33_82                                                                                 \
        Interp8(pOut + dstPitch + dstPitch + dstPitch + SIZE_PIXEL + SIZE_PIXEL + SIZE_PIXEL,      \
                c[5],                                                                              \
                c[8]);

#endif // #ifdef _HQ4X

// function header
#ifdef _16BIT
#ifdef _HQ3X
        void hq3x16(
#endif
#ifdef _HQ4X
	void hq4x16(
#endif
#endif

#ifdef _32BIT
#ifdef _HQ3X
	void hq3x32(
#endif
#ifdef _HQ4X
	void hq4x32(
#endif
#endif

unsigned char *pIn, unsigned int srcPitch,
unsigned char *,
unsigned char *pOut, unsigned int dstPitch,
int Xres, int Yres )
{
        unsigned int yuv[10] = { 0 }; // yuv[0] not used
        // yuv[1-9] allows reusage of calculated YUV values
        int x, y;
        unsigned int linePlus, lineMinus;

        COLORTYPE c[10]; // c[0] not used
        // +----+----+----+
        // |    |    |    |
        // | c1 | c2 | c3 |
        // +----+----+----+
        // |    |    |    |
        // | c4 | c5 | c6 |
        // +----+----+----+
        // |    |    |    |
        // | c7 | c8 | c9 |
        // +----+----+----+

        for (y = 0; y < Yres; y++) {
                if (y == 0) {
                        linePlus = srcPitch;
                        lineMinus = 0;
                } else if (y == (Yres - 1)) {
                        linePlus = 0;
                        lineMinus = srcPitch;
                } else {
                        linePlus = srcPitch;
                        lineMinus = srcPitch;
                }

                for (x = 0; x < Xres; x++) {
                        c[2] = *((COLORTYPE *)(pIn - lineMinus));
                        c[5] = *((COLORTYPE *)(pIn));
                        c[8] = *((COLORTYPE *)(pIn + linePlus));

                        if (x > 0) {
                                // upper border possible:
                                c[1] = *((COLORTYPE *)(pIn - lineMinus - SIZE_PIXEL));

                                c[4] = *((COLORTYPE *)(pIn - SIZE_PIXEL));

                                // lower border possible:
                                c[7] = *((COLORTYPE *)(pIn + linePlus - SIZE_PIXEL));
                        } else { // left border
                                c[1] = c[2];
                                c[4] = c[5];
                                c[7] = c[8];
                        }

                        if (x < Xres - 1) {
                                // upper border possible:
                                c[3] = *((COLORTYPE *)(pIn - lineMinus + SIZE_PIXEL));

                                c[6] = *((COLORTYPE *)(pIn + SIZE_PIXEL));

                                // lower border possible:
                                c[9] = *((COLORTYPE *)(pIn + linePlus + SIZE_PIXEL));
                        } else { // right border
                                c[3] = c[2];
                                c[6] = c[5];
                                c[9] = c[8];
                        }

                        unsigned int pattern = 0;
                        unsigned int flag = 1;

                        yuv[5] = RGBtoYUV(c[5]);

                        for (unsigned char k = 1; k <= 9; k++) {
                                if (k == 5)
                                        continue;

                                if (c[k] != c[5]) {
                                        // pre-calculating the YUV-values for every pixel does
                                        // not speed up the process
                                        yuv[k] = RGBtoYUV(c[k]);

                                        if ((abs_32((yuv[5] & 0x00FF0000) - (yuv[k] & 0x00FF0000)) >
                                             0x00300000) ||
                                            (abs_32((yuv[5] & 0x0000FF00) - (yuv[k] & 0x0000FF00)) >
                                             0x00000700) ||
                                            (abs_32((yuv[5] & 0x000000FF) - (yuv[k] & 0x000000FF)) >
                                             0x00000006)) {
                                                pattern |= flag;
                                        }
                                }
                                flag <<= 1;
                        }

#ifdef _HQ3X
#include "hq3x_pattern.h"
#endif

#ifdef _HQ4X
#include "hq4x_pattern.h"
#endif

                        pIn += SIZE_PIXEL;
                        pOut += _MAGNIFICATION * SIZE_PIXEL;
                }
                pIn += srcPitch - (Xres * SIZE_PIXEL);
                pOut += dstPitch - (_MAGNIFICATION * Xres * SIZE_PIXEL);
                pOut += (_MAGNIFICATION - 1) * dstPitch;
        }
}

#ifdef _32BIT
#ifdef _HQ3X
void hq3x32_32(unsigned char *pIn,  unsigned int srcPitch, unsigned char *, unsigned char *pOut, unsigned int dstPitch, int Xres, int Yres)
{
        hq3x32(pIn, srcPitch, 0, pOut, dstPitch, Xres, Yres);
}
#endif
#ifdef _HQ4X
void hq4x32_32(unsigned char *pIn,  unsigned int srcPitch, unsigned char *, unsigned char *pOut, unsigned int dstPitch, int Xres, int Yres)
{
        hq4x32(pIn, srcPitch, 0, pOut, dstPitch, Xres, Yres);
}
#endif
#endif

#undef SIZE_PIXEL
#undef COLORTYPE
#undef _MAGNIFICATION
#undef RGBtoYUV
#undef Interp1
#undef Interp2
#undef Interp3
#undef Interp4
#undef Interp5
#undef Interp6
#undef Interp7
#undef Interp8
