#include "wx/dialogs/game-boy-config.h"

#include <array>
#include <cstddef>
#include <cstdint>

#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/clrpicker.h>
#include <wx/filepicker.h>
#include <wx/panel.h>
#include <wx/stattext.h>

#include <wx/xrc/xmlres.h>

#include "wx/config/option-observer.h"
#include "wx/config/option-proxy.h"
#include "wx/dialogs/validated-child.h"
#include "wx/widgets/option-validator.h"

namespace dialogs {

namespace {

static constexpr size_t kNbPalettes = 3;

// These are the choices for canned colors; their order must match the names in
// wxChoice control.
// clang-format off
static constexpr std::array<std::array<uint16_t, 8>, 9> kDefaultPalettes = {{
    // Standard
    {0x7FFF, 0x56B5, 0x318C, 0x0000, 0x7FFF, 0x56B5, 0x318C, 0x0000},
    // Blue Sea
    {0x6200, 0x7E10, 0x7C10, 0x5000, 0x6200, 0x7E10, 0x7C10, 0x5000},
    // Dark Night
    {0x4008, 0x4000, 0x2000, 0x2008, 0x4008, 0x4000, 0x2000, 0x2008},
    // Green Forest
    {0x43F0, 0x03E0, 0x4200, 0x2200, 0x43F0, 0x03E0, 0x4200, 0x2200},
    // Hot Desert
    {0x43FF, 0x03FF, 0x221F, 0x021F, 0x43FF, 0x03FF, 0x221F, 0x021F},
    // Pink Dreams
    {0x621F, 0x7E1F, 0x7C1F, 0x2010, 0x621F, 0x7E1F, 0x7C1F, 0x2010},
    // Weird Colors
    {0x621F, 0x401F, 0x001F, 0x2010, 0x621F, 0x401F, 0x001F, 0x2010},
    // Real GB Colors
    {0x1B8E, 0x02C0, 0x0DA0, 0x1140, 0x1B8E, 0x02C0, 0x0DA0, 0x1140},
    // Real 'GB on GBASP' Colors
    // TODO: Figure out what the commented out values mean.
    {0x7BDE, /*0x23F0*/ 0x5778, /*0x5DC0*/ 0x5640, 0x0000, 0x7BDE, /*0x3678*/ 0x529C, /*0x0980*/ 0x2990, 0x0000},
}};
// clang-format on

// This is a custom wxClientData held by wxPanels implementing the
// GBColorPrefPanel panel. This takes care of setting the wxPanel with the
// appropriate event handlers and validators for its internal widgets.
class GBPalettePanelData final : public wxClientData {
public:
    GBPalettePanelData(wxPanel* panel, size_t palette_id);
    ~GBPalettePanelData() final = default;

private:
    // Update `colour_pickers_` to match the `palette_` value and reset
    // `default_selector_` if the current palette corresponds to one of the
    // default palettes.
    void UpdateColourPickers();

    // Callback for the `colour_pickers_` wxEVT_COLOURPICKER_CHANGED events.
    void OnColourChanged(size_t colour_index, wxColourPickerEvent& event);

    // Callback for the `default_selector_` wxEVT_CHOICE events.
    void OnDefaultPaletteSelected(wxCommandEvent& event);

    // Callback for the "Reset" wxButton event.
    void OnPaletteReset(wxCommandEvent& event);

    wxChoice* const default_selector_;
    const config::OptionID option_id_;

    std::array<wxColourPickerCtrl*, 8> colour_pickers_;
    std::array<uint16_t, 8> palette_;

    friend class PaletteValidator;
};

// Custom validator for kGbPalette options. The "work" palette is held by the
// GBPalettePanelData object attached to this wxPanel.
class PaletteValidator final : public widgets::OptionValidator {
public:
    PaletteValidator(GBPalettePanelData* palette_data)
        : widgets::OptionValidator(palette_data->option_id_),
          palette_data_(palette_data) {
        assert(option()->is_gb_palette());
        assert(palette_data);
    }
    ~PaletteValidator() final = default;

private:
    // widgets::OptionValidator implementation.
    wxObject* Clone() const final {
        return new PaletteValidator(palette_data_);
    }

    bool IsWindowValueValid() final { return true; }

    bool WriteToWindow() final {
        palette_data_->palette_ = option()->GetGbPalette();
        palette_data_->UpdateColourPickers();
        return true;
    }

