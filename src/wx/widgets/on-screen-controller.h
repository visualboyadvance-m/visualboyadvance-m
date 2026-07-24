#ifndef VBAM_WX_WIDGETS_ON_SCREEN_CONTROLLER_H_
#define VBAM_WX_WIDGETS_ON_SCREEN_CONTROLLER_H_

#include <array>
#include <functional>
#include <map>
#include <set>
#include <vector>

#include <wx/window.h>

#include "wx/config/command.h"

// Forward declarations.
namespace config {
class EmulatedGamepad;
}  // namespace config

class wxEraseEvent;
class wxMouseEvent;
class wxPaintEvent;
class wxSizeEvent;

namespace widgets {

// A semi-transparent on-screen GBA controller drawn over the emulation panel.
//
// Presses map directly to game keys through EmulatedGamepad (bypassing the
// physical-input bindings), and the dedicated Menu button invokes a
// caller-provided callback (typically pops up the application menu, since a
// touch device has no menu bar). Real multi-touch is used on the wxQt/Android
// backend so that, e.g., holding A while pressing the D-pad works; other
// platforms fall back to a single-pointer mouse emulation.
//
// The widget is a transparent child window meant to be stacked on top of the
// render panel inside the GameArea. It only reacts to touches that land on a
// control; taps elsewhere are ignored.
class OnScreenController final : public wxWindow {
public:
    OnScreenController(wxWindow* parent,
                       config::EmulatedGamepad* gamepad,
                       std::function<void()> on_menu);
    ~OnScreenController() final;

    // Disable copy and copy assignment.
    OnScreenController(const OnScreenController&) = delete;
    OnScreenController& operator=(const OnScreenController&) = delete;

    // Called by the platform touch backend (wxQt) for a single touch point.
    // `released` is true when the point is lifted or the sequence is cancelled.
    // Coordinates are in widget-local pixels.
    void OnPlatformTouch(int pointer_id, double x, double y, bool released);

private:
    enum class Shape { kCircle, kRoundedRect, kPill };

    struct Button {
        config::GameKey key;  // Ignored when is_menu is true.
        bool is_menu;
        Shape shape;
        wxString label;
        wxRect rect;
    };

    // Per-pointer hit result.
    struct PointerState {
        std::set<config::GameKey> keys;
        bool on_menu = false;
    };

    // Recomputes button rectangles from the current client size.
    void LayoutButtons();

    // Maps a point to the controls under it.
    void HitTest(const wxPoint& pos, PointerState* out) const;

    // Applies the union of all active pointers to the emulated gamepad.
    void RecomputePressedKeys();

    // Pointer lifecycle helpers, shared by mouse and touch paths.
    void UpdatePointer(int pointer_id, const wxPoint& pos);
    void ReleasePointer(int pointer_id);

    void OnPaint(wxPaintEvent& event);
    void OnSize(wxSizeEvent& event);
    void OnEraseBackground(wxEraseEvent& event);

    // Single-pointer mouse fallback (desktop).
    void OnMouseDown(wxMouseEvent& event);
    void OnMouseUp(wxMouseEvent& event);
    void OnMouseMove(wxMouseEvent& event);
    void OnMouseLeave(wxMouseEvent& event);

    config::EmulatedGamepad* const gamepad_;
    const std::function<void()> on_menu_;

    // The generic buttons (face, shoulder, start/select, menu).
    std::vector<Button> buttons_;
    // The D-pad is handled specially (a point maps to up to two directions).
    wxRect dpad_rect_;

    std::map<int, PointerState> pointers_;
    std::array<bool, config::kNbGameKeys> pressed_{};

    static constexpr int kMousePointerId = -1;

#ifdef __WXQT__
    // Opaque pointer to the installed Qt touch event filter (QObject).
    void* touch_filter_ = nullptr;
#endif

    wxDECLARE_EVENT_TABLE();
};

}  // namespace widgets

#endif  // VBAM_WX_WIDGETS_ON_SCREEN_CONTROLLER_H_
