#include "wx/widgets/slider-value-label.h"

#include <memory>
#include <vector>

#include <wx/button.h>
#include <wx/control.h>
#include <wx/event.h>
#include <wx/intl.h>
#include <wx/sizer.h>
#include <wx/slider.h>
#include <wx/stattext.h>
#include <wx/validate.h>

#include "wx/widgets/dpi-support.h"

namespace widgets {

namespace {

// Name of the value box, so a slider that already has one is recognized.
const char kValueLabelSuffix[] = "ValueLabel";

wxString ValueLabelName(const wxSlider* slider) {
    return slider->GetName() + kValueLabelSuffix;
}

// Width that fits the widest value the slider can show, so the box does not
// resize as digits are added and drag the layout with it.
int WidestValueWidth(wxControl* box, const wxSlider* slider, const wxString& suffix,
                     bool include_reset_label) {
    const wxString lo = wxString::Format("%d%s", slider->GetMin(), suffix);
    const wxString hi = wxString::Format("%d%s", slider->GetMax(), suffix);
    int width = wxMax(box->GetTextExtent(lo).x, box->GetTextExtent(hi).x);
    if (include_reset_label) {
        width = wxMax(width, box->GetTextExtent(_("[Default]")).x);
    }

    // Padding for the control's own border and internal margins.
    return width + FromDIP(16, box);
}

// Replaces the slider's sizer cell with a column holding the slider and `box`,
// preserving the cell's layout attributes so a wxFlexGridSizer keeps its shape.
void SwapInColumn(wxSlider* slider, wxSizer* sizer, size_t index, wxSizerItem* item,
                  wxWindow* box) {
    const int proportion = item->GetProportion();
    const int flag = item->GetFlag();
    const int border = item->GetBorder();

    sizer->Detach(slider);
    wxBoxSizer* const column = new wxBoxSizer(wxVERTICAL);
    column->Add(slider, 0, wxEXPAND);
    column->Add(box, 0, wxALIGN_CENTRE_HORIZONTAL | wxTOP, FromDIP(2, slider));
    sizer->Insert(index, column, proportion, flag, border);

    // Mutating an already-laid-out sizer does not reposition anything, and the
    // caller's Fit() is a no-op once the dialog has reached its final size, so
    // no size event follows to trigger a layout. Without this the box is left
    // at its construction origin, on top of the static box label.
    //
    // Every ancestor that owns a sizer is laid out, outermost first: a panel
    // can only grow once its parent has given it the room, so laying out just
    // the innermost one redistributes the old height and squeezes whichever box
    // comes last. Deferred to the next idle because a page usually carries more
    // than one slider, and laying out after each compresses the boxes still
    // being built.
    std::vector<wxWindow*> chain;
    for (wxWindow* w = slider->GetParent(); w; w = w->GetParent()) {
        // A wxStaticBox owns no sizer of its own; its parent panel does.
        if (w->GetSizer()) {
            chain.push_back(w);
        }
        if (w->IsTopLevel()) {
            break;
        }
    }
    if (!chain.empty()) {
        chain.front()->CallAfter([chain]() {
            for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
                (*it)->Layout();
            }
        });
    }
}

// Locates the slider's own item in its containing sizer. Returns false when the
// slider has no sizer, or already carries a box.
bool FindSliderCell(wxSlider* slider, wxSizer** sizer_out, size_t* index_out,
                    wxSizerItem** item_out) {
    if (!slider || slider->GetParent()->FindWindow(ValueLabelName(slider))) {
        return false;
    }
    wxSizer* const sizer = slider->GetContainingSizer();
    if (!sizer) {
        // Hand-placed slider with no sizer; there is nowhere to put the box.
        return false;
    }
    const wxSizerItemList& items = sizer->GetChildren();
    size_t index = 0;
    for (wxSizerItemList::const_iterator it = items.begin(); it != items.end(); ++it, ++index) {
        if ((*it)->IsWindow() && (*it)->GetWindow() == slider) {
            *sizer_out = sizer;
            *index_out = index;
            *item_out = *it;
            return true;
        }
    }
    return false;
}

}  // namespace

