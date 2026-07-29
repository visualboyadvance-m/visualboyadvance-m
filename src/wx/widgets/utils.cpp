#include "wx/widgets/utils.h"

#include <wx/display.h>

#if defined(__WXQT__) && defined(__ANDROID__)
#include <algorithm>
#include <set>
#include <vector>

#include <android/log.h>

#include <wx/dialog.h>
#include <wx/event.h>
#include <wx/scrolwin.h>
#include <wx/sizer.h>

#include "wx/android-compat.h"
#endif

namespace widgets {

wxRect GetDisplayRect() {
    wxRect display_rect;
    for (unsigned int i = 0; i < wxDisplay::GetCount(); i++) {
        display_rect.Union(wxDisplay(i).GetClientArea());
    }

    return display_rect;
}

#if defined(__WXQT__) && defined(__ANDROID__)

namespace {

// Name of the wrapper scroller, so a dialog that has already been adapted is
// recognized on the next call. Dialogs are re-adapted every time they are
// shown: the initialization code in guiinit.cpp calls Fit() after populating
// controls, and notebook tabs are parsed lazily, both of which change the size
// the content wants after the first adaptation.
const char kScrollerName[] = "vbamAndroidScroller";

// Every window laid out by `sizer`, recursively.
void CollectSizerWindows(const wxSizer* sizer, std::set<wxWindow*>* out) {
    const wxSizerItemList& items = sizer->GetChildren();
    for (wxSizerItemList::const_iterator it = items.begin(); it != items.end(); ++it) {
        const wxSizerItem* item = *it;
        if (item->IsWindow()) {
            out->insert(item->GetWindow());
        } else if (item->IsSizer()) {
            CollectSizerWindows(item->GetSizer(), out);
        }
    }
}

// The dialog's OK/Cancel row, if it has the standard one, plus the sizer holding
// it -- which is where it has to be detached from. Usually that is the top-level
// sizer, but not always (Logging.xrc nests the row one level deeper).
wxSizer* FindButtonSizer(wxSizer* sizer, wxSizer** owner) {
    const wxSizerItemList& items = sizer->GetChildren();
    for (wxSizerItemList::const_iterator it = items.begin(); it != items.end(); ++it) {
        const wxSizerItem* item = *it;
        if (!item->IsSizer()) {
            continue;
        }
        if (wxDynamicCast(item->GetSizer(), wxStdDialogButtonSizer)) {
            *owner = sizer;
            return item->GetSizer();
        }
        if (wxSizer* const found = FindButtonSizer(item->GetSizer(), owner)) {
            return found;
        }
    }
    return nullptr;
}

// How large a dialog may be, in wx logical pixels. The activity content view is
// authoritative where known: Qt's screen geometry does not subtract the action
// bar, and a dialog sized from it would hang off the bottom of the screen.
wxSize AvailableSize() {
    int w = 0, h = 0;
    if (VbamAndroidScreenClientSize(&w, &h)) {
        return wxSize(w, h);
    }
    const wxRect display_rect = GetDisplayRect();
    return wxSize(display_rect.width, display_rect.height);
}

// Set while a clamp is resizing a dialog, so the size event that resize
// generates does not start another one. Dialogs are only ever resized from the
// UI thread, so a single flag covers all of them.
bool g_clamping = false;

// Sizes an already-wrapped dialog: as large as its content wants, but never
// larger than the screen, with the scrollable area set to the content so
// whatever did not fit can be reached.
void ClampDialog(wxDialog* dialog, wxScrolledWindow* scroller) {
    wxSizer* const content = scroller->GetSizer();
    wxSizer* const outer = dialog->GetSizer();
    if (!content || !outer) {
        return;
    }

    // What the dialog would need to show everything at once. The scroller's own
    // minimum has to be the content's for this one measurement, and small again
    // afterwards: a scroller that reports the full content as its minimum would
    // force the dialog to be that big, which is the whole problem here.
    const wxSize content_size = content->CalcMin();
    scroller->SetMinSize(content_size);
    const wxSize needed = outer->ComputeFittingClientSize(dialog);
    scroller->SetMinSize(dialog->FromDIP(wxSize(48, 48)));

    // XRC gives top-level windows a minimum size fitting their content, which
    // would keep the dialog from shrinking below the screen. Drop it.
    dialog->SetSizeHints(wxDefaultSize, wxDefaultSize);
    dialog->SetMinSize(wxDefaultSize);

    // Leave room for whatever the window manager draws around the client area.
    const wxSize decorations = dialog->GetSize() - dialog->GetClientSize();
    const wxSize available = AvailableSize() - decorations;
    const wxSize client(std::min(needed.x, std::max(available.x, dialog->FromDIP(64))),
                        std::min(needed.y, std::max(available.y, dialog->FromDIP(64))));

    __android_log_print(ANDROID_LOG_INFO, "VBAM",
                        "AdaptDialog %s: content %dx%d needed %dx%d avail %dx%d -> %dx%d",
                        static_cast<const char*>(dialog->GetName().utf8_str()),
                        content_size.x, content_size.y, needed.x, needed.y,
                        available.x, available.y, client.x, client.y);

    g_clamping = true;
    dialog->SetClientSize(client);
    dialog->Layout();

    // The scrollable area is the content's own size, which is what makes the
    // dialog scroll when it did not all fit. Set it explicitly rather than with
    // FitInside(): the scroller's best size is deliberately tiny by now, so
    // FitInside() would settle on the visible size and never scroll anything.
    // Where the dialog ended up larger than the content, the content stretches
    // to fill it instead of leaving a gap.
    const wxSize visible = scroller->GetClientSize();
    scroller->SetVirtualSize(std::max(content_size.x, visible.x),
                             std::max(content_size.y, visible.y));
    scroller->Layout();

    // Now that it fits, make sure all of it is actually on screen: a dialog
    // placed by Qt, or offset from its parent, can still hang off an edge.
    const wxPoint pos = dialog->GetPosition();
    const wxSize outer_size = dialog->GetSize();
    const wxSize screen = AvailableSize();
    const wxPoint fitted(std::max(0, std::min(pos.x, screen.x - outer_size.x)),
                         std::max(0, std::min(pos.y, screen.y - outer_size.y)));
    if (fitted != pos) {
        dialog->Move(fitted);
    }
    g_clamping = false;
}

}  // namespace

bool AdaptDialogToScreen(wxDialog* dialog, bool wrap) {
    if (!dialog) {
        return false;
    }
    // Hand-built dialogs without a sizer have nothing to reflow.
    if (!dialog->GetSizer()) {
        __android_log_print(ANDROID_LOG_INFO, "VBAM", "AdaptDialog %s: no sizer, skipped",
                            static_cast<const char*>(dialog->GetName().utf8_str()));
        return false;
    }

    wxScrolledWindow* scroller =
        wxDynamicCast(dialog->FindWindow(kScrollerName), wxScrolledWindow);

    __android_log_print(ANDROID_LOG_INFO, "VBAM", "AdaptDialog %s: wrap=%d adapted=%d",
                        static_cast<const char*>(dialog->GetName().utf8_str()),
                        wrap ? 1 : 0, scroller ? 1 : 0);

    if (!scroller && !wrap) {
        return false;
    }

    if (!scroller) {
        // Move the whole XRC layout into a scroller, so content designed for a
        // desktop window stays reachable on a phone screen. The standard button
        // row stays out of it, pinned below, so OK/Cancel never scroll away.
        wxSizer* const content = dialog->GetSizer();
        wxSizer* buttons_owner = nullptr;
        wxSizer* const buttons = FindButtonSizer(content, &buttons_owner);

        std::set<wxWindow*> pinned;
        if (buttons) {
            CollectSizerWindows(buttons, &pinned);
        }

        // Snapshot the children before the scroller becomes one of them.
        std::vector<wxWindow*> content_windows;
        const wxWindowList& children = dialog->GetChildren();
        for (wxWindowList::const_iterator it = children.begin(); it != children.end(); ++it) {
            content_windows.push_back(*it);
        }

        scroller = new wxScrolledWindow(dialog, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                        wxVSCROLL | wxHSCROLL, kScrollerName);
        for (std::vector<wxWindow*>::iterator it = content_windows.begin();
             it != content_windows.end(); ++it) {
            if (pinned.count(*it) == 0) {
                (*it)->Reparent(scroller);
            }
        }

        if (buttons) {
            buttons_owner->Detach(buttons);
        }
        // Hand the XRC sizer over to the scroller without destroying it.
        dialog->SetSizer(nullptr, /*deleteOld=*/false);
        scroller->SetSizer(content);
        scroller->SetScrollRate(dialog->FromDIP(8), dialog->FromDIP(8));

        wxBoxSizer* const outer = new wxBoxSizer(wxVERTICAL);
        outer->Add(scroller, 1, wxEXPAND);
        if (buttons) {
            outer->Add(buttons, 0, wxEXPAND | wxALL, dialog->FromDIP(4));
        }
        dialog->SetSizer(outer);

        VbamEnableAndroidTouchScrolling(scroller->GetHandle());

        __android_log_print(ANDROID_LOG_INFO, "VBAM",
                            "AdaptDialog %s: wrapped %d children, buttons pinned=%d",
                            static_cast<const char*>(dialog->GetName().utf8_str()),
                            static_cast<int>(content_windows.size()), buttons ? 1 : 0);

        // Anything that resizes the dialog from here on gets clamped back.
        // Dialog code does size itself after being shown -- DisplayConfig's
        // AdjustSizeOnShow() runs from a CallAfter() and sizes to the content's
        // best height, and the viewers resize on user actions -- and reacting to
        // the resize itself catches all of it without every such place having to
        // know about the screen.
        wxScrolledWindow* const bound_scroller = scroller;
        dialog->Bind(wxEVT_SIZE, [dialog, bound_scroller](wxSizeEvent& event) {
            event.Skip();
            if (g_clamping) {
                return;
            }
            const wxSize size = dialog->GetSize();
            const wxSize available = AvailableSize();
            if (size.x > available.x || size.y > available.y) {
                ClampDialog(dialog, bound_scroller);
                return;
            }
            // Still fits: just keep the scrollable area in step with the new
            // size, so the content stretches into it or scrolls out of it.
            if (wxSizer* const content = bound_scroller->GetSizer()) {
                const wxSize content_size = content->CalcMin();
                const wxSize visible = bound_scroller->GetClientSize();
                bound_scroller->SetVirtualSize(std::max(content_size.x, visible.x),
                                               std::max(content_size.y, visible.y));
            }
        });

        // And once more after the window is really on screen. Qt applies its own
        // geometry when a window is first shown, and QDialog::exec() shows the
        // dialog itself, so the size we set before that can be overridden
        // without a wx size event ever reaching us.
        dialog->Bind(wxEVT_SHOW, [dialog, bound_scroller](wxShowEvent& event) {
            event.Skip();
            if (!event.IsShown()) {
                return;
            }
            dialog->CallAfter([dialog, bound_scroller] {
                ClampDialog(dialog, bound_scroller);
            });
        });
    }

    ClampDialog(dialog, scroller);
    return true;
}

#endif  // defined(__WXQT__) && defined(__ANDROID__)

}  // namespace widgets
