#include "wx/dialogs/directories-config.h"

#include <wx/filepicker.h>

#include "wx/dialogs/base-dialog.h"
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

DirectoriesConfig::DirectoriesConfig(wxWindow* parent) : BaseDialog(parent, "DirectoriesConfig") {
    // clang-format off
    SetUpDirPicker(GetValidatedChild<wxDirPickerCtrl>("GBARoms"),
                   config::OptionID::kGBAROMDir);
    SetUpDirPicker(GetValidatedChild<wxDirPickerCtrl>("GBRoms"),
                   config::OptionID::kGBROMDir);
    SetUpDirPicker(GetValidatedChild<wxDirPickerCtrl>("GBCRoms"),
                   config::OptionID::kGBGBCROMDir);
    SetUpDirPicker(GetValidatedChild<wxDirPickerCtrl>("BatSaves"),
                   config::OptionID::kGenBatteryDir);
    SetUpDirPicker(GetValidatedChild<wxDirPickerCtrl>("StateSaves"),
                   config::OptionID::kGenStateDir);
    SetUpDirPicker(GetValidatedChild<wxDirPickerCtrl>("Screenshots"),
                   config::OptionID::kGenScreenshotDir);
    SetUpDirPicker(GetValidatedChild<wxDirPickerCtrl>("Recordings"),
                   config::OptionID::kGenRecordingDir);
    // clang-format on

    this->Fit();
}

}  // namespace dialogs
