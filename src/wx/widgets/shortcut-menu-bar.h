#ifndef VBAM_WIDGETS_SHORTCUT_MENU_BAR_H_
#define VBAM_WIDGETS_SHORTCUT_MENU_BAR_H_

#include <wx/menu.h>

namespace widgets {

// A menu bar with no accelerator table. This is used to prevent the menu bar
// from stealing keyboard shortcuts from the main window.
class ShortcutMenuBar : public wxMenuBar {
public:
    ShortcutMenuBar() = default;
    ~ShortcutMenuBar() override = default;

    // wxMenuBar implementation.
    void SetAcceleratorTable(const wxAcceleratorTable& accel) override;

    wxDECLARE_DYNAMIC_CLASS(ShortcutMenuBar);
};

}  // namespace widgets

#endif  // VBAM_WIDGETS_SHORTCUT_MENU_BAR_H_
