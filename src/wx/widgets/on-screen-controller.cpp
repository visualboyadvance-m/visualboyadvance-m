#include "wx/widgets/on-screen-controller.h"

#include <algorithm>
#include <cmath>
#include <memory>

#include <wx/bitmap.h>
#include <wx/dcclient.h>
#include <wx/dcmemory.h>
#include <wx/graphics.h>
#include <wx/image.h>

#include "wx/config/emulated-gamepad.h"

#ifdef __WXQT__
#include <QtCore/QEvent>
#include <QtGui/QTouchEvent>
#include <QtWidgets/QWidget>
#endif

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
const wxColour kFill(255, 255, 255, 70);
const wxColour kFillPressed(120, 200, 255, 170);
const wxColour kBorder(0, 0, 0, 130);
const wxColour kLabel(20, 20, 20, 200);

#ifdef __WXQT__
// Forwards Qt multi-touch events to the owning OnScreenController. A plain
// QObject subclass overriding eventFilter needs no meta-object, so no moc.
class QtTouchFilter final : public QObject {
public:
    explicit QtTouchFilter(OnScreenController* owner) : owner_(owner) {}

protected:
    bool eventFilter(QObject* object, QEvent* event) override {
        switch (event->type()) {
            case QEvent::TouchBegin:
            case QEvent::TouchUpdate:
            case QEvent::TouchEnd:
            case QEvent::TouchCancel: {
                auto* touch = static_cast<QTouchEvent*>(event);
                const bool cancel = event->type() == QEvent::TouchCancel;
                for (const QEventPoint& point : touch->points()) {
                    const bool released =
                        cancel || point.state() == QEventPoint::State::Released;
                    const QPointF pos = point.position();
                    owner_->OnPlatformTouch(point.id(), pos.x(), pos.y(), released);
                }
                event->accept();
                return true;
            }
            default:
                return QObject::eventFilter(object, event);
        }
    }

private:
    OnScreenController* const owner_;
};
#endif  // __WXQT__

}  // namespace

wxBEGIN_EVENT_TABLE(OnScreenController, wxWindow)
    EVT_PAINT(OnScreenController::OnPaint)
    EVT_SIZE(OnScreenController::OnSize)
    EVT_ERASE_BACKGROUND(OnScreenController::OnEraseBackground)
    EVT_LEFT_DOWN(OnScreenController::OnMouseDown)
    EVT_LEFT_UP(OnScreenController::OnMouseUp)
    EVT_MOTION(OnScreenController::OnMouseMove)
    EVT_LEAVE_WINDOW(OnScreenController::OnMouseLeave)
wxEND_EVENT_TABLE()

OnScreenController::OnScreenController(wxWindow* parent,
                                      config::EmulatedGamepad* gamepad,
                                      std::function<void()> on_menu)
    : wxWindow(), gamepad_(gamepad), on_menu_(std::move(on_menu)) {
    // Request a transparent background before the native peer is created so the
    // game image shows through the gaps between buttons.
    SetBackgroundStyle(wxBG_STYLE_TRANSPARENT);
    Create(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize,
           wxTRANSPARENT_WINDOW | wxFULL_REPAINT_ON_RESIZE | wxWANTS_CHARS);

    pressed_.fill(false);
    LayoutButtons();

#ifdef __WXQT__
    if (auto* qwidget = reinterpret_cast<QWidget*>(GetHandle())) {
        qwidget->setAttribute(Qt::WA_AcceptTouchEvents, true);
        qwidget->setAttribute(Qt::WA_TranslucentBackground, true);
        qwidget->setAttribute(Qt::WA_NoSystemBackground, true);
        auto* filter = new QtTouchFilter(this);
        qwidget->installEventFilter(filter);
        touch_filter_ = filter;
    }
#endif
}