    bool WriteToOption() final {
        return option()->SetGbPalette(palette_data_->palette_);
    }

    GBPalettePanelData* const palette_data_;
};

// Custom validator for a kString Option and a wxFilePickerCtrl widget. This
// also updates "label" to display the file currently saved as the BIOS.
class BIOSPickerValidator final : public widgets::OptionValidator {
public:
    BIOSPickerValidator(config::OptionID option_id, wxStaticText* label)
        : widgets::OptionValidator(option_id), label_(label) {
        assert(label_);
        assert(option()->is_string());
    }
    ~BIOSPickerValidator() final = default;

private:
    // widgets::OptionValidator implementation.
    wxObject* Clone() const final {
        return new BIOSPickerValidator(option()->id(), label_);
    }

    bool IsWindowValueValid() final { return true; }

    bool WriteToWindow() final {
        const wxString& selection = option()->GetString();

        if (selection.empty()) {
            label_->SetLabel(_("(None)"));
        } else {
            wxFilePickerCtrl* file_picker =
                wxDynamicCast(GetWindow(), wxFilePickerCtrl);
            assert(file_picker);
            file_picker->SetPath(selection);
            label_->SetLabel(selection);
        }

        return true;
    }

    bool WriteToOption() final {
        const wxFilePickerCtrl* file_picker =
            wxDynamicCast(GetWindow(), wxFilePickerCtrl);
        assert(file_picker);
        return option()->SetString(file_picker->GetPath());
    }

    wxStaticText* label_;
};

// Custom validator for the kPrefBorderOn and kPrefBorderAutomatic Options.
// These Options are controlled by a single wxChoice widget and are only ever
// updated by this dialog so we don't need to observe them.
class BorderSelectorValidator final : public wxValidator {
public:
    BorderSelectorValidator() = default;
    ~BorderSelectorValidator() final = default;

private:
    // wxValidator implementation.
    wxObject* Clone() const final { return new BorderSelectorValidator(); }

    bool TransferFromWindow() final {
        const wxChoice* borders_selector = wxDynamicCast(GetWindow(), wxChoice);
        assert(borders_selector);
        switch (borders_selector->GetSelection()) {
            case 0:
                OPTION(kPrefBorderOn) = false;
                OPTION(kPrefBorderAutomatic) = false;
                break;

            case 1:
                OPTION(kPrefBorderOn) = true;
                break;

            case 2:
                OPTION(kPrefBorderOn) = false;
                OPTION(kPrefBorderAutomatic) = true;
                break;
        }

        return true;
    }

    bool Validate(wxWindow*) final { return true; }

