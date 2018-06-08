#include <string.h>
#include "GBAGfx.h"
#include "../System.h"

int coeff[32] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16
};

uint32_t line0[240];
uint32_t line1[240];
uint32_t line2[240];
uint32_t line3[240];
uint32_t lineOBJ[240];
uint32_t lineOBJWin[240];
uint32_t lineMix[240];
bool gfxInWin0[240];
bool gfxInWin1[240];
int lineOBJpixleft[128];

int gfxBG2Changed = 0;
int gfxBG3Changed = 0;

int gfxBG2X = 0;
int gfxBG2Y = 0;
int gfxBG3X = 0;
int gfxBG3Y = 0;
int gfxLastVCOUNT = 0;

#ifdef TILED_RENDERING
#ifdef _MSC_VER
union uint8_th
{
   __pragma( pack(push, 1));
   struct
   {
#ifdef MSB_FIRST
      /* 4*/	unsigned char hi:4;
      /* 0*/	unsigned char lo:4;
#else
      /* 0*/	unsigned char lo:4;
      /* 4*/	unsigned char hi:4;
#endif
   }
   __pragma(pack(pop));
   uint8_t val;
};
#else // !_MSC_VER
union uint8_th
{
   struct
   {
#ifdef MSB_FIRST
      /* 4*/	unsigned char hi:4;
      /* 0*/	unsigned char lo:4;
#else
      /* 0*/	unsigned char lo:4;
      /* 4*/	unsigned char hi:4;
#endif
   } __attribute__ ((packed));
   uint8_t val;
};
#endif

union TileEntry
{
   struct
   {
#ifdef MSB_FIRST
      /*14*/	unsigned palette:4;
      /*13*/	unsigned vFlip:1;
      /*12*/	unsigned hFlip:1;
      /* 0*/	unsigned tileNum:10;
#else
      /* 0*/	unsigned tileNum:10;
      /*12*/	unsigned hFlip:1;
      /*13*/	unsigned vFlip:1;
      /*14*/	unsigned palette:4;
#endif
   };
   uint16_t val;
};

struct TileLine {
    uint32_t pixels[8];
};

typedef const TileLine (*TileReader)(const uint16_t*, const int, const uint8_t*, uint16_t*, const uint32_t);

static inline void gfxDrawPixel(uint32_t* dest, const uint8_t color, const uint16_t* palette, const uint32_t prio)
{
    *dest = color ? (READ16LE(&palette[color]) | prio) : 0x80000000;
}

inline const TileLine gfxReadTile(const uint16_t* screenSource, const int yyy, const uint8_t* charBase, uint16_t* palette, const uint32_t prio)
{
    TileEntry tile;
    tile.val = READ16LE(screenSource);

    int tileY = yyy & 7;
    if (tile.vFlip)
        tileY = 7 - tileY;
    TileLine tileLine;

    const uint8_t* tileBase = &charBase[tile.tileNum * 64 + tileY * 8];

    if (!tile.hFlip) {
        gfxDrawPixel(&tileLine.pixels[0], tileBase[0], palette, prio);
        gfxDrawPixel(&tileLine.pixels[1], tileBase[1], palette, prio);
        gfxDrawPixel(&tileLine.pixels[2], tileBase[2], palette, prio);
        gfxDrawPixel(&tileLine.pixels[3], tileBase[3], palette, prio);
        gfxDrawPixel(&tileLine.pixels[4], tileBase[4], palette, prio);
        gfxDrawPixel(&tileLine.pixels[5], tileBase[5], palette, prio);
        gfxDrawPixel(&tileLine.pixels[6], tileBase[6], palette, prio);
        gfxDrawPixel(&tileLine.pixels[7], tileBase[7], palette, prio);
    } else {
        gfxDrawPixel(&tileLine.pixels[0], tileBase[7], palette, prio);
        gfxDrawPixel(&tileLine.pixels[1], tileBase[6], palette, prio);
        gfxDrawPixel(&tileLine.pixels[2], tileBase[5], palette, prio);
        gfxDrawPixel(&tileLine.pixels[3], tileBase[4], palette, prio);
        gfxDrawPixel(&tileLine.pixels[4], tileBase[3], palette, prio);
        gfxDrawPixel(&tileLine.pixels[5], tileBase[2], palette, prio);
        gfxDrawPixel(&tileLine.pixels[6], tileBase[1], palette, prio);
        gfxDrawPixel(&tileLine.pixels[7], tileBase[0], palette, prio);
    }

    return tileLine;
}

