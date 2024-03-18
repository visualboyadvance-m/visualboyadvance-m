#ifndef VBAM_WX_DIALOGS_GAME_BOY_CONFIG_H_
#define VBAM_WX_DIALOGS_GAME_BOY_CONFIG_H_

#include <wx/clrpicker.h>
#include <wx/dialog.h>

#include "wx/widgets/keep-on-top-styler.h"

namespace dialogs {

// Manages the Game Boy configuration dialog.
class GameBoyConfig : public wxDialog {
public:
    static GameBoyConfig* NewInstance(wxWindow* parent);
    ~GameBoyConfig() override = default;

private:
    // The constructor is private so initialization has to be done via the
    // static method. This is because this class is destroyed when its
    // owner, `parent` is destroyed. This prevents accidental deletion.
    GameBoyConfig(wxWindow* parent);

    const widgets::KeepOnTopStyler keep_on_top_styler_;
};

}  // namespace dialogs

#endif  // VBAM_WX_DIALOGS_GAME_BOY_CONFIG_H_
