#ifndef VBAM_WX_DIALOGS_GAME_BOY_CONFIG_H_
#define VBAM_WX_DIALOGS_GAME_BOY_CONFIG_H_

#include <wx/clrpicker.h>

#include "wx/dialogs/base-dialog.h"

namespace dialogs {

// Manages the Game Boy configuration dialog.
class GameBoyConfig : public BaseDialog {
public:
    static GameBoyConfig* NewInstance(wxWindow* parent);
    ~GameBoyConfig() override = default;

private:
    // The constructor is private so initialization has to be done via the
    // static method. This is because this class is destroyed when its
    // owner, `parent` is destroyed. This prevents accidental deletion.
    GameBoyConfig(wxWindow* parent);
};

}  // namespace dialogs

#endif  // VBAM_WX_DIALOGS_GAME_BOY_CONFIG_H_
