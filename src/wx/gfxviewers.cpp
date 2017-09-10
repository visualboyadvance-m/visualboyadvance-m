
// these are all the viewer dialogs with graphical panel areas
// they can be instantiated multiple times

#include "viewsupt.h"
#include "wxvbam.h"
#include <wx/colordlg.h>
#include <wx/ffile.h>

// FIXME: many of these read e.g. palette data directly without regard to
// byte order.  Need to determine where things are stored in emulated machine
// order and where in native order, and swap the latter on big-endian

// most of these have label fields that need to be sized and later filled
// mv is a string to initialize with for sizing
#define getlab(v, n, mv)                  \
    do {                                  \
        v = XRCCTRL(*this, n, wxControl); \
        if (!v)                           \
            baddialog();                  \
        v->SetLabel(wxT(mv));             \
    } while (0)

// FIXME: this should be in a header
extern uint8_t gbInvertTab[256];

// avoid exporting classes
namespace Viewers {
class MapViewer : public GfxViewer {
public:
    MapViewer()
        : GfxViewer(wxT("MapViewer"), 1024, 1024)
    {
        frame = bg = 0;
        getradio(fr0 =, "Frame0", frame, 0);
        getradio(fr1 =, "Frame1", frame, 0xa000);
        getradio(bg0 =, "BG0", bg, 0);
        getradio(bg1 =, "BG1", bg, 1);
        getradio(bg2 =, "BG2", bg, 2);
        getradio(bg3 =, "BG3", bg, 3);
        getlab(modelab, "Mode", "8");
        getlab(mapbase, "MapBase", "0xWWWWWWWW");
        getlab(charbase, "CharBase", "0xWWWWWWWW");
        getlab(size, "Size", "1024x1024");
        getlab(colors, "Colors", "2WW");
        getlab(prio, "Priority", "3");
        getlab(mosaic, "Mosaic", "0");
        getlab(overflow, "Overflow", "0");
        getlab(coords, "Coords", "(1023,1023)");
        getlab(addr, "Address", "0xWWWWWWWW");
        getlab(tile, "Tile", "1023");
        getlab(flip, "Flip", "HV");
        getlab(palette, "Palette", "---");
        Fit();
        selx = sely = -1;
        Update();
    }
    void Update()
    {
        mode = DISPCNT & 7;

        switch (bg) {
        case 0:
            control = BG0CNT;
            break;

        case 1:
            control = BG1CNT;
            break;

        case 2:
            control = BG2CNT;
            break;

        case 3:
            control = BG3CNT;
            break;
        }

        bool fr0en = true, fr1en = true, bg0en = true, bg1en = true,
             bg2en = true, bg3en = true;

        switch (mode) {
        case 0:
            fr0en = fr1en = false;
            renderTextScreen();
            break;

        case 1:
            fr0en = fr1en = false;
            bg3en = false;

            if (bg == 3) {
                bg = 0;
                control = BG0CNT;
                bg0->SetValue(true);
            }

            if (bg < 2)
                renderTextScreen();
            else
                renderRotScreen();

            break;

        case 2:
            fr0en = fr1en = false;
            bg0en = bg1en = false;

            if (bg < 2) {
                bg = 2;
                control = BG2CNT;
                bg2->SetValue(true);
            }

            renderRotScreen();
            break;

        case 3:
            fr0en = fr1en = false;
            bg0en = bg1en = bg2en = bg3en = false;
            bg = 2;
            bg2->SetValue(true);
            renderMode3();
            break;

        case 4:
            bg0en = bg1en = bg2en = bg3en = false;
            bg = 2;
            bg2->SetValue(true);
            renderMode4();
            break;

        case 5:
        case 6:
        case 7:
            bg = 2;
            bg2->SetValue(true);
            renderMode5();
            break;
        }

        ChangeBMP();
        fr0->Enable(fr0en);
        fr1->Enable(fr1en);
        bg0->Enable(bg0en);
        bg1->Enable(bg1en);
        bg2->Enable(bg2en);
        bg3->Enable(bg3en);
        wxString s;
        s.Printf(wxT("%d"), (int)mode);
        modelab->SetLabel(s);

        if (mode >= 3) {
            mapbase->SetLabel(wxEmptyString);
            charbase->SetLabel(wxEmptyString);
        } else {
            s.Printf(wxT("0x%08X"), ((control >> 8) & 0x1f) * 0x800 + 0x6000000);
            mapbase->SetLabel(s);
            s.Printf(wxT("0x%08X"), ((control >> 2) & 0x03) * 0x4000 + 0x6000000);
            charbase->SetLabel(s);
        }

        s.Printf(wxT("%dx%d"), gv->bmw, gv->bmh);
        size->SetLabel(s);
        colors->SetLabel(control & 0x80 ? wxT("256") : wxT("16"));
        s.Printf(wxT("%d"), control & 3);
        prio->SetLabel(s);
        mosaic->SetLabel(control & 0x40 ? wxT("1") : wxT("0"));
        overflow->SetLabel(bg <= 1 ? wxEmptyString : control & 0x2000 ? wxT("1") : wxT("0"));
        UpdateMouseInfo();
    }

    void UpdateMouseInfoEv(wxMouseEvent& ev)
    {
        selx = ev.GetX();
        sely = ev.GetY();
        UpdateMouseInfo(); // note that this will be inaccurate if game
        // not paused since last refresh
    }

    uint32_t AddressFromSel()
    {
        uint32_t base = ((control >> 8) & 0x1f) * 0x800 + 0x6000000;

        // all text bgs (16 bits)
        if (mode == 0 || (mode < 3 && bg < 2) || mode == 6 || mode == 7) {
            if (sely > 255) {
                base += 0x800;

                if (gv->bmw > 256)
                    base += 0x800;
            }

            if (selx >= 256)
                base += 0x800;

            return base + ((selx & 0xff) >> 3) * 2 + 64 * ((sely & 0xff) >> 3);
        }

        // rot bgs (8 bits)
        if (mode < 3)
            return base + (selx >> 3) + (gv->bmw >> 3) * (sely >> 3);

        // mode 3/5 (16 bits)
        if (mode != 4)
            return 0x6000000 + 0xa000 * frame + (selx + gv->bmw * sely) * 2;

        // mode 4 (8 bits)
        return 0x6000000 + 0xa000 * frame + selx + gv->bmw * sely;
    }

