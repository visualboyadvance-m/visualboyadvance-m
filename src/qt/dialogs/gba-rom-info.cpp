#include "qt/dialogs/gba-rom-info.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QShowEvent>
#include <QVBoxLayout>

#include "core/base/check.h"
#include "core/gba/gbaGlobals.h"
#include "qt/app.h"
#include "qt/dialogs/game-maker.h"
#include "qt/game-area.h"
#include "qt/main-window.h"

namespace dialogs {

namespace {

QString RomString(const uint8_t* p, int len) {
    // Header fields are NUL-padded ASCII.
    int n = 0;
    while (n < len && p[n] != 0)
        n++;
    return QString::fromLatin1(reinterpret_cast<const char*>(p), n);
}

QString Hex2(unsigned int b) {
    return QStringLiteral("%1").arg(b & 0xff, 2, 16, QLatin1Char('0'));
}

}  // namespace

// static
GbaRomInfo* GbaRomInfo::NewInstance(QWidget* parent) {
    VBAM_CHECK(parent);
    return new GbaRomInfo(parent);
}

GbaRomInfo::GbaRomInfo(QWidget* parent) : BaseDialog(parent, "GBAROMInfo") {
    setWindowTitle(tr("ROM Information"));

    auto* layout = new QVBoxLayout(this);
    auto* form = new QFormLayout();

    auto add = [&](const QString& label) {
        auto* value = new QLabel(this);
        value->setTextInteractionFlags(Qt::TextSelectableByMouse);
        form->addRow(label, value);
        return value;
    };

    title_ = add(tr("Game title:"));
    int_title_ = add(tr("Internal title:"));
    scene_ = add(tr("Scene Release:"));
    release_ = add(tr("Release Number:"));
    crc32_ = add(QStringLiteral("CRC32:"));
    game_code_ = add(tr("Game code:"));
    maker_code_ = add(tr("Maker code:"));
    maker_name_ = add(tr("Maker name:"));
    unit_code_ = add(tr("Main unit code:"));
    device_type_ = add(tr("Device type:"));
    version_ = add(tr("ROM version:"));
    crc_ = add(QStringLiteral("CRC:"));

    layout->addLayout(form);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    layout->addWidget(buttons);
}

void GbaRomInfo::Populate() {
    GameArea* panel = vbamApp().frame->GetPanel();
    if (!panel || !g_rom)
        return;

    vbamApp().frame->IdentifyRom();

    title_->setText(panel->rom_name);
    int_title_->setText(RomString(&g_rom[0xa0], 12));
    scene_->setText(panel->rom_scene_rls_name);
    release_->setText(panel->rom_scene_rls);
    crc32_->setText(QStringLiteral("%1").arg(panel->rom_crc32, 8, 16, QLatin1Char('0')).toUpper());
    game_code_->setText(RomString(&g_rom[0xac], 4));

    const QString maker_code = RomString(&g_rom[0xb0], 2);
    maker_code_->setText(maker_code);
    maker_name_->setText(GetGameMakerName(maker_code.toStdString()));
    unit_code_->setText(Hex2(g_rom[0xb3]));

    QString device = Hex2(g_rom[0xb4]);
    if (g_rom[0xb4] & 0x80)
        device.append(QStringLiteral(" (DACS)"));
    device_type_->setText(device);

    version_->setText(Hex2(g_rom[0xbc]));

    uint8_t crc = 0x19;
    for (int i = 0xa0; i < 0xbd; i++)
        crc += g_rom[i];
    crc = -crc;
    crc_->setText(QStringLiteral("%1 (%2)").arg(Hex2(crc), Hex2(g_rom[0xbd])));

    adjustSize();
}

void GbaRomInfo::showEvent(QShowEvent* event) {
    Populate();
    BaseDialog::showEvent(event);
}

}  // namespace dialogs
