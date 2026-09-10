// These are all the viewer dialogs except for the ones with graphical areas.
// They can be instantiated multiple times. Port of the wx port's viewers.cpp.

#include <limits>

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>

#include "core/base/port.h"
#include "core/gb/gb.h"
#include "core/gb/gbDis.h"
#include "core/gb/gbGlobals.h"
#include "core/gba/gba.h"
#include "core/gba/gbaCpu.h"
#include "core/gba/gbaCpuArmDis.h"
#include "core/gba/gbaGlobals.h"
#include "qt/app.h"
#include "qt/config/option-proxy.h"
#include "qt/game-area.h"
#include "qt/log.h"
#include "qt/main-window.h"
#include "qt/viewers/viewer.h"

namespace viewers {

namespace {

// A read-only value label with a fixed-width font, sized for `sample`.
QLabel* NewValueLabel(QWidget* parent, const QString& sample) {
    QLabel* lab = new QLabel(sample, parent);
    lab->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    lab->setMinimumWidth(lab->sizeHint().width());
    lab->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return lab;
}

// ---------------------------------------------------------------------------
// GBA disassembler

class DisassembleViewer final : public Viewer {
public:
    explicit DisassembleViewer(QWidget* parent) : Viewer(parent, QStringLiteral("Disassemble")) {
        setWindowTitle(tr("Disassemble"));

        QVBoxLayout* top = new QVBoxLayout(this);
        QHBoxLayout* main = new QHBoxLayout();

        // Left column: mode radios + goto + listing.
        QVBoxLayout* left = new QVBoxLayout();
        QHBoxLayout* modes = new QHBoxLayout();
        QRadioButton* ins_auto = new QRadioButton(tr("&Automatic"), this);
        QRadioButton* ins_arm = new QRadioButton(QStringLiteral("A&RM"), this);
        QRadioButton* ins_thumb = new QRadioButton(QStringLiteral("&THUMB"), this);
        ins_auto->setChecked(true);
        connect(ins_auto, &QRadioButton::toggled, this, [this](bool c) {
            if (c) SetMode(DisassemblyMode::Automatic);
        });
        connect(ins_arm, &QRadioButton::toggled, this, [this](bool c) {
            if (c) SetMode(DisassemblyMode::Arm);
        });
        connect(ins_thumb, &QRadioButton::toggled, this, [this](bool c) {
            if (c) SetMode(DisassemblyMode::Thumb);
        });
        modes->addWidget(ins_auto);
        modes->addWidget(ins_arm);
        modes->addWidget(ins_thumb);
        goto_addr = NewHexEdit(this, 8);
        modes->addWidget(goto_addr, 1);
        QPushButton* go = new QPushButton(tr("&Go"), this);
        connect(go, &QPushButton::clicked, this, &DisassembleViewer::Goto);
        connect(goto_addr, &QLineEdit::returnPressed, this, &DisassembleViewer::Goto);
        modes->addWidget(go);
        left->addLayout(modes);

        dis = new DisList(this);
        dis->refill_callback = [this] { RefillList(); };
        left->addWidget(dis, 1);
        main->addLayout(left, 1);

        // Right column: registers, flags, mode.
        QVBoxLayout* right = new QVBoxLayout();
        QGridLayout* regs = new QGridLayout();
        static const char* const kRegNames[17] = {"R0:",  "R1:",  "R2:",  "R3:", "R4:",  "R5:",
                                                  "R6:",  "R7:",  "R8:",  "R9:", "R10:", "R11:",
                                                  "R12:", "SP:",  "LR:",  "PC:", "CPSR:"};
        for (int i = 0; i < 17; i++) {
            regs->addWidget(new QLabel(QString::fromLatin1(kRegNames[i]), this), i, 0);
            regv[i] = NewValueLabel(this, QStringLiteral("00000000"));
            regs->addWidget(regv[i], i, 1);
        }
        right->addLayout(regs);

        QGridLayout* flags = new QGridLayout();
        N = new DispCheckBox(QStringLiteral("N"), this);
        I = new DispCheckBox(QStringLiteral("I"), this);
        Z = new DispCheckBox(QStringLiteral("Z"), this);
        F = new DispCheckBox(QStringLiteral("F"), this);
        C = new DispCheckBox(QStringLiteral("C"), this);
        T = new DispCheckBox(QStringLiteral("T"), this);
        V = new DispCheckBox(QStringLiteral("V"), this);
        flags->addWidget(N, 0, 0);
        flags->addWidget(I, 0, 1);
        flags->addWidget(Z, 1, 0);
        flags->addWidget(F, 1, 1);
        flags->addWidget(C, 2, 0);
        flags->addWidget(T, 2, 1);
        flags->addWidget(V, 3, 0);
        right->addLayout(flags);

        QHBoxLayout* mode_row = new QHBoxLayout();
        mode_row->addWidget(new QLabel(tr("Mode:"), this));
        Modev = NewValueLabel(this, QStringLiteral("00"));
        mode_row->addWidget(Modev);
        mode_row->addStretch();
        right->addLayout(mode_row);
        right->addStretch();
        main->addLayout(right);
        top->addLayout(main, 1);

        QHBoxLayout* bottom = new QHBoxLayout();
        bottom->addWidget(NewAutoUpdateCheckBox());
        bottom->addStretch();
        QPushButton* goto_pc = new QPushButton(tr("G&oto PC"), this);
        connect(goto_pc, &QPushButton::clicked, this, [this] { GotoPC(); });
        bottom->addWidget(goto_pc);
        bottom->addWidget(NewRefreshButton(tr("Re&fresh")));
        QPushButton* next = new QPushButton(tr("&Next"), this);
        connect(next, &QPushButton::clicked, this, [this] {
            CPULoop(1);
            GotoPC();
        });
        bottom->addWidget(next);
        bottom->addWidget(NewCloseButton());
        top->addLayout(bottom);

        // refit listing for longest line
        dis->Refit(70);
        dis->maxaddr = static_cast<uint32_t>(~0);
        disassembly_mode_ = DisassemblyMode::Automatic;
        Fit();
        goto_addr->setFocus();
        GotoPC();
    }

