#include "MapView.h"
#include "FileDlg.h"
#include "Reg.h"
#include "WinResUtil.h"
#include "stdafx.h"
#include "vba.h"

#include "../NLS.h"
#include "../System.h"
#include "../Util.h"
#include "../gba/GBA.h"
#include "../gba/Globals.h"

extern "C" {
#include <png.h>
}

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// MapView dialog

MapView::MapView(CWnd* pParent /*=NULL*/)
    : ResizeDlg(MapView::IDD, pParent)
{
    //{{AFX_DATA_INIT(MapView)
    //}}AFX_DATA_INIT
    autoUpdate = false;

    memset(&bmpInfo.bmiHeader, 0, sizeof(bmpInfo.bmiHeader));

    bmpInfo.bmiHeader.biSize = sizeof(bmpInfo.bmiHeader);
    bmpInfo.bmiHeader.biWidth = 1024;
    bmpInfo.bmiHeader.biHeight = -1024;
    bmpInfo.bmiHeader.biPlanes = 1;
    bmpInfo.bmiHeader.biBitCount = 24;
    bmpInfo.bmiHeader.biCompression = BI_RGB;
    data = (uint8_t*)calloc(1, 3 * 1024 * 1024);

    mapView.setData(data);
    mapView.setBmpInfo(&bmpInfo);

    control = BG0CNT;

    bg = 0;
    frame = 0;
}

MapView::~MapView()
{
    free(data);
    data = NULL;
}

void MapView::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    //{{AFX_DATA_MAP(MapView)
    DDX_Control(pDX, IDC_NUMCOLORS, m_numcolors);
    DDX_Control(pDX, IDC_MODE, m_mode);
    DDX_Control(pDX, IDC_OVERFLOW, m_overflow);
    DDX_Control(pDX, IDC_MOSAIC, m_mosaic);
    DDX_Control(pDX, IDC_PRIORITY, m_priority);
    DDX_Control(pDX, IDC_DIM, m_dim);
    DDX_Control(pDX, IDC_CHARBASE, m_charbase);
    DDX_Control(pDX, IDC_MAPBASE, m_mapbase);
    //}}AFX_DATA_MAP
    DDX_Control(pDX, IDC_MAP_VIEW, mapView);
    DDX_Control(pDX, IDC_MAP_VIEW_ZOOM, mapViewZoom);
    DDX_Control(pDX, IDC_COLOR, color);
}

BEGIN_MESSAGE_MAP(MapView, CDialog)
//{{AFX_MSG_MAP(MapView)
ON_BN_CLICKED(IDC_REFRESH, OnRefresh)
ON_BN_CLICKED(IDC_FRAME_0, OnFrame0)
ON_BN_CLICKED(IDC_FRAME_1, OnFrame1)
ON_BN_CLICKED(IDC_BG0, OnBg0)
ON_BN_CLICKED(IDC_BG1, OnBg1)
ON_BN_CLICKED(IDC_BG2, OnBg2)
ON_BN_CLICKED(IDC_BG3, OnBg3)
ON_BN_CLICKED(IDC_STRETCH, OnStretch)
ON_BN_CLICKED(IDC_AUTO_UPDATE, OnAutoUpdate)
ON_BN_CLICKED(IDC_CLOSE, OnClose)
ON_BN_CLICKED(IDC_SAVE, OnSave)
//}}AFX_MSG_MAP
ON_MESSAGE(WM_MAPINFO, OnMapInfo)
ON_MESSAGE(WM_COLINFO, OnColInfo)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// MapView message handlers

