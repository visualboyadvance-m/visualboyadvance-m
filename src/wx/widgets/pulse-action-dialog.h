#ifndef VBAM_WX_WIDGETS_PULSE_ACTION_DIALOG_H_
#define VBAM_WX_WIDGETS_PULSE_ACTION_DIALOG_H_

#include <functional>

#include <wx/datetime.h>
#include <wx/dialog.h>
#include <wx/timer.h>

// Forward declarations.
class wxGauge;
class wxStaticText;

namespace widgets {

// A modal dialog that displays a pulsing gauge, a message, elapsed time, and a
// Cancel button. On each timer tick, a user-supplied callback is invoked. The
// callback can update the displayed message and signal completion.
//
// This replaces wxProgressDialog for cases where the progress is indeterminate
// and the caller just needs a cancellable wait loop.
//
// Sample usage:
//
//   bool connected = false;
//   PulseActionDialog dlg(this, "Connecting...", "Please wait",
//       [&](wxString&) { connected = Check(); return !connected; });
//   dlg.ShowModal();
//   if (dlg.WasCancelled()) { /* user hit cancel */ }
class PulseActionDialog : public wxDialog {
public:
    // The callback is called on each timer tick. It receives a mutable
    // reference to the message string; changes are reflected in the dialog.
    // Return true to keep waiting, false to finish (closes the dialog).
    using PollCallback = std::function<bool(wxString& message)>;

    PulseActionDialog(wxWindow* parent,
                      const wxString& title,
                      const wxString& message,
                      PollCallback callback,
                      int poll_interval_ms = 50);
    ~PulseActionDialog() override = default;

    // Returns true if the user pressed Cancel rather than the action
    // completing normally.
    bool WasCancelled() const { return cancelled_; }

    // Disable copy and assignment.
    PulseActionDialog(const PulseActionDialog&) = delete;
    PulseActionDialog& operator=(const PulseActionDialog&) = delete;

private:
    void OnTimer(wxTimerEvent& event);
    void OnCancel(wxCommandEvent& event);

    wxStaticText* message_text_;
    wxGauge* gauge_;
    wxStaticText* elapsed_text_;
    wxTimer timer_;
    PollCallback callback_;
    bool cancelled_ = false;
    wxDateTime start_time_;
};

}  // namespace widgets

#endif  // VBAM_WX_WIDGETS_PULSE_ACTION_DIALOG_H_
