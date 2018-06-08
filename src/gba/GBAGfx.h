#ifndef GFX_H
#define GFX_H

#include "GBA.h"
#include "Globals.h"

#include "../common/Port.h"

//#define SPRITE_DEBUG

#ifdef TILED_RENDERING
extern void gfxDrawTextScreen(uint16_t, uint16_t, uint16_t, uint32_t*);
#else
static void gfxDrawTextScreen(uint16_t, uint16_t, uint16_t, uint32_t*);
#endif
static void gfxDrawRotScreen(uint16_t, uint16_t, uint16_t, uint16_t, uint16_t, uint16_t, uint16_t, uint16_t, uint16_t, int&, int&, int, uint32_t*);
static void gfxDrawRotScreen16Bit(uint16_t, uint16_t, uint16_t, uint16_t, uint16_t, uint16_t, uint16_t, uint16_t, uint16_t, int&, int&, int,
    uint32_t*);
static void gfxDrawRotScreen256(uint16_t, uint16_t, uint16_t, uint16_t, uint16_t, uint16_t, uint16_t, uint16_t, uint16_t, int&, int&, int,
    uint32_t*);
static void gfxDrawRotScreen16Bit160(uint16_t, uint16_t, uint16_t, uint16_t, uint16_t, uint16_t, uint16_t, uint16_t, uint16_t, int&, int&, int,
    uint32_t*);
static void gfxDrawSprites(uint32_t*);
static void gfxIncreaseBrightness(uint32_t* line, int coeff);
static void gfxDecreaseBrightness(uint32_t* line, int coeff);
static void gfxAlphaBlend(uint32_t* ta, uint32_t* tb, int ca, int cb);

void mode0RenderLine();
void mode0RenderLineNoWindow();
void mode0RenderLineAll();

void mode1RenderLine();
void mode1RenderLineNoWindow();
void mode1RenderLineAll();

void mode2RenderLine();
void mode2RenderLineNoWindow();
void mode2RenderLineAll();

void mode3RenderLine();
void mode3RenderLineNoWindow();
void mode3RenderLineAll();

void mode4RenderLine();
void mode4RenderLineNoWindow();
void mode4RenderLineAll();

void mode5RenderLine();
void mode5RenderLineNoWindow();
void mode5RenderLineAll();

extern int coeff[32];
extern uint32_t line0[240];
extern uint32_t line1[240];
extern uint32_t line2[240];
extern uint32_t line3[240];
extern uint32_t lineOBJ[240];
extern uint32_t lineOBJWin[240];
extern uint32_t lineMix[240];
extern bool gfxInWin0[240];
extern bool gfxInWin1[240];
extern int lineOBJpixleft[128];

extern int gfxBG2Changed;
extern int gfxBG3Changed;

extern int gfxBG2X;
extern int gfxBG2Y;
extern int gfxBG3X;
extern int gfxBG3Y;
extern int gfxLastVCOUNT;

static inline void gfxClearArray(uint32_t* array)
{
    for (int i = 0; i < 240; i++) {
        *array++ = 0x80000000;
    }
}

#ifndef TILED_RENDERING
static inline void gfxDrawTextScreen(uint16_t control, uint16_t hofs, uint16_t vofs, uint32_t* line)
{
    uint16_t* palette = (uint16_t*)paletteRAM;
    uint8_t* charBase = &vram[((control >> 2) & 0x03) * 0x4000];
    uint16_t* screenBase = (uint16_t*)&vram[((control >> 8) & 0x1f) * 0x800];
    uint32_t prio = ((control & 3) << 25) + 0x1000000;
    int sizeX = 256;
    int sizeY = 256;
    switch ((control >> 14) & 3) {
    case 0:
        break;
    case 1:
        sizeX = 512;
        break;
    case 2:
        sizeY = 512;
        break;
    case 3:
        sizeX = 512;
        sizeY = 512;
        break;
    }

    int maskX = sizeX - 1;
    int maskY = sizeY - 1;

    bool mosaicOn = (control & 0x40) ? true : false;

    int xxx = hofs & maskX;
    int yyy = (vofs + VCOUNT) & maskY;
    int mosaicX = (MOSAIC & 0x000F) + 1;
    int mosaicY = ((MOSAIC & 0x00F0) >> 4) + 1;

    if (mosaicOn) {
        if ((VCOUNT % mosaicY) != 0) {
            mosaicY = VCOUNT - (VCOUNT % mosaicY);
            yyy = (vofs + mosaicY) & maskY;
        }
    }

    if (yyy > 255 && sizeY > 256) {
        yyy &= 255;
        screenBase += 0x400;
        if (sizeX > 256)
            screenBase += 0x400;
    }

    int yshift = ((yyy >> 3) << 5);
    if ((control)&0x80) {
        uint16_t* screenSource = screenBase + 0x400 * (xxx >> 8) + ((xxx & 255) >> 3) + yshift;
        for (int x = 0; x < 240; x++) {
            uint16_t data = READ16LE(screenSource);

            int tile = data & 0x3FF;
            int tileX = (xxx & 7);
            int tileY = yyy & 7;

            if (tileX == 7)
                screenSource++;

            if (data & 0x0400)
                tileX = 7 - tileX;
            if (data & 0x0800)
                tileY = 7 - tileY;

            uint8_t color = charBase[tile * 64 + tileY * 8 + tileX];

            line[x] = color ? (READ16LE(&palette[color]) | prio) : 0x80000000;

            xxx++;
            if (xxx == 256) {
                if (sizeX > 256)
                    screenSource = screenBase + 0x400 + yshift;
                else {
                    screenSource = screenBase + yshift;
                    xxx = 0;
                }
            } else if (xxx >= sizeX) {
                xxx = 0;
                screenSource = screenBase + yshift;
            }
        }
    } else {
        uint16_t* screenSource = screenBase + 0x400 * (xxx >> 8) + ((xxx & 255) >> 3) + yshift;
        for (int x = 0; x < 240; x++) {
            uint16_t data = READ16LE(screenSource);

            int tile = data & 0x3FF;
            int tileX = (xxx & 7);
            int tileY = yyy & 7;

            if (tileX == 7)
                screenSource++;

            if (data & 0x0400)
                tileX = 7 - tileX;
            if (data & 0x0800)
                tileY = 7 - tileY;

            uint8_t color = charBase[(tile << 5) + (tileY << 2) + (tileX >> 1)];

            if (tileX & 1) {
                color = (color >> 4);
            } else {
                color &= 0x0F;
            }

            int pal = (data >> 8) & 0xF0;
            line[x] = color ? (READ16LE(&palette[pal + color]) | prio) : 0x80000000;

            xxx++;
            if (xxx == 256) {
                if (sizeX > 256)
                    screenSource = screenBase + 0x400 + yshift;
                else {
                    screenSource = screenBase + yshift;
                    xxx = 0;
                }
            } else if (xxx >= sizeX) {
                xxx = 0;
                screenSource = screenBase + yshift;
            }
        }
    }
    if (mosaicOn) {
        if (mosaicX > 1) {
            int m = 1;
            for (int i = 0; i < 239; i++) {
                line[i + 1] = line[i];
                m++;
                if (m == mosaicX) {
                    m = 1;
                    i++;
                }
            }
        }
    }
}
#endif // !__TILED_RENDERING