OnScreenController::~OnScreenController() {
#ifdef __WXQT__
    if (touch_filter_) {
        if (auto* qwidget = reinterpret_cast<QWidget*>(GetHandle())) {
            qwidget->removeEventFilter(static_cast<QObject*>(touch_filter_));
        }
        delete static_cast<QObject*>(touch_filter_);
        touch_filter_ = nullptr;
    }
#endif
}

void OnScreenController::LayoutButtons() {
    const wxSize sz = GetClientSize();
    const int w = sz.x;
    const int h = sz.y;
    buttons_.clear();
    dpad_rect_ = wxRect();
    if (w <= 0 || h <= 0) {
        return;
    }

    const int unit = std::min(w, h);
    const int face = static_cast<int>(unit * 0.17);
    const int shoulder_w = static_cast<int>(w * 0.16);
    const int shoulder_h = static_cast<int>(h * 0.09);
    const int pill_w = static_cast<int>(w * 0.13);
    const int pill_h = static_cast<int>(h * 0.07);
    const int menu_w = static_cast<int>(w * 0.14);
    const int menu_h = static_cast<int>(h * 0.07);
    const int m = std::max(4, static_cast<int>(unit * 0.03));

    // Shoulder buttons along the top corners.
    buttons_.push_back({config::GameKey::L, false, Shape::kRoundedRect, "L",
                        wxRect(m, m, shoulder_w, shoulder_h)});
    buttons_.push_back({config::GameKey::R, false, Shape::kRoundedRect, "R",
                        wxRect(w - m - shoulder_w, m, shoulder_w, shoulder_h)});

    // Menu button, top center.
    buttons_.push_back({config::GameKey::A /* unused */, true, Shape::kRoundedRect,
                        "MENU", wxRect((w - menu_w) / 2, m, menu_w, menu_h)});

    // Start / Select, bottom center.
    buttons_.push_back({config::GameKey::Select, false, Shape::kPill, "SEL",
                        wxRect(w / 2 - pill_w - m / 2, h - m - pill_h, pill_w, pill_h)});
    buttons_.push_back({config::GameKey::Start, false, Shape::kPill, "START",
                        wxRect(w / 2 + m / 2, h - m - pill_h, pill_w, pill_h)});

    // Face buttons, bottom right (A upper-right, B lower-left of A).
    const int ax = w - m - face - static_cast<int>(w * 0.02);
    const int ay = h - m - face - static_cast<int>(h * 0.22);
    buttons_.push_back({config::GameKey::A, false, Shape::kCircle, "A",
                        wxRect(ax, ay, face, face)});
    const int bx = ax - static_cast<int>(face * 1.15);
    const int by = ay + static_cast<int>(face * 0.9);
    buttons_.push_back({config::GameKey::B, false, Shape::kCircle, "B",
                        wxRect(bx, by, face, face)});

    // D-pad, bottom left, square.
    const int dp = static_cast<int>(unit * 0.42);
    dpad_rect_ = wxRect(m, h - m - dp, dp, dp);
}

void OnScreenController::HitTest(const wxPoint& pos, PointerState* out) const {
    // D-pad: a point can produce a direction on each axis, giving diagonals.
    if (dpad_rect_.width > 0 && dpad_rect_.Contains(pos)) {
        const double cx = dpad_rect_.x + dpad_rect_.width / 2.0;
        const double cy = dpad_rect_.y + dpad_rect_.height / 2.0;
        const double dx = (pos.x - cx) / (dpad_rect_.width / 2.0);
        const double dy = (pos.y - cy) / (dpad_rect_.height / 2.0);
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
            const double rx = button.rect.width / 2.0;
            const double ry = button.rect.height / 2.0;
            const double nx = (pos.x - (button.rect.x + rx)) / rx;
            const double ny = (pos.y - (button.rect.y + ry)) / ry;
            hit = (nx * nx + ny * ny) <= 1.0;
        } else {
            hit = button.rect.Contains(pos);
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
            gamepad_->SetGameKey(kJoypad, key, desired[index]);
            changed = true;
        }
    }

    if (changed) {
        ++revision_;
    }
    Refresh();
}

