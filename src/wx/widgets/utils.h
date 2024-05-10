#ifndef VBAM_WX_WIDGETS_UTILS_H_
#define VBAM_WX_WIDGETS_UTILS_H_

#include <wx/window.h>
#include <wx/gdicmn.h>

#include "core/base/check.h"

// This file contains a collection of various utility functions for wxWidgets.

namespace widgets {

// Helper function to get the display rectangle. Useful for avoiding drawing a
// dialog outside the screen.
wxRect GetDisplayRect();

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