    void Update() override { GotoPC(); }

private:
    enum class DisassemblyMode {
        Automatic,
        Arm,
        Thumb,
    };

    void SetMode(DisassemblyMode mode) {
        disassembly_mode_ = mode;
        RefillList();
    }

    void Goto() {
        const QString as = goto_addr->text();
        if (as.isEmpty())
            return;

        bool ok = false;
        const uint32_t a = as.toUInt(&ok, 16);
        if (!ok)
            return;
        dis->SetSel(a);
        UpdateDis();
    }

    void GotoPC() {
        dis->SetSel(armNextPC);
        UpdateDis();
    }

    void UpdateDis() {
        N->setChecked(reg[16].I & 0x80000000);
        Z->setChecked(reg[16].I & 0x40000000);
        C->setChecked(reg[16].I & 0x20000000);
        V->setChecked(reg[16].I & 0x10000000);
        I->setChecked(reg[16].I & 0x00000080);
        F->setChecked(reg[16].I & 0x00000040);
        T->setChecked(reg[16].I & 0x00000020);
        Modev->setText(Hex(reg[16].I & 0x1f, 2));

        for (int i = 0; i < 17; i++) {
            regv[i]->setText(Hex(reg[i].I, 8));
        }
    }

    void RefillList() {
        // examination of disArm shows that max len is 69 chars
        // (e.g. 0x081cb6db), and I assume disThumb is shorter
        char buf[4096];
        dis->strings.clear();
        dis->addrs.clear();
        uint32_t addr = dis->topaddr;
        const bool arm_mode = disassembly_mode_ == DisassemblyMode::Arm ||
                              (armState && disassembly_mode_ == DisassemblyMode::Automatic);
        dis->back_size = arm_mode ? 4 : 2;

        for (int i = 0; i < dis->nlines; i++) {
            dis->addrs.push_back(addr);

            if (arm_mode)
                addr += disArm(addr, buf, sizeof(buf), DIS_VIEW_CODE | DIS_VIEW_ADDRESS);
            else
                addr += disThumb(addr, buf, sizeof(buf), DIS_VIEW_CODE | DIS_VIEW_ADDRESS);

            dis->strings.push_back(QString::fromLatin1(buf));
        }

        dis->Refill();
    }

