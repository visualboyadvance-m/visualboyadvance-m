#include "qt/widgets/on-screen-controller.h"

#include <algorithm>
#include <cmath>

#include <QEvent>
#include <QFont>
#include <QImage>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QResizeEvent>
#include <QTouchEvent>

#include "qt/config/emulated-gamepad.h"

namespace widgets {

namespace {

// The player-1 joypad. On-screen input always drives the first pad.
const config::GameJoy kJoypad(0);

// The keys the on-screen controller manages. Every RecomputePressedKeys() pass
// touches exactly these, so a released control is always cleared.
constexpr std::array<config::GameKey, 10> kManagedKeys = {
    config::GameKey::Up,     config::GameKey::Down,  config::GameKey::Left,
    config::GameKey::Right,  config::GameKey::A,     config::GameKey::B,
    config::GameKey::L,      config::GameKey::R,     config::GameKey::Select,
    config::GameKey::Start,
};

// Overlay colors (all with alpha for the semi-transparent look).
const QColor kFill(255, 255, 255, 70);
const QColor kFillPressed(120, 200, 255, 170);
const QColor kBorder(0, 0, 0, 130);
const QColor kLabel(20, 20, 20, 200);

}  // namespace

OnScreenController::OnScreenController(QWidget* parent,
                                       config::EmulatedGamepad* gamepad,
                                       std::function<void()> on_menu)
    : QWidget(parent), gamepad_(gamepad), on_menu_(std::move(on_menu)) {
    // Transparent child: the game image shows through the gaps between the
    // controls, and nothing is erased behind them.
    setAttribute(Qt::WA_AcceptTouchEvents, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAutoFillBackground(false);
    setFocusPolicy(Qt::NoFocus);
    setMouseTracking(true);
    pressed_.fill(false);
    LayoutButtons();
}

OnScreenController::~OnScreenController() = default;

void OnScreenController::SetGameAspect(double aspect) {
    if (aspect == game_aspect_) {
        return;
    }
    game_aspect_ = aspect;
    LayoutButtons();
    ++revision_;
    update();
}

void OnScreenController::SetShowMenuButton(bool show) {
    if (show == show_menu_button_) {
        return;
    }
    show_menu_button_ = show;
    LayoutButtons();
    ++revision_;
    update();
}

void OnScreenController::LayoutButtons() {
    const int w = width();
    const int h = height();
    buttons_.clear();
    dpad_rect_ = QRect();
    if (w <= 0 || h <= 0) {
        return;
    }

    const int unit = std::min(w, h);
    const int m = std::max(4, static_cast<int>(unit * 0.03));

    // The pillarbox column beside the centered, aspect-fit game image. Wide
    // enough (the usual case on a landscape phone, much wider than the GBA's
    // 3:2), the controls all move into the two columns, leaving the picture
    // unobscured.
    int gutter = 0;
    if (game_aspect_ > 0) {
        const int game_w = std::min(w, static_cast<int>(h * game_aspect_));
        gutter = (w - game_w) / 2;
    }
    if (gutter >= static_cast<int>(unit * 0.25)) {
        LayoutButtonsSideColumns(w, h, gutter, m);
        return;
    }

    const int face = static_cast<int>(unit * 0.17);
    const int shoulder_w = static_cast<int>(w * 0.16);
    const int shoulder_h = static_cast<int>(h * 0.09);
    const int pill_w = static_cast<int>(w * 0.13);
    const int pill_h = static_cast<int>(h * 0.07);

    // Shoulder buttons along the top corners.
    buttons_.push_back({config::GameKey::L, false, Shape::kRoundedRect, QStringLiteral("L"),
                        QRect(m, m, shoulder_w, shoulder_h)});
    buttons_.push_back({config::GameKey::R, false, Shape::kRoundedRect, QStringLiteral("R"),
                        QRect(w - m - shoulder_w, m, shoulder_w, shoulder_h)});

    // Menu button directly below L, in the left edge strip the D-pad leaves
    // free above itself; kept off the picture's top center.
    if (show_menu_button_) {
        const int menu_h = static_cast<int>(h * 0.07);
        buttons_.push_back({config::GameKey::A /* unused */, true, Shape::kRoundedRect,
                            tr("MENU"), QRect(m, m + shoulder_h + m, shoulder_w, menu_h)});
    }

    // Start / Select, bottom center.
    buttons_.push_back({config::GameKey::Select, false, Shape::kPill, tr("SEL"),
                        QRect(w / 2 - pill_w - m / 2, h - m - pill_h, pill_w, pill_h)});
    buttons_.push_back({config::GameKey::Start, false, Shape::kPill, tr("START"),
                        QRect(w / 2 + m / 2, h - m - pill_h, pill_w, pill_h)});

    // Face buttons, bottom right (A upper-right, B lower-left of A).
    const int ax = w - m - face - static_cast<int>(w * 0.02);
    const int ay = h - m - face - static_cast<int>(h * 0.22);
    buttons_.push_back({config::GameKey::A, false, Shape::kCircle, QStringLiteral("A"),
                        QRect(ax, ay, face, face)});
    const int bx = ax - static_cast<int>(face * 1.15);
    const int by = ay + static_cast<int>(face * 0.9);
    buttons_.push_back({config::GameKey::B, false, Shape::kCircle, QStringLiteral("B"),
                        QRect(bx, by, face, face)});

    // D-pad, bottom left, square.
    const int dp = static_cast<int>(unit * 0.42);
    dpad_rect_ = QRect(m, h - m - dp, dp, dp);
}

void OnScreenController::LayoutButtonsSideColumns(int w, int h, int gutter, int margin) {
    const int m = margin;
    const int cw = gutter - 2 * m;       // usable width of each column
    const int right_x = w - gutter + m;  // left edge of the right column

    // Shoulders across the top of each column.
    const int shoulder_w = std::min(cw, static_cast<int>(h * 0.40));
    const int shoulder_h = static_cast<int>(h * 0.10);
    buttons_.push_back({config::GameKey::L, false, Shape::kRoundedRect, QStringLiteral("L"),
                        QRect(m + (cw - shoulder_w) / 2, m, shoulder_w, shoulder_h)});
    buttons_.push_back({config::GameKey::R, false, Shape::kRoundedRect, QStringLiteral("R"),
                        QRect(right_x + (cw - shoulder_w) / 2, m, shoulder_w, shoulder_h)});

    // Menu button directly below L, inside the left column so it never covers
    // the picture. The D-pad band below starts under it.
    int menu_h = 0;
    if (show_menu_button_) {
        menu_h = static_cast<int>(h * 0.07);
        buttons_.push_back({config::GameKey::A /* unused */, true, Shape::kRoundedRect,
                            tr("MENU"),
                            QRect(m + (cw - shoulder_w) / 2, m + shoulder_h + m, shoulder_w,
                                  menu_h)});
        menu_h += m;
    }

    // Select / Start along the bottom of the left / right column.
    const int pill_w = std::min(cw, static_cast<int>(h * 0.35));
    const int pill_h = static_cast<int>(h * 0.08);
    buttons_.push_back({config::GameKey::Select, false, Shape::kPill, tr("SEL"),
                        QRect(m + (cw - pill_w) / 2, h - m - pill_h, pill_w, pill_h)});
    buttons_.push_back({config::GameKey::Start, false, Shape::kPill, tr("START"),
                        QRect(right_x + (cw - pill_w) / 2, h - m - pill_h, pill_w, pill_h)});

    // The vertical band left free between the shoulders (plus the menu button
    // under L) and the pills. Both columns share it so A/B line up with the
    // D-pad.
    const int band_top = m + shoulder_h + m + menu_h;
    const int band_h = (h - m - pill_h - m) - band_top;
    if (band_h <= 0) {
        return;
    }

    // D-pad centered in the left band.
    const int dp = std::min({cw, band_h, static_cast<int>(h * 0.52)});
    dpad_rect_ = QRect(m + (cw - dp) / 2, band_top + (band_h - dp) / 2, dp, dp);

    // A/B centered in the right band (A upper-right, B lower-left of A).
    const int face = std::min(static_cast<int>(cw / 2.15), static_cast<int>(h * 0.20));
    const int cluster_w = static_cast<int>(face * 2.15);
    const int cluster_h = static_cast<int>(face * 1.9);
    const int cx = right_x + (cw - cluster_w) / 2;
    const int cy = band_top + (band_h - cluster_h) / 2;
    buttons_.push_back({config::GameKey::A, false, Shape::kCircle, QStringLiteral("A"),
                        QRect(cx + static_cast<int>(face * 1.15), cy, face, face)});
    buttons_.push_back({config::GameKey::B, false, Shape::kCircle, QStringLiteral("B"),
                        QRect(cx, cy + static_cast<int>(face * 0.9), face, face)});
}

void OnScreenController::HitTest(const QPoint& pos, PointerState* out) const {
    // D-pad: a point can produce a direction on each axis, giving diagonals.
    if (dpad_rect_.width() > 0 && dpad_rect_.contains(pos)) {
        const double cx = dpad_rect_.x() + dpad_rect_.width() / 2.0;
        const double cy = dpad_rect_.y() + dpad_rect_.height() / 2.0;
        const double dx = (pos.x() - cx) / (dpad_rect_.width() / 2.0);
        const double dy = (pos.y() - cy) / (dpad_rect_.height() / 2.0);
        constexpr double kThreshold = 0.30;
        if (dx < -kThreshold) {
            out->keys.insert(config::GameKey::Left);
        } else if (dx > kThreshold) {
            out->keys.insert(config::GameKey::Right);
        }
        if (dy < -kThreshold) {
            out->keys.insert(config::GameKey::Up);
        } else if (dy > kThreshold) {
            out->keys.insert(config::GameKey::Down);
        }
    }

    for (const Button& button : buttons_) {
        bool hit = false;
        if (button.shape == Shape::kCircle) {
            const double rx = button.rect.width() / 2.0;
            const double ry = button.rect.height() / 2.0;
            const double nx = (pos.x() - (button.rect.x() + rx)) / rx;
            const double ny = (pos.y() - (button.rect.y() + ry)) / ry;
            hit = (nx * nx + ny * ny) <= 1.0;
        } else {
            hit = button.rect.contains(pos);
        }
        if (!hit) {
            continue;
        }
        if (button.is_menu) {
            out->on_menu = true;
        } else {
            out->keys.insert(button.key);
        }
    }
}

void OnScreenController::RecomputePressedKeys() {
    std::array<bool, config::kNbGameKeys> desired{};
    for (const auto& entry : pointers_) {
        for (const config::GameKey key : entry.second.keys) {
            desired[static_cast<size_t>(key)] = true;
        }
    }

    bool changed = false;
    for (const config::GameKey key : kManagedKeys) {
        const size_t index = static_cast<size_t>(key);
        if (desired[index] != pressed_[index]) {
            pressed_[index] = desired[index];
            if (gamepad_) {
                gamepad_->SetGameKey(kJoypad, key, desired[index]);
            }
            changed = true;
        }
    }

    if (changed) {
        ++revision_;
    }
    update();
}

void OnScreenController::UpdatePointer(int pointer_id, const QPoint& pos) {
    PointerState state;
    HitTest(pos, &state);
    pointers_[pointer_id] = std::move(state);
    RecomputePressedKeys();
}

void OnScreenController::ReleasePointer(int pointer_id) {
    const auto iter = pointers_.find(pointer_id);
    if (iter == pointers_.end()) {
        return;
    }
    const bool fire_menu = iter->second.on_menu;
    pointers_.erase(iter);
    RecomputePressedKeys();

    // Fire the menu action only after releasing so it behaves like a tap.
    if (fire_menu && on_menu_) {
        on_menu_();
    }
}

void OnScreenController::OnPlatformTouch(int pointer_id, double x, double y, bool released) {
    const QPoint pos(static_cast<int>(x), static_cast<int>(y));
    if (released) {
        ReleasePointer(pointer_id);
    } else {
        UpdatePointer(pointer_id, pos);
    }
}

// ---- Events -----------------------------------------------------------------

bool OnScreenController::event(QEvent* ev) {
    switch (ev->type()) {
        case QEvent::TouchBegin:
        case QEvent::TouchUpdate:
        case QEvent::TouchEnd:
        case QEvent::TouchCancel: {
            auto* touch = static_cast<QTouchEvent*>(ev);
            const bool cancel = ev->type() == QEvent::TouchCancel;
            for (const QEventPoint& point : touch->points()) {
                const bool released =
                    cancel || point.state() == QEventPoint::State::Released;
                const QPointF pos = point.position();
                OnPlatformTouch(point.id(), pos.x(), pos.y(), released);
            }
            ev->accept();
            return true;
        }
        default:
            return QWidget::event(ev);
    }
}

void OnScreenController::resizeEvent(QResizeEvent* ev) {
    QWidget::resizeEvent(ev);
    LayoutButtons();
    ++revision_;
    update();
}

// ---- Painting ---------------------------------------------------------------

void OnScreenController::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    DrawContent(painter);
}

bool OnScreenController::RenderRgba(std::vector<uint8_t>* rgba, int* out_width,
                                    int* out_height, double scale) {
    if (width() < 1 || height() < 1) {
        return false;
    }
    if (scale <= 0.0) {
        scale = 1.0;
    }
    const int bitmap_w = std::max(1, static_cast<int>(width() * scale));
    const int bitmap_h = std::max(1, static_cast<int>(height() * scale));

    QImage image(bitmap_w, bitmap_h, QImage::Format_RGBA8888);
    image.fill(Qt::transparent);
    {
        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.scale(scale, scale);
        DrawContent(painter);
    }

    const size_t row = static_cast<size_t>(bitmap_w) * 4;
    rgba->resize(row * static_cast<size_t>(bitmap_h));
    for (int y = 0; y < bitmap_h; ++y) {
        std::copy_n(image.constScanLine(y), row, rgba->data() + row * y);
    }
    *out_width = bitmap_w;
    *out_height = bitmap_h;
    return true;
}

void OnScreenController::DrawContent(QPainter& painter) {
    const auto draw_shape = [&](const QRect& rect, bool round, bool pressed) {
        painter.setPen(QPen(kBorder, 2));
        painter.setBrush(pressed ? kFillPressed : kFill);
        if (round) {
            const double radius = std::min(rect.width(), rect.height()) / 2.0;
            painter.drawRoundedRect(rect, radius, radius);
        } else {
            painter.drawRect(rect);
        }
    };

    const auto draw_label = [&](const QRect& rect, const QString& label) {
        if (label.isEmpty()) {
            return;
        }
        QFont font = painter.font();
        font.setPixelSize(std::max(8, rect.height() / 3));
        font.setBold(true);
        painter.setFont(font);
        painter.setPen(kLabel);
        painter.drawText(rect, Qt::AlignCenter, label);
    };

    // Generic buttons.
    for (const Button& button : buttons_) {
        const bool pressed = !button.is_menu && pressed_[static_cast<size_t>(button.key)];
        switch (button.shape) {
            case Shape::kCircle:
                painter.setPen(QPen(kBorder, 2));
                painter.setBrush(pressed ? kFillPressed : kFill);
                painter.drawEllipse(button.rect);
                break;
            case Shape::kPill:
            case Shape::kRoundedRect:
                draw_shape(button.rect, /*round=*/true, pressed);
                break;
        }
        draw_label(button.rect, button.label);
    }

    // D-pad: a plus made of the two bars, with pressed arms highlighted.
    if (dpad_rect_.width() > 0) {
        const QRect& r = dpad_rect_;
        const int third = r.width() / 3;
        const QRect vertical(r.x() + third, r.y(), third, r.height());
        const QRect horizontal(r.x(), r.y() + third, r.width(), third);

        painter.setPen(QPen(kBorder, 2));
        painter.setBrush(kFill);
        painter.drawRoundedRect(vertical, third / 3.0, third / 3.0);
        painter.drawRoundedRect(horizontal, third / 3.0, third / 3.0);

        painter.setPen(Qt::NoPen);
        painter.setBrush(kFillPressed);
        if (pressed_[static_cast<size_t>(config::GameKey::Up)]) {
            painter.drawRect(vertical.x(), r.y(), third, r.height() / 2);
        }
        if (pressed_[static_cast<size_t>(config::GameKey::Down)]) {
            painter.drawRect(vertical.x(), r.y() + r.height() / 2, third, r.height() / 2);
        }
        if (pressed_[static_cast<size_t>(config::GameKey::Left)]) {
            painter.drawRect(r.x(), horizontal.y(), r.width() / 2, third);
        }
        if (pressed_[static_cast<size_t>(config::GameKey::Right)]) {
            painter.drawRect(r.x() + r.width() / 2, horizontal.y(), r.width() / 2, third);
        }
    }
}

// ---- Single-pointer mouse fallback -----------------------------------------

void OnScreenController::mousePressEvent(QMouseEvent* ev) {
    if (ev->button() != Qt::LeftButton || ev->source() != Qt::MouseEventNotSynthesized) {
        // Synthesized mouse events are the echo of a touch already handled.
        ev->ignore();
        return;
    }
    PointerState probe;
    HitTest(ev->pos(), &probe);
    if (probe.keys.empty() && !probe.on_menu) {
        // Not on a control: let the press through to the panel below.
        ev->ignore();
        return;
    }
    mouse_captured_ = true;
    UpdatePointer(kMousePointerId, ev->pos());
    ev->accept();
}

void OnScreenController::mouseMoveEvent(QMouseEvent* ev) {
    if (mouse_captured_ && (ev->buttons() & Qt::LeftButton)) {
        UpdatePointer(kMousePointerId, ev->pos());
        ev->accept();
    } else {
        ev->ignore();
    }
}

void OnScreenController::mouseReleaseEvent(QMouseEvent* ev) {
    if (!mouse_captured_) {
        ev->ignore();
        return;
    }
    mouse_captured_ = false;
    ReleasePointer(kMousePointerId);
    ev->accept();
}

void OnScreenController::leaveEvent(QEvent* ev) {
    if (!mouse_captured_) {
        ReleasePointer(kMousePointerId);
    }
    QWidget::leaveEvent(ev);
}

}  // namespace widgets
