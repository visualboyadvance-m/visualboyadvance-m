#include "core/gba/gbaGfx.h"

#include "core/gba/gbaGlobals.h"

void mode0RenderLine()
{
    uint16_t* palette = (uint16_t*)g_paletteRAM;

    if (DISPCNT & 0x80) {
        for (int x = 0; x < 240; x++) {
            g_lineMix[x] = 0x7fff;
        }
        return;
    }

    if (coreOptions.layerEnable & 0x0100) {
        gfxDrawTextScreen(BG0CNT, BG0HOFS, BG0VOFS, g_line0);
    }

    if (coreOptions.layerEnable & 0x0200) {
        gfxDrawTextScreen(BG1CNT, BG1HOFS, BG1VOFS, g_line1);
    }

    if (coreOptions.layerEnable & 0x0400) {
        gfxDrawTextScreen(BG2CNT, BG2HOFS, BG2VOFS, g_line2);
    }

    if (coreOptions.layerEnable & 0x0800) {
        gfxDrawTextScreen(BG3CNT, BG3HOFS, BG3VOFS, g_line3);
    }

    gfxDrawSprites(g_lineOBJ);

    uint32_t backdrop;
    if (customBackdropColor == -1) {
        backdrop = (READ16LE(&palette[0]) | 0x30000000);
    } else {
        backdrop = ((customBackdropColor & 0x7FFF) | 0x30000000);
    }

    for (int x = 0; x < 240; x++) {
        uint32_t color = backdrop;
        uint8_t top = 0x20;

        if (g_line0[x] < color) {
            color = g_line0[x];
            top = 0x01;
        }

        if ((uint8_t)(g_line1[x] >> 24) < (uint8_t)(color >> 24)) {
            color = g_line1[x];
            top = 0x02;
        }

        if ((uint8_t)(g_line2[x] >> 24) < (uint8_t)(color >> 24)) {
            color = g_line2[x];
            top = 0x04;
        }

        if ((uint8_t)(g_line3[x] >> 24) < (uint8_t)(color >> 24)) {
            color = g_line3[x];
            top = 0x08;
        }

        if ((uint8_t)(g_lineOBJ[x] >> 24) < (uint8_t)(color >> 24)) {
            color = g_lineOBJ[x];
            top = 0x10;
        }

        if ((top & 0x10) && (color & 0x00010000)) {
            // semi-transparent OBJ
            uint32_t back = backdrop;
            uint8_t top2 = 0x20;

            if ((uint8_t)(g_line0[x] >> 24) < (uint8_t)(back >> 24)) {
                back = g_line0[x];
                top2 = 0x01;
            }

            if ((uint8_t)(g_line1[x] >> 24) < (uint8_t)(back >> 24)) {
                back = g_line1[x];
                top2 = 0x02;
            }

            if ((uint8_t)(g_line2[x] >> 24) < (uint8_t)(back >> 24)) {
                back = g_line2[x];
                top2 = 0x04;
            }

            if ((uint8_t)(g_line3[x] >> 24) < (uint8_t)(back >> 24)) {
                back = g_line3[x];
                top2 = 0x08;
            }

            if (top2 & (BLDMOD >> 8))
                color = gfxAlphaBlend(color, back,
                    g_coeff[COLEV & 0x1F],
                    g_coeff[(COLEV >> 8) & 0x1F]);
            else {
                switch ((BLDMOD >> 6) & 3) {
                case 2:
                    if (BLDMOD & top)
                        color = gfxIncreaseBrightness(color, g_coeff[COLY & 0x1F]);
                    break;
                case 3:
                    if (BLDMOD & top)
                        color = gfxDecreaseBrightness(color, g_coeff[COLY & 0x1F]);
                    break;
                }
            }
        }

        g_lineMix[x] = color;
    }
}