static inline void gfxDrawRotScreen(uint16_t control, uint16_t x_l, uint16_t x_h, uint16_t y_l, uint16_t y_h, uint16_t pa, uint16_t pb,
    uint16_t pc, uint16_t pd, int& currentX, int& currentY, int changed,
    uint32_t* line)
{
    uint16_t* palette = (uint16_t*)paletteRAM;
    uint8_t* charBase = &vram[((control >> 2) & 0x03) * 0x4000];
    uint8_t* screenBase = (uint8_t*)&vram[((control >> 8) & 0x1f) * 0x800];
    int prio = ((control & 3) << 25) + 0x1000000;

    int sizeX = 128;
    int sizeY = 128;
    switch ((control >> 14) & 3) {
    case 0:
        break;
    case 1:
        sizeX = sizeY = 256;
        break;
    case 2:
        sizeX = sizeY = 512;
        break;
    case 3:
        sizeX = sizeY = 1024;
        break;
    }

    int maskX = sizeX - 1;
    int maskY = sizeY - 1;

    int yshift = ((control >> 14) & 3) + 4;

    int dx = pa & 0x7FFF;
    if (pa & 0x8000)
        dx |= 0xFFFF8000;
    int dmx = pb & 0x7FFF;
    if (pb & 0x8000)
        dmx |= 0xFFFF8000;
    int dy = pc & 0x7FFF;
    if (pc & 0x8000)
        dy |= 0xFFFF8000;
    int dmy = pd & 0x7FFF;
    if (pd & 0x8000)
        dmy |= 0xFFFF8000;

    if (VCOUNT == 0)
        changed = 3;

    if (changed & 1) {
        currentX = (x_l) | ((x_h & 0x07FF) << 16);
        if (x_h & 0x0800)
            currentX |= 0xF8000000;
    } else {
        currentX += dmx;
    }

    if (changed & 2) {
        currentY = (y_l) | ((y_h & 0x07FF) << 16);
        if (y_h & 0x0800)
            currentY |= 0xF8000000;
    } else {
        currentY += dmy;
    }

    int realX = currentX;
    int realY = currentY;

    if (control & 0x40) {
        int mosaicY = ((MOSAIC & 0xF0) >> 4) + 1;
        int y = (VCOUNT % mosaicY);
        realX -= y * dmx;
        realY -= y * dmy;
    }

    if (control & 0x2000) {
        for (int x = 0; x < 240; x++) {
            int xxx = (realX >> 8) & maskX;
            int yyy = (realY >> 8) & maskY;

            int tile = screenBase[(xxx >> 3) + ((yyy >> 3) << yshift)];

            int tileX = (xxx & 7);
            int tileY = yyy & 7;

            uint8_t color = charBase[(tile << 6) + (tileY << 3) + tileX];

            line[x] = color ? (READ16LE(&palette[color]) | prio) : 0x80000000;

            realX += dx;
            realY += dy;
        }
    } else {
        for (int x = 0; x < 240; x++) {
            int xxx = (realX >> 8);
            int yyy = (realY >> 8);

            if (xxx < 0 || yyy < 0 || xxx >= sizeX || yyy >= sizeY) {
                line[x] = 0x80000000;
            } else {
                int tile = screenBase[(xxx >> 3) + ((yyy >> 3) << yshift)];

                int tileX = (xxx & 7);
                int tileY = yyy & 7;

                uint8_t color = charBase[(tile << 6) + (tileY << 3) + tileX];

                line[x] = color ? (READ16LE(&palette[color]) | prio) : 0x80000000;
            }
            realX += dx;
            realY += dy;
        }
    }

    if (control & 0x40) {
        int mosaicX = (MOSAIC & 0xF) + 1;
        if (mosaicX > 1) {
            int m = 1;
            for (int i = 0; i < 239; i++) {
                line[i + 1] = line[i];
                m++;
                if (m == mosaicX) {
                    m = 1;
                    i++;
                }
            }
        }
    }
}

static inline void gfxDrawRotScreen16Bit(uint16_t control, uint16_t x_l, uint16_t x_h, uint16_t y_l, uint16_t y_h, uint16_t pa,
    uint16_t pb, uint16_t pc, uint16_t pd, int& currentX, int& currentY,
    int changed, uint32_t* line)
{
    uint16_t* screenBase = (uint16_t*)&vram[0];
    int prio = ((control & 3) << 25) + 0x1000000;
    int sizeX = 240;
    int sizeY = 160;

    int startX = (x_l) | ((x_h & 0x07FF) << 16);
    if (x_h & 0x0800)
        startX |= 0xF8000000;
    int startY = (y_l) | ((y_h & 0x07FF) << 16);
    if (y_h & 0x0800)
        startY |= 0xF8000000;

    int dx = pa & 0x7FFF;
    if (pa & 0x8000)
        dx |= 0xFFFF8000;
    int dmx = pb & 0x7FFF;
    if (pb & 0x8000)
        dmx |= 0xFFFF8000;
    int dy = pc & 0x7FFF;
    if (pc & 0x8000)
        dy |= 0xFFFF8000;
    int dmy = pd & 0x7FFF;
    if (pd & 0x8000)
        dmy |= 0xFFFF8000;

    if (VCOUNT == 0)
        changed = 3;

    if (changed & 1) {
        currentX = (x_l) | ((x_h & 0x07FF) << 16);
        if (x_h & 0x0800)
            currentX |= 0xF8000000;
    } else
        currentX += dmx;

    if (changed & 2) {
        currentY = (y_l) | ((y_h & 0x07FF) << 16);
        if (y_h & 0x0800)
            currentY |= 0xF8000000;
    } else {
        currentY += dmy;
    }

    int realX = currentX;
    int realY = currentY;

    if (control & 0x40) {
        int mosaicY = ((MOSAIC & 0xF0) >> 4) + 1;
        int y = (VCOUNT % mosaicY);
        realX -= y * dmx;
        realY -= y * dmy;
    }

    int xxx = (realX >> 8);
    int yyy = (realY >> 8);

    for (int x = 0; x < 240; x++) {
        if (xxx < 0 || yyy < 0 || xxx >= sizeX || yyy >= sizeY) {
            line[x] = 0x80000000;
        } else {
            line[x] = (READ16LE(&screenBase[yyy * sizeX + xxx]) | prio);
        }
        realX += dx;
        realY += dy;

        xxx = (realX >> 8);
        yyy = (realY >> 8);
    }

    if (control & 0x40) {
        int mosaicX = (MOSAIC & 0xF) + 1;
        if (mosaicX > 1) {
            int m = 1;
            for (int i = 0; i < 239; i++) {
                line[i + 1] = line[i];
                m++;
                if (m == mosaicX) {
                    m = 1;
                    i++;
                }
            }
        }
    }
}