void MapView::renderTextScreen(uint16_t control)
{
    uint16_t* palette = (uint16_t*)paletteRAM;
    uint8_t* charBase = &vram[((control >> 2) & 0x03) * 0x4000];
    uint16_t* screenBase = (uint16_t*)&vram[((control >> 8) & 0x1f) * 0x800];
    uint8_t* bmp = data;

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

    w = sizeX;
    h = sizeY;

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

                *bmp++ = ((color >> 10) & 0x1f) << 3;
                *bmp++ = ((color >> 5) & 0x1f) << 3;
                *bmp++ = (color & 0x1f) << 3;

                if (data & 0x0400) {
                    if (tileX == 0)
                        screenSource++;
                } else if (tileX == 7)
                    screenSource++;
                if (x == 255 && sizeX > 256) {
                    screenSource = screenBase + 0x400 + ((yy >> 3) * 32);
                }
            }
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

                *bmp++ = ((color2 >> 10) & 0x1f) << 3;
                *bmp++ = ((color2 >> 5) & 0x1f) << 3;
                *bmp++ = (color2 & 0x1f) << 3;

                if (data & 0x0400) {
                    if (tileX == 0)
                        screenSource++;
                } else if (tileX == 7)
                    screenSource++;

                if (x == 255 && sizeX > 256) {
                    screenSource = screenBase + 0x400 + ((yy >> 3) * 32);
                }
            }
        }
    }
    /*
    switch(bg) {
    case 0:
    renderView(BG0HOFS<<8, BG0VOFS<<8,
    0x100, 0x000,
    0x000, 0x100,
    (sizeX -1) <<8,
    (sizeY -1) << 8,
    true);
    break;
    case 1:
    renderView(BG1HOFS<<8, BG1VOFS<<8,
    0x100, 0x000,
    0x000, 0x100,
    (sizeX -1) <<8,
    (sizeY -1) << 8,
    true);
    break;
    case 2:
    renderView(BG2HOFS<<8, BG2VOFS<<8,
    0x100, 0x000,
    0x000, 0x100,
    (sizeX -1) <<8,
    (sizeY -1) << 8,
    true);
    break;
    case 3:
    renderView(BG3HOFS<<8, BG3VOFS<<8,
    0x100, 0x000,
    0x000, 0x100,
    (sizeX -1) <<8,
    (sizeY -1) << 8,
    true);
    break;
    }
  */
}

void MapView::renderRotScreen(uint16_t control)
{
    uint16_t* palette = (uint16_t*)paletteRAM;
    uint8_t* charBase = &vram[((control >> 2) & 0x03) * 0x4000];
    uint8_t* screenBase = (uint8_t*)&vram[((control >> 8) & 0x1f) * 0x800];
    uint8_t* bmp = data;

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

    w = sizeX;
    h = sizeY;

    for (int y = 0; y < sizeY; y++) {
        for (int x = 0; x < sizeX; x++) {
            int tile = screenBase[(x >> 3) + (y >> 3) * (w >> 3)];

            int tileX = (x & 7);
            int tileY = y & 7;

            uint8_t color = charBase[tile * 64 + tileY * 8 + tileX];
            uint16_t color2 = palette[color];

            *bmp++ = ((color2 >> 10) & 0x1f) << 3;
            *bmp++ = ((color2 >> 5) & 0x1f) << 3;
            *bmp++ = (color2 & 0x1f) << 3;
        }
    }

    uint32_t xx;
    uint32_t yy;

    switch (bg) {
    case 2:
        xx = BG2X_L | BG2X_H << 16;
        yy = BG2Y_L | BG2Y_H << 16;

        /*
          renderView(xx, yy,
          BG2PA, BG2PC,
          BG2PB, BG2PD,
          (sizeX -1) <<8,
          (sizeY -1) << 8,
          (control & 0x2000) != 0);
    */
        break;
    case 3:
        xx = BG3X_L | BG3X_H << 16;
        yy = BG3Y_L | BG3Y_H << 16;
        /*
          renderView(xx, yy,
          BG3PA, BG3PC,
          BG3PB, BG3PD,
          (sizeX -1) <<8,
          (sizeY -1) << 8,
          (control & 0x2000) != 0);
    */
        break;
    }
}

void MapView::renderMode0()
{
    renderTextScreen(control);
}

void MapView::renderMode1()
{
    switch (bg) {
    case 0:
    case 1:
        renderTextScreen(control);
        break;
    case 2:
        renderRotScreen(control);
        break;
    default:
        bg = 0;
        control = BG0CNT;
        renderTextScreen(control);
        break;
    }
}

void MapView::renderMode2()
{
    switch (bg) {
    case 2:
    case 3:
        renderRotScreen(control);
        break;
    default:
        bg = 2;
        control = BG2CNT;
        renderRotScreen(control);
        break;
    }
}