void mode0RenderLineNoWindow()
{
    uint16_t* palette = (uint16_t*)g_paletteRAM;

    if (DISPCNT & 0x80) {
        for (int x = 0; x < 240; x++) {
            g_lineMix[x] = 0x7fff;
        }
        return;
    }

    if (coreOptions.layerEnable & 0x0100) {
        gfxDrawTextScreen(BG0CNT, BG0HOFS, BG0VOFS, g_line0);
    }

    if (coreOptions.layerEnable & 0x0200) {
        gfxDrawTextScreen(BG1CNT, BG1HOFS, BG1VOFS, g_line1);
    }

    if (coreOptions.layerEnable & 0x0400) {
        gfxDrawTextScreen(BG2CNT, BG2HOFS, BG2VOFS, g_line2);
    }

    if (coreOptions.layerEnable & 0x0800) {
        gfxDrawTextScreen(BG3CNT, BG3HOFS, BG3VOFS, g_line3);
    }

    gfxDrawSprites(g_lineOBJ);

    uint32_t backdrop;
    if (customBackdropColor == -1) {
        backdrop = (READ16LE(&palette[0]) | 0x30000000);
    } else {
        backdrop = ((customBackdropColor & 0x7FFF) | 0x30000000);
    }

    int effect = (BLDMOD >> 6) & 3;

    for (int x = 0; x < 240; x++) {
        uint32_t color = backdrop;
        uint8_t top = 0x20;

        if (g_line0[x] < color) {
            color = g_line0[x];
            top = 0x01;
        }

        if (g_line1[x] < (color & 0xFF000000)) {
            color = g_line1[x];
            top = 0x02;
        }

        if (g_line2[x] < (color & 0xFF000000)) {
            color = g_line2[x];
            top = 0x04;
        }

        if (g_line3[x] < (color & 0xFF000000)) {
            color = g_line3[x];
            top = 0x08;
        }

        if (g_lineOBJ[x] < (color & 0xFF000000)) {
            color = g_lineOBJ[x];
            top = 0x10;
        }

        if (!(color & 0x00010000)) {
            switch (effect) {
            case 0:
                break;
            case 1: {
                if (top & BLDMOD) {
                    uint32_t back = backdrop;
                    uint8_t top2 = 0x20;
                    if (g_line0[x] < back) {
                        if (top != 0x01) {
                            back = g_line0[x];
                            top2 = 0x01;
                        }
                    }

                    if (g_line1[x] < (back & 0xFF000000)) {
                        if (top != 0x02) {
                            back = g_line1[x];
                            top2 = 0x02;
                        }
                    }

                    if (g_line2[x] < (back & 0xFF000000)) {
                        if (top != 0x04) {
                            back = g_line2[x];
                            top2 = 0x04;
                        }
                    }

                    if (g_line3[x] < (back & 0xFF000000)) {
                        if (top != 0x08) {
                            back = g_line3[x];
                            top2 = 0x08;
                        }
                    }

                    if (g_lineOBJ[x] < (back & 0xFF000000)) {
                        if (top != 0x10) {
                            back = g_lineOBJ[x];
                            top2 = 0x10;
                        }
                    }

                    if (top2 & (BLDMOD >> 8))
                        color = gfxAlphaBlend(color, back,
                            g_coeff[COLEV & 0x1F],
                            g_coeff[(COLEV >> 8) & 0x1F]);
                }
            } break;
            case 2:
                if (BLDMOD & top)
                    color = gfxIncreaseBrightness(color, g_coeff[COLY & 0x1F]);
                break;
            case 3:
                if (BLDMOD & top)
                    color = gfxDecreaseBrightness(color, g_coeff[COLY & 0x1F]);
                break;
            }
        } else {
            // semi-transparent OBJ
            uint32_t back = backdrop;
            uint8_t top2 = 0x20;

            if (g_line0[x] < back) {
                back = g_line0[x];
                top2 = 0x01;
            }

            if (g_line1[x] < (back & 0xFF000000)) {
                back = g_line1[x];
                top2 = 0x02;
            }

            if (g_line2[x] < (back & 0xFF000000)) {
                back = g_line2[x];
                top2 = 0x04;
            }

            if (g_line3[x] < (back & 0xFF000000)) {
                back = g_line3[x];
                top2 = 0x08;
            }

            if (top2 & (BLDMOD >> 8))
                color = gfxAlphaBlend(color, back,
                    g_coeff[COLEV & 0x1F],
                    g_coeff[(COLEV >> 8) & 0x1F]);
            else {
                switch ((BLDMOD >> 6) & 3) {
                case 2:
                    if (BLDMOD & top)
                        color = gfxIncreaseBrightness(color, g_coeff[COLY & 0x1F]);
                    break;
                case 3:
                    if (BLDMOD & top)
                        color = gfxDecreaseBrightness(color, g_coeff[COLY & 0x1F]);
                    break;
                }
            }
        }

        g_lineMix[x] = color;
    }
}