void OnScreenController::UpdatePointer(int pointer_id, const wxPoint& pos) {
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
    const wxPoint pos(static_cast<int>(x), static_cast<int>(y));
    if (released) {
        ReleasePointer(pointer_id);
    } else {
        UpdatePointer(pointer_id, pos);
    }
}

// ---- Painting ---------------------------------------------------------------

void OnScreenController::OnEraseBackground(wxEraseEvent&) {
    // Intentionally empty: keep the background transparent, avoid flicker.
}

void OnScreenController::OnPaint(wxPaintEvent&) {
    wxPaintDC dc(this);
    std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(dc));
    if (!gc) {
        return;
    }
    DrawContent(*gc);
}

bool OnScreenController::RenderRgba(std::vector<uint8_t>* rgba, int* out_width,
                                    int* out_height, double scale) {
    const wxSize sz = GetClientSize();
    if (sz.GetWidth() < 1 || sz.GetHeight() < 1) {
        return false;
    }
    if (scale <= 0.0) {
        scale = 1.0;
    }
    const int bitmap_w = std::max(1, static_cast<int>(sz.GetWidth() * scale));
    const int bitmap_h = std::max(1, static_cast<int>(sz.GetHeight() * scale));

    // Draw onto a fully transparent 32-bit bitmap, so the caller gets the same
    // overlay it would see on screen with the gaps between controls left clear.
    // `scale` renders it at device resolution for a compositing renderer.
    wxBitmap bitmap(bitmap_w, bitmap_h, 32);
    if (!bitmap.IsOk()) {
        return false;
    }
    bitmap.UseAlpha();
    {
        wxMemoryDC dc(bitmap);
        std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(dc));
        if (!gc) {
            return false;
        }
        // wxMemoryDC leaves the bitmap opaque-white on some backends; start from
        // transparent instead.
        gc->SetCompositionMode(wxCOMPOSITION_SOURCE);
        gc->SetBrush(wxBrush(wxColour(0, 0, 0, wxALPHA_TRANSPARENT)));
        gc->DrawRectangle(0, 0, bitmap_w, bitmap_h);
        gc->SetCompositionMode(wxCOMPOSITION_OVER);
        gc->Scale(scale, scale);
        DrawContent(*gc);
    }

    const wxImage image = bitmap.ConvertToImage();
    if (!image.IsOk()) {
        return false;
    }
    const int w = image.GetWidth();
    const int h = image.GetHeight();
    const unsigned char* rgb = image.GetData();
    const unsigned char* alpha = image.HasAlpha() ? image.GetAlpha() : nullptr;
    if (!rgb) {
        return false;
    }

    rgba->resize(static_cast<size_t>(w) * static_cast<size_t>(h) * 4);
    uint8_t* out = rgba->data();
    for (size_t i = 0, n = static_cast<size_t>(w) * static_cast<size_t>(h); i < n; ++i) {
        out[i * 4 + 0] = rgb[i * 3 + 0];
        out[i * 4 + 1] = rgb[i * 3 + 1];
        out[i * 4 + 2] = rgb[i * 3 + 2];
        out[i * 4 + 3] = alpha ? alpha[i] : 0xFF;
    }
    *out_width = w;
    *out_height = h;
    return true;
}