    DisList* dis;
    QLineEdit* goto_addr;
    QCheckBox *N, *Z, *C, *V, *I, *F, *T;
    DisassemblyMode disassembly_mode_;
    QLabel *regv[17], *Modev;
};

// ---------------------------------------------------------------------------
// GB disassembler

class GBDisassembleViewer final : public Viewer {
public:
    explicit GBDisassembleViewer(QWidget* parent)
        : Viewer(parent, QStringLiteral("GBDisassemble")) {
        setWindowTitle(tr("Disassemble"));

        QVBoxLayout* top = new QVBoxLayout(this);
        QHBoxLayout* main = new QHBoxLayout();

        QVBoxLayout* left = new QVBoxLayout();
        QHBoxLayout* goto_row = new QHBoxLayout();
        goto_addr = NewHexEdit(this, 4);
        goto_row->addWidget(goto_addr, 1);
        QPushButton* go = new QPushButton(tr("&Go"), this);
        connect(go, &QPushButton::clicked, this, &GBDisassembleViewer::Goto);
        connect(goto_addr, &QLineEdit::returnPressed, this, &GBDisassembleViewer::Goto);
        goto_row->addWidget(go);
        left->addLayout(goto_row);
        dis = new DisList(this);
        dis->refill_callback = [this] { RefillList(); };
        left->addWidget(dis, 1);
        main->addLayout(left, 1);

        QVBoxLayout* right = new QVBoxLayout();
        QGridLayout* regs = new QGridLayout();
        int row = 0;
        auto add_reg = [&](const char* name, QLabel*& lab, const QString& sample) {
            regs->addWidget(new QLabel(QString::fromLatin1(name), this), row, 0);
            lab = NewValueLabel(this, sample);
            regs->addWidget(lab, row, 1);
            row++;
        };
        add_reg("AF:", AFv, QStringLiteral("0000"));
        add_reg("BC:", BCv, QStringLiteral("0000"));
        add_reg("DE:", DEv, QStringLiteral("0000"));
        add_reg("HL:", HLv, QStringLiteral("0000"));
        add_reg("SP:", SPv, QStringLiteral("0000"));
        add_reg("PC:", PCv, QStringLiteral("0000"));
        add_reg("IFF:", IFFv, QStringLiteral("0000"));
        add_reg("LY:", LYv, QStringLiteral("00"));
        right->addLayout(regs);

        QGridLayout* flags = new QGridLayout();
        Z = new DispCheckBox(QStringLiteral("Z"), this);
        N = new DispCheckBox(QStringLiteral("N"), this);
        H = new DispCheckBox(QStringLiteral("H"), this);
        C = new DispCheckBox(QStringLiteral("C"), this);
        flags->addWidget(Z, 0, 0);
        flags->addWidget(N, 0, 1);
        flags->addWidget(H, 1, 0);
        flags->addWidget(C, 1, 1);
        right->addLayout(flags);
        right->addStretch();
        main->addLayout(right);
        top->addLayout(main, 1);

        QHBoxLayout* bottom = new QHBoxLayout();
        bottom->addWidget(NewAutoUpdateCheckBox());
        bottom->addStretch();
        QPushButton* goto_pc = new QPushButton(tr("G&oto PC"), this);
        connect(goto_pc, &QPushButton::clicked, this, [this] { GotoPC(); });
        bottom->addWidget(goto_pc);
        bottom->addWidget(NewRefreshButton(tr("Re&fresh")));
        QPushButton* next = new QPushButton(tr("&Next"), this);
        connect(next, &QPushButton::clicked, this, [this] {
            gbEmulate(1);
            GotoPC();
        });
        bottom->addWidget(next);
        bottom->addWidget(NewCloseButton());
        top->addLayout(bottom);

        // refit listing for longest line
        dis->Refit(26);
        dis->maxaddr = static_cast<uint32_t>(~0);
        Fit();
        goto_addr->setFocus();
        GotoPC();
    }

    void Update() override { GotoPC(); }

private:
    void Goto() {
        const QString as = goto_addr->text();
        if (as.isEmpty())
            return;

        bool ok = false;
        const uint32_t a = as.toUInt(&ok, 16);
        if (!ok)
            return;
        dis->SetSel(a);
        UpdateDis();
    }

    void GotoPC() {
        dis->SetSel(PC.W);
        UpdateDis();
    }

    void UpdateDis() {
        Z->setChecked(AF.B.B0 & GB_Z_FLAG);
        N->setChecked(AF.B.B0 & GB_N_FLAG);
        H->setChecked(AF.B.B0 & GB_H_FLAG);
        C->setChecked(AF.B.B0 & GB_C_FLAG);
        AFv->setText(Hex(AF.W, 4));
        BCv->setText(Hex(BC.W, 4));
        DEv->setText(Hex(DE.W, 4));
        HLv->setText(Hex(HL.W, 4));
        SPv->setText(Hex(SP.W, 4));
        PCv->setText(Hex(PC.W, 4));
        IFFv->setText(Hex(IFF, 4));
        LYv->setText(Hex(register_LY, 2));
    }

