#include "wx/dialogs/directories-config.h"

#include <wx/filepicker.h>

#include <wx/xrc/xmlres.h>

#include "wx/dialogs/validated-child.h"
#include "wx/widgets/option-validator.h"

namespace dialogs {

namespace {

// Custom validator for a kString Option and a wxDirPickerCtrl widget.
class DirectoryStringValidator final : public widgets::OptionValidator {
public:
    DirectoryStringValidator(config::OptionID option_id)
        : widgets::OptionValidator(option_id) {
        assert(option()->is_string());
    }
    ~DirectoryStringValidator() final = default;

private:
    // widgets::OptionValidator implementation.
    wxObject* Clone() const final {
        return new DirectoryStringValidator(option()->id());
    }

    bool IsWindowValueValid() final { return true; }

    bool WriteToWindow() final {
        wxDirPickerCtrl* dir_picker =
            wxDynamicCast(GetWindow(), wxDirPickerCtrl);
        assert(dir_picker);
        dir_picker->SetPath(option()->GetString());
        return true;
    }

    bool WriteToOption() final {
        const wxDirPickerCtrl* dir_picker =
            wxDynamicCast(GetWindow(), wxDirPickerCtrl);
        assert(dir_picker);
        return option()->SetString(dir_picker->GetPath());
    }
};

void SetUpDirPicker(wxDirPickerCtrl* dir_picker,
                    const config::OptionID& option_id) {
    dir_picker->SetValidator(DirectoryStringValidator(option_id));
    dir_picker->GetPickerCtrl()->SetLabel(_("Browse"));
}

}  // namespace

// static
DirectoriesConfig* DirectoriesConfig::NewInstance(wxWindow* parent) {
    assert(parent);
    return new DirectoriesConfig(parent);
}

DirectoriesConfig::DirectoriesConfig(wxWindow* parent)
    : wxDialog(), keep_on_top_styler_(this) {
#if !wxCHECK_VERSION(3, 1, 0)
    // This needs to be set before loading any element on the window. This also
    // has no effect since wx 3.1.0, where it became the default.
    this->SetExtraStyle(wxWS_EX_VALIDATE_RECURSIVELY);
#endif
    wxXmlResource::Get()->LoadDialog(this, parent, "DirectoriesConfig");

    SetUpDirPicker(GetValidatedChild<wxDirPickerCtrl>(this, "GBARoms"),
                   config::OptionID::kGBAROMDir);
    SetUpDirPicker(GetValidatedChild<wxDirPickerCtrl>(this, "GBRoms"),
                   config::OptionID::kGBROMDir);
    SetUpDirPicker(GetValidatedChild<wxDirPickerCtrl>(this, "GBCRoms"),
                   config::OptionID::kGBGBCROMDir);
    SetUpDirPicker(GetValidatedChild<wxDirPickerCtrl>(this, "BatSaves"),
                   config::OptionID::kGenBatteryDir);
    SetUpDirPicker(GetValidatedChild<wxDirPickerCtrl>(this, "StateSaves"),
                   config::OptionID::kGenStateDir);
    SetUpDirPicker(GetValidatedChild<wxDirPickerCtrl>(this, "Screenshots"),
                   config::OptionID::kGenScreenshotDir);
    SetUpDirPicker(GetValidatedChild<wxDirPickerCtrl>(this, "Recordings"),
                   config::OptionID::kGenRecordingDir);

    this->Fit();
}

}  // namespace dialogs