    void UpdateMouseInfo()
    {
        if (selx > gv->bmw || sely > gv->bmh)
            selx = sely = -1;

        if (selx < 0) {
            coords->SetLabel(wxEmptyString);
            addr->SetLabel(wxEmptyString);
            tile->SetLabel(wxEmptyString);
            flip->SetLabel(wxEmptyString);
            palette->SetLabel(wxEmptyString);
        } else {
            wxString s;
            s.Printf(wxT("(%d,%d)"), selx, sely);
            coords->SetLabel(s);
            uint32_t address = AddressFromSel();
            s.Printf(wxT("0x%08X"), address);
            addr->SetLabel(s);

            if (!mode || (mode < 3 || mode > 5) && bg < 2) {
                uint16_t value = *((uint16_t*)&vram[address - 0x6000000]);
                s.Printf(wxT("%d"), value & 1023);
                tile->SetLabel(s);
                s = value & 1024 ? wxT('H') : wxT('-');
                s += value & 2048 ? wxT('V') : wxT('-');
                flip->SetLabel(s);

                if (control & 0x80)
                    palette->SetLabel(wxT("---"));
                else {
                    s.Printf(wxT("%d"), (value >> 12) & 15);
                    palette->SetLabel(s);
                }
            } else {
                tile->SetLabel(wxT("---"));
                flip->SetLabel(wxT("--"));
                palette->SetLabel(wxT("---"));
            }
        }
    }

protected:
    uint16_t control, mode;
    int frame, bg;
    wxRadioButton *fr0, *fr1, *bg0, *bg1, *bg2, *bg3;
    wxControl *modelab, *mapbase, *charbase, *size, *colors, *prio, *mosaic,
        *overflow;
    wxControl *coords, *addr, *tile, *flip, *palette;
    int selx, sely;

    // following routines were copied from win32/MapView.cpp with little
    // attempt to read & validate, except:
    //    stride = 1024, rgb instead of bgr
    // FIXME: probably needs changing for big-endian

    void renderTextScreen()
    {
        uint16_t* palette = (uint16_t*)paletteRAM;
        uint8_t* charBase = &vram[((control >> 2) & 0x03) * 0x4000];
        uint16_t* screenBase = (uint16_t*)&vram[((control >> 8) & 0x1f) * 0x800];
        uint8_t* bmp = image.GetData();
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

        BMPSize(sizeX, sizeY);

        if (control & 0x80) {
            for (int y = 0; y < sizeY; y++) {
                int yy = y & 255;

                if (y == 256 && sizeY > 256) {
                    screenBase += 0x400;

                    if (sizeX > 256)
                        screenBase += 0x400;
                }

                uint16_t* screenSource = screenBase + ((yy >> 3) * 32);

                for (int x = 0; x < sizeX; x++) {
                    uint16_t data = *screenSource;
                    int tile = data & 0x3FF;
                    int tileX = (x & 7);
                    int tileY = y & 7;

                    if (data & 0x0400)
                        tileX = 7 - tileX;

                    if (data & 0x0800)
                        tileY = 7 - tileY;

                    uint8_t c = charBase[tile * 64 + tileY * 8 + tileX];
                    uint16_t color = palette[c];
                    *bmp++ = (color & 0x1f) << 3;
                    *bmp++ = ((color >> 5) & 0x1f) << 3;
                    *bmp++ = ((color >> 10) & 0x1f) << 3;

                    if (data & 0x0400) {
                        if (tileX == 0)
                            screenSource++;
                    } else if (tileX == 7)
                        screenSource++;

                    if (x == 255 && sizeX > 256) {
                        screenSource = screenBase + 0x400 + ((yy >> 3) * 32);
                    }
                }

                bmp += 3 * (1024 - sizeX);
            }
        } else {
            for (int y = 0; y < sizeY; y++) {
                int yy = y & 255;

                if (y == 256 && sizeY > 256) {
                    screenBase += 0x400;

                    if (sizeX > 256)
                        screenBase += 0x400;
                }

                uint16_t* screenSource = screenBase + ((yy >> 3) * 32);

                for (int x = 0; x < sizeX; x++) {
                    uint16_t data = *screenSource;
                    int tile = data & 0x3FF;
                    int tileX = (x & 7);
                    int tileY = y & 7;

                    if (data & 0x0400)
                        tileX = 7 - tileX;

                    if (data & 0x0800)
                        tileY = 7 - tileY;

                    uint8_t color = charBase[tile * 32 + tileY * 4 + (tileX >> 1)];

                    if (tileX & 1) {
                        color = (color >> 4);
                    } else {
                        color &= 0x0F;
                    }

                    int pal = (*screenSource >> 8) & 0xF0;
                    uint16_t color2 = palette[pal + color];
                    *bmp++ = (color2 & 0x1f) << 3;
                    *bmp++ = ((color2 >> 5) & 0x1f) << 3;
                    *bmp++ = ((color2 >> 10) & 0x1f) << 3;

                    if (data & 0x0400) {
                        if (tileX == 0)
                            screenSource++;
                    } else if (tileX == 7)
                        screenSource++;

                    if (x == 255 && sizeX > 256) {
                        screenSource = screenBase + 0x400 + ((yy >> 3) * 32);
                    }
                }

                bmp += 3 * (1024 - sizeX);
            }
        }

#if 0

		switch (bg)
		{
		case 0:
			renderView(BG0HOFS << 8, BG0VOFS << 8,
			           0x100, 0x000,
			           0x000, 0x100,
			           (sizeX - 1) << 8,
			           (sizeY - 1) << 8,
			           true);
			break;

		case 1:
			renderView(BG1HOFS << 8, BG1VOFS << 8,
			           0x100, 0x000,
			           0x000, 0x100,
			           (sizeX - 1) << 8,
			           (sizeY - 1) << 8,
			           true);
			break;

		case 2:
			renderView(BG2HOFS << 8, BG2VOFS << 8,
			           0x100, 0x000,
			           0x000, 0x100,
			           (sizeX - 1) << 8,
			           (sizeY - 1) << 8,
			           true);
			break;

		case 3:
			renderView(BG3HOFS << 8, BG3VOFS << 8,
			           0x100, 0x000,
			           0x000, 0x100,
			           (sizeX - 1) << 8,
			           (sizeY - 1) << 8,
			           true);
			break;
		}

#endif
    }