inline const TileLine gfxReadTilePal(const uint16_t* screenSource, const int yyy, const uint8_t* charBase, uint16_t* palette, const uint32_t prio)
{
    TileEntry tile;
    tile.val = READ16LE(screenSource);

    int tileY = yyy & 7;
    if (tile.vFlip)
        tileY = 7 - tileY;
    palette += tile.palette * 16;
    TileLine tileLine;

    const uint8_th* tileBase = (uint8_th*)&charBase[tile.tileNum * 32 + tileY * 4];

    if (!tile.hFlip) {
        gfxDrawPixel(&tileLine.pixels[0], tileBase[0].lo, palette, prio);
        gfxDrawPixel(&tileLine.pixels[1], tileBase[0].hi, palette, prio);
        gfxDrawPixel(&tileLine.pixels[2], tileBase[1].lo, palette, prio);
        gfxDrawPixel(&tileLine.pixels[3], tileBase[1].hi, palette, prio);
        gfxDrawPixel(&tileLine.pixels[4], tileBase[2].lo, palette, prio);
        gfxDrawPixel(&tileLine.pixels[5], tileBase[2].hi, palette, prio);
        gfxDrawPixel(&tileLine.pixels[6], tileBase[3].lo, palette, prio);
        gfxDrawPixel(&tileLine.pixels[7], tileBase[3].hi, palette, prio);
    } else {
        gfxDrawPixel(&tileLine.pixels[0], tileBase[3].hi, palette, prio);
        gfxDrawPixel(&tileLine.pixels[1], tileBase[3].lo, palette, prio);
        gfxDrawPixel(&tileLine.pixels[2], tileBase[2].hi, palette, prio);
        gfxDrawPixel(&tileLine.pixels[3], tileBase[2].lo, palette, prio);
        gfxDrawPixel(&tileLine.pixels[4], tileBase[1].hi, palette, prio);
        gfxDrawPixel(&tileLine.pixels[5], tileBase[1].lo, palette, prio);
        gfxDrawPixel(&tileLine.pixels[6], tileBase[0].hi, palette, prio);
        gfxDrawPixel(&tileLine.pixels[7], tileBase[0].lo, palette, prio);
    }

    return tileLine;
}

static inline void gfxDrawTile(const TileLine& tileLine, uint32_t* line)
{
    memcpy(line, tileLine.pixels, sizeof(tileLine.pixels));
}

static inline void gfxDrawTileClipped(const TileLine& tileLine, uint32_t* line, const int start, int w)
{
    memcpy(line, tileLine.pixels + start, w * sizeof(uint32_t));
}

template <TileReader readTile>
static void gfxDrawTextScreen(uint16_t control, uint16_t hofs, uint16_t vofs,
    uint32_t* line)
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

    uint16_t* screenSource = screenBase + 0x400 * (xxx >> 8) + ((xxx & 255) >> 3) + yshift;
    int x = 0;
    const int firstTileX = xxx & 7;

    // First tile, if clipped
    if (firstTileX) {
        gfxDrawTileClipped(readTile(screenSource, yyy, charBase, palette, prio), &line[x], firstTileX, 8 - firstTileX);
        screenSource++;
        x += 8 - firstTileX;
        xxx += 8 - firstTileX;

        if (xxx == 256 && sizeX > 256) {
            screenSource = screenBase + 0x400 + yshift;
        } else if (xxx >= sizeX) {
            xxx = 0;
            screenSource = screenBase + yshift;
        }
    }

    // Middle tiles, full
    while (x < 240 - firstTileX) {
        gfxDrawTile(readTile(screenSource, yyy, charBase, palette, prio), &line[x]);
        screenSource++;
        xxx += 8;
        x += 8;

        if (xxx == 256 && sizeX > 256) {
            screenSource = screenBase + 0x400 + yshift;
        } else if (xxx >= sizeX) {
            xxx = 0;
            screenSource = screenBase + yshift;
        }
    }

    // Last tile, if clipped
    if (firstTileX) {
        gfxDrawTileClipped(readTile(screenSource, yyy, charBase, palette, prio), &line[x], 0, firstTileX);
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

void gfxDrawTextScreen(uint16_t control, uint16_t hofs, uint16_t vofs, uint32_t* line)
{
    if (control & 0x80) // 1 pal / 256 col
        gfxDrawTextScreen<gfxReadTile>(control, hofs, vofs, line);
    else // 16 pal / 16 col
        gfxDrawTextScreen<gfxReadTilePal>(control, hofs, vofs, line);
}
#endif // TILED_RENDERING