void OnScreenController::DrawContent(wxGraphicsContext& context) {
    wxGraphicsContext* const gc = &context;

    const auto draw_shape = [&](const wxRect& rect, bool round, bool pressed) {
        gc->SetPen(wxPen(kBorder, 2));
        gc->SetBrush(wxBrush(pressed ? kFillPressed : kFill));
        if (round) {
            const double radius = std::min(rect.width, rect.height) / 2.0;
            gc->DrawRoundedRectangle(rect.x, rect.y, rect.width, rect.height, radius);
        } else {
            gc->DrawRectangle(rect.x, rect.y, rect.width, rect.height);
        }
    };

    const auto draw_label = [&](const wxRect& rect, const wxString& label) {
        if (label.empty()) {
            return;
        }
        wxFont font(wxFontInfo(std::max(8, rect.height / 3)).Bold());
        gc->SetFont(font, kLabel);
        double tw = 0;
        double th = 0;
        gc->GetTextExtent(label, &tw, &th);
        gc->DrawText(label, rect.x + (rect.width - tw) / 2.0,
                     rect.y + (rect.height - th) / 2.0);
    };

    // Generic buttons.
    for (const Button& button : buttons_) {
        const bool pressed = !button.is_menu && pressed_[static_cast<size_t>(button.key)];
        switch (button.shape) {
            case Shape::kCircle: {
                gc->SetPen(wxPen(kBorder, 2));
                gc->SetBrush(wxBrush(pressed ? kFillPressed : kFill));
                gc->DrawEllipse(button.rect.x, button.rect.y, button.rect.width,
                                button.rect.height);
                break;
            }
            case Shape::kPill:
            case Shape::kRoundedRect:
                draw_shape(button.rect, /*round=*/true, pressed);
                break;
        }
        draw_label(button.rect, button.label);
    }

    // D-pad: a plus made of the two bars, with pressed arms highlighted.
    if (dpad_rect_.width > 0) {
        const wxRect& r = dpad_rect_;
        const int third = r.width / 3;
        const wxRect vertical(r.x + third, r.y, third, r.height);
        const wxRect horizontal(r.x, r.y + third, r.width, third);

        gc->SetPen(wxPen(kBorder, 2));
        gc->SetBrush(wxBrush(kFill));
        gc->DrawRoundedRectangle(vertical.x, vertical.y, vertical.width, vertical.height,
                                 third / 3.0);
        gc->DrawRoundedRectangle(horizontal.x, horizontal.y, horizontal.width,
                                 horizontal.height, third / 3.0);

        gc->SetPen(*wxTRANSPARENT_PEN);
        gc->SetBrush(wxBrush(kFillPressed));
        if (pressed_[static_cast<size_t>(config::GameKey::Up)]) {
            gc->DrawRectangle(vertical.x, r.y, third, r.height / 2);
        }
        if (pressed_[static_cast<size_t>(config::GameKey::Down)]) {
            gc->DrawRectangle(vertical.x, r.y + r.height / 2, third, r.height / 2);
        }
        if (pressed_[static_cast<size_t>(config::GameKey::Left)]) {
            gc->DrawRectangle(r.x, horizontal.y, r.width / 2, third);
        }
        if (pressed_[static_cast<size_t>(config::GameKey::Right)]) {
            gc->DrawRectangle(r.x + r.width / 2, horizontal.y, r.width / 2, third);
        }
    }
}

void OnScreenController::OnSize(wxSizeEvent& event) {
    LayoutButtons();
    ++revision_;
    Refresh();
    event.Skip();
}

// ---- Single-pointer mouse fallback -----------------------------------------

void OnScreenController::OnMouseDown(wxMouseEvent& event) {
    PointerState probe;
    HitTest(event.GetPosition(), &probe);
    if (probe.keys.empty() && !probe.on_menu) {
        // Not on a control: let the event through (cursor / menu-bar logic).
        event.Skip();
        return;
    }
    if (!HasCapture()) {
        CaptureMouse();
    }
    UpdatePointer(kMousePointerId, event.GetPosition());
}

void OnScreenController::OnMouseMove(wxMouseEvent& event) {
    if (HasCapture() && event.Dragging() && event.LeftIsDown()) {
        UpdatePointer(kMousePointerId, event.GetPosition());
    } else {
        event.Skip();
    }
}

void OnScreenController::OnMouseUp(wxMouseEvent& event) {
    if (HasCapture()) {
        ReleaseMouse();
    }
    ReleasePointer(kMousePointerId);
    event.Skip();
}

void OnScreenController::OnMouseLeave(wxMouseEvent& event) {
    if (!HasCapture()) {
        ReleasePointer(kMousePointerId);
    }
    event.Skip();
}

}  // namespace widgets
