// The QuickDraw renderer (config::RenderMethod::kQuickDraw) for classic Mac OS.
//
// Each frame is converted from the panel's buffer (`todraw`, in whatever depth
// and border layout the filters left it; the same cases BuildImage() handles)
// straight into an offscreen 32-bit GWorld, and CopyBits() puts it into the
// window, stretched to the client area. The Simple renderer makes a wxImage,
// then a wxBitmap of it -- a new GWorld and a second conversion every frame --
// and draws that through wx.

#include <cmath>

#include <wx/dcclient.h>
#include <wx/log.h>
#include <wx/retro68/dc.h>

#include "core/base/system.h"
#include "wx/config/option-proxy.h"
#include "wx/drawing.h"
#include "wx/retro68/quickdraw-blit.h"

namespace {

inline uint32_t XRGB(uint32_t r, uint32_t g, uint32_t b) {
    return ((r & 0xFF) << 16) | ((g & 0xFF) << 8) | (b & 0xFF);
}

}  // namespace

QuickDrawDrawingPanel::QuickDrawDrawingPanel(wxWindow* parent, int _width, int _height)
    : BasicDrawingPanel(parent, _width, _height), surface_(new qdblit::Surface) {}

QuickDrawDrawingPanel::~QuickDrawDrawingPanel() = default;

void QuickDrawDrawingPanel::DrawArea(wxWindowDC& dc) {
    const int frame_width = static_cast<int>(std::ceil(width * scale));
    const int frame_height = static_cast<int>(std::ceil(height * scale));

    wxRetro68DCImpl* impl = wxDynamicCast(dc.GetImpl(), wxRetro68DCImpl);
    int stride = 0;
    uint32_t* pixels = nullptr;
    if (impl && impl->GetPort() && surface_->Resize(frame_width, frame_height))
        pixels = surface_->Lock(&stride);

    if (!pixels) {
        // No 32-bit GWorld (a Mac without 32-bit QuickDraw, or out of
        // memory): draw the way the Simple renderer does.
        if (!fallback_logged_) {
            wxLogDebug(wxT("QuickDraw renderer: no offscreen GWorld, drawing through wx"));
            fallback_logged_ = true;
        }
        BasicDrawingPanel::DrawArea(dc);
        return;
    }

    ConvertFrame(pixels, stride, frame_width, frame_height);
    surface_->Unlock();

    int client_width = 0;
    int client_height = 0;
    GetClientSize(&client_width, &client_height);

    // The window port, current and clipped the way wx clips this DC.
    impl->SelectPort();
    const wxPoint origin = impl->GetPortOffset();
    surface_->Blit(impl->GetPort(), origin.x, origin.y, origin.x + client_width,
                   origin.y + client_height);

    // The fore and back colours were changed behind wx's back.
    wxRetro68DCImpl::InvalidateCachedPortState();
}

void QuickDrawDrawingPanel::ConvertFrame(uint32_t* dst, int stride, int frame_width,
                                         int frame_height) {
    const bool filtered = OPTION(kDispFilter) != config::Filter::kNone;

    if (panel_color_depth_ == 24) {
        // R G B bytes. Without a filter there are no borders; with one, a row
        // of top border.
        const uint8_t* src = todraw + (filtered ? frame_width * 3 : 0);
        for (int y = 0; y < frame_height; y++, dst += stride) {
            for (int x = 0; x < frame_width; x++, src += 3)
                dst[x] = XRGB(src[0], src[1], src[2]);
        }
    } else if (panel_color_depth_ == 8) {
        // RGB332, with a 4-pixel right border (scaled) and a row of top border.
        const int border = static_cast<int>(std::ceil(4 * scale));
        const uint8_t* src = todraw + static_cast<int>(std::ceil((width + 4) * scale));
        for (int y = 0; y < frame_height; y++, dst += stride, src += border) {
            for (int x = 0; x < frame_width; x++, src++) {
                const uint8_t p = *src;
                // White color fix, as in BuildImage().
                dst[x] = p == 0xFF ? 0x00FFFFFF
                                   : XRGB(((p >> 5) & 0x7) << 5, ((p >> 2) & 0x7) << 5,
                                          (p & 0x3) << 6);
            }
        }
    } else if (panel_color_depth_ == 16) {
        // RGB555, with a 2-pixel right border (scaled) and a row of top border.
        const int border = static_cast<int>(std::ceil(2 * scale));
        const uint16_t* src = reinterpret_cast<const uint16_t*>(todraw) +
                              static_cast<int>(std::ceil((width + 2) * scale));
        for (int y = 0; y < frame_height; y++, dst += stride, src += border) {
            for (int x = 0; x < frame_width; x++, src++) {
                const uint16_t p = *src;
                dst[x] = XRGB(((p >> 10) & 0x1F) << 3, ((p >> 5) & 0x1F) << 3, (p & 0x1F) << 3);
            }
        }
    } else {
        // 32 bits laid out by systemRed/Green/BlueShift, with a 1-pixel right
        // border (scaled when a filter scaled the frame) and a row of top
        // border.
        const int border = filtered ? static_cast<int>(std::ceil(scale)) : 1;
        const uint32_t* src = reinterpret_cast<const uint32_t*>(todraw) +
                              static_cast<int>(std::ceil((width + 1) * scale));
        const int rs = systemRedShift - 3;
        const int gs = systemGreenShift - 3;
        const int bs = systemBlueShift - 3;
        for (int y = 0; y < frame_height; y++, dst += stride, src += border) {
            for (int x = 0; x < frame_width; x++, src++) {
                const uint32_t p = *src;
                dst[x] = XRGB(p >> rs, p >> gs, p >> bs);
            }
        }
    }
}