void MapView::renderMode3()
{
    uint8_t* bmp = data;
    uint16_t* src = (uint16_t*)&vram[0];

    w = 240;
    h = 160;

    for (int y = 0; y < 160; y++) {
        for (int x = 0; x < 240; x++) {
            uint16_t data = *src++;
            *bmp++ = ((data >> 10) & 0x1f) << 3;
            *bmp++ = ((data >> 5) & 0x1f) << 3;
            *bmp++ = (data & 0x1f) << 3;
        }
    }
    bg = 2;
}

void MapView::renderMode4()
{
    uint8_t* bmp = data;
    uint8_t* src = frame ? &vram[0xa000] : &vram[0];
    uint16_t* pal = (uint16_t*)&paletteRAM[0];

    w = 240;
    h = 160;

    for (int y = 0; y < 160; y++) {
        for (int x = 0; x < 240; x++) {
            uint8_t c = *src++;
            uint16_t data = pal[c];
            *bmp++ = ((data >> 10) & 0x1f) << 3;
            *bmp++ = ((data >> 5) & 0x1f) << 3;
            *bmp++ = (data & 0x1f) << 3;
        }
    }
    bg = 2;
}

void MapView::renderMode5()
{
    uint8_t* bmp = data;
    uint16_t* src = (uint16_t*)(frame ? &vram[0xa000] : &vram[0]);

    w = 160;
    h = 128;

    for (int y = 0; y < 128; y++) {
        for (int x = 0; x < 160; x++) {
            uint16_t data = *src++;
            *bmp++ = ((data >> 10) & 0x1f) << 3;
            *bmp++ = ((data >> 5) & 0x1f) << 3;
            *bmp++ = (data & 0x1f) << 3;
        }
    }
    bg = 2;
}

void MapView::OnRefresh()
{
    paint();
}