    void RefillList() {
        // examination of gbDis shows that max len is 26 chars (e.g. 0xe2)
        char buf[30];
        uint16_t addr = dis->topaddr;
        dis->strings.clear();
        dis->addrs.clear();
        dis->back_size = 1;

        for (int i = 0; i < dis->nlines; i++) {
            dis->addrs.push_back(addr);
            addr += gbDis(buf, sizeof(buf), addr);
            dis->strings.push_back(QString::fromLatin1(buf));
        }

        dis->Refill();
    }

    DisList* dis;
    QLineEdit* goto_addr;
    QLabel *AFv, *BCv, *DEv, *HLv, *SPv, *PCv, *LYv, *IFFv;
    QCheckBox *Z, *N, *H, *C;
};

}  // namespace

Viewer* NewDisassembleViewer(QWidget* parent) {
    return new DisassembleViewer(parent);
}

Viewer* NewGBDisassembleViewer(QWidget* parent) {
    return new GBDisassembleViewer(parent);
}

}  // namespace viewers

// for CPUWriteHalfWord and CPURead... below
#include "core/gba/gbaInline.h"

namespace viewers {

#include "qt/viewers/ioregs.h"

namespace {

// ---------------------------------------------------------------------------
// I/O viewer

class IOViewer final : public Viewer {
public:
    explicit IOViewer(QWidget* parent) : Viewer(parent, QStringLiteral("IOViewer")) {
        setWindowTitle(tr("IO Viewer"));

        QVBoxLayout* top = new QVBoxLayout(this);
        addr_ = new QComboBox(this);
        top->addWidget(addr_);

        QHBoxLayout* val_row = new QHBoxLayout();
        val_row->addWidget(new QLabel(tr("Value:"), this));
        val = NewValueLabel(this, QStringLiteral("0000"));
        val_row->addWidget(val);
        val_row->addStretch();
        top->addLayout(val_row);

        QGridLayout* grid = new QGridLayout();
        for (int i = 15; i >= 0; i--) {
            const int row = 15 - i;
            bit[i] = new QCheckBox(this);
            connect(bit[i], &QCheckBox::clicked, this, [this] { CheckBit(); });
            grid->addWidget(bit[i], row, 0);
            grid->addWidget(new QLabel(QStringLiteral("%1 ").arg(i), this), row, 1);
            bitlab[i] = new QLabel(this);
            grid->addWidget(bitlab[i], row, 2);
        }
        grid->setColumnStretch(2, 1);
        top->addLayout(grid);

        top->addWidget(NewAutoUpdateCheckBox());

        QHBoxLayout* buttons = new QHBoxLayout();
        buttons->addWidget(NewRefreshButton());
        buttons->addStretch();
        QPushButton* apply = new QPushButton(tr("&Apply"), this);
        connect(apply, &QPushButton::clicked, this, [this] { Apply(); });
        buttons->addWidget(apply);
        buttons->addWidget(NewCloseButton());
        top->addLayout(buttons);

        // Translate all the strings once and find the longest label so the
        // dialog does not resize when the register changes.
        QString longline;
        int lwidth = 0;
        const QFontMetrics fm(bitlab[0]->font());
        for (size_t i = 0; i < NUM_IOREGS; i++) {
            addr_->addItem(QCoreApplication::translate("IOViewer", ioregs[i].name));

            for (int j = 0; j < 16; j++) {
                if (ioregs[i].bits[j][0]) {
                    const QString s = QCoreApplication::translate("IOViewer", ioregs[i].bits[j]);
                    const int w = fm.horizontalAdvance(s);
                    if (w > lwidth) {
                        lwidth = w;
                        longline = s;
                    }
                }
            }
        }

        for (int j = 0; j < 16; j++)
            bitlab[j]->setMinimumWidth(lwidth);

        connect(addr_, qOverload<int>(&QComboBox::currentIndexChanged), this,
                [this](int sel) {
                    if (sel >= 0)
                        Select(sel);
                });
        Fit();
        addr_->setCurrentIndex(0);
        Select(0);
    }

    void Update() override { Update(addr_->currentIndex()); }

private:
    void Select(int sel) {
        int i;
        uint16_t mask;

        for (mask = 1, i = 0; mask; mask <<= 1, i++) {
            bit[i]->setEnabled(mask & ioregs[sel].write);
            bitlab[i]->setText(ioregs[sel].bits[i][0]
                                   ? QCoreApplication::translate("IOViewer", ioregs[sel].bits[i])
                                   : QString());
        }

        Update(sel);
    }

