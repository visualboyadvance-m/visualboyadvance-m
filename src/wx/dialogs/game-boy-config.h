#ifndef VBAM_WX_DIALOGS_GAME_BOY_CONFIG_H_
#define VBAM_WX_DIALOGS_GAME_BOY_CONFIG_H_

#include <vector>

#include <wx/clrpicker.h>

#include "wx/dialogs/base-dialog.h"

class wxNotebook;

namespace dialogs {

// Manages the Game Boy configuration dialog.
class GameBoyConfig : public BaseDialog {
public:
    static GameBoyConfig* NewInstance(wxWindow* parent);
    ~GameBoyConfig() override = default;

    int LazyTabCount() const override { return kTabCount; }
    bool LoadLazyTab(int index) override;

private:
    // Outer tabs 0..2, inner Custom Colors palette sub-tabs 3..5.
    enum Tab {
        kTabSystem = 0,
        kTabBootRom,
        kTabCustomColors,
        kTabPalette0,
        kTabPalette1,
        kTabPalette2,
        kTabCount,
    };

    GameBoyConfig(wxWindow* parent);

    void InitSystemTab();
    void InitBootRomTab();
    void InitCustomColorsTab();
    void InitPaletteTab(size_t palette_id);

    wxNotebook* outer_notebook_ = nullptr;
    wxNotebook* inner_notebook_ = nullptr;
    std::vector<bool> tab_loaded_;
};

}  // namespace dialogs

#endif  // VBAM_WX_DIALOGS_GAME_BOY_CONFIG_H_