    bool TransferToWindow() final {
        wxChoice* borders_selector = wxDynamicCast(GetWindow(), wxChoice);
        assert(borders_selector);

        if (!OPTION(kPrefBorderOn) && !OPTION(kPrefBorderAutomatic)) {
            borders_selector->SetSelection(0);
        } else if (OPTION(kPrefBorderOn)) {
            borders_selector->SetSelection(1);
        } else {
            borders_selector->SetSelection(2);
        }

        return true;
    }

#if WX_HAS_VALIDATOR_SET_WINDOW_OVERRIDE
    void SetWindow(wxWindow* window) final {
        wxValidator::SetWindow(window);
        TransferToWindow();
    };
#endif
};

GBPalettePanelData::GBPalettePanelData(wxPanel* panel, size_t palette_id)
    : wxClientData(),
      default_selector_(GetValidatedChild<wxChoice>(panel, "DefaultPalette")),
      option_id_(static_cast<config::OptionID>(
          static_cast<size_t>(config::OptionID::kGBPalette0) + palette_id)) {
    assert(panel);
    assert(palette_id < kNbPalettes);

    default_selector_->Bind(
        wxEVT_CHOICE, &GBPalettePanelData::OnDefaultPaletteSelected, this);

    GetValidatedChild<wxCheckBox>(panel, "UsePalette")
        ->SetValidator(widgets::OptionSelectedValidator(
            config::OptionID::kPrefGBPaletteOption, palette_id));

    for (size_t i = 0; i < colour_pickers_.size(); i++) {
        wxColourPickerCtrl* colour_picker =
            GetValidatedChild<wxColourPickerCtrl>(
                panel, wxString::Format("Color%zu", i));
        colour_pickers_[i] = colour_picker;

        // Update the internal palette reference on colour change.
        colour_picker->Bind(wxEVT_COLOURPICKER_CHANGED,
                            std::bind(&GBPalettePanelData::OnColourChanged,
                                      this, i, std::placeholders::_1),
                            colour_picker->GetId());
    }

    GetValidatedChild(panel, "Reset")
        ->Bind(wxEVT_BUTTON, &GBPalettePanelData::OnPaletteReset, this);
}

void GBPalettePanelData::UpdateColourPickers() {
    // Update all of the wxColourPickers based on the current palette values.
    for (size_t i = 0; i < palette_.size(); i++) {
        const uint16_t element = palette_[i];
        colour_pickers_[i]->SetColour(wxColour(((element << 3) & 0xf8),
                                               ((element >> 2) & 0xf8),
                                               ((element >> 7) & 0xf8)));
    }

    // See if the current palette corresponds to a default palette.
    for (size_t i = 0; i < kDefaultPalettes.size(); i++) {
        if (palette_ == kDefaultPalettes[i]) {
            default_selector_->SetSelection(i + 1);
            return;
        }
    }

    // The configuration is not a default palette, set it to "Custom".
    default_selector_->SetSelection(0);
}

void GBPalettePanelData::OnColourChanged(size_t colour_index,
                                         wxColourPickerEvent& event) {
    assert(colour_index < palette_.size());

    // Update the colour value.
    const wxColour colour = event.GetColour();
    palette_[colour_index] = ((colour.Red() & 0xf8) >> 3) +
                             ((colour.Green() & 0xf8) << 2) +
                             ((colour.Blue() & 0xf8) << 7);

    // Reflect changes to the user.
    UpdateColourPickers();

    // Let the event propagate.
    event.Skip();
}

void GBPalettePanelData::OnDefaultPaletteSelected(wxCommandEvent& event) {
    if (event.GetSelection() > 0) {
        // Update the palette to one of the default palettes.
        palette_ = kDefaultPalettes[event.GetSelection() - 1];
        UpdateColourPickers();
    }

    // Let the event propagate.
    event.Skip();
}

void GBPalettePanelData::OnPaletteReset(wxCommandEvent& event) {
    // Reset the palette to the last user-saved value.
    palette_ = config::Option::ByID(option_id_)->GetGbPalette();
    UpdateColourPickers();

    // Let the event propagate.
    event.Skip();
}

}  // namespace

// static
GameBoyConfig* GameBoyConfig::NewInstance(wxWindow* parent) {
    assert(parent);
    return new GameBoyConfig(parent);
}

GameBoyConfig::GameBoyConfig(wxWindow* parent)
    : wxDialog(), keep_on_top_styler_(this) {
#if !wxCHECK_VERSION(3, 1, 0)
    // This needs to be set before loading any element on the window. This also
    // has no effect since wx 3.1.0, where it became the default.
    this->SetExtraStyle(wxWS_EX_VALIDATE_RECURSIVELY);
#endif
    wxXmlResource::Get()->LoadDialog(this, parent, "GameBoyConfig");

    // System and Peripherals.
    GetValidatedChild(this, "System")
        ->SetValidator(widgets::OptionChoiceValidator(
            config::OptionID::kPrefEmulatorType));

    // "Display borders" corresponds to 2 variables.
    GetValidatedChild(this, "Borders")->SetValidator(BorderSelectorValidator());

    // GB BIOS ROM
    GetValidatedChild(this, "GBBiosPicker")
        ->SetValidator(BIOSPickerValidator(
            config::OptionID::kGBBiosFile,
            GetValidatedChild<wxStaticText>(this, "GBBiosLabel")));

    // GBC BIOS ROM
    GetValidatedChild(this, "GBCBiosPicker")
        ->SetValidator(BIOSPickerValidator(
            config::OptionID::kGBGBCBiosFile,
            GetValidatedChild<wxStaticText>(this, "GBCBiosLabel")));

    for (size_t i = 0; i < kNbPalettes; i++) {
        // All of the wxPanel logic is handled in its client object.
        wxPanel* panel =
            GetValidatedChild<wxPanel>(this, wxString::Format("cp%zu", i));
        GBPalettePanelData* palette_data = new GBPalettePanelData(panel, i);

        // `panel` takes ownership of `palette_data` here.
        panel->SetClientObject(palette_data);
        panel->SetValidator(PaletteValidator(palette_data));
    }

    this->Fit();
}

}  // namespace dialogs
