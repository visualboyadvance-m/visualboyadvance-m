#include "wx/widgets/utils.h"

#include <wx/display.h>

namespace widgets {

wxRect GetDisplayRect() {
    wxRect display_rect;
    for (unsigned int i = 0; i < wxDisplay::GetCount(); i++) {
        display_rect.Union(wxDisplay(i).GetClientArea());
    }

    return display_rect;
}

}  // namespace widgets