    void Update(int sel) {
        if (sel < 0)
            return;
        uint16_t* addr = ioregs[sel].address
                             ? ioregs[sel].address
                             : reinterpret_cast<uint16_t*>(&g_ioMem[ioregs[sel].offset]);
        uint16_t mask, _reg = *addr;
        int i;

        for (mask = 1, i = 0; mask; mask <<= 1, i++)
            bit[i]->setChecked(mask & _reg);

        val->setText(Hex(_reg, 4));
    }

    void CheckBit() {
        // it'd be faster to store the value and just flip the bit, but it's
        // easier this way
        uint16_t mask, _reg = 0;
        int j;

        for (mask = 1, j = 0; mask; mask <<= 1, j++)
            if (bit[j]->isChecked())
                _reg |= mask;

        val->setText(Hex(_reg, 4));

        // When auto-update is enabled, the next UpdateViewers tick (every
        // emulated frame) calls Update() which re-reads the current value of
        // the IO register and resets every checkbox to match. That happens
        // before the user can click Apply, wiping their edit. Commit the
        // change here so the bit toggle takes effect immediately and survives
        // the next refresh. When auto-update is off, keep the legacy
        // preview-then-Apply flow.
        if (auto_update)
            Apply();
    }

    void Apply() {
        const int sel = addr_->currentIndex();
        if (sel < 0)
            return;
        uint16_t* addr = ioregs[sel].address
                             ? ioregs[sel].address
                             : reinterpret_cast<uint16_t*>(&g_ioMem[ioregs[sel].offset]);
        uint16_t mask, _reg = *addr;
        _reg &= ~ioregs[sel].write;
        int i;

        for (mask = 1, i = 0; mask; mask <<= 1, i++) {
            if ((mask & ioregs[sel].write) && bit[i]->isChecked())
                _reg |= mask;
        }

        CPUWriteHalfWord(0x4000000 + ioregs[sel].offset, _reg);
        Update(sel);
    }

    QComboBox* addr_;
    QLabel* val;
    QCheckBox* bit[16];
    QLabel* bitlab[16];
};

}  // namespace

Viewer* NewIOViewer(QWidget* parent) {
    return new IOViewer(parent);
}

}  // namespace viewers

// These are what mfc interface used.  Maybe it would be safer
// to avoid the Quick() routines in favor of the long ones...
#define CPUWriteByteQuick(addr, b) \
    ::map[(addr) >> 24].address[(addr) & ::map[(addr) >> 24].mask] = (b)
#define CPUWriteHalfWordQuick(addr, b) \
    WRITE16LE((uint16_t*)&::map[(addr) >> 24].address[(addr) & ::map[(addr) >> 24].mask], b)
#define CPUWriteMemoryQuick(addr, b) \
    WRITE32LE((uint32_t*)&::map[(addr) >> 24].address[(addr) & ::map[(addr) >> 24].mask], b)
#define GBWriteByteQuick(addr, b) \
    *((uint8_t*)&gbMemoryMap[(addr) >> 12][(addr)&0xfff]) = (b)
#define GBWriteHalfWordQuick(addr, b) \
    WRITE16LE((uint16_t*)&gbMemoryMap[(addr) >> 12][(addr)&0xfff], b)
#define GBWriteMemoryQuick(addr, b) \
    WRITE32LE((uint32_t*)&gbMemoryMap[(addr) >> 12][(addr)&0xfff], b)
#define GBReadMemoryQuick(addr) \
    READ32LE((uint32_t*)&gbMemoryMap[(addr) >> 12][(addr)&0xfff])

