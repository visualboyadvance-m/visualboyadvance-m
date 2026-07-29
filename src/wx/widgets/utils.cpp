#include "wx/widgets/utils.h"

#include <wx/display.h>

#if defined(__WXQT__) && defined(__ANDROID__)
#include <algorithm>
#include <set>
#include <vector>

#include <android/log.h>

#include <wx/button.h>
#include <wx/dialog.h>
#include <wx/event.h>
#include <wx/intl.h>
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

// Name of the close button added by AddAndroidCloseButton(), so a window that
// already has one is recognized.
const char kCloseButtonName[] = "vbamAndroidCloseButton";

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

// Clears the cached best size of a whole subtree, children first.
//
// Every window caches its best size, and a wxNotebook's cache still reflects
// the page count it had when it was first measured. The config dialogs build
// their notebook pages lazily -- and this runs from MainFrame::ShowModal(),
// before BaseDialog::Show() has force-loaded them -- so without this the dialog
// gets measured for an empty notebook and comes out a sliver tall.
// DisplayConfig::RefitForVisibilityChange() does the same thing for the same
// reason after it hides or shows controls.
void InvalidateBestSizeRecursively(wxWindow* window) {
    const wxWindowList& children = window->GetChildren();
    for (wxWindowList::const_iterator it = children.begin(); it != children.end(); ++it) {
        InvalidateBestSizeRecursively(*it);
    }
    window->InvalidateBestSize();
}

// Sizes an already-wrapped dialog: as large as its content wants, but never
// larger than the screen, with the scrollable area set to the content so
// whatever did not fit can be reached.
void ClampDialog(wxDialog* dialog, wxScrolledWindow* scroller) {
    wxSizer* const content = scroller->GetSizer();
    wxSizer* const outer = dialog->GetSizer();
    if (!content || !outer) {
        return;
    }

    // Measure against what the content is now, not what it was when some
    // window last cached its best size.
    InvalidateBestSizeRecursively(scroller);
    scroller->Layout();

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
    // Skip a no-op resize: a layout whose minimum depends on the width it was
    // given can otherwise ping-pong between two sizes.
    if (dialog->GetClientSize() != client) {
        dialog->SetClientSize(client);
    }
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

// Shrinks a top-level window that is larger than the screen and puts it at the
// screen's origin. ClampDialog() does this and much more for dialogs, whose
// content is reflowed into a scroller; a frame keeps its own layout and only
// needs to be no bigger than what it is displayed on.
void ClampFrame(wxWindow* window) {
    const wxSize available = AvailableSize();
    const wxSize size = window->GetSize();
    const wxSize fitted(std::min(size.x, available.x), std::min(size.y, available.y));
    if (fitted != size) {
        window->SetSize(fitted);
    }
    if (window->GetPosition() != wxPoint(0, 0)) {
        window->Move(0, 0);
    }
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
                // ... and into the viewport wx actually scrolls, which
                // wxWindow::Reparent() does not do by itself. Without this the
                // content is a sibling of the viewport rather than its child, so
                // scrolling moves the scrollbars and leaves the content behind.
                VbamReparentIntoAndroidViewport((*it)->GetHandle(), scroller->GetHandle());
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
            // Centered rather than expanded: a phone-width row of two buttons
            // pushed to the opposite edges reads as two unrelated controls, and
            // the bottom-center of the screen is where a thumb already is.
            outer->Add(buttons, 0, wxALIGN_CENTER_HORIZONTAL | wxALL, dialog->FromDIP(4));
        }
        dialog->SetSizer(outer);

        // Touch scrolling, plus the Qt-to-wx feedback that makes it move
        // anything: wx owns the position of the content inside the scroller, so
        // a scrollbar Qt moved by itself has to be replayed as a wx scroll.
        //
        // Every wx scroll path -- a dragged thumb, an arrow, a page click, the
        // kinetic drag -- ends in wxScrollHelper calling SetScrollPos(), i.e.
        // QScrollBar::setValue(), so this one hook sees all of them, and all of
        // them need the re-layout below.
        wxScrolledWindow* const scrolled = scroller;
        VbamEnableAndroidTouchScrolling(scroller->GetHandle(), [scrolled](int x, int y) {
            scrolled->Scroll(x, y);
            // Re-place the content at the new scrolled origin ourselves.
            //
            // wxScrollHelper moves the content by handing the scroll delta to
            // wxWindow::ScrollWindow(), which on wxQt is QWidget::scroll() on
            // the scroll area's viewport -- and that does not visibly move the
            // child widgets here: the scrollbar slides, wx's scroll position
            // updates, and the dialog keeps showing the same pixels until
            // something lays it out again. (Closing and reopening the dialog
            // used to be the only way to see the new position, since
            // ClampDialog() lays the scroller out on the way in.) Layout() is
            // wxScrolled's ScrollLayout(), which sets the content sizer's
            // dimension at CalcScrolledPosition(0, 0) -- exactly the placement
            // ScrollWindow() failed to do -- so doing it on every scroll makes
            // the content follow the scrollbar.
            scrolled->Layout();
            scrolled->Refresh();
        });

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
            // Authoritative in both directions, not just "shrink if too big".
            // Once the content lives in the scroller the dialog's own best size
            // is the scroller's deliberately tiny minimum, so anything sizing
            // itself from GetBestSize() -- DisplayConfig::AdjustSizeOnShow()
            // does exactly that -- collapses the dialog to a sliver. There is no
            // user window sizing on Android, so the fit always wins.
            ClampDialog(dialog, bound_scroller);
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

bool AddAndroidCloseButton(wxWindow* window) {
    if (!window) {
        return false;
    }
    if (window->FindWindow(kCloseButtonName)) {
        return true;
    }
    wxSizer* const sizer = window->GetSizer();
    if (!sizer) {
        __android_log_print(ANDROID_LOG_INFO, "VBAM", "CloseButton %s: no sizer, skipped",
                            static_cast<const char*>(window->GetName().utf8_str()));
        return false;
    }

    wxButton* const button = new wxButton(window, wxID_CANCEL, _("Close"), wxDefaultPosition,
                                          wxDefaultSize, 0, wxDefaultValidator,
                                          kCloseButtonName);
    wxStdDialogButtonSizer* const row = new wxStdDialogButtonSizer();
    row->Add(button, 0, wxALIGN_CENTER_VERTICAL);
    sizer->Add(row, 0, wxALIGN_CENTER_HORIZONTAL | wxALL, window->FromDIP(6));

    // A frame gets no wxID_CANCEL handling from wx, and closing it is what the
    // button is for. Bound on the button rather than the window so a dialog,
    // where wxDialog already does the right thing, is left alone.
    if (!wxDynamicCast(window, wxDialog)) {
        button->Bind(wxEVT_BUTTON, [window](wxCommandEvent&) { window->Close(); });

        // Nothing clamps a frame to the screen -- AdaptDialogToScreen() only
        // handles dialogs -- and the Lua windows ask for desktop-sized frames,
        // which would put this button past the bottom edge. Once at creation and
        // again on show, since Qt applies its own geometry when a window is first
        // shown and the screen may have rotated in between.
        ClampFrame(window);
        window->Bind(wxEVT_SHOW, [window](wxShowEvent& event) {
            event.Skip();
            if (event.IsShown()) {
                ClampFrame(window);
            }
        });
    }

    window->Layout();
    return true;
}

#endif  // defined(__WXQT__) && defined(__ANDROID__)

}  // namespace widgets
