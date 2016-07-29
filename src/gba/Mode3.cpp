#include "GBA.h"
#include "GBAGfx.h"
#include "Globals.h"

void mode3RenderLine()
{
    uint16_t* palette = (uint16_t*)paletteRAM;

    if (DISPCNT & 0x80) {
        for (int x = 0; x < 240; x++) {
            lineMix[x] = 0x7fff;
        }
        gfxLastVCOUNT = VCOUNT;
        return;
    }

    if (layerEnable & 0x0400) {
        int changed = gfxBG2Changed;

        if (gfxLastVCOUNT > VCOUNT)
            changed = 3;

        gfxDrawRotScreen16Bit(BG2CNT, BG2X_L, BG2X_H,
            BG2Y_L, BG2Y_H, BG2PA, BG2PB,
            BG2PC, BG2PD,
            gfxBG2X, gfxBG2Y, changed,
            line2);
    }

    gfxDrawSprites(lineOBJ);

    uint32_t background;
    if (customBackdropColor == -1) {
        background = (READ16LE(&palette[0]) | 0x30000000);
    } else {
        background = ((customBackdropColor & 0x7FFF) | 0x30000000);
    }

    for (int x = 0; x < 240; x++) {
        uint32_t color = background;
        uint8_t top = 0x20;

        if (line2[x] < color) {
            color = line2[x];
            top = 0x04;
        }

        if ((uint8_t)(lineOBJ[x] >> 24) < (uint8_t)(color >> 24)) {
            color = lineOBJ[x];
            top = 0x10;
        }

        if ((top & 0x10) && (color & 0x00010000)) {
            // semi-transparent OBJ
            uint32_t back = background;
            uint8_t top2 = 0x20;

            if (line2[x] < back) {
                back = line2[x];
                top2 = 0x04;
            }

            if (top2 & (BLDMOD >> 8))
                color = gfxAlphaBlend(color, back,
                    coeff[COLEV & 0x1F],
                    coeff[(COLEV >> 8) & 0x1F]);
            else {
                switch ((BLDMOD >> 6) & 3) {
                case 2:
                    if (BLDMOD & top)
                        color = gfxIncreaseBrightness(color, coeff[COLY & 0x1F]);
                    break;
                case 3:
                    if (BLDMOD & top)
                        color = gfxDecreaseBrightness(color, coeff[COLY & 0x1F]);
                    break;
                }
            }
        }

        lineMix[x] = color;
    }
    gfxBG2Changed = 0;
    gfxLastVCOUNT = VCOUNT;
}

void mode3RenderLineNoWindow()
{
    uint16_t* palette = (uint16_t*)paletteRAM;

    if (DISPCNT & 0x80) {
        for (int x = 0; x < 240; x++) {
            lineMix[x] = 0x7fff;
        }
        gfxLastVCOUNT = VCOUNT;
        return;
    }

    if (layerEnable & 0x0400) {
        int changed = gfxBG2Changed;

        if (gfxLastVCOUNT > VCOUNT)
            changed = 3;

        gfxDrawRotScreen16Bit(BG2CNT, BG2X_L, BG2X_H,
            BG2Y_L, BG2Y_H, BG2PA, BG2PB,
            BG2PC, BG2PD,
            gfxBG2X, gfxBG2Y, changed,
            line2);
    }

    gfxDrawSprites(lineOBJ);

    uint32_t background;
    if (customBackdropColor == -1) {
        background = (READ16LE(&palette[0]) | 0x30000000);
    } else {
        background = ((customBackdropColor & 0x7FFF) | 0x30000000);
    }

    for (int x = 0; x < 240; x++) {
        uint32_t color = background;
        uint8_t top = 0x20;

        if (line2[x] < color) {
            color = line2[x];
            top = 0x04;
        }

        if ((uint8_t)(lineOBJ[x] >> 24) < (uint8_t)(color >> 24)) {
            color = lineOBJ[x];
            top = 0x10;
        }

        if (!(color & 0x00010000)) {
            switch ((BLDMOD >> 6) & 3) {
            case 0:
                break;
            case 1: {
                if (top & BLDMOD) {
                    uint32_t back = background;
                    uint8_t top2 = 0x20;

                    if (line2[x] < back) {
                        if (top != 0x04) {
                            back = line2[x];
                            top2 = 0x04;
                        }
                    }

                    if ((uint8_t)(lineOBJ[x] >> 24) < (uint8_t)(back >> 24)) {
                        if (top != 0x10) {
                            back = lineOBJ[x];
                            top2 = 0x10;
                        }
                    }

                    if (top2 & (BLDMOD >> 8))
                        color = gfxAlphaBlend(color, back,
                            coeff[COLEV & 0x1F],
                            coeff[(COLEV >> 8) & 0x1F]);
                }
            } break;
            case 2:
                if (BLDMOD & top)
                    color = gfxIncreaseBrightness(color, coeff[COLY & 0x1F]);
                break;
            case 3:
                if (BLDMOD & top)
                    color = gfxDecreaseBrightness(color, coeff[COLY & 0x1F]);
                break;
            }
        } else {
            // semi-transparent OBJ
            uint32_t back = background;
            uint8_t top2 = 0x20;

            if (line2[x] < back) {
                back = line2[x];
                top2 = 0x04;
            }

            if (top2 & (BLDMOD >> 8))
                color = gfxAlphaBlend(color, back,
                    coeff[COLEV & 0x1F],
                    coeff[(COLEV >> 8) & 0x1F]);
            else {
                switch ((BLDMOD >> 6) & 3) {
                case 2:
                    if (BLDMOD & top)
                        color = gfxIncreaseBrightness(color, coeff[COLY & 0x1F]);
                    break;
                case 3:
                    if (BLDMOD & top)
                        color = gfxDecreaseBrightness(color, coeff[COLY & 0x1F]);
                    break;
                }
            }
        }

        lineMix[x] = color;
    }
    gfxBG2Changed = 0;
    gfxLastVCOUNT = VCOUNT;
}