static inline void gfxDrawRotScreen256(uint16_t control, uint16_t x_l, uint16_t x_h, uint16_t y_l, uint16_t y_h, uint16_t pa,
    uint16_t pb, uint16_t pc, uint16_t pd, int& currentX, int& currentY,
    int changed, uint32_t* line)
{
    uint16_t* palette = (uint16_t*)paletteRAM;
    uint8_t* screenBase = (DISPCNT & 0x0010) ? &vram[0xA000] : &vram[0x0000];
    int prio = ((control & 3) << 25) + 0x1000000;
    int sizeX = 240;
    int sizeY = 160;

    int startX = (x_l) | ((x_h & 0x07FF) << 16);
    if (x_h & 0x0800)
        startX |= 0xF8000000;
    int startY = (y_l) | ((y_h & 0x07FF) << 16);
    if (y_h & 0x0800)
        startY |= 0xF8000000;

    int dx = pa & 0x7FFF;
    if (pa & 0x8000)
        dx |= 0xFFFF8000;
    int dmx = pb & 0x7FFF;
    if (pb & 0x8000)
        dmx |= 0xFFFF8000;
    int dy = pc & 0x7FFF;
    if (pc & 0x8000)
        dy |= 0xFFFF8000;
    int dmy = pd & 0x7FFF;
    if (pd & 0x8000)
        dmy |= 0xFFFF8000;

    if (VCOUNT == 0)
        changed = 3;

    if (changed & 1) {
        currentX = (x_l) | ((x_h & 0x07FF) << 16);
        if (x_h & 0x0800)
            currentX |= 0xF8000000;
    } else {
        currentX += dmx;
    }

    if (changed & 2) {
        currentY = (y_l) | ((y_h & 0x07FF) << 16);
        if (y_h & 0x0800)
            currentY |= 0xF8000000;
    } else {
        currentY += dmy;
    }

    int realX = currentX;
    int realY = currentY;

    if (control & 0x40) {
        int mosaicY = ((MOSAIC & 0xF0) >> 4) + 1;
        int y = VCOUNT - (VCOUNT % mosaicY);
        realX = startX + y * dmx;
        realY = startY + y * dmy;
    }

    int xxx = (realX >> 8);
    int yyy = (realY >> 8);

    for (int x = 0; x < 240; x++) {
        if (xxx < 0 || yyy < 0 || xxx >= sizeX || yyy >= sizeY) {
            line[x] = 0x80000000;
        } else {
            uint8_t color = screenBase[yyy * 240 + xxx];

            line[x] = color ? (READ16LE(&palette[color]) | prio) : 0x80000000;
        }
        realX += dx;
        realY += dy;

        xxx = (realX >> 8);
        yyy = (realY >> 8);
    }

    if (control & 0x40) {
        int mosaicX = (MOSAIC & 0xF) + 1;
        if (mosaicX > 1) {
            int m = 1;
            for (int i = 0; i < 239; i++) {
                line[i + 1] = line[i];
                m++;
                if (m == mosaicX) {
                    m = 1;
                    i++;
                }
            }
        }
    }
}

static inline void gfxDrawRotScreen16Bit160(uint16_t control, uint16_t x_l, uint16_t x_h, uint16_t y_l, uint16_t y_h, uint16_t pa,
    uint16_t pb, uint16_t pc, uint16_t pd, int& currentX, int& currentY,
    int changed, uint32_t* line)
{
    uint16_t* screenBase = (DISPCNT & 0x0010) ? (uint16_t*)&vram[0xa000] : (uint16_t*)&vram[0];
    int prio = ((control & 3) << 25) + 0x1000000;
    int sizeX = 160;
    int sizeY = 128;

    int startX = (x_l) | ((x_h & 0x07FF) << 16);
    if (x_h & 0x0800)
        startX |= 0xF8000000;
    int startY = (y_l) | ((y_h & 0x07FF) << 16);
    if (y_h & 0x0800)
        startY |= 0xF8000000;

    int dx = pa & 0x7FFF;
    if (pa & 0x8000)
        dx |= 0xFFFF8000;
    int dmx = pb & 0x7FFF;
    if (pb & 0x8000)
        dmx |= 0xFFFF8000;
    int dy = pc & 0x7FFF;
    if (pc & 0x8000)
        dy |= 0xFFFF8000;
    int dmy = pd & 0x7FFF;
    if (pd & 0x8000)
        dmy |= 0xFFFF8000;

    if (VCOUNT == 0)
        changed = 3;

    if (changed & 1) {
        currentX = (x_l) | ((x_h & 0x07FF) << 16);
        if (x_h & 0x0800)
            currentX |= 0xF8000000;
    } else {
        currentX += dmx;
    }

    if (changed & 2) {
        currentY = (y_l) | ((y_h & 0x07FF) << 16);
        if (y_h & 0x0800)
            currentY |= 0xF8000000;
    } else {
        currentY += dmy;
    }

    int realX = currentX;
    int realY = currentY;

    if (control & 0x40) {
        int mosaicY = ((MOSAIC & 0xF0) >> 4) + 1;
        int y = VCOUNT - (VCOUNT % mosaicY);
        realX = startX + y * dmx;
        realY = startY + y * dmy;
    }

    int xxx = (realX >> 8);
    int yyy = (realY >> 8);

    for (int x = 0; x < 240; x++) {
        if (xxx < 0 || yyy < 0 || xxx >= sizeX || yyy >= sizeY) {
            line[x] = 0x80000000;
        } else {
            line[x] = (READ16LE(&screenBase[yyy * sizeX + xxx]) | prio);
        }
        realX += dx;
        realY += dy;

        xxx = (realX >> 8);
        yyy = (realY >> 8);
    }

    if (control & 0x40) {
        int mosaicX = (MOSAIC & 0xF) + 1;
        if (mosaicX > 1) {
            int m = 1;
            for (int i = 0; i < 239; i++) {
                line[i + 1] = line[i];
                m++;
                if (m == mosaicX) {
                    m = 1;
                    i++;
                }
            }
        }
    }
}

