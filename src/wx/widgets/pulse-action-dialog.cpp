#include "wx/widgets/pulse-action-dialog.h"

#include <wx/button.h>
#include <wx/gauge.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

namespace widgets {

PulseActionDialog::PulseActionDialog(wxWindow* parent,
                                     const wxString& title,
                                     const wxString& message,
                                     PollCallback callback,
                                     int poll_interval_ms)
    : wxDialog(parent, wxID_ANY, title, wxDefaultPosition, wxDefaultSize,
               wxDEFAULT_DIALOG_STYLE),
      timer_(this),
      callback_(std::move(callback)),
      start_time_(wxDateTime::Now()) {
    auto* sizer = new wxBoxSizer(wxVERTICAL);

    message_text_ = new wxStaticText(this, wxID_ANY, message);
    sizer->Add(message_text_, 0, wxALL | wxEXPAND, 10);

    gauge_ = new wxGauge(this, wxID_ANY, 100, wxDefaultPosition,
                         wxSize(300, -1), wxGA_HORIZONTAL | wxGA_SMOOTH);
    gauge_->Pulse();
    sizer->Add(gauge_, 0, wxLEFT | wxRIGHT | wxEXPAND, 10);

    elapsed_text_ = new wxStaticText(this, wxID_ANY, _("Elapsed time: 00:00"));
    sizer->Add(elapsed_text_, 0, wxALL, 10);

    auto* cancel_button = new wxButton(this, wxID_CANCEL, _("Cancel"));
    sizer->Add(cancel_button, 0, wxALIGN_CENTER | wxBOTTOM, 10);

    SetSizerAndFit(sizer);
    CentreOnParent();

    Bind(wxEVT_TIMER, &PulseActionDialog::OnTimer, this);
    Bind(wxEVT_BUTTON, &PulseActionDialog::OnCancel, this, wxID_CANCEL);

    timer_.Start(poll_interval_ms);
}

void PulseActionDialog::OnTimer(wxTimerEvent&) {
    gauge_->Pulse();

    // Update elapsed time display.
    wxTimeSpan elapsed = wxDateTime::Now() - start_time_;
    int minutes = elapsed.GetMinutes();
    int seconds = elapsed.GetSeconds().ToLong() % 60;
    elapsed_text_->SetLabel(wxString::Format(_("Elapsed time: %02d:%02d"),
                                             minutes, seconds));

    // Invoke the user callback.
    wxString msg = message_text_->GetLabel();
    bool keep_going = callback_(msg);
    if (msg != message_text_->GetLabel()) {
        message_text_->SetLabel(msg);
        Fit();
    }

    if (!keep_going) {
        timer_.Stop();
        EndModal(wxID_OK);
    }
}

void PulseActionDialog::OnCancel(wxCommandEvent&) {
    timer_.Stop();
    cancelled_ = true;
    EndModal(wxID_CANCEL);
}

}  // namespace widgets
