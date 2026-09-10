#include "qt/dialogs/gb-rom-info.h"

#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QShowEvent>
#include <QVBoxLayout>

#include "core/base/check.h"
#include "core/base/sizes.h"
#include "core/gb/gb.h"
#include "core/gb/gbCartData.h"
#include "qt/dialogs/game-maker.h"

namespace dialogs {

namespace {

QString Hex2(unsigned int b) {
    return QStringLiteral("%1").arg(b & 0xff, 2, 16, QLatin1Char('0')).toUpper();
}

QString Hex4(unsigned int b) {
    return QStringLiteral("%1").arg(b & 0xffff, 4, 16, QLatin1Char('0')).toUpper();
}

QString Tr(const char* s) {
    return QCoreApplication::translate("dialogs::GbRomInfo", s);
}

// Returns a localized string indicating the cartridge type (mapper) for the
// loaded GB/GBC cartridge.
QString GetCartType() {
    QString mapper_type;
    switch (g_gbCartData.mapper_type()) {
        case gbCartData::MapperType::kNone:
            mapper_type = Tr("No mapper");
            break;
        case gbCartData::MapperType::kMbc1:
            mapper_type = "MBC1";
            break;
        case gbCartData::MapperType::kMbc2:
            mapper_type = "MBC2";
            break;
        case gbCartData::MapperType::kMbc3:
            mapper_type = "MBC3";
            break;
        case gbCartData::MapperType::kMbc5:
            mapper_type = "MBC5";
            break;
        case gbCartData::MapperType::kMbc6:
            mapper_type = "MBC6";
            break;
        case gbCartData::MapperType::kMbc7:
            mapper_type = "MBC7";
            break;
        case gbCartData::MapperType::kPocketCamera:
            mapper_type = Tr("Pocket Camera");
            break;
        case gbCartData::MapperType::kMmm01:
            mapper_type = "MMM01";
            break;
        case gbCartData::MapperType::kHuC1:
            mapper_type = "HuC-1";
            break;
        case gbCartData::MapperType::kHuC3:
            mapper_type = "HuC-3";
            break;
        case gbCartData::MapperType::kTama5:
            mapper_type = "Bandai TAMA5";
            break;
        case gbCartData::MapperType::kGameGenie:
            mapper_type = "Game Genie";
            break;
        case gbCartData::MapperType::kGameShark:
            mapper_type = "Game Shark";
            break;
        case gbCartData::MapperType::kUnknown:
            mapper_type = Tr("Unknown");
            break;
    }

    const QString has_ram = g_gbCartData.HasRam() ? Tr(" + RAM") : QString();
    const QString has_rtc = g_gbCartData.has_rtc() ? Tr(" + RTC") : QString();
    const QString has_battery = g_gbCartData.has_battery() ? Tr(" + Battery") : QString();
    const QString has_rumble = g_gbCartData.has_rumble() ? Tr(" + Rumble") : QString();
    const QString has_motion = g_gbCartData.has_sensor() ? Tr(" + Motion Sensor") : QString();

    return QStringLiteral("%1 (%2%3%4%5%6%7)")
        .arg(Hex2(g_gbCartData.mapper_flag()), mapper_type, has_ram, has_rtc, has_battery,
             has_rumble, has_motion);
}

QString GetCartSGBFlag() {
    if (g_gbCartData.sgb_support()) {
        return Tr("%1 (Supported)").arg(Hex2(g_gbCartData.sgb_flag()));
    } else {
        return Tr("%1 (Not supported)").arg(Hex2(g_gbCartData.sgb_flag()));
    }
}

QString GetCartCGBFlag() {
    switch (g_gbCartData.cgb_support()) {
        case gbCartData::CGBSupport::kNone:
            return Tr("%1 (Not supported)").arg(Hex2(g_gbCartData.cgb_flag()));
        case gbCartData::CGBSupport::kSupported:
            return Tr("%1 (Supported)").arg(Hex2(g_gbCartData.cgb_flag()));
        case gbCartData::CGBSupport::kRequired:
            return Tr("%1 (Required)").arg(Hex2(g_gbCartData.cgb_flag()));
    }

    VBAM_NOTREACHED_RETURN(QString());
}

QString GetCartRomSize() {
    const QString flag = Hex2(g_gbCartData.rom_flag());
    switch (g_gbCartData.rom_size()) {
        case k32KiB:
            return Tr("%1 (32 KiB)").arg(flag);
        case k64KiB:
            return Tr("%1 (64 KiB)").arg(flag);
        case k128KiB:
            return Tr("%1 (128 KiB)").arg(flag);
        case k256KiB:
            return Tr("%1 (256 KiB)").arg(flag);
        case k512KiB:
            return Tr("%1 (512 KiB)").arg(flag);
        case k1MiB:
            return Tr("%1 (1 MiB)").arg(flag);
        case k2MiB:
            return Tr("%1 (2 MiB)").arg(flag);
        case k4MiB:
            return Tr("%1 (4 MiB)").arg(flag);
        default:
            return Tr("%1 (Unknown)").arg(flag);
    }
}

QString GetCartRamSize() {
    const QString flag = Hex2(g_gbCartData.ram_flag());
    switch (g_gbCartData.ram_size()) {
        case 0:
            return Tr("%1 (None)").arg(flag);
        case k256B:
            return Tr("%1 (256 B)").arg(flag);
        case k512B:
            return Tr("%1 (512 B)").arg(flag);
        case k2KiB:
            return Tr("%1 (2 KiB)").arg(flag);
        case k8KiB:
            return Tr("%1 (8 KiB)").arg(flag);
        case k32KiB:
            return Tr("%1 (32 KiB)").arg(flag);
        case k128KiB:
            return Tr("%1 (128 KiB)").arg(flag);
        case k64KiB:
            return Tr("%1 (64 KiB)").arg(flag);
        default:
            return Tr("%1 (Unknown)").arg(flag);
    }
}

QString GetCartDestinationCode() {
    const QString flag = Hex2(g_gbCartData.destination_code_flag());
    switch (g_gbCartData.destination_code()) {
        case gbCartData::DestinationCode::kJapanese:
            return Tr("%1 (Japan)").arg(flag);
        case gbCartData::DestinationCode::kWorldwide:
            return Tr("%1 (World)").arg(flag);
        case gbCartData::DestinationCode::kUnknown:
            return Tr("%1 (Unknown)").arg(flag);
    }

    VBAM_NOTREACHED_RETURN(QString());
}

}  // namespace

// static
GbRomInfo* GbRomInfo::NewInstance(QWidget* parent) {
    VBAM_CHECK(parent);
    return new GbRomInfo(parent);
}

GbRomInfo::GbRomInfo(QWidget* parent) : BaseDialog(parent, "GBROMInfo") {
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
    maker_code_ = add(tr("Maker code:"));
    maker_name_ = add(tr("Maker name:"));
    cartridge_type_ = add(tr("Cartridge type:"));
    sgb_code_ = add(tr("SGB code:"));
    cgb_code_ = add(tr("CGB code:"));
    rom_size_ = add(tr("ROM size:"));
    ram_size_ = add(tr("RAM size:"));
    dest_code_ = add(tr("Destination code:"));
    lic_code_ = add(tr("License code:"));
    version_ = add(tr("Version:"));
    header_checksum_ = add(tr("Header checksum:"));
    cartridge_checksum_ = add(tr("Cartridge checksum:"));

    layout->addLayout(form);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    layout->addWidget(buttons);
}

void GbRomInfo::Populate() {
    title_->setText(QString::fromStdString(g_gbCartData.title()));
    maker_code_->setText(QString::fromStdString(g_gbCartData.maker_code()));
    maker_name_->setText(GetGameMakerName(g_gbCartData.maker_code()));
    cartridge_type_->setText(GetCartType());
    sgb_code_->setText(GetCartSGBFlag());
    cgb_code_->setText(GetCartCGBFlag());
    rom_size_->setText(GetCartRomSize());
    ram_size_->setText(GetCartRamSize());
    dest_code_->setText(GetCartDestinationCode());
    lic_code_->setText(Hex2(g_gbCartData.old_licensee_code()));
    version_->setText(Hex2(g_gbCartData.version_flag()));
    header_checksum_->setText(tr("%1 (Actual: %2)")
                                  .arg(Hex2(g_gbCartData.header_checksum()),
                                       Hex2(g_gbCartData.actual_header_checksum())));
    cartridge_checksum_->setText(tr("%1 (Actual: %2)")
                                     .arg(Hex4(g_gbCartData.global_checksum()),
                                          Hex4(g_gbCartData.actual_global_checksum())));
    adjustSize();
}

void GbRomInfo::showEvent(QShowEvent* event) {
    Populate();
    BaseDialog::showEvent(event);
}

}  // namespace dialogs
