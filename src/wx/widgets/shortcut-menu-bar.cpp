#include "wx/widgets/shortcut-menu-bar.h"

#include <wx/accel.h>

namespace widgets {

void ShortcutMenuBar::SetAcceleratorTable(const wxAcceleratorTable& /*accel*/) {
    // Do nothing. We don't want to set up accelerators on the menu bar.
    wxMenuBar::SetAcceleratorTable(wxNullAcceleratorTable);
}

wxIMPLEMENT_DYNAMIC_CLASS(ShortcutMenuBar, wxMenuBar);

}  // namespace widgets
