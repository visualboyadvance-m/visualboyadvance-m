#ifndef VBAM_WX_WIDGETS_UTILS_H_
#define VBAM_WX_WIDGETS_UTILS_H_

#include <wx/window.h>
#include <wx/gdicmn.h>

#include "core/base/check.h"

class wxDialog;

// This file contains a collection of various utility functions for wxWidgets.

namespace widgets {

// Helper function to get the display rectangle. Useful for avoiding drawing a
// dialog outside the screen.
wxRect GetDisplayRect();

#if defined(__WXQT__) && defined(__ANDROID__)

// Makes an XRC-loaded dialog usable on a phone screen: moves its content into a
// scroller (leaving the OK/Cancel row pinned below) and clamps the dialog to the
// area the activity actually has, so nothing is laid out off-screen. Idempotent,
// and cheap enough to call every time a dialog is shown.
//
// Pass wrap=false to only re-clamp a dialog that has already been adapted. That
// is what Fit() overrides want: they can run while a dialog is still being
// built, and moving the content into the scroller before its constructor has
// added everything would leave later children parented to the dialog while
// their sizer belongs to the scroller.
//
// Returns whether the dialog is adapted, i.e. whether its size is now managed
// here and the caller should not size it itself. Always false off Android,
// where dialogs are left exactly as their XRC describes them.
bool AdaptDialogToScreen(wxDialog* dialog, bool wrap = true);

#else

inline bool AdaptDialogToScreen(wxDialog*, bool = true) { return false; }

#endif  // defined(__WXQT__) && defined(__ANDROID__)

// Helper functions to assert on the returned value.
inline wxWindow* GetValidatedChild(const wxWindow* parent,
                                   const wxString& name) {
    wxWindow* window = parent->FindWindow(name);
    VBAM_CHECK(window);
    return window;
}

template <class T>
T* GetValidatedChild(const wxWindow* parent, const wxString& name) {
    T* child = wxDynamicCast(GetValidatedChild(parent, name), T);
    VBAM_CHECK(child);
    return child;
}


} // namespace widgets

#endif // VBAM_WX_WIDGETS_UTILS_H_