void MapView::paint()
{
    if (vram == NULL)
        return;
    int mode = DISPCNT & 7;

    switch (bg) {
    default:
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

    switch (mode) {
    case 0:
        renderMode0();
        break;
    case 1:
        renderMode1();
        break;
    case 2:
        renderMode2();
        break;
    case 3:
        renderMode3();
        break;
    case 4:
        renderMode4();
        break;
    case 5:
        renderMode5();
        break;
    case 6:
        renderMode5();
        break;
    case 7:
        renderMode5();
        break;
    }
    enableButtons(mode);
    SIZE s;

    if (mapView.getStretch()) {
        mapView.setSize(w, h);
        s.cx = s.cy = 1;
        mapView.SetScrollSizes(MM_TEXT, s);
    } else {
        mapView.setSize(w, h);
        s.cx = w;
        s.cy = h;
        mapView.SetScrollSizes(MM_TEXT, s);
    }

    mapView.refresh();

    CString buffer;

    uint32_t charBase = ((control >> 2) & 0x03) * 0x4000 + 0x6000000;
    uint32_t screenBase = ((control >> 8) & 0x1f) * 0x800 + 0x6000000;

    buffer.Format("%d", mode);
    m_mode.SetWindowText(buffer);

    if (mode >= 3) {
        m_mapbase.SetWindowText("");
        m_charbase.SetWindowText("");
    } else {
        buffer.Format("0x%08X", screenBase);
        m_mapbase.SetWindowText(buffer);

        buffer.Format("0x%08X", charBase);
        m_charbase.SetWindowText(buffer);
    }

    buffer.Format("%dx%d", w, h);
    m_dim.SetWindowText(buffer);

    m_numcolors.SetWindowText(control & 0x80 ? "256" : "16");

    buffer.Format("%d", control & 3);
    m_priority.SetWindowText(buffer);

    m_mosaic.SetWindowText(control & 0x40 ? "1" : "0");

    m_overflow.SetWindowText(bg <= 1 ? "" : control & 0x2000 ? "1" : "0");
}

BOOL MapView::OnInitDialog()
{
    CDialog::OnInitDialog();

    DIALOG_SIZER_START(sz)
    DIALOG_SIZER_ENTRY(IDC_MAP_VIEW, DS_SizeX | DS_SizeY)
    DIALOG_SIZER_ENTRY(IDC_REFRESH, DS_MoveY)
    DIALOG_SIZER_ENTRY(IDC_CLOSE, DS_MoveY)
    DIALOG_SIZER_ENTRY(IDC_SAVE, DS_MoveY)
    DIALOG_SIZER_ENTRY(IDC_COLOR, DS_MoveY)
    DIALOG_SIZER_ENTRY(IDC_R, DS_MoveY)
    DIALOG_SIZER_ENTRY(IDC_G, DS_MoveY)
    DIALOG_SIZER_ENTRY(IDC_B, DS_MoveY)
    DIALOG_SIZER_END()
    SetData(sz,
        TRUE,
        HKEY_CURRENT_USER,
        "Software\\Emulators\\VisualBoyAdvance\\Viewer\\MapView",
        NULL);
    SIZE size;
    size.cx = 1;
    size.cy = 1;
    mapView.SetScrollSizes(MM_TEXT, size);
    int s = regQueryDwordValue("mapViewStretch", 0);
    if (s)
        mapView.setStretch(true);
    ((CButton*)GetDlgItem(IDC_STRETCH))->SetCheck(s);
    paint();

    return TRUE; // return TRUE unless you set the focus to a control
    // EXCEPTION: OCX Property Pages should return FALSE
}

void MapView::PostNcDestroy()
{
    delete this;
}

void MapView::enableButtons(int mode)
{
    bool enable[6] = { true, true, true, true, true, true };

    switch (mode) {
    case 0:
        enable[4] = false;
        enable[5] = false;
        break;
    case 1:
        enable[3] = false;
        enable[4] = false;
        enable[5] = false;
        break;
    case 2:
        enable[0] = false;
        enable[1] = false;
        enable[4] = false;
        enable[5] = false;
        break;
    case 3:
        enable[0] = false;
        enable[1] = false;
        enable[2] = false;
        enable[3] = false;
        enable[4] = false;
        enable[5] = false;
        break;
    case 4:
        enable[0] = false;
        enable[1] = false;
        enable[2] = false;
        enable[3] = false;
        break;
    case 5:
        enable[0] = false;
        enable[1] = false;
        enable[2] = false;
        enable[3] = false;
        break;
    }
    GetDlgItem(IDC_BG0)->EnableWindow(enable[0]);
    GetDlgItem(IDC_BG1)->EnableWindow(enable[1]);
    GetDlgItem(IDC_BG2)->EnableWindow(enable[2]);
    GetDlgItem(IDC_BG3)->EnableWindow(enable[3]);
    GetDlgItem(IDC_FRAME_0)->EnableWindow(enable[4]);
    GetDlgItem(IDC_FRAME_1)->EnableWindow(enable[5]);
    int id = IDC_BG0;
    switch (bg) {
    case 1:
        id = IDC_BG1;
        break;
    case 2:
        id = IDC_BG2;
        break;
    case 3:
        id = IDC_BG3;
        break;
    }
    CheckRadioButton(IDC_BG0, IDC_BG3, id);
    id = IDC_FRAME_0;
    if (frame != 0)
        id = IDC_FRAME_1;
    CheckRadioButton(IDC_FRAME_0, IDC_FRAME_1, id);
}

void MapView::OnFrame0()
{
    frame = 0;
    paint();
}

void MapView::OnFrame1()
{
    frame = 1;
    paint();
}

void MapView::OnBg0()
{
    bg = 0;
    control = BG0CNT;
    paint();
}

void MapView::OnBg1()
{
    bg = 1;
    control = BG1CNT;
    paint();
}

void MapView::OnBg2()
{
    bg = 2;
    control = BG2CNT;
    paint();
}

void MapView::OnBg3()
{
    bg = 3;
    control = BG3CNT;
    paint();
}

void MapView::OnStretch()
{
    mapView.setStretch(!mapView.getStretch());
    paint();
    regSetDwordValue("mapViewStretch", mapView.getStretch());
}

void MapView::OnAutoUpdate()
{
    autoUpdate = !autoUpdate;
    if (autoUpdate) {
        theApp.winAddUpdateListener(this);
    } else {
        theApp.winRemoveUpdateListener(this);
    }
    (CButton*)GetDlgItem(IDC_REFRESH)->EnableWindow(autoUpdate ? FALSE : TRUE);
}

void MapView::update()
{
    paint();
}

void MapView::OnClose()
{
    theApp.winRemoveUpdateListener(this);

    DestroyWindow();
}

uint32_t MapView::GetTextClickAddress(uint32_t base, int x, int y)
{
    if (y > 255 && h > 256) {
        base += 0x800;
        if (w > 256)
            base += 0x800;
    }
    if (x >= 256)
        base += 0x800;
    x &= 255;
    y &= 255;
    base += (x >> 3) * 2 + 64 * (y >> 3);

    return base;
}

uint32_t MapView::GetClickAddress(int x, int y)
{
    int mode = DISPCNT & 7;

    uint32_t base = ((control >> 8) & 0x1f) * 0x800 + 0x6000000;

    // all text bgs (16 bits)
    if (mode == 0 || (mode < 3 && bg < 2) || mode == 6 || mode == 7) {
        return GetTextClickAddress(base, x, y);
    }
    // rot bgs (8 bits)
    if (mode < 3) {
        return base + (x >> 3) + (w >> 3) * (y >> 3);
    }
    // mode 3/5 (16 bits)
    if (mode != 4) {
        return 0x6000000 + 0xa000 * frame + 2 * x + w * y * 2;
    }
    // mode 4 (8 bits)
    return 0x6000000 + 0xa000 * frame + x + w * y;
}

LRESULT MapView::OnMapInfo(WPARAM wParam, LPARAM lParam)
{
    uint8_t* colors = (uint8_t*)lParam;
    mapViewZoom.setColors(colors);

    int x = (int)(wParam & 0xffff);
    int y = (int)(wParam >> 16);

    CString buffer;
    buffer.Format("(%d,%d)", x, y);
    GetDlgItem(IDC_XY)->SetWindowText(buffer);

    uint32_t address = GetClickAddress(x, y);
    buffer.Format("0x%08X", address);
    GetDlgItem(IDC_ADDRESS)->SetWindowText(buffer);

    int mode = DISPCNT & 7;
    if (mode >= 3 && mode <= 5) {
        // bitmap modes
        GetDlgItem(IDC_TILE_NUM)->SetWindowText("---");
        GetDlgItem(IDC_FLIP)->SetWindowText("--");
        GetDlgItem(IDC_PALETTE_NUM)->SetWindowText("---");
    } else if (mode == 0 || bg < 2) {
        // text bgs
        uint16_t value = *((uint16_t*)&vram[address - 0x6000000]);

        int tile = value & 1023;
        buffer.Format("%d", tile);
        GetDlgItem(IDC_TILE_NUM)->SetWindowText(buffer);
        buffer.Empty();
        buffer += value & 1024 ? 'H' : '-';
        buffer += value & 2048 ? 'V' : '-';
        GetDlgItem(IDC_FLIP)->SetWindowText(buffer);

        if (!(control & 0x80)) {
            buffer.Format("%d", (value >> 12) & 15);
        } else
            buffer = "---";
        GetDlgItem(IDC_PALETTE_NUM)->SetWindowText(buffer);
    } else {
        // rot bgs
        GetDlgItem(IDC_TILE_NUM)->SetWindowText("---");
        GetDlgItem(IDC_FLIP)->SetWindowText("--");
        GetDlgItem(IDC_PALETTE_NUM)->SetWindowText("---");
    }

    return TRUE;
}

LRESULT MapView::OnColInfo(WPARAM wParam, LPARAM lParam)
{
    uint16_t c = (uint16_t)wParam;

    color.setColor(c);

    int r = (c & 0x1f);
    int g = (c & 0x3e0) >> 5;
    int b = (c & 0x7c00) >> 10;

    CString buffer;
    buffer.Format("R: %d", r);
    GetDlgItem(IDC_R)->SetWindowText(buffer);

    buffer.Format("G: %d", g);
    GetDlgItem(IDC_G)->SetWindowText(buffer);

    buffer.Format("B: %d", b);
    GetDlgItem(IDC_B)->SetWindowText(buffer);

    return TRUE;
}

void MapView::saveBMP(const char* name)
{
    uint8_t writeBuffer[1024 * 3];

    FILE* fp = fopen(name, "wb");

    if (!fp) {
        systemMessage(MSG_ERROR_CREATING_FILE, "Error creating file %s", name);
        return;
    }

    struct {
        uint8_t ident[2];
        uint8_t filesize[4];
        uint8_t reserved[4];
        uint8_t dataoffset[4];
        uint8_t headersize[4];
        uint8_t width[4];
        uint8_t height[4];
        uint8_t planes[2];
        uint8_t bitsperpixel[2];
        uint8_t compression[4];
        uint8_t datasize[4];
        uint8_t hres[4];
        uint8_t vres[4];
        uint8_t colors[4];
        uint8_t importantcolors[4];
        uint8_t pad[2];
    } bmpheader;
    memset(&bmpheader, 0, sizeof(bmpheader));

    bmpheader.ident[0] = 'B';
    bmpheader.ident[1] = 'M';

    uint32_t fsz = sizeof(bmpheader) + w * h * 3;
    utilPutDword(bmpheader.filesize, fsz);
    utilPutDword(bmpheader.dataoffset, 0x38);
    utilPutDword(bmpheader.headersize, 0x28);
    utilPutDword(bmpheader.width, w);
    utilPutDword(bmpheader.height, h);
    utilPutDword(bmpheader.planes, 1);
    utilPutDword(bmpheader.bitsperpixel, 24);
    utilPutDword(bmpheader.datasize, 3 * w * h);

    fwrite(&bmpheader, 1, sizeof(bmpheader), fp);

    uint8_t* b = writeBuffer;

    int sizeX = w;
    int sizeY = h;

    uint8_t* pixU8 = (uint8_t*)data + 3 * w * (h - 1);
    for (int y = 0; y < sizeY; y++) {
        for (int x = 0; x < sizeX; x++) {
            *b++ = *pixU8++; // B
            *b++ = *pixU8++; // G
            *b++ = *pixU8++; // R
        }
        pixU8 -= 2 * 3 * w;
        fwrite(writeBuffer, 1, 3 * w, fp);

        b = writeBuffer;
    }

    fclose(fp);
}

void MapView::savePNG(const char* name)
{
    uint8_t writeBuffer[1024 * 3];

    FILE* fp = fopen(name, "wb");

    if (!fp) {
        systemMessage(MSG_ERROR_CREATING_FILE, "Error creating file %s", name);
        return;
    }

    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,
        NULL,
        NULL,
        NULL);
    if (!png_ptr) {
        fclose(fp);
        return;
    }

    png_infop info_ptr = png_create_info_struct(png_ptr);

    if (!info_ptr) {
        png_destroy_write_struct(&png_ptr, NULL);
        fclose(fp);
        return;
    }

    if (setjmp(png_ptr->jmpbuf)) {
        png_destroy_write_struct(&png_ptr, NULL);
        fclose(fp);
        return;
    }

    png_init_io(png_ptr, fp);

    png_set_IHDR(png_ptr,
        info_ptr,
        w,
        h,
        8,
        PNG_COLOR_TYPE_RGB,
        PNG_INTERLACE_NONE,
        PNG_COMPRESSION_TYPE_DEFAULT,
        PNG_FILTER_TYPE_DEFAULT);

    png_write_info(png_ptr, info_ptr);

    uint8_t* b = writeBuffer;

    int sizeX = w;
    int sizeY = h;

    uint8_t* pixU8 = (uint8_t*)data;
    for (int y = 0; y < sizeY; y++) {
        for (int x = 0; x < sizeX; x++) {
            int blue = *pixU8++;
            int green = *pixU8++;
            int red = *pixU8++;

            *b++ = red;
            *b++ = green;
            *b++ = blue;
        }
        png_write_row(png_ptr, writeBuffer);

        b = writeBuffer;
    }

    png_write_end(png_ptr, info_ptr);

    png_destroy_write_struct(&png_ptr, &info_ptr);

    fclose(fp);
}

void MapView::OnSave()
{
    if (rom != NULL) {
        CString filename;

        if (captureFormat == 0)
            filename = "map.png";
        else
            filename = "map.bmp";

        LPCTSTR exts[] = { ".png", ".bmp" };

        CString filter = theApp.winLoadFilter(IDS_FILTER_PNG);
        CString title = winResLoadString(IDS_SELECT_CAPTURE_NAME);

        FileDlg dlg(this,
            filename,
            filter,
            captureFormat ? 2 : 1,
            captureFormat ? "BMP" : "PNG",
            exts,
            "",
            title,
            true);

        if (dlg.DoModal() == IDCANCEL) {
            return;
        }

        if (dlg.getFilterIndex() == 2)
            saveBMP(dlg.GetPathName());
        else
            savePNG(dlg.GetPathName());
    }
}