namespace viewers {

namespace {

QString memsave_dir;

// ---------------------------------------------------------------------------
// Memory viewers

class MemViewerBase : public Viewer {
public:
    void Update() override {}

protected:
    MemViewerBase(QWidget* parent, uint32_t max) : Viewer(parent, QStringLiteral("MemViewer")) {
        setWindowTitle(tr("Memory Viewer"));

        QVBoxLayout* top = new QVBoxLayout(this);
        QHBoxLayout* ctrls = new QHBoxLayout();
        bs = new QComboBox(this);
        bs->addItem(QString());
        connect(bs, qOverload<int>(&QComboBox::activated), this, [this](int) { BlockStart(); });
        ctrls->addWidget(bs);

        mv = new MemView(this);
        mv->fmt = max > 0xffff ? 2 : 1;
        mv->maxaddr = max;
        mv->refill_callback = [this] { Update(); };
        mv->write_callback = [this] { WriteVal(); };

        QRadioButton* fmt8 = new QRadioButton(tr("&8-bit"), this);
        QRadioButton* fmt16 = new QRadioButton(tr("&16-bit"), this);
        QRadioButton* fmt32 = new QRadioButton(tr("&32-bit"), this);
        BindRadio(fmt8, &mv->fmt, 0);
        BindRadio(fmt16, &mv->fmt, 1);
        BindRadio(fmt32, &mv->fmt, 2);
        ctrls->addWidget(fmt8);
        ctrls->addWidget(fmt16);
        ctrls->addWidget(fmt32);

        addrlen = max > 0xffff ? 8 : 4;
        goto_addr = NewHexEdit(this, addrlen);
        ctrls->addWidget(goto_addr, 1);
        QPushButton* go = new QPushButton(tr("&Go"), this);
        connect(go, &QPushButton::clicked, this, [this] { GotoEv(); });
        connect(goto_addr, &QLineEdit::returnPressed, this, [this] { GotoEv(); });
        ctrls->addWidget(go);
        top->addLayout(ctrls);

        top->addWidget(mv, 1);

        QHBoxLayout* status = new QHBoxLayout();
        status->addWidget(NewAutoUpdateCheckBox());
        status->addStretch();
        status->addWidget(new QLabel(tr("Current address:"), this));
        QLabel* addr = NewValueLabel(
            this, addrlen == 8 ? QStringLiteral("0xWWWWWWWW") : QStringLiteral("0xWWWW"));
        // don't let address display resize when window size changes
        addr->setMinimumWidth(addr->sizeHint().width());
        addr->setText(QString());
        status->addWidget(addr);
        top->addLayout(status);

        QHBoxLayout* buttons = new QHBoxLayout();
        buttons->addWidget(NewRefreshButton());
        QPushButton* load = new QPushButton(tr("&Load..."), this);
        connect(load, &QPushButton::clicked, this, [this] { Load(); });
        buttons->addWidget(load);
        QPushButton* save = new QPushButton(tr("&Save..."), this);
        connect(save, &QPushButton::clicked, this, [this] { Save(); });
        buttons->addWidget(save);
        buttons->addWidget(NewCloseButton());
        top->addLayout(buttons);

        // refit listing for longest line
        mv->Refit();
        mv->addrlab = addr;
        bs->setFocus();

        // initialize load/save support dialog already
        selregion = new QDialog(this);
        selregion->setWindowTitle(tr("Select memory region"));
        QVBoxLayout* sl = new QVBoxLayout(selregion);
        QFormLayout* form = new QFormLayout();
        selreg_addr = NewHexEdit(selregion, addrlen);
        form->addRow(tr("Address:"), selreg_addr);
        selreg_len = NewHexEdit(selregion, addrlen);
        selreg_lenlab = new QLabel(tr("Size:"), selregion);
        form->addRow(selreg_lenlab, selreg_len);
        sl->addLayout(form);
        QDialogButtonBox* box =
            new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, selregion);
        connect(box, &QDialogButtonBox::accepted, selregion, &QDialog::accept);
        connect(box, &QDialogButtonBox::rejected, selregion, &QDialog::reject);
        sl->addWidget(box);
    }

    void BlockStart() {
        bool ok = false;
        const QString s = bs->currentText().section(QLatin1Char(' '), 0, 0);
        const uint32_t l = s.toUInt(&ok, 0);
        if (ok)
            Goto(l);
    }

    void GotoEv() {
        const QString v = goto_addr->text();
        if (v.isEmpty())
            return;

        bool ok = false;
        const uint32_t l = v.toUInt(&ok, 16);
        if (ok)
            Goto(l);
    }

    void Goto(uint32_t addr) { mv->ShowAddr(addr, true); }

    virtual void WriteVal() = 0;

    int ShowSelRegion() {
        if (OPTION(kDispKeepOnTop))
            selregion->setWindowFlags(selregion->windowFlags() | Qt::WindowStaysOnTopHint);
        else
            selregion->setWindowFlags(selregion->windowFlags() & ~Qt::WindowStaysOnTopHint);
        return selregion->exec();
    }

