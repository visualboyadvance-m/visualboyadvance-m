// These are all the viewer dialogs with graphical panel areas. They can be
// instantiated multiple times. Port of the wx port's gfxviewers.cpp.

#include <cstdio>
#include <cstring>
#include <vector>

#include <QColorDialog>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QtEndian>

#include "core/base/system.h"
#include "core/gb/gbGlobals.h"
#include "core/gba/gbaGlobals.h"
#include "qt/app.h"
#include "qt/config/option-proxy.h"
#include "qt/game-area.h"
#include "qt/main-window.h"
#include "qt/viewers/viewer.h"

// FIXME: this should be in a header
extern uint8_t gbInvertTab[256];

namespace viewers {

namespace {

void utilReadScreenPixels(uint8_t* dest, int w, int h) {
    uint8_t* b = dest;
    int sizeX = w;
    int sizeY = h;
    switch (systemColorDepth) {
        case 8: {
            uint8_t* p = (uint8_t*)(g_pix + (w + 2));  // skip first black line
            for (int y = 0; y < sizeY; y++) {
                for (int x = 0; x < sizeX; x++) {
                    uint8_t v = *p++;

                    // White color fix
                    if (v == 0xff) {
                        *b++ = 0xff;
                        *b++ = 0xff;
                        *b++ = 0xff;
                    } else {
                        *b++ = (((v >> 5) & 0x7) << 5);
                        *b++ = (((v >> 2) & 0x7) << 5);
                        *b++ = ((v & 0x3) << 6);
                    }
                }
                p++;  // skip black pixel for filters
                p++;  // skip black pixel for filters
            }
        } break;
        case 16: {
            uint16_t* p = (uint16_t*)(g_pix + (w + 2) * 2);  // skip first black line
            for (int y = 0; y < sizeY; y++) {
                for (int x = 0; x < sizeX; x++) {
                    uint16_t v = *p++;

                    *b++ = ((v >> systemRedShift) & 0x001f) << 3;    // R
                    *b++ = ((v >> systemGreenShift) & 0x001f) << 3;  // G
                    *b++ = ((v >> systemBlueShift) & 0x01f) << 3;    // B
                }
                p++;  // skip black pixel for filters
                p++;  // skip black pixel for filters
            }
        } break;
        case 24: {
            uint8_t* pixU8 = (uint8_t*)g_pix;
            for (int y = 0; y < sizeY; y++) {
                for (int x = 0; x < sizeX; x++) {
                    if (systemRedShift < systemBlueShift) {
                        *b++ = *pixU8++;  // R
                        *b++ = *pixU8++;  // G
                        *b++ = *pixU8++;  // B
                    } else {
                        uint8_t blue = *pixU8++;
                        uint8_t green = *pixU8++;
                        uint8_t red = *pixU8++;

                        *b++ = red;
                        *b++ = green;
                        *b++ = blue;
                    }
                }
            }
        } break;
        case 32: {
            uint32_t* pixU32 = (uint32_t*)(g_pix + 4 * (w + 1));
            for (int y = 0; y < sizeY; y++) {
                for (int x = 0; x < sizeX; x++) {
                    uint32_t v = *pixU32++;
                    *b++ = ((v >> systemBlueShift) & 0x001f) << 3;   // B
                    *b++ = ((v >> systemGreenShift) & 0x001f) << 3;  // G
                    *b++ = ((v >> systemRedShift) & 0x001f) << 3;    // R
                }
                pixU32++;
            }
        } break;
    }
}

// Copies a `w` x `h` RGB24 block (stride `src_stride` pixels) into an RGB24
// destination image of width `dst_w` at (x, y). Equivalent of wxImage::Paste.
void PasteRGB(uint8_t* dst, int dst_w, const uint8_t* src, int src_stride, int w, int h,
              int x, int y) {
    for (int j = 0; j < h; j++) {
        std::memcpy(dst + ((y + j) * dst_w + x) * 3, src + j * src_stride * 3, w * 3);
    }
}

// A value label sized for `sample` (the labels with mv= in the wx getlab macro).
QLabel* NewValueLabel(QWidget* parent, const QString& sample) {
    QLabel* lab = new QLabel(sample, parent);
    lab->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    lab->setMinimumWidth(lab->sizeHint().width());
    return lab;
}

QGroupBox* NewRadioGroup(QWidget* parent, const QString& title,
                         const std::vector<QRadioButton*>& radios) {
    QGroupBox* box = new QGroupBox(title, parent);
    QVBoxLayout* l = new QVBoxLayout(box);
    for (QRadioButton* r : radios)
        l->addWidget(r);
    return box;
}

// Standard three-button row of the gfx viewers.
QLayout* GfxButtonRow(QPushButton* refresh, QPushButton* save, QPushButton* close) {
    QHBoxLayout* l = new QHBoxLayout();
    l->addWidget(refresh);
    l->addWidget(save);
    l->addWidget(close);
    return l;
}

// Standard zoom + color view block.
QLayout* ZoomBlock(PixView* zoom, ColorView* cv) {
    QVBoxLayout* l = new QVBoxLayout();
    zoom->setFixedSize(64, 64);
    l->addWidget(zoom, 0, Qt::AlignHCenter);
    l->addWidget(cv);
    l->addStretch();
    return l;
}

// FIXME: many of these read e.g. palette data directly without regard to
// byte order.  Need to determine where things are stored in emulated machine
// order and where in native order, and swap the latter on big-endian

// ---------------------------------------------------------------------------
// GBA map viewer

class MapViewer final : public GfxViewer {
public:
    explicit MapViewer(QWidget* parent) : GfxViewer(parent, QStringLiteral("MapViewer"), 1024, 1024) {
        setWindowTitle(tr("Map Viewer"));
        frame = bg = 0;

        fr0 = new QRadioButton(tr("Frame 0"), this);
        fr1 = new QRadioButton(tr("Frame 1"), this);
        BindRadio(fr0, &frame, 0);
        BindRadio(fr1, &frame, 0xa000);
        bg0 = new QRadioButton(QStringLiteral("BG 0"), this);
        bg1 = new QRadioButton(QStringLiteral("BG 1"), this);
        bg2 = new QRadioButton(QStringLiteral("BG 2"), this);
        bg3 = new QRadioButton(QStringLiteral("BG 3"), this);
        BindRadio(bg0, &bg, 0);
        BindRadio(bg1, &bg, 1);
        BindRadio(bg2, &bg, 2);
        BindRadio(bg3, &bg, 3);

        QVBoxLayout* top = new QVBoxLayout(this);
        QHBoxLayout* main = new QHBoxLayout();

        QVBoxLayout* left = new QVBoxLayout();
        left->addWidget(NewRadioGroup(this, tr("Frame"), {fr0, fr1}));
        left->addWidget(NewRadioGroup(this, tr("Background"), {bg0, bg1, bg2, bg3}));
        left->addWidget(str_);
        left->addWidget(auto_update_);

        QGridLayout* info = new QGridLayout();
        int row = 0;
        auto add_info = [&](const QString& label, QLabel*& lab, const char* sample) {
            info->addWidget(new QLabel(label, this), row, 0);
            lab = NewValueLabel(this, QString::fromLatin1(sample));
            info->addWidget(lab, row, 1);
            row++;
        };
        add_info(tr("Mode:"), modelab, "8");
        add_info(tr("Map Base:"), mapbase, "0xWWWWWWWW");
        add_info(tr("Char Base:"), charbase, "0xWWWWWWWW");
        add_info(tr("Size:"), size, "1024x1024");
        add_info(tr("Colors:"), colors, "2WW");
        add_info(tr("Priority:"), prio, "3");
        add_info(tr("Mosaic:"), mosaic, "0");
        add_info(tr("Overflow:"), overflow, "0");
        coords_ = NewValueLabel(this, QStringLiteral("(1023,1023)"));
        info->addWidget(coords_, row++, 0, 1, 2);
        add_info(tr("Address:"), addr_, "0xWWWWWWWW");
        add_info(tr("Tile:"), tile_, "1023");
        add_info(tr("Flip:"), flip_, "HV");
        add_info(tr("Palette:"), palette_, "---");
        left->addLayout(info);
        left->addStretch();
        main->addLayout(left);

        gvs_->setMinimumSize(512 + 20, 512 + 20);
        main->addWidget(gvs_, 1);
        main->addLayout(ZoomBlock(zoom_, cv_));
        top->addLayout(main, 1);
        top->addLayout(GfxButtonRow(refresh_, save_, close_));

        connect(gv, &GfxPanel::gfxClick, this, &MapViewer::UpdateMouseInfoEv);

        selx = sely = -1;
        Fit();
        Update();
    }