void AttachSliderDefaultButton(wxSlider* slider, std::function<void()> reset_to_default) {
    wxSizer* sizer = nullptr;
    size_t index = 0;
    wxSizerItem* item = nullptr;
    if (!reset_to_default || !FindSliderCell(slider, &sizer, &index, &item)) {
        return;
    }

    wxButton* const box =
        new wxButton(slider->GetParent(), wxID_ANY, _("Default"), wxDefaultPosition,
                     wxDefaultSize, wxBU_EXACTFIT, wxDefaultValidator, ValueLabelName(slider));

    // Follow the slider's enabled state; the box is a sibling window, so
    // Enable() on the slider does not reach it.
    box->Bind(wxEVT_UPDATE_UI, [slider, box](wxUpdateUIEvent& event) {
        if (box->IsEnabled() != slider->IsEnabled()) {
            box->Enable(slider->IsEnabled());
        }
        event.Skip();
    });
    box->Bind(wxEVT_BUTTON, [slider, reset_to_default](wxCommandEvent& event) {
        // See AttachSliderValueLabel(): the option setter is a no-op when the
        // value is unchanged, so the observer cannot be relied on to move the
        // slider back.
        reset_to_default();
        if (wxValidator* const validator = slider->GetValidator()) {
            validator->TransferToWindow();
        }
        event.Skip();
    });

    SwapInColumn(slider, sizer, index, item, box);
}

void AttachSliderValueLabel(wxSlider* slider, const wxString& suffix,
                            std::function<void()> reset_to_default) {
    wxSizer* sizer = nullptr;
    size_t index = 0;
    wxSizerItem* item = nullptr;
    if (!FindSliderCell(slider, &sizer, &index, &item)) {
        return;
    }

    // A button where there is a default to restore, a static text where there
    // is not. A read-only wxTextCtrl looks the part but still takes focus and
    // shows a blinking caret when clicked, and a button already carries the
    // click affordance, the hover feedback and the disabled greying.
    wxControl* box = nullptr;
    if (reset_to_default) {
        box = new wxButton(slider->GetParent(), wxID_ANY, wxEmptyString, wxDefaultPosition,
                           wxDefaultSize, wxBU_EXACTFIT, wxDefaultValidator,
                           ValueLabelName(slider));
    } else {
        box = new wxStaticText(slider->GetParent(), wxID_ANY, wxEmptyString, wxDefaultPosition,
                               wxDefaultSize, wxALIGN_CENTRE_HORIZONTAL | wxBORDER_SIMPLE);
        box->SetName(ValueLabelName(slider));
    }
    box->SetMinSize(
        wxSize(WidestValueWidth(box, slider, suffix, reset_to_default != nullptr), -1));

    auto refresh = [slider, box, suffix]() {
        const wxString text = wxString::Format("%d%s", slider->GetValue(), suffix);
        if (box->GetLabel() != text) {
            box->SetLabel(text);
        }
        // The box is a sibling window, so Enable() on the slider does not reach
        // it. Follow the slider instead of making every caller enable both.
        if (box->IsEnabled() != slider->IsEnabled()) {
            box->Enable(slider->IsEnabled());
        }
    };
    refresh();

    // wxEVT_SLIDER alone misses the intermediate positions of a thumb drag on
    // some ports, and neither fires when the value is set programmatically (a
    // validator loading the option, or a "reset" button). wxEVT_UPDATE_UI runs
    // on idle and catches whatever the other two do not.
    slider->Bind(wxEVT_SLIDER, [refresh](wxCommandEvent& event) {
        refresh();
        event.Skip();
    });
    slider->Bind(wxEVT_SCROLL_THUMBTRACK, [refresh](wxScrollEvent& event) {
        refresh();
        event.Skip();
    });
    box->Bind(wxEVT_UPDATE_UI, [refresh](wxUpdateUIEvent& event) {
        refresh();
        event.Skip();
    });

    if (reset_to_default) {
        // Hovering swaps the value for the reset affordance. The wxEVT_UPDATE_UI
        // refresh above would overwrite it on the next idle, so it is suspended
        // while the pointer is over the box.
        auto hovered = std::make_shared<bool>(false);
        box->Bind(wxEVT_UPDATE_UI, [hovered](wxUpdateUIEvent& event) {
            if (*hovered) {
                // Claim the event so the refresh handler below does not run.
                return;
            }
            event.Skip();
        });
        box->Bind(wxEVT_ENTER_WINDOW, [box, hovered](wxMouseEvent& event) {
            *hovered = true;
            box->SetLabel(_("[Default]"));
            event.Skip();
        });
        box->Bind(wxEVT_LEAVE_WINDOW, [hovered, refresh](wxMouseEvent& event) {
            *hovered = false;
            refresh();
            event.Skip();
        });
        box->Bind(wxEVT_BUTTON, [slider, refresh, reset_to_default](wxCommandEvent& event) {
            // The slider moves via its validator observing the option -- but
            // only when the option actually changed. Clicking Default with the
            // option already at its default is a no-op setter, no observer
            // runs, and a slider dragged since the last write stays put. Pull
            // the value in directly rather than relying on the notification.
            reset_to_default();
            if (wxValidator* const validator = slider->GetValidator()) {
                validator->TransferToWindow();
            }
            refresh();
            event.Skip();
        });
    }

    SwapInColumn(slider, sizer, index, item, box);
}

}  // namespace widgets