    void renderRotScreen()
    {
        uint16_t* palette = (uint16_t*)paletteRAM;
        uint8_t* charBase = &vram[((control >> 2) & 0x03) * 0x4000];
        uint8_t* screenBase = (uint8_t*)&vram[((control >> 8) & 0x1f) * 0x800];
        uint8_t* bmp = image.GetData();
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

        BMPSize(sizeX, sizeY);

        if (control & 0x80) {
            for (int y = 0; y < sizeY; y++) {
                for (int x = 0; x < sizeX; x++) {
                    int tile = screenBase[(x >> 3) + (y >> 3) * (sizeX >> 3)];
                    int tileX = (x & 7);
                    int tileY = y & 7;
                    uint8_t color = charBase[tile * 64 + tileY * 8 + tileX];
                    uint16_t color2 = palette[color];
                    *bmp++ = (color2 & 0x1f) << 3;
                    *bmp++ = ((color2 >> 5) & 0x1f) << 3;
                    *bmp++ = ((color2 >> 10) & 0x1f) << 3;
                }
            }

            bmp += 3 * (1024 - sizeX);
        } else {
            for (int y = 0; y < sizeY; y++) {
                for (int x = 0; x < sizeX; x++) {
                    int tile = screenBase[(x >> 3) + (y >> 3) * (sizeX >> 3)];
                    int tileX = (x & 7);
                    int tileY = y & 7;
                    uint8_t color = charBase[tile * 64 + tileY * 8 + tileX];
                    uint16_t color2 = palette[color];
                    *bmp++ = (color2 & 0x1f) << 3;
                    *bmp++ = ((color2 >> 5) & 0x1f) << 3;
                    *bmp++ = ((color2 >> 10) & 0x1f) << 3;
                }
            }

            bmp += 3 * (1024 - sizeX);
        }
    }

    void renderMode3()
    {
        uint8_t* bmp = image.GetData();
        uint16_t* src = (uint16_t*)&vram[0];
        BMPSize(240, 160);

        for (int y = 0; y < 160; y++) {
            for (int x = 0; x < 240; x++) {
                uint16_t data = *src++;
                *bmp++ = (data & 0x1f) << 3;
                *bmp++ = ((data >> 5) & 0x1f) << 3;
                *bmp++ = ((data >> 10) & 0x1f) << 3;
            }

            bmp += 3 * (1024 - 240);
        }
    }

    void renderMode4()
    {
        uint8_t* bmp = image.GetData();
        uint8_t* src = frame ? &vram[0xa000] : &vram[0];
        uint16_t* pal = (uint16_t*)&paletteRAM[0];
        BMPSize(240, 160);

        for (int y = 0; y < 160; y++) {
            for (int x = 0; x < 240; x++) {
                uint8_t c = *src++;
                uint16_t data = pal[c];
                *bmp++ = (data & 0x1f) << 3;
                *bmp++ = ((data >> 5) & 0x1f) << 3;
                *bmp++ = ((data >> 10) & 0x1f) << 3;
            }

            bmp += 3 * (1024 - 240);
        }
    }

    void renderMode5()
    {
        uint8_t* bmp = image.GetData();
        uint16_t* src = (uint16_t*)(frame ? &vram[0xa000] : &vram[0]);
        BMPSize(160, 128);

        for (int y = 0; y < 128; y++) {
            for (int x = 0; x < 160; x++) {
                uint16_t data = *src++;
                *bmp++ = (data & 0x1f) << 3;
                *bmp++ = ((data >> 5) & 0x1f) << 3;
                *bmp++ = ((data >> 10) & 0x1f) << 3;
            }

            bmp += 3 * (1024 - 160);
        }
    }

    DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(MapViewer, GfxViewer)
EVT_GFX_CLICK(wxID_ANY, MapViewer::UpdateMouseInfoEv)
END_EVENT_TABLE()

class GBMapViewer : public GfxViewer {
public:
    GBMapViewer()
        : GfxViewer(wxT("GBMapViewer"), 256, 256)
    {
        getradio(, "CharBase0", charbase, 0x0000);
        getradio(, "CharBase1", charbase, 0x0800);
        getradio(, "MapBase0", mapbase, 0x1800);
        getradio(, "MapBase1", mapbase, 0x1c00);
        getlab(coords, "Coords", "(2WW,2WW)");
        getlab(addr, "Address", "0xWWWW");
        getlab(tile, "Tile", "2WW");
        getlab(flip, "Flip", "HV");
        getlab(palette, "Palette", "---");
        getlab(prio, "Priority", "P");
        Fit();
        selx = sely = -1;
        Update();
    }
    void Update()
    {
        uint8_t *bank0, *bank1;

        if (gbCgbMode) {
            bank0 = &gbVram[0x0000];
            bank1 = &gbVram[0x2000];
        } else {
            bank0 = &gbMemory[0x8000];
            bank1 = NULL;
        }

        int tile_map_address = mapbase;
        // following copied almost verbatim from win32/GBMapView.cpp

        for (int y = 0; y < 32; y++) {
            for (int x = 0; x < 32; x++) {
                uint8_t* bmp = &image.GetData()[y * 8 * 32 * 24 + x * 24];
                uint8_t attrs = 0;

                if (bank1 != NULL)
                    attrs = bank1[tile_map_address];

                uint8_t tile = bank0[tile_map_address];
                tile_map_address++;

                if (charbase) {
                    if (tile < 128)
                        tile += 128;
                    else
                        tile -= 128;
                }

                for (int j = 0; j < 8; j++) {
                    int charbase_address = attrs & 0x40 ? charbase + tile * 16 + (7 - j) * 2 : charbase + tile * 16 + j * 2;
                    uint8_t tile_a = 0;
                    uint8_t tile_b = 0;

                    if (attrs & 0x08) {
                        tile_a = bank1[charbase_address++];
                        tile_b = bank1[charbase_address];
                    } else {
                        tile_a = bank0[charbase_address++];
                        tile_b = bank0[charbase_address];
                    }

                    if (attrs & 0x20) {
                        tile_a = gbInvertTab[tile_a];
                        tile_b = gbInvertTab[tile_b];
                    }

                    uint8_t mask = 0x80;

                    while (mask > 0) {
                        uint8_t c = (tile_a & mask) ? 1 : 0;
                        c += (tile_b & mask) ? 2 : 0;

                        if (gbCgbMode)
                            c = c + (attrs & 7) * 4;

                        uint16_t color = gbPalette[c];
                        *bmp++ = (color & 0x1f) << 3;
                        *bmp++ = ((color >> 5) & 0x1f) << 3;
                        *bmp++ = ((color >> 10) & 0x1f) << 3;
                        mask >>= 1;
                    }

                    bmp += 31 * 24;
                }
            }
        }

        ChangeBMP();
        UpdateMouseInfo();
    }