    void Update() override {
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

        bool fr0en = true, fr1en = true, bg0en = true, bg1en = true, bg2en = true, bg3en = true;

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
                    SetRadioQuiet(bg0);
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
                    SetRadioQuiet(bg2);
                }

                renderRotScreen();
                break;

            case 3:
                fr0en = fr1en = false;
                bg0en = bg1en = bg2en = bg3en = false;
                bg = 2;
                SetRadioQuiet(bg2);
                renderMode3();
                break;

            case 4:
                bg0en = bg1en = bg2en = bg3en = false;
                bg = 2;
                SetRadioQuiet(bg2);
                renderMode4();
                break;

            case 5:
            case 6:
            case 7:
                bg = 2;
                SetRadioQuiet(bg2);
                renderMode5();
                break;
        }

        ChangeBMP();
        fr0->setEnabled(fr0en);
        fr1->setEnabled(fr1en);
        bg0->setEnabled(bg0en);
        bg1->setEnabled(bg1en);
        bg2->setEnabled(bg2en);
        bg3->setEnabled(bg3en);
        modelab->setText(QString::number(static_cast<int>(mode)));

        if (mode >= 3) {
            mapbase->setText(QString());
            charbase->setText(QString());
        } else {
            mapbase->setText(QStringLiteral("0x") +
                             Hex(((control >> 8) & 0x1f) * 0x800 + 0x6000000, 8));
            charbase->setText(QStringLiteral("0x") +
                              Hex(((control >> 2) & 0x03) * 0x4000 + 0x6000000, 8));
        }

        size->setText(QStringLiteral("%1x%2").arg(gv->bmw).arg(gv->bmh));
        colors->setText(control & 0x80 ? QStringLiteral("256") : QStringLiteral("16"));
        prio->setText(QString::number(control & 3));
        mosaic->setText(control & 0x40 ? QStringLiteral("1") : QStringLiteral("0"));
        overflow->setText(bg <= 1 ? QString()
                                  : control & 0x2000 ? QStringLiteral("1") : QStringLiteral("0"));
        UpdateMouseInfo();
    }

private:
    // Checks a radio without triggering its Update() binding.
    static void SetRadioQuiet(QRadioButton* r) {
        const bool blocked = r->blockSignals(true);
        r->setChecked(true);
        r->blockSignals(blocked);
    }

    void UpdateMouseInfoEv(int x, int y) {
        selx = x;
        sely = y;
        UpdateMouseInfo();  // note that this will be inaccurate if game
        // not paused since last refresh
    }

    uint32_t AddressFromSel() {
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

    void UpdateMouseInfo() {
        if (selx > gv->bmw || sely > gv->bmh)
            selx = sely = -1;

        if (selx < 0) {
            coords_->setText(QString());
            addr_->setText(QString());
            tile_->setText(QString());
            flip_->setText(QString());
            palette_->setText(QString());
        } else {
            coords_->setText(QStringLiteral("(%1,%2)").arg(selx).arg(sely));
            uint32_t address = AddressFromSel();
            addr_->setText(QStringLiteral("0x") + Hex(address, 8));

            if ((!mode || (mode < 3 || mode > 5)) && bg < 2) {
                uint16_t value = *((uint16_t*)&g_vram[address - 0x6000000]);
                tile_->setText(QString::number(value & 1023));
                QString s = value & 1024 ? QStringLiteral("H") : QStringLiteral("-");
                s += value & 2048 ? QLatin1Char('V') : QLatin1Char('-');
                flip_->setText(s);

                if (control & 0x80)
                    palette_->setText(QStringLiteral("---"));
                else
                    palette_->setText(QString::number((value >> 12) & 15));
            } else {
                tile_->setText(QStringLiteral("---"));
                flip_->setText(QStringLiteral("--"));
                palette_->setText(QStringLiteral("---"));
            }
        }
    }

    // following routines were copied from win32/MapView.cpp with little
    // attempt to read & validate, except:
    //    stride = 1024, rgb instead of bgr
    // FIXME: probably needs changing for big-endian

    void renderTextScreen() {
        uint16_t* palette = (uint16_t*)g_paletteRAM;
        uint8_t* charBase = &g_vram[((control >> 2) & 0x03) * 0x4000];
        uint16_t* screenBase = (uint16_t*)&g_vram[((control >> 8) & 0x1f) * 0x800];
        uint8_t* bmp = ImageData();
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
    }

    void renderRotScreen() {
        uint16_t* palette = (uint16_t*)g_paletteRAM;
        uint8_t* charBase = &g_vram[((control >> 2) & 0x03) * 0x4000];
        uint8_t* screenBase = (uint8_t*)&g_vram[((control >> 8) & 0x1f) * 0x800];
        uint8_t* bmp = ImageData();
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

            bmp += 3 * (1024 - sizeX);
        }
    }