static inline void gfxDrawSprites(uint32_t* lineOBJ)
{
    // lineOBJpix is used to keep track of the drawn OBJs
    // and to stop drawing them if the 'maximum number of OBJ per line'
    // has been reached.
    int lineOBJpix = (DISPCNT & 0x20) ? 954 : 1226;
    int m = 0;
    gfxClearArray(lineOBJ);
    if (layerEnable & 0x1000) {
        uint16_t* sprites = (uint16_t*)oam;
        uint16_t* spritePalette = &((uint16_t*)paletteRAM)[256];
        int mosaicY = ((MOSAIC & 0xF000) >> 12) + 1;
        int mosaicX = ((MOSAIC & 0xF00) >> 8) + 1;
        for (int x = 0; x < 128; x++) {
            uint16_t a0 = READ16LE(sprites++);
            uint16_t a1 = READ16LE(sprites++);
            uint16_t a2 = READ16LE(sprites++);
            sprites++;

            lineOBJpixleft[x] = lineOBJpix;

            lineOBJpix -= 2;
            if (lineOBJpix <= 0)
                continue;

            if ((a0 & 0x0c00) == 0x0c00)
                a0 &= 0xF3FF;

            if ((a0 >> 14) == 3) {
                a0 &= 0x3FFF;
                a1 &= 0x3FFF;
            }

            int sizeX = 8 << (a1 >> 14);
            int sizeY = sizeX;

            if ((a0 >> 14) & 1) {
                if (sizeX < 32)
                    sizeX <<= 1;
                if (sizeY > 8)
                    sizeY >>= 1;
            } else if ((a0 >> 14) & 2) {
                if (sizeX > 8)
                    sizeX >>= 1;
                if (sizeY < 32)
                    sizeY <<= 1;
            }

#ifdef SPRITE_DEBUG
            int maskX = sizeX - 1;
            int maskY = sizeY - 1;
#endif

            int sy = (a0 & 255);
            int sx = (a1 & 0x1FF);

            // computes ticks used by OBJ-WIN if OBJWIN is enabled
            if (((a0 & 0x0c00) == 0x0800) && (layerEnable & 0x8000)) {
                if ((a0 & 0x0300) == 0x0300) {
                    sizeX <<= 1;
                    sizeY <<= 1;
                }
                if ((sy + sizeY) > 256)
                    sy -= 256;
                if ((sx + sizeX) > 512)
                    sx -= 512;
                if (sx < 0) {
                    sizeX += sx;
                    sx = 0;
                } else if ((sx + sizeX) > 240)
                    sizeX = 240 - sx;
                if ((VCOUNT >= sy) && (VCOUNT < sy + sizeY) && (sx < 240)) {
                    if (a0 & 0x0100)
                        lineOBJpix -= 8 + 2 * sizeX;
                    else
                        lineOBJpix -= sizeX - 2;
                }
                continue;
            }
            // else ignores OBJ-WIN if OBJWIN is disabled, and ignored disabled OBJ
            else if (((a0 & 0x0c00) == 0x0800) || ((a0 & 0x0300) == 0x0200))
                continue;

            if (lineOBJpix < 0)
                continue;

            if (a0 & 0x0100) {
                int fieldX = sizeX;
                int fieldY = sizeY;
                if (a0 & 0x0200) {
                    fieldX <<= 1;
                    fieldY <<= 1;
                }
                if ((sy + fieldY) > 256)
                    sy -= 256;
                int t = VCOUNT - sy;
                if ((t >= 0) && (t < fieldY)) {
                    int startpix = 0;
                    if ((sx + fieldX) > 512) {
                        startpix = 512 - sx;
                    }
                    if (lineOBJpix > 0)
                        if ((sx < 240) || startpix) {
                            lineOBJpix -= 8;
                            // int t2 = t - (fieldY >> 1);
                            int rot = (a1 >> 9) & 0x1F;
                            uint16_t* OAM = (uint16_t*)oam;
                            int dx = READ16LE(&OAM[3 + (rot << 4)]);
                            if (dx & 0x8000)
                                dx |= 0xFFFF8000;
                            int dmx = READ16LE(&OAM[7 + (rot << 4)]);
                            if (dmx & 0x8000)
                                dmx |= 0xFFFF8000;
                            int dy = READ16LE(&OAM[11 + (rot << 4)]);
                            if (dy & 0x8000)
                                dy |= 0xFFFF8000;
                            int dmy = READ16LE(&OAM[15 + (rot << 4)]);
                            if (dmy & 0x8000)
                                dmy |= 0xFFFF8000;

                            if (a0 & 0x1000) {
                                t -= (t % mosaicY);
                            }

                            int realX = ((sizeX) << 7) - (fieldX >> 1) * dx - (fieldY >> 1) * dmx + t * dmx;
                            int realY = ((sizeY) << 7) - (fieldX >> 1) * dy - (fieldY >> 1) * dmy + t * dmy;

                            uint32_t prio = (((a2 >> 10) & 3) << 25) | ((a0 & 0x0c00) << 6);

                            if (a0 & 0x2000) {
                                int c = (a2 & 0x3FF);
                                if ((DISPCNT & 7) > 2 && (c < 512))
                                    continue;
                                int inc = 32;
                                if (DISPCNT & 0x40)
                                    inc = sizeX >> 2;
                                else
                                    c &= 0x3FE;
                                for (int x = 0; x < fieldX; x++) {
                                    if (x >= startpix)
                                        lineOBJpix -= 2;
                                    if (lineOBJpix < 0)
                                        continue;
                                    int xxx = realX >> 8;
                                    int yyy = realY >> 8;

                                    if (xxx < 0 || xxx >= sizeX || yyy < 0 || yyy >= sizeY || sx >= 240)
                                        ;
                                    else {
                                        uint32_t color = vram
                                            [0x10000 + ((((c + (yyy >> 3) * inc)
                                                             << 5)
                                                            + ((yyy & 7)
                                                                  << 3)
                                                            + ((xxx >> 3)
                                                                  << 6)
                                                            + (xxx & 7))
                                                           & 0x7FFF)];
                                        if ((color == 0) && (((prio >> 25) & 3) < ((lineOBJ
                                                                                           [sx]
                                                                                       >> 25)
                                                                                      & 3))) {
                                            lineOBJ[sx] = (lineOBJ
                                                                  [sx]
                                                              & 0xF9FFFFFF)
                                                | prio;
                                            if ((a0 & 0x1000) && m)
                                                lineOBJ[sx] = (lineOBJ
                                                                      [sx - 1]
                                                                  & 0xF9FFFFFF)
                                                    | prio;
                                        } else if (
                                            (color) && (prio < (lineOBJ[sx] & 0xFF000000))) {
                                            lineOBJ[sx] = READ16LE(
                                                              &spritePalette
                                                                  [color])
                                                | prio;
                                            if ((a0 & 0x1000) && m)
                                                lineOBJ[sx] = (lineOBJ
                                                                      [sx - 1]
                                                                  & 0xF9FFFFFF)
                                                    | prio;
                                        }

                                        if (a0 & 0x1000) {
                                            m++;
                                            if (m == mosaicX)
                                                m = 0;
                                        }
#ifdef SPRITE_DEBUG
                                        if (t == 0 || t == maskY || x == 0 || x == maskX)
                                            lineOBJ[sx] = 0x001F;
#endif
                                    }
                                    sx = (sx + 1) & 511;
                                    realX += dx;
                                    realY += dy;
                                }
                            } else {
                                int c = (a2 & 0x3FF);
                                if ((DISPCNT & 7) > 2 && (c < 512))
                                    continue;

                                int inc = 32;
                                if (DISPCNT & 0x40)
                                    inc = sizeX >> 3;
                                int palette = (a2 >> 8) & 0xF0;
                                for (int x = 0; x < fieldX; x++) {
                                    if (x >= startpix)
                                        lineOBJpix -= 2;
                                    if (lineOBJpix < 0)
                                        continue;
                                    int xxx = realX >> 8;
                                    int yyy = realY >> 8;
                                    if (xxx < 0 || xxx >= sizeX || yyy < 0 || yyy >= sizeY || sx >= 240)
                                        ;
                                    else {
                                        uint32_t color = vram
                                            [0x10000 + ((((c + (yyy >> 3) * inc)
                                                             << 5)
                                                            + ((yyy & 7)
                                                                  << 2)
                                                            + ((xxx >> 3)
                                                                  << 5)
                                                            + ((xxx & 7) >> 1))
                                                           & 0x7FFF)];
                                        if (xxx & 1)
                                            color >>= 4;
                                        else
                                            color &= 0x0F;

                                        if ((color == 0) && (((prio >> 25) & 3) < ((lineOBJ
                                                                                           [sx]
                                                                                       >> 25)
                                                                                      & 3))) {
                                            lineOBJ[sx] = (lineOBJ
                                                                  [sx]
                                                              & 0xF9FFFFFF)
                                                | prio;
                                            if ((a0 & 0x1000) && m)
                                                lineOBJ[sx] = (lineOBJ
                                                                      [sx - 1]
                                                                  & 0xF9FFFFFF)
                                                    | prio;
                                        } else if (
                                            (color) && (prio < (lineOBJ[sx] & 0xFF000000))) {
                                            lineOBJ[sx] = READ16LE(
                                                              &spritePalette
                                                                  [palette + color])
                                                | prio;
                                            if ((a0 & 0x1000) && m)
                                                lineOBJ[sx] = (lineOBJ
                                                                      [sx - 1]
                                                                  & 0xF9FFFFFF)
                                                    | prio;
                                        }
                                    }
                                    if ((a0 & 0x1000) && m) {
                                        m++;
                                        if (m == mosaicX)
                                            m = 0;
                                    }

#ifdef SPRITE_DEBUG
                                    if (t == 0 || t == maskY || x == 0 || x == maskX)
                                        lineOBJ[sx] = 0x001F;
#endif
                                    sx = (sx + 1) & 511;
                                    realX += dx;
                                    realY += dy;
                                }
                            }
                        }
                }
            } else {
                if (sy + sizeY > 256)
                    sy -= 256;
                int t = VCOUNT - sy;
                if ((t >= 0) && (t < sizeY)) {
                    int startpix = 0;
                    if ((sx + sizeX) > 512) {
                        startpix = 512 - sx;
                    }
                    if ((sx < 240) || startpix) {
                        lineOBJpix += 2;
                        if (a0 & 0x2000) {
                            if (a1 & 0x2000)
                                t = sizeY - t - 1;
                            int c = (a2 & 0x3FF);
                            if ((DISPCNT & 7) > 2 && (c < 512))
                                continue;

                            int inc = 32;
                            if (DISPCNT & 0x40) {
                                inc = sizeX >> 2;
                            } else {
                                c &= 0x3FE;
                            }
                            int xxx = 0;
                            if (a1 & 0x1000)
                                xxx = sizeX - 1;

                            if (a0 & 0x1000) {
                                t -= (t % mosaicY);
                            }

                            int address = 0x10000 + ((((c + (t >> 3) * inc) << 5) + ((t & 7) << 3) + ((xxx >> 3) << 6) + (xxx & 7)) & 0x7FFF);

                            if (a1 & 0x1000)
                                xxx = 7;
                            uint32_t prio = (((a2 >> 10) & 3) << 25) | ((a0 & 0x0c00) << 6);

                            for (int xx = 0; xx < sizeX; xx++) {
                                if (xx >= startpix)
                                    lineOBJpix--;
                                if (lineOBJpix < 0)
                                    continue;
                                if (sx < 240) {
                                    uint8_t color = vram[address];
                                    if ((color == 0) && (((prio >> 25) & 3) < ((lineOBJ[sx] >> 25) & 3))) {
                                        lineOBJ[sx] = (lineOBJ[sx] & 0xF9FFFFFF) | prio;
                                        if ((a0 & 0x1000) && m)
                                            lineOBJ[sx] = (lineOBJ
                                                                  [sx - 1]
                                                              & 0xF9FFFFFF)
                                                | prio;
                                    } else if ((color) && (prio < (lineOBJ[sx] & 0xFF000000))) {
                                        lineOBJ[sx] = READ16LE(
                                                          &spritePalette
                                                              [color])
                                            | prio;
                                        if ((a0 & 0x1000) && m)
                                            lineOBJ[sx] = (lineOBJ
                                                                  [sx - 1]
                                                              & 0xF9FFFFFF)
                                                | prio;
                                    }

                                    if (a0 & 0x1000) {
                                        m++;
                                        if (m == mosaicX)
                                            m = 0;
                                    }

#ifdef SPRITE_DEBUG
                                    if (t == 0 || t == maskY || xx == 0 || xx == maskX)
                                        lineOBJ[sx] = 0x001F;
#endif
                                }

                                sx = (sx + 1) & 511;
                                if (a1 & 0x1000) {
                                    xxx--;
                                    address--;
                                    if (xxx == -1) {
                                        address -= 56;
                                        xxx = 7;
                                    }
                                    if (address < 0x10000)
                                        address += 0x8000;
                                } else {
                                    xxx++;
                                    address++;
                                    if (xxx == 8) {
                                        address += 56;
                                        xxx = 0;
                                    }
                                    if (address > 0x17fff)
                                        address -= 0x8000;
                                }
                            }
                        } else {
                            if (a1 & 0x2000)
                                t = sizeY - t - 1;
                            int c = (a2 & 0x3FF);
                            if ((DISPCNT & 7) > 2 && (c < 512))
                                continue;

                            int inc = 32;
                            if (DISPCNT & 0x40) {
                                inc = sizeX >> 3;
                            }
                            int xxx = 0;
                            if (a1 & 0x1000)
                                xxx = sizeX - 1;

                            if (a0 & 0x1000) {
                                t -= (t % mosaicY);
                            }

                            int address = 0x10000 + ((((c + (t >> 3) * inc) << 5) + ((t & 7) << 2) + ((xxx >> 3) << 5) + ((xxx & 7) >> 1)) & 0x7FFF);
                            uint32_t prio = (((a2 >> 10) & 3) << 25) | ((a0 & 0x0c00) << 6);
                            int palette = (a2 >> 8) & 0xF0;
                            if (a1 & 0x1000) {
                                xxx = 7;
                                for (int xx = sizeX - 1; xx >= 0;
                                     xx--) {
                                    if (xx >= startpix)
                                        lineOBJpix--;
                                    if (lineOBJpix < 0)
                                        continue;
                                    if (sx < 240) {
                                        uint8_t color = vram[address];
                                        if (xx & 1) {
                                            color = (color >> 4);
                                        } else
                                            color &= 0x0F;

                                        if ((color == 0) && (((prio >> 25) & 3) < ((lineOBJ
                                                                                           [sx]
                                                                                       >> 25)
                                                                                      & 3))) {
                                            lineOBJ[sx] = (lineOBJ
                                                                  [sx]
                                                              & 0xF9FFFFFF)
                                                | prio;
                                            if ((a0 & 0x1000) && m)
                                                lineOBJ[sx] = (lineOBJ
                                                                      [sx - 1]
                                                                  & 0xF9FFFFFF)
                                                    | prio;
                                        } else if (
                                            (color) && (prio < (lineOBJ[sx] & 0xFF000000))) {
                                            lineOBJ[sx] = READ16LE(
                                                              &spritePalette
                                                                  [palette + color])
                                                | prio;
                                            if ((a0 & 0x1000) && m)
                                                lineOBJ[sx] = (lineOBJ
                                                                      [sx - 1]
                                                                  & 0xF9FFFFFF)
                                                    | prio;
                                        }
                                    }
                                    if (a0 & 0x1000) {
                                        m++;
                                        if (m == mosaicX)
                                            m = 0;
                                    }
#ifdef SPRITE_DEBUG
                                    if (t == 0 || t == maskY || xx == 0 || xx == maskX)
                                        lineOBJ[sx] = 0x001F;
#endif
                                    sx = (sx + 1) & 511;
                                    xxx--;
                                    if (!(xx & 1))
                                        address--;
                                    if (xxx == -1) {
                                        xxx = 7;
                                        address -= 28;
                                    }
                                    if (address < 0x10000)
                                        address += 0x8000;
                                }
                            } else {
                                for (int xx = 0; xx < sizeX; xx++) {
                                    if (xx >= startpix)
                                        lineOBJpix--;
                                    if (lineOBJpix < 0)
                                        continue;
                                    if (sx < 240) {
                                        uint8_t color = vram[address];
                                        if (xx & 1) {
                                            color = (color >> 4);
                                        } else
                                            color &= 0x0F;

                                        if ((color == 0) && (((prio >> 25) & 3) < ((lineOBJ
                                                                                           [sx]
                                                                                       >> 25)
                                                                                      & 3))) {
                                            lineOBJ[sx] = (lineOBJ
                                                                  [sx]
                                                              & 0xF9FFFFFF)
                                                | prio;
                                            if ((a0 & 0x1000) && m)
                                                lineOBJ[sx] = (lineOBJ
                                                                      [sx - 1]
                                                                  & 0xF9FFFFFF)
                                                    | prio;
                                        } else if (
                                            (color) && (prio < (lineOBJ[sx] & 0xFF000000))) {
                                            lineOBJ[sx] = READ16LE(
                                                              &spritePalette
                                                                  [palette + color])
                                                | prio;
                                            if ((a0 & 0x1000) && m)
                                                lineOBJ[sx] = (lineOBJ
                                                                      [sx - 1]
                                                                  & 0xF9FFFFFF)
                                                    | prio;
                                        }
                                    }
                                    if (a0 & 0x1000) {
                                        m++;
                                        if (m == mosaicX)
                                            m = 0;
                                    }
#ifdef SPRITE_DEBUG
                                    if (t == 0 || t == maskY || xx == 0 || xx == maskX)
                                        lineOBJ[sx] = 0x001F;
#endif
                                    sx = (sx + 1) & 511;
                                    xxx++;
                                    if (xx & 1)
                                        address++;
                                    if (xxx == 8) {
                                        address += 28;
                                        xxx = 0;
                                    }
                                    if (address > 0x17fff)
                                        address -= 0x8000;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

static inline void gfxDrawOBJWin(uint32_t* lineOBJWin)
{
    gfxClearArray(lineOBJWin);
    if ((layerEnable & 0x9000) == 0x9000) {
        uint16_t* sprites = (uint16_t*)oam;
        // uint16_t *spritePalette = &((uint16_t *)paletteRAM)[256];
        for (int x = 0; x < 128; x++) {
            int lineOBJpix = lineOBJpixleft[x];
            uint16_t a0 = READ16LE(sprites++);
            uint16_t a1 = READ16LE(sprites++);
            uint16_t a2 = READ16LE(sprites++);
            sprites++;

            if (lineOBJpix <= 0)
                continue;

            // ignores non OBJ-WIN and disabled OBJ-WIN
            if (((a0 & 0x0c00) != 0x0800) || ((a0 & 0x0300) == 0x0200))
                continue;

            if ((a0 & 0x0c00) == 0x0c00)
                a0 &= 0xF3FF;

            if ((a0 >> 14) == 3) {
                a0 &= 0x3FFF;
                a1 &= 0x3FFF;
            }

            int sizeX = 8 << (a1 >> 14);
            int sizeY = sizeX;

            if ((a0 >> 14) & 1) {
                if (sizeX < 32)
                    sizeX <<= 1;
                if (sizeY > 8)
                    sizeY >>= 1;
            } else if ((a0 >> 14) & 2) {
                if (sizeX > 8)
                    sizeX >>= 1;
                if (sizeY < 32)
                    sizeY <<= 1;
            }

            int sy = (a0 & 255);

            if (a0 & 0x0100) {
                int fieldX = sizeX;
                int fieldY = sizeY;
                if (a0 & 0x0200) {
                    fieldX <<= 1;
                    fieldY <<= 1;
                }
                if ((sy + fieldY) > 256)
                    sy -= 256;
                int t = VCOUNT - sy;
                if ((t >= 0) && (t < fieldY)) {
                    int sx = (a1 & 0x1FF);
                    int startpix = 0;
                    if ((sx + fieldX) > 512) {
                        startpix = 512 - sx;
                    }
                    if ((sx < 240) || startpix) {
                        lineOBJpix -= 8;
                        // int t2 = t - (fieldY >> 1);
                        int rot = (a1 >> 9) & 0x1F;
                        uint16_t* OAM = (uint16_t*)oam;
                        int dx = READ16LE(&OAM[3 + (rot << 4)]);
                        if (dx & 0x8000)
                            dx |= 0xFFFF8000;
                        int dmx = READ16LE(&OAM[7 + (rot << 4)]);
                        if (dmx & 0x8000)
                            dmx |= 0xFFFF8000;
                        int dy = READ16LE(&OAM[11 + (rot << 4)]);
                        if (dy & 0x8000)
                            dy |= 0xFFFF8000;
                        int dmy = READ16LE(&OAM[15 + (rot << 4)]);
                        if (dmy & 0x8000)
                            dmy |= 0xFFFF8000;

                        int realX = ((sizeX) << 7) - (fieldX >> 1) * dx - (fieldY >> 1) * dmx + t * dmx;
                        int realY = ((sizeY) << 7) - (fieldX >> 1) * dy - (fieldY >> 1) * dmy + t * dmy;

                        // uint32_t prio = (((a2 >> 10) & 3) << 25) | ((a0 &
                        // 0x0c00)<<6);

                        if (a0 & 0x2000) {
                            int c = (a2 & 0x3FF);
                            if ((DISPCNT & 7) > 2 && (c < 512))
                                continue;
                            int inc = 32;
                            if (DISPCNT & 0x40)
                                inc = sizeX >> 2;
                            else
                                c &= 0x3FE;
                            for (int x = 0; x < fieldX; x++) {
                                if (x >= startpix)
                                    lineOBJpix -= 2;
                                if (lineOBJpix < 0)
                                    continue;
                                int xxx = realX >> 8;
                                int yyy = realY >> 8;

                                if (xxx < 0 || xxx >= sizeX || yyy < 0 || yyy >= sizeY || sx >= 240) {
                                } else {
                                    uint32_t color = vram
                                        [0x10000 + ((((c + (yyy >> 3) * inc)
                                                         << 5)
                                                        + ((yyy & 7) << 3) + ((xxx >> 3) << 6) + (xxx & 7))
                                                       & 0x7fff)];
                                    if (color) {
                                        lineOBJWin[sx] = 1;
                                    }
                                }
                                sx = (sx + 1) & 511;
                                realX += dx;
                                realY += dy;
                            }
                        } else {
                            int c = (a2 & 0x3FF);
                            if ((DISPCNT & 7) > 2 && (c < 512))
                                continue;

                            int inc = 32;
                            if (DISPCNT & 0x40)
                                inc = sizeX >> 3;
                            // int palette = (a2 >> 8) & 0xF0;
                            for (int x = 0; x < fieldX; x++) {
                                if (x >= startpix)
                                    lineOBJpix -= 2;
                                if (lineOBJpix < 0)
                                    continue;
                                int xxx = realX >> 8;
                                int yyy = realY >> 8;

                                //              if(x == 0 || x ==
                                //              (sizeX-1) ||
                                //                 t == 0 || t ==
                                //                 (sizeY-1)) {
                                //                lineOBJ[sx] =
                                //                0x001F | prio;
                                //              } else {
                                if (xxx < 0 || xxx >= sizeX || yyy < 0 || yyy >= sizeY || sx >= 240) {
                                } else {
                                    uint32_t color = vram
                                        [0x10000 + ((((c + (yyy >> 3) * inc)
                                                         << 5)
                                                        + ((yyy & 7) << 2) + ((xxx >> 3) << 5) + ((xxx & 7) >> 1))
                                                       & 0x7fff)];
                                    if (xxx & 1)
                                        color >>= 4;
                                    else
                                        color &= 0x0F;

                                    if (color) {
                                        lineOBJWin[sx] = 1;
                                    }
                                }
                                //            }
                                sx = (sx + 1) & 511;
                                realX += dx;
                                realY += dy;
                            }
                        }
                    }
                }
            } else {
                if ((sy + sizeY) > 256)
                    sy -= 256;
                int t = VCOUNT - sy;
                if ((t >= 0) && (t < sizeY)) {
                    int sx = (a1 & 0x1FF);
                    int startpix = 0;
                    if ((sx + sizeX) > 512) {
                        startpix = 512 - sx;
                    }
                    if ((sx < 240) || startpix) {
                        lineOBJpix += 2;
                        if (a0 & 0x2000) {
                            if (a1 & 0x2000)
                                t = sizeY - t - 1;
                            int c = (a2 & 0x3FF);
                            if ((DISPCNT & 7) > 2 && (c < 512))
                                continue;

                            int inc = 32;
                            if (DISPCNT & 0x40) {
                                inc = sizeX >> 2;
                            } else {
                                c &= 0x3FE;
                            }
                            int xxx = 0;
                            if (a1 & 0x1000)
                                xxx = sizeX - 1;
                            int address = 0x10000 + ((((c + (t >> 3) * inc) << 5) + ((t & 7) << 3) + ((xxx >> 3) << 6) + (xxx & 7)) & 0x7fff);
                            if (a1 & 0x1000)
                                xxx = 7;
                            // uint32_t prio = (((a2 >> 10) & 3) << 25) |
                            // ((a0 & 0x0c00)<<6);
                            for (int xx = 0; xx < sizeX; xx++) {
                                if (xx >= startpix)
                                    lineOBJpix--;
                                if (lineOBJpix < 0)
                                    continue;
                                if (sx < 240) {
                                    uint8_t color = vram[address];
                                    if (color) {
                                        lineOBJWin[sx] = 1;
                                    }
                                }

                                sx = (sx + 1) & 511;
                                if (a1 & 0x1000) {
                                    xxx--;
                                    address--;
                                    if (xxx == -1) {
                                        address -= 56;
                                        xxx = 7;
                                    }
                                    if (address < 0x10000)
                                        address += 0x8000;
                                } else {
                                    xxx++;
                                    address++;
                                    if (xxx == 8) {
                                        address += 56;
                                        xxx = 0;
                                    }
                                    if (address > 0x17fff)
                                        address -= 0x8000;
                                }
                            }
                        } else {
                            if (a1 & 0x2000)
                                t = sizeY - t - 1;
                            int c = (a2 & 0x3FF);
                            if ((DISPCNT & 7) > 2 && (c < 512))
                                continue;

                            int inc = 32;
                            if (DISPCNT & 0x40) {
                                inc = sizeX >> 3;
                            }
                            int xxx = 0;
                            if (a1 & 0x1000)
                                xxx = sizeX - 1;
                            int address = 0x10000 + ((((c + (t >> 3) * inc) << 5) + ((t & 7) << 2) + ((xxx >> 3) << 5) + ((xxx & 7) >> 1)) & 0x7fff);
                            // uint32_t prio = (((a2 >> 10) & 3) << 25) |
                            // ((a0 & 0x0c00)<<6);
                            // int palette = (a2 >> 8) & 0xF0;
                            if (a1 & 0x1000) {
                                xxx = 7;
                                for (int xx = sizeX - 1; xx >= 0;
                                     xx--) {
                                    if (xx >= startpix)
                                        lineOBJpix--;
                                    if (lineOBJpix < 0)
                                        continue;
                                    if (sx < 240) {
                                        uint8_t color = vram[address];
                                        if (xx & 1) {
                                            color = (color >> 4);
                                        } else
                                            color &= 0x0F;

                                        if (color) {
                                            lineOBJWin
                                                [sx]
                                                = 1;
                                        }
                                    }
                                    sx = (sx + 1) & 511;
                                    xxx--;
                                    if (!(xx & 1))
                                        address--;
                                    if (xxx == -1) {
                                        xxx = 7;
                                        address -= 28;
                                    }
                                    if (address < 0x10000)
                                        address += 0x8000;
                                }
                            } else {
                                for (int xx = 0; xx < sizeX; xx++) {
                                    if (xx >= startpix)
                                        lineOBJpix--;
                                    if (lineOBJpix < 0)
                                        continue;
                                    if (sx < 240) {
                                        uint8_t color = vram[address];
                                        if (xx & 1) {
                                            color = (color >> 4);
                                        } else
                                            color &= 0x0F;

                                        if (color) {
                                            lineOBJWin
                                                [sx]
                                                = 1;
                                        }
                                    }
                                    sx = (sx + 1) & 511;
                                    xxx++;
                                    if (xx & 1)
                                        address++;
                                    if (xxx == 8) {
                                        address += 28;
                                        xxx = 0;
                                    }
                                    if (address > 0x17fff)
                                        address -= 0x8000;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

static inline uint32_t gfxIncreaseBrightness(uint32_t color, int coeff)
{
    color &= 0xffff;
    color = ((color << 16) | color) & 0x3E07C1F;

    color = color + (((0x3E07C1F - color) * coeff) >> 4);
    color &= 0x3E07C1F;

    return (color >> 16) | color;
}

static inline void gfxIncreaseBrightness(uint32_t* line, int coeff)
{
    for (int x = 0; x < 240; x++) {
        uint32_t color = *line;
        int r = (color & 0x1F);
        int g = ((color >> 5) & 0x1F);
        int b = ((color >> 10) & 0x1F);

        r = r + (((31 - r) * coeff) >> 4);
        g = g + (((31 - g) * coeff) >> 4);
        b = b + (((31 - b) * coeff) >> 4);
        if (r > 31)
            r = 31;
        if (g > 31)
            g = 31;
        if (b > 31)
            b = 31;
        *line++ = (color & 0xFFFF0000) | (b << 10) | (g << 5) | r;
    }
}

static inline uint32_t gfxDecreaseBrightness(uint32_t color, int coeff)
{
    color &= 0xffff;
    color = ((color << 16) | color) & 0x3E07C1F;

    color = color - (((color * coeff) >> 4) & 0x3E07C1F);

    return (color >> 16) | color;
}

static inline void gfxDecreaseBrightness(uint32_t* line, int coeff)
{
    for (int x = 0; x < 240; x++) {
        uint32_t color = *line;
        int r = (color & 0x1F);
        int g = ((color >> 5) & 0x1F);
        int b = ((color >> 10) & 0x1F);

        r = r - ((r * coeff) >> 4);
        g = g - ((g * coeff) >> 4);
        b = b - ((b * coeff) >> 4);
        if (r < 0)
            r = 0;
        if (g < 0)
            g = 0;
        if (b < 0)
            b = 0;
        *line++ = (color & 0xFFFF0000) | (b << 10) | (g << 5) | r;
    }
}

static inline uint32_t gfxAlphaBlend(uint32_t color, uint32_t color2, int ca, int cb)
{
    if (color < 0x80000000) {
        color &= 0xffff;
        color2 &= 0xffff;

        color = ((color << 16) | color) & 0x03E07C1F;
        color2 = ((color2 << 16) | color2) & 0x03E07C1F;
        color = ((color * ca) + (color2 * cb)) >> 4;

        if ((ca + cb) > 16) {
            if (color & 0x20)
                color |= 0x1f;
            if (color & 0x8000)
                color |= 0x7C00;
            if (color & 0x4000000)
                color |= 0x03E00000;
        }

        color &= 0x03E07C1F;
        color = (color >> 16) | color;
    }
    return color;
}

static inline void gfxAlphaBlend(uint32_t* ta, uint32_t* tb, int ca, int cb)
{
    for (int x = 0; x < 240; x++) {
        uint32_t color = *ta;
        if (color < 0x80000000) {
            int r = (color & 0x1F);
            int g = ((color >> 5) & 0x1F);
            int b = ((color >> 10) & 0x1F);
            uint32_t color2 = (*tb++);
            int r0 = (color2 & 0x1F);
            int g0 = ((color2 >> 5) & 0x1F);
            int b0 = ((color2 >> 10) & 0x1F);

            r = ((r * ca) + (r0 * cb)) >> 4;
            g = ((g * ca) + (g0 * cb)) >> 4;
            b = ((b * ca) + (b0 * cb)) >> 4;

            if (r > 31)
                r = 31;
            if (g > 31)
                g = 31;
            if (b > 31)
                b = 31;

            *ta++ = (color & 0xFFFF0000) | (b << 10) | (g << 5) | r;
        } else {
            ta++;
            tb++;
        }
    }
}

#endif // GFX_H
