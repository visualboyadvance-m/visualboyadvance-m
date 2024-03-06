#include "dialogs/gb-rom-info.h"

#include <wx/control.h>
#include <wx/xrc/xmlres.h>

#include "../common/sizes.h"
#include "../gb/gb.h"
#include "dialogs/game-maker.h"
#include "dialogs/validated-child.h"

namespace dialogs {

namespace {

// Returns a localized string indicating the cartridge type (mapper) for the loaded GB/GBC
// cartridge.
wxString GetCartType() {
    wxString mapper_type;
    switch (g_gbCartData.mapper_type()) {
        case gbCartData::MapperType::kNone:
            mapper_type = _("No mapper");
            break;
        case gbCartData::MapperType::kMbc1:
            mapper_type = _("MBC1");
            break;
        case gbCartData::MapperType::kMbc2:
            mapper_type = _("MBC2");
            break;
        case gbCartData::MapperType::kMbc3:
            mapper_type = _("MBC3");
            break;
        case gbCartData::MapperType::kMbc5:
            mapper_type = _("MBC5");
            break;
        case gbCartData::MapperType::kMbc6:
            mapper_type = _("MBC6");
            break;
        case gbCartData::MapperType::kMbc7:
            mapper_type = _("MBC7");
            break;
        case gbCartData::MapperType::kPocketCamera:
            mapper_type = _("Pocket Camera");
            break;
        case gbCartData::MapperType::kMmm01:
            mapper_type = _("MMM01");
            break;
        case gbCartData::MapperType::kHuC1:
            mapper_type = _("HuC-1");
            break;
        case gbCartData::MapperType::kHuC3:
            mapper_type = _("HuC-3");
            break;
        case gbCartData::MapperType::kTama5:
            mapper_type = _("Bandai TAMA5");
            break;
        case gbCartData::MapperType::kGameGenie:
            mapper_type = _("Game Genie");
            break;
        case gbCartData::MapperType::kGameShark:
            mapper_type = _("Game Shark");
            break;
        case gbCartData::MapperType::kUnknown:
            mapper_type = _("Unknown");
            break;
    }

    const wxString has_ram = g_gbCartData.HasRam() ? _(" + RAM") : "";
    const wxString has_rtc = g_gbCartData.has_rtc() ? _(" + RTC") : "";
    const wxString has_battery = g_gbCartData.has_battery() ? _(" + Battery") : "";
    const wxString has_rumble = g_gbCartData.has_rumble() ? _(" + Rumble") : "";
    const wxString has_motion = g_gbCartData.has_sensor() ? _(" + Motion Sensor") : "";

    return wxString::Format(_("%02X (%s%s%s%s%s%s)"), g_gbCartData.mapper_flag(), mapper_type,
                            has_ram, has_rtc, has_battery, has_rumble, has_motion);
}

// Returns a localized string indicating SGB support for the loaded GB/GBC cartridge.
wxString GetCartSGBFlag() {
    if (g_gbCartData.sgb_support()) {
        return wxString::Format(_("%02X (Supported)"), g_gbCartData.sgb_flag());
    } else {
        return wxString::Format(_("%02X (Not supported)"), g_gbCartData.sgb_flag());
    }
}

// Returns a localized string indicating CGB support for the loaded GB/GBC cartridge.
wxString GetCartCGBFlag() {
    switch (g_gbCartData.cgb_support()) {
        case gbCartData::CGBSupport::kNone:
            return wxString::Format(_("%02X (Not supported)"), g_gbCartData.cgb_flag());
        case gbCartData::CGBSupport::kSupported:
            return wxString::Format(_("%02X (Supported)"), g_gbCartData.cgb_flag());
        case gbCartData::CGBSupport::kRequired:
            return wxString::Format(_("%02X (Required)"), g_gbCartData.cgb_flag());
        default:
            // Unreachable.
            assert(false);
            return "";
    }
}

// Returns a localized string indicating the ROM size of the loaded GB/GBC cartridge.
wxString GetCartRomSize() {
    switch (g_gbCartData.rom_size()) {
        case k32KiB:
            return wxString::Format(_("%02X (32 KiB)"), g_gbCartData.rom_flag());
        case k64KiB:
            return wxString::Format(_("%02X (64 KiB)"), g_gbCartData.rom_flag());
        case k128KiB:
            return wxString::Format(_("%02X (128 KiB)"), g_gbCartData.rom_flag());
        case k256KiB:
            return wxString::Format(_("%02X (256 KiB)"), g_gbCartData.rom_flag());
        case k512KiB:
            return wxString::Format(_("%02X (512 KiB)"), g_gbCartData.rom_flag());
        case k1MiB:
            return wxString::Format(_("%02X (1 MiB)"), g_gbCartData.rom_flag());
        case k2MiB:
            return wxString::Format(_("%02X (2 MiB)"), g_gbCartData.rom_flag());
        case k4MiB:
            return wxString::Format(_("%02X (4 MiB)"), g_gbCartData.rom_flag());
        default:
            return wxString::Format(_("%02X (Unknown)"), g_gbCartData.rom_flag());
    }
}

// Returns a localized string indicating the ROM size of the loaded GB/GBC cartridge.
wxString GetCartRamSize() {
    switch (g_gbCartData.ram_size()) {
        case 0:
            return wxString::Format(_("%02X (None)"), g_gbCartData.ram_flag());
        case k256B:
            return wxString::Format(_("%02X (256 B)"), g_gbCartData.ram_flag());
        case k512B:
            return wxString::Format(_("%02X (512 B)"), g_gbCartData.ram_flag());
        case k2KiB:
            return wxString::Format(_("%02X (2 KiB)"), g_gbCartData.ram_flag());
        case k8KiB:
            return wxString::Format(_("%02X (8 KiB)"), g_gbCartData.ram_flag());
        case k32KiB:
            return wxString::Format(_("%02X (32 KiB)"), g_gbCartData.ram_flag());
        case k128KiB:
            return wxString::Format(_("%02X (128 KiB)"), g_gbCartData.ram_flag());
        case k64KiB:
            return wxString::Format(_("%02X (64 KiB)"), g_gbCartData.ram_flag());
        default:
            return wxString::Format(_("%02X (Unknown)"), g_gbCartData.ram_flag());
    }
}

// Returns a localized string indicating the destination code of the loaded GB/GBC cartridge.
wxString GetCartDestinationCode() {
    switch (g_gbCartData.destination_code()) {
        case gbCartData::DestinationCode::kJapanese:
            return wxString::Format(_("%02X (Japan)"), g_gbCartData.destination_code_flag());
        case gbCartData::DestinationCode::kWorldwide:
            return wxString::Format(_("%02X (World)"), g_gbCartData.destination_code_flag());
        case gbCartData::DestinationCode::kUnknown:
            return wxString::Format(_("%02X (Unknown)"), g_gbCartData.destination_code_flag());
        default:
            // Unreachable.
            assert(false);
            return "";
    }
}

}  // namespace

// static
GbRomInfo* GbRomInfo::NewInstance(wxWindow* parent) {
    assert(parent);
    return new GbRomInfo(parent);
}

GbRomInfo::GbRomInfo(wxWindow* parent) : wxDialog(), keep_on_top_styler_(this) {
#if !wxCHECK_VERSION(3, 1, 0)
    // This needs to be set before loading any element on the window. This also
    // has no effect since wx 3.1.0, where it became the default.
    this->SetExtraStyle(wxWS_EX_VALIDATE_RECURSIVELY);
#endif
    wxXmlResource::Get()->LoadDialog(this, parent, "GBROMInfo");

    Bind(wxEVT_SHOW, &GbRomInfo::OnDialogShowEvent, this);
}

void GbRomInfo::OnDialogShowEvent(wxShowEvent& event) {
    // Let the event propagate.
    event.Skip();

    if (!event.IsShown()) {
        return;
    }

    // Populate the dialog.
    GetValidatedChild(this, "Title")->SetLabel(g_gbCartData.title());
    GetValidatedChild(this, "MakerCode")->SetLabel(g_gbCartData.maker_code());
    GetValidatedChild(this, "MakerName")->SetLabel(GetGameMakerName(g_gbCartData.maker_code()));
    GetValidatedChild(this, "CartridgeType")->SetLabel(GetCartType());
    GetValidatedChild(this, "SGBCode")->SetLabel(GetCartSGBFlag());
    GetValidatedChild(this, "CGBCode")->SetLabel(GetCartCGBFlag());
    GetValidatedChild(this, "ROMSize")->SetLabel(GetCartRomSize());
    GetValidatedChild(this, "RAMSize")->SetLabel(GetCartRamSize());
    GetValidatedChild(this, "DestCode")->SetLabel(GetCartDestinationCode());
    GetValidatedChild(this, "LicCode")
        ->SetLabel(wxString::Format("%02X", g_gbCartData.old_licensee_code()));
    GetValidatedChild(this, "Version")
        ->SetLabel(wxString::Format("%02X", g_gbCartData.version_flag()));
    GetValidatedChild(this, "HeaderChecksum")
        ->SetLabel(wxString::Format(_("%02X (Actual: %02X)"), g_gbCartData.header_checksum(),
                                    g_gbCartData.actual_header_checksum()));
    GetValidatedChild(this, "CartridgeChecksum")
        ->SetLabel(wxString::Format(_("%04X (Actual: %04X)"), g_gbCartData.global_checksum(),
                                    g_gbCartData.actual_global_checksum()));

    // Re-fit everything.
    Fit();
}

}  // namespace dialogs