    void Load() {
        if (memsave_fn.isEmpty())
            memsave_fn = vbamApp().frame->GetPanel()->game_name() + QStringLiteral(".dmp");

        const QString path = QFileDialog::getOpenFileName(
            this, tr("Select memory dump file"), QDir(memsave_dir).filePath(memsave_fn),
            tr("Memory dumps (*.dmp *.bin)") + QStringLiteral(";;") + tr("All files (*)"));

        if (path.isEmpty())
            return;

        memsave_fn = QFileInfo(path).fileName();
        memsave_dir = QFileInfo(path).absolutePath();

        const QFileInfo fi(path);
        if (!fi.isReadable()) {
            vbam::LogError(tr("Can't open file %1").arg(path));
            return;
        }

        const uint32_t len = static_cast<uint32_t>(fi.size());

        if (!len)
            return;

        selreg_addr->setText(Hex(mv->GetAddr(), addrlen));
        selreg_len->setEnabled(false);
        selreg_lenlab->setEnabled(false);
        selreg_len->setText(Hex(len, addrlen));

        if (ShowSelRegion() != QDialog::Accepted)
            return;

        bool ok = false;
        const uint32_t addr = selreg_addr->text().toUInt(&ok, 16);
        if (!ok)
            return;
        MemLoad(path, addr, len);
    }

    virtual void MemLoad(const QString& name, uint32_t addr, uint32_t len) = 0;

    void Save() {
        selreg_addr->setText(Hex(mv->GetAddr(), addrlen));
        selreg_len->setEnabled(true);
        selreg_lenlab->setEnabled(true);
        selreg_len->setText(QString());

        if (ShowSelRegion() != QDialog::Accepted)
            return;

        bool ok_addr = false, ok_len = false;
        const uint32_t addr = selreg_addr->text().toUInt(&ok_addr, 16);
        const uint32_t len = selreg_len->text().toUInt(&ok_len, 16);
        if (!ok_addr || !ok_len)
            return;

        if (memsave_fn.isEmpty())
            memsave_fn = vbamApp().frame->GetPanel()->game_name() + QStringLiteral(".dmp");

        const QString path = QFileDialog::getSaveFileName(
            this, tr("Select output file"), QDir(memsave_dir).filePath(memsave_fn),
            tr("Memory dumps (*.dmp *.bin)") + QStringLiteral(";;") + tr("All files (*)"));

        if (path.isEmpty())
            return;

        memsave_dir = QFileInfo(path).absolutePath();
        memsave_fn = QFileInfo(path).fileName();

        MemSave(path, addr, len);
    }

    virtual void MemSave(const QString& name, uint32_t addr, uint32_t len) = 0;

    int addrlen;
    QComboBox* bs;
    QLineEdit* goto_addr;
    MemView* mv;

    QDialog* selregion;
    QLineEdit *selreg_addr, *selreg_len;
    QLabel* selreg_lenlab;
    QString memsave_fn;
};

class MemViewer final : public MemViewerBase {
public:
    explicit MemViewer(QWidget* parent)
        : MemViewerBase(parent, std::numeric_limits<uint32_t>::max()) {
        bs->addItem(QStringLiteral("0x00000000 - BIOS"));
        bs->addItem(QStringLiteral("0x02000000 - WRAM"));
        bs->addItem(QStringLiteral("0x03000000 - IRAM"));
        bs->addItem(QStringLiteral("0x04000000 - I / O"));
        bs->addItem(QStringLiteral("0x05000000 - PALETTE"));
        bs->addItem(QStringLiteral("0x06000000 - VRAM"));
        bs->addItem(QStringLiteral("0x07000000 - OAM"));
        bs->addItem(QStringLiteral("0x08000000 - ROM"));
        bs->setCurrentIndex(1);
        Fit();
        Goto(0);
    }

    void Update() override {
        uint32_t addr = mv->topaddr;
        mv->words.resize(mv->nlines * 4);

        for (int i = 0; i < mv->nlines; i++) {
            if (i && !addr)
                break;

            for (int j = 0; j < 4; j++, addr += 4)
                mv->words[i * 4 + j] = CPUReadMemoryQuick(addr);
        }

        mv->Refill();
    }

    void WriteVal() override {
        switch (mv->fmt) {
            case 0:
                CPUWriteByteQuick(mv->writeaddr, mv->writeval);
                break;

            case 1:
                CPUWriteHalfWordQuick(mv->writeaddr, mv->writeval);
                break;

            case 2:
                CPUWriteMemoryQuick(mv->writeaddr, mv->writeval);
                break;
        }
    }