void mode0RenderLineAll()
{
    uint16_t* palette = (uint16_t*)g_paletteRAM;

    if (DISPCNT & 0x80) {
        for (int x = 0; x < 240; x++) {
            g_lineMix[x] = 0x7fff;
        }
        return;
    }

    bool inWindow0 = false;
    bool inWindow1 = false;

    if (coreOptions.layerEnable & 0x2000) {
        uint8_t v0 = WIN0V >> 8;
        uint8_t v1 = WIN0V & 255;
        inWindow0 = ((v0 == v1) && (v0 >= 0xe8));
        if (v1 >= v0)
            inWindow0 |= (VCOUNT >= v0 && VCOUNT < v1);
        else
            inWindow0 |= (VCOUNT >= v0 || VCOUNT < v1);
    }
    if (coreOptions.layerEnable & 0x4000) {
        uint8_t v0 = WIN1V >> 8;
        uint8_t v1 = WIN1V & 255;
        inWindow1 = ((v0 == v1) && (v0 >= 0xe8));
        if (v1 >= v0)
            inWindow1 |= (VCOUNT >= v0 && VCOUNT < v1);
        else
            inWindow1 |= (VCOUNT >= v0 || VCOUNT < v1);
    }

    if ((coreOptions.layerEnable & 0x0100)) {
        gfxDrawTextScreen(BG0CNT, BG0HOFS, BG0VOFS, g_line0);
    }

    if ((coreOptions.layerEnable & 0x0200)) {
        gfxDrawTextScreen(BG1CNT, BG1HOFS, BG1VOFS, g_line1);
    }

    if ((coreOptions.layerEnable & 0x0400)) {
        gfxDrawTextScreen(BG2CNT, BG2HOFS, BG2VOFS, g_line2);
    }

    if ((coreOptions.layerEnable & 0x0800)) {
        gfxDrawTextScreen(BG3CNT, BG3HOFS, BG3VOFS, g_line3);
    }

    gfxDrawSprites(g_lineOBJ);
    gfxDrawOBJWin(g_lineOBJWin);

    uint32_t backdrop;
    if (customBackdropColor == -1) {
        backdrop = (READ16LE(&palette[0]) | 0x30000000);
    } else {
        backdrop = ((customBackdropColor & 0x7FFF) | 0x30000000);
    }

    uint8_t inWin0Mask = WININ & 0xFF;
    uint8_t inWin1Mask = WININ >> 8;
    uint8_t outMask = WINOUT & 0xFF;

    for (int x = 0; x < 240; x++) {
        uint32_t color = backdrop;
        uint8_t top = 0x20;
        uint8_t mask = outMask;

        if (!(g_lineOBJWin[x] & 0x80000000)) {
            mask = WINOUT >> 8;
        }

        if (inWindow1) {
            if (gfxInWin1[x])
                mask = inWin1Mask;
        }

        if (inWindow0) {
            if (gfxInWin0[x]) {
                mask = inWin0Mask;
            }
        }

        if ((mask & 1) && (g_line0[x] < color)) {
            color = g_line0[x];
            top = 0x01;
        }

        if ((mask & 2) && ((uint8_t)(g_line1[x] >> 24) < (uint8_t)(color >> 24))) {
            color = g_line1[x];
            top = 0x02;
        }

        if ((mask & 4) && ((uint8_t)(g_line2[x] >> 24) < (uint8_t)(color >> 24))) {
            color = g_line2[x];
            top = 0x04;
        }

        if ((mask & 8) && ((uint8_t)(g_line3[x] >> 24) < (uint8_t)(color >> 24))) {
            color = g_line3[x];
            top = 0x08;
        }

        if ((mask & 16) && ((uint8_t)(g_lineOBJ[x] >> 24) < (uint8_t)(color >> 24))) {
            color = g_lineOBJ[x];
            top = 0x10;
        }

        if (color & 0x00010000) {
            // semi-transparent OBJ
            uint32_t back = backdrop;
            uint8_t top2 = 0x20;

            if ((mask & 1) && ((uint8_t)(g_line0[x] >> 24) < (uint8_t)(back >> 24))) {
                back = g_line0[x];
                top2 = 0x01;
            }

            if ((mask & 2) && ((uint8_t)(g_line1[x] >> 24) < (uint8_t)(back >> 24))) {
                back = g_line1[x];
                top2 = 0x02;
            }

            if ((mask & 4) && ((uint8_t)(g_line2[x] >> 24) < (uint8_t)(back >> 24))) {
                back = g_line2[x];
                top2 = 0x04;
            }

            if ((mask & 8) && ((uint8_t)(g_line3[x] >> 24) < (uint8_t)(back >> 24))) {
                back = g_line3[x];
                top2 = 0x08;
            }

            if (top2 & (BLDMOD >> 8))
                color = gfxAlphaBlend(color, back,
                    g_coeff[COLEV & 0x1F],
                    g_coeff[(COLEV >> 8) & 0x1F]);
            else {
                switch ((BLDMOD >> 6) & 3) {
                case 2:
                    if (BLDMOD & top)
                        color = gfxIncreaseBrightness(color, g_coeff[COLY & 0x1F]);
                    break;
                case 3:
                    if (BLDMOD & top)
                        color = gfxDecreaseBrightness(color, g_coeff[COLY & 0x1F]);
                    break;
                }
            }
        } else if (mask & 32) {
            // special FX on in the window
            switch ((BLDMOD >> 6) & 3) {
            case 0:
                break;
            case 1: {
                if (top & BLDMOD) {
                    uint32_t back = backdrop;
                    uint8_t top2 = 0x20;
                    if ((mask & 1) && (uint8_t)(g_line0[x] >> 24) < (uint8_t)(back >> 24)) {
                        if (top != 0x01) {
                            back = g_line0[x];
                            top2 = 0x01;
                        }
                    }

                    if ((mask & 2) && (uint8_t)(g_line1[x] >> 24) < (uint8_t)(back >> 24)) {
                        if (top != 0x02) {
                            back = g_line1[x];
                            top2 = 0x02;
                        }
                    }

                    if ((mask & 4) && (uint8_t)(g_line2[x] >> 24) < (uint8_t)(back >> 24)) {
                        if (top != 0x04) {
                            back = g_line2[x];
                            top2 = 0x04;
                        }
                    }

                    if ((mask & 8) && (uint8_t)(g_line3[x] >> 24) < (uint8_t)(back >> 24)) {
                        if (top != 0x08) {
                            back = g_line3[x];
                            top2 = 0x08;
                        }
                    }

                    if ((mask & 16) && (uint8_t)(g_lineOBJ[x] >> 24) < (uint8_t)(back >> 24)) {
                        if (top != 0x10) {
                            back = g_lineOBJ[x];
                            top2 = 0x10;
                        }
                    }

                    if (top2 & (BLDMOD >> 8))
                        color = gfxAlphaBlend(color, back,
                            g_coeff[COLEV & 0x1F],
                            g_coeff[(COLEV >> 8) & 0x1F]);
                }
            } break;
            case 2:
                if (BLDMOD & top)
                    color = gfxIncreaseBrightness(color, g_coeff[COLY & 0x1F]);
                break;
            case 3:
                if (BLDMOD & top)
                    color = gfxDecreaseBrightness(color, g_coeff[COLY & 0x1F]);
                break;
            }
        }

        g_lineMix[x] = color;
    }
}
