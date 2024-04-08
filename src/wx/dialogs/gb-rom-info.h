#ifndef VBAM_WX_DIALOGS_GB_ROM_INFO_H_
#define VBAM_WX_DIALOGS_GB_ROM_INFO_H_

#include "wx/dialogs/base-dialog.h"

namespace dialogs {

class GbRomInfo : public BaseDialog {
public:
    static GbRomInfo* NewInstance(wxWindow* parent);
    ~GbRomInfo() override = default;

private:
    // The constructor is private so initialization has to be done via the
    // static method. This is because this class is destroyed when its
    // owner, `parent` is destroyed. This prevents accidental deletion.
    GbRomInfo(wxWindow* parent);

    // Handler for the wxEVT_SHOW event.
    void OnDialogShowEvent(wxShowEvent& event);
};

}  // namespace dialogs

#endif  // VBAM_WX_DIALOGS_GB_ROM_INFO_H_
