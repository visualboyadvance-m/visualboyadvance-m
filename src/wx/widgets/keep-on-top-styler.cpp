#include "wx/widgets/keep-on-top-styler.h"

#include <wx/toplevel.h>

#include "core/base/check.h"
#include "wx/config/option.h"

namespace widgets {

KeepOnTopStyler::KeepOnTopStyler(wxTopLevelWindow* window)
    : window_(window),
      on_top_observer_(config::OptionID::kDispKeepOnTop,
                       std::bind(&KeepOnTopStyler::OnKeepOnTopChanged,
                                 this,
                                 std::placeholders::_1)) {
    VBAM_CHECK(window_);
    window_->Bind(wxEVT_SHOW, &KeepOnTopStyler::OnShow, this);
}

KeepOnTopStyler::~KeepOnTopStyler() {
    // Need to manually unbind to stop processing events for this window.
    window_->Unbind(wxEVT_SHOW, &KeepOnTopStyler::OnShow, this);
}

void KeepOnTopStyler::OnShow(wxShowEvent& show_event) {
    if (show_event.IsShown()) {
        // This must be called when the window is shown or it has no effect.
        OnKeepOnTopChanged(
            config::Option::ByID(config::OptionID::kDispKeepOnTop));
    }

    // Let the event propagate.
    show_event.Skip();
}

void KeepOnTopStyler::OnKeepOnTopChanged(config::Option* option) {
    if (option->GetBool()) {
        window_->SetWindowStyle(window_->GetWindowStyle() | wxSTAY_ON_TOP);
    } else {
        window_->SetWindowStyle(window_->GetWindowStyle() & ~wxSTAY_ON_TOP);
    }
}

}  // namespace widgets