void mode3RenderLineAll()
{
    uint16_t* palette = (uint16_t*)paletteRAM;

    if (DISPCNT & 0x80) {
        for (int x = 0; x < 240; x++) {
            lineMix[x] = 0x7fff;
        }
        gfxLastVCOUNT = VCOUNT;
        return;
    }

    bool inWindow0 = false;
    bool inWindow1 = false;

    if (layerEnable & 0x2000) {
        uint8_t v0 = WIN0V >> 8;
        uint8_t v1 = WIN0V & 255;
        inWindow0 = ((v0 == v1) && (v0 >= 0xe8));
        if (v1 >= v0)
            inWindow0 |= (VCOUNT >= v0 && VCOUNT < v1);
        else
            inWindow0 |= (VCOUNT >= v0 || VCOUNT < v1);
    }
    if (layerEnable & 0x4000) {
        uint8_t v0 = WIN1V >> 8;
        uint8_t v1 = WIN1V & 255;
        inWindow1 = ((v0 == v1) && (v0 >= 0xe8));
        if (v1 >= v0)
            inWindow1 |= (VCOUNT >= v0 && VCOUNT < v1);
        else
            inWindow1 |= (VCOUNT >= v0 || VCOUNT < v1);
    }

    if (layerEnable & 0x0400) {
        int changed = gfxBG2Changed;

        if (gfxLastVCOUNT > VCOUNT)
            changed = 3;

        gfxDrawRotScreen16Bit(BG2CNT, BG2X_L, BG2X_H,
            BG2Y_L, BG2Y_H, BG2PA, BG2PB,
            BG2PC, BG2PD,
            gfxBG2X, gfxBG2Y, changed,
            line2);
    }

    gfxDrawSprites(lineOBJ);
    gfxDrawOBJWin(lineOBJWin);

    uint8_t inWin0Mask = WININ & 0xFF;
    uint8_t inWin1Mask = WININ >> 8;
    uint8_t outMask = WINOUT & 0xFF;

    uint32_t background;
    if (customBackdropColor == -1) {
        background = (READ16LE(&palette[0]) | 0x30000000);
    } else {
        background = ((customBackdropColor & 0x7FFF) | 0x30000000);
    }

    for (int x = 0; x < 240; x++) {
        uint32_t color = background;
        uint8_t top = 0x20;
        uint8_t mask = outMask;

        if (!(lineOBJWin[x] & 0x80000000)) {
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

        if ((mask & 4) && (line2[x] < color)) {
            color = line2[x];
            top = 0x04;
        }

        if ((mask & 16) && ((uint8_t)(lineOBJ[x] >> 24) < (uint8_t)(color >> 24))) {
            color = lineOBJ[x];
            top = 0x10;
        }

        if (color & 0x00010000) {
            // semi-transparent OBJ
            uint32_t back = background;
            uint8_t top2 = 0x20;

            if ((mask & 4) && line2[x] < back) {
                back = line2[x];
                top2 = 0x04;
            }

            if (top2 & (BLDMOD >> 8))
                color = gfxAlphaBlend(color, back,
                    coeff[COLEV & 0x1F],
                    coeff[(COLEV >> 8) & 0x1F]);
            else {
                switch ((BLDMOD >> 6) & 3) {
                case 2:
                    if (BLDMOD & top)
                        color = gfxIncreaseBrightness(color, coeff[COLY & 0x1F]);
                    break;
                case 3:
                    if (BLDMOD & top)
                        color = gfxDecreaseBrightness(color, coeff[COLY & 0x1F]);
                    break;
                }
            }
        } else if (mask & 32) {
            switch ((BLDMOD >> 6) & 3) {
            case 0:
                break;
            case 1: {
                if (top & BLDMOD) {
                    uint32_t back = background;
                    uint8_t top2 = 0x20;

                    if ((mask & 4) && line2[x] < back) {
                        if (top != 0x04) {
                            back = line2[x];
                            top2 = 0x04;
                        }
                    }

                    if ((mask & 16) && (uint8_t)(lineOBJ[x] >> 24) < (uint8_t)(back >> 24)) {
                        if (top != 0x10) {
                            back = lineOBJ[x];
                            top2 = 0x10;
                        }
                    }

                    if (top2 & (BLDMOD >> 8))
                        color = gfxAlphaBlend(color, back,
                            coeff[COLEV & 0x1F],
                            coeff[(COLEV >> 8) & 0x1F]);
                }
            } break;
            case 2:
                if (BLDMOD & top)
                    color = gfxIncreaseBrightness(color, coeff[COLY & 0x1F]);
                break;
            case 3:
                if (BLDMOD & top)
                    color = gfxDecreaseBrightness(color, coeff[COLY & 0x1F]);
                break;
            }
        }

        lineMix[x] = color;
    }
    gfxBG2Changed = 0;
    gfxLastVCOUNT = VCOUNT;
}