    void UpdateMouseInfoEv(wxMouseEvent& ev)
    {
        selx = ev.GetX();
        sely = ev.GetY();
        UpdateMouseInfo(); // note that this will be inaccurate if game
        // not paused since last refresh
    }

    void UpdateMouseInfo()
    {
        if (selx > gv->bmw || sely > gv->bmh)
            selx = sely = -1;

        if (selx < 0) {
            coords->SetLabel(wxEmptyString);
            addr->SetLabel(wxEmptyString);
            tile->SetLabel(wxEmptyString);
            flip->SetLabel(wxEmptyString);
            palette->SetLabel(wxEmptyString);
            prio->SetLabel(wxEmptyString);
        } else {
            wxString s;
            s.Printf(wxT("(%d,%d)"), selx, sely);
            coords->SetLabel(s);
            uint16_t address = mapbase + 0x8000 + (sely >> 3) * 32 + (selx >> 3);
            s.Printf(wxT("0x%04X"), address);
            addr->SetLabel(s);
            uint8_t attrs = 0;
            uint8_t tilev = gbMemoryMap[9][address & 0xfff];

            if (gbCgbMode) {
                attrs = gbVram[0x2000 + address - 0x8000];
                tilev = gbVram[address & 0x1fff];
            }

            if (charbase) {
                if (tilev >= 128)
                    tilev -= 128;
                else
                    tilev += 128;
            }

            s.Printf(wxT("%d"), (int)tilev);
            tile->SetLabel(s);
            s = attrs & 0x20 ? wxT('H') : wxT('-');
            s += attrs & 0x40 ? wxT('V') : wxT('-');
            flip->SetLabel(s);

            if (gbCgbMode) {
                s.Printf(wxT("%d"), attrs & 7);
                palette->SetLabel(s);
            } else
                palette->SetLabel(wxT("---"));

            prio->SetLabel(wxString(attrs & 0x80 ? wxT('P') : wxT('-')));
        }
    }

protected:
    int charbase, mapbase;
    wxControl *coords, *addr, *tile, *flip, *palette, *prio;
    int selx, sely;

    DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(GBMapViewer, GfxViewer)
EVT_GFX_CLICK(wxID_ANY, GBMapViewer::UpdateMouseInfoEv)
END_EVENT_TABLE()
}

void MainFrame::MapViewer()
{
    switch (panel->game_type()) {
    case IMAGE_GBA:
        LoadXRCViewer(Map);
        break;

    case IMAGE_GB:
        LoadXRCViewer(GBMap);
        break;
    }
}

namespace Viewers {
class OAMViewer : public GfxViewer {
public:
    OAMViewer()
        : GfxViewer(wxT("OAMViewer"), 544, 496)
    {
        sprite = 0;
        getspin(, "Sprite", sprite);
        getlab(pos, "Pos", "5WW,2WW");
        getlab(mode, "Mode", "3");
        getlab(colors, "Colors", "256");
        getlab(pallab, "Palette", "1W");
        getlab(tile, "Tile", "1WWW");
        getlab(prio, "Priority", "3");
        getlab(size, "Size", "64x64");
        getlab(rot, "Rotation", "3W");
        getlab(flg, "Flags", "RHVMD");
        Fit();
        Update();
    }
    void Update()
    {
        BMPSize(544, 496);
        wxImage screen(240, 160);
        systemRedShift = 19;
        systemGreenShift = 11;
        systemBlueShift = 3;
        utilReadScreenPixels(screen.GetData(), 240, 160);
        systemRedShift = 3;
        systemGreenShift = 11;
        systemBlueShift = 19;

        for (int sprite_no = 0; sprite_no < 128; sprite_no++) {
            uint16_t* sparms = &((uint16_t*)oam)[4 * sprite_no];
            uint16_t a0 = sparms[0], a1 = sparms[1], a2 = sparms[2];
            uint16_t* pal = &((uint16_t*)paletteRAM)[0x100];
            int sizeX = 8, sizeY = 8;

            // following is almost verbatim from OamView.cpp
            // shape = (a0 >> 14) & 3;
            // size = (a1 >> 14) & 3;
            switch (((a0 >> 12) & 0xc) | (a1 >> 14)) {
            case 0:
                break;

            case 1:
                sizeX = sizeY = 16;
                break;

            case 2:
                sizeX = sizeY = 32;
                break;

            case 3:
                sizeX = sizeY = 64;
                break;

            case 4:
                sizeX = 16;
                break;

            case 5:
                sizeX = 32;
                break;

            case 6:
                sizeX = 32;
                sizeY = 16;
                break;

            case 7:
                sizeX = 64;
                sizeY = 32;
                break;

            case 8:
                sizeY = 16;
                break;

            case 9:
                sizeY = 32;
                break;

            case 10:
                sizeX = 16;
                sizeY = 32;
                break;

            case 11:
                sizeX = 32;
                sizeY = 64;
                break;

            default:
                pos->SetLabel(wxEmptyString);
                mode->SetLabel(wxEmptyString);
                colors->SetLabel(wxEmptyString);
                pallab->SetLabel(wxEmptyString);
                tile->SetLabel(wxEmptyString);
                prio->SetLabel(wxEmptyString);
                size->SetLabel(wxEmptyString);
                rot->SetLabel(wxEmptyString);
                flg->SetLabel(wxEmptyString);
                continue;
            }

            wxImage spriteData(64, 64);
            uint8_t* bmp = spriteData.GetData();

            if (a0 & 0x2000) {
                int c = (a2 & 0x3FF);
                //if((DISPCNT & 7) > 2 && (c < 512))
                //    return;
                int inc = 32;

                if (DISPCNT & 0x40)
                    inc = sizeX >> 2;
                else
                    c &= 0x3FE;

                for (int y = 0; y < sizeY; y++) {
                    for (int x = 0; x < sizeX; x++) {
                        uint32_t color = vram[0x10000 + (((c + (y >> 3) * inc) * 32 + (y & 7) * 8 + (x >> 3) * 64 + (x & 7)) & 0x7FFF)];
                        color = pal[color];
                        *bmp++ = (color & 0x1f) << 3;
                        *bmp++ = ((color >> 5) & 0x1f) << 3;
                        *bmp++ = ((color >> 10) & 0x1f) << 3;
                    }

                    bmp += (64 - sizeX) * 3;
                }
            } else {
                int c = (a2 & 0x3FF);
                //if((DISPCNT & 7) > 2 && (c < 512))
                //    return;
                int inc = 32;

                if (DISPCNT & 0x40)
                    inc = sizeX >> 3;

                int palette = (a2 >> 8) & 0xF0;

                for (int y = 0; y < sizeY; y++) {
                    for (int x = 0; x < sizeX; x++) {
                        uint32_t color = vram[0x10000 + (((c + (y >> 3) * inc) * 32 + (y & 7) * 4 + (x >> 3) * 32 + ((x & 7) >> 1)) & 0x7FFF)];

                        if (x & 1)
                            color >>= 4;
                        else
                            color &= 0x0F;

                        color = pal[palette + color];
                        *bmp++ = (color & 0x1f) << 3;
                        *bmp++ = ((color >> 5) & 0x1f) << 3;
                        *bmp++ = ((color >> 10) & 0x1f) << 3;
                    }

                    bmp += (64 - sizeX) * 3;
                }
            }

            if (sprite == sprite_no) {
                wxString s;
                s.Printf(wxT("%d,%d"), a1 & 511, a0 & 255);
                pos->SetLabel(s);
                s.Printf(wxT("%d"), (a0 >> 10) & 3);
                mode->SetLabel(s);
                colors->SetLabel(a0 & 8192 ? wxT("256") : wxT("16"));
                s.Printf(wxT("%d"), (a2 >> 12) & 15);
                pallab->SetLabel(s);
                s.Printf(wxT("%d"), a2 & 1023);
                tile->SetLabel(s);
                s.Printf(wxT("%d"), (a2 >> 10) & 3);
                prio->SetLabel(s);
                s.Printf(wxT("%dx%d"), sizeX, sizeY);
                s.Printf(wxT("%dx%d"), 0, 0);
                size->SetLabel(s);

                if (a0 & 512) {
                    s.Printf(wxT("%d"), (a1 >> 9) & 31);
                    rot->SetLabel(s);
                } else
                    rot->SetLabel(wxEmptyString);

                s = wxEmptyString;

                if (a0 & 512)
                    s.append(wxT("R--"));
                else {
                    s.append(wxT('-'));
                    s.append(a1 & 4096 ? wxT('H') : wxT('-'));
                    s.append(a1 & 8192 ? wxT('V') : wxT('-'));
                }

                s.append(a0 & 4096 ? wxT('M') : wxT('-'));
                s.append(a0 & 1024 ? wxT('D') : wxT('-'));
                flg->SetLabel(s);
                uint8_t* box = spriteData.GetData();
                int sprite_posx = a1 & 511;
                int sprite_posy = a0 & 255;
                uint8_t* screen_box = screen.GetData();

                if (sprite_posx >= 0 && sprite_posx <= (239 - sizeY) && sprite_posy >= 0 && sprite_posy <= (159 - sizeX))
                    screen_box += (sprite_posx * 3) + (sprite_posy * screen.GetWidth() * 3);

                for (int y = 0; y < sizeY; y++) {
                    for (int x = 0; x < sizeX; x++) {
                        uint32_t color = 0;

                        if (y == 0 || y == sizeY - 1 || x == 0 || x == sizeX - 1) {
                            color = 255;
                            *box++ = (color & 0x1f) << 3;
                            *box++ = ((color >> 5) & 0x1f) << 3;
                            *box++ = ((color >> 10) & 0x1f) << 3;

                            if (sprite_posx >= 0 && sprite_posx <= (239 - sizeY) && sprite_posy >= 0 && sprite_posy <= (159 - sizeX)) {
                                *screen_box++ = (color & 0x1f) << 3;
                                *screen_box++ = ((color >> 5) & 0x1f) << 3;
                                *screen_box++ = ((color >> 10) & 0x1f) << 3;
                            }
                        } else {
                            box += 3;

                            if (sprite_posx >= 0 && sprite_posx <= (239 - sizeY) && sprite_posy >= 0 && sprite_posy <= (159 - sizeX))
                                screen_box += 3;
                        }
                    }

                    box += (spriteData.GetWidth() - sizeX) * 3;

                    if (sprite_posx >= 0 && sprite_posx <= (239 - sizeY) && sprite_posy >= 0 && sprite_posy <= (159 - sizeX))
                        screen_box += (screen.GetWidth() - sizeX) * 3;
                }
            }

            image.Paste(spriteData, (sprite_no % 16) * 34, (sprite_no / 16) * 34);
        }

        image.Paste(screen, 0, 304);
        ChangeBMP();
    }

protected:
    int sprite;
    wxControl *pos, *mode, *colors, *pallab, *tile, *prio, *size, *rot, *flg;
};

class GBOAMViewer : public GfxViewer {
public:
    GBOAMViewer()
        : GfxViewer(wxT("GBOAMViewer"), 8, 16)
    {
        sprite = 0;
        getspin(, "Sprite", sprite);
        getlab(pos, "Pos", "2WW,2WW");
        getlab(tilelab, "Tile", "2WW");
        getlab(prio, "Priority", "W");
        getlab(oap, "OAP", "W");
        getlab(pallab, "Palette", "W");
        getlab(flg, "Flags", "HV");
        getlab(banklab, "Bank", "W");
        Fit();
        Update();
    }
    void Update()
    {
        uint8_t* bmp = image.GetData();
        // following is almost verbatim from GBOamView.cpp
        uint16_t addr = sprite * 4 + 0xfe00;
        int size = register_LCDC & 4;
        uint8_t y = gbMemory[addr++];
        uint8_t x = gbMemory[addr++];
        uint8_t tile = gbMemory[addr++];

        if (size)
            tile &= 254;

        uint8_t flags = gbMemory[addr++];
        int w = 8;
        int h = size ? 16 : 8;
        BMPSize(w, h);
        uint8_t* bank0;
        uint8_t* bank1;

        if (gbCgbMode) {
            if (register_VBK & 1) {
                bank0 = &gbVram[0x0000];
                bank1 = &gbVram[0x2000];
            } else {
                bank0 = &gbVram[0x0000];
                bank1 = &gbVram[0x2000];
            }
        } else {
            bank0 = &gbMemory[0x8000];
            bank1 = NULL;
        }

        int init = 0x0000;
        uint8_t* pal = gbObp0;

        if ((flags & 0x10))
            pal = gbObp1;

        for (int yy = 0; yy < h; yy++) {
            int address = init + tile * 16 + 2 * yy;
            int a = 0;
            int b = 0;

            if (gbCgbMode && flags & 0x08) {
                a = bank1[address++];
                b = bank1[address++];
            } else {
                a = bank0[address++];
                b = bank0[address++];
            }

            for (int xx = 0; xx < 8; xx++) {
                uint8_t mask = 1 << (7 - xx);
                uint8_t c = 0;

                if ((a & mask))
                    c++;

                if ((b & mask))
                    c += 2;

                // make sure that sprites will work even in CGB mode
                if (gbCgbMode) {
                    c = c + (flags & 0x07) * 4 + 32;
                } else {
                    c = pal[c];
                }

                uint16_t color = gbPalette[c];
                *bmp++ = (color & 0x1f) << 3;
                *bmp++ = ((color >> 5) & 0x1f) << 3;
                *bmp++ = ((color >> 10) & 0x1f) << 3;
            }
        }

        ChangeBMP();
        wxString s;
        s.Printf(wxT("%d,%d"), x, y);
        pos->SetLabel(s);
        s.Printf(wxT("%d"), tile);
        tilelab->SetLabel(s);
        prio->SetLabel(flags & 0x80 ? wxT("1") : wxT("0"));
        oap->SetLabel(flags & 0x08 ? wxT("1") : wxT("0"));
        s.Printf(wxT("%d"), flags & 7);
        pallab->SetLabel(s);
        s = flags & 0x20 ? wxT('H') : wxT('-');
        s.append(flags & 0x40 ? wxT('V') : wxT('-'));
        flg->SetLabel(s);
        banklab->SetLabel(flags & 0x10 ? wxT("1") : wxT("0"));
    }

protected:
    int sprite;
    wxControl *pos, *tilelab, *prio, *oap, *pallab, *flg, *banklab;
};
}

void MainFrame::OAMViewer()
{
    switch (panel->game_type()) {
    case IMAGE_GBA:
        LoadXRCViewer(OAM);
        break;

    case IMAGE_GB:
        LoadXRCViewer(GBOAM);
        break;
    }
}

namespace Viewers {
static int ptype = 0;
static wxString pdir;
void savepal(wxWindow* parent, const uint8_t* data, int ncols, const wxString type)
{
    // no attempt is made here to translate the palette type name
    // it's just a suggested name, anyway
    wxString def_name = wxGetApp().frame->GetPanel()->game_name() + wxT('-') + type;

    if (ptype == 2)
        def_name += wxT(".act");
    else
        def_name += wxT(".pal");

    wxFileDialog dlg(parent, _("Select output file and type"), pdir, def_name,
        _("Windows Palette (*.pal)|*.pal|PaintShop Palette (*.pal)|*.pal|Adobe Color Table (*.act)|*.act"),
        wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    dlg.SetFilterIndex(ptype);
    int ret = dlg.ShowModal();
    ptype = dlg.GetFilterIndex();
    pdir = dlg.GetDirectory();

    if (ret != wxID_OK)
        return;

    wxFFile f(dlg.GetPath(), wxT("wb"));

    // FIXME: check for errors
    switch (ptype) {
    case 0: // Windows palette
    {
        f.Write("RIFF", 4);
        uint32_t d = wxUINT32_SWAP_ON_BE(256 * 4 + 16);
        f.Write(&d, 4);
        f.Write("PAL data", 8);
        d = wxUINT32_SWAP_ON_BE(256 * 4 + 4);
        f.Write(&d, 4);
        uint16_t w = wxUINT16_SWAP_ON_BE(0x0300);
        f.Write(&w, 2);
        w = wxUINT16_SWAP_ON_BE(256); // cuases problems if not 16 or 256
        f.Write(&w, 2);

        for (int i = 0; i < ncols; i++, data += 3) {
            f.Write(data, 3);
            uint8_t z = 0;
            f.Write(&z, 1);
        }

        for (int i = ncols; i < 256; i++) {
            d = 0;
            f.Write(&d, 4);
        }
    } break;

    case 1: // PaintShop palette
    {
#define jasc_head "JASC-PAL\r\n0100\r\n256\r\n"
        f.Write(jasc_head, sizeof(jasc_head) - 1);

        for (int i = 0; i < ncols; i++, data += 3) {
            char buf[14];
            int l = sprintf(buf, "%d %d %d\r\n", data[0], data[1], data[2]);
            f.Write(buf, l);
        }

        for (int i = ncols; i < 256; i++)
            f.Write("0 0 0\r\n", 7);

        break;
    }

    case 2: // Adobe color table
    {
        f.Write(data, ncols * 3);
        uint32_t d = 0;

        for (int i = ncols; i < 256; i++)
            f.Write(&d, 3);
    } break;
    }

    f.Close(); // FIXME: check for errors
}

class PaletteViewer : public Viewer {
public:
    PaletteViewer()
        : Viewer(wxT("PaletteViewer"))
    {
        colorctrl(cv, "Color");
        pixview(bpv, "Background", 16, 16, cv);
        pixview(spv, "Sprite", 16, 16, cv);
        getlab(addr, "Address", "0x5000WWW");
        getlab(val, "Value", "0xWWWW");
        Fit();
        Update();
    }
    void Update()
    {
        if (paletteRAM) {
            uint16_t* pp = (uint16_t*)paletteRAM;
            uint8_t* bmp = colbmp;

            for (int i = 0; i < 512; i++, pp++) {
                *bmp++ = (*pp & 0x1f) << 3;
                *bmp++ = (*pp & 0x3e0) >> 2;
                *bmp++ = (*pp & 0x7c00) >> 7;
            }
        } else
            memset(colbmp, 0, sizeof(colbmp));

        bpv->SetData(colbmp, 16, 0, 0);
        spv->SetData(colbmp + 16 * 16 * 3, 16, 0, 0);
        ShowSel();
    }
    void SelBG(wxMouseEvent& ev)
    {
        spv->SetSel(-1, -1, false);
        ShowSel();
    }
    void SelSprite(wxMouseEvent& ev)
    {
        bpv->SetSel(-1, -1, false);
        ShowSel();
    }
    void ShowSel()
    {
        int x, y;
        bool isbg = true;
        bpv->GetSel(x, y);

        if (x < 0) {
            isbg = false;
            spv->GetSel(x, y);

            if (x < 0) {
                addr->SetLabel(wxEmptyString);
                val->SetLabel(wxEmptyString);
                return;
            }
        }

        int off = x + y * 16;

        if (!isbg)
            off += 16 * 16;

        uint8_t* pix = &colbmp[off * 3];
        uint16_t v = (pix[0] >> 3) + ((pix[1] >> 3) << 5) + ((pix[2] >> 3) << 10);
        wxString s;
        s.Printf(wxT("0x%04X"), (int)v);
        val->SetLabel(s);
        s.Printf(wxT("0x%08X"), 0x5000000 + 2 * off);
        addr->SetLabel(s);
    }
    void SaveBG(wxCommandEvent& ev)
    {
        savepal(this, colbmp, 16 * 16, wxT("bg"));
    }
    void SaveOBJ(wxCommandEvent& ev)
    {
        savepal(this, colbmp + 16 * 16 * 3, 16 * 16, wxT("obj"));
    }
    void ChangeBackdrop(wxCommandEvent& ev)
    {
        // FIXME: this should really be a preference
        // should also have some way of indicating selection
        // perhaps replace w/ checkbox + colorpickerctrl
        static wxColourData* cd = NULL;
        wxColourDialog dlg(this, cd);

        if (dlg.ShowModal() == wxID_OK) {
            if (!cd)
                cd = new wxColourData();

            *cd = dlg.GetColourData();
            wxColour c = cd->GetColour();
            //Binary or the upper 5 bits of each color choice
            customBackdropColor = (c.Red() >> 3) || ((c.Green() >> 3) << 5) || ((c.Blue() >> 3) << 10);
        } else
            // kind of an unintuitive way to turn it off...
            customBackdropColor = -1;
    }

protected:
    ColorView* cv;
    PixView *bpv, *spv;
    uint8_t colbmp[16 * 16 * 3 * 2];
    wxControl *addr, *val;

    DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(PaletteViewer, Viewer)
EVT_BUTTON(XRCID("SaveBG"), PaletteViewer::SaveBG)
EVT_BUTTON(XRCID("SaveOBJ"), PaletteViewer::SaveOBJ)
EVT_BUTTON(XRCID("ChangeBackdrop"), PaletteViewer::ChangeBackdrop)
EVT_GFX_CLICK(XRCID("Background"), PaletteViewer::SelBG)
EVT_GFX_CLICK(XRCID("Sprite"), PaletteViewer::SelSprite)
END_EVENT_TABLE()

class GBPaletteViewer : public Viewer {
public:
    GBPaletteViewer()
        : Viewer(wxT("GBPaletteViewer"))
    {
        colorctrl(cv, "Color");
        pixview(bpv, "Background", 4, 8, cv);
        pixview(spv, "Sprite", 4, 8, cv);
        getlab(idx, "Index", "3W");
        getlab(val, "Value", "0xWWWW");
        Fit();
        Update();
    }
    void Update()
    {
        uint16_t* pp = gbPalette;
        uint8_t* bmp = colbmp;

        for (int i = 0; i < 64; i++, pp++) {
            *bmp++ = (*pp & 0x1f) << 3;
            *bmp++ = (*pp & 0x3e0) >> 2;
            *bmp++ = (*pp & 0x7c00) >> 7;
        }

        bpv->SetData(colbmp, 4, 0, 0);
        spv->SetData(colbmp + 4 * 8 * 3, 4, 0, 0);
        ShowSel();
    }
    void SelBG(wxMouseEvent& ev)
    {
        spv->SetSel(-1, -1, false);
        ShowSel();
    }
    void SelSprite(wxMouseEvent& ev)
    {
        bpv->SetSel(-1, -1, false);
        ShowSel();
    }
    void ShowSel()
    {
        int x, y;
        bool isbg = true;
        bpv->GetSel(x, y);

        if (x < 0) {
            isbg = false;
            spv->GetSel(x, y);

            if (x < 0) {
                idx->SetLabel(wxEmptyString);
                val->SetLabel(wxEmptyString);
                return;
            }
        }

        uint8_t* pix = &colbmp[(x + y * 4) * 3];

        if (isbg)
            pix += 4 * 8 * 3;

        uint16_t v = (pix[0] >> 3) + ((pix[1] >> 3) << 5) + ((pix[2] >> 3) << 10);
        wxString s;
        s.Printf(wxT("0x%04X"), (int)v);
        val->SetLabel(s);
        s.Printf(wxT("%d"), x + y * 4);
        idx->SetLabel(s);
    }
    void SaveBG(wxCommandEvent& ev)
    {
        savepal(this, colbmp, 4 * 8, wxT("bg"));
    }
    void SaveOBJ(wxCommandEvent& ev)
    {
        savepal(this, colbmp + 4 * 8 * 3, 4 * 8, wxT("obj"));
    }

protected:
    ColorView* cv;
    PixView *bpv, *spv;
    uint8_t colbmp[4 * 8 * 3 * 2];
    wxControl *idx, *val;
    DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(GBPaletteViewer, Viewer)
EVT_BUTTON(XRCID("SaveBG"), GBPaletteViewer::SaveBG)
EVT_BUTTON(XRCID("SaveOBJ"), GBPaletteViewer::SaveOBJ)
EVT_GFX_CLICK(XRCID("Background"), GBPaletteViewer::SelBG)
EVT_GFX_CLICK(XRCID("Sprite"), GBPaletteViewer::SelSprite)
END_EVENT_TABLE()
}

void MainFrame::PaletteViewer()
{
    switch (panel->game_type()) {
    case IMAGE_GBA:
        LoadXRCViewer(Palette);
        break;

    case IMAGE_GB:
        LoadXRCViewer(GBPalette);
        break;
    }
}

namespace Viewers {
class TileViewer : public GfxViewer {
public:
    TileViewer()
        : GfxViewer(wxT("TileViewer"), 32 * 8, 32 * 8)
    {
        is256 = charbase = 0;
        getradio(, "Color16", is256, 0);
        getradio(, "Color256", is256, 1);
        getradio(, "CharBase0", charbase, 0);
        getradio(, "CharBase1", charbase, 0x4000);
        getradio(, "CharBase2", charbase, 0x8000);
        getradio(, "CharBase3", charbase, 0xc000);
        getradio(, "CharBase4", charbase, 0x10000);
        getslider(, "Palette", palette);
        getlab(tileno, "Tile", "1WWW");
        getlab(addr, "Address", "06WWWWWW");
        selx = sely = -1;
        Fit();
        Update();
    }
    void Update()
    {
        // Following copied almost verbatim from TileView.cpp
        uint16_t* palette = (uint16_t*)paletteRAM;
        uint8_t* charBase = &vram[charbase];
        int maxY;

        if (is256) {
            int tile = 0;
            maxY = 16;

            for (int y = 0; y < maxY; y++) {
                for (int x = 0; x < 32; x++) {
                    if (charbase == 4 * 0x4000)
                        render256(tile, x, y, charBase, &palette[256]);
                    else
                        render256(tile, x, y, charBase, palette);

                    tile++;
                }
            }

            BMPSize(32 * 8, maxY * 8);
        } else {
            int tile = 0;
            maxY = 32;

            if (charbase == 3 * 0x4000)
                maxY = 16;

            for (int y = 0; y < maxY; y++) {
                for (int x = 0; x < 32; x++) {
                    render16(tile, x, y, charBase, palette);
                    tile++;
                }
            }

            BMPSize(32 * 8, maxY * 8);
        }

        ChangeBMP();
        UpdateMouseInfo();
    }
    void UpdateMouseInfoEv(wxMouseEvent& ev)
    {
        selx = ev.GetX();
        sely = ev.GetY();
        UpdateMouseInfo();
    }

    void UpdateMouseInfo()
    {
        if (selx > gv->bmw || sely > gv->bmh)
            selx = sely = -1;

        if (selx < 0) {
            addr->SetLabel(wxEmptyString);
            tileno->SetLabel(wxEmptyString);
        } else {
            int x = selx / 8;
            int y = sely / 8;
            int t = 32 * y + x;

            if (is256)
                t *= 2;

            wxString s;
            s.Printf(wxT("%d"), t);
            tileno->SetLabel(s);
            s.Printf(wxT("%08X"), 0x6000000 + charbase + 32 * t);
            addr->SetLabel(s);
        }
    }
    // following 2 functions copied almost verbatim from TileView.cpp
    void render256(int tile, int x, int y, uint8_t* charBase, uint16_t* palette)
    {
        uint8_t* bmp = &image.GetData()[24 * x + 8 * 32 * 24 * y];

        for (int j = 0; j < 8; j++) {
            for (int i = 0; i < 8; i++) {
                uint8_t c = charBase[tile * 64 + j * 8 + i];
                uint16_t color = palette[c];
                *bmp++ = (color & 0x1f) << 3;
                *bmp++ = ((color >> 5) & 0x1f) << 3;
                *bmp++ = ((color >> 10) & 0x1f) << 3;
            }

            bmp += 31 * 24; // advance line
        }
    }

    void render16(int tile, int x, int y, uint8_t* charBase, uint16_t* palette)
    {
        uint8_t* bmp = &image.GetData()[24 * x + 8 * 32 * 24 * y];
        int pal = this->palette;

        if (this->charbase == 4 * 0x4000)
            pal += 16;

        for (int j = 0; j < 8; j++) {
            for (int i = 0; i < 8; i++) {
                uint8_t c = charBase[tile * 32 + j * 4 + (i >> 1)];

                if (i & 1)
                    c = c >> 4;
                else
                    c = c & 15;

                uint16_t color = palette[pal * 16 + c];
                *bmp++ = (color & 0x1f) << 3;
                *bmp++ = ((color >> 5) & 0x1f) << 3;
                *bmp++ = ((color >> 10) & 0x1f) << 3;
            }

            bmp += 31 * 24; // advance line
        }
    }

protected:
    int charbase, is256, palette;
    wxControl *tileno, *addr;
    int selx, sely;

    DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(TileViewer, GfxViewer)
EVT_GFX_CLICK(wxID_ANY, TileViewer::UpdateMouseInfoEv)
END_EVENT_TABLE()

class GBTileViewer : public GfxViewer {
public:
    GBTileViewer()
        : GfxViewer(wxT("GBTileViewer"), 16 * 8, 16 * 8)
    {
        bank = charbase = 0;
        getradio(, "Bank0", bank, 0);
        getradio(, "Bank1", bank, 0x2000);
        getradio(, "CharBase0", charbase, 0);
        getradio(, "CharBase1", charbase, 0x800);
        getslider(, "Palette", palette);
        getlab(tileno, "Tile", "2WW");
        getlab(addr, "Address", "WWWW");
        selx = sely = -1;
        Fit();
        Update();
    }
    void Update()
    {
        // following copied almost verbatim from GBTileView.cpp
        uint8_t* charBase = (gbVram != NULL) ? &gbVram[bank + charbase] : &gbMemory[0x8000 + charbase];
        int tile = 0;

        for (int y = 0; y < 16; y++) {
            for (int x = 0; x < 16; x++) {
                render(tile, x, y, charBase);
                tile++;
            }
        }

        ChangeBMP();
        UpdateMouseInfo();
    }
    void UpdateMouseInfoEv(wxMouseEvent& ev)
    {
        selx = ev.GetX();
        sely = ev.GetY();
        UpdateMouseInfo();
    }

    void UpdateMouseInfo()
    {
        if (selx > gv->bmw || sely > gv->bmh)
            selx = sely = -1;

        if (selx < 0) {
            addr->SetLabel(wxEmptyString);
            tileno->SetLabel(wxEmptyString);
        } else {
            int x = selx / 8;
            int y = sely / 8;
            int t = 16 * y + x;
            wxString s;
            s.Printf(wxT("%d"), t);
            tileno->SetLabel(s);
            s.Printf(wxT("%04X"), 0x8000 + charbase + 16 * t);
            addr->SetLabel(s);
        }
    }

    // following function copied almost verbatim from GBTileView.cpp
    void render(int tile, int x, int y, uint8_t* charBase)
    {
        uint8_t* bmp = &image.GetData()[24 * x + 8 * 16 * 24 * y];

        for (int j = 0; j < 8; j++) {
            uint8_t mask = 0x80;
            uint8_t tile_a = charBase[tile * 16 + j * 2];
            uint8_t tile_b = charBase[tile * 16 + j * 2 + 1];

            for (int i = 0; i < 8; i++) {
                uint8_t c = (tile_a & mask) ? 1 : 0;
                c += ((tile_b & mask) ? 2 : 0);

                if (gbCgbMode) {
                    c = c + palette * 4;
                } else {
                    c = gbBgp[c];
                }

                uint16_t color = gbPalette[c];
                *bmp++ = (color & 0x1f) << 3;
                *bmp++ = ((color >> 5) & 0x1f) << 3;
                *bmp++ = ((color >> 10) & 0x1f) << 3;
                mask >>= 1;
            }

            bmp += 15 * 24; // advance line
        }
    }

protected:
    int bank, charbase, palette;
    wxControl *addr, *tileno;
    int selx, sely;

    DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(GBTileViewer, GfxViewer)
EVT_GFX_CLICK(wxID_ANY, GBTileViewer::UpdateMouseInfoEv)
END_EVENT_TABLE()
}

void MainFrame::TileViewer()
{
    switch (panel->game_type()) {
    case IMAGE_GBA:
        LoadXRCViewer(Tile);
        break;

    case IMAGE_GB:
        LoadXRCViewer(GBTile);
        break;
    }
}