    void MemLoad(const QString& name, uint32_t addr, uint32_t len) override {
        QFile f(name);
        if (!f.open(QIODevice::ReadOnly))
            return;

        // this does the equivalent of the CPUWriteMemoryQuick()
        while (len > 0) {
            memoryMap m = map[addr >> 24];
            uint32_t off = addr & m.mask;
            int wlen = (off + len) > m.mask ? m.mask + 1 - off : len;
            wlen = f.read(reinterpret_cast<char*>(m.address + off), wlen);

            if (wlen <= 0)
                return;  // FIXME: give error

            len -= wlen;
            addr += wlen;
        }
    }

    void MemSave(const QString& name, uint32_t addr, uint32_t len) override {
        QFile f(name);
        if (!f.open(QIODevice::WriteOnly))
            return;

        // this does the equivalent of the CPUReadMemoryQuick()
        while (len > 0) {
            memoryMap m = map[addr >> 24];
            uint32_t off = addr & m.mask;
            int wlen = (off + len) > m.mask ? m.mask + 1 - off : len;
            wlen = f.write(reinterpret_cast<const char*>(m.address + off), wlen);

            if (wlen <= 0)
                return;  // FIXME: give error

            len -= wlen;
            addr += wlen;
        }
    }
};

class GBMemViewer final : public MemViewerBase {
public:
    explicit GBMemViewer(QWidget* parent) : MemViewerBase(parent, static_cast<uint16_t>(~0)) {
        bs->addItem(QStringLiteral("0x0000 - ROM"));
        bs->addItem(QStringLiteral("0x4000 - ROM"));
        bs->addItem(QStringLiteral("0x8000 - VRAM"));
        bs->addItem(QStringLiteral("0xA000 - SRAM"));
        bs->addItem(QStringLiteral("0xC000 - RAM"));
        bs->addItem(QStringLiteral("0xD000 - WRAM"));
        bs->addItem(QStringLiteral("0xFF00 - I / O"));
        bs->addItem(QStringLiteral("0xFF80 - RAM"));
        bs->setCurrentIndex(1);
        Fit();
        Goto(0);
    }

    void Update() override {
        uint32_t addr = mv->topaddr;
        mv->words.resize(mv->nlines * 4);

        for (int i = 0; i < mv->nlines; i++) {
            if (i && !static_cast<uint16_t>(addr))
                break;

            for (int j = 0; j < 4; j++, addr += 4)
                mv->words[i * 4 + j] = GBReadMemoryQuick(addr);
        }

        mv->Refill();
    }

    void WriteVal() override {
        switch (mv->fmt) {
            case 0:
                GBWriteByteQuick(mv->writeaddr, mv->writeval);
                break;

            case 1:
                GBWriteHalfWordQuick(mv->writeaddr, mv->writeval);
                break;

            case 2:
                GBWriteMemoryQuick(mv->writeaddr, mv->writeval);
                break;
        }
    }

    void MemLoad(const QString& name, uint32_t addr, uint32_t len) override {
        QFile f(name);
        if (!f.open(QIODevice::ReadOnly))
            return;

        // this does the equivalent of the GBWriteMemoryQuick()
        while (len > 0) {
            uint8_t* maddr = gbMemoryMap[addr >> 12];
            uint32_t off = addr & 0xfff;
            int wlen = (off + len) > 0xfff ? 0x1000 - off : len;
            wlen = f.read(reinterpret_cast<char*>(maddr + off), wlen);

            if (wlen <= 0)
                return;  // FIXME: give error

            len -= wlen;
            addr += wlen;
        }
    }

    void MemSave(const QString& name, uint32_t addr, uint32_t len) override {
        QFile f(name);
        if (!f.open(QIODevice::WriteOnly))
            return;

        // this does the equivalent of the GBReadMemoryQuick()
        while (len > 0) {
            uint8_t* maddr = gbMemoryMap[addr >> 12];
            uint32_t off = addr & 0xfff;
            int wlen = (off + len) > 0xfff ? 0x1000 - off : len;
            wlen = f.write(reinterpret_cast<const char*>(maddr + off), wlen);

            if (wlen <= 0)
                return;  // FIXME: give error

            len -= wlen;
            addr += wlen;
        }
    }
};

}  // namespace

Viewer* NewMemViewer(QWidget* parent) {
    return new MemViewer(parent);
}

Viewer* NewGBMemViewer(QWidget* parent) {
    return new GBMemViewer(parent);
}

}  // namespace viewers
