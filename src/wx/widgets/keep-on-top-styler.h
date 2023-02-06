#ifndef VBAM_WX_DIALOGS_BASE_DIALOG_H_
#define VBAM_WX_DIALOGS_BASE_DIALOG_H_

#include <wx/event.h>

#include "config/option-observer.h"

// Forward declarations.
class wxTopLevelWindow;

namespace config {
class Option;
}

namespace widgets {

// Helper class to automatically set and unset the wxSTAY_ON_TOP to any
// top-level window. Simply add it as a private member to any top-level window
// implementation and pass the reference to the window in the constructor.
//
// Sample usage:
//
// class MyDialog: public wxDialog {
// public:
//     MyDialog() : wxDialog(), keep_on_top_styler_(this) {}
//     ~MyDialog() override = default;

// private:
//     KeepOnTopStyler keep_on_top_styler_;
// };
class KeepOnTopStyler {
public:
    // `window` must outlive this object. The easiest way to do so is to add
    // this object as a private member of the top-level window.
    explicit KeepOnTopStyler(wxTopLevelWindow* window);
    ~KeepOnTopStyler();

    // Disable copy and assignment.
    KeepOnTopStyler(const KeepOnTopStyler&) = delete;
    KeepOnTopStyler& operator=(const KeepOnTopStyler&) = delete;

private:
    // Callback for the `window` wxEVT_SHOW event.
    void OnShow(wxShowEvent& show_event);

    // Callback fired when the KeepOnTop setting has changed.
    void OnKeepOnTopChanged(config::Option* option);

    // The non-owned window whose style should be modified.
    wxTopLevelWindow *const window_;

    // Observer for the KeepOnTop setting changed event.
    const config::OptionsObserver on_top_observer_;
};

}  // namespace widgets

#endif  // VBAM_WX_DIALOGS_BASE_DIALOG_H_