    void renderMode3() {
        uint8_t* bmp = ImageData();
        uint16_t* src = (uint16_t*)&g_vram[0];
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

    void renderMode4() {
        uint8_t* bmp = ImageData();
        uint8_t* src = frame ? &g_vram[0xa000] : &g_vram[0];
        uint16_t* pal = (uint16_t*)&g_paletteRAM[0];
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

    void renderMode5() {
        uint8_t* bmp = ImageData();
        uint16_t* src = (uint16_t*)(frame ? &g_vram[0xa000] : &g_vram[0]);
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

    uint16_t control = 0, mode = 0;
    int frame, bg;
    QRadioButton *fr0, *fr1, *bg0, *bg1, *bg2, *bg3;
    QLabel *modelab, *mapbase, *charbase, *size, *colors, *prio, *mosaic, *overflow;
    QLabel *coords_, *addr_, *tile_, *flip_, *palette_;
    int selx, sely;
};

// ---------------------------------------------------------------------------
// GB map viewer

class GBMapViewer final : public GfxViewer {
public:
    explicit GBMapViewer(QWidget* parent)
        : GfxViewer(parent, QStringLiteral("GBMapViewer"), 256, 256) {
        setWindowTitle(tr("Map Viewer"));
        charbase = 0x0000;
        mapbase = 0x1800;

        QRadioButton* cb0 = new QRadioButton(QStringLiteral("0x8000"), this);
        QRadioButton* cb1 = new QRadioButton(QStringLiteral("0x8800"), this);
        BindRadio(cb0, &charbase, 0x0000);
        BindRadio(cb1, &charbase, 0x0800);
        QRadioButton* mb0 = new QRadioButton(QStringLiteral("0x9800"), this);
        QRadioButton* mb1 = new QRadioButton(QStringLiteral("0x9C00"), this);
        BindRadio(mb0, &mapbase, 0x1800);
        BindRadio(mb1, &mapbase, 0x1c00);

        QVBoxLayout* top = new QVBoxLayout(this);
        QHBoxLayout* main = new QHBoxLayout();

        QVBoxLayout* left = new QVBoxLayout();
        left->addWidget(NewRadioGroup(this, tr("Char Base"), {cb0, cb1}));
        left->addWidget(NewRadioGroup(this, tr("Map Base"), {mb0, mb1}));
        left->addWidget(str_);
        left->addWidget(auto_update_);

        QGridLayout* info = new QGridLayout();
        int row = 0;
        coords_ = NewValueLabel(this, QStringLiteral("(2WW,2WW)"));
        info->addWidget(coords_, row++, 0, 1, 2);
        auto add_info = [&](const QString& label, QLabel*& lab, const char* sample) {
            info->addWidget(new QLabel(label, this), row, 0);
            lab = NewValueLabel(this, QString::fromLatin1(sample));
            info->addWidget(lab, row, 1);
            row++;
        };
        add_info(tr("Address:"), addr_, "0xWWWW");
        add_info(tr("Tile:"), tile_, "2WW");
        add_info(tr("Flip:"), flip_, "HV");
        add_info(tr("Palette:"), palette_, "---");
        add_info(tr("Priority:"), prio_, "P");
        left->addLayout(info);
        left->addStretch();
        main->addLayout(left);

        gvs_->setMinimumSize(256 + 20, 256 + 20);
        main->addWidget(gvs_, 1);
        main->addLayout(ZoomBlock(zoom_, cv_));
        top->addLayout(main, 1);
        top->addLayout(GfxButtonRow(refresh_, save_, close_));

        connect(gv, &GfxPanel::gfxClick, this, &GBMapViewer::UpdateMouseInfoEv);

        selx = sely = -1;
        Fit();
        Update();
    }

    void Update() override {
        uint8_t *bank0, *bank1;

        if (gbCgbMode) {
            bank0 = &gbVram[0x0000];
            bank1 = &gbVram[0x2000];
        } else {
            bank0 = &gbMemory[0x8000];
            bank1 = nullptr;
        }

        int tile_map_address = mapbase;
        // following copied almost verbatim from win32/GBMapView.cpp

        for (int y = 0; y < 32; y++) {
            for (int x = 0; x < 32; x++) {
                uint8_t* bmp = &ImageData()[y * 8 * 32 * 24 + x * 24];
                uint8_t attrs = 0;

                if (bank1 != nullptr)
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
                    int charbase_address = attrs & 0x40 ? charbase + tile * 16 + (7 - j) * 2
                                                        : charbase + tile * 16 + j * 2;
                    uint8_t tile_a = 0;
                    uint8_t tile_b = 0;

                    if ((attrs & 0x08) && bank1) {
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

private:
    void UpdateMouseInfoEv(int x, int y) {
        selx = x;
        sely = y;
        UpdateMouseInfo();  // note that this will be inaccurate if game
        // not paused since last refresh
    }

    void UpdateMouseInfo() {
        if (selx > gv->bmw || sely > gv->bmh)
            selx = sely = -1;

        if (selx < 0) {
            coords_->setText(QString());
            addr_->setText(QString());
            tile_->setText(QString());
            flip_->setText(QString());
            palette_->setText(QString());
            prio_->setText(QString());
        } else {
            coords_->setText(QStringLiteral("(%1,%2)").arg(selx).arg(sely));
            uint16_t address = mapbase + 0x8000 + (sely >> 3) * 32 + (selx >> 3);
            addr_->setText(QStringLiteral("0x") + Hex(address, 4));
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

            tile_->setText(QString::number(static_cast<int>(tilev)));
            QString s = attrs & 0x20 ? QStringLiteral("H") : QStringLiteral("-");
            s += attrs & 0x40 ? QLatin1Char('V') : QLatin1Char('-');
            flip_->setText(s);

            if (gbCgbMode)
                palette_->setText(QString::number(attrs & 7));
            else
                palette_->setText(QStringLiteral("---"));

            prio_->setText(attrs & 0x80 ? QStringLiteral("P") : QStringLiteral("-"));
        }
    }

    int charbase, mapbase;
    QLabel *coords_, *addr_, *tile_, *flip_, *palette_, *prio_;
    int selx, sely;
};

// ---------------------------------------------------------------------------
// GBA OAM viewer

class OAMViewer final : public GfxViewer {
public:
    explicit OAMViewer(QWidget* parent) : GfxViewer(parent, QStringLiteral("OAMViewer"), 544, 496) {
        setWindowTitle(tr("OAM Viewer"));
        sprite = 0;

        QVBoxLayout* top = new QVBoxLayout(this);
        QHBoxLayout* main = new QHBoxLayout();

        QVBoxLayout* left = new QVBoxLayout();
        QHBoxLayout* spin_row = new QHBoxLayout();
        spin_row->addWidget(new QLabel(tr("Sprite:"), this));
        QSpinBox* spin = new QSpinBox(this);
        spin->setRange(0, 127);
        BindSpin(spin, &sprite);
        spin_row->addWidget(spin);
        left->addLayout(spin_row);

        QGridLayout* info = new QGridLayout();
        int row = 0;
        auto add_info = [&](const QString& label, QLabel*& lab, const char* sample) {
            info->addWidget(new QLabel(label, this), row, 0);
            lab = NewValueLabel(this, QString::fromLatin1(sample));
            info->addWidget(lab, row, 1);
            row++;
        };
        add_info(tr("Pos:"), pos, "5WW,2WW");
        add_info(tr("Mode:"), mode, "3");
        add_info(tr("Colors:"), colors, "256");
        add_info(tr("Pal:"), pallab, "1W");
        add_info(tr("Tile:"), tile, "1WWW");
        add_info(tr("Prio:"), prio, "3");
        add_info(tr("Size:"), size, "64x64");
        add_info(tr("Rot.:"), rot, "3W");
        add_info(tr("Flags:"), flg, "RHVMD");
        left->addLayout(info);
        left->addWidget(str_);
        left->addWidget(auto_update_);
        left->addStretch();
        main->addLayout(left);

        QVBoxLayout* right = new QVBoxLayout();
        gvs_->setMinimumSize(544 + 20, 496 + 20);
        right->addWidget(gvs_, 1);
        QHBoxLayout* zoom_row = new QHBoxLayout();
        zoom_row->addLayout(ZoomBlock(zoom_, cv_));
        zoom_row->addStretch();
        right->addLayout(zoom_row);
        main->addLayout(right, 1);
        top->addLayout(main, 1);
        top->addLayout(GfxButtonRow(refresh_, save_, close_));

        Fit();
        Update();
    }

    void Update() override {
        BMPSize(544, 496);
        std::vector<uint8_t> screen(240 * 160 * 3);
        systemRedShift = 19;
        systemGreenShift = 11;
        systemBlueShift = 3;
        utilReadScreenPixels(screen.data(), 240, 160);
        systemRedShift = 3;
        systemGreenShift = 11;
        systemBlueShift = 19;

        for (int sprite_no = 0; sprite_no < 128; sprite_no++) {
            uint16_t* sparms = &((uint16_t*)g_oam)[4 * sprite_no];
            uint16_t a0 = sparms[0], a1 = sparms[1], a2 = sparms[2];
            uint16_t* pal = &((uint16_t*)g_paletteRAM)[0x100];
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
                    pos->setText(QString());
                    mode->setText(QString());
                    colors->setText(QString());
                    pallab->setText(QString());
                    tile->setText(QString());
                    prio->setText(QString());
                    size->setText(QString());
                    rot->setText(QString());
                    flg->setText(QString());
                    continue;
            }

            std::vector<uint8_t> spriteData(64 * 64 * 3, 0);
            uint8_t* bmp = spriteData.data();

            if (a0 & 0x2000) {
                int c = (a2 & 0x3FF);
                int inc = 32;

                if (DISPCNT & 0x40)
                    inc = sizeX >> 2;
                else
                    c &= 0x3FE;

                for (int y = 0; y < sizeY; y++) {
                    for (int x = 0; x < sizeX; x++) {
                        uint32_t color = g_vram[0x10000 + (((c + (((y >> 3) * inc) << 5) +
                                                              ((y & 7) << 4) + ((x >> 3) << 6) +
                                                              (x & 7))) &
                                                            0x7FFF)];
                        color = pal[color];

                        *bmp++ = (color & 0x1f) << 3;
                        *bmp++ = ((color >> 5) & 0x1f) << 3;
                        *bmp++ = ((color >> 10) & 0x1f) << 3;
                    }

                    bmp += (64 - sizeX) * 3;
                }
            } else {
                int c = (a2 & 0x3FF);
                int inc = 32;

                if (DISPCNT & 0x40)
                    inc = sizeX >> 3;

                int palette = (a2 >> 8) & 0xF0;

                for (int y = 0; y < sizeY; y++) {
                    for (int x = 0; x < sizeX; x++) {
                        uint32_t color =
                            g_vram[0x10000 + ((((((c + (((y >> 3) * inc) << 5)) + ((y & 7) << 2)) +
                                                 ((x >> 3) << 5)) +
                                                ((x & 7) >> 1))) &
                                              0x7FFF)];

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
                pos->setText(QStringLiteral("%1,%2").arg(a1 & 511).arg(a0 & 255));
                mode->setText(QString::number((a0 >> 10) & 3));
                colors->setText(a0 & 8192 ? QStringLiteral("256") : QStringLiteral("16"));
                pallab->setText(QString::number((a2 >> 12) & 15));
                tile->setText(QString::number(a2 & 1023));
                prio->setText(QString::number((a2 >> 10) & 3));
                size->setText(QStringLiteral("%1x%2").arg(sizeX).arg(sizeY));

                if (a0 & 512)
                    rot->setText(QString::number((a1 >> 9) & 31));
                else
                    rot->setText(QString());

                QString s;

                if (a0 & 512)
                    s.append(QStringLiteral("R--"));
                else {
                    s.append(QLatin1Char('-'));
                    s.append(a1 & 4096 ? QLatin1Char('H') : QLatin1Char('-'));
                    s.append(a1 & 8192 ? QLatin1Char('V') : QLatin1Char('-'));
                }

                s.append(a0 & 4096 ? QLatin1Char('M') : QLatin1Char('-'));
                s.append(a0 & 1024 ? QLatin1Char('D') : QLatin1Char('-'));
                flg->setText(s);
                uint8_t* box = spriteData.data();
                int sprite_posx = a1 & 511;
                int sprite_posy = a0 & 255;
                uint8_t* screen_box = screen.data();
                const bool on_screen = sprite_posx >= 0 && sprite_posx <= (239 - sizeY) &&
                                       sprite_posy >= 0 && sprite_posy <= (159 - sizeX);

                if (on_screen)
                    screen_box += (sprite_posx * 3) + (sprite_posy * 240 * 3);

                for (int y = 0; y < sizeY; y++) {
                    for (int x = 0; x < sizeX; x++) {
                        uint32_t color = 0;

                        if (y == 0 || y == sizeY - 1 || x == 0 || x == sizeX - 1) {
                            color = 255;
                            *box++ = (color & 0x1f) << 3;
                            *box++ = ((color >> 5) & 0x1f) << 3;
                            *box++ = ((color >> 10) & 0x1f) << 3;

                            if (on_screen) {
                                *screen_box++ = (color & 0x1f) << 3;
                                *screen_box++ = ((color >> 5) & 0x1f) << 3;
                                *screen_box++ = ((color >> 10) & 0x1f) << 3;
                            }
                        } else {
                            box += 3;

                            if (on_screen)
                                screen_box += 3;
                        }
                    }

                    box += (64 - sizeX) * 3;

                    if (on_screen)
                        screen_box += (240 - sizeX) * 3;
                }
            }

            PasteRGB(ImageData(), ImageWidth(), spriteData.data(), 64, 64, 64,
                     (sprite_no % 16) * 34, (sprite_no / 16) * 34);
        }

        PasteRGB(ImageData(), ImageWidth(), screen.data(), 240, 240, 160, 0, 304);
        ChangeBMP();
    }

private:
    int sprite;
    QLabel *pos, *mode, *colors, *pallab, *tile, *prio, *size, *rot, *flg;
};

// ---------------------------------------------------------------------------
// GB OAM viewer

class GBOAMViewer final : public GfxViewer {
public:
    explicit GBOAMViewer(QWidget* parent) : GfxViewer(parent, QStringLiteral("GBOAMViewer"), 8, 16) {
        setWindowTitle(tr("OAM Viewer"));
        sprite = 0;

        QVBoxLayout* top = new QVBoxLayout(this);
        QHBoxLayout* main = new QHBoxLayout();

        QVBoxLayout* left = new QVBoxLayout();
        QHBoxLayout* spin_row = new QHBoxLayout();
        spin_row->addWidget(new QLabel(tr("Sprite:"), this));
        QSpinBox* spin = new QSpinBox(this);
        spin->setRange(0, 39);
        BindSpin(spin, &sprite);
        spin_row->addWidget(spin);
        left->addLayout(spin_row);

        QGridLayout* info = new QGridLayout();
        int row = 0;
        auto add_info = [&](const QString& label, QLabel*& lab, const char* sample) {
            info->addWidget(new QLabel(label, this), row, 0);
            lab = NewValueLabel(this, QString::fromLatin1(sample));
            info->addWidget(lab, row, 1);
            row++;
        };
        add_info(tr("Pos:"), pos, "2WW,2WW");
        add_info(tr("Tile:"), tilelab, "2WW");
        add_info(tr("Prio:"), prio, "W");
        add_info(QStringLiteral("OAP:"), oap, "W");
        add_info(tr("Pal:"), pallab, "W");
        add_info(tr("Flags:"), flg, "HV");
        add_info(tr("Bank:"), banklab, "W");
        left->addLayout(info);
        left->addWidget(str_);
        left->addWidget(auto_update_);
        left->addStretch();
        main->addLayout(left);

        QVBoxLayout* right = new QVBoxLayout();
        gvs_->setMinimumSize(8 * 8 + 20, 16 * 8 + 20);
        // The sprite is tiny; show it stretched by default.
        right->addWidget(gvs_, 1);
        QHBoxLayout* zoom_row = new QHBoxLayout();
        zoom_row->addLayout(ZoomBlock(zoom_, cv_));
        zoom_row->addStretch();
        right->addLayout(zoom_row);
        main->addLayout(right, 1);
        top->addLayout(main, 1);
        top->addLayout(GfxButtonRow(refresh_, save_, close_));

        Fit();
        Update();
    }

    void Update() override {
        uint8_t* bmp = ImageData();
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
            bank0 = &gbVram[0x0000];
            bank1 = &gbVram[0x2000];
        } else {
            bank0 = &gbMemory[0x8000];
            bank1 = nullptr;
        }

        int init = 0x0000;
        uint8_t* pal = gbObp0;

        if ((flags & 0x10))
            pal = gbObp1;

        for (int yy = 0; yy < h; yy++) {
            int address = init + tile * 16 + 2 * yy;
            int a = 0;
            int b = 0;

            if (gbCgbMode && (flags & 0x08) && bank1) {
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
        pos->setText(QStringLiteral("%1,%2").arg(x).arg(y));
        tilelab->setText(QString::number(tile));
        prio->setText(flags & 0x80 ? QStringLiteral("1") : QStringLiteral("0"));
        oap->setText(flags & 0x08 ? QStringLiteral("1") : QStringLiteral("0"));
        pallab->setText(QString::number(flags & 7));
        QString s = flags & 0x20 ? QStringLiteral("H") : QStringLiteral("-");
        s.append(flags & 0x40 ? QLatin1Char('V') : QLatin1Char('-'));
        flg->setText(s);
        banklab->setText(flags & 0x10 ? QStringLiteral("1") : QStringLiteral("0"));
    }

private:
    int sprite;
    QLabel *pos, *tilelab, *prio, *oap, *pallab, *flg, *banklab;
};

// ---------------------------------------------------------------------------
// Palette viewers

int ptype = 0;
QString pdir;

void savepal(QWidget* parent, const uint8_t* data, int ncols, const QString& type) {
    // no attempt is made here to translate the palette type name
    // it's just a suggested name, anyway
    QString def_name = vbamApp().frame->GetPanel()->game_name() + QLatin1Char('-') + type;

    if (ptype == 2)
        def_name += QStringLiteral(".act");
    else
        def_name += QStringLiteral(".pal");

    const QStringList filters = {QObject::tr("Windows Palette (*.pal)"),
                                 QObject::tr("PaintShop Palette (*.pal)"),
                                 QObject::tr("Adobe Color Table (*.act)")};
    QString selected = filters.value(ptype);
    const QString path = QFileDialog::getSaveFileName(
        parent, QObject::tr("Select output file and type"), QDir(pdir).filePath(def_name),
        filters.join(QStringLiteral(";;")), &selected);

    if (path.isEmpty())
        return;

    ptype = std::max(0, static_cast<int>(filters.indexOf(selected)));
    pdir = QFileInfo(path).absolutePath();

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return;

    // FIXME: check for errors
    switch (ptype) {
        case 0:  // Windows palette
        {
            f.write("RIFF", 4);
            uint32_t d = qToLittleEndian<uint32_t>(256 * 4 + 16);
            f.write(reinterpret_cast<const char*>(&d), 4);
            f.write("PAL data", 8);
            d = qToLittleEndian<uint32_t>(256 * 4 + 4);
            f.write(reinterpret_cast<const char*>(&d), 4);
            uint16_t w = qToLittleEndian<uint16_t>(0x0300);
            f.write(reinterpret_cast<const char*>(&w), 2);
            w = qToLittleEndian<uint16_t>(256);  // causes problems if not 16 or 256
            f.write(reinterpret_cast<const char*>(&w), 2);

            for (int i = 0; i < ncols; i++, data += 3) {
                f.write(reinterpret_cast<const char*>(data), 3);
                uint8_t z = 0;
                f.write(reinterpret_cast<const char*>(&z), 1);
            }

            for (int i = ncols; i < 256; i++) {
                d = 0;
                f.write(reinterpret_cast<const char*>(&d), 4);
            }
        } break;

        case 1:  // PaintShop palette
        {
            static const char jasc_head[] = "JASC-PAL\r\n0100\r\n256\r\n";
            f.write(jasc_head, sizeof(jasc_head) - 1);

            for (int i = 0; i < ncols; i++, data += 3) {
                char buf[14];
                int l = snprintf(buf, sizeof(buf), "%d %d %d\r\n", data[0], data[1], data[2]);
                f.write(buf, l);
            }

            for (int i = ncols; i < 256; i++)
                f.write("0 0 0\r\n", 7);

            break;
        }

        case 2:  // Adobe color table
        {
            f.write(reinterpret_cast<const char*>(data), ncols * 3);
            uint32_t d = 0;

            for (int i = ncols; i < 256; i++)
                f.write(reinterpret_cast<const char*>(&d), 3);
        } break;
    }

    f.close();  // FIXME: check for errors
}

class PaletteViewer final : public Viewer {
public:
    explicit PaletteViewer(QWidget* parent) : Viewer(parent, QStringLiteral("PaletteViewer")) {
        setWindowTitle(tr("Palette Viewer"));

        QVBoxLayout* top = new QVBoxLayout(this);
        top->addWidget(new QLabel(tr("Click on a color for more information"), this));

        cv = new ColorView(this);
        bpv = new PixViewEvt(this);
        bpv->InitBMP(16, 16, cv);
        bpv->setFixedSize(16 * 10, 16 * 10);
        spv = new PixViewEvt(this);
        spv->InitBMP(16, 16, cv);
        spv->setFixedSize(16 * 10, 16 * 10);
        connect(bpv, &PixView::gfxClick, this, [this](int, int) { SelBG(); });
        connect(spv, &PixView::gfxClick, this, [this](int, int) { SelSprite(); });

        QGridLayout* grid = new QGridLayout();
        QGroupBox* bgbox = new QGroupBox(tr("Background"), this);
        QVBoxLayout* bgl = new QVBoxLayout(bgbox);
        bgl->addWidget(bpv);
        grid->addWidget(bgbox, 0, 0);
        QGroupBox* spbox = new QGroupBox(tr("Sprite"), this);
        QVBoxLayout* spl = new QVBoxLayout(spbox);
        spl->addWidget(spv);
        grid->addWidget(spbox, 0, 1);

        QGridLayout* info = new QGridLayout();
        info->addWidget(new QLabel(tr("Address:"), this), 0, 0);
        addr = NewValueLabel(this, QStringLiteral("0x5000WWW"));
        info->addWidget(addr, 0, 1);
        info->addWidget(new QLabel(tr("Value:"), this), 1, 0);
        val = NewValueLabel(this, QStringLiteral("0xWWWW"));
        info->addWidget(val, 1, 1);
        grid->addLayout(info, 1, 0);
        grid->addWidget(cv, 1, 1);
        top->addLayout(grid);

        QHBoxLayout* opts = new QHBoxLayout();
        opts->addWidget(NewAutoUpdateCheckBox());
        opts->addStretch();
        QPushButton* backdrop = new QPushButton(tr("C&hange backdrop color..."), this);
        connect(backdrop, &QPushButton::clicked, this, [this] { ChangeBackdrop(); });
        opts->addWidget(backdrop);
        top->addLayout(opts);

        QHBoxLayout* buttons = new QHBoxLayout();
        buttons->addWidget(NewRefreshButton());
        QPushButton* save_bg = new QPushButton(tr("Save &BG..."), this);
        connect(save_bg, &QPushButton::clicked, this,
                [this] { savepal(this, colbmp, 16 * 16, QStringLiteral("bg")); });
        buttons->addWidget(save_bg);
        QPushButton* save_obj = new QPushButton(tr("Save &Sprite..."), this);
        connect(save_obj, &QPushButton::clicked, this, [this] {
            savepal(this, colbmp + 16 * 16 * 3, 16 * 16, QStringLiteral("obj"));
        });
        buttons->addWidget(save_obj);
        buttons->addWidget(NewCloseButton());
        top->addLayout(buttons);

        Fit();
        Update();
    }

    void Update() override {
        if (g_paletteRAM) {
            uint16_t* pp = (uint16_t*)g_paletteRAM;
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

private:
    void SelBG() {
        spv->SetSel(-1, -1, false);
        ShowSel();
    }

    void SelSprite() {
        bpv->SetSel(-1, -1, false);
        ShowSel();
    }

    void ShowSel() {
        int x, y;
        bool isbg = true;
        bpv->GetSel(x, y);

        if (x < 0) {
            isbg = false;
            spv->GetSel(x, y);

            if (x < 0) {
                addr->setText(QString());
                val->setText(QString());
                return;
            }
        }

        int off = x + y * 16;

        if (!isbg)
            off += 16 * 16;

        uint8_t* pix = &colbmp[off * 3];
        uint16_t v = (pix[0] >> 3) + ((pix[1] >> 3) << 5) + ((pix[2] >> 3) << 10);
        val->setText(QStringLiteral("0x") + Hex(v, 4));
        addr->setText(QStringLiteral("0x") + Hex(0x5000000 + 2 * off, 8));
    }

    void ChangeBackdrop() {
        // FIXME: this should really be a preference
        // should also have some way of indicating selection
        static QColor last = Qt::black;
        const QColor c = QColorDialog::getColor(last, this);

        if (c.isValid()) {
            last = c;
            // Binary or the upper 5 bits of each color choice
            customBackdropColor = ((c.red() >> 3) != 0) || (((c.green() >> 3) << 5) != 0) ||
                                  (((c.blue() >> 3) << 10) != 0);
        } else
            // kind of an unintuitive way to turn it off...
            customBackdropColor = -1;
    }

    ColorView* cv;
    PixViewEvt *bpv, *spv;
    uint8_t colbmp[16 * 16 * 3 * 2];
    QLabel *addr, *val;
};

class GBPaletteViewer final : public Viewer {
public:
    explicit GBPaletteViewer(QWidget* parent) : Viewer(parent, QStringLiteral("GBPaletteViewer")) {
        setWindowTitle(tr("Palette Viewer"));

        QVBoxLayout* top = new QVBoxLayout(this);
        top->addWidget(new QLabel(tr("Click on a color for more information"), this));

        cv = new ColorView(this);
        bpv = new PixViewEvt(this);
        bpv->InitBMP(4, 8, cv);
        bpv->setFixedSize(4 * 20, 8 * 20);
        spv = new PixViewEvt(this);
        spv->InitBMP(4, 8, cv);
        spv->setFixedSize(4 * 20, 8 * 20);
        connect(bpv, &PixView::gfxClick, this, [this](int, int) { SelBG(); });
        connect(spv, &PixView::gfxClick, this, [this](int, int) { SelSprite(); });

        QHBoxLayout* pals = new QHBoxLayout();
        QGroupBox* bgbox = new QGroupBox(tr("Background"), this);
        QVBoxLayout* bgl = new QVBoxLayout(bgbox);
        bgl->addWidget(bpv, 0, Qt::AlignHCenter);
        QPushButton* save_bg = new QPushButton(tr("Save &BG..."), bgbox);
        connect(save_bg, &QPushButton::clicked, this,
                [this] { savepal(this, colbmp, 4 * 8, QStringLiteral("bg")); });
        bgl->addWidget(save_bg);
        pals->addWidget(bgbox);
        QGroupBox* spbox = new QGroupBox(tr("Sprite"), this);
        QVBoxLayout* spl = new QVBoxLayout(spbox);
        spl->addWidget(spv, 0, Qt::AlignHCenter);
        QPushButton* save_obj = new QPushButton(tr("Save &Sprite..."), spbox);
        connect(save_obj, &QPushButton::clicked, this,
                [this] { savepal(this, colbmp + 4 * 8 * 3, 4 * 8, QStringLiteral("obj")); });
        spl->addWidget(save_obj);
        pals->addWidget(spbox);
        top->addLayout(pals);

        QHBoxLayout* info_row = new QHBoxLayout();
        QGridLayout* info = new QGridLayout();
        info->addWidget(new QLabel(tr("Index:"), this), 0, 0);
        idx = NewValueLabel(this, QStringLiteral("3W"));
        info->addWidget(idx, 0, 1);
        info->addWidget(new QLabel(tr("Value:"), this), 1, 0);
        val = NewValueLabel(this, QStringLiteral("0xWWWW"));
        info->addWidget(val, 1, 1);
        info_row->addLayout(info);
        info_row->addWidget(cv);
        top->addLayout(info_row);

        top->addWidget(NewAutoUpdateCheckBox());

        QHBoxLayout* buttons = new QHBoxLayout();
        buttons->addWidget(NewRefreshButton());
        buttons->addStretch();
        buttons->addWidget(NewCloseButton());
        top->addLayout(buttons);

        Fit();
        Update();
    }

    void Update() override {
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

private:
    void SelBG() {
        spv->SetSel(-1, -1, false);
        ShowSel();
    }

    void SelSprite() {
        bpv->SetSel(-1, -1, false);
        ShowSel();
    }

    void ShowSel() {
        int x, y;
        bool isbg = true;
        bpv->GetSel(x, y);

        if (x < 0) {
            isbg = false;
            spv->GetSel(x, y);

            if (x < 0) {
                idx->setText(QString());
                val->setText(QString());
                return;
            }
        }

        uint8_t* pix = &colbmp[(x + y * 4) * 3];

        if (isbg)
            pix += 4 * 8 * 3;

        uint16_t v = (pix[0] >> 3) + ((pix[1] >> 3) << 5) + ((pix[2] >> 3) << 10);
        val->setText(QStringLiteral("0x") + Hex(v, 4));
        idx->setText(QString::number(x + y * 4));
    }

    ColorView* cv;
    PixViewEvt *bpv, *spv;
    uint8_t colbmp[4 * 8 * 3 * 2];
    QLabel *idx, *val;
};

// ---------------------------------------------------------------------------
// Tile viewers

// Slider with a value label next to it (widgets::AttachSliderValueLabel).
QLayout* SliderWithLabel(QSlider* slider, QWidget* parent) {
    QHBoxLayout* l = new QHBoxLayout();
    l->addWidget(slider, 1);
    QLabel* lab = new QLabel(QString::number(slider->value()), parent);
    lab->setMinimumWidth(lab->fontMetrics().horizontalAdvance(QStringLiteral("99")));
    QObject::connect(slider, &QSlider::valueChanged, lab,
                     [lab](int v) { lab->setText(QString::number(v)); });
    l->addWidget(lab);
    return l;
}

class TileViewer final : public GfxViewer {
public:
    explicit TileViewer(QWidget* parent)
        : GfxViewer(parent, QStringLiteral("TileViewer"), 32 * 8, 32 * 8) {
        setWindowTitle(tr("Tile Viewer"));
        is256_ = charbase_ = 0;

        QRadioButton* c16 = new QRadioButton(QStringLiteral("1&6"), this);
        QRadioButton* c256 = new QRadioButton(QStringLiteral("&256"), this);
        BindRadio(c16, &is256_, 0);
        BindRadio(c256, &is256_, 1);
        QRadioButton* cb0 = new QRadioButton(QStringLiteral("0x600&0000"), this);
        QRadioButton* cb1 = new QRadioButton(QStringLiteral("0x600&4000"), this);
        QRadioButton* cb2 = new QRadioButton(QStringLiteral("0x600&8000"), this);
        QRadioButton* cb3 = new QRadioButton(QStringLiteral("0x600&C000"), this);
        QRadioButton* cb4 = new QRadioButton(QStringLiteral("0x60&10000"), this);
        BindRadio(cb0, &charbase_, 0);
        BindRadio(cb1, &charbase_, 0x4000);
        BindRadio(cb2, &charbase_, 0x8000);
        BindRadio(cb3, &charbase_, 0xc000);
        BindRadio(cb4, &charbase_, 0x10000);

        QVBoxLayout* top = new QVBoxLayout(this);
        QHBoxLayout* main = new QHBoxLayout();

        QVBoxLayout* left = new QVBoxLayout();
        left->addWidget(NewRadioGroup(this, tr("Colors"), {c16, c256}));
        left->addWidget(NewRadioGroup(this, tr("Char Base"), {cb0, cb1, cb2, cb3, cb4}));
        left->addWidget(new QLabel(tr("Palette:"), this));
        QSlider* pal_slider = new QSlider(Qt::Horizontal, this);
        pal_slider->setRange(0, 15);
        BindSlider(pal_slider, &palette_);
        left->addLayout(SliderWithLabel(pal_slider, this));
        left->addWidget(str_);
        left->addWidget(auto_update_);
        left->addStretch();
        main->addLayout(left);

        QVBoxLayout* mid = new QVBoxLayout();
        QGridLayout* info = new QGridLayout();
        info->addWidget(new QLabel(tr("Tile:"), this), 0, 0);
        tileno_ = NewValueLabel(this, QStringLiteral("1WWW"));
        info->addWidget(tileno_, 0, 1);
        info->addWidget(new QLabel(tr("Address:"), this), 1, 0);
        addr_ = NewValueLabel(this, QStringLiteral("06WWWWWW"));
        info->addWidget(addr_, 1, 1);
        mid->addLayout(info);
        mid->addLayout(ZoomBlock(zoom_, cv_));
        main->addLayout(mid);

        gvs_->setMinimumSize(256 + 20, 256 + 20);
        main->addWidget(gvs_, 1);
        top->addLayout(main, 1);
        top->addLayout(GfxButtonRow(refresh_, save_, close_));

        connect(gv, &GfxPanel::gfxClick, this, &TileViewer::UpdateMouseInfoEv);

        selx_ = sely_ = -1;
        Fit();
        Update();
    }

    void Update() override {
        // Following copied almost verbatim from TileView.cpp
        uint16_t* palette = (uint16_t*)g_paletteRAM;
        uint8_t* charBase = &g_vram[charbase_];
        int maxY;

        if (is256_) {
            int tile = 0;
            maxY = 16;

            for (int y = 0; y < maxY; y++) {
                for (int x = 0; x < 32; x++) {
                    if (charbase_ == 4 * 0x4000)
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

            if (charbase_ == 3 * 0x4000)
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

private:
    void UpdateMouseInfoEv(int x, int y) {
        selx_ = x;
        sely_ = y;
        UpdateMouseInfo();
    }

    void UpdateMouseInfo() {
        if (selx_ > gv->bmw || sely_ > gv->bmh)
            selx_ = sely_ = -1;

        if (selx_ < 0) {
            addr_->setText(QString());
            tileno_->setText(QString());
        } else {
            int x = selx_ / 8;
            int y = sely_ / 8;
            int t = 32 * y + x;

            if (is256_)
                t *= 2;

            tileno_->setText(QString::number(t));
            addr_->setText(Hex(0x6000000 + charbase_ + 32 * t, 8));
        }
    }

    // following 2 functions copied almost verbatim from TileView.cpp
    void render256(int tile, int x, int y, uint8_t* charBase, uint16_t* palette) {
        uint8_t* bmp = &ImageData()[24 * x + 8 * 32 * 24 * y];

        for (int j = 0; j < 8; j++) {
            for (int i = 0; i < 8; i++) {
                uint8_t c = charBase[tile * 64 + j * 8 + i];
                uint16_t color = palette[c];
                *bmp++ = (color & 0x1f) << 3;
                *bmp++ = ((color >> 5) & 0x1f) << 3;
                *bmp++ = ((color >> 10) & 0x1f) << 3;
            }

            bmp += 31 * 24;  // advance line
        }
    }

    void render16(int tile, int x, int y, uint8_t* charBase, uint16_t* palette) {
        uint8_t* bmp = &ImageData()[24 * x + 8 * 32 * 24 * y];
        int pal = this->palette_;

        if (this->charbase_ == 4 * 0x4000)
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

            bmp += 31 * 24;  // advance line
        }
    }

    int charbase_ = 0;
    int is256_ = 0;
    int palette_ = 0;
    QLabel *tileno_, *addr_;
    int selx_, sely_;
};

class GBTileViewer final : public GfxViewer {
public:
    explicit GBTileViewer(QWidget* parent)
        : GfxViewer(parent, QStringLiteral("GBTileViewer"), 16 * 8, 16 * 8) {
        setWindowTitle(tr("Tile Viewer"));
        bank = charbase = palette = 0;

        QRadioButton* b0 = new QRadioButton(QStringLiteral("&0"), this);
        QRadioButton* b1 = new QRadioButton(QStringLiteral("&1"), this);
        BindRadio(b0, &bank, 0);
        BindRadio(b1, &bank, 0x2000);
        QRadioButton* cb0 = new QRadioButton(QStringLiteral("0x&8000"), this);
        QRadioButton* cb1 = new QRadioButton(QStringLiteral("0x8&800"), this);
        BindRadio(cb0, &charbase, 0);
        BindRadio(cb1, &charbase, 0x800);

        QVBoxLayout* top = new QVBoxLayout(this);
        QHBoxLayout* main = new QHBoxLayout();

        QVBoxLayout* left = new QVBoxLayout();
        left->addWidget(NewRadioGroup(this, tr("VRAM Bank"), {b0, b1}));
        left->addWidget(NewRadioGroup(this, tr("Char Base"), {cb0, cb1}));
        left->addWidget(new QLabel(tr("Palette:"), this));
        QSlider* pal_slider = new QSlider(Qt::Horizontal, this);
        pal_slider->setRange(0, 7);
        BindSlider(pal_slider, &palette);
        left->addLayout(SliderWithLabel(pal_slider, this));
        left->addWidget(str_);
        left->addWidget(auto_update_);
        left->addStretch();
        main->addLayout(left);

        QVBoxLayout* mid = new QVBoxLayout();
        QGridLayout* info = new QGridLayout();
        info->addWidget(new QLabel(tr("Tile:"), this), 0, 0);
        tileno = NewValueLabel(this, QStringLiteral("2WW"));
        info->addWidget(tileno, 0, 1);
        info->addWidget(new QLabel(tr("Address:"), this), 1, 0);
        addr = NewValueLabel(this, QStringLiteral("WWWW"));
        info->addWidget(addr, 1, 1);
        mid->addLayout(info);
        mid->addLayout(ZoomBlock(zoom_, cv_));
        main->addLayout(mid);

        gvs_->setMinimumSize(128 + 20, 128 + 20);
        main->addWidget(gvs_, 1);
        top->addLayout(main, 1);
        top->addLayout(GfxButtonRow(refresh_, save_, close_));

        connect(gv, &GfxPanel::gfxClick, this, &GBTileViewer::UpdateMouseInfoEv);

        selx = sely = -1;
        Fit();
        Update();
    }

    void Update() override {
        // following copied almost verbatim from GBTileView.cpp
        uint8_t* charBase =
            (gbVram != nullptr) ? &gbVram[bank + charbase] : &gbMemory[0x8000 + charbase];
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

private:
    void UpdateMouseInfoEv(int x, int y) {
        selx = x;
        sely = y;
        UpdateMouseInfo();
    }

    void UpdateMouseInfo() {
        if (selx > gv->bmw || sely > gv->bmh)
            selx = sely = -1;

        if (selx < 0) {
            addr->setText(QString());
            tileno->setText(QString());
        } else {
            int x = selx / 8;
            int y = sely / 8;
            int t = 16 * y + x;
            tileno->setText(QString::number(t));
            addr->setText(Hex(0x8000 + charbase + 16 * t, 4));
        }
    }

    // following function copied almost verbatim from GBTileView.cpp
    void render(int tile, int x, int y, uint8_t* charBase) {
        uint8_t* bmp = &ImageData()[24 * x + 8 * 16 * 24 * y];

        for (int j = 0; j < 8; j++) {
            uint8_t mask = 0x80;
            uint8_t tile_a = charBase[tile * 16 + j * 2];
            uint8_t tile_b = charBase[tile * 16 + j * 2 + 1];

            for (int i = 0; i < 8; i++) {
                uint8_t c = (tile_a & mask) ? 1 : 0;
                c += ((tile_b & mask) ? 2 : 0);

                uint16_t color = 0;
                if (gbCgbMode) {
                    int pal_idx = c + palette * 4;
                    if (pal_idx >= 0 && pal_idx < 64)  // CGB palettes: 8 palettes * 4 colors
                        color = gbPalette[pal_idx];
                    else
                        color = 0;  // fallback to black
                } else {
                    int pal_idx = gbBgp[c];
                    if (pal_idx >= 0 && pal_idx < 4)  // DMG palettes: 4 colors
                        color = gbPalette[pal_idx];
                    else
                        color = 0;  // fallback to black
                }

                *bmp++ = (color & 0x1f) << 3;
                *bmp++ = ((color >> 5) & 0x1f) << 3;
                *bmp++ = ((color >> 10) & 0x1f) << 3;
                mask >>= 1;
            }

            bmp += 15 * 24;  // advance line
        }
    }

    int bank, charbase, palette;
    QLabel *addr, *tileno;
    int selx, sely;
};

}  // namespace

Viewer* NewMapViewer(QWidget* parent) {
    return new MapViewer(parent);
}

Viewer* NewGBMapViewer(QWidget* parent) {
    return new GBMapViewer(parent);
}

Viewer* NewOAMViewer(QWidget* parent) {
    return new OAMViewer(parent);
}

Viewer* NewGBOAMViewer(QWidget* parent) {
    return new GBOAMViewer(parent);
}

Viewer* NewPaletteViewer(QWidget* parent) {
    return new PaletteViewer(parent);
}

Viewer* NewGBPaletteViewer(QWidget* parent) {
    return new GBPaletteViewer(parent);
}

Viewer* NewTileViewer(QWidget* parent) {
    return new TileViewer(parent);
}

Viewer* NewGBTileViewer(QWidget* parent) {
    return new GBTileViewer(parent);
}

}  // namespace viewers
