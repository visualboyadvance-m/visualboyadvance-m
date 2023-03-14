#ifndef VBAM_WX_DIALOGS_VALIDATED_CHILD_H_
#define VBAM_WX_DIALOGS_VALIDATED_CHILD_H_

#include <cassert>

#include <wx/string.h>
#include <wx/window.h>

namespace dialogs {

// Helper functions to assert on the returned value.
inline wxWindow* GetValidatedChild(const wxWindow* parent,
                                   const wxString& name) {
    wxWindow* window = parent->FindWindow(name);
    assert(window);
    return window;
}

template <class T>
T* GetValidatedChild(const wxWindow* parent, const wxString& name) {
    T* child = wxDynamicCast(GetValidatedChild(parent, name), T);
    assert(child);
    return child;
}

}  // namespace dialogs

#endif  // VBAM_WX_DIALOGS_VALIDATED_CHILD_H_
