#ifndef VBAM_WX_DIALOGS_GB_ROM_INFO_H_
#define VBAM_WX_DIALOGS_GB_ROM_INFO_H_

#include <wx/dialog.h>

#include "wx/widgets/keep-on-top-styler.h"

namespace dialogs {

class GbRomInfo : public wxDialog {
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

    const widgets::KeepOnTopStyler keep_on_top_styler_;
};

}  // namespace dialogs

#endif  // VBAM_WX_DIALOGS_GB_ROM_INFO_H_
