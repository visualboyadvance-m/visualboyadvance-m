#ifndef VBAM_QT_WIDGETS_ON_SCREEN_CONTROLLER_H_
#define VBAM_QT_WIDGETS_ON_SCREEN_CONTROLLER_H_

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <set>
#include <vector>

#include <QRect>
#include <QString>
#include <QWidget>

#include "qt/config/command.h"

class QMouseEvent;
class QPainter;
class QPaintEvent;
class QResizeEvent;
class QTouchEvent;

namespace config {
class EmulatedGamepad;
}  // namespace config

namespace widgets {

// A semi-transparent on-screen GBA controller drawn over the emulation panel.
//
// Presses map directly to game keys through EmulatedGamepad (bypassing the
// physical-input bindings), and the optional Menu button invokes a
// caller-provided callback (typically pops up the application menu, for when
// no menu bar is on screen; see SetShowMenuButton()). Multi-touch is handled
// through QTouchEvent so that, e.g., holding A while pressing the D-pad works;
// a single-pointer mouse path covers the desktop.
//
// When the caller reports the game image's aspect ratio via SetGameAspect()
// and the pillarbox columns beside the (centered, aspect-fit) picture are wide
// enough, the controls are laid out inside those columns so the picture stays
// unobscured; otherwise they overlay the corners of the whole widget.
//
// The widget is a transparent child stacked on top of the render panel inside
// the GameArea. It only reacts to touches that land on a control; presses
// elsewhere are ignored and fall through to the widget below.
class OnScreenController final : public QWidget {
    Q_OBJECT

public:
    OnScreenController(QWidget* parent,
                       config::EmulatedGamepad* gamepad,
                       std::function<void()> on_menu);
    ~OnScreenController() override;

    OnScreenController(const OnScreenController&) = delete;
    OnScreenController& operator=(const OnScreenController&) = delete;

    // Feeds a single touch point. `released` is true when the point is lifted
    // or the sequence is cancelled. Coordinates are in widget-local pixels.
    void OnPlatformTouch(int pointer_id, double x, double y, bool released);

    // Renders the overlay into straight-alpha RGBA at the widget's size times
    // `scale` (pass the device pixel ratio to match a native surface). Used by
    // renderers that present into their own layer and composite the controller
    // themselves. Returns false when the widget has no usable size.
    bool RenderRgba(std::vector<uint8_t>* rgba, int* out_width, int* out_height,
                    double scale = 1.0);

    // Bumped whenever the drawn appearance changes (press state, layout), so a
    // compositing renderer can skip re-rendering an unchanged overlay.
    uint32_t revision() const { return revision_; }

    // The aspect ratio (width / height) of the game image; zero or negative
    // (the default) keeps the corner-overlay layout.
    void SetGameAspect(double aspect);

    // Whether to lay out the Menu button (default true).
    void SetShowMenuButton(bool show);

protected:
    bool event(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    enum class Shape { kCircle, kRoundedRect, kPill };

    struct Button {
        config::GameKey key;  // Ignored when is_menu is true.
        bool is_menu;
        Shape shape;
        QString label;
        QRect rect;
    };

    // Per-pointer hit result.
    struct PointerState {
        std::set<config::GameKey> keys;
        bool on_menu = false;
    };

    void LayoutButtons();
    void LayoutButtonsSideColumns(int w, int h, int gutter, int margin);
    void HitTest(const QPoint& pos, PointerState* out) const;
    void RecomputePressedKeys();
    void UpdatePointer(int pointer_id, const QPoint& pos);
    void ReleasePointer(int pointer_id);
    void DrawContent(QPainter& painter);

    config::EmulatedGamepad* const gamepad_;
    const std::function<void()> on_menu_;

    std::vector<Button> buttons_;
    QRect dpad_rect_;

    double game_aspect_ = 0.0;
    bool show_menu_button_ = true;

    std::map<int, PointerState> pointers_;
    std::array<bool, config::kNbGameKeys> pressed_{};
    bool mouse_captured_ = false;

    uint32_t revision_ = 1;

    static constexpr int kMousePointerId = -1;
};

}  // namespace widgets

#endif  // VBAM_QT_WIDGETS_ON_SCREEN_CONTROLLER_H_
