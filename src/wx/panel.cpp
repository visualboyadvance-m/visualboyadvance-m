#if defined(__APPLE__) && defined(__MACH__)
#define GL_SILENCE_DEPRECATION 1
#endif

#include "wx/wxvbam.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

#ifdef __WXGTK__
    #include <X11/Xlib.h>
    #include <X11/Xutil.h>
    #define Status int
    #include <gdk/gdkx.h>

#ifndef NO_WAYLAND
    #include <gdk/gdkwayland.h>
    #include <wayland-client.h>
#ifdef HAVE_WAYLAND_VIEWPORTER
    #include "viewporter-client-protocol.h"
#endif
    // For the software "Simple" Wayland HDR path's shm buffers.
    #include <sys/mman.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <cstdlib>
#endif

    #include <gtk/gtk.h>
    // For Wayland EGL.
    #ifdef HAVE_EGL
        #include <EGL/egl.h>
        #include <EGL/eglext.h>
    #endif
#ifdef HAVE_XSS
    #include <X11/extensions/scrnsaver.h>
#endif
#endif

#ifdef ENABLE_SDL3
#include <SDL3/SDL.h>
#else
#include <SDL.h>
#endif

#include <wx/dcbuffer.h>
#include <wx/menu.h>

#include "components/draw_text/draw_text.h"
#include "components/filters/filters.h"
#include "components/filters_agb/filters_agb.h"
#include "components/filters_cgb/filters_cgb.h"
#include "components/filters_interframe/interframe.h"
#include "components/filters_scalefx/scalefx.h"
#include "core/base/check.h"
#include "core/base/file_util.h"
#include "core/base/image_util.h"
#include "core/base/patch.h"
#include "core/base/system.h"
#include "core/base/version.h"
#include "core/gb/gb.h"
#include "core/gb/gbCheats.h"
#include "core/gb/gbGlobals.h"
#include "core/gb/gbPrinter.h"
#include "core/gb/gbSound.h"
#include "core/gba/gbaCheats.h"
#include "core/gba/gbaEeprom.h"
#include "core/gba/gbaFlash.h"
#include "core/gba/gbaGlobals.h"
#include "core/gba/gbaPrint.h"
#include "core/gba/gbaRtc.h"
#include "core/gba/gbaSound.h"
#include "wx/background-input.h"
#include "wx/config/cmdtab.h"
#include "wx/config/emulated-gamepad.h"
#include "wx/config/option-id.h"
#include "wx/config/option-proxy.h"
#include "wx/config/option.h"
#include "wx/drawing.h"

#if !defined(NO_VULKAN) && (defined(__WXMSW__) || defined(__WXGTK__))
#include <wx/dynlib.h>  // wxDynamicLibrary, for run-time Vulkan loader resolution
#endif

#ifndef NO_WAYLAND
#include "wx/wayland.h"
#endif

#include "wx/widgets/render-plugin.h"
#include "wx/widgets/user-input-event.h"

#ifdef __WXMSW__
#include <windows.h>
#endif

#ifdef _MSC_VER
#define strdup _strdup
#endif

namespace {

double GetFilterScale() {
    switch (OPTION(kDispFilter)) {
        case config::Filter::kNone:
            return 1.0;
        case config::Filter::kSuper2xsai:
        case config::Filter::kSupereagle:
        case config::Filter::kPixelate:
        case config::Filter::kAdvmame:
        case config::Filter::kBilinearplus:
        case config::Filter::kScanlines:
        case config::Filter::kTvmode:
        case config::Filter::kLQ2x:
        case config::Filter::kXbrz2x:
            return 2.0;
        case config::Filter::kSimple4x:
        case config::Filter::kHQ4x:
            return 4.0;
        case config::Filter::kXbrz6x:
            return 6.0;
        case config::Filter::kXbrz9x:
        case config::Filter::kScaleFX9x:
            return 9.0;
        case config::Filter::kScaleFX3x:
            return 3.0;
        case config::Filter::kPlugin:
        case config::Filter::kLast:
            VBAM_NOTREACHED_RETURN(1.0);
    }
    VBAM_NOTREACHED_RETURN(1.0);
}

// Maximum number of filter threads, capped at 6 to avoid diminishing returns
// and potential issues with filter state sharing.
int GetMaxFilterThreads() {
    unsigned int hw_threads = std::thread::hardware_concurrency();
    if (hw_threads == 0)
        hw_threads = 1;  // Fallback if hardware_concurrency fails
    return static_cast<int>(std::min(hw_threads, 8u));
}

// Filter context radius - max rows above/below a source row that filters examine
// 2xSaI family needs 2, most others need 1, some need 0
constexpr int kFilterContextRadius = 2;

// Extra rows for seam processing margin
constexpr int kSeamMarginRows = 1;

// Total source rows to process for each seam band (above + below seam)
[[maybe_unused]] constexpr int kSeamBandSourceRows = (kFilterContextRadius + kSeamMarginRows) * 2;

// Serialize direct (non-proxy) RPI plugin calls. Most RPI plugins have
// non-thread-safe global state — the 64-bit proxy host already serializes calls
// via its own critical section, but the 32-bit in-process path calls the
// plugin directly from whichever filter thread, and concurrent calls corrupt
// the plugin's internals (see 2xBRZ, whose "MT" sibling is a separate build).
static std::mutex g_rpi_output_mutex;

// Apply the currently selected 32-bit filter to the image region.
// This is the core filter dispatch function used by both threaded rendering
// and seam smoothing. Plugin filters are not supported here.
void ApplyFilter32(uint8_t* src, int instride, uint8_t* delta, uint8_t* dst,
                   int outstride, int width, int height) {
    switch (OPTION(kDispFilter)) {
        case config::Filter::kSuper2xsai:
            Super2xSaI32(src, instride, delta, dst, outstride, width, height);
            break;
        case config::Filter::kSupereagle:
            SuperEagle32(src, instride, delta, dst, outstride, width, height);
            break;
        case config::Filter::kPixelate:
            Pixelate32(src, instride, delta, dst, outstride, width, height);
            break;
        case config::Filter::kAdvmame:
            AdMame2x32(src, instride, delta, dst, outstride, width, height);
            break;
        case config::Filter::kBilinearplus:
            BilinearPlus32(src, instride, delta, dst, outstride, width, height);
            break;
        case config::Filter::kScanlines:
            Scanlines32(src, instride, delta, dst, outstride, width, height);
            break;
        case config::Filter::kTvmode:
            ScanlinesTV32(src, instride, delta, dst, outstride, width, height);
            break;
        case config::Filter::kLQ2x:
            lq2x32(src, instride, delta, dst, outstride, width, height);
            break;
        case config::Filter::kSimple4x:
            Simple4x32(src, instride, delta, dst, outstride, width, height);
            break;
        case config::Filter::kHQ4x:
            hq4x32_32(src, instride, delta, dst, outstride, width, height);
            break;
        case config::Filter::kXbrz2x:
            xbrz2x32(src, instride, delta, dst, outstride, width, height);
            break;
        case config::Filter::kXbrz6x:
            xbrz6x32(src, instride, delta, dst, outstride, width, height);
            break;
        case config::Filter::kXbrz9x:
            xbrz9x32(src, instride, delta, dst, outstride, width, height);
            break;
        case config::Filter::kScaleFX3x:
            scalefx3x32(src, instride, delta, dst, outstride, width, height);
            break;
        case config::Filter::kScaleFX9x:
            scalefx9x32(src, instride, delta, dst, outstride, width, height);
            break;
        case config::Filter::kPlugin:
            // Plugin filters require RENDER_PLUGIN_INFO, not supported here
            break;
        case config::Filter::kNone:
        case config::Filter::kLast:
            break;
    }
}

// Re-process a band around a seam with full source context to eliminate artifacts.
// This is called after all filter threads complete to fix the visible seams at
// thread boundaries caused by filters not having access to neighboring filtered pixels.
//
// src: full source buffer (pointing to actual image data, after top border row)
// dst: full destination buffer (pointing to actual output, after top border rows)
// bandStart, bandEnd: source row range to process (pre-calculated, may be merged)
// All other params are for the full image, not a slice.
void ProcessSeamBand(uint8_t* src, int srcPitch, uint8_t* dst, int dstPitch,
                     int width, int fullHeight, int bandStart, int bandEnd, int scale) {
    (void)fullHeight;  // Reserved for future use
    const int bandHeight = bandEnd - bandStart;
    if (bandHeight <= 0) return;

    const bool is_8bit = (systemColorDepth == 8);
    const bool is_16bit = (systemColorDepth == 16);
    const bool is_24bit = (systemColorDepth == 24);
    const bool is_32bit = !is_8bit && !is_16bit && !is_24bit;

    const int outBandHeight = bandHeight * scale;
    const int outRowBytes = width * 4 * scale;

    // Allocate temp buffer for band output (always 32bpp internally)
    std::vector<uint8_t> tempBuf(outRowBytes * outBandHeight);

    if (is_32bit) {
        // Direct 32bpp path - no conversion needed
        uint8_t* bandSrc = src + srcPitch * bandStart;

        ApplyFilter32(bandSrc, srcPitch, nullptr, tempBuf.data(), outRowBytes,
                      width, bandHeight);

        // Copy result to destination (skip context rows at edges)
        const int skipRows = kFilterContextRadius * scale;
        const int copyRows = outBandHeight - 2 * skipRows;
        const int dstStartY = (bandStart + kFilterContextRadius) * scale;

        if (copyRows > 0) {
            uint8_t* dstRow = dst + dstPitch * dstStartY;
            const uint8_t* srcRow = tempBuf.data() + outRowBytes * skipRows;

            for (int y = 0; y < copyRows; y++) {
                memcpy(dstRow, srcRow, width * 4 * scale);
                dstRow += dstPitch;
                srcRow += outRowBytes;
            }
        }
    } else {
        // Non-32bpp: convert band to 32bpp, apply filter, convert back
        // Save current color shift values
        const int saved_red_shift = systemRedShift;
        const int saved_green_shift = systemGreenShift;
        const int saved_blue_shift = systemBlueShift;
        const int saved_rgb_low_bits_mask = RGB_LOW_BITS_MASK;

        // Set shift values for 32bpp format that filters expect
#if wxBYTE_ORDER == wxLITTLE_ENDIAN
        systemRedShift = 19;
        systemGreenShift = 11;
        systemBlueShift = 3;
        RGB_LOW_BITS_MASK = 0x00010101;
#else
        systemRedShift = 27;
        systemGreenShift = 19;
        systemBlueShift = 11;
        RGB_LOW_BITS_MASK = 0x01010100;
#endif

        // For 32bpp conversion: filters need left+right borders of 1 each
        const int total_width32 = width + 2;
        const int instride32 = total_width32 * 4;

        // Allocate 32bpp conversion buffers
        std::vector<uint32_t> src32((total_width32) * (bandHeight + 3));

        // Convert source band to 32bpp
        int pos = total_width32;  // Start at row 1 (skip row 0 for top border)
        uint8_t* bandSrcRow = src + srcPitch * bandStart;

        if (is_8bit) {
            for (int y = 0; y < bandHeight; y++) {
                const int left_border_pos = pos++;
                for (int x = 0; x < width; x++) {
                    const uint8_t src_val = bandSrcRow[x];
                    const uint8_t r3 = (src_val >> 5) & 0x7;
                    const uint8_t g3 = (src_val >> 2) & 0x7;
                    const uint8_t b2 = src_val & 0x3;
                    const uint8_t r8 = (r3 << 5) | (r3 << 2) | (r3 >> 1);
                    const uint8_t g8 = (g3 << 5) | (g3 << 2) | (g3 >> 1);
                    const uint8_t b8 = (b2 << 6) | (b2 << 4) | (b2 << 2) | b2;
#if wxBYTE_ORDER == wxLITTLE_ENDIAN
                    src32[pos++] = b8 | (g8 << 8) | (r8 << 16);
#else
                    src32[pos++] = (r8 << 24) | (g8 << 16) | (b8 << 8);
#endif
                }
                src32[left_border_pos] = src32[left_border_pos + 1];
                src32[pos] = src32[pos - 1];
                pos++;
                bandSrcRow += srcPitch;
            }
        } else if (is_16bit) {
            for (int y = 0; y < bandHeight; y++) {
                const uint16_t* src16 = reinterpret_cast<const uint16_t*>(bandSrcRow);
                const int left_border_pos = pos++;
                for (int x = 0; x < width; x++) {
                    const uint16_t src_val = src16[x];
                    const uint8_t r5 = (src_val >> 10) & 0x1f;
                    const uint8_t g5 = (src_val >> 5) & 0x1f;
                    const uint8_t b5 = src_val & 0x1f;
                    const uint8_t r8 = (r5 << 3) | (r5 >> 2);
                    const uint8_t g8 = (g5 << 3) | (g5 >> 2);
                    const uint8_t b8 = (b5 << 3) | (b5 >> 2);
#if wxBYTE_ORDER == wxLITTLE_ENDIAN
                    src32[pos++] = b8 | (g8 << 8) | (r8 << 16);
#else
                    src32[pos++] = (r8 << 24) | (g8 << 16) | (b8 << 8);
#endif
                }
                src32[left_border_pos] = src32[left_border_pos + 1];
                src32[pos] = src32[pos - 1];
                pos++;
                bandSrcRow += srcPitch;
            }
        } else {  // is_24bit
            for (int y = 0; y < bandHeight; y++) {
                const int left_border_pos = pos++;
                for (int x = 0; x < width; x++) {
                    const int src_pos = x * 3;
#if wxBYTE_ORDER == wxLITTLE_ENDIAN
                    src32[pos++] = bandSrcRow[src_pos] | (bandSrcRow[src_pos+1] << 8) | (bandSrcRow[src_pos+2] << 16);
#else
                    src32[pos++] = (bandSrcRow[src_pos] << 24) | (bandSrcRow[src_pos+1] << 16) | (bandSrcRow[src_pos+2] << 8);
#endif
                }
                src32[left_border_pos] = src32[left_border_pos + 1];
                src32[pos] = src32[pos - 1];
                pos++;
                bandSrcRow += srcPitch;
            }
        }

        // Fill top border row by duplicating row 1
        memcpy(src32.data(), src32.data() + total_width32, total_width32 * sizeof(uint32_t));

        // Fill bottom 2 extra rows by duplicating the last image row
        const int last_row_offset = total_width32 * bandHeight;
        memcpy(src32.data() + last_row_offset + total_width32, src32.data() + last_row_offset, total_width32 * sizeof(uint32_t));
        memcpy(src32.data() + last_row_offset + total_width32 * 2, src32.data() + last_row_offset, total_width32 * sizeof(uint32_t));

        // Point to first image pixel (skip top row + left border)
        uint8_t* filter_src = reinterpret_cast<uint8_t*>(src32.data() + total_width32 + 1);

        // Apply filter
        ApplyFilter32(filter_src, instride32, nullptr, tempBuf.data(), outRowBytes,
                      width, bandHeight);

        // Convert 32bpp output back to original depth and copy to destination
        const int skipRows = kFilterContextRadius * scale;
        const int copyRows = outBandHeight - 2 * skipRows;
        const int dstStartY = (bandStart + kFilterContextRadius) * scale;
        const int scaled_width = width * scale;

        if (copyRows > 0) {
            const uint32_t* src32_row = reinterpret_cast<const uint32_t*>(tempBuf.data() + outRowBytes * skipRows);

            if (is_8bit) {
                uint8_t* dstRow = dst + dstPitch * dstStartY;
                for (int y = 0; y < copyRows; y++) {
                    for (int x = 0; x < scaled_width; x++) {
                        const uint32_t color = src32_row[x];
#if wxBYTE_ORDER == wxLITTLE_ENDIAN
                        const uint8_t r8 = (color >> 16) & 0xff;
                        const uint8_t g8 = (color >> 8) & 0xff;
                        const uint8_t b8 = color & 0xff;
#else
                        const uint8_t r8 = (color >> 24) & 0xff;
                        const uint8_t g8 = (color >> 16) & 0xff;
                        const uint8_t b8 = (color >> 8) & 0xff;
#endif
                        dstRow[x] = ((r8 >> 5) << 5) | ((g8 >> 5) << 2) | (b8 >> 6);
                    }
                    dstRow += dstPitch;
                    src32_row += scaled_width;
                }
            } else if (is_16bit) {
                uint16_t* dstRow = reinterpret_cast<uint16_t*>(dst + dstPitch * dstStartY);
                const int dst16Pitch = dstPitch / 2;
                for (int y = 0; y < copyRows; y++) {
                    for (int x = 0; x < scaled_width; x++) {
                        const uint32_t color = src32_row[x];
#if wxBYTE_ORDER == wxLITTLE_ENDIAN
                        const uint8_t r8 = (color >> 16) & 0xff;
                        const uint8_t g8 = (color >> 8) & 0xff;
                        const uint8_t b8 = color & 0xff;
#else
                        const uint8_t r8 = (color >> 24) & 0xff;
                        const uint8_t g8 = (color >> 16) & 0xff;
                        const uint8_t b8 = (color >> 8) & 0xff;
#endif
                        dstRow[x] = ((r8 >> 3) << 10) | ((g8 >> 3) << 5) | (b8 >> 3);
                    }
                    dstRow += dst16Pitch;
                    src32_row += scaled_width;
                }
            } else {  // is_24bit
                uint8_t* dstRow = dst + dstPitch * dstStartY;
                for (int y = 0; y < copyRows; y++) {
                    for (int x = 0; x < scaled_width; x++) {
                        const uint8_t* src_pixel = reinterpret_cast<const uint8_t*>(&src32_row[x]);
                        dstRow[x * 3] = src_pixel[0];
                        dstRow[x * 3 + 1] = src_pixel[1];
                        dstRow[x * 3 + 2] = src_pixel[2];
                    }
                    dstRow += dstPitch;
                    src32_row += scaled_width;
                }
            }
        }

        // Restore original shift values
        systemRedShift = saved_red_shift;
        systemGreenShift = saved_green_shift;
        systemBlueShift = saved_blue_shift;
        RGB_LOW_BITS_MASK = saved_rgb_low_bits_mask;
    }
}

long GetSampleRate() {
    switch (OPTION(kSoundAudioRate)) {
        case config::AudioRate::k48kHz:
            return 48000;
            break;
        case config::AudioRate::k44kHz:
            return 44100;
            break;
        case config::AudioRate::k22kHz:
            return 22050;
            break;
        case config::AudioRate::k11kHz:
            return 11025;
            break;
        case config::AudioRate::kLast:
            VBAM_NOTREACHED_RETURN(44100);
    }
    VBAM_NOTREACHED_RETURN(44100);
}

// Use panel_color_depth_ (per-panel member) instead of global systemColorDepth.
// systemColorDepth can be changed by panel recreation or other code, causing
// format mismatch crashes in DrawArea (e.g., reading 16-bit data as 32-bit).
#define out_8  (panel_color_depth_ ==  8)
#define out_16 (panel_color_depth_ == 16)
#define out_24 (panel_color_depth_ == 24)

// Draw Unicode text on raw pixel buffer using wxWidgets
void drawTextWx(uint8_t* buffer, int pitch, int x, int y, const wxString& text,
                int buffer_width, int buffer_height, double scale, int color_depth) {
    if (text.empty()) return;

    // Determine bytes per pixel based on color depth
    int bpp = color_depth >> 3;  // 16->2, 24->3, 32->4

    // Scale font size with filter scale
    // Use 11pt base size at native resolution - sweet spot for threshold-based anti-aliasing removal
    // Larger font makes character strokes more consistent, helping threshold preserve details like 'e' bar
    int fontSize = (int)std::ceil(11 * scale);

    // Use system default font with Unicode support
    wxFont font = wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT);
    font.SetPointSize(fontSize);
    font.SetWeight(wxFONTWEIGHT_NORMAL);

    // Create memory DC to measure text and handle line wrapping
    wxBitmap tempBmp(1, 1);
    wxMemoryDC tempDc(tempBmp);
    tempDc.SetFont(font);

    // Calculate maximum width for text (leave small right margin)
    int maxWidth = buffer_width - x - (int)std::ceil(5 * scale);
    if (maxWidth <= 0) return;

    // Character-wrap the text into lines (breaks at any character)
    wxArrayString lines;
    wxString currentLine;

    for (size_t i = 0; i < text.length(); i++) {
        wxChar ch = text[i];

        // Handle newline characters
        if (ch == '\n') {
            lines.Add(currentLine);
            currentLine.clear();
            continue;
        }

        // Try adding this character to the current line
        wxString testLine = currentLine + ch;
        wxSize testSize = tempDc.GetTextExtent(testLine);

        if (testSize.GetWidth() > maxWidth && !currentLine.empty()) {
            // Current line is full, start a new line with this character
            lines.Add(currentLine);
            currentLine = ch;
        } else {
            // Add character to current line
            currentLine += ch;
        }
    }

    // Add the last line if not empty
    if (!currentLine.empty()) {
        lines.Add(currentLine);
    }

    // Measure total size needed
    int maxLineWidth = 0;
    int lineHeight = tempDc.GetCharHeight();
    // Tighter line spacing for compact display at native resolution
    int lineSpacing = lineHeight + 1;
    for (size_t i = 0; i < lines.GetCount(); i++) {
        wxSize lineSize = tempDc.GetTextExtent(lines[i]);
        if (lineSize.GetWidth() > maxLineWidth) {
            maxLineWidth = lineSize.GetWidth();
        }
    }

    int textWidth = maxLineWidth + 4;
    int textHeight = (int)(lines.GetCount() * lineSpacing) + 4;

    // Don't clamp bitmap size - let it be full size and clip during pixel copy
    if (textWidth <= 0 || textHeight <= 0) return;

    wxBitmap textBmp(textWidth, textHeight, 24);
    wxMemoryDC dc(textBmp);

    // Set up the DC for text rendering with bold red text
    dc.SetFont(font);
    dc.SetBackground(*wxBLACK_BRUSH);
    dc.Clear();
    dc.SetTextForeground(*wxRED);

    // Draw each line of text with compact spacing
    int currentY = 1;
    int drawLineSpacing = dc.GetCharHeight() + 1;
    for (size_t i = 0; i < lines.GetCount(); i++) {
        dc.DrawText(lines[i], 1, currentY);
        currentY += drawLineSpacing;
    }
    dc.SelectObject(wxNullBitmap);

    // Convert bitmap to image to access pixel data
    wxImage textImg = textBmp.ConvertToImage();
    const unsigned char* imgData = textImg.GetData();

    // Blend the text image into the buffer
    for (int ty = 0; ty < textHeight; ty++) {
        for (int tx = 0; tx < textWidth; tx++) {
            int bufX = x + tx;
            int bufY = y + ty;

            if (bufX >= buffer_width || bufY >= buffer_height) continue;

            // Get pixel from text image (always 24-bit RGB)
            int imgIdx = (ty * textWidth + tx) * 3;
            uint8_t r = imgData[imgIdx];
            (void)imgData[imgIdx + 1];  // g - unused
            (void)imgData[imgIdx + 2];  // b - unused

            // Calculate buffer position
            uint8_t* bufPtr = buffer + (bufY * pitch) + (bufX * bpp);

            // Threshold anti-aliased text to create crisp pixels
            // Use threshold of 100 (~39%) - balances crispness with capturing thin strokes (like 'e' bar)
            if (r > 100) {
                // Render as pure red for sharp, pixel-perfect text
                if (color_depth == 8) {
                    // 8-bit RGB332 format: bits 7-5=R, 4-2=G, 1-0=B
                    // Pure red = 0xE0 (all red bits set, green/blue zero)
                    *bufPtr = 0xE0;
                } else if (color_depth == 16) {
                    uint16_t* pixel = (uint16_t*)bufPtr;
                    // 16-bit RGB555 format: bits 14-10=R, 9-5=G, 4-0=B
                    // Pure red = 0x7C00 (all red bits set, green/blue zero)
                    *pixel = 0x7C00;
                } else if (color_depth == 24) {
                    // 24-bit RGB format (systemRedShift=3 means red at low byte)
                    bufPtr[0] = 255;  // Red
                    bufPtr[1] = 0;    // Green
                    bufPtr[2] = 0;    // Blue
                } else if (color_depth == 32) {
                    uint32_t* pixel = (uint32_t*)bufPtr;
                    *pixel = (0xff << systemRedShift);  // Pure red in 32-bit
                }
            }
        }
    }
}

}  // namespace

int emulating;

IMPLEMENT_DYNAMIC_CLASS(GameArea, wxPanel)

GameArea::GameArea()
    : wxPanel(),
      panel(NULL),
      emusys(NULL),
      was_paused(false),
      rewind_time(0),
      do_rewind(false),
      rewind_mem(0),
      num_rewind_states(0),
      loaded(IMAGE_UNKNOWN),
      basic_width(GBAWidth),
      basic_height(GBAHeight),
      fullscreen(false),
      paused(false),
      pointer_blanked(false),
      mouse_active_time(0),
      render_observer_({config::OptionID::kDispBilinear, config::OptionID::kDispFilter,
                        config::OptionID::kDispFilterPlugin, config::OptionID::kDispRenderMethod,
                        config::OptionID::kDispIFB, config::OptionID::kDispStretch,
                        config::OptionID::kPrefVsync, config::OptionID::kSDLRenderer,
                        config::OptionID::kBitDepth, config::OptionID::kDispHDR,
                        config::OptionID::kDispDeepColor},
                       std::bind(&GameArea::ResetPanel, this)),
      color_correction_auto_observer_(config::OptionID::kDispHDR,
                                      std::bind(&GameArea::ApplyAutoColorCorrection, this)),
      scale_observer_(config::OptionID::kDispScale, std::bind(&GameArea::AdjustSize, this, true)),
      gb_border_observer_(config::OptionID::kPrefBorderOn,
                          std::bind(&GameArea::OnGBBorderChanged, this, std::placeholders::_1)),
      gb_palette_observer_({config::OptionID::kGBPalette0, config::OptionID::kGBPalette1,
                            config::OptionID::kGBPalette2, config::OptionID::kPrefGBPaletteOption},
                           std::bind(&gbResetPalette)),
      gb_declick_observer_(
          config::OptionID::kSoundGBDeclicking,
          [&](config::Option* option) { gbSoundSetDeclicking(option->GetBool()); }),
      lcd_filters_observer_({config::OptionID::kGBLCDFilter, config::OptionID::kGBADarken, config::OptionID::kGBLighten, config::OptionID::kDispColorCorrectionProfile, config::OptionID::kGBALCDFilter,
                             config::OptionID::kDispHDRReferenceWhite, config::OptionID::kDispHDRPeakBrightness, config::OptionID::kDispHDRHighlightKnee, config::OptionID::kDispHDRShadowContrast},
                            std::bind(&GameArea::UpdateLcdFilter, this)),
      audio_rate_observer_(config::OptionID::kSoundAudioRate,
                           std::bind(&GameArea::OnAudioRateChanged, this)),
      audio_volume_observer_(config::OptionID::kSoundVolume,
                             std::bind(&GameArea::OnVolumeChanged, this, std::placeholders::_1)),
      audio_observer_({config::OptionID::kSoundAudioAPI, config::OptionID::kSoundAudioDevice,
                       config::OptionID::kSoundBuffers, config::OptionID::kSoundDSoundHWAccel,
                       config::OptionID::kSoundUpmix},
                      [&](config::Option*) { schedule_audio_restart_ = true; }) {
    SetSizer(new wxBoxSizer(wxVERTICAL));
    // all renderers prefer 32-bit
    // well, "simple" prefers 24-bit, but that's not available for filters
    systemColorDepth = (OPTION(kBitDepth) + 1) << 3;

    hq2x_init(32);
    Init_2xSaI(32);

    // Bind once here - panel creation happens repeatedly on filter changes,
    // and binding inside that block accumulates duplicate handlers, causing
    // OnSize to fire N times after N filter changes.
    this->Bind(wxEVT_SIZE, &GameArea::OnSize, this);
}

// Returns a valid override key for the loaded GBA ROM.
// Uses the 4-char game code if all bytes are printable ASCII,
// otherwise falls back to "CRC_XXXXXXXX" using zlib crc32.
static wxString gbaGetOverrideId() {
    bool valid = true;
    for (int i = 0; i < 4; i++) {
        uint8_t c = g_rom[0xac + i];
        if (c < 0x21 || c > 0x7e) { valid = false; break; }
    }
    if (valid)
        return wxString((const char*)&g_rom[0xac], wxConvLibc, 4);
    uint32_t romcrc = crc32(0L, g_rom, gbaGetRomSize());
    return wxString::Format(wxT("CRC_%08X"), romcrc);
}

void GameArea::LoadGame(const wxString& name)
{
    rom_scene_rls = "-";
    rom_scene_rls_name = "-";
    rom_name = "";
    // fex just crashes if file does not exist and it's compressed,
    // so check first
    wxFileName fnfn(name);
    bool badfile = !fnfn.IsFileReadable();

    // if path was relative, look for it before giving up
    if (badfile && !fnfn.IsAbsolute()) {
        wxString rp = fnfn.GetPath();

        // can't really decide which dir to use, so try GBA first, then GB
        const wxString gba_rom_dir = OPTION(kGBAROMDir);
        if (!wxGetApp().GetAbsolutePath(gba_rom_dir).empty()) {
            fnfn.SetPath(wxGetApp().GetAbsolutePath(gba_rom_dir) + '/' + rp);
            badfile = !fnfn.IsFileReadable();
        }

        const wxString gb_rom_dir = OPTION(kGBROMDir);
        if (badfile && !wxGetApp().GetAbsolutePath(gb_rom_dir).empty()) {
            fnfn.SetPath(wxGetApp().GetAbsolutePath(gb_rom_dir) + '/' + rp);
            badfile = !fnfn.IsFileReadable();
        }

        const wxString gbc_rom_dir = OPTION(kGBGBCROMDir);
        if (badfile && !wxGetApp().GetAbsolutePath(gbc_rom_dir).empty()) {
            fnfn.SetPath(wxGetApp().GetAbsolutePath(gbc_rom_dir) + '/' + rp);
            badfile = !fnfn.IsFileReadable();
        }
    }

    // auto-conversion of wxCharBuffer to const char * seems broken
    // so save underlying wxCharBuffer (or create one of none is used)
    wxCharBuffer fnb(UTF8(fnfn.GetFullPath()));
    const char* fn = fnb.data();
    IMAGE_TYPE t = badfile ? IMAGE_UNKNOWN : utilFindType(fn);

    if (t == IMAGE_UNKNOWN) {
        wxString s;
        s.Printf(_("%s is not a valid ROM file"), name.mb_str());
        wxMessageDialog dlg(GetParent(), s, _("Problem loading file"), wxOK | wxICON_ERROR);
        dlg.ShowModal();
        return;
    }

    {
        wxConfigBase* cfg = wxConfigBase::Get();

        if (!OPTION(kGenFreezeRecent)) {
            gopts.recent->AddFileToHistory(name);
            wxGetApp().frame->ResetRecentAccelerators();
            cfg->SetPath("/Recent");
            gopts.recent->Save(*cfg);
            cfg->SetPath("/");
            cfg->Flush();
        }
    }

    UnloadGame();
    // strip extension from actual game file name
    // FIXME: save actual file name of archive so patches & cheats can
    // be loaded from archive
    // FIXME: if archive name does not match game name, prepend archive
    // name to uniquify game name
    loaded_game = fnfn;
    loaded_game.ClearExt();
    loaded_game.MakeAbsolute();
    // load patch, if enabled
    // note that it is difficult to load from archive due to
    // ../common/Patch.cpp depending on opening the file itself and doing
    // lots of seeking & such.  The only way would be to copy the patch
    // out to a temporary file and load it (and can't just use
    // AssignTempFileName because it needs correct extension)
    // too much trouble for now, though
    bool loadpatch = OPTION(kPrefAutoPatch);
    wxFileName pfn = loaded_game;
    int ovSaveType = 0;

    if (loadpatch) {
        // SetExt may strip something off by accident, so append to text instead
        // pfn.SetExt(wxT("ips")
        pfn.SetFullName(pfn.GetFullName() + wxT(".ips"));

        if (!pfn.IsFileReadable()) {
            pfn.SetExt(wxT("ups"));

            if (!pfn.IsFileReadable()) {
                pfn.SetExt(wxT("bps"));

                if (!pfn.IsFileReadable()) {
                    pfn.SetExt(wxT("ppf"));
                    loadpatch = pfn.IsFileReadable();
                }
            }
        }
    }

    if (t == IMAGE_GB) {
        if (!gbLoadRom(fn)) {
            wxString s;
            s.Printf(_("Unable to load Game Boy ROM %s"), name.mb_str());
            wxMessageDialog dlg(GetParent(), s, _("Problem loading file"), wxOK | wxICON_ERROR);
            dlg.ShowModal();
            return;
        }

        if (loadpatch) {
            gbApplyPatch(UTF8(pfn.GetFullPath()));
        }

        // Apply overrides.
        wxFileConfig* cfg = wxGetApp().gb_overrides_.get();
        const std::string& title = g_gbCartData.title();
        if (cfg->HasGroup(title)) {
            cfg->SetPath(title);
            coreOptions.gbPrinterEnabled = cfg->Read("gbPrinter", coreOptions.gbPrinterEnabled);
            cfg->SetPath("/");
        }

        // start sound; this must happen before CPU stuff
        gb_effects_config.enabled = OPTION(kSoundGBEnableEffects);
        gb_effects_config.surround = OPTION(kSoundGBSurround);
        gb_effects_config.echo = (float)OPTION(kSoundGBEcho) / 100.0;
        gb_effects_config.stereo = (float)OPTION(kSoundGBStereo) / 100.0;
        if (!soundInit()) {
            wxLogError(_("Could not initialize the sound driver!"));
        }
        soundSetEnable(gopts.sound_en);
        gbSoundSetSampleRate(GetSampleRate());
        // this **MUST** be called **AFTER** setting sample rate because the core calls soundInit()
        soundSetThrottle(coreOptions.throttle);
        gbGetHardwareType();


        // Disable bios loading when using colorizer hack.
        if (OPTION(kPrefUseBiosGB) && OPTION(kGBColorizerHack)) {
            wxLogError(_("Cannot use Game Boy BIOS file when Colorizer Hack is enabled, disabling Game Boy BIOS file."));
            OPTION(kPrefUseBiosGB) = false;
        }

        // Set up the core for the colorizer hack.
        setColorizerHack(OPTION(kGBColorizerHack));

        // Load BIOS for the actual system being emulated
        // gbHardware: 1=GB, 2=GBC, 4=SGB/SGB2, 8=GBA
        // Only load BIOS for GB and GBC modes
        bool use_bios = false;
        wxString bios_file;

        if (gbHardware == 2) {
            // Game Boy Color
            use_bios = OPTION(kPrefUseBiosGBC).Get();
            bios_file = OPTION(kGBGBCBiosFile).Get();
        } else if (gbHardware == 1) {
            // Game Boy
            use_bios = OPTION(kPrefUseBiosGB).Get();
            bios_file = OPTION(kGBBiosFile).Get();
        }
        // For SGB/SGB2 (4) and GBA (8), use_bios remains false (no BIOS loaded)

        gbCPUInit(bios_file.To8BitData().data(), use_bios);

        if (use_bios && !coreOptions.useBios) {
            wxLogError(_("Could not load BIOS %s"), bios_file);
            // could clear use flag & file name now, but better to force
            // user to do it
        }

        gbReset();

        if (OPTION(kPrefBorderOn)) {
            basic_width = gbBorderLineSkip = SGBWidth;
            basic_height = SGBHeight;
            gbBorderColumnSkip = (SGBWidth - GBWidth) / 2;
            gbBorderRowSkip = (SGBHeight - GBHeight) / 2;
        } else {
            basic_width = gbBorderLineSkip = GBWidth;
            basic_height = GBHeight;
            gbBorderColumnSkip = gbBorderRowSkip = 0;
        }

        emusys = &GBSystem;
    } else /* if(t == IMAGE_GBA) */
    {
        if (!(rom_size = CPULoadRom(fn))) {
            wxString s;
            s.Printf(_("Unable to load Game Boy Advance ROM %s"), name.mb_str());
            wxMessageDialog dlg(GetParent(), s, _("Problem loading file"), wxOK | wxICON_ERROR);
            dlg.ShowModal();
            return;
        }

        rom_crc32 = crc32(0L, g_rom, rom_size);

        if (loadpatch) {
            // don't use real rom size or it might try to resize rom[]
            // instead, use known size of rom[]
            int size = 0x2000000 < rom_size ? 0x2000000 : rom_size;
            applyPatch(UTF8(pfn.GetFullPath()), &g_rom, &size);
            // that means we no longer really know rom_size either <sigh>

            gbaUpdateRomSize(size);
        }


        wxFileConfig* cfg = wxGetApp().overrides_.get();
        wxString id = gbaGetOverrideId();

        // Check for game-specific overrides based on ROM ID
        if (cfg->HasGroup(id)) {
            cfg->SetPath(id);
            // Apply RTC override
            bool enable_rtc = cfg->Read(wxT("rtcEnabled"), coreOptions.rtcEnabled);
            rtcEnable(enable_rtc);

            // Set Flash size from config; fallback to global preference
            int fsz = cfg->Read(wxT("flashSize"), (long)0);

            if (fsz != 0x10000 && fsz != 0x20000)
                fsz = 0x10000 << OPTION(kPrefFlashSize);

            flashSetSize(fsz);
            // Set save type from override; fallback to detection if set to Auto (0)
            ovSaveType = cfg->Read(wxT("saveType"), coreOptions.cpuSaveType);

            if (ovSaveType < 0 || ovSaveType > 5)
                ovSaveType = 0;

            if (ovSaveType == 0) {
                flashDetectSaveType(rom_size);
            } else {
                coreOptions.saveType = ovSaveType;
            }

            // Initialize eepromMask to prevent DMA freeze on un-initialized battery
            if (coreOptions.saveType == GBA_SAVE_EEPROM) {
                eepromSetSize(SIZE_EEPROM_512);
            }

            // Apply ROM mirroring and reset path
            coreOptions.mirroringEnable = cfg->Read(wxT("mirroringEnabled"), (long)1);
            cfg->SetPath(wxT("/"));
        } else {
            // Apply global defaults for games without specific overrides
            rtcEnable(coreOptions.rtcEnabled);
            flashSetSize(0x10000 << OPTION(kPrefFlashSize));

            // Determine save type based on global preference or internal detection
            if (coreOptions.cpuSaveType < 0 || coreOptions.cpuSaveType > 5)
                coreOptions.cpuSaveType = 0;

            if (coreOptions.cpuSaveType == 0) {
                flashDetectSaveType(rom_size);
            } else {
                coreOptions.saveType = coreOptions.cpuSaveType;
            }

            // Initialize eepromMask to prevent DMA freeze on un-initialized battery
            if (coreOptions.saveType == GBA_SAVE_EEPROM) {
                eepromSetSize(SIZE_EEPROM_512);
            }

            // Disable mirroring by default
            coreOptions.mirroringEnable = false;
        }

        doMirroring(coreOptions.mirroringEnable);
        // start sound; this must happen before CPU stuff
        if (!soundInit()) {
            wxLogError(_("Could not initialize the sound driver!"));
        }
        soundSetEnable(gopts.sound_en);
        soundSetSampleRate(GetSampleRate());
        // this **MUST** be called **AFTER** setting sample rate because the core calls soundInit()
        soundSetThrottle(coreOptions.throttle);
        soundFiltering = (float)OPTION(kSoundGBAFiltering) / 100.0f;

        rtcEnableRumble(true);

        CPUInit(UTF8(gopts.gba_bios), OPTION(kPrefUseBiosGBA));

        if (OPTION(kPrefUseBiosGBA) && !coreOptions.useBios) {
            wxLogError(_("Could not load BIOS %s"), gopts.gba_bios.mb_str());
            // could clear use flag & file name now, but better to force
            // user to do it
        }

        CPUReset();
        basic_width = GBAWidth;
        basic_height = GBAHeight;
        emusys = &GBASystem;
    }

    // Set sound volume. The --mute command-line switch forces silence for the
    // session without altering the saved volume.
    soundSetVolume(wxGetApp().mute ? 0.0 : (float)OPTION(kSoundVolume) / 100.0);

    if (OPTION(kGeomFullScreen)) {
        GameArea::ShowFullScreen(true);
    }

    loaded = t;
    SetFrameTitle();
    SetFocus();
    // Use custom geometry
    AdjustSize(false);
    emulating = true;
    was_paused = true;
    schedule_audio_restart_ = false;
    MainFrame* mf = wxGetApp().frame;
    mf->cmd_enable &= ~(CMDEN_GB | CMDEN_GBA);
    mf->cmd_enable |= ONLOAD_CMDEN;
    mf->cmd_enable |= loaded == IMAGE_GB ? CMDEN_GB : (CMDEN_GBA | CMDEN_NGDB_GBA);
    mf->enable_menus();
#if (defined __WIN32__ || defined _WIN32)
#ifndef NO_LINK
    gbSerialFunction = gbStartLink;
#else
    gbSerialFunction = NULL;
#endif
#endif

    SuspendScreenSaver();

    // probably only need to do this for GB carts
    if (coreOptions.gbPrinterEnabled)
        gbSerialFunction = gbPrinterSend;

    // probably only need to do this for GBA carts
    agbPrintEnable(OPTION(kPrefAgbPrint));

    // set frame skip based on ROM type
    const int frame_skip = OPTION(kPrefFrameSkip);
    if (frame_skip != -1) {
        systemFrameSkip = frame_skip;
    }

    // load battery and/or saved state
    recompute_dirs();
    mf->update_state_ts(true);
    bool did_autoload = OPTION(kGenAutoLoadLastState) ? LoadState() : false;

    if (!did_autoload || coreOptions.skipSaveGameBattery) {
        wxString bname = loaded_game.GetFullName();
#ifndef NO_LINK
        // MakeInstanceFilename doesn't do wxString, so just add slave ID here
        int playerId = GetLinkPlayerId();

        if (playerId >= 0) {
            bname.append(wxT('-'));
            bname.append(wxChar(wxT('1') + playerId));
        }

#endif
        bname.append(wxT(".sav"));
        wxFileName bat(batdir, bname);

        if (emusys->emuReadBattery(UTF8(bat.GetFullPath()))) {
            wxString msg;
            msg.Printf(_("Loaded battery %s"), bat.GetFullPath().wc_str());
            systemScreenMessage(msg);

            if (coreOptions.cpuSaveType == 0 && ovSaveType == 0 && t == IMAGE_GBA) {
                switch (bat.GetSize().GetValue()) {
                case 0x200:
                case 0x2000:
                    coreOptions.saveType = GBA_SAVE_EEPROM;
                    eepromSetSize(bat.GetSize().GetValue());
                    break;

                case 0x8000:
                    coreOptions.saveType = GBA_SAVE_SRAM;
                    break;

                case 0x10000:
                    if (coreOptions.saveType == GBA_SAVE_EEPROM || coreOptions.saveType == GBA_SAVE_SRAM)
                        break;
                    break;

                case 0x20000:
                    coreOptions.saveType = GBA_SAVE_FLASH;
                    flashSetSize(bat.GetSize().GetValue());
                    break;

                default:
                    break;
                }

                SetSaveType(coreOptions.saveType);
            }
        }

        // forget old save writes
        systemSaveUpdateCounter = SYSTEM_SAVE_NOT_UPDATED;
    }

    // do an immediate rewind save
    // even if loaded from state file: not smart enough yet to just
    // do a reset or load from state file when # rewinds == 0
    do_rewind = gopts.rewind_interval > 0;
    // FIXME: backup battery file (useful if game name conflict)
    cheats_dirty = (did_autoload && !coreOptions.skipSaveGameCheats) || (loaded == IMAGE_GB ? gbCheatNumber > 0 : cheatsNumber > 0);

    if (OPTION(kPrefAutoSaveLoadCheatList) && (!did_autoload || coreOptions.skipSaveGameCheats)) {
        wxFileName cfn = loaded_game;
        // SetExt may strip something off by accident, so append to text instead
        cfn.SetFullName(cfn.GetFullName() + wxT(".clt"));

        if (cfn.IsFileReadable()) {
            bool cld;

            if (loaded == IMAGE_GB)
                cld = gbCheatsLoadCheatList(UTF8(cfn.GetFullPath()));
            else
                cld = cheatsLoadCheatList(UTF8(cfn.GetFullPath()));

            if (cld) {
                systemScreenMessage(_("Loaded cheats"));
                cheats_dirty = false;
            }
        }
    }

#ifndef NO_LINK
    if (OPTION(kGBALinkAuto)) {
        BootLink(mf->GetConfiguredLinkMode(), UTF8(gopts.link_host),
                 gopts.link_timeout, OPTION(kGBALinkFast),
                 gopts.link_num_players);
    }
#endif

#if defined(VBAM_ENABLE_DEBUGGER)
    if (OPTION(kPrefGDBBreakOnLoad)) {
        mf->GDBBreak();
    }
#endif  // defined(VBAM_ENABLE_DEBUGGER)
}

void GameArea::SetFrameTitle()
{
    wxString tit = wxT("");

    if (loaded != IMAGE_UNKNOWN) {
        tit.append(loaded_game.GetFullName());
        tit.append(wxT(" - "));
    }

    tit.append(wxT("VisualBoyAdvance-M "));
    tit.append(kVbamVersion);

#ifndef NO_LINK
    int playerId = GetLinkPlayerId();

    if (playerId >= 0) {
        tit.append(_(" player "));
        tit.append(wxChar(wxT('1') + playerId));
    }

#endif
    wxGetApp().frame->SetTitle(tit);
}

// Get system name string for the currently loaded ROM
// Returns expanded system name based on the ROM type
static wxString GetCurrentSystemName(IMAGE_TYPE game_type) {
    if (game_type == IMAGE_GBA)
        return wxT("GameBoy Advance");

    // For Game Boy ROMs, check for GBC and SGB modes
    if (game_type == IMAGE_GB) {
        if (gbCgbMode)
            return wxT("GameBoy Color");
        if (gbSgbMode)
            return wxT("Super GameBoy");
        return wxT("GameBoy");
    }

    return wxT("GameBoy Advance");
}

// Expand %s in path to system name
static wxString ExpandSystemPath(const wxString& path, IMAGE_TYPE game_type) {
    wxString result = path;
    if (result.Contains(wxT("%s"))) {
        wxString system_name = GetCurrentSystemName(game_type);
        result.Replace(wxT("%s"), system_name);
    }
    return result;
}

void GameArea::recompute_dirs()
{
    batdir = ExpandSystemPath(OPTION(kGenBatteryDir), loaded);

    if (batdir.empty()) {
        batdir = loaded_game.GetPathWithSep();
    } else {
        batdir = wxGetApp().GetAbsolutePath(batdir);
        // Try to create the directory if it doesn't exist
        if (!wxFileName::DirExists(batdir)) {
            if (!wxFileName::Mkdir(batdir, 0777, wxPATH_MKDIR_FULL)) {
                wxMessageBox(
                    wxString::Format(_("Could not create Native Saves directory:\n%s\n\nPlease check your configured path in Options > Directories."), batdir),
                    _("Directory Error"),
                    wxOK | wxICON_ERROR);
            }
        }
    }

    if (!wxIsWritable(batdir)) {
        batdir = wxGetApp().GetDataDir();
    }

    statedir = ExpandSystemPath(OPTION(kGenStateDir), loaded);

    if (statedir.empty()) {
        statedir = loaded_game.GetPathWithSep();
    } else {
        statedir = wxGetApp().GetAbsolutePath(statedir);
        // Try to create the directory if it doesn't exist
        if (!wxFileName::DirExists(statedir)) {
            if (!wxFileName::Mkdir(statedir, 0777, wxPATH_MKDIR_FULL)) {
                wxMessageBox(
                    wxString::Format(_("Could not create Emulator Saves directory:\n%s\n\nPlease check your configured path in Options > Directories."), statedir),
                    _("Directory Error"),
                    wxOK | wxICON_ERROR);
            }
        }
    }

    if (!wxIsWritable(statedir)) {
        statedir = wxGetApp().GetDataDir();
    }
}

void GameArea::UnloadGame(bool destruct)
{
    if (!emulating)
        return;

    // last opportunity to autosave cheats
    if (OPTION(kPrefAutoSaveLoadCheatList) && cheats_dirty) {
        wxFileName cfn = loaded_game;
        // SetExt may strip something off by accident, so append to text instead
        cfn.SetFullName(cfn.GetFullName() + wxT(".clt"));

        if (loaded == IMAGE_GB) {
            if (!gbCheatNumber)
                wxRemoveFile(cfn.GetFullPath());
            else
                gbCheatsSaveCheatList(UTF8(cfn.GetFullPath()));
        } else {
            if (!cheatsNumber)
                wxRemoveFile(cfn.GetFullPath());
            else
                cheatsSaveCheatList(UTF8(cfn.GetFullPath()));
        }
    }

    // if timer was counting down for save, go ahead and save
    // this might not be safe, though..
    if (systemSaveUpdateCounter > SYSTEM_SAVE_NOT_UPDATED) {
        SaveBattery();
    }

    MainFrame* mf = wxGetApp().frame;
#ifndef NO_FFMPEG
    snd_rec.Stop();
    vid_rec.Stop();
#endif
    systemStopGameRecording();
    systemStopGamePlayback();

#if defined(VBAM_ENABLE_DEBUGGER)
    debugger = false;
    remoteCleanUp();
    mf->cmd_enable |= CMDEN_NGDB_ANY;
#endif  // VBAM_ENABLE_DEBUGGER

    if (loaded == IMAGE_GB) {
        gbCleanUp();
        gbCheatRemoveAll();

        // Reset overrides.
        coreOptions.gbPrinterEnabled = OPTION(kPrefGBPrinter);
    } else if (loaded == IMAGE_GBA) {
        CPUCleanUp();
        cheatsDeleteAll(false);
    }

    UnsuspendScreenSaver();
    emulating = false;
    loaded = IMAGE_UNKNOWN;
    emusys = NULL;
    soundShutdown();

    disableKeyboardBackgroundInput();

    if (destruct)
        return;

    // in destructor, panel should be auto-deleted by wx since all panels
    // are derived from a window attached as child to GameArea
    ResetPanel();

    // close any game-related viewer windows
    // in destructor, viewer windows are in process of being deleted anyway
    while (!mf->popups.empty())
        mf->popups.front()->Close(true);

    // remaining items are GUI updates that should not be needed in destructor
    SetFrameTitle();
    mf->cmd_enable &= UNLOAD_CMDEN_KEEP;
    mf->update_state_ts(true);
    mf->enable_menus();
    mf->ResetCheatSearch();

    if (rewind_mem)
        num_rewind_states = 0;
}

bool GameArea::LoadState()
{
    int slot = wxGetApp().frame->newest_state_slot();

    if (slot < 1)
        return false;

    return LoadState(slot);
}

bool GameArea::LoadState(int slot)
{
    wxString fname;
    fname.Printf(SAVESLOT_FMT, game_name().wc_str(), slot);
    return LoadState(wxFileName(statedir, fname));
}

bool GameArea::LoadState(const wxFileName& fname)
{
    // FIXME: first save to backup state if not backup state
    bool ret = emusys->emuReadState(UTF8(fname.GetFullPath()));

    if (ret && num_rewind_states) {
        MainFrame* mf = wxGetApp().frame;
        mf->cmd_enable &= ~CMDEN_REWIND;
        mf->enable_menus();
        num_rewind_states = 0;
        // do an immediate rewind save
        // even if loaded from state file: not smart enough yet to just
        // do a reset or load from state file when # rewinds == 0
        do_rewind = true;
        rewind_time = gopts.rewind_interval * 6;
    }

    if (ret) {
        // forget old save writes
        systemSaveUpdateCounter = SYSTEM_SAVE_NOT_UPDATED;
        // no point in blending after abrupt change
        InterframeClear();
        // frame rate calc should probably reset as well
        was_paused = true;
        // save state had a screen frame, so draw it
        systemDrawScreen();
    }

    wxString msg;
    msg.Printf(ret ? _("Loaded state %s") : _("Error loading state %s"),
        fname.GetFullPath().wc_str());
    systemScreenMessage(msg);
    return ret;
}

bool GameArea::SaveState()
{
    return SaveState(wxGetApp().frame->oldest_state_slot());
}

bool GameArea::SaveState(int slot)
{
    wxString fname;
    fname.Printf(SAVESLOT_FMT, game_name().wc_str(), slot);
    return SaveState(wxFileName(statedir, fname));
}

bool GameArea::SaveState(const wxFileName& fname)
{
    // FIXME: first copy to backup state if not backup state
    bool ret = emusys->emuWriteState(UTF8(fname.GetFullPath()));
    wxGetApp().frame->update_state_ts(true);
    wxString msg;
    msg.Printf(ret ? _("Saved state %s") : _("Error saving state %s"),
        fname.GetFullPath().wc_str());
    systemScreenMessage(msg);
    return ret;
}

void GameArea::SaveBattery()
{
    // MakeInstanceFilename doesn't do wxString, so just add slave ID here
    wxString bname = game_name();
#ifndef NO_LINK
    int playerId = GetLinkPlayerId();

    if (playerId >= 0) {
        bname.append(wxT('-'));
        bname.append(wxChar(wxT('1') + playerId));
    }

#endif
    bname.append(wxT(".sav"));
    wxFileName bat(batdir, bname);
    bat.Mkdir(0777, wxPATH_MKDIR_FULL);
    wxString fn = bat.GetFullPath();

    // FIXME: add option to support ring of backups
    // of course some games just write battery way too often for such
    // a thing to be useful
    if (!emusys->emuWriteBattery(UTF8(fn)))
        wxLogError(_("Error writing battery %s"), fn.mb_str());

    systemSaveUpdateCounter = SYSTEM_SAVE_NOT_UPDATED;
}

void GameArea::AddBorder()
{
    if (basic_width != GBWidth)
        return;

    basic_width = SGBWidth;
    basic_height = SGBHeight;
    gbBorderLineSkip = SGBWidth;
    gbBorderColumnSkip = (SGBWidth - GBWidth) / 2;
    gbBorderRowSkip = (SGBHeight - GBHeight) / 2;
    AdjustSize(false);
    wxGetApp().frame->Fit();
    GetSizer()->Detach(panel->GetWindow());
    ResetPanel();
}

void GameArea::DelBorder()
{
    if (basic_width != SGBWidth)
        return;

    basic_width = GBWidth;
    basic_height = GBHeight;
    gbBorderLineSkip = GBWidth;
    gbBorderColumnSkip = gbBorderRowSkip = 0;
    AdjustSize(false);
    wxGetApp().frame->Fit();
    GetSizer()->Detach(panel->GetWindow());
    ResetPanel();
}

void GameArea::AdjustMinSize()
{
    wxWindow* frame           = wxGetApp().frame;
    double dpi_scale_factor = widgets::DPIScaleFactorForWindow(this);
    const double display_scale = OPTION(kDispScale);

    // note: could safely set min size to 1x or less regardless of video_scale
    // but setting it to scaled size makes resizing to default easier
    wxSize sz((std::ceil(basic_width * display_scale) * dpi_scale_factor),
              (std::ceil(basic_height * display_scale) * dpi_scale_factor));
    SetMinSize(sz);
#if wxCHECK_VERSION(2, 8, 8)
    sz = frame->ClientToWindowSize(sz);
#else
    sz += frame->GetSize() - frame->GetClientSize();
#endif
    frame->SetMinSize(sz);
}

void GameArea::LowerMinSize()
{
    wxWindow* frame           = wxGetApp().frame;
    double dpi_scale_factor = widgets::DPIScaleFactorForWindow(this);

    wxSize sz(std::ceil(basic_width * dpi_scale_factor),
              std::ceil(basic_height * dpi_scale_factor));

    SetMinSize(sz);
    // do not take decorations into account
    frame->SetMinSize(sz);
}

void GameArea::AdjustSize(bool force)
{
    AdjustMinSize();

    if (fullscreen)
        return;

    double dpi_scale_factor = widgets::DPIScaleFactorForWindow(this);
    const double display_scale = OPTION(kDispScale);

    const wxSize newsz(
        (std::ceil(basic_width * display_scale) * dpi_scale_factor),
        (std::ceil(basic_height * display_scale) * dpi_scale_factor));

    if (!force) {
        wxSize sz = GetClientSize();

        if (sz.GetWidth() >= newsz.GetWidth() && sz.GetHeight() >= newsz.GetHeight())
            return;
    }

    SetSize(newsz);
    GetParent()->SetClientSize(newsz);
    wxGetApp().frame->Fit();
}

void GameArea::ResetPanel() {
    // The new panel's renderer-init and HDR success must be re-evaluated.
    hdr_evaluated_ = false;
    renderer_evaluated_ = false;
    if (panel) {
        // Pause emulation while panel is being reset to prevent crashes
        // from accessing invalid panel state during the transition.
        // Note: We force paused=true directly because Pause() skips pausing
        // when link mode is enabled, but we MUST pause for panel reset.
        bool was_running = !paused;
        if (was_running) {
            paused = was_paused = true;
            UnsuspendScreenSaver();
            wxGetApp().emulated_gamepad()->Reset();
            if (loaded != IMAGE_UNKNOWN)
                soundPause();
        }

        // Stop filter threads SYNCHRONOUSLY before cleaning up InterframeManager.
        // This is critical because Destroy() defers destruction, and we need to
        // ensure threads aren't accessing InterframeManager when it's cleaned up.
        panel->StopFilterThreads();
        InterframeCleanup();

#if defined(__WXMSW__) && !defined(NO_D3D12)
        // D3D panels must be destroyed synchronously. Their destructors release
        // the D3D device/swap chain. If we use wx's deferred Destroy(), those
        // resources stay alive until the wx event loop processes the deferred
        // delete - and D3D/DXGI will block the new panel's device/swap chain
        // creation until the old ones are fully released, causing a noticeable
        // stall when reinitialising after a filter change.
        // We must delete via the correctly-typed pointer so the right destructor
        // chain runs - DrawingPanelBase's destructor is not virtual.
        if (auto* dx12 = dynamic_cast<DX12DrawingPanel*>(panel)) {
            delete dx12;
        } else
#endif
#if defined(__WXMSW__) && !defined(NO_D3D)
        if (auto* dx9 = dynamic_cast<DXDrawingPanel*>(panel)) {
            delete dx9;
        } else
#endif
        // SDL panels must also be destroyed synchronously. wx's deferred
        // Destroy() keeps the old SDL window -- and the swapchain/CAMetalLayer it
        // attached to the borrowed host view -- alive until the event loop runs
        // the deferred delete. The replacement panel creates a new SDL window on
        // the same host view first, so the stale layer from the old renderer
        // lingers behind it (seen as bars of the previous frame at the edges)
        // when switching SDL sub-renderers. Deleting now tears it down in time.
        // Use the concrete type so the (non-virtual) DrawingPanelBase destructor
        // chain runs correctly.
        if (auto* sdl = dynamic_cast<SDLDrawingPanel*>(panel)) {
            delete sdl;
        } else
        {
        panel->Destroy();
        }
        panel = NULL;

        // Mark that we need to resume after panel is recreated
        if (was_running)
            pending_resume_after_panel_ = true;
    }
}

void GameArea::SchedulePanelReset() {
    // Set flag to defer panel reset until start of next OnIdle.
    // This avoids resetting the panel while CPULoop is in the middle of rendering,
    // which would cause crashes due to invalid g_pix or panel state.
    pending_panel_reset_ = true;
}

void GameArea::ShowFullScreen(bool full)
{
    if (full == fullscreen) {
        // in case the tlw somehow lost its mind, force it to proper mode
        if (wxGetApp().frame->IsFullScreen() != fullscreen)
            wxGetApp().frame->ShowFullScreen(full);

        return;
    }

// on Mac maximize is native fullscreen, so ignore fullscreen requests
#ifdef __WXMAC__
    if (full && main_frame->IsMaximized()) return;
#endif

    fullscreen = full;

    // just in case screen mode is going to change, go ahead and preemptively
    // delete panel to be recreated immediately after resize
    ResetPanel();

    // Windows does not restore old window size/pos
    // at least under Wine
    // so store them before entering fullscreen
    static bool cursz_valid = false;
    static wxSize cursz;
    static wxPoint curpos;
    MainFrame* tlw = wxGetApp().frame;
    int dno = wxDisplay::GetFromWindow(tlw);

    if (!full) {
        if (gopts.fs_mode.w && gopts.fs_mode.h) {
            wxDisplay d(dno);
            d.ChangeMode(wxDefaultVideoMode);
        }

        tlw->ShowFullScreen(false);

        if (!cursz_valid) {
            curpos = wxDefaultPosition;
            cursz = tlw->GetMinSize();
        }

        tlw->SetSize(cursz);
        tlw->SetPosition(curpos);
        AdjustMinSize();
    } else {
        // close all non-modal dialogs
        while (!tlw->popups.empty())
            tlw->popups.front()->Close();

        // mouse stays blank whenever full-screen
        HidePointer();
        cursz_valid = true;
        cursz = tlw->GetSize();
        curpos = tlw->GetPosition();
        LowerMinSize();

        if (gopts.fs_mode.w && gopts.fs_mode.h) {
            // grglflargm
            // stupid wx does not do fullscreen properly when video mode is
            // changed under X11.   Since it does not use xrandr to switch, it
            // just changes the video mode without resizing the desktop.  It
            // still uses the original desktop size for fullscreen, though.
            // It also seems to stick the top-left corner wherever it pleases,
            // so I can't just resize the panel and expect it to show up.
            // ^(*&^^&!!!!  maybe just disable this code altogether on UNIX
            // most 3d cards are fine with full screen size, anyway.
            wxDisplay d(dno);

            if (!d.ChangeMode(gopts.fs_mode)) {
                wxLogInfo(_("Fullscreen mode %d x %d - %d @ %d not supported; looking for another"),
                    gopts.fs_mode.w, gopts.fs_mode.h, gopts.fs_mode.bpp, gopts.fs_mode.refresh);
                // specifying a mode may not work with bpp/rate of 0
                // in particular, unix does Matches() in wrong direction
                wxArrayVideoModes vm = d.GetModes();
                int best_mode = -1;
                size_t i;

                for (i = 0; i < vm.size(); i++) {
                    if (vm[i].w != gopts.fs_mode.w || vm[i].h != gopts.fs_mode.h)
                        continue;

                    int bpp = vm[i].bpp;

                    if (gopts.fs_mode.bpp && bpp == gopts.fs_mode.bpp)
                        break;

                    if (!gopts.fs_mode.bpp && bpp == 32) {
                        gopts.fs_mode.bpp = 32;
                        break;
                    }

                    int bm_bpp = best_mode < 0 ? 0 : vm[i].bpp;

                    if (bpp == 32 && bm_bpp != 32)
                        best_mode = i;
                    else if (bpp == 24 && bm_bpp < 24)
                        best_mode = i;
                    else if (bpp == 16 && bm_bpp < 24 && bm_bpp != 16)
                        best_mode = i;
                    else if (!best_mode)
                        best_mode = i;
                }

                if (i == vm.size() && best_mode >= 0)
                    i = best_mode;

                if (i == vm.size()) {
                    wxLogWarning(_("Fullscreen mode %d x %d - %d @ %d not supported"),
                        gopts.fs_mode.w, gopts.fs_mode.h, gopts.fs_mode.bpp, gopts.fs_mode.refresh);
                    gopts.fs_mode = wxVideoMode();

                    for (i = 0; i < vm.size(); i++)
                        wxLogInfo(_("Valid mode: %d x %d - %d @ %d"), vm[i].w,
                            vm[i].h, vm[i].bpp,
                            vm[i].refresh);
                } else {
                    gopts.fs_mode.bpp = vm[i].bpp;
                    gopts.fs_mode.refresh = vm[i].refresh;
                }

                wxLogInfo(_("Chose mode %d x %d - %d @ %d"),
                    gopts.fs_mode.w, gopts.fs_mode.h, gopts.fs_mode.bpp, gopts.fs_mode.refresh);

                if (!d.ChangeMode(gopts.fs_mode)) {
                    wxLogWarning(_("Failed to change mode to %d x %d - %d @ %d"),
                        gopts.fs_mode.w, gopts.fs_mode.h, gopts.fs_mode.bpp, gopts.fs_mode.refresh);
                    gopts.fs_mode.w = 0;
                }
            }
        }

        tlw->ShowFullScreen(true);
    }
}

GameArea::~GameArea()
{
    UnloadGame(true);

    if (rewind_mem)
        free(rewind_mem);

    if (gopts.fs_mode.w && gopts.fs_mode.h && fullscreen) {
        MainFrame* tlw = wxGetApp().frame;
        int dno = wxDisplay::GetFromWindow(tlw);
        wxDisplay d(dno);
        d.ChangeMode(wxDefaultVideoMode);
    }
}

void GameArea::OnKillFocus(wxFocusEvent& ev)
{
    wxGetApp().emulated_gamepad()->Reset();
    ev.Skip();
}

void GameArea::Pause()
{
    if (paused)
        return;

    // don't pause when linked
#ifndef NO_LINK
    if (GetLinkMode() != LINK_DISCONNECTED)
        return;
#endif

    paused = was_paused = true;
    UnsuspendScreenSaver();

    // when the game is paused like this, we should not allow any
    // input to remain pressed, because they could be released
    // outside of the game zone and we would not know about it.
    wxGetApp().emulated_gamepad()->Reset();

    if (loaded != IMAGE_UNKNOWN)
        soundPause();
}

void GameArea::Resume()
{
    if (!paused)
        return;

    paused = false;
    SuspendScreenSaver();
    SetExtraStyle(GetExtraStyle() | wxWS_EX_PROCESS_IDLE);

    if (loaded != IMAGE_UNKNOWN)
        soundResume();

    SetFocus();
}

void GameArea::OnIdle(wxIdleEvent& event)
{
    wxString pl = wxGetApp().pending_load;
    MainFrame* mf = wxGetApp().frame;

    // Preload one config dialog per idle tick while no ROM is running, so
    // the user doesn't pay the XRC parse cost the first time they open
    // Options. Skipped once emulation is active. Only request another idle
    // round if no user input is queued, so we yield promptly on click/key.
    if (!emusys && mf && mf->PreloadOneDialog() &&
        !wxTheApp->HasPendingEvents()) {
        event.RequestMore();
    }

    if (pl.size()) {
        // sometimes this gets into a loop if LoadGame() called before
        // clearing pending_load.  weird.
        wxGetApp().pending_load = wxEmptyString;
        LoadGame(pl);

#if defined(VBAM_ENABLE_DEBUGGER)
        if (OPTION(kPrefGDBBreakOnLoad)) {
            mf->GDBBreak();
        }

        if (debugger && loaded != IMAGE_GBA) {
            wxLogError(_("Not a valid Game Boy Advance cartridge"));
            UnloadGame();
        }
#endif  // defined(VBAM_ENABLE_DEBUGGER)
    }

    // stupid wx doesn't resize to screen size
    // forcing it this way just puts it in an infinite loop, though
    // with wx trying to resize/reposition to what it thinks is full screen
    // every time it detects a manual resize like this
    if (gopts.fs_mode.w && gopts.fs_mode.h && fullscreen) {
        wxSize sz = GetSize();

        if (sz.GetWidth() != gopts.fs_mode.w || sz.GetHeight() != gopts.fs_mode.h) {
            wxGetApp().frame->Move(0, 84);
            SetSize(gopts.fs_mode.w, gopts.fs_mode.h);
        }
    }

    if (!emusys) {
        // No game loaded, can't create panel
        return;
    }

    // Handle deferred panel reset (set by option observer callbacks).
    // This must be done at the start of OnIdle, BEFORE running emulation,
    // to avoid resetting the panel while CPULoop is in progress.
    if (pending_panel_reset_) {
        pending_panel_reset_ = false;
        ResetPanel();
        // After reset, panel is NULL and will be recreated below.
        // Request more idle events to trigger panel creation.
        event.RequestMore();
        return;
    }

    // Keep systemColorDepth in sync with the bit depth option, but only
    // when no plugin filter is managing color depth. Plugin filters set
    // systemColorDepth to match their supported format (e.g., 16-bit for
    // RGB555/565 plugins), and overriding it would cause the core to
    // produce pixels in the wrong format.
    if (OPTION(kDispFilter) != config::Filter::kPlugin) {
        int newColorDepth = (OPTION(kBitDepth) + 1) << 3;
        if (newColorDepth != systemColorDepth) {
            systemColorDepth = newColorDepth;
            // Update color shift values for new depth
            if (systemColorDepth == 24 || systemColorDepth == 32) {
#if wxBYTE_ORDER == wxLITTLE_ENDIAN
                systemRedShift = 3;
                systemGreenShift = 11;
                systemBlueShift = 19;
                RGB_LOW_BITS_MASK = 0x00010101;
#else
                systemRedShift = 27;
                systemGreenShift = 19;
                systemBlueShift = 11;
                RGB_LOW_BITS_MASK = 0x01010100;
#endif
            } else {
                // 8-bit or 16-bit
                systemRedShift = 10;
                systemGreenShift = 5;
                systemBlueShift = 0;
                RGB_LOW_BITS_MASK = 0x0421;
            }
            // Note: hq2x_init/Init_2xSaI/UpdateLcdFilter will be called when panel
            // is created/recreated via DrawingPanelInit()
        }
    }

    if (schedule_audio_restart_) {
        soundShutdown();
        if (!soundInit()) {
            wxLogError(_("Could not initialize the sound driver!"));
        }
        schedule_audio_restart_ = false;
    }

    if (!panel) {
        panel = NewPanelForRenderMethod(OPTION(kDispRenderMethod));
        if (!panel)
            return;

        wxWindow* w = NULL;

        w = panel->GetWindow();

        // set up event handlers
        w->Bind(VBAM_EVT_USER_INPUT, &GameArea::OnUserInput, this);
        w->Bind(wxEVT_PAINT, &GameArea::PaintEv, this);
        w->Bind(wxEVT_ERASE_BACKGROUND, &GameArea::EraseBackground, this);

        // We need to check if the buttons stayed pressed when focus the panel.
        w->Bind(wxEVT_KILL_FOCUS, &GameArea::OnKillFocus, this);

        // Update mouse last-used timers on mouse events etc..
        w->Bind(wxEVT_MOTION, &GameArea::MouseEvent, this);
        w->Bind(wxEVT_LEFT_DOWN, &GameArea::MouseEvent, this);
        w->Bind(wxEVT_RIGHT_DOWN, &GameArea::MouseEvent, this);
        w->Bind(wxEVT_MIDDLE_DOWN, &GameArea::MouseEvent, this);
        w->Bind(wxEVT_MOUSEWHEEL, &GameArea::MouseEvent, this);

        w->SetBackgroundStyle(wxBG_STYLE_CUSTOM);
        w->SetSize(wxSize(basic_width, basic_height));

        // Allow input while on background
        if (OPTION(kUIAllowKeyboardBackgroundInput)) {
            enableKeyboardBackgroundInput(w->GetEventHandler());
        }

        if (gopts.max_scale)
            w->SetMaxSize(wxSize(basic_width * gopts.max_scale,
                                 basic_height * gopts.max_scale));

        // if user changed Display/Scale config, this needs to run
        AdjustMinSize();
        AdjustSize(false);

        const bool retain_aspect = OPTION(kDispStretch);
        const unsigned frame_priority = retain_aspect ? 0 : 1;

        GetSizer()->Clear();

        // add spacers on top and bottom to center panel vertically
        // but not on 2.8 which does not handle this correctly
        if (retain_aspect) {
            GetSizer()->Add(0, 0, wxEXPAND);
        }

        // this triggers an assertion dialog in <= 3.1.2 in debug mode
        GetSizer()->Add(
            w, frame_priority,
            retain_aspect ? (wxSHAPED | wxALIGN_CENTER | wxEXPAND) : wxEXPAND);

        if (retain_aspect) {
            GetSizer()->Add(0, 0, wxEXPAND);
        }

        Layout();
        SendSizeEvent();

        if (pointer_blanked)
            w->SetCursor(wxCursor(wxCURSOR_BLANK));

        // set focus to panel
        w->SetFocus();

        // generate system color maps (after output module init)
        UpdateLcdFilter();

        // Return here to let the panel fully initialize before running emulation.
        // The next OnIdle will start emulation.
        event.RequestMore();
        return;
    }

    // Once the new panel is initialized, check whether it actually presents HDR
    // (when HDR is requested). A renderer that fails is recorded and another is
    // tried; if all fail, HDR reverts to SDR. This may change the render method
    // or HDR option, scheduling another panel reset.
    // If the active renderer failed to initialize (no device/context/renderer),
    // fall back to the next renderer in the platform priority list before
    // evaluating HDR/deep color. Gate on init having resolved -- either it
    // succeeded (DrawingInitialized) or it explicitly failed (DrawingInitFailed).
    if (panel && !renderer_evaluated_ &&
        (panel->DrawingInitialized() || panel->DrawingInitFailed())) {
        renderer_evaluated_ = true;
        if (panel->DrawingInitFailed()) {
            EvaluateRenderer();  // may switch the render method, resetting the panel
            if (!panel) {
                event.RequestMore();
                return;
            }
        }
    }

    if (panel && !hdr_evaluated_ && panel->DrawingInitialized()) {
        hdr_evaluated_ = true;
        EvaluateHdrRenderer();
        EvaluateDeepColorRenderer();
        // Switching the render method fires the render observer, which resets
        // the panel synchronously (panel == NULL). The recreation path below
        // requires another idle tick, and the RequestMore() at the bottom of
        // OnIdle only runs while a panel exists -- so request one here and let
        // the next tick recreate the panel with the new renderer.
        if (!panel) {
            event.RequestMore();
            return;
        }
    }

    // Resume emulation if we paused for a panel reset and the panel is now ready
    if (pending_resume_after_panel_ && panel) {
        pending_resume_after_panel_ = false;
        Resume();
    }

    if (!paused && panel) {
        // Defensive check: ensure g_pix is valid before running emulation
        extern uint8_t* g_pix;
        if (!g_pix) {
            wxLogDebug(wxT("g_pix is NULL, skipping emulation frame"));
            return;
        }

        HidePointer();
        HideMenuBar();
        event.RequestMore();

#if defined(VBAM_ENABLE_DEBUGGER)
        if (debugger) {
            was_paused = true;
            dbgMain();

            if (!emulating) {
                emulating = true;
                SuspendScreenSaver();
                UnloadGame();
            }

            return;
        }
#endif  // defined(VBAM_ENABLE_DEBUGGER)

        emusys->emuMain(emusys->emuCount);
#ifndef NO_LINK

        if (loaded == IMAGE_GBA && GetLinkMode() != LINK_DISCONNECTED)
            CheckLinkConnection();

#endif
    } else {
        was_paused = true;

        if (paused)
            SetExtraStyle(GetExtraStyle() & ~wxWS_EX_PROCESS_IDLE);

        ShowMenuBar();
    }

    if (do_rewind && emusys->emuWriteMemState) {
        if (!rewind_mem) {
            rewind_mem = (char*)malloc(NUM_REWINDS * REWIND_SIZE);
            num_rewind_states = next_rewind_state = 0;
        }

        if (!rewind_mem) {
            wxLogError(_("No memory for rewinding"));
            wxGetApp().frame->Close(true);
            return;
        }

        long resize;

        if (!emusys->emuWriteMemState(&rewind_mem[REWIND_SIZE * next_rewind_state],
                REWIND_SIZE, resize /* actual size */))
            // if you see a lot of these, maybe increase REWIND_SIZE
            wxLogInfo(_("Error writing rewind state"));
        else {
            if (!num_rewind_states) {
                mf->cmd_enable |= CMDEN_REWIND;
                mf->enable_menus();
            }

            if (num_rewind_states < NUM_REWINDS)
                ++num_rewind_states;

            next_rewind_state = (next_rewind_state + 1) % NUM_REWINDS;
        }

        do_rewind = false;
    }
}

static void draw_black_background(wxWindow* win) {
    wxClientDC dc(win);
    wxCoord w, h;
    dc.GetSize(&w, &h);
    dc.SetPen(*wxBLACK_PEN);
    dc.SetBrush(*wxBLACK_BRUSH);
    dc.DrawRectangle(0, 0, w, h);
}

#ifdef __WXGTK__
static Display* GetX11Display() {
    return GDK_WINDOW_XDISPLAY(gtk_widget_get_window(wxGetApp().frame->GetHandle()));
}
#endif  // __WXGTK__

void GameArea::OnUserInput(widgets::UserInputEvent& event) {
    // Keyboard inputs are mutated synchronously via the sync_sink wired
    // in wxvbamApp's constructor before this queued event fires.
    // Re-running OnInputPressed/Released here for keyboard would race
    // with sync_sink: if focus changes between a press and its release,
    // the press queued to the panel re-runs OnInputPressed AFTER
    // sync_sink already released, but the release event went to a
    // different (now-focused) handler that doesn't bind
    // VBAM_EVT_USER_INPUT, so nothing clears the bit. Joystick inputs
    // do NOT go through sync_sink (only the SDL poller queue path), so
    // we still need to mutate state for those here.
    bool emulated_key_pressed = false;
    const config::Bindings* const bindings = wxGetApp().bindings();
    for (const auto& event_data : event.data()) {
        if (event_data.input.is_keyboard()) {
            // Already handled by sync_sink; just check if it's
            // game-bound for the wakeup/screensaver logic below.
            const auto command = bindings->CommandForInput(event_data.input);
            if (command && command->is_game()) {
                emulated_key_pressed = true;
            }
        } else if (event_data.pressed) {
            if (wxGetApp().emulated_gamepad()->OnInputPressed(event_data.input)) {
                emulated_key_pressed = true;
            }
        } else {
            if (wxGetApp().emulated_gamepad()->OnInputReleased(event_data.input)) {
                emulated_key_pressed = true;
            }
        }
    }

    if (emulated_key_pressed) {
        wxWakeUpIdle();

#if defined(__WXGTK__) && defined(HAVE_X11) && !defined(HAVE_XSS)
        // Tell X11 to turn off the screensaver/screen-blank if a button was
        // was pressed. This shouldn't be necessary.
#ifndef NO_WAYLAND
        if (!wxGetApp().UsingWayland()) {
#endif
            auto display = GetX11Display();
            XResetScreenSaver(display);
            XFlush(display);
#ifndef NO_WAYLAND
        }
#endif
#endif
    }
}

// these three are forwarded to the DrawingPanel instance
void GameArea::PaintEv(wxPaintEvent& ev)
{
    panel->PaintEv(ev);
}

void GameArea::EraseBackground(wxEraseEvent& ev)
{
    panel->EraseBackground(ev);
}

void GameArea::OnSize(wxSizeEvent& ev)
{
    draw_black_background(this);
    Layout();

    // panel may resize
    if (panel)
        panel->OnSize(ev);
    Layout();

    ev.Skip();
}

BEGIN_EVENT_TABLE(GameArea, wxPanel)
EVT_IDLE(GameArea::OnIdle)
// FIXME: wxGTK does not generate motion events in MainFrame (not sure
// what to do about it)
EVT_MOUSE_EVENTS(GameArea::MouseEvent)
END_EVENT_TABLE()

DrawingPanelBase::DrawingPanelBase(int _width, int _height)
    : width(_width),
      height(_height),
      scale(1),
      did_init(false),
      todraw(0),
      pixbuf1(0),
      pixbuf2(0),
      nthreads(0),
      rpi_(NULL) {
    memset(delta, 0xff, sizeof(delta));
    memset(&rpi_info_, 0, sizeof(rpi_info_));

    if (OPTION(kDispFilter) == config::Filter::kPlugin) {
        const wxString pluginPath = OPTION(kDispFilterPlugin);
        bool loaded = false;

#ifdef VBAM_RPI_PROXY_SUPPORT
        // Check if this is a 32-bit plugin that needs the proxy
        bool needsProxy = widgets::PluginNeedsProxy(pluginPath);
        if (needsProxy) {
            rpi_proxy_client_ = rpi_proxy::RpiProxyClient::GetSharedInstance();
            if (widgets::MaybeLoadFilterPluginViaProxy(pluginPath,
                    rpi_proxy_client_, &rpi_info_)) {
                rpi_ = &rpi_info_;
                using_rpi_proxy_ = true;
                loaded = true;
            } else {
                rpi_proxy_client_ = nullptr;
            }
        }
#endif

        // Try direct loading if proxy wasn't used or failed
        if (!loaded) {
            RENDER_PLUGIN_INFO* plugin_info = widgets::MaybeLoadFilterPlugin(pluginPath, &filter_plugin_);
            if (plugin_info) {
                // Copy to local storage so the panel owns its own flags.
                // The DLL's static RENDER_PLUGIN_INFO can be reset by any
                // subsequent RenderPluginGetInfo() call (e.g., during plugin
                // enumeration), which would corrupt our modified Flags.
                rpi_info_ = *plugin_info;
                rpi_ = &rpi_info_;
                loaded = true;
            }
        }

        if (loaded && rpi_) {
            // Select the best color format the plugin supports.
            // Prefer 565 > 555 > 888. The RPI_888_SUPP flag is ambiguous — some
            // plugins (e.g. g2xscale) treat it as 24-bit packed BGR, others as
            // 32-bit BGRX, and there's no way to tell from flags alone. The
            // 16-bit formats are unambiguous and every 888 plugin we've seen in
            // the wild also advertises at least one 16-bit format, so we only
            // fall back to 888 when the plugin offers no 16-bit path.
            // 565 is chosen over 555 because at least one real-world plugin
            // (2xBR-v3.4) has a latent bug where its lookup-table init routine
            // is only invoked on the 565 code path — the 555 path falls through
            // with an uninitialized table and produces black pixels.
            bool using_rgb565 = false;
            if (rpi_->Flags & RPI_565_SUPP) {
                rpi_->Flags &= ~(RPI_555_SUPP | RPI_888_SUPP);
                panel_color_depth_ = 16;
                systemColorDepth = 16;  // Core needs this to output correct format
                rpi_bpp_ = 2;
                using_rgb565 = true;
            } else if (rpi_->Flags & RPI_555_SUPP) {
                rpi_->Flags &= ~(RPI_565_SUPP | RPI_888_SUPP);
                panel_color_depth_ = 16;
                systemColorDepth = 16;  // Core needs this to output correct format
                rpi_bpp_ = 2;
            } else if (rpi_->Flags & RPI_888_SUPP) {
                rpi_->Flags &= ~(RPI_555_SUPP | RPI_565_SUPP);
                panel_color_depth_ = 32;
                systemColorDepth = 32;  // Core needs this to output correct format
                rpi_bpp_ = 4;
            } else {
                // No supported color format - this shouldn't happen
                panel_color_depth_ = 32;
                systemColorDepth = 32;  // Core needs this to output correct format
                rpi_bpp_ = 4;
            }
            // Store for color shift configuration later
            rpi_using_rgb565_ = using_rgb565;
            // Detect plugins that need single-threaded full-frame processing:
            // - Multi-threaded plugins (e.g., "4xBRZ MT") - their internal
            //   threads conflict with VBA-M's multi-threaded band processing
            // - Version 1 plugins - designed for Kega Fusion's full-frame model
            //   and may not respect SrcH/DstH for partial images
            // - 3x+ scalers (e.g., hq4xS, lcd3x_vblend) - produce visible seams or
            //   ghost copies at thread-band boundaries. Vertical-blend filters read
            //   adjacent rows that cross the slice boundary into another thread's
            //   unwritten area, leaving ghost artifacts the seam-fix can't recover.
            // - MMX/SSE plugins (e.g., mdntsc_sse) - use internal buffers sized for
            //   the full image; partial-frame calls across threads leave visible
            //   band seams that seam-fix can't cover because they hardcode strides
            unsigned int pluginVersion = rpi_->Flags & 0xff;
            uint32_t scaleFlag = (rpi_->Flags & RPI_OUT_SCLMSK) >> RPI_OUT_SCLSH;
            rpi_is_mt_ = (strstr(rpi_->Name, " MT") != nullptr)
                || (pluginVersion == 1)
                || (scaleFlag >= 3)
                || (rpi_->Flags & RPI_MMX_USED) != 0;

            // Some plugins (e.g. Super2xSaI) advertise RPI_MMX_USED as an optional
            // fast path but ship a broken MMX routine whose call target lands in
            // the .data section. If MMX isn't required, clear the "use MMX" bit so
            // the plugin's internal dispatcher takes the portable non-MMX path.
            if ((rpi_->Flags & RPI_MMX_USED) && !(rpi_->Flags & RPI_MMX_REQD)) {
                rpi_->Flags &= ~RPI_MMX_USED;
            }

#ifdef VBAM_RPI_PROXY_SUPPORT
            // For proxy mode, Output is handled by the proxy client
            if (!using_rpi_proxy_)
#endif
            {
                if (!rpi_->Output) {
                    // note that in actual kega fusion plugins, rpi_->Output is
                    // unused (as is rpi_->Handle)
                    rpi_->Output =
                        (RENDPLUG_Output)filter_plugin_.GetSymbol("RenderPluginOutput");
                }
            }
            uint32_t pluginScale = (rpi_->Flags & RPI_OUT_SCLMSK) >> RPI_OUT_SCLSH;
            if (pluginScale == 0) {
                // Version 1 plugins (original Kega Fusion format) didn't have
                // scale flags - they were always 2x scalers.
                pluginScale = (pluginVersion == 1) ? 2 : 1;
            }
            // Ensure scale is at least 1 before multiplying
            if (scale < 1.0) {
                scale = 1.0;
            }
            scale *= pluginScale;
        } else {
            // Plugin failed to load - fall back to no filter.
            OPTION(kDispFilterPlugin) = wxEmptyString;
            OPTION(kDispFilter) = config::Filter::kNone;
            // Don't return early - fall through to initialize panel_color_depth_
            // and color tables for kNone mode.
        }
    }

    if (OPTION(kDispFilter) != config::Filter::kPlugin) {
        scale *= GetFilterScale();

        panel_color_depth_ = (OPTION(kBitDepth) + 1) << 3;
        systemColorDepth = panel_color_depth_;  // Core needs this to output correct format
    }

    // HDR output re-encodes the final RGBA8 image, so force a 32-bit source
    // when it is enabled (unless an RPI plugin has locked us to 16-bit).
    if (hdr::HdrAvailable() && OPTION(kDispHDR) && !(rpi_ && rpi_bpp_ == 2)) {
        panel_color_depth_ = 32;
        systemColorDepth = 32;
    }

    // Intialize color tables based on panel's color depth
    if (panel_color_depth_ == 24) {
#if wxBYTE_ORDER == wxLITTLE_ENDIAN
        systemRedShift = 3;
        systemGreenShift = 11;
        systemBlueShift = 19;
        RGB_LOW_BITS_MASK = 0x00010101;
#else
        systemRedShift = 27;
        systemGreenShift = 19;
        systemBlueShift = 11;
        RGB_LOW_BITS_MASK = 0x01010100;
#endif
    } else if (panel_color_depth_ == 32) {
#if wxBYTE_ORDER == wxLITTLE_ENDIAN
        systemRedShift = 3;
        systemGreenShift = 11;
        systemBlueShift = 19;
        RGB_LOW_BITS_MASK = 0x00010101;
#else
        systemRedShift = 27;
        systemGreenShift = 19;
        systemBlueShift = 11;
        RGB_LOW_BITS_MASK = 0x01010100;
#endif
    } else {
        // 16-bit mode: RGB555 or RGB565
        if (rpi_ && rpi_using_rgb565_) {
            // RGB565: 5 bits red, 6 bits green, 5 bits blue
            systemRedShift = 11;
            systemGreenShift = 5;
            systemBlueShift = 0;
            RGB_LOW_BITS_MASK = 0x0821;  // Low bit of each component (R:bit 11, G:bit 5, B:bit 0)
        } else {
            // RGB555: 5 bits each for red, green, blue
            systemRedShift = 10;
            systemGreenShift = 5;
            systemBlueShift = 0;
            RGB_LOW_BITS_MASK = 0x0421;
        }
    }
}

DrawingPanel::DrawingPanel(wxWindow* parent, int _width, int _height)
    : DrawingPanelBase(_width, _height)
    , wxPanel(parent, wxID_ANY, wxPoint(0, 0), parent->GetClientSize(),
          wxFULL_REPAINT_ON_RESIZE | wxWANTS_CHARS)
{
}

void DrawingPanelBase::DrawingPanelInit()
{
    did_init = true;
    UpdateHdrState();
}

void DrawingPanelBase::UpdateHdrState()
{
    hdr::Settings s;
    s.enabled = OPTION(kDispHDR) && SupportsHdr();
    s.sdr_reference_nits = static_cast<float>(OPTION(kDispHDRReferenceWhite));
    s.peak_nits = static_cast<float>(OPTION(kDispHDRPeakBrightness));
    // Never encode above what the display can present: on the scRGB/EDR path
    // anything brighter just clips, and on PQ the compositor tone-maps a phantom
    // range and dims real highlights. When the display's peak is known, cap to it
    // -- this is also what makes the "auto" default (peak set to the option max)
    // resolve to the display's peak. 0 = unknown, leave the setting as-is.
    if (const uint32_t dp = hdr::DisplayPeakNits())
        s.peak_nits = std::min(s.peak_nits, static_cast<float>(dp));
    s.highlight_knee = static_cast<float>(OPTION(kDispHDRHighlightKnee)) / 100.0f;
    s.shadow_contrast = static_cast<float>(OPTION(kDispHDRShadowContrast)) / 100.0f;
    s.input_is_rec2020 =
        OPTION(kDispColorCorrectionProfile) == config::ColorCorrectionProfile::kRec2020;
    // scRGB consumers (macOS EDR, SDL3) use a relative model where SDR white is
    // 1.0; the PQ path doesn't use this. Map our reference white to 1.0.
    // scRGB white point: the nits that map to 1.0 in the float output.
    //  * macOS EDR (and any relative scRGB surface): SDR white is the system
    //    brightness, so map the reference white to 1.0.
    //  * Windows scRGB via SDL: SDL auto-multiplies its SDR white point (> 1)
    //    into the output, so map that absolute white (HdrScRgbWhiteNits) to 1.0
    //    to cancel the multiply and land content at its true nits.
    //  * PQ path doesn't use this.
    if (PreferredHdrEncoding() == hdr::Encoding::kScRGBFp16) {
        const float scrgb_white = HdrScRgbWhiteNits();
        s.scrgb_white_nits = scrgb_white > 0.0f ? scrgb_white : s.sdr_reference_nits;
    } else {
        s.scrgb_white_nits = 80.0f;
    }
    s.scrgb_target_p3 = HdrScRgbUsesP3();
    hdr::Configure(s);

    hdr_encoding_ = s.enabled ? PreferredHdrEncoding() : hdr::Encoding::kNone;
}

const uint8_t* DrawingPanelBase::EncodeHdr(const uint8_t* src, int src_stride,
                                           int row_pixels, int rows, int* out_bpp)
{
    const int bpp = hdr::BytesPerPixel(hdr_encoding_);
    if (out_bpp)
        *out_bpp = bpp;
    if (bpp == 0 || row_pixels <= 0 || rows <= 0)
        return src;

    const int dst_stride = row_pixels * bpp;
    hdr_buf_.resize(static_cast<size_t>(dst_stride) * rows);

    switch (hdr_encoding_) {
        case hdr::Encoding::kPQ10:
            hdr::EncodePQ10(src, src_stride, row_pixels, rows,
                            reinterpret_cast<uint32_t*>(hdr_buf_.data()), dst_stride);
            break;
        case hdr::Encoding::kScRGBFp16:
            hdr::EncodeScRGBFp16(src, src_stride, row_pixels, rows,
                                 reinterpret_cast<uint16_t*>(hdr_buf_.data()), dst_stride);
            break;
        case hdr::Encoding::kNone:
            return src;
    }
    return hdr_buf_.data();
}

void DrawingPanelBase::PaintEv(wxPaintEvent& ev)
{
    (void)ev; // unused params
    wxPaintDC dc(GetWindow());

    if (!todraw) {
        // since this is set for custom background, not drawing anything
        // will cause garbage to be displayed, so draw a black area
        draw_black_background(GetWindow());
        return;
    }

    DrawArea(dc);

    // currently we draw the OSD directly on the framebuffer to reduce flickering
    //DrawOSD(dc);
}

void DrawingPanelBase::EraseBackground(wxEraseEvent& ev)
{
    (void)ev; // unused params
    // do nothing, do not allow propagation
}

// In order to run filters in parallel, they have to run from the method of
// a wxThread-derived class
//   threads are only created once, if possible, to avoid thread creation
//   overhead every frame; mutex+cond used to signal start and sem for end

//   when threading, there _will_ be bands for any nontrivial filter
//   usually the top and bottom line(s) of each region will look a little
//   different.  To ease this a tiny bit, 2 extra lines are generated at
//   the top of each region, so hopefully only the bottom of the previous
//   region will look screwed up.  The only correct way to handle this
//   is to draw an increasingly larger band around the seam until the
//   seam cover matches a line in both top & bottom regions, and then apply
//   cover to seam.   Way too much trouble for this, though.

//   another issue to consider is whether or not these filters are thread-
//   safe to begin with.  The built-in ones are verifyable (I didn't verify
//   them, though).  The plugins cannot be verified.  However, unlike the MFC
//   interface, I will allow them to be threaded at user's discretion.
class FilterThread : public wxThread {
public:
    // Three-phase execution: IFB first, then Filter, then optional SeamFix
    enum class Phase { IFB, Filter, SeamFix };

    FilterThread() : wxThread(wxTHREAD_JOINABLE), lock_(), sig_(lock_),
                     phase_(Phase::IFB), seamBandStart_(-1), seamBandEnd_(-1),
                     seamSrc_(nullptr), seamDst_(nullptr),
                     src2_(nullptr), dst2_(nullptr), src2_size_(0), dst2_size_(0) {}

    ~FilterThread() {
        // Free pre-allocated conversion buffers
        if (src2_) free(src2_);
        if (dst2_) free(dst2_);
    }

    wxMutex lock_;
    wxCondition sig_;
    wxSemaphore* done_;
    wxSemaphore* ready_;  // Posted when thread is ready to receive work signals

    // Set these params before running
    int nthreads_;
    int threadno_;
    int width_;
    int height_;
    double scale_;
    const RENDER_PLUGIN_INFO* rpi_;
    int rpi_bpp_ = 4;  // Bytes per pixel for RPI plugin (default 32-bit = 4)
    bool rpi_using_rgb565_ = false;  // Plugin uses RGB565 (needs 555↔565 conversion)
    int panel_color_depth_ = 32;  // Panel's color depth (independent from global systemColorDepth)
    uint8_t* dst_;
    uint8_t* delta_;
#ifdef VBAM_RPI_PROXY_SUPPORT
    rpi_proxy::RpiProxyClient* rpi_proxy_client_ = nullptr;
    bool proxy_thread_started_ = false;
#endif

    // set this param every round
    // if NULL, end thread
    uint8_t* src_;

    // Execution phase
    Phase phase_;

    // Seam fix parameters (set when phase_ == SeamFix)
    // -1 means no seam work assigned to this thread
    int seamBandStart_;
    int seamBandEnd_;
    uint8_t* seamSrc_;  // Source base for seam processing
    uint8_t* seamDst_;  // Dest base for seam processing

    // Pre-allocated conversion buffers (reused across frames)
    uint32_t* src2_;
    uint32_t* dst2_;
    size_t src2_size_;
    size_t dst2_size_;

    ExitCode Entry() override {
#ifdef VBAM_RPI_PROXY_SUPPORT
        // Start the proxy thread for this filter thread
        if (rpi_proxy_client_ && OPTION(kDispFilter) == config::Filter::kPlugin) {
            rpi_proxy_client_->StartThread(static_cast<uint32_t>(threadno_));
            proxy_thread_started_ = true;
        }
#endif

        // Pre-compute all constants once at thread start (not per-frame)
        const int height_real = height_;
        const int procy = height_ * threadno_ / nthreads_;
        height_ = height_ * (threadno_ + 1) / nthreads_ - procy;

        // Cache depth format flags for fast branching
        // Use panel_color_depth_ instead of systemColorDepth to avoid race conditions
        const bool is_8bit = (panel_color_depth_ == 8);
        const bool is_16bit = (panel_color_depth_ == 16);
        const bool is_24bit = (panel_color_depth_ == 24);
        const bool is_32bit = !is_8bit && !is_16bit && !is_24bit;

        // Pre-compute strides using integer math where possible
        // For plugin filters, use the cached rpi_bpp_ to avoid global state issues
        // For non-plugin filters, use panel_color_depth_ (not systemColorDepth) to avoid race conditions
        const bool using_plugin = (rpi_ != nullptr);
        const int inbpp = using_plugin ? rpi_bpp_ : (panel_color_depth_ >> 3);
        // Calculate inrb based on actual bpp for plugins to avoid global state issues
        const int inrb = using_plugin ? ((rpi_bpp_ == 1) ? 4 : (rpi_bpp_ == 2) ? 2 : (rpi_bpp_ == 3) ? 0 : 1)
                                      : (is_8bit ? 4 : is_16bit ? 2 : is_24bit ? 0 : 1);
        const int instride = (width_ + inrb) * inbpp;

        // For 32bpp conversion: filters need left+right borders of 1 each
        const int total_width32 = width_ + 2;  // left border + image + right border
        const int instride32 = total_width32 * 4;

        const int outbpp = using_plugin ? rpi_bpp_ : (panel_color_depth_ >> 3);
        // Use integer scale calculation (scale_ is typically 2, 3, 4, etc.)
        const int scale_int = static_cast<int>(scale_);
        const int outstride = (width_ + inrb) * outbpp * scale_int;
        const int outstride32 = width_ * 4 * scale_int;

        // Pre-compute source offset (procy + 1 rows)
        const int src_offset = instride * (procy + 1);

        // Adjust delta once for this thread's band
        delta_ += instride * procy;

        // Skip scale rows of top border in output buffer
        dst_ += outstride * (procy + 1) * scale_int;

        uint8_t* const dest = dst_;

        // Pre-allocate conversion buffers for non-32bpp modes (reused across frames)
        if (!is_32bit) {
            // Calculate required buffer sizes
            const size_t src2_required = static_cast<size_t>(total_width32) * (height_real + 3);
            const size_t dst2_required = static_cast<size_t>(width_ * scale_int) * (height_real * scale_int + scale_int);

            // Reallocate only if needed (buffers persist across frames)
            if (src2_size_ < src2_required) {
                if (src2_) free(src2_);
                src2_ = static_cast<uint32_t*>(calloc(4, src2_required));
                src2_size_ = src2_required;
            }
            if (dst2_size_ < dst2_required) {
                if (dst2_) free(dst2_);
                dst2_ = static_cast<uint32_t*>(calloc(4, dst2_required));
                dst2_size_ = dst2_required;
            }
        }

        // Pre-compute scaled dimensions for conversion loops
        // Use height_ (this thread's slice) for multi-threaded mode, not height_real
        const int scaled_height = height_ * scale_int;
        const int scaled_width = width_ * scale_int;
        const int scaled_border_dest = inrb * scale_int;

        // Lock mutex before waiting on condition (required for wxCondition::Wait)
        if (nthreads_ > 1) {
            lock_.Lock();
            // Signal that this thread is ready to receive work signals
            ready_->Post();
        }

        while (nthreads_ == 1 || sig_.Wait() == wxCOND_NO_ERROR) {
            if (!src_) {
#ifdef VBAM_RPI_PROXY_SUPPORT
                // Stop the proxy thread when this filter thread is terminating
                if (proxy_thread_started_) {
                    rpi_proxy_client_->StopThread(static_cast<uint32_t>(threadno_));
                    proxy_thread_started_ = false;
                }
#endif
                if (nthreads_ > 1)
                    lock_.Unlock();
                return 0;
            }

            // Handle two-phase execution
            if (phase_ == Phase::IFB) {
                // Phase 1: Apply IFB to source frame before filtering
                const config::Interframe ifb_option = OPTION(kDispIFB);
                if (ifb_option != config::Interframe::kNone) {
                    // src_ points to frame start (set by DrawArea before signaling)
                    // Skip the 1-row top border to point to actual image data
                    // IFB functions use starty to offset both source AND history buffers
                    uint8_t* ifb_src = src_ + instride;  // Skip top border row
                    if (ifb_option == config::Interframe::kSmart) {
                        InterframeManager::Instance().ApplySmartIBRegion(
                            ifb_src, instride, width_, procy, height_,
                            systemColorDepth, threadno_);
                    } else if (ifb_option == config::Interframe::kMotionBlur) {
                        InterframeManager::Instance().ApplyMotionBlurRegion(
                            ifb_src, instride, width_, procy, height_,
                            systemColorDepth, threadno_);
                    }
                }

                if (nthreads_ == 1)
                    return 0;

                done_->Post();
                continue;
            }

            // Phase 3: SeamFix - re-process seam bands with full context
            if (phase_ == Phase::SeamFix) {
                if (seamBandStart_ >= 0 && seamBandEnd_ > seamBandStart_) {
                    if (OPTION(kDispFilter) == config::Filter::kPlugin && rpi_) {
                        // Plugin seam fix: re-run the plugin filter on the seam band.
                        // Same layout rules as ApplyFilterOptimized — seamSrc_ points at the
                        // first data pixel, rows are padded on the right by plugin_inrb pixels.
                        const int bandHeight = seamBandEnd_ - seamBandStart_;
                        const int bpp = rpi_bpp_;
                        const int plugin_inrb = (bpp == 4) ? 1 : 2;
                        const int plugin_instride = (width_ + plugin_inrb) * bpp;
                        const int plugin_outstride = plugin_instride * scale_int;

                        RENDER_PLUGIN_OUTP outdesc;
                        outdesc.Size = sizeof(outdesc);
                        outdesc.Flags = rpi_->Flags;
                        outdesc.SrcPtr = seamSrc_ + plugin_instride * seamBandStart_;
                        outdesc.SrcPitch = plugin_instride;
                        outdesc.SrcW = width_;
                        outdesc.SrcH = bandHeight;
                        outdesc.DstW = static_cast<int>(width_ * scale_);
                        outdesc.DstH = static_cast<int>(bandHeight * scale_);
                        outdesc.DstPitch = plugin_outstride;
                        outdesc.OutW = outdesc.DstW;
                        outdesc.OutH = outdesc.DstH;

                        // Source format conversion for seam fix (same as main filter path)
                        std::vector<uint8_t> src_seam_converted;
                        if (bpp == 4) {
                            size_t srcBytes = static_cast<size_t>(outdesc.SrcPitch) * outdesc.SrcH;
                            src_seam_converted.resize(srcBytes);
                            const uint32_t* s = static_cast<const uint32_t*>(outdesc.SrcPtr);
                            uint32_t* d = reinterpret_cast<uint32_t*>(src_seam_converted.data());
                            for (size_t j = 0; j < srcBytes / 4; j++) {
                                uint32_t p = s[j];
                                d[j] = 0xFF000000u | (p & 0x0000FF00u) | ((p >> 16) & 0xFFu) | ((p & 0xFFu) << 16);
                            }
                            outdesc.SrcPtr = src_seam_converted.data();
                        } else if (rpi_using_rgb565_) {
                            outdesc.Flags = (outdesc.Flags & ~RPI_555_SUPP) | RPI_565_SUPP;
                            size_t srcBytes = static_cast<size_t>(outdesc.SrcPitch) * outdesc.SrcH;
                            src_seam_converted.resize(srcBytes);
                            const uint16_t* s = static_cast<const uint16_t*>(outdesc.SrcPtr);
                            uint16_t* d = reinterpret_cast<uint16_t*>(src_seam_converted.data());
                            for (size_t j = 0; j < srcBytes / 2; j++) {
                                uint16_t p = s[j];
                                uint8_t r = (p >> 10) & 0x1f;
                                uint8_t g = (p >> 5) & 0x1f;
                                uint8_t b = p & 0x1f;
                                d[j] = (r << 11) | (((g << 1) | (g >> 4)) << 5) | b;
                            }
                            outdesc.SrcPtr = src_seam_converted.data();
                        }

                        std::vector<uint8_t> tempBuf(static_cast<size_t>(plugin_outstride) * outdesc.DstH);
                        outdesc.DstPtr = tempBuf.data();

#ifdef VBAM_RPI_PROXY_SUPPORT
                        if (rpi_proxy_client_) {
                            rpi_proxy_client_->ApplyFilter(&outdesc, static_cast<uint32_t>(threadno_), 0);
                        } else
#endif
                        {
                            std::lock_guard<std::mutex> lock(g_rpi_output_mutex);
                            rpi_->Output(&outdesc);
                        }

                        // Convert seam fix output back to VBA-M format (strip alpha, swap R↔B).
                        if (bpp == 4) {
                            uint32_t* d = reinterpret_cast<uint32_t*>(tempBuf.data());
                            size_t dstPixels = static_cast<size_t>(plugin_outstride) * outdesc.DstH / 4;
                            for (size_t j = 0; j < dstPixels; j++) {
                                uint32_t p = d[j];
                                d[j] = (p & 0x0000FF00u) | ((p >> 16) & 0xFFu) | ((p & 0xFFu) << 16);
                            }
                        } else if (rpi_using_rgb565_) {
                            uint16_t* d = reinterpret_cast<uint16_t*>(tempBuf.data());
                            size_t dstBytes = static_cast<size_t>(plugin_outstride) * outdesc.DstH;
                            for (size_t j = 0; j < dstBytes / 2; j++) {
                                uint16_t p = d[j];
                                uint8_t r = (p >> 11) & 0x1f;
                                uint8_t g6 = (p >> 5) & 0x3f;
                                uint8_t b = p & 0x1f;
                                d[j] = (r << 10) | ((g6 >> 1) << 5) | b;
                            }
                        }

                        // Copy inner rows (skip context at edges) to destination.
                        // Copy only the data portion of each row; tempBuf rows have trailing padding.
                        const int skipRows = kFilterContextRadius * scale_int;
                        const int copyRows = outdesc.DstH - 2 * skipRows;
                        const int dstStartY = (seamBandStart_ + kFilterContextRadius) * scale_int;

                        if (copyRows > 0) {
                            const int data_bytes_per_row = width_ * bpp * scale_int;
                            uint8_t* dstRow = seamDst_ + outstride * dstStartY;
                            const uint8_t* srcRow = tempBuf.data() + plugin_outstride * skipRows;

                            for (int y = 0; y < copyRows; y++) {
                                memcpy(dstRow, srcRow, data_bytes_per_row);
                                dstRow += outstride;
                                srcRow += plugin_outstride;
                            }
                        }
                    } else {
                        // Built-in filter seam fix
                        ProcessSeamBand(seamSrc_, instride, seamDst_, outstride,
                                        width_, height_real, seamBandStart_, seamBandEnd_, scale_int);
                    }
                }

                done_->Post();
                continue;
            }

            // Phase 2: Apply filter to IFB-processed source
            // Advance source pointer to this thread's band
            src_ += src_offset;

            // For single-threaded mode, apply IFB before filter
            // (multi-threaded mode handles this in separate phase)
            if (nthreads_ == 1) {
                const config::Interframe ifb_option = OPTION(kDispIFB);
                if (ifb_option != config::Interframe::kNone) {
                    // src_ already points to this thread's band (after src_offset)
                    // IFB operates on source at native resolution before scaling
                    if (ifb_option == config::Interframe::kSmart) {
                        InterframeManager::Instance().ApplySmartIBRegion(
                            src_, instride, width_, procy, height_,
                            systemColorDepth, threadno_);
                    } else if (ifb_option == config::Interframe::kMotionBlur) {
                        InterframeManager::Instance().ApplyMotionBlurRegion(
                            src_, instride, width_, procy, height_,
                            systemColorDepth, threadno_);
                    }
                }
            }

            // Cache filter option for this frame
            const config::Filter filter_option = OPTION(kDispFilter);

            if (filter_option == config::Filter::kNone) {
                // No filter, but IFB might be enabled (already applied above).
                // We need to copy the (possibly IFB-modified) source to destination.
                // src_ points to this thread's band in the source buffer.
                // dest points to this thread's band in the destination buffer.
                for (int y = 0; y < height_; y++) {
                    memcpy(dest + y * outstride, src_ + y * instride, width_ * outbpp);
                }

                if (nthreads_ == 1)
                    return 0;

                done_->Post();
                continue;
            }

            // Apply filter - optimized path selection
            if (is_32bit) {
                // Direct 32bpp path - no conversion needed
                ApplyFilterOptimized(instride, outstride, filter_option);
            } else if (filter_option == config::Filter::kPlugin) {
                // Plugin filters handle their own format - pass data directly without conversion
                // Plugins with RPI_OUT_565 flag expect 16-bit data, not 32-bit converted data
                ApplyFilterOptimized(instride, outstride, filter_option);
            } else {
                // Non-32bpp with built-in filter: convert to 32bpp, apply filter, convert back
                // Save current shift values (set for original depth)
                const int saved_red_shift = systemRedShift;
                const int saved_green_shift = systemGreenShift;
                const int saved_blue_shift = systemBlueShift;
                const int saved_rgb_low_bits_mask = RGB_LOW_BITS_MASK;

                // Set shift values for 32bpp format that filters expect
#if wxBYTE_ORDER == wxLITTLE_ENDIAN
                systemRedShift = 19;
                systemGreenShift = 11;
                systemBlueShift = 3;
                RGB_LOW_BITS_MASK = 0x00010101;
#else
                systemRedShift = 27;
                systemGreenShift = 19;
                systemBlueShift = 11;
                RGB_LOW_BITS_MASK = 0x01010100;
#endif

                // Convert input to 32bpp (buffers pre-allocated above)
                int pos = total_width32;  // Start at row 1 (skip row 0 for top border)

                if (is_8bit) {
                    // 8bpp format: RRRGGGBB (3 red, 3 green, 2 blue bits)
                    int src_pos = 0;
                    for (int y = 0; y < height_; y++) {
                        const int left_border_pos = pos++;
                        for (int x = 0; x < width_; x++) {
                            const uint8_t src_val = src_[src_pos++];
                            const uint8_t r3 = (src_val >> 5) & 0x7;
                            const uint8_t g3 = (src_val >> 2) & 0x7;
                            const uint8_t b2 = src_val & 0x3;
                            const uint8_t r8 = (r3 << 5) | (r3 << 2) | (r3 >> 1);
                            const uint8_t g8 = (g3 << 5) | (g3 << 2) | (g3 >> 1);
                            const uint8_t b8 = (b2 << 6) | (b2 << 4) | (b2 << 2) | b2;
#if wxBYTE_ORDER == wxLITTLE_ENDIAN
                            src2_[pos++] = b8 | (g8 << 8) | (r8 << 16);
#else
                            src2_[pos++] = (r8 << 24) | (g8 << 16) | (b8 << 8);
#endif
                        }
                        src2_[left_border_pos] = src2_[left_border_pos + 1];
                        src2_[pos] = src2_[pos - 1];
                        pos++;
                        src_pos += inrb;
                    }
                } else if (is_16bit) {
                    // 16bpp RGB555 format
                    const uint16_t* src16 = reinterpret_cast<const uint16_t*>(src_);
                    int src_pos = 0;
                    for (int y = 0; y < height_; y++) {
                        const int left_border_pos = pos++;
                        for (int x = 0; x < width_; x++) {
                            const uint16_t src_val = src16[src_pos++];
                            const uint8_t r5 = (src_val >> 10) & 0x1f;
                            const uint8_t g5 = (src_val >> 5) & 0x1f;
                            const uint8_t b5 = src_val & 0x1f;
                            const uint8_t r8 = (r5 << 3) | (r5 >> 2);
                            const uint8_t g8 = (g5 << 3) | (g5 >> 2);
                            const uint8_t b8 = (b5 << 3) | (b5 >> 2);
#if wxBYTE_ORDER == wxLITTLE_ENDIAN
                            src2_[pos++] = b8 | (g8 << 8) | (r8 << 16);
#else
                            src2_[pos++] = (r8 << 24) | (g8 << 16) | (b8 << 8);
#endif
                        }
                        src2_[left_border_pos] = src2_[left_border_pos + 1];
                        src2_[pos] = src2_[pos - 1];
                        pos++;
                        src_pos += inrb;
                    }
                } else {  // is_24bit
                    int src_pos = 0;
                    for (int y = 0; y < height_; y++) {
                        const int left_border_pos = pos++;
                        for (int x = 0; x < width_; x++) {
#if wxBYTE_ORDER == wxLITTLE_ENDIAN
                            src2_[pos++] = src_[src_pos] | (src_[src_pos+1] << 8) | (src_[src_pos+2] << 16);
#else
                            src2_[pos++] = (src_[src_pos] << 24) | (src_[src_pos+1] << 16) | (src_[src_pos+2] << 8);
#endif
                            src_pos += 3;
                        }
                        src2_[left_border_pos] = src2_[left_border_pos + 1];
                        src2_[pos] = src2_[pos - 1];
                        pos++;
                    }
                }

                // Fill top border row by duplicating row 1
                memcpy(src2_, src2_ + total_width32, total_width32 * sizeof(uint32_t));

                // Fill bottom 2 extra rows by duplicating the last image row
                const int last_row_offset = total_width32 * height_;
                memcpy(src2_ + last_row_offset + total_width32, src2_ + last_row_offset, total_width32 * sizeof(uint32_t));
                memcpy(src2_ + last_row_offset + total_width32 * 2, src2_ + last_row_offset, total_width32 * sizeof(uint32_t));

                // Point to first image pixel (skip top row + left border)
                uint8_t* filter_src = reinterpret_cast<uint8_t*>(src2_ + total_width32 + 1);
                // Skip top border rows in output
                const int dst_offset = scaled_width * scale_int;
                uint8_t* filter_dst = reinterpret_cast<uint8_t*>(dst2_ + dst_offset);

                if (filter_option == config::Filter::kPlugin) {
                    // Plugin filter - use proxy or direct plugin call
                    // This path uses internal 32-bit buffers (src2_, dst2_) which have
                    // no side borders, only top border rows. The borders are handled
                    // during conversion back to original format.
                    RENDER_PLUGIN_OUTP outdesc;
                    outdesc.Size = sizeof(outdesc);
                    outdesc.Flags = rpi_->Flags;
                    outdesc.SrcPtr = filter_src;
                    outdesc.SrcPitch = instride32;
                    outdesc.SrcW = width_;
                    outdesc.SrcH = height_;
                    outdesc.DstPtr = filter_dst;
                    // dst2_ is tightly packed (no side borders), so DstPitch = data width
                    const int data_bytes_per_row32 = scaled_width * 4;
                    outdesc.DstPitch = data_bytes_per_row32;
                    outdesc.DstW = scaled_width;
                    outdesc.DstH = static_cast<int>(height_ * scale_);
                    outdesc.OutW = outdesc.DstW;
                    outdesc.OutH = outdesc.DstH;

#ifdef VBAM_RPI_PROXY_SUPPORT
                    if (rpi_proxy_client_) {
                        // Pass outstride32 as actualDstPitch for consistency
                        // (they're equal since dst2_ has no side borders)
                        rpi_proxy_client_->ApplyFilter(&outdesc, static_cast<uint32_t>(threadno_), outstride32);
                    } else
#endif
                    {
                        std::lock_guard<std::mutex> lock(g_rpi_output_mutex);
                        rpi_->Output(&outdesc);
                    }
                } else {
                    // Built-in filter
                    ApplyFilter32(filter_src, instride32, delta_, filter_dst, outstride32, width_, height_);
                }

                // Convert 32bpp output back to original depth
                const uint32_t* dst32 = dst2_ + dst_offset;

                if (is_8bit) {
                    int dst_pos = 0;
                    pos = 0;
                    for (int y = 0; y < scaled_height; y++) {
                        for (int x = 0; x < scaled_width; x++) {
                            const uint32_t color = dst32[dst_pos++];
#if wxBYTE_ORDER == wxLITTLE_ENDIAN
                            const uint8_t r8 = (color >> 16) & 0xff;
                            const uint8_t g8 = (color >> 8) & 0xff;
                            const uint8_t b8 = color & 0xff;
#else
                            const uint8_t r8 = (color >> 24) & 0xff;
                            const uint8_t g8 = (color >> 16) & 0xff;
                            const uint8_t b8 = (color >> 8) & 0xff;
#endif
                            dest[pos++] = ((r8 >> 5) << 5) | ((g8 >> 5) << 2) | (b8 >> 6);
                        }
                        pos += scaled_border_dest;
                    }
                } else if (is_16bit) {
                    uint16_t* dest16 = reinterpret_cast<uint16_t*>(dest);
                    int dst_pos = 0;
                    pos = 0;
                    for (int y = 0; y < scaled_height; y++) {
                        for (int x = 0; x < scaled_width; x++) {
                            const uint32_t color = dst32[dst_pos++];
#if wxBYTE_ORDER == wxLITTLE_ENDIAN
                            const uint8_t r8 = (color >> 16) & 0xff;
                            const uint8_t g8 = (color >> 8) & 0xff;
                            const uint8_t b8 = color & 0xff;
#else
                            const uint8_t r8 = (color >> 24) & 0xff;
                            const uint8_t g8 = (color >> 16) & 0xff;
                            const uint8_t b8 = (color >> 8) & 0xff;
#endif
                            dest16[pos++] = ((r8 >> 3) << 10) | ((g8 >> 3) << 5) | (b8 >> 3);
                        }
                        pos += scaled_border_dest;
                    }
                } else {  // is_24bit
                    int dst_pos = 0;
                    pos = 0;
                    for (int y = 0; y < scaled_height; y++) {
                        for (int x = 0; x < scaled_width; x++) {
                            const uint8_t* src_pixel = reinterpret_cast<const uint8_t*>(&dst32[dst_pos++]);
                            dest[pos] = src_pixel[0];
                            dest[pos+1] = src_pixel[1];
                            dest[pos+2] = src_pixel[2];
                            pos += 3;
                        }
                    }
                }

                // Restore original shift values
                systemRedShift = saved_red_shift;
                systemGreenShift = saved_green_shift;
                systemBlueShift = saved_blue_shift;
                RGB_LOW_BITS_MASK = saved_rgb_low_bits_mask;
            }

            if (nthreads_ == 1) {
                return 0;
            }

            done_->Post();
        }

#ifdef VBAM_RPI_PROXY_SUPPORT
        // Stop the proxy thread when this filter thread is done
        if (proxy_thread_started_) {
            rpi_proxy_client_->StopThread(static_cast<uint32_t>(threadno_));
            proxy_thread_started_ = false;
        }
#endif

        return 0;
    }

private:
    // Optimized filter application with cached option
    void ApplyFilterOptimized(int instride, int outstride, config::Filter filter_option) {
        if (filter_option == config::Filter::kPlugin) {
            // Stage source/destination into buffers suitable for RPI plugins.
            // Different plugin flavors assume different source strides:
            // - xBR-family (v2+, no MMX) access SrcPtr-4 and wider context — 2 pixels
            //   of padding per side (edge-replicated) keeps those reads on sane data.
            // - v1 plugins (e.g. BilinearPlus) hardcode `SrcPtr + SrcW*2 + 4` for the
            //   next row — that matches master's `(SrcW + 2) * bpp` with 1-pixel pads.
            // - MMX/SSE plugins (e.g. mdntsc_sse) advance the source by `SrcW * 2`
            //   per row (tight), so for those we must use zero padding.
            const unsigned int pluginVersion = rpi_->Flags & 0xff;
            const bool pluginIsMMX = (rpi_->Flags & RPI_MMX_USED) != 0;
            int kPluginCtx;
            if (pluginIsMMX) {
                kPluginCtx = 0;
            } else if (pluginVersion == 1) {
                kPluginCtx = 1;
            } else {
                kPluginCtx = 2;
            }
            const int bpp = rpi_bpp_;
            const int paddedWidth = width_ + 2 * kPluginCtx;
            const int paddedHeight = height_ + 2 * kPluginCtx;
            const int paddedSrcPitch = paddedWidth * bpp;
            const int scaled_width = static_cast<int>(width_ * scale_);
            const int scaled_height = static_cast<int>(height_ * scale_);

            RENDER_PLUGIN_OUTP outdesc;
            outdesc.Size = sizeof(outdesc);
            outdesc.Flags = rpi_->Flags;
            outdesc.SrcPitch = paddedSrcPitch;
            outdesc.SrcW = width_;
            outdesc.SrcH = height_;
            outdesc.DstPitch = outstride;
            outdesc.DstW = scaled_width;
            outdesc.DstH = scaled_height;
            outdesc.OutW = outdesc.DstW;
            outdesc.OutH = outdesc.DstH;

            // Zero-copy path for the proxy: write format-converted source
            // straight into the shared-memory src buffer and have the plugin
            // write output straight into the shared-memory dst buffer. Reading
            // back then folds the format conversion with the shared-mem read,
            // avoiding two full-frame memcpys per call.
            const size_t srcBytes = static_cast<size_t>(paddedSrcPitch) * paddedHeight;
#ifdef VBAM_RPI_PROXY_SUPPORT
            const size_t dstBytes = static_cast<size_t>(outstride) * scaled_height;
#endif
            uint8_t* src_buffer = nullptr;
            uint8_t* dst_buffer = dst_;
            std::vector<uint8_t> src_fallback;
#ifdef VBAM_RPI_PROXY_SUPPORT
            if (rpi_proxy_client_ &&
                srcBytes <= rpi_proxy::RpiProxyClient::kMaxBufferBytes &&
                dstBytes <= rpi_proxy::RpiProxyClient::kMaxBufferBytes) {
                src_buffer = static_cast<uint8_t*>(
                    rpi_proxy_client_->GetSrcBuffer(static_cast<uint32_t>(threadno_)));
                uint8_t* shared_dst = static_cast<uint8_t*>(
                    rpi_proxy_client_->GetDstBuffer(static_cast<uint32_t>(threadno_)));
                if (src_buffer && shared_dst) {
                    dst_buffer = shared_dst;
                } else {
                    src_buffer = nullptr;  // Proxy not ready — fall back to staging.
                }
            }
#endif
            if (!src_buffer) {
                src_fallback.resize(srcBytes);
                src_buffer = src_fallback.data();
            }

            auto convert_pixel_888 = [](uint32_t p) -> uint32_t {
                return 0xFF000000u | (p & 0x0000FF00u) | ((p >> 16) & 0xFFu) | ((p & 0xFFu) << 16);
            };
            auto convert_pixel_565 = [](uint16_t p) -> uint16_t {
                uint8_t r = (p >> 10) & 0x1f;
                uint8_t g = (p >> 5) & 0x1f;
                uint8_t b = p & 0x1f;
                return (r << 11) | (((g << 1) | (g >> 4)) << 5) | b;
            };

            if (bpp == 4) {
                for (int y = 0; y < height_; y++) {
                    const uint32_t* s = reinterpret_cast<const uint32_t*>(src_ + y * instride);
                    uint32_t* d = reinterpret_cast<uint32_t*>(
                        src_buffer + (y + kPluginCtx) * paddedSrcPitch) + kPluginCtx;
                    for (int x = 0; x < width_; x++) d[x] = convert_pixel_888(s[x]);
                    // Replicate edge pixels into horizontal padding.
                    for (int k = 1; k <= kPluginCtx; k++) { d[-k] = d[0]; d[width_ - 1 + k] = d[width_ - 1]; }
                }
            } else if (rpi_using_rgb565_) {
                outdesc.Flags = (outdesc.Flags & ~RPI_555_SUPP) | RPI_565_SUPP;
                for (int y = 0; y < height_; y++) {
                    const uint16_t* s = reinterpret_cast<const uint16_t*>(src_ + y * instride);
                    uint16_t* d = reinterpret_cast<uint16_t*>(
                        src_buffer + (y + kPluginCtx) * paddedSrcPitch) + kPluginCtx;
                    for (int x = 0; x < width_; x++) d[x] = convert_pixel_565(s[x]);
                    for (int k = 1; k <= kPluginCtx; k++) { d[-k] = d[0]; d[width_ - 1 + k] = d[width_ - 1]; }
                }
            } else if (bpp == 2) {
                for (int y = 0; y < height_; y++) {
                    const uint16_t* s = reinterpret_cast<const uint16_t*>(src_ + y * instride);
                    uint16_t* d = reinterpret_cast<uint16_t*>(
                        src_buffer + (y + kPluginCtx) * paddedSrcPitch) + kPluginCtx;
                    memcpy(d, s, width_ * 2);
                    for (int k = 1; k <= kPluginCtx; k++) { d[-k] = d[0]; d[width_ - 1 + k] = d[width_ - 1]; }
                }
            }

            // Replicate the first/last real rows into the vertical padding rows.
            for (int k = 1; k <= kPluginCtx; k++) {
                memcpy(src_buffer + (kPluginCtx - k) * paddedSrcPitch,
                       src_buffer + kPluginCtx * paddedSrcPitch,
                       paddedSrcPitch);
                memcpy(src_buffer + (kPluginCtx + height_ - 1 + k) * paddedSrcPitch,
                       src_buffer + (kPluginCtx + height_ - 1) * paddedSrcPitch,
                       paddedSrcPitch);
            }

            outdesc.SrcPtr = src_buffer + kPluginCtx * paddedSrcPitch + kPluginCtx * bpp;
            outdesc.DstPtr = dst_buffer;

            bool plugin_ok = true;
#ifdef VBAM_RPI_PROXY_SUPPORT
            if (rpi_proxy_client_) {
                plugin_ok = rpi_proxy_client_->ApplyFilter(
                    &outdesc, static_cast<uint32_t>(threadno_), 0);
            } else
#endif
            {
                std::lock_guard<std::mutex> lock(g_rpi_output_mutex);
                rpi_->Output(&outdesc);
            }

            if (!plugin_ok) {
                // Plugin call failed — dst_ and the shared dst buffer are in an
                // undefined state. Leaving dst_ unchanged keeps the last good
                // frame on screen rather than rendering garbage.
                return;
            }

            // Convert plugin output to VBA-M's native format, reading from
            // dst_buffer (shared or panel) and writing to dst_ (panel). When
            // dst_buffer == dst_ (no zero-copy), the loop effectively works
            // in place; when dst_buffer is the shared buffer this loop also
            // folds in the copy-back step, so no extra pass is needed.
            if (bpp == 4) {
                for (int y = 0; y < scaled_height; y++) {
                    const uint32_t* s = reinterpret_cast<const uint32_t*>(dst_buffer + y * outstride);
                    uint32_t* d = reinterpret_cast<uint32_t*>(dst_ + y * outstride);
                    for (int x = 0; x < scaled_width; x++) {
                        uint32_t p = s[x];
                        d[x] = (p & 0x0000FF00u) | ((p >> 16) & 0xFFu) | ((p & 0xFFu) << 16);
                    }
                }
            } else if (rpi_using_rgb565_) {
                for (int y = 0; y < scaled_height; y++) {
                    const uint16_t* s = reinterpret_cast<const uint16_t*>(dst_buffer + y * outstride);
                    uint16_t* d = reinterpret_cast<uint16_t*>(dst_ + y * outstride);
                    for (int x = 0; x < scaled_width; x++) {
                        uint16_t p = s[x];
                        uint8_t r = (p >> 11) & 0x1f;
                        uint8_t g6 = (p >> 5) & 0x3f;
                        uint8_t b = p & 0x1f;
                        d[x] = (r << 10) | ((g6 >> 1) << 5) | b;
                    }
                }
            } else if (dst_buffer != dst_) {
                // 555 plugin on the zero-copy path: no format conversion, but
                // we still need to move the scaled image out of shared memory
                // into the panel buffer.
                const int data_bytes = scaled_width * bpp;
                for (int y = 0; y < scaled_height; y++) {
                    memcpy(dst_ + y * outstride, dst_buffer + y * outstride, data_bytes);
                }
            }
        } else {
            ApplyFilter32(src_, instride, delta_, dst_, outstride, width_, height_);
        }
    }
};

void DrawingPanelBase::DrawArea(uint8_t** data)
{
    // double-buffer buffer:
    //   if filtering, this is filter output, retained for redraws
    //   if not filtering, we still retain current image for redraws
    int outbpp = panel_color_depth_ >> 3;
    int inrb = (panel_color_depth_ == 8) ? 4 : (panel_color_depth_ == 16) ? 2 : (panel_color_depth_ == 24) ? 0 : 1;
    int outstride = std::ceil((width + inrb) * outbpp * scale);


    if (OPTION(kDispFilter) != config::Filter::kNone ||
        OPTION(kDispIFB) != config::Interframe::kNone) {
        if (!pixbuf2) {
            int allocstride = outstride, alloch = height;

            // gb may write borders, so allocate enough for them
            if (width == GameArea::GBWidth && height == GameArea::GBHeight) {
                allocstride = std::ceil((GameArea::SGBWidth + inrb) * outbpp * scale);
                alloch = GameArea::SGBHeight;
            }

            pixbuf2 = (uint8_t*)calloc(allocstride, (int)std::ceil((alloch + 2) * scale));
        }
        todraw = pixbuf2;
    } else {
        // No filter: use g_pix directly, don't allocate or swap buffers
        todraw = *data;
    }

    // Use multiple threads for filter processing (up to 8).
    // Force single-threaded for MT plugins - they handle parallelism internally
    // and their thread decomposition fails on small band sizes.
    const int max_threads = rpi_is_mt_ ? 1 : GetMaxFilterThreads();

    // Compute stride for InterframeManager initialization
    int instride = (width + inrb) * (panel_color_depth_ >> 3);

    // First, apply filters, if applicable, in parallel, if enabled
    // FIXME: && (gopts.ifb != FF_MOTION_BLUR || !renderer_can_motion_blur)
    if (OPTION(kDispFilter) != config::Filter::kNone ||
        OPTION(kDispIFB) != config::Interframe::kNone) {
        if (nthreads != max_threads) {
            if (nthreads) {
                if (nthreads > 1)
                    for (int i = 0; i < nthreads; i++) {
                        threads[i].lock_.Lock();
                        threads[i].src_ = NULL;
                        threads[i].sig_.Signal();
                        threads[i].lock_.Unlock();
                        threads[i].Wait();
                    }

                delete[] threads;
            }

            nthreads = max_threads;
            threads = new FilterThread[nthreads];

            // Initialize InterframeManager for per-region IFB processing (only if IFB enabled)
            if (OPTION(kDispIFB) != config::Interframe::kNone) {
                InterframeManager::Instance().Init(width, height, instride, nthreads);
            }

            // first time around, no threading in order to avoid
            // static initializer conflicts
            threads[0].threadno_ = 0;
            threads[0].nthreads_ = 1;
            threads[0].width_ = width;
            threads[0].height_ = height;
            threads[0].scale_ = scale;
            threads[0].src_ = *data;
            threads[0].dst_ = todraw;
            threads[0].delta_ = delta;
            threads[0].rpi_ = rpi_;
            threads[0].phase_ = FilterThread::Phase::Filter;
            threads[0].rpi_bpp_ = rpi_bpp_;
            threads[0].rpi_using_rgb565_ = rpi_using_rgb565_;
            threads[0].panel_color_depth_ = panel_color_depth_;
#ifdef VBAM_RPI_PROXY_SUPPORT
            threads[0].rpi_proxy_client_ = rpi_proxy_client_;
#endif
            threads[0].Entry();
            // IFB is handled inside Entry() for single-threaded mode

            // go ahead and start the threads up, though
            if (nthreads > 1) {
                for (int i = 0; i < nthreads; i++) {
                    threads[i].threadno_ = i;
                    threads[i].nthreads_ = nthreads;
                    threads[i].width_ = width;
                    threads[i].height_ = height;
                    threads[i].scale_ = scale;
                    threads[i].dst_ = todraw;
                    threads[i].delta_ = delta;
                    threads[i].rpi_ = rpi_;
                    threads[i].rpi_bpp_ = rpi_bpp_;
                    threads[i].rpi_using_rgb565_ = rpi_using_rgb565_;
                    threads[i].panel_color_depth_ = panel_color_depth_;
#ifdef VBAM_RPI_PROXY_SUPPORT
                    threads[i].rpi_proxy_client_ = rpi_proxy_client_;
#endif
                    threads[i].done_ = &filt_done;
                    threads[i].phase_ = FilterThread::Phase::Filter;
                    threads[i].ready_ = &filt_ready;
                    threads[i].Create();
                    threads[i].Run();
                }
                // Wait for all threads to signal they're ready before returning
                // This prevents lost signals on the next frame
                for (int i = 0; i < nthreads; i++) {
                    filt_ready.Wait();
                }
            }
        } else if (nthreads == 1) {
            threads[0].threadno_ = 0;
            threads[0].nthreads_ = 1;
            threads[0].width_ = width;
            threads[0].height_ = height;
            threads[0].scale_ = scale;
            threads[0].src_ = *data;
            threads[0].dst_ = todraw;
            threads[0].delta_ = delta;
            threads[0].rpi_ = rpi_;
            threads[0].phase_ = FilterThread::Phase::Filter;
            threads[0].rpi_bpp_ = rpi_bpp_;
            threads[0].rpi_using_rgb565_ = rpi_using_rgb565_;
            threads[0].panel_color_depth_ = panel_color_depth_;
#ifdef VBAM_RPI_PROXY_SUPPORT
            threads[0].rpi_proxy_client_ = rpi_proxy_client_;
#endif
            threads[0].Entry();
            // IFB is handled inside Entry() for single-threaded mode
        } else {
            // Phase 1: Run IFB threads first (before filters)
            if (OPTION(kDispIFB) != config::Interframe::kNone) {
                for (int i = 0; i < nthreads; i++) {
                    threads[i].phase_ = FilterThread::Phase::IFB;
                    threads[i].lock_.Lock();
                    threads[i].src_ = *data;
                    threads[i].sig_.Signal();
                    threads[i].lock_.Unlock();
                }

                for (int i = 0; i < nthreads; i++)
                    filt_done.Wait();
            }

            // Phase 2: Run filter threads (on IFB-processed source)
            for (int i = 0; i < nthreads; i++) {
                threads[i].phase_ = FilterThread::Phase::Filter;
                threads[i].lock_.Lock();
                threads[i].src_ = *data;
                threads[i].sig_.Signal();
                threads[i].lock_.Unlock();
            }

            for (int i = 0; i < nthreads; i++)
                filt_done.Wait();

            // Fix seams between thread regions by re-processing bands around boundaries
            // This runs in parallel using the existing filter threads
            // Only runs here (not on first frame when threads are just starting)
            if (nthreads > 1 && OPTION(kDispFilter) != config::Filter::kNone) {
                // Collect seam positions and merge overlapping bands
                std::vector<std::pair<int, int>> seamBands;  // (startY, endY) in source coords

                for (int i = 1; i < nthreads; i++) {
                    int seamY = height * i / nthreads;
                    int bandStart = std::max(0, seamY - kFilterContextRadius - kSeamMarginRows);
                    int bandEnd = std::min(height, seamY + kFilterContextRadius + kSeamMarginRows);

                    // Merge with previous band if overlapping
                    if (!seamBands.empty() && bandStart <= seamBands.back().second) {
                        seamBands.back().second = std::max(seamBands.back().second, bandEnd);
                    } else {
                        seamBands.push_back({bandStart, bandEnd});
                    }
                }

                // Assign seam bands to threads and run in parallel
                const int scale_int = static_cast<int>(scale);
                // Source base: skip 1 border row
                uint8_t* src_base = *data + instride;
                // Dest base: skip 1*scale border rows
                uint8_t* dst_base = todraw + outstride * scale_int;

                const int numBands = static_cast<int>(seamBands.size());
                for (int i = 0; i < nthreads; i++) {
                    threads[i].phase_ = FilterThread::Phase::SeamFix;
                    // Assign band to thread (some threads may have no work)
                    if (i < numBands) {
                        threads[i].seamBandStart_ = seamBands[i].first;
                        threads[i].seamBandEnd_ = seamBands[i].second;
                    } else {
                        threads[i].seamBandStart_ = -1;  // No work for this thread
                        threads[i].seamBandEnd_ = -1;
                    }
                    threads[i].seamSrc_ = src_base;
                    threads[i].seamDst_ = dst_base;
                    threads[i].lock_.Lock();
                    threads[i].src_ = src_base;  // Keep src_ non-null to avoid thread exit
                    threads[i].sig_.Signal();
                    threads[i].lock_.Unlock();
                }

                for (int i = 0; i < nthreads; i++)
                    filt_done.Wait();
            }
        }
    }

    // Note: We do NOT modify *data (which is g_pix) here.
    // g_pix must always point to the emulator's allocated buffer.
    // Modifying it would cause the panel destructor to free the wrong buffer,
    // leaving g_pix as a dangling pointer after panel reset.

    // draw OSD text old-style (directly into output buffer), if needed
    // new style flickers too much, so we'll stick to this for now
    if (wxGetApp().frame->IsFullScreen() || !OPTION(kPrefDisableStatus)) {
        GameArea* panel = wxGetApp().frame->GetPanel();

        int scaled_width = (int)std::ceil(width * scale);
        int scaled_height = (int)std::ceil(height * scale);

        if (panel->osdstat.size()) {
            wxString statText = wxString::FromUTF8(panel->osdstat.c_str());
            // Position closer to top-left for better visibility at native resolution
            // At scale=1.0 (native GBA/GB res): (4, 4) instead of (10, 20)
            int x = (int)std::ceil(4 * scale);
            int y = (int)std::ceil(4 * scale);
            drawTextWx(todraw + outstride * (panel_color_depth_ != 24), outstride,
                x, y, statText, scaled_width, scaled_height, scale, panel_color_depth_);
        }

        if (!panel->osdtext.empty()) {
            if (systemGetClock() - panel->osdtime < OSD_TIME) {
                // Position near bottom of screen, scaled with filter
                // At scale=1.0 (native res): positioned to allow 3 lines of text
                int x = (int)std::ceil(4 * scale);
                int osd_offset = (panel->game_type() == IMAGE_GB) ? 106 : 64;
                int y = scaled_height - (int)std::ceil(osd_offset * scale);
                drawTextWx(todraw + outstride * (systemColorDepth != 24), outstride,
                    x, y, panel->osdtext, scaled_width, scaled_height, scale, panel_color_depth_);
            } else
                panel->osdtext.clear();
        }
    }

    // Guard against NULL todraw (can happen if calloc failed or scale is 0)
    if (!todraw) {
        return;
    }

    // next, draw the frame (queue a PaintEv) Refresh must be used under
    // Wayland or nothing is drawn.
#ifndef NO_WAYLAND
    if (wxGetApp().UsingWayland())
        GetWindow()->Refresh();
    else {
#endif
        DrawingPanelBase* panel = wxGetApp().frame->GetPanel()->panel;
        if (panel) {
            wxWindow* win = panel->GetWindow();
#ifdef __WXMSW__
            // Verify window handle is valid before creating DC
            // This prevents crashes if the panel is being destroyed
            HWND hwnd = win ? win->GetHWND() : nullptr;
            if (!hwnd || !::IsWindow(hwnd)) {
                return;
            }
#endif
            if (win) {
                wxClientDC dc(win);
                panel->DrawArea(dc);
            }
        }
#ifndef NO_WAYLAND
    }
#endif

    // finally, draw on-screen text using wx method, if possible
    // this method flickers too much right now
    //DrawOSD(dc);
}

void DrawingPanelBase::DrawOSD(wxWindowDC& dc)
{
    // draw OSD message, if available
    // doing this here rather than using drawText() directly into the screen
    // image gives us 2 advantages:
    //   - non-ASCII text in messages
    //   - message is always default font size, regardless of screen stretch
    // and one known disadvantage:
    //   - 3d renderers may overwrite text asynchronously
    // Until I go through the trouble of making this render to a bitmap, and
    // then using the bitmap as a texture for the 3d renderers or rendering
    // directly into the output like DrawText, this is only enabled for
    // non-3d renderers.
    GameArea* panel = wxGetApp().frame->GetPanel();
    dc.SetTextForeground(wxColour(255, 0, 0, 255));
    dc.SetTextBackground(wxColour(0, 0, 0, 0));
    dc.SetUserScale(1.0, 1.0);

    if (panel->osdstat.size())
        dc.DrawText(panel->osdstat, 10, 20);

    if (!panel->osdtext.empty()) {
        if (systemGetClock() - panel->osdtime >= OSD_TIME) {
            panel->osdtext.clear();
            wxGetApp().frame->PopStatusText();
        }
    }

    if (!OPTION(kPrefDisableStatus) && !panel->osdtext.empty()) {
        wxSize asz = dc.GetSize();
        wxString msg = panel->osdtext;
        int lw, lh;
        int mlw = asz.GetWidth() - 20;
        dc.GetTextExtent(msg, &lw, &lh);

        if (lw <= mlw)
            dc.DrawText(msg, 10, asz.GetHeight() - 16 - lh);
        else {
            // Since font is likely proportional, the only way to
            // find amt of text that will fit on a line is to search
            wxArrayInt llen; // length of each line, in chars

            for (size_t off = 0; off < msg.size();) {
// One way would be to bsearch on GetTextExtent() looking for
// best fit.
// Another would be to use GetPartialTextExtents and search
// the resulting array.
// GetPartialTextExtents may be inaccurate, but calling it
// once per line may be more efficient than calling
// GetTextExtent log2(remaining_len) times per line.
#if 1
                wxArrayInt clen;
                dc.GetPartialTextExtents(msg.substr(off), clen);
#endif
                int lenl = 0, lenh = msg.size() - off - 1, len;

                do {
                    len = (lenl + lenh) / 2;
#if 1
                    lw = clen[len];
#else
                    dc.GetTextExtent(msg.substr(off, len), &lw, NULL);
#endif

                    if (lw > mlw)
                        lenh = len - 1;
                    else if (lw < mlw)
                        lenl = len + 1;
                    else
                        break;
                } while (lenh >= lenl);

                if (lw <= mlw)
                    len++;

                off += len;
                llen.push_back(len);
            }

            int nlines = llen.size();
            int cury = asz.GetHeight() - 14 - nlines * (lh + 2);

            for (int off = 0, line = 0; line < nlines; off += llen[line], line++, cury += lh + 2)
                dc.DrawText(msg.substr(off, llen[line]), 10, cury);
        }
    }
}

void DrawingPanelBase::OnSize(wxSizeEvent& ev)
{
    ev.Skip();
}

void DrawingPanelBase::StopFilterThreads()
{
    // Stop all filter threads and free thread array
    // This must be called before InterframeCleanup() to prevent threads
    // from accessing InterframeManager after it's been cleaned up
    if (nthreads) {
        if (nthreads > 1)
            for (int i = 0; i < nthreads; i++) {
                threads[i].lock_.Lock();
                threads[i].src_ = NULL;
                threads[i].sig_.Signal();
                threads[i].lock_.Unlock();
                threads[i].Wait();
            }

        delete[] threads;
        threads = nullptr;
        nthreads = 0;
    }
}

DrawingPanelBase::~DrawingPanelBase()
{
    // Stop filter threads if not already stopped
    StopFilterThreads();

    // Now safe to free buffers - threads are stopped
    // pixbuf1 is expected to be g_pix (emulator's buffer) or a previous
    // value of pixbuf2 - either way, we don't free it here.
    // pixbuf2 may be our allocated filter output buffer, OR it may point
    // to g_pix when no filters are used. Only free if it's our buffer.
    extern uint8_t* g_pix;
    if (pixbuf2 && pixbuf2 != g_pix && pixbuf1 != pixbuf2)
    {
        free(pixbuf2);
        pixbuf2 = NULL;
    }

    InterframeCleanup();
#ifdef VBAM_RPI_PROXY_SUPPORT
    // Unload plugin from proxy client if we were using it
    if (using_rpi_proxy_ && rpi_proxy_client_) {
        rpi_proxy_client_->UnloadPlugin();
        using_rpi_proxy_ = false;
        rpi_proxy_client_ = nullptr;
    }
#endif

    disableKeyboardBackgroundInput();
}


BEGIN_EVENT_TABLE(SDLDrawingPanel, wxPanel)
EVT_PAINT(SDLDrawingPanel::PaintEv)
END_EVENT_TABLE()

SDLDrawingPanel::SDLDrawingPanel(wxWindow* parent, int _width, int _height)
    : DrawingPanel(parent, _width, _height)
{
    memset(delta, 0xff, sizeof(delta));

    // wxImage is 24-bit RGB, so 24-bit is preferred.  Filters require
    // 16 or 32, though
    if (OPTION(kDispFilter) == config::Filter::kNone &&
        OPTION(kDispIFB) == config::Interframe::kNone) {
        // changing from 32 to 24 does not require regenerating color tables
        systemColorDepth = (OPTION(kBitDepth) + 1) << 3;
    }

    DrawingPanelInit();
}

SDLDrawingPanel::~SDLDrawingPanel()
{
    if (did_init)
    {
        // Tear down in reverse order of creation: texture and renderer (which
        // own the swapchain / CAMetalLayer attached to the borrowed host view)
        // before the window. Destroying the window first orphans the renderer
        // and can leave its layer behind on the host view as stale content.
        if (texture != NULL)
             SDL_DestroyTexture(texture);

        if (renderer != NULL)
             SDL_DestroyRenderer(renderer);

        if (sdlwindow != NULL)
             SDL_DestroyWindow(sdlwindow);
#if defined(__WXMAC__)
        // SDL leaves its SDL3_cocoametalview attached to the borrowed (persistent
        // GameArea) host view on teardown; detach it so its CAMetalLayer's last
        // frame doesn't show through as stale bars after a sub-renderer switch.
        VbamRemoveSdlMetalViews();
#endif

        SDL_QuitSubSystem(SDL_INIT_VIDEO);

#if defined(HAVE_WAYLAND_SUPPORT)
        // Tear down the Wayland subsurface SDL was presenting into, now that SDL
        // has released it (destroy child objects before their globals).
        if (sdl_wl_subsurface_) {
            wl_subsurface_destroy(static_cast<wl_subsurface*>(sdl_wl_subsurface_));
            sdl_wl_subsurface_ = nullptr;
        }
        if (sdl_wl_child_surface_) {
            wl_surface_destroy(static_cast<wl_surface*>(sdl_wl_child_surface_));
            sdl_wl_child_surface_ = nullptr;
        }
        if (sdl_wl_subcompositor_) {
            wl_subcompositor_destroy(static_cast<wl_subcompositor*>(sdl_wl_subcompositor_));
            sdl_wl_subcompositor_ = nullptr;
        }
        if (sdl_wl_compositor_) {
            wl_compositor_destroy(static_cast<wl_compositor*>(sdl_wl_compositor_));
            sdl_wl_compositor_ = nullptr;
        }
        // Destroy the private bind queue last: the globals/subsurface above were
        // assigned to it.
        if (sdl_wl_queue_) {
            wl_event_queue_destroy(static_cast<wl_event_queue*>(sdl_wl_queue_));
            sdl_wl_queue_ = nullptr;
        }
#endif

        did_init = false;
    }
}

void SDLDrawingPanel::EraseBackground(wxEraseEvent& ev)
{
    (void)ev; // unused params
    // do nothing, do not allow propagation
}

void SDLDrawingPanel::PaintEv(wxPaintEvent& ev)
{
    if (!did_init) {
        DrawingPanelInit();
    }

    (void)ev; // unused params

    if (!todraw) {
        // No frame yet. Clear the placeholder through SDL, never with a wx DC:
        // on macOS the GL backends make this view an NSOpenGLView and any wx DC
        // drawing on it spins an endless repaint loop (see DrawingPanelInit), which
        // would also starve emulation so todraw never arrives. SDL_RenderClear is
        // the right way to blank an SDL surface on every platform anyway.
        if (renderer) {
            SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0xFF);
            SDL_RenderClear(renderer);
            SDL_RenderPresent(renderer);
        }
        return;
    }

    DrawArea();
}

#ifdef ENABLE_SDL3
// Pick a 10-bit packed (2101010) texture format the SDL renderer advertises, or
// SDL_PIXELFORMAT_UNKNOWN when it offers none. Used for X11 "deep color" SDR:
// only backends that can render to a 10-bit target qualify.
static SDL_PixelFormat SdlPickDeepColorFormat(SDL_Renderer* r) {
    auto is_2101010 = [](SDL_PixelFormat f) {
        return f == SDL_PIXELFORMAT_XRGB2101010 || f == SDL_PIXELFORMAT_ARGB2101010 ||
               f == SDL_PIXELFORMAT_XBGR2101010 || f == SDL_PIXELFORMAT_ABGR2101010;
    };
    SDL_PropertiesID rp = SDL_GetRendererProperties(r);
    const SDL_PixelFormat* fmts = (const SDL_PixelFormat*)SDL_GetPointerProperty(
        rp, SDL_PROP_RENDERER_TEXTURE_FORMATS_POINTER, NULL);
    for (; fmts && *fmts != SDL_PIXELFORMAT_UNKNOWN; ++fmts)
        if (is_2101010(*fmts))
            return *fmts;
    return SDL_PIXELFORMAT_UNKNOWN;
}

// Pack a color-corrected RGBA8 image (memory order R,G,B,X) into a 2101010
// texture. The 8-bit source is expanded to 10 bits, so the win is a 10-bit
// output surface and 10-bit scaling/blending (less banding), not extra source
// precision -- the same trade-off the OpenGL deep-color path makes.
static void SdlPackDeepColor(const uint8_t* src, int src_stride, int w, int h,
                             SDL_PixelFormat fmt, uint32_t* dst, int dst_stride_px) {
    const bool bgr = (fmt == SDL_PIXELFORMAT_ABGR2101010 ||
                      fmt == SDL_PIXELFORMAT_XBGR2101010);
    for (int y = 0; y < h; ++y) {
        const uint8_t* sp = src + (size_t)y * src_stride;
        uint32_t* dp = dst + (size_t)y * dst_stride_px;
        for (int x = 0; x < w; ++x, sp += 4) {
            uint32_t r = sp[0], g = sp[1], b = sp[2];
            r = (r << 2) | (r >> 6);
            g = (g << 2) | (g >> 6);
            b = (b << 2) | (b >> 6);
            // ARGB2101010 layout: A[31:30] R[29:20] G[19:10] B[9:0]; the BGR
            // variants swap red and blue. Alpha is opaque (3).
            const uint32_t hi = bgr ? b : r;
            const uint32_t lo = bgr ? r : b;
            dp[x] = (3u << 30) | (hi << 20) | (g << 10) | lo;
        }
    }
}
#endif

#if defined(__WXGTK__) && !defined(NO_WAYLAND) && defined(ENABLE_SDL3)
namespace {
// SDL cannot present into GTK's own wl_surface on Wayland (GTK drives it), so we
// hand SDL a dedicated child wl_subsurface instead -- the same approach
// VKDrawingPanel uses for its swapchain. Bound per panel.
struct VbamSdlWlGlobals {
    struct wl_compositor*    comp    = nullptr;
    struct wl_subcompositor* subcomp = nullptr;
};
void vbam_sdl_wl_global(void* data, struct wl_registry* reg, uint32_t name,
                        const char* iface, uint32_t /*version*/) {
    auto* g = static_cast<VbamSdlWlGlobals*>(data);
    if (std::strcmp(iface, "wl_compositor") == 0)
        g->comp = static_cast<struct wl_compositor*>(
            wl_registry_bind(reg, name, &wl_compositor_interface, 1));
    else if (std::strcmp(iface, "wl_subcompositor") == 0)
        g->subcomp = static_cast<struct wl_subcompositor*>(
            wl_registry_bind(reg, name, &wl_subcompositor_interface, 1));
}
void vbam_sdl_wl_global_remove(void*, struct wl_registry*, uint32_t) {}
const struct wl_registry_listener kVbamSdlWlRegistryListener = {
    vbam_sdl_wl_global, vbam_sdl_wl_global_remove
};
}  // namespace
#endif

void SDLDrawingPanel::DrawingPanelInit()
{
    wxString renderer_name = OPTION(kSDLRenderer);

    // Two SDL backends don't work into our borrowed Wayland subsurface: "gpu"
    // presents a black frame (its SDL_GPU swapchain is gated on the window being
    // "shown", which never happens for our custom roleless surface), and
    // "opengles2" fails to create (eglMakeCurrent -> EGL_BAD_SURFACE on the EGL
    // surface SDL makes for our wl_egl_window). The dialog no longer offers
    // either on Wayland, but a saved config may still select one -- substitute
    // the working equivalent (vulkan for gpu, desktop opengl for opengles2) so we
    // never come up black or fail.
    if (IsWayland()) {
        if (renderer_name == wxString("gpu"))
            renderer_name = wxString("vulkan");
        else if (renderer_name == wxString("opengles2"))
            renderer_name = wxString("opengl");
    }

#if defined(__WXMAC__)
    // macOS has no native OpenGL ES, so SDL's "opengles2" renderer fails to
    // create; desktop "opengl" is the working equivalent (and the dialog no
    // longer offers opengles2 here). Substitute it so a saved choice still runs
    // instead of erroring on every launch.
    if (renderer_name == wxString("opengles2"))
        renderer_name = wxString("opengl");

    // The view we hand SDL as its render target: the GL backends need this
    // panel's own (frontmost) view -- SDL turns it into an NSOpenGLView and GL
    // into GameArea would sit under this view -- while metal/gpu/software use the
    // shared GameArea view. SDL then makes whichever we pass the window's
    // contentView; VbamSdlCaptureViewState/ReattachViewState undo that below.
    sdl_cocoa_view_ = (renderer_name == wxString("opengl"))
                          ? GetHandle()
                          : wxGetApp().frame->GetPanel()->GetHandle();
#endif

#ifdef ENABLE_SDL3
    SDL_PropertiesID props = SDL_CreateProperties();
#endif
#ifdef __WXGTK__
    GtkWidget *widget = wxGetApp().frame->GetPanel()->GetHandle();
    gtk_widget_realize(widget);
    XID xid = 0;
#ifndef NO_WAYLAND
#ifdef ENABLE_SDL3
    struct wl_surface *wayland_surface = NULL;
    struct wl_display *wayland_display = NULL;

    if (GDK_IS_WAYLAND_WINDOW(gtk_widget_get_window(widget))) {
        wayland_display = gdk_wayland_display_get_wl_display(gtk_widget_get_display(widget));
        struct wl_surface* gtk_surface =
            gdk_wayland_window_get_wl_surface(gtk_widget_get_window(widget));

        // SDL cannot present into GTK's own surface; create a dedicated child
        // subsurface and hand SDL that instead (mirrors VKDrawingPanel).
        //
        // Bind our globals on a PRIVATE event queue. wl_display_roundtrip()
        // dispatches the display's *default* queue -- the one GTK/GDK owns and
        // pumps from its own main loop. Driving it from here reenters GDK's
        // dispatch and corrupts libwayland's reader bookkeeping, which surfaces
        // as GDK's "Error 22 (Invalid argument) dispatching to Wayland display".
        // A dedicated queue keeps this one-shot registry roundtrip off GDK's
        // queue; objects bound from the registry inherit the queue, so it is
        // kept alive for the panel's lifetime (torn down in the destructor).
        VbamSdlWlGlobals g;
        struct wl_event_queue* wlq = wl_display_create_queue(wayland_display);
        struct wl_registry* reg = wl_display_get_registry(wayland_display);
        wl_proxy_set_queue(reinterpret_cast<wl_proxy*>(reg), wlq);
        wl_registry_add_listener(reg, &kVbamSdlWlRegistryListener, &g);
        wl_display_roundtrip_queue(wayland_display, wlq);
        wl_registry_destroy(reg);
        sdl_wl_queue_ = wlq;
        if (g.comp && g.subcomp && gtk_surface) {
            struct wl_surface* child = wl_compositor_create_surface(g.comp);
            struct wl_subsurface* sub =
                wl_subcompositor_get_subsurface(g.subcomp, child, gtk_surface);
            // Desync so SDL's presents take effect without a parent commit; drop
            // the input region so GTK keeps handling pointer/keyboard.
            wl_subsurface_set_desync(sub);
            struct wl_region* empty = wl_compositor_create_region(g.comp);
            wl_surface_set_input_region(child, empty);
            wl_region_destroy(empty);
            // Position over the panel (toplevel-relative, like the Vulkan path).
            GtkWidget* toplevel = gtk_widget_get_toplevel(widget);
            int sx = 0, sy = 0;
            gtk_widget_translate_coordinates(widget, toplevel, 0, 0, &sx, &sy);
            wl_subsurface_set_position(sub, sx, sy);
            wl_surface_commit(child);

            sdl_wl_compositor_    = g.comp;
            sdl_wl_subcompositor_ = g.subcomp;
            sdl_wl_child_surface_ = child;
            sdl_wl_subsurface_    = sub;
            wayland_surface       = child;

            // A subsurface has no intrinsic size (unlike an X11 window), so tell
            // SDL how big to make its swapchain.
            int cw = 0, ch = 0;
            GetClientSize(&cw, &ch);
            if (cw < 1) cw = static_cast<int>(std::ceil(width * scale));
            if (ch < 1) ch = static_cast<int>(std::ceil(height * scale));
            SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, cw);
            SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, ch);
        } else {
            // Couldn't bind a (sub)compositor; fall back to GTK's surface (likely
            // still blank, but no worse than before).
            if (g.comp)    wl_compositor_destroy(g.comp);
            if (g.subcomp) wl_subcompositor_destroy(g.subcomp);
            wayland_surface = gtk_surface;
        }

        // Import GDK's wl_display so SDL shares our connection -- our subsurface
        // is a proxy on that display, and SDL must put it on a queue from the
        // same display or wl_proxy_set_queue aborts. This is the *global video*
        // import property ("SDL.video.wayland.wl_display"), read before video
        // init; the non-create SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER is only a
        // per-window getter, so SDL was opening its own connection instead.
        if (SDL_SetPointerProperty(SDL_GetGlobalProperties(), SDL_PROP_GLOBAL_VIDEO_WAYLAND_WL_DISPLAY_POINTER, wayland_display) == false) {
            wxLogError(_("Failed to set wayland display"));
            init_failed_ = true;
            return;
        }
    } else {
#endif
#endif
        xid = GDK_WINDOW_XID(gtk_widget_get_window(widget));
#ifndef NO_WAYLAND
#ifdef ENABLE_SDL3
    }
#endif

#ifdef ENABLE_SDL3
    if (GDK_IS_WAYLAND_WINDOW(gtk_widget_get_window(widget))) {
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "wayland");
    } else {
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "x11");
    }
#else
    SDL_SetHint(SDL_HINT_VIDEODRIVER, "x11");
#endif
#endif
#endif

    DrawingPanel::DrawingPanelInit();

#ifdef ENABLE_SDL3
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) == false) {
#else
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0) {
#endif
        wxLogError(_("Failed to initialize SDL video subsystem"));
        init_failed_ = true;
        return;
    }

#ifdef ENABLE_SDL3
    if (SDL_WasInit(SDL_INIT_VIDEO) == false) {
        if (SDL_Init(SDL_INIT_VIDEO) == false) {
#else
    if (SDL_WasInit(SDL_INIT_VIDEO) == 0) {
        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
#endif
            wxLogError(_("Failed to initialize SDL video"));
            init_failed_ = true;
            return;
        }
    }

#ifdef ENABLE_SDL3
#ifdef __WXGTK__
#ifndef NO_WAYLAND
    if (GDK_IS_WAYLAND_WINDOW(gtk_widget_get_window(widget))) {
        // Wrap our existing wl_surface. This is the *create* property
        // ("...create.wayland.wl_surface"); the non-create
        // SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER is only a getter, so passing it
        // here was ignored and SDL spawned its own toplevel window instead.
        if (SDL_SetPointerProperty(props, SDL_PROP_WINDOW_CREATE_WAYLAND_WL_SURFACE_POINTER, wayland_surface) == false) {
            wxLogError(_("Failed to set wayland surface"));
            init_failed_ = true;
            return;
        }
        // The surface we pass is a wl_subsurface we gave its role; tell SDL not
        // to attach its own XDG toplevel (which is what created the separate
        // window).
        SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_WAYLAND_SURFACE_ROLE_CUSTOM_BOOLEAN, true);

        // Create the wl_egl_window only for the GL-based SDL renderers
        // (opengl/opengles2), which need it to present into our custom surface.
        // Vulkan and software don't use EGL, and forcing an unused GL buffer
        // source onto our wl_surface is just cruft (and a second "role"-like
        // attachment on a surface that already drives a Vulkan swapchain). The
        // "default" backend picks opengl on Wayland, so it needs the EGL window
        // too -- except when HDR makes us force the vulkan backend instead.
        const bool wl_force_vulkan = (renderer_name == wxString("default")) &&
                                     hdr::HdrAvailable() && OPTION(kDispHDR);
        const bool wl_gl_renderer =
            renderer_name == wxString("opengl") ||
            renderer_name == wxString("opengles2") ||
            (renderer_name == wxString("default") && !wl_force_vulkan);
        if (wl_gl_renderer)
            SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_WAYLAND_CREATE_EGL_WINDOW_BOOLEAN, true);
    } else {
#endif
        if (SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_X11_WINDOW_NUMBER, xid) == false)
#elif defined(__WXMAC__)
        // sdl_cocoa_view_ (computed above): GL backends target this panel's own
        // frontmost view (an NSOpenGLView backing its own layer -- GameArea would
        // put GL under this view and show black); metal/gpu/software target the
        // shared GameArea view where SDL adds a CAMetalLayer subview.
        if (SDL_SetPointerProperty(props, SDL_PROP_WINDOW_CREATE_COCOA_VIEW_POINTER,
                sdl_cocoa_view_) == false)
#elif defined(__WXMSW__)
        if (SDL_SetPointerProperty(props, SDL_PROP_WINDOW_CREATE_WIN32_HWND_POINTER, GetHandle()) == false)
#else
        if (SDL_SetPointerProperty(props, "sdl2-compat.external_window", GetWindow()->GetHandle()) == false)
#endif
        {
            wxLogError(_("Failed to set parent window"));
            init_failed_ = true;
            return;
        }

#ifdef __WXGTK__
#ifndef NO_WAYLAND
    }
#endif
#endif

    if (SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_OPENGL_BOOLEAN, true) == false) {
        wxLogError(_("Failed to set OpenGL properties"));
    }

    // This is necessary for joysticks to work at all with SDL video.
    // We control this ourselves so it does not affect the GUI option.
#ifdef ENABLE_SDL3
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
#endif

#if defined(__WXMAC__)
    // SDL is about to make sdl_cocoa_view_ the window's contentView, detaching
    // the wx tree (status bar included). Capture the pre-takeover state so we can
    // undo it after SDL is set up (see the reattach at the end of init).
    VbamSdlCaptureViewState(sdl_cocoa_view_, &sdl_saved_window_,
                            &sdl_saved_content_view_, &sdl_saved_superview_);
#endif

    sdlwindow = SDL_CreateWindowWithProperties(props);
    SDL_DestroyProperties(props);

    if (sdlwindow == NULL) {
        wxLogError(_("Failed to create SDL window"));
        init_failed_ = true;
        return;
    }

#if defined(__WXMAC__)
    // The GL backends make our own view an NSOpenGLView (see the COCOA_VIEW pick
    // above). wx's MacDoRedraw erases a window's background through a wxWindowDC
    // unless its style is wxBG_STYLE_PAINT, and any wx DC drawing on an
    // NSOpenGLView triggers an endless repaint loop (EnsureIsValid ->
    // wxOSXLockFocus -> setNeedsDisplay:YES), pinning the main thread (spinning
    // cursor). GameArea sets this style on us, but only after construction --
    // i.e. after this init runs from our ctor -- so set it now, before SDL turns
    // the view into a GL surface. SDL clears the whole view itself, so suppressing
    // the wx erase loses nothing.
    GetWindow()->SetBackgroundStyle(wxBG_STYLE_PAINT);
#endif

    // The SDL_GPU-backed "gpu" renderer only acquires its swapchain for a window
    // SDL considers "shown". A window wrapping an already-visible host view
    // (Cocoa NSView / X11 / Win32 HWND) starts hidden in SDL's bookkeeping, so
    // the gpu renderer would present a black frame. Show it so the swapchain is
    // claimed. (Other backends present fine regardless; this is a no-op for an
    // already-mapped host surface.)
    if (renderer_name == wxString("gpu"))
        SDL_ShowWindow(sdlwindow);

    // When HDR is available and on, ask SDL for a linear scRGB output so it can
    // present HDR; we then upload a float (scRGB fp16) texture. Otherwise create
    // an ordinary SDR renderer.
    const bool sdl_want_hdr = hdr::HdrAvailable() && OPTION(kDispHDR);
    // On native Wayland, SDL's own HDR path does not work for our borrowed
    // child subsurface: SDL never sees the output's HDR headroom (it reports
    // SDR), so SDL_PROP_RENDERER_HDR_ENABLED stays false. Instead we drive HDR
    // through the compositor exactly like the OpenGL/Simple panels: present a
    // 10-bit PQ-encoded texture and tag the child surface BT.2020 PQ via
    // wp_color_manager_v1. So on Wayland the SDL HDR path wants a 10-bit
    // (2101010) surface, not SDL's scRGB-float one.
    const bool sdl_wl_hdr = sdl_want_hdr && IsWayland() && WaylandHdrPqSupported();
    // 10-bit "deep color" SDR (X11) wants a 2101010 texture; mutually exclusive
    // with HDR (which wants RGBA64_FLOAT). Decided here, before renderer
    // creation, because the high-bit-depth lanes need a particular SDL backend.
    const bool sdl_want_deep = !sdl_want_hdr && OPTION(kDispDeepColor) &&
                               hdr::DeepColor10Available() && panel_color_depth_ == 32;
    const bool sdl_default_backend = (OPTION(kSDLRenderer) == wxString("default"));

    // The SDL backend that can present our high-bit-depth surfaces. On macOS that
    // is "metal": it advertises RGBA64_FLOAT and drives EDR HDR (and is SDL's own
    // default). SDL's "vulkan" backend there is MoltenVK, which fails to even load
    // the Vulkan Portability library, so forcing it breaks HDR entirely.
    // Everywhere else "metal" does not exist and "vulkan" is the backend that
    // advertises both RGBA64_FLOAT (HDR) and 2101010 (deep color); SDL's usual
    // default pick ("opengl") exposes 8-bit formats only.
#if defined(__WXMAC__)
    const char* const kSdlHiDepthBackend = "metal";
#else
    const char* const kSdlHiDepthBackend = "vulkan";
#endif

    // When the user hasn't pinned a backend but we need a high-bit-depth surface,
    // ask for the high-bit-depth backend explicitly -- SDL's default pick would
    // otherwise be 8-bit-only and the panel would get bounced to another renderer.
    const char* sdl_hidepth_backend =
        (sdl_default_backend && (sdl_want_hdr || sdl_want_deep)) ? kSdlHiDepthBackend : nullptr;

    // Same problem for an *explicit* backend pick that isn't the high-bit-depth
    // one: "opengl"/"opengles2"/"software" expose 8-bit formats only and the "gpu"
    // backend can't create a suitable swapchain here, so such a pick would
    // silently drop to 8-bit and make GameArea abandon the SDL render method.
    // Substitute the high-bit-depth backend so SDL actually delivers 10-bit/HDR.
    // (Wayland HDR drives its own backend via sdl_wl_hdr and is handled
    // separately, so skip it there.)
    if (!IsWayland() && !sdl_default_backend && (sdl_want_hdr || sdl_want_deep) &&
        renderer_name != wxString(kSdlHiDepthBackend)) {
        renderer_name = wxString(kSdlHiDepthBackend);
    }

    if (sdl_want_hdr && !sdl_wl_hdr) {
        SDL_PropertiesID rprops = SDL_CreateProperties();
        SDL_SetPointerProperty(rprops, SDL_PROP_RENDERER_CREATE_WINDOW_POINTER, sdlwindow);
        if (!sdl_default_backend)
            SDL_SetStringProperty(rprops, SDL_PROP_RENDERER_CREATE_NAME_STRING,
                                  renderer_name.mb_str());
        else if (sdl_hidepth_backend)
            SDL_SetStringProperty(rprops, SDL_PROP_RENDERER_CREATE_NAME_STRING,
                                  sdl_hidepth_backend);
        SDL_SetNumberProperty(rprops, SDL_PROP_RENDERER_CREATE_OUTPUT_COLORSPACE_NUMBER,
                              SDL_COLORSPACE_SRGB_LINEAR);
        renderer = SDL_CreateRendererWithProperties(rprops);
        SDL_DestroyProperties(rprops);
    }
    // Wayland compositor-driven PQ HDR uses the 10-bit-capable backend (same as
    // deep color) with SDL's default (sRGB) output colorspace: we hand SDL
    // already-PQ-encoded 10-bit pixels and let the compositor interpret the
    // surface as BT.2020 PQ, so SDL must not colour-convert them.
    if (sdl_wl_hdr && renderer == NULL && !sdl_default_backend) {
        renderer = SDL_CreateRenderer(sdlwindow, renderer_name.mb_str());
    }

    // Deep color on the default backend: ask for the 10-bit-capable backend
    // before falling back to SDL's plain default.
    if (renderer == NULL && sdl_default_backend && sdl_hidepth_backend) {
        renderer = SDL_CreateRenderer(sdlwindow, sdl_hidepth_backend);
    }

    if (renderer == NULL && sdl_default_backend) {
        renderer = SDL_CreateRenderer(sdlwindow, NULL);
    } else if (renderer == NULL) {
        renderer = SDL_CreateRenderer(sdlwindow, renderer_name.mb_str());

        if (renderer == NULL) {
            wxLogError(_("Renderer creating failed, using default renderer"));
            wxLogDebug(_("SDL Error: %s"), SDL_GetError());

            renderer = SDL_CreateRenderer(sdlwindow, NULL);
        }
    }

    if (renderer == NULL) {
        wxLogError(_("Failed to create SDL renderer"));
        init_failed_ = true;
        return;
    }

    // Did we actually get an HDR-enabled renderer?
    if (sdl_want_hdr && !sdl_wl_hdr) {
        // scRGB HDR (macOS EDR, etc.): SDL drives HDR itself.
        SDL_PropertiesID rp = SDL_GetRendererProperties(renderer);
        sdl_hdr_ = SDL_GetBooleanProperty(rp, SDL_PROP_RENDERER_HDR_ENABLED_BOOLEAN, false);
        // SDL auto-multiplies this SDR white point into the color scale; the HDR
        // encoder cancels it (see HdrScRgbWhiteNits) so content lands at true nits.
        sdl_sdr_white_point_ =
            SDL_GetFloatProperty(rp, SDL_PROP_RENDERER_SDR_WHITE_POINT_FLOAT, 1.0f);
    } else if (sdl_wl_hdr) {
        // Wayland: drive HDR via the compositor. We need a 2101010 texture
        // whose memory order matches EncodePQ10's A2B10G10R10 output -- that is
        // an *BGR* 2101010 SDL format -- and the compositor must accept the
        // BT.2020 PQ tag on our child surface. Only then is HDR really live.
        //
        // Restrict this to the "vulkan" backend. SDL's "gpu" renderer advertises
        // the 2101010 format and accepts everything without error, but presents
        // a black frame into our custom-role subsurface (its swapchain/present
        // path doesn't drive a borrowed wl_surface the way the vulkan renderer
        // does). Other backends (opengl, software) don't offer a 2101010 format
        // at all. So vulkan is the only SDL backend that actually presents HDR
        // here; on anything else we leave sdl_hdr_ false and EvaluateHdrRenderer
        // falls back to the (working) native Vulkan renderer rather than showing
        // a black HDR screen. "gpu" stays fully usable for SDR.
        const bool vulkan_backend =
            wxString(SDL_GetRendererName(renderer)) == wxString("vulkan");
        sdl_deep_color_fmt_ = SdlPickDeepColorFormat(renderer);
        const bool bgr10 = (sdl_deep_color_fmt_ == SDL_PIXELFORMAT_ABGR2101010 ||
                            sdl_deep_color_fmt_ == SDL_PIXELFORMAT_XBGR2101010);
        if (vulkan_backend && bgr10 && sdl_wl_child_surface_ &&
            WaylandSetWlSurfaceHdrPq(
                static_cast<wl_surface*>(sdl_wl_child_surface_),
                static_cast<float>(OPTION(kDispHDRReferenceWhite)),
                static_cast<float>(OPTION(kDispHDRPeakBrightness)))) {
            sdl_wl_hdr_tagged_ = true;
            sdl_hdr_ = true;
        }
    }

    // 10-bit "deep color" SDR (X11): when the option is on, a 10-bit screen was
    // probed and the source is 32-bit, present into a 2101010 texture if the
    // chosen SDL backend offers one, so scaling/blending and the output surface
    // stay 10-bit (less banding). Still SDR -- no tone mapping. Mutually
    // exclusive with HDR; if the backend has no 10-bit format it stays off and
    // the panel falls back to the ordinary 8-bit path (the GameArea then records
    // the failure and tries another renderer).
    sdl_deep_color_ = false;
    if (!sdl_hdr_ && sdl_want_deep) {
        sdl_deep_color_fmt_ = SdlPickDeepColorFormat(renderer);
        sdl_deep_color_ = (sdl_deep_color_fmt_ != SDL_PIXELFORMAT_UNKNOWN);
    }

    // Set vsync
    if (SDL_SetRenderVSync(renderer, OPTION(kPrefVsync) ? 1 : 0) == false) {
        wxLogError(_("Failed to set vsync for SDL renderer"));
    }

    renderername = wxString(SDL_GetRendererName(renderer));
    wxLogDebug(_("SDL renderer: %s"), (const char*)renderername.mb_str());
#else
#ifdef __WXGTK__
    sdlwindow = SDL_CreateWindowFrom((void *)xid);
#elif defined(__WXMAC__)
    sdlwindow = SDL_CreateWindowFrom(wxGetApp().frame->GetPanel()->GetHandle());
#else
    sdlwindow = SDL_CreateWindowFrom(GetWindow()->GetHandle());
#endif

    if (sdlwindow == NULL) {
        wxLogError(_("Failed to create SDL window"));
        init_failed_ = true;
        return;
    }

    int vsync_flag = OPTION(kPrefVsync) ? SDL_RENDERER_PRESENTVSYNC : 0;

    if (OPTION(kSDLRenderer) == wxString("default")) {
        renderer = SDL_CreateRenderer(sdlwindow, -1, vsync_flag);
        wxLogDebug(_("SDL renderer: default"));
    } else {
        for (int i = 0; i < SDL_GetNumRenderDrivers(); i++) {
            wxString renderer_name = OPTION(kSDLRenderer);
            SDL_RendererInfo render_info;

            SDL_GetRenderDriverInfo(i, &render_info);

            if (!strcmp(renderer_name.mb_str(), render_info.name)) {
                if (!strcmp(render_info.name, "software")) {
                    renderer = SDL_CreateRenderer(sdlwindow, i, SDL_RENDERER_SOFTWARE | vsync_flag);
                } else {
                    renderer = SDL_CreateRenderer(sdlwindow, i, SDL_RENDERER_ACCELERATED | vsync_flag);
                }

                wxLogDebug(_("SDL renderer: %s"), render_info.name);
            }
        }

        if (renderer == NULL) {
            wxLogError(_("Renderer creating failed, using default renderer"));
            renderer = SDL_CreateRenderer(sdlwindow, -1, vsync_flag);
        }
    }

    if (renderer == NULL) {
        wxLogError(_("Failed to create SDL renderer"));
        init_failed_ = true;
        return;
    }

    renderername = OPTION(kSDLRenderer);
#endif

    // Decide the active HDR encoding now that the renderer exists.
    UpdateHdrState();

#ifdef ENABLE_SDL3
    if (sdl_hdr_ && !sdl_wl_hdr_tagged_) {
        // HDR: a linear scRGB float texture; we upload scRGB fp16 each frame.
        SDL_PropertiesID tp = SDL_CreateProperties();
        SDL_SetNumberProperty(tp, SDL_PROP_TEXTURE_CREATE_COLORSPACE_NUMBER,
                              SDL_COLORSPACE_SRGB_LINEAR);
        SDL_SetNumberProperty(tp, SDL_PROP_TEXTURE_CREATE_FORMAT_NUMBER,
                              SDL_PIXELFORMAT_RGBA64_FLOAT);
        SDL_SetNumberProperty(tp, SDL_PROP_TEXTURE_CREATE_ACCESS_NUMBER,
                              SDL_TEXTUREACCESS_STREAMING);
        SDL_SetNumberProperty(tp, SDL_PROP_TEXTURE_CREATE_WIDTH_NUMBER,
                              (int)std::ceil(width * scale));
        SDL_SetNumberProperty(tp, SDL_PROP_TEXTURE_CREATE_HEIGHT_NUMBER,
                              (int)std::ceil(height * scale));
        texture = SDL_CreateTextureWithProperties(renderer, tp);
        SDL_DestroyProperties(tp);
    } else if (sdl_deep_color_ || sdl_wl_hdr_tagged_) {
        // 10-bit 2101010 texture, used two ways:
        //  * deep color (X11 SDR): pack the corrected RGBA8 frame, 10-bit output.
        //  * Wayland HDR: upload PQ10-encoded pixels; the child surface is tagged
        //    BT.2020 PQ so the compositor presents them as HDR.
        // Either way SDL must NOT transfer/gamut-convert: a 2101010 texture
        // otherwise defaults to HDR10 (BT.2020 PQ) and SDL would convert our
        // already-final data to the sRGB output (over-saturated, red cast). Tag
        // it sRGB so the bits are presented as-is.
        SDL_PropertiesID tp = SDL_CreateProperties();
        SDL_SetNumberProperty(tp, SDL_PROP_TEXTURE_CREATE_COLORSPACE_NUMBER,
                              SDL_COLORSPACE_SRGB);
        SDL_SetNumberProperty(tp, SDL_PROP_TEXTURE_CREATE_FORMAT_NUMBER,
                              sdl_deep_color_fmt_);
        SDL_SetNumberProperty(tp, SDL_PROP_TEXTURE_CREATE_ACCESS_NUMBER,
                              SDL_TEXTUREACCESS_STREAMING);
        SDL_SetNumberProperty(tp, SDL_PROP_TEXTURE_CREATE_WIDTH_NUMBER,
                              (int)std::ceil(width * scale));
        SDL_SetNumberProperty(tp, SDL_PROP_TEXTURE_CREATE_HEIGHT_NUMBER,
                              (int)std::ceil(height * scale));
        texture = SDL_CreateTextureWithProperties(renderer, tp);
        SDL_DestroyProperties(tp);
    } else
#endif
    if (out_8) {
#ifdef ENABLE_SDL3
        if (renderername == wxString("direct3d")) {
            texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB332, SDL_TEXTUREACCESS_STREAMING, (width * scale), (height * scale));
        } else {
            texture = SDL_CreateTexture(renderer, SDL_GetPixelFormatForMasks(8, 0xE0, 0x1C, 0x03, 0x00),
SDL_TEXTUREACCESS_STREAMING, (width * scale), (height * scale));
        }
#else
        if (renderername == wxString("direct3d")) {
            texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB332, SDL_TEXTUREACCESS_STREAMING, (width * scale), (height * scale));
        } else {
            texture = SDL_CreateTexture(renderer, SDL_MasksToPixelFormatEnum(8, 0xE0, 0x1C, 0x03, 0x00), SDL_TEXTUREACCESS_STREAMING, (width * scale), (height * scale));
        }
#endif
    } else if (out_16) {
#ifdef ENABLE_SDL3
        if (renderername == wxString("direct3d")) {
            texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, (width * scale), (height * scale));
        } else {
            texture = SDL_CreateTexture(renderer, SDL_GetPixelFormatForMasks(16, 0x7C00, 0x03E0, 0x001F, 0x0000), SDL_TEXTUREACCESS_STREAMING, (width * scale), (height * scale));
        }
#else
        if (renderername == wxString("direct3d")) {
            texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, (width * scale), (height * scale));
        } else {
            texture = SDL_CreateTexture(renderer, SDL_MasksToPixelFormatEnum(16, 0x7C00, 0x03E0, 0x001F, 0x0000), SDL_TEXTUREACCESS_STREAMING, (width * scale), (height * scale));
        }
#endif
    } else if (out_24) {
#ifdef ENABLE_SDL3
        if (renderername == wxString("direct3d")) {
            texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, (width * scale), (height * scale));
        } else {
            texture = SDL_CreateTexture(renderer, SDL_GetPixelFormatForMasks(24, 0x000000FF, 0x0000FF00, 0x00FF0000, 0x00000000), SDL_TEXTUREACCESS_STREAMING, (width * scale), (height * scale));
        }
#else
        if (renderername == wxString("direct3d")) {
            texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, (width * scale), (height * scale));
        } else {
            texture = SDL_CreateTexture(renderer, SDL_MasksToPixelFormatEnum(24, 0x000000FF, 0x0000FF00, 0x00FF0000, 0x00000000), SDL_TEXTUREACCESS_STREAMING, (width * scale), (height * scale));
        }
#endif
    } else {
#ifdef ENABLE_SDL3
        if (renderername == wxString("direct3d")) {
            texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, (width * scale), (height * scale));
        } else {
            texture = SDL_CreateTexture(renderer, SDL_GetPixelFormatForMasks(32, 0x000000FF, 0x0000FF00, 0x00FF0000, 0x00000000), SDL_TEXTUREACCESS_STREAMING, (width * scale), (height * scale));
        }
#else
        if (renderername == wxString("direct3d")) {
            texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, (width * scale), (height * scale));
        } else {
            texture = SDL_CreateTexture(renderer, SDL_MasksToPixelFormatEnum(32, 0x000000FF, 0x0000FF00, 0x00FF0000, 0x00000000), SDL_TEXTUREACCESS_STREAMING, (width * scale), (height * scale));
        }
#endif
    }

// Set bilinear or nearest on the texture.
#ifdef ENABLE_SDL3
#ifdef HAVE_SDL3_PIXELART
    SDL_SetTextureScaleMode(texture, OPTION(kDispSDLPixelArt) ? SDL_SCALEMODE_PIXELART : OPTION(kDispBilinear) ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST);
#else
    SDL_SetTextureScaleMode(texture, OPTION(kDispBilinear) ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST);
#endif
#else
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, OPTION(kDispBilinear) ? "1" : "0");
#endif

#if defined(__WXMAC__)
    // SDL has finished taking over the window's contentView; undo it now that its
    // renderer is up. Re-nest SDL's view under its original superview and put wx's
    // contentView back, so the status bar (and the rest of the wx frame) stays
    // visible and laid out by wx while SDL keeps rendering into its nested view.
    VbamSdlReattachViewState(sdl_cocoa_view_, sdl_saved_window_,
                             sdl_saved_content_view_, sdl_saved_superview_);
    sdl_saved_content_view_ = nullptr;  // released by ReattachViewState
    sdl_saved_window_ = nullptr;
    sdl_saved_superview_ = nullptr;

    // SDL's takeover left our view (GameArea) filling the whole window, covering
    // the status bar. wx sizes the client area and reserves the status-bar strip
    // in the frame's layout, which the takeover bypassed -- re-run it so GameArea
    // shrinks to leave the bar visible.
    if (wxGetApp().frame)
        wxGetApp().frame->SendSizeEvent();
#endif

    did_init = true;
}
        
void SDLDrawingPanel::OnSize(wxSizeEvent& ev)
{
#if defined(HAVE_WAYLAND_SUPPORT)
    // Keep the SDL subsurface aligned with and sized to the panel.
    if (sdl_wl_subsurface_) {
        GtkWidget* widget   = wxGetApp().frame->GetPanel()->GetHandle();
        GtkWidget* toplevel = gtk_widget_get_toplevel(widget);
        int sx = 0, sy = 0;
        gtk_widget_translate_coordinates(widget, toplevel, 0, 0, &sx, &sy);
        wl_subsurface_set_position(static_cast<wl_subsurface*>(sdl_wl_subsurface_), sx, sy);
        if (sdlwindow) {
            int cw = 0, ch = 0;
            GetClientSize(&cw, &ch);
            if (cw > 0 && ch > 0)
                SDL_SetWindowSize(sdlwindow, cw, ch);
        }
    }
#endif
    if (todraw) {
        DrawArea();
    }

    ev.Skip();
}
        
void SDLDrawingPanel::DrawArea(wxWindowDC& dc)
{
    (void)dc;
    DrawArea();
}

void SDLDrawingPanel::DrawArea()
{
    uint32_t srcPitch = 0;
    uint32_t *todraw_argb = NULL;
    uint16_t *todraw_rgb565 = NULL;

    if (!did_init)
        DrawingPanelInit();

    int inrb = out_8 ? 4 : out_16 ? 2 : out_24 ? 0 : 1;
    if (out_8) {
        srcPitch = std::ceil((width + inrb) * scale * 1);
    } else if (out_16) {
        srcPitch = std::ceil((width + inrb) * scale * 2);
    } else if (out_24) {
        srcPitch = std::ceil(width * scale * 3);
    } else {
        srcPitch = std::ceil((width + inrb) * scale * 4);
    }

    SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0x00);
    SDL_RenderClear(renderer);

    if (HdrActive() && sdl_wl_hdr_tagged_ && panel_color_depth_ == 32 && texture != NULL) {
        // Wayland HDR: encode the corrected RGBA8 image to BT.2100 PQ packed as
        // A2B10G10R10 and upload it to the 2101010 texture. The child surface is
        // tagged BT.2020 PQ, so the compositor presents these bits as HDR.
        int tex_w = (int)std::ceil(width * scale);
        int tex_h = (int)std::ceil(height * scale);
        sdl_deep_color_buf_.resize((size_t)tex_w * tex_h * 4);
        hdr::EncodePQ10(todraw + srcPitch, srcPitch, tex_w, tex_h,
                        reinterpret_cast<uint32_t*>(sdl_deep_color_buf_.data()), tex_w * 4);
        SDL_UpdateTexture(texture, NULL, sdl_deep_color_buf_.data(), tex_w * 4);
    }
    else if (HdrActive() && sdl_hdr_ && panel_color_depth_ == 32 && texture != NULL) {
        // HDR: encode the corrected RGBA8 image to linear scRGB fp16 (8 bytes
        // per pixel) and upload it to the float texture.
        int tex_w = (int)std::ceil(width * scale);
        int tex_h = (int)std::ceil(height * scale);
        sdl_hdr_buf_.resize((size_t)tex_w * tex_h * 8);
        hdr::EncodeScRGBFp16(todraw + srcPitch, srcPitch, tex_w, tex_h,
                             reinterpret_cast<uint16_t*>(sdl_hdr_buf_.data()), tex_w * 8);
        SDL_UpdateTexture(texture, NULL, sdl_hdr_buf_.data(), tex_w * 8);
    }
#ifdef ENABLE_SDL3
    else if (sdl_deep_color_ && panel_color_depth_ == 32 && texture != NULL) {
        // 10-bit SDR: pack the corrected RGBA8 image into the 2101010 texture.
        int tex_w = (int)std::ceil(width * scale);
        int tex_h = (int)std::ceil(height * scale);
        sdl_deep_color_buf_.resize((size_t)tex_w * tex_h * 4);
        SdlPackDeepColor(todraw + srcPitch, srcPitch, tex_w, tex_h,
                         sdl_deep_color_fmt_,
                         reinterpret_cast<uint32_t*>(sdl_deep_color_buf_.data()),
                         tex_w);
        SDL_UpdateTexture(texture, NULL, sdl_deep_color_buf_.data(), tex_w * 4);
    }
#endif
    else if ((renderername == wxString("direct3d")) && (panel_color_depth_ == 32)) {
        todraw_argb = (uint32_t *)(todraw + srcPitch);

        for (int i = 0; i < (height * scale); i++) {
            for (int j = 0; j < (width * scale); j++) {
                todraw_argb[j + (i * (srcPitch / 4))] = 0xFF000000 | ((todraw_argb[j + (i * (srcPitch / 4))] & 0xFF) << 16) | (todraw_argb[j + (i * (srcPitch / 4))] & 0xFF00) | ((todraw_argb[j + (i * (srcPitch / 4))] & 0xFF0000) >> 16);
            }
        }

        if (texture != NULL)
            SDL_UpdateTexture(texture, NULL, todraw_argb, srcPitch);
    } else if ((renderername == wxString("direct3d")) && (panel_color_depth_ == 16)) {
        todraw_rgb565 = (uint16_t *)(todraw + srcPitch);

        for (int i = 0; i < (height * scale); i++) {
            for (int j = 0; j < (width * scale); j++) {
                todraw_rgb565[j + (i * (srcPitch / 2))] = ((todraw_rgb565[j + (i * (srcPitch / 2))] & 0x7FE0) << 1) | (todraw_rgb565[j + (i * (srcPitch / 2))] & 0x1F);
            }
        }

        if (texture != NULL)
            SDL_UpdateTexture(texture, NULL, todraw_rgb565, srcPitch);
    } else {
        if (texture != NULL)
            SDL_UpdateTexture(texture, NULL, todraw + srcPitch, srcPitch);
    }

    if (texture != NULL)
#ifdef ENABLE_SDL3
        SDL_RenderTexture(renderer, texture, NULL, NULL);
#else
        SDL_RenderCopy(renderer, texture, NULL, NULL);
#endif
    
    SDL_RenderPresent(renderer);
}
        
void SDLDrawingPanel::DrawArea(uint8_t** data)
{
    // double-buffer buffer:
    //   if filtering, this is filter output, retained for redraws
    //   if not filtering, we still retain current image for redraws
    int outbpp = panel_color_depth_ >> 3;
    int inrb = (panel_color_depth_ == 8) ? 4 : (panel_color_depth_ == 16) ? 2 : (panel_color_depth_ == 24) ? 0 : 1;
    int outstride = std::ceil((width + inrb) * outbpp * scale);

    // Use multiple threads for filter processing, but force single-threaded
    // for plugin filters. RPI plugins expect the full frame.
    const int max_threads = (OPTION(kDispFilter) == config::Filter::kPlugin)
        ? 1 : GetMaxFilterThreads();

    // Compute stride for InterframeManager initialization
    int instride = (width + inrb) * (panel_color_depth_ >> 3);

    // Only allocate pixbuf2 when filters are used
    if (OPTION(kDispFilter) != config::Filter::kNone ||
        OPTION(kDispIFB) != config::Interframe::kNone) {
        if (!pixbuf2) {
            int allocstride = outstride, alloch = height;

            // gb may write borders, so allocate enough for them
            if (width == GameArea::GBWidth && height == GameArea::GBHeight) {
                allocstride = std::ceil((GameArea::SGBWidth + inrb) * outbpp * scale);
                alloch = GameArea::SGBHeight;
            }

            pixbuf2 = (uint8_t*)calloc(allocstride, std::ceil((alloch + 2) * scale));
        }
        todraw = pixbuf2;
    } else {
        // No filter: use g_pix directly, don't allocate or swap buffers
        todraw = *data;
    }

    // First, apply filters, if applicable, in parallel, if enabled
    // FIXME: && (gopts.ifb != FF_MOTION_BLUR || !renderer_can_motion_blur)
    if (OPTION(kDispFilter) != config::Filter::kNone ||
        OPTION(kDispIFB) != config::Interframe::kNone) {
        if (nthreads != max_threads) {
            if (nthreads) {
                if (nthreads > 1)
                    for (int i = 0; i < nthreads; i++) {
                        threads[i].lock_.Lock();
                        threads[i].src_ = NULL;
                        threads[i].sig_.Signal();
                        threads[i].lock_.Unlock();
                        threads[i].Wait();
                    }

                delete[] threads;
            }

            nthreads = max_threads;
            threads = new FilterThread[nthreads];

            // Initialize InterframeManager for per-region IFB processing (only if IFB enabled)
            if (OPTION(kDispIFB) != config::Interframe::kNone) {
                InterframeManager::Instance().Init(width, height, instride, nthreads);
            }

            // first time around, no threading in order to avoid
            // static initializer conflicts
            threads[0].threadno_ = 0;
            threads[0].nthreads_ = 1;
            threads[0].width_ = width;
            threads[0].height_ = height;
            threads[0].scale_ = scale;
            threads[0].src_ = *data;
            threads[0].dst_ = todraw;
            threads[0].delta_ = delta;
            threads[0].rpi_ = rpi_;
            threads[0].phase_ = FilterThread::Phase::Filter;
            threads[0].rpi_bpp_ = rpi_bpp_;
            threads[0].rpi_using_rgb565_ = rpi_using_rgb565_;
            threads[0].panel_color_depth_ = panel_color_depth_;
#ifdef VBAM_RPI_PROXY_SUPPORT
            threads[0].rpi_proxy_client_ = rpi_proxy_client_;
#endif
            threads[0].Entry();
            // IFB is handled inside Entry() for single-threaded mode

            // go ahead and start the threads up, though
            if (nthreads > 1) {
                for (int i = 0; i < nthreads; i++) {
                    threads[i].threadno_ = i;
                    threads[i].nthreads_ = nthreads;
                    threads[i].width_ = width;
                    threads[i].height_ = height;
                    threads[i].scale_ = scale;
                    threads[i].dst_ = todraw;
                    threads[i].delta_ = delta;
                    threads[i].rpi_ = rpi_;
                    threads[i].rpi_bpp_ = rpi_bpp_;
                    threads[i].rpi_using_rgb565_ = rpi_using_rgb565_;
                    threads[i].panel_color_depth_ = panel_color_depth_;
#ifdef VBAM_RPI_PROXY_SUPPORT
                    threads[i].rpi_proxy_client_ = rpi_proxy_client_;
#endif
                    threads[i].done_ = &filt_done;
                    threads[i].phase_ = FilterThread::Phase::Filter;
                    threads[i].ready_ = &filt_ready;
                    threads[i].Create();
                    threads[i].Run();
                }
                // Wait for all threads to signal they're ready before returning
                for (int i = 0; i < nthreads; i++) {
                    filt_ready.Wait();
                }
            }
        } else if (nthreads == 1) {
            threads[0].threadno_ = 0;
            threads[0].nthreads_ = 1;
            threads[0].width_ = width;
            threads[0].height_ = height;
            threads[0].scale_ = scale;
            threads[0].src_ = *data;
            threads[0].dst_ = todraw;
            threads[0].delta_ = delta;
            threads[0].rpi_ = rpi_;
            threads[0].phase_ = FilterThread::Phase::Filter;
            threads[0].rpi_bpp_ = rpi_bpp_;
            threads[0].rpi_using_rgb565_ = rpi_using_rgb565_;
            threads[0].panel_color_depth_ = panel_color_depth_;
#ifdef VBAM_RPI_PROXY_SUPPORT
            threads[0].rpi_proxy_client_ = rpi_proxy_client_;
#endif
            threads[0].Entry();
            // IFB is handled inside Entry() for single-threaded mode
        } else {
            // Phase 1: Run IFB threads first (before filters)
            if (OPTION(kDispIFB) != config::Interframe::kNone) {
                for (int i = 0; i < nthreads; i++) {
                    threads[i].phase_ = FilterThread::Phase::IFB;
                    threads[i].lock_.Lock();
                    threads[i].src_ = *data;
                    threads[i].sig_.Signal();
                    threads[i].lock_.Unlock();
                }

                for (int i = 0; i < nthreads; i++)
                    filt_done.Wait();
            }

            // Phase 2: Run filter threads (on IFB-processed source)
            for (int i = 0; i < nthreads; i++) {
                threads[i].phase_ = FilterThread::Phase::Filter;
                threads[i].lock_.Lock();
                threads[i].src_ = *data;
                threads[i].sig_.Signal();
                threads[i].lock_.Unlock();
            }

            for (int i = 0; i < nthreads; i++)
                filt_done.Wait();

            // Fix seams between thread regions by re-processing bands around boundaries
            // This runs in parallel using the existing filter threads
            // Only runs here (not on first frame when threads are just starting)
            if (nthreads > 1 && OPTION(kDispFilter) != config::Filter::kNone) {
                // Collect seam positions and merge overlapping bands
                std::vector<std::pair<int, int>> seamBands;  // (startY, endY) in source coords

                for (int i = 1; i < nthreads; i++) {
                    int seamY = height * i / nthreads;
                    int bandStart = std::max(0, seamY - kFilterContextRadius - kSeamMarginRows);
                    int bandEnd = std::min(height, seamY + kFilterContextRadius + kSeamMarginRows);

                    // Merge with previous band if overlapping
                    if (!seamBands.empty() && bandStart <= seamBands.back().second) {
                        seamBands.back().second = std::max(seamBands.back().second, bandEnd);
                    } else {
                        seamBands.push_back({bandStart, bandEnd});
                    }
                }

                // Assign seam bands to threads and run in parallel
                const int scale_int = static_cast<int>(scale);
                // Source base: skip 1 border row
                uint8_t* src_base = *data + instride;
                // Dest base: skip 1*scale border rows
                uint8_t* dst_base = todraw + outstride * scale_int;

                const int numBands = static_cast<int>(seamBands.size());
                for (int i = 0; i < nthreads; i++) {
                    threads[i].phase_ = FilterThread::Phase::SeamFix;
                    // Assign band to thread (some threads may have no work)
                    if (i < numBands) {
                        threads[i].seamBandStart_ = seamBands[i].first;
                        threads[i].seamBandEnd_ = seamBands[i].second;
                    } else {
                        threads[i].seamBandStart_ = -1;  // No work for this thread
                        threads[i].seamBandEnd_ = -1;
                    }
                    threads[i].seamSrc_ = src_base;
                    threads[i].seamDst_ = dst_base;
                    threads[i].lock_.Lock();
                    threads[i].src_ = src_base;  // Keep src_ non-null to avoid thread exit
                    threads[i].sig_.Signal();
                    threads[i].lock_.Unlock();
                }

                for (int i = 0; i < nthreads; i++)
                    filt_done.Wait();
            }
        }
    }

    // Note: We do NOT modify *data (which is g_pix) here.
    // g_pix must always point to the emulator's allocated buffer.
    // Modifying it would cause the panel destructor to free the wrong buffer,
    // leaving g_pix as a dangling pointer after panel reset.

    // draw OSD text old-style (directly into output buffer), if needed
    // new style flickers too much, so we'll stick to this for now
    if (wxGetApp().frame->IsFullScreen() || !OPTION(kPrefDisableStatus)) {
        GameArea* panel = wxGetApp().frame->GetPanel();

        int scaled_width = (int)std::ceil(width * scale);
        int scaled_height = (int)std::ceil(height * scale);

        if (panel->osdstat.size()) {
            wxString statText = wxString::FromUTF8(panel->osdstat.c_str());
            // Position closer to top-left for better visibility at native resolution
            // At scale=1.0 (native GBA/GB res): (4, 4) instead of (10, 20)
            int x = (int)std::ceil(4 * scale);
            int y = (int)std::ceil(4 * scale);
            drawTextWx(todraw + outstride * (panel_color_depth_ != 24), outstride,
                x, y, statText, scaled_width, scaled_height, scale, panel_color_depth_);
        }

        if (!panel->osdtext.empty()) {
            if (systemGetClock() - panel->osdtime < OSD_TIME) {
                // Position near bottom of screen, scaled with filter
                // At scale=1.0 (native res): positioned to allow 3 lines of text
                int x = (int)std::ceil(4 * scale);
                int osd_offset = (panel->game_type() == IMAGE_GB) ? 106 : 64;
                int y = scaled_height - (int)std::ceil(osd_offset * scale);
                drawTextWx(todraw + outstride * (systemColorDepth != 24), outstride,
                    x, y, panel->osdtext, scaled_width, scaled_height, scale, panel_color_depth_);
            } else
                panel->osdtext.clear();
        }
    }

    // Draw the current frame
    DrawArea();
}

BasicDrawingPanel::BasicDrawingPanel(wxWindow* parent, int _width, int _height)
        : DrawingPanel(parent, _width, _height)
{
    // wxImage is 24-bit RGB, so 24-bit is preferred.  Filters require
    // 16 or 32, though
    if (OPTION(kDispFilter) == config::Filter::kNone &&
        OPTION(kDispIFB) == config::Interframe::kNone) {
        // changing from 32 to 24 does not require regenerating color tables
        systemColorDepth = (OPTION(kBitDepth) + 1) << 3;
    }
            
    DrawingPanelInit();
}
        
void BasicDrawingPanel::DrawArea(wxWindowDC& dc)
{
    wxImage* im;

    if (out_24 && OPTION(kDispFilter) == config::Filter::kNone) {
        // never scaled, no borders, no transformations needed
        im = new wxImage(width, height, todraw, true);
    } else if (out_24) {
        // 24-bit with filter: scaled by filters, no color transform needed
        int scaled_width = (int)std::ceil(width * scale);
        int scaled_height = (int)std::ceil(height * scale);
        im = new wxImage(scaled_width, scaled_height, false);
        // Skip 1 row of top border
        uint8_t* src = todraw + scaled_width * 3;
        uint8_t* dst = im->GetData();

        for (int y = 0; y < scaled_height; y++) {
            std::memcpy(dst, src, scaled_width * 3);
            dst += scaled_width * 3;
            src += scaled_width * 3;
        }
    } else if (out_8) {
        // scaled by filters, top/right borders, transform to 24-bit
        im = new wxImage(std::ceil(width * scale), std::ceil(height * scale), false);
        int inrb = 4; // 8-bit has 4-pixel filter border
        int scaled_stride = (int)std::ceil((width + inrb) * scale);
        // Skip 1 row of top border
        uint8_t* src = (uint8_t*)todraw + scaled_stride;
        uint8_t* dst = im->GetData();

        for (int y = 0; y < std::ceil(height * scale); y++) {
            for (int x = 0; x < std::ceil(width * scale); x++, src++) {
                // White color fix
                if (*src == 0xff) {
                    *dst++ = 0xff;
                    *dst++ = 0xff;
                    *dst++ = 0xff;
                } else {
                    *dst++ = (((*src >> 5) & 0x7) << 5);
                    *dst++ = (((*src >> 2) & 0x7) << 5);
                    *dst++ = ((*src & 0x3) << 6);
                }
            }

            src += (int)std::ceil(inrb * scale); // skip rhs border (scaled)
        }
    } else if (out_16) {
        // scaled by filters, top/right borders, transform to 24-bit
        im = new wxImage(std::ceil(width * scale), std::ceil(height * scale), false);
        int inrb = 2; // 16-bit has 2-pixel filter border
        int scaled_stride = (int)std::ceil((width + inrb) * scale);
        // Skip 1 row of top border
        uint16_t* src = (uint16_t*)todraw + scaled_stride;
        uint8_t* dst = im->GetData();

        for (int y = 0; y < std::ceil(height * scale); y++) {
            for (int x = 0; x < std::ceil(width * scale); x++, src++) {
                *dst++ = ((*src >> 10) & 0x1f) << 3;
                *dst++ = ((*src >> 5) & 0x1f) << 3;
                *dst++ = (*src & 0x1f) << 3;
            }

            src += (int)std::ceil(inrb * scale); // skip rhs border (scaled)
        }
    } else if (OPTION(kDispFilter) != config::Filter::kNone) {
        // scaled by filters, top/right borders, transform to 24-bit
        im = new wxImage(std::ceil(width * scale), std::ceil(height * scale), false);
        int inrb = 1; // 32-bit has 1-pixel filter border
        int scaled_stride = (int)std::ceil((width + inrb) * scale);
        // Skip 1 row of top border
        uint32_t* src = (uint32_t*)todraw + scaled_stride;
        uint8_t* dst = im->GetData();

        for (int y = 0; y < std::ceil(height * scale); y++) {
            for (int x = 0; x < std::ceil(width * scale); x++, src++) {
                *dst++ = *src >> (systemRedShift - 3);
                *dst++ = *src >> (systemGreenShift - 3);
                *dst++ = *src >> (systemBlueShift - 3);
            }

            src += (int)std::ceil(inrb * scale); // skip rhs border (scaled)
        }
    } else {  // 32 bit
        // not scaled by filters, top/right borders, transform to 24-bit
        im = new wxImage(std::ceil(width * scale), std::ceil(height * scale), false);
        uint32_t* src = (uint32_t*)todraw + (int)std::ceil((width + 1) * scale); // skip top border
        uint8_t* dst = im->GetData();

        for (int y = 0; y < std::ceil(height * scale); y++) {
            for (int x = 0; x < std::ceil(width * scale); x++, src++) {
                *dst++ = *src >> (systemRedShift - 3);
                *dst++ = *src >> (systemGreenShift - 3);
                *dst++ = *src >> (systemBlueShift - 3);
            }
                    
            ++src; // skip rhs border
        }
    }
            
    DrawImage(dc, im);
            
    delete im;
}
        
void BasicDrawingPanel::DrawImage(wxWindowDC& dc, wxImage* im)
{
    double sx, sy;
    int w, h;
    GetClientSize(&w, &h);
    sx = w / (width * scale);
    sy = h / (height * scale);
    dc.SetUserScale(sx, sy);
    wxBitmap bm(*im);
    dc.DrawBitmap(bm, 0, 0);
}

#if defined(__WXGTK__)
// ─── XOrg native software driver (backs Simple on X11) ──────────────────────

XOrgDrawingPanel::XOrgDrawingPanel(wxWindow* parent, int _width, int _height)
    : BasicDrawingPanel(parent, _width, _height)
{
    // Set up the 10-bit child window now so DeepColorActive() is valid before
    // the first paint. BasicDrawingPanel never runs DrawingPanelInit().
    EnsureSetup();
    // Mark initialized so GameArea runs the 10-bit "working" check
    // (EvaluateDeepColorRenderer): if we couldn't get a 10-bit visual despite
    // deep color being available, it records the failure and falls back to
    // another capable renderer.
    did_init = true;
}

XOrgDrawingPanel::~XOrgDrawingPanel()
{
    Teardown();
}

void XOrgDrawingPanel::EnsureSetup()
{
    if (xorg_setup_done_)
        return;
    xorg_setup_done_ = true;

    // Only stand up the 10-bit child window when deep color is actually wanted
    // and the display supports it; otherwise behave exactly like the plain wx
    // software panel. The panel is recreated when kDispDeepColor changes
    // (render_observer_ -> ResetPanel), so this is re-evaluated on toggle.
    if (!OPTION(kDispDeepColor) || !hdr::DeepColor10Available())
        return;

    GtkWidget* widget = GetWindow()->GetHandle();
    if (!widget)
        return;
    gtk_widget_realize(widget);
    GdkWindow* gwin = gtk_widget_get_window(widget);
    // X11 only: on Wayland (or any non-X11 GdkWindow) stay on the 8-bit wx path.
    // GDK_IS_X11_WINDOW comes from gdkx.h and needs no Wayland headers.
    if (!gwin || !GDK_IS_X11_WINDOW(gwin))
        return;

    Display* dpy = GDK_WINDOW_XDISPLAY(gwin);
    Window parent_win = GDK_WINDOW_XID(gwin);
    if (!dpy || !parent_win)
        return;

    // Find a 30-bit TrueColor visual (10 bits per channel).
    XVisualInfo tmpl{};
    tmpl.screen  = DefaultScreen(dpy);
    tmpl.depth   = 30;
    tmpl.c_class = TrueColor;
    int nvis = 0;
    XVisualInfo* vis = XGetVisualInfo(
        dpy, VisualScreenMask | VisualDepthMask | VisualClassMask, &tmpl, &nvis);
    if (!vis || nvis == 0) {
        if (vis) XFree(vis);
        return;  // no 10-bit visual -> stay on the 8-bit wx path
    }

    Visual* visual = vis[0].visual;
    // Derive channel shifts from the visual's masks (normally X2R10G10B10).
    auto mask_shift = [](unsigned long mask) {
        int s = 0;
        if (!mask) return 0;
        while (!(mask & 1)) { mask >>= 1; ++s; }
        return s;
    };
    xorg_r_shift_ = mask_shift(vis[0].red_mask);
    xorg_g_shift_ = mask_shift(vis[0].green_mask);
    xorg_b_shift_ = mask_shift(vis[0].blue_mask);

    Colormap cmap = XCreateColormap(dpy, parent_win, visual, AllocNone);

    int pw = 1, ph = 1;
    GetClientSize(&pw, &ph);
    if (pw < 1) pw = 1;
    if (ph < 1) ph = 1;

    XSetWindowAttributes swa{};
    swa.colormap         = cmap;
    swa.background_pixel  = 0;
    swa.border_pixel      = 0;
    swa.event_mask        = ExposureMask;  // pointer/key events propagate to parent
    Window child = XCreateWindow(
        dpy, parent_win, 0, 0, pw, ph, 0, 30, InputOutput, visual,
        CWColormap | CWBackPixel | CWBorderPixel | CWEventMask, &swa);
    XFree(vis);
    if (!child) {
        XFreeColormap(dpy, cmap);
        return;
    }
    // Mapped lazily in DrawImage so it never obscures the wx software path when
    // deep color is on but inactive (e.g. a 16-bit source).

    xorg_display_    = dpy;
    xorg_window_     = child;
    xorg_visual_     = visual;
    xorg_gc_         = XCreateGC(dpy, child, 0, nullptr);
    xorg_deep_color_ = true;
}

void XOrgDrawingPanel::Teardown()
{
    if (!xorg_display_)
        return;
    Display* dpy = static_cast<Display*>(xorg_display_);
    if (xorg_image_) {
        XImage* img = static_cast<XImage*>(xorg_image_);
        img->data = nullptr;  // we own the buffer (xorg_buf_); don't let X free it
        XDestroyImage(img);
        xorg_image_ = nullptr;
    }
    if (xorg_gc_) { XFreeGC(dpy, static_cast<GC>(xorg_gc_)); xorg_gc_ = nullptr; }
    if (xorg_window_) { XDestroyWindow(dpy, xorg_window_); xorg_window_ = 0; }
    xorg_display_    = nullptr;
    xorg_deep_color_ = false;
}

void XOrgDrawingPanel::DrawImage(wxWindowDC& dc, wxImage* im)
{
    const bool active = xorg_deep_color_ && OPTION(kDispDeepColor) &&
                        hdr::DeepColor10Available() && panel_color_depth_ == 32;
    if (!active || !im) {
        // Hide the 10-bit child (if any) so the wx software path shows through.
        if (xorg_mapped_) {
            XUnmapWindow(static_cast<Display*>(xorg_display_), xorg_window_);
            xorg_mapped_ = false;
        }
        BasicDrawingPanel::DrawImage(dc, im);  // ordinary 8-bit wx path
        return;
    }

    Display* dpy = static_cast<Display*>(xorg_display_);
    Visual*  vis = static_cast<Visual*>(xorg_visual_);

    if (!xorg_mapped_) {
        XMapWindow(dpy, xorg_window_);
        xorg_mapped_ = true;
    }

    int W = 1, H = 1;
    GetClientSize(&W, &H);
    if (W < 1) W = 1;
    if (H < 1) H = 1;

    // Keep the child window covering the panel.
    XMoveResizeWindow(dpy, xorg_window_, 0, 0, W, H);

    // (Re)allocate the 10-bit image when the window size changes.
    if (!xorg_image_ || xorg_img_w_ != W || xorg_img_h_ != H) {
        if (xorg_image_) {
            XImage* old = static_cast<XImage*>(xorg_image_);
            old->data = nullptr;
            XDestroyImage(old);
            xorg_image_ = nullptr;
        }
        xorg_buf_.assign((size_t)W * H, 0);
        XImage* img = XCreateImage(dpy, vis, 30, ZPixmap, 0,
                                   reinterpret_cast<char*>(xorg_buf_.data()),
                                   W, H, 32, 0);
        if (!img) {
            BasicDrawingPanel::DrawImage(dc, im);
            return;
        }
        xorg_image_ = img;
        xorg_img_w_ = W;
        xorg_img_h_ = H;
    }

    // Nearest-neighbor scale the corrected 8-bit image to the window size and
    // expand each channel to 10 bits. The source is 8-bit, so (like the GL and
    // Vulkan deep-color paths) the win is a 10-bit output surface and 10-bit
    // scaling -- less banding -- not extra source precision.
    const unsigned char* sd = im->GetData();
    const int sw = im->GetWidth();
    const int sh = im->GetHeight();
    const int rs = xorg_r_shift_, gs = xorg_g_shift_, bs = xorg_b_shift_;
    uint32_t* dst = xorg_buf_.data();
    for (int y = 0; y < H; ++y) {
        const int sy = (sh == H) ? y : (y * sh / H);
        const unsigned char* srow = sd + (size_t)sy * sw * 3;
        uint32_t* drow = dst + (size_t)y * W;
        for (int x = 0; x < W; ++x) {
            const int sx = (sw == W) ? x : (x * sw / W);
            const unsigned char* p = srow + (size_t)sx * 3;
            uint32_t r = p[0], g = p[1], b = p[2];
            r = (r << 2) | (r >> 6);
            g = (g << 2) | (g >> 6);
            b = (b << 2) | (b >> 6);
            drow[x] = (r << rs) | (g << gs) | (b << bs);
        }
    }

    XPutImage(dpy, xorg_window_, static_cast<GC>(xorg_gc_),
              static_cast<XImage*>(xorg_image_), 0, 0, 0, 0, W, H);
    XFlush(dpy);
}
#endif  // __WXGTK__

#if defined(HAVE_WAYLAND_SUPPORT)
// ============================================================================
// WaylandDrawingPanel -- software HDR sub-driver backing Simple on Wayland.
// Mirrors XOrgDrawingPanel: a child native surface (here a wl_subsurface) carries
// a deep/HDR image while the wx software path handles the non-HDR case.
// ============================================================================
namespace {

// Anonymous shm file for a wl_shm pool. Canonical Wayland pattern: a temp file
// in XDG_RUNTIME_DIR, immediately unlinked, then sized via ftruncate.
int VbamCreateShmFile(size_t size) {
    const char* runtime = getenv("XDG_RUNTIME_DIR");
    if (!runtime || !*runtime)
        runtime = "/tmp";
    char tmpl[256];
    std::snprintf(tmpl, sizeof(tmpl), "%s/vbam-wl-shm-XXXXXX", runtime);
    int fd = mkstemp(tmpl);
    if (fd < 0)
        return -1;
    unlink(tmpl);
    int flags = fcntl(fd, F_GETFD);
    if (flags != -1)
        fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
    if (ftruncate(fd, static_cast<off_t>(size)) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

// Bind only the globals the software HDR path needs: compositor (child surface),
// subcompositor (subsurface to overlay the panel), shm (CPU buffers).
struct VbamWlSimpleGlobals {
    struct wl_compositor*    comp    = nullptr;
    struct wl_subcompositor* subcomp = nullptr;
    struct wl_shm*           shm     = nullptr;
#ifdef HAVE_WAYLAND_VIEWPORTER
    struct wp_viewporter*    vp      = nullptr;
#endif
};
void vbam_wl_simple_global(void* data, struct wl_registry* reg, uint32_t name,
                           const char* iface, uint32_t /*version*/) {
    auto* g = static_cast<VbamWlSimpleGlobals*>(data);
    if (std::strcmp(iface, "wl_compositor") == 0)
        g->comp = static_cast<struct wl_compositor*>(
            wl_registry_bind(reg, name, &wl_compositor_interface, 1));
    else if (std::strcmp(iface, "wl_subcompositor") == 0)
        g->subcomp = static_cast<struct wl_subcompositor*>(
            wl_registry_bind(reg, name, &wl_subcompositor_interface, 1));
    else if (std::strcmp(iface, "wl_shm") == 0)
        g->shm = static_cast<struct wl_shm*>(
            wl_registry_bind(reg, name, &wl_shm_interface, 1));
#ifdef HAVE_WAYLAND_VIEWPORTER
    else if (std::strcmp(iface, "wp_viewporter") == 0)
        g->vp = static_cast<struct wp_viewporter*>(
            wl_registry_bind(reg, name, &wp_viewporter_interface, 1));
#endif
}
void vbam_wl_simple_global_remove(void*, struct wl_registry*, uint32_t) {}
const struct wl_registry_listener kVbamWlSimpleRegistryListener = {
    vbam_wl_simple_global, vbam_wl_simple_global_remove
};

}  // namespace

void WaylandDrawingPanel::BufferReleaseCb(void* data, struct wl_buffer* buf) {
    auto* self = static_cast<WaylandDrawingPanel*>(data);
    for (int i = 0; i < 2; ++i)
        if (self->wl_buffer_[i] == buf)
            self->wl_buffer_busy_[i] = false;
}
static const struct wl_buffer_listener kVbamWlBufferListener = {
    WaylandDrawingPanel::BufferReleaseCb
};

WaylandDrawingPanel::WaylandDrawingPanel(wxWindow* parent, int _width, int _height)
    : BasicDrawingPanel(parent, _width, _height)
{
    // Stand up the HDR subsurface now so SupportsHdr() is valid before the first
    // paint (BasicDrawingPanel never runs DrawingPanelInit()).
    EnsureSetup();
    // Configure the HDR encoder and latch hdr_encoding_ from SupportsHdr().
    UpdateHdrState();
    // Mark initialized so GameArea's EvaluateHdrRenderer runs its "working"
    // check: if HDR is wanted but we couldn't present it, it falls back to
    // another HDR-capable renderer.
    did_init = true;
}

WaylandDrawingPanel::~WaylandDrawingPanel()
{
    Teardown();
}

void WaylandDrawingPanel::EnsureSetup()
{
    if (wl_setup_done_)
        return;
    wl_setup_done_ = true;

    // Only stand up the HDR subsurface when HDR is actually wanted and the
    // compositor can present BT.2020 PQ; otherwise behave exactly like the plain
    // wx software panel. The panel is recreated when kDispHDR changes (render
    // observer -> ResetPanel), so this is re-evaluated on toggle.
    if (!OPTION(kDispHDR) || !hdr::HdrAvailable() || !WaylandHdrPqSupported())
        return;
    if (!IsWayland())
        return;

    GtkWidget* widget = GetWindow()->GetHandle();
    if (!widget)
        return;
    gtk_widget_realize(widget);
    GdkWindow* gwin = gtk_widget_get_window(widget);
    if (!gwin || !GDK_IS_WAYLAND_WINDOW(gwin))
        return;

    GdkDisplay* gdpy = gtk_widget_get_display(widget);
    struct wl_display* dpy = gdk_wayland_display_get_wl_display(gdpy);
    struct wl_surface* parent = gdk_wayland_window_get_wl_surface(gwin);
    if (!dpy || !parent)
        return;

    // Bind our globals on a private event queue, so dispatching buffer-release
    // events (PresentHdr) never reenters GTK's default-queue handlers. Objects
    // created from these globals inherit the queue.
    struct wl_event_queue* queue = wl_display_create_queue(dpy);
    VbamWlSimpleGlobals g;
    struct wl_registry* reg = wl_display_get_registry(dpy);
    wl_proxy_set_queue(reinterpret_cast<wl_proxy*>(reg), queue);
    wl_registry_add_listener(reg, &kVbamWlSimpleRegistryListener, &g);
    wl_display_roundtrip_queue(dpy, queue);
    wl_registry_destroy(reg);
    if (!g.comp || !g.subcomp || !g.shm) {
        if (g.comp)    wl_compositor_destroy(g.comp);
        if (g.subcomp) wl_subcompositor_destroy(g.subcomp);
        if (g.shm)     wl_shm_destroy(g.shm);
#ifdef HAVE_WAYLAND_VIEWPORTER
        if (g.vp)      wp_viewporter_destroy(g.vp);
#endif
        wl_event_queue_destroy(queue);
        return;
    }

    // Child surface + subsurface under GTK's surface.
    struct wl_surface* child = wl_compositor_create_surface(g.comp);
    struct wl_subsurface* sub =
        wl_subcompositor_get_subsurface(g.subcomp, child, parent);
    if (!child || !sub) {
        if (sub)   wl_subsurface_destroy(sub);
        if (child) wl_surface_destroy(child);
        wl_compositor_destroy(g.comp);
        wl_subcompositor_destroy(g.subcomp);
        wl_shm_destroy(g.shm);
#ifdef HAVE_WAYLAND_VIEWPORTER
        if (g.vp) wp_viewporter_destroy(g.vp);
#endif
        wl_event_queue_destroy(queue);
        return;
    }
    // Desync so our commits take effect without a parent commit; drop the input
    // region so GTK keeps handling pointer/keyboard.
    wl_subsurface_set_desync(sub);
    struct wl_region* empty = wl_compositor_create_region(g.comp);
    wl_surface_set_input_region(child, empty);
    wl_region_destroy(empty);
    wl_surface_commit(child);

    // Tag the child surface BT.2020 PQ. If the compositor refuses, abandon the
    // HDR path (fall back to the wx 8-bit software path).
    if (!WaylandSetWlSurfaceHdrPq(child,
            static_cast<float>(OPTION(kDispHDRReferenceWhite)),
            static_cast<float>(OPTION(kDispHDRPeakBrightness)))) {
        wl_subsurface_destroy(sub);
        wl_surface_destroy(child);
        wl_compositor_destroy(g.comp);
        wl_subcompositor_destroy(g.subcomp);
        wl_shm_destroy(g.shm);
#ifdef HAVE_WAYLAND_VIEWPORTER
        if (g.vp) wp_viewporter_destroy(g.vp);
#endif
        wl_event_queue_destroy(queue);
        return;
    }

    wl_display_        = dpy;
    wl_queue_          = queue;
    wl_parent_surface_ = parent;
    wl_compositor_     = g.comp;
    wl_subcompositor_  = g.subcomp;
    wl_shm_            = g.shm;
    wl_child_surface_  = child;
    wl_subsurface_     = sub;
#ifdef HAVE_WAYLAND_VIEWPORTER
    // Optional: when the compositor offers viewporter, scale a source-resolution
    // buffer to the window on its side so we PQ-encode only source pixels per
    // frame instead of the whole window.
    if (g.vp) {
        wl_viewporter_ = g.vp;
        wl_viewport_   = wp_viewporter_get_viewport(g.vp, child);
    }
#endif
    wl_hdr_ready_      = true;
}

bool WaylandDrawingPanel::EnsureBuffers(int W, int H)
{
    if (wl_pool_ && W == wl_buf_w_ && H == wl_buf_h_)
        return true;

    // Tear down any previous pool/buffers.
    for (int i = 0; i < 2; ++i) {
        if (wl_buffer_[i]) {
            wl_buffer_destroy(static_cast<wl_buffer*>(wl_buffer_[i]));
            wl_buffer_[i] = nullptr;
        }
        wl_buffer_busy_[i] = false;
    }
    if (wl_pool_) {
        wl_shm_pool_destroy(static_cast<wl_shm_pool*>(wl_pool_));
        wl_pool_ = nullptr;
    }
    if (wl_buffer_data_[0] && wl_pool_size_)
        munmap(wl_buffer_data_[0], wl_pool_size_);
    wl_buffer_data_[0] = wl_buffer_data_[1] = nullptr;
    if (wl_pool_fd_ >= 0) { close(wl_pool_fd_); wl_pool_fd_ = -1; }
    wl_pool_size_ = 0;
    wl_buf_w_ = wl_buf_h_ = 0;

    if (W < 1 || H < 1)
        return false;

    const size_t stride = static_cast<size_t>(W) * 4;       // ABGR2101010: 4 B/px
    const size_t one    = stride * static_cast<size_t>(H);
    const size_t total  = one * 2;                          // double-buffered

    int fd = VbamCreateShmFile(total);
    if (fd < 0)
        return false;
    void* base = mmap(nullptr, total, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (base == MAP_FAILED) {
        close(fd);
        return false;
    }
    struct wl_shm_pool* pool = wl_shm_create_pool(
        static_cast<wl_shm*>(wl_shm_), fd, static_cast<int32_t>(total));
    if (!pool) {
        munmap(base, total);
        close(fd);
        return false;
    }
    for (int i = 0; i < 2; ++i) {
        struct wl_buffer* b = wl_shm_pool_create_buffer(
            pool, static_cast<int32_t>(i * one), W, H,
            static_cast<int32_t>(stride), WL_SHM_FORMAT_ABGR2101010);
        if (!b) {
            if (i == 1 && wl_buffer_[0]) {
                wl_buffer_destroy(static_cast<wl_buffer*>(wl_buffer_[0]));
                wl_buffer_[0] = nullptr;
            }
            wl_shm_pool_destroy(pool);
            munmap(base, total);
            close(fd);
            return false;
        }
        wl_buffer_add_listener(b, &kVbamWlBufferListener, this);
        wl_buffer_[i]      = b;
        wl_buffer_data_[i] = static_cast<uint8_t*>(base) + i * one;
        wl_buffer_busy_[i] = false;
    }
    wl_pool_      = pool;
    wl_pool_fd_   = fd;
    wl_pool_size_ = total;
    wl_buf_w_     = W;
    wl_buf_h_     = H;
    return true;
}

int WaylandDrawingPanel::AcquireBuffer()
{
    for (int i = 0; i < 2; ++i)
        if (!wl_buffer_busy_[i])
            return i;
    return -1;
}

bool WaylandDrawingPanel::PresentHdr()
{
    struct wl_display* dpy = static_cast<wl_display*>(wl_display_);

    int W = 1, H = 1;
    GetClientSize(&W, &H);
    if (W < 1) W = 1;
    if (H < 1) H = 1;

    // Source: 32-bit RGBA8 todraw (R in the low byte) with a 1px filter border;
    // skip the top border row, like the GL/Vulkan/Quartz HDR paths.
    const int sw     = static_cast<int>(std::ceil(width * scale));
    const int sh     = static_cast<int>(std::ceil(height * scale));
    const int rowlen = static_cast<int>(std::ceil((width + 1) * scale));
    const uint8_t* srcb = todraw + static_cast<size_t>(rowlen) * 4;

#ifdef HAVE_WAYLAND_VIEWPORTER
    // Fast path: encode the source-resolution frame and let the compositor scale
    // it to the window via the viewport, so the per-frame PQ encode is only
    // sw*sh pixels (and the buffer is reallocated only when the source size
    // changes, not on every window resize).
    const bool use_vp = (wl_viewport_ != nullptr) && sw > 0 && sh > 0;
#else
    const bool use_vp = false;
#endif

    const int bufW = use_vp ? sw : W;
    const int bufH = use_vp ? sh : H;
    if (!EnsureBuffers(bufW, bufH))
        return false;

    // Harvest any pending buffer-release events on our private queue so the busy
    // flags are current (GTK's main loop reads the socket; we only dispatch our
    // queue, never GTK's).
    wl_display_dispatch_queue_pending(dpy, static_cast<wl_event_queue*>(wl_queue_));

    int idx = AcquireBuffer();
    if (idx < 0)
        return true;  // both buffers still held; keep showing the last frame

    if (use_vp) {
        // No CPU scaling: PQ10-encode source pixels straight into the
        // source-sized buffer (ABGR2101010 == EncodePQ10's A2B10G10R10 output).
        hdr::EncodePQ10(srcb, rowlen * 4, sw, sh,
                        reinterpret_cast<uint32_t*>(wl_buffer_data_[idx]), sw * 4);
    } else {
        // Fallback (no viewporter): nearest-scale to the client size, then
        // encode the window-sized result.
        const uint32_t* src = reinterpret_cast<const uint32_t*>(srcb);
        wl_scaled_.resize(static_cast<size_t>(W) * H * 4);
        uint32_t* scaled = reinterpret_cast<uint32_t*>(wl_scaled_.data());
        for (int y = 0; y < H; ++y) {
            const int sy = (sh == H) ? y : (sh > 0 ? y * sh / H : 0);
            const uint32_t* srow = src + static_cast<size_t>(sy) * rowlen;
            uint32_t* drow = scaled + static_cast<size_t>(y) * W;
            for (int x = 0; x < W; ++x) {
                const int sx = (sw == W) ? x : (sw > 0 ? x * sw / W : 0);
                drow[x] = srow[sx];
            }
        }
        hdr::EncodePQ10(wl_scaled_.data(), W * 4, W, H,
                        reinterpret_cast<uint32_t*>(wl_buffer_data_[idx]), W * 4);
    }

    // Keep the subsurface positioned over the panel (toplevel-relative, like the
    // Vulkan path), then attach + present.
    GtkWidget* widget   = GetWindow()->GetHandle();
    GtkWidget* toplevel = gtk_widget_get_toplevel(widget);
    int px = 0, py = 0;
    gtk_widget_translate_coordinates(widget, toplevel, 0, 0, &px, &py);
    wl_subsurface_set_position(static_cast<wl_subsurface*>(wl_subsurface_), px, py);

#ifdef HAVE_WAYLAND_VIEWPORTER
    // Scale the source-sized buffer to the current window size (re-issued only
    // when it changes).
    if (use_vp && (W != wl_vp_dst_w_ || H != wl_vp_dst_h_)) {
        wp_viewport_set_destination(static_cast<wp_viewport*>(wl_viewport_), W, H);
        wl_vp_dst_w_ = W;
        wl_vp_dst_h_ = H;
    }
#endif

    wl_surface* child = static_cast<wl_surface*>(wl_child_surface_);
    wl_surface_attach(child, static_cast<wl_buffer*>(wl_buffer_[idx]), 0, 0);
    // Our wl_surface comes from a v1 wl_compositor, which predates
    // wl_surface_damage_buffer (v4); use the v1 wl_surface_damage with the
    // "whole surface" sentinel, which damages everything in any coordinate space
    // (with or without a viewport). (Calling the v4 request on a v1 surface is a
    // protocol error that drops the whole Wayland connection.)
    wl_surface_damage(child, 0, 0, INT32_MAX, INT32_MAX);
    wl_surface_commit(child);
    wl_buffer_busy_[idx] = true;
    wl_mapped_ = true;
    wl_display_flush(dpy);
    return true;
}

void WaylandDrawingPanel::DrawImage(wxWindowDC& dc, wxImage* im)
{
    const bool active =
        wl_hdr_ready_ && HdrActive() && panel_color_depth_ == 32 && todraw;
    if (active && PresentHdr())
        return;

    // Not HDR (or present failed): hide our subsurface so the wx path shows
    // through, then draw the ordinary 8-bit image.
    if (wl_mapped_ && wl_child_surface_) {
        wl_surface* child = static_cast<wl_surface*>(wl_child_surface_);
        wl_surface_attach(child, nullptr, 0, 0);
        wl_surface_commit(child);
        wl_mapped_ = false;
        if (wl_display_)
            wl_display_flush(static_cast<wl_display*>(wl_display_));
    }
    BasicDrawingPanel::DrawImage(dc, im);
}

void WaylandDrawingPanel::Teardown()
{
    if (wl_child_surface_)
        WaylandClearWlSurfaceColor(static_cast<wl_surface*>(wl_child_surface_));
    for (int i = 0; i < 2; ++i)
        if (wl_buffer_[i]) {
            wl_buffer_destroy(static_cast<wl_buffer*>(wl_buffer_[i]));
            wl_buffer_[i] = nullptr;
        }
    if (wl_pool_) {
        wl_shm_pool_destroy(static_cast<wl_shm_pool*>(wl_pool_));
        wl_pool_ = nullptr;
    }
    if (wl_buffer_data_[0] && wl_pool_size_)
        munmap(wl_buffer_data_[0], wl_pool_size_);
    wl_buffer_data_[0] = wl_buffer_data_[1] = nullptr;
    if (wl_pool_fd_ >= 0) { close(wl_pool_fd_); wl_pool_fd_ = -1; }
#ifdef HAVE_WAYLAND_VIEWPORTER
    if (wl_viewport_) {
        wp_viewport_destroy(static_cast<wp_viewport*>(wl_viewport_));
        wl_viewport_ = nullptr;
    }
    if (wl_viewporter_) {
        wp_viewporter_destroy(static_cast<wp_viewporter*>(wl_viewporter_));
        wl_viewporter_ = nullptr;
    }
#endif
    if (wl_subsurface_) {
        wl_subsurface_destroy(static_cast<wl_subsurface*>(wl_subsurface_));
        wl_subsurface_ = nullptr;
    }
    if (wl_child_surface_) {
        wl_surface_destroy(static_cast<wl_surface*>(wl_child_surface_));
        wl_child_surface_ = nullptr;
    }
    if (wl_shm_) {
        wl_shm_destroy(static_cast<wl_shm*>(wl_shm_));
        wl_shm_ = nullptr;
    }
    if (wl_subcompositor_) {
        wl_subcompositor_destroy(static_cast<wl_subcompositor*>(wl_subcompositor_));
        wl_subcompositor_ = nullptr;
    }
    if (wl_compositor_) {
        wl_compositor_destroy(static_cast<wl_compositor*>(wl_compositor_));
        wl_compositor_ = nullptr;
    }
    if (wl_queue_) {
        wl_event_queue_destroy(static_cast<wl_event_queue*>(wl_queue_));
        wl_queue_ = nullptr;
    }
    wl_hdr_ready_ = false;
    wl_mapped_    = false;
}
#endif  // HAVE_WAYLAND_SUPPORT
        
#ifndef NO_OGL
// following 3 for vsync
#ifdef __WXMAC__
#include <OpenGL/OpenGL.h>
#endif
#ifdef __WXGTK__ // should actually check for X11, but GTK implies X11
#ifndef Status
#define Status int
#endif
#include <GL/glx.h>
#endif
#ifdef __WXMSW__
#include <GL/gl.h>
#include <GL/glext.h>

// Cached GL context to avoid AMD driver crash on context deletion
// We cache one context and try to reuse it with new canvases
static wxGLContext* g_cachedGLContext = nullptr;

// Try to reuse cached context with a new canvas
// Returns the reused context if successful, nullptr otherwise
static wxGLContext* TryReuseCachedGLContext(wxGLCanvas* canvas)
{
    if (!g_cachedGLContext)
        return nullptr;

    // Try to make the cached context current on the new canvas
    // This will succeed if the pixel formats are compatible
    if (canvas->SetCurrent(*g_cachedGLContext)) {
        wxGLContext* reused = g_cachedGLContext;
        g_cachedGLContext = nullptr;
        return reused;
    }

    // Pixel format incompatible - can't reuse
    // Leave cached context for potential future reuse or leak it
    return nullptr;
}

// Cache context for reuse instead of deleting it
// This avoids AMD driver crashes during context deletion
static void CacheGLContext(wxGLContext* ctx)
{
    if (!ctx)
        return;

    // Release context from current thread
    wglMakeCurrent(NULL, NULL);

    // If we already have a cached context, we have to leak the old one
    // (deleting it could crash on AMD drivers)
    // This should be rare - typically only one context at a time
    g_cachedGLContext = ctx;
}
#endif

// This is supposed to be the default, but DOUBLEBUFFER doesn't seem to be
// turned on by default for wxGTK.
static int glopts[] = {
    WX_GL_RGBA, WX_GL_DOUBLEBUFFER, 0
};

// 10-bit (deep color) visual, required for an HDR / BT.2020 PQ surface.
static int glopts_hdr[] = {
    WX_GL_RGBA, WX_GL_DOUBLEBUFFER,
    WX_GL_MIN_RED, 10, WX_GL_MIN_GREEN, 10, WX_GL_MIN_BLUE, 10, WX_GL_MIN_ALPHA, 2,
    0
};

// Depth of the X11 screen the app is running on, or 0 when not on X11 (e.g.
// Wayland). A depth >= 30 means the root is a 10-bit "deep color" screen.
static int X11RootDepth() {
#if defined(__WXGTK__)
    GdkDisplay* gdk_dpy = gdk_display_get_default();
    if (!gdk_dpy || !GDK_IS_X11_DISPLAY(gdk_dpy))
        return 0;
    Display* dpy = GDK_DISPLAY_XDISPLAY(gdk_dpy);
    if (!dpy)
        return 0;
    return DefaultDepth(dpy, DefaultScreen(dpy));
#else
    return 0;
#endif
}

// Choose the GL pixel-format attributes for a new canvas. A 10-bit visual is
// only useful where we can actually present HDR (Wayland EGL today), so request
// it solely in that case to avoid disturbing the ordinary 8-bit path.
static int* SelectGlOpts() {
#ifdef HAVE_WAYLAND_EGL
    if (hdr::HdrAvailable() && OPTION(kDispHDR) && IsWayland())
        return glopts_hdr;
#endif
    // 10-bit "deep color" SDR visual (X11). The option is a preference that may
    // be on without 10-bit support, so gate on actual availability here.
    if (hdr::DeepColor10Available() && OPTION(kDispDeepColor))
        return glopts_hdr;
    // On a 10-bit (depth-30) X screen the GL canvas visual must match the
    // screen depth: wx's default FBConfig pick is an 8-bit (depth-24) visual,
    // and a depth-24 GL window composited onto a depth-30 root renders with a
    // heavy red/magenta cast. Request the native 10-bit visual regardless of
    // the deep-color option (which only controls the GL_RGB10_A2 upload
    // precision); without a matching 10-bit FBConfig there is nothing better
    // to pick, so fall through to the ordinary 8-bit attributes.
    if (X11RootDepth() >= 30 && hdr::DeepColor10Available())
        return glopts_hdr;
    return glopts;
}

bool GLDrawingPanel::SetContext()
{
#ifdef __WXMSW__
    // On Windows, verify the window handle is still valid before GL operations.
    // This prevents crashes in GL drivers when the window is being destroyed.
    HWND hwnd = GetHWND();
    if (!hwnd || !::IsWindow(hwnd)) {
        return false;
    }
#endif

#ifndef wxGL_IMPLICIT_CONTEXT
    // Check if the current context is valid
    if (!ctx
#if wxCHECK_VERSION(3, 1, 0)
        || !ctx->IsOK()
#endif
        )
    {
        // Cache the old context on Windows (deleting can crash AMD drivers)
#ifdef __WXMSW__
        CacheGLContext(ctx);
#else
        delete ctx;
#endif
        ctx = NULL;

        // Try to reuse cached context first (Windows only)
#ifdef __WXMSW__
        ctx = TryReuseCachedGLContext(this);
#endif
        // Create a new context if we couldn't reuse one
        if (!ctx) {
            ctx = new wxGLContext(this);
        }
        DrawingPanelInit();
    }
    return wxGLCanvas::SetCurrent(*ctx);
#else
    return wxGLContext::SetCurrent(*this);
#endif
}
        
GLDrawingPanel::GLDrawingPanel(wxWindow* parent, int _width, int _height)
        : DrawingPanelBase(_width, _height)
        , wxglc(parent, wxID_ANY, SelectGlOpts(), wxPoint(0, 0), parent->GetClientSize(),
                wxFULL_REPAINT_ON_RESIZE | wxWANTS_CHARS)
{
    widgets::RequestHighResolutionOpenGlSurfaceForWindow(this);
    SetContext();
}
        
GLDrawingPanel::~GLDrawingPanel()
{
    DestroyHdrSurface();

    // Skip GL cleanup entirely on Windows when caching context
    // The resources will be reused or cleaned up when the context is eventually deleted
#ifdef __WXMSW__
    #ifndef wxGL_IMPLICIT_CONTEXT
    // Cache context on Windows for reuse (deleting can crash AMD drivers)
    // Skip glDeleteLists/glDeleteTextures as they may cause heap corruption
    // when the window is being destroyed
    CacheGLContext(ctx);
    ctx = nullptr;  // Prevent base class from trying to use it
    #endif
#else
    // Clean up GL resources if we have a valid context
    // Don't call SetContext() here as it might try to recreate the context
    // on a half-destroyed window, causing driver crashes
#ifndef wxGL_IMPLICIT_CONTEXT
    if (did_init && ctx) {
        // Try to make the context current for cleanup, but don't recreate it
#if wxCHECK_VERSION(3, 1, 0)
        if (ctx->IsOK()) {
#endif
            if (wxGLCanvas::SetCurrent(*ctx)) {
                glDeleteLists(vlist, 1);
                glDeleteTextures(1, &texid);
            }
#if wxCHECK_VERSION(3, 1, 0)
        }
#endif
    }
    delete ctx;
#else
    // With implicit context, just try to clean up if initialized
    if (did_init) {
        glDeleteLists(vlist, 1);
        glDeleteTextures(1, &texid);
    }
#endif
#endif
}
        
void GLDrawingPanel::DrawingPanelInit()
{
    // No usable GL context (creation or MakeCurrent failed) -> flag so GameArea
    // falls back to the next renderer in the priority list. We return before the
    // base init, so did_init stays false and the destructor's GL cleanup (guarded
    // by did_init) is correctly skipped -- texid/vlist were never generated.
    if (!SetContext()) {
        init_failed_ = true;
        return;
    }

    DrawingPanelBase::DrawingPanelInit();

    AdjustViewport();
            
    const bool bilinear = OPTION(kDispBilinear);
            
    // taken from GTK front end almost verbatim
    glDisable(GL_CULL_FACE);
    glEnable(GL_TEXTURE_2D);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, 1.0, 1.0, 0.0, 0.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    vlist = glGenLists(1);
    glNewList(vlist, GL_COMPILE);
    glBegin(GL_TRIANGLE_STRIP);
    glTexCoord2f(0.0, 0.0);
    glVertex3i(0, 0, 0);
    glTexCoord2f(1.0, 0.0);
    glVertex3i(1, 0, 0);
    glTexCoord2f(0.0, 1.0);
    glVertex3i(0, 1, 0);
    glTexCoord2f(1.0, 1.0);
    glVertex3i(1, 1, 0);
    glEnd();
    glEndList();
    glGenTextures(1, &texid);
    glBindTexture(GL_TEXTURE_2D, texid);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                    bilinear ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                    bilinear ? GL_LINEAR : GL_NEAREST);
            
#define int_fmt out_16 ? GL_RGB5 : GL_RGB
#define tex_fmt out_8  ? GL_RGB : \
    out_16 ? GL_BGRA : \
    out_24 ? GL_RGB : GL_RGBA, \
    out_8 ? GL_UNSIGNED_BYTE_3_3_2 : \
    out_16 ? GL_UNSIGNED_SHORT_1_5_5_5_REV : GL_UNSIGNED_BYTE
#if 0
    texsize = width > height ? width : height;
    texsize = std::ceil(texsize * scale);
    // texsize = 1 << ffs(texsize);
    texsize = texsize | (texsize >> 1);
    texsize = texsize | (texsize >> 2);
    texsize = texsize | (texsize >> 4);
    texsize = texsize | (texsize >> 8);
    texsize = (texsize >> 1) + 1;
    glTexImage2D(GL_TEXTURE_2D, 0, int_fmt, texsize, texsize, 0, tex_fmt, NULL);
#else
    // but really, most cards support non-p2 and rect
    // if not, use cairo or wx renderer
    glTexImage2D(GL_TEXTURE_2D, 0, int_fmt, std::ceil(width * scale), std::ceil(height * scale), 0, tex_fmt, NULL);
#endif
    glClearColor(0.0, 0.0, 0.0, 1.0);

    // Record whether we actually got a >=10-bit visual, so the deep-color
    // option can tell if it took effect (legacy GL: GL_RED_BITS is valid).
    {
        GLint red_bits = 8;
        glGetIntegerv(GL_RED_BITS, &red_bits);
        gl_deep_color_ = OPTION(kDispDeepColor) && red_bits >= 10;
    }

    // non-portable vsync code
#if defined(__WXGTK__)
#ifndef NO_WAYLAND
    if (IsWayland()) {
#ifdef HAVE_EGL
        if (OPTION(kPrefVsync))
            wxLogDebug(_("Enabling EGL VSync."));
        else
            wxLogDebug(_("Disabling EGL VSync."));
                
        eglSwapInterval(0, OPTION(kPrefVsync));
#endif
    }
    else {
#endif
        if (OPTION(kPrefVsync))
            wxLogDebug(_("Enabling GLX VSync."));
        else
            wxLogDebug(_("Disabling GLX VSync."));
                
        static PFNGLXSWAPINTERVALEXTPROC glXSwapIntervalEXT = NULL;
        static PFNGLXSWAPINTERVALSGIPROC glXSwapIntervalSGI = NULL;
        static PFNGLXSWAPINTERVALMESAPROC glXSwapIntervalMESA = NULL;

        auto display        = GetX11Display();
        auto default_screen = DefaultScreen(display);
                
        char* glxQuery = (char*)glXQueryExtensionsString(display, default_screen);
                
        if (strstr(glxQuery, "GLX_EXT_swap_control") != NULL)
        {
            glXSwapIntervalEXT = reinterpret_cast<PFNGLXSWAPINTERVALEXTPROC>(glXGetProcAddress((const GLubyte*)"glXSwapIntervalEXT"));
            if (glXSwapIntervalEXT)
                glXSwapIntervalEXT(glXGetCurrentDisplay(),
                                    glXGetCurrentDrawable(), OPTION(kPrefVsync));
            else
                wxLogError(_("Failed to set glXSwapIntervalEXT"));
        }
        if (strstr(glxQuery, "GLX_SGI_swap_control") != NULL)
        {
            glXSwapIntervalSGI = reinterpret_cast<PFNGLXSWAPINTERVALSGIPROC>(glXGetProcAddress((const GLubyte*)("glXSwapIntervalSGI")));
                    
            if (glXSwapIntervalSGI)
                glXSwapIntervalSGI(OPTION(kPrefVsync));
            else
                wxLogError(_("Failed to set glXSwapIntervalSGI"));
        }
        if (strstr(glxQuery, "GLX_MESA_swap_control") != NULL)
        {
            glXSwapIntervalMESA = reinterpret_cast<PFNGLXSWAPINTERVALMESAPROC>(glXGetProcAddress((const GLubyte*)("glXSwapIntervalMESA")));
                    
            if (glXSwapIntervalMESA)
                glXSwapIntervalMESA(OPTION(kPrefVsync));
            else
                wxLogError(_("Failed to set glXSwapIntervalMESA"));
        }
#ifndef NO_WAYLAND
    }
#endif
#elif defined(__WXMSW__)
    typedef const char* (*wglext)();
    wglext wglGetExtensionsStringEXT = reinterpret_cast<wglext>(reinterpret_cast<void*>(wglGetProcAddress("wglGetExtensionsStringEXT")));
    if (wglGetExtensionsStringEXT == NULL) {
        wxLogError(_("No support for wglGetExtensionsStringEXT"));
    }
    else if (strstr(wglGetExtensionsStringEXT(), "WGL_EXT_swap_control") == 0) {
        wxLogError(_("No support for WGL_EXT_swap_control"));
    }
            
    typedef BOOL (__stdcall *PFNWGLSWAPINTERVALEXTPROC)(BOOL);
    static PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT = NULL;
    wglSwapIntervalEXT = reinterpret_cast<PFNWGLSWAPINTERVALEXTPROC>(reinterpret_cast<void*>(wglGetProcAddress("wglSwapIntervalEXT")));
    if (wglSwapIntervalEXT)
        wglSwapIntervalEXT(OPTION(kPrefVsync));
    else
        wxLogError(_("Failed to set wglSwapIntervalEXT"));
#elif defined(__WXMAC__)
    int swap_interval = OPTION(kPrefVsync) ? 1 : 0;
    CGLContextObj cgl_context = CGLGetCurrentContext();
    CGLSetParameter(cgl_context, kCGLCPSwapInterval, &swap_interval);
#else
    systemScreenMessage(_("No VSYNC available on this platform"));
#endif

    SetupHdrSurface();
}

bool GLDrawingPanel::SupportsHdr() const
{
    // True HDR presentation requires a compositor color-management path, which
    // on Linux means Wayland. X11/GLX has no way to signal an HDR surface.
    //
    // Two routes signal BT.2020 PQ to the compositor:
    //  - the wp_color_manager_v1 protocol (KWin etc.), which tags our existing
    //    EGL surface -- the general, driver-independent path (works on Mesa); or
    //  - EGL_EXT_gl_colorspace_bt2020_pq, exposed only by the NVIDIA driver.
    // Probe both here so HDR is advertised only when one can actually present
    // it, rather than enabling HDR and failing later at draw time.
#ifdef HAVE_WAYLAND_EGL
    return IsWayland() && (WaylandHdrPqSupported() || HdrEglColorspaceAvailable());
#else
    return false;
#endif
}

bool GLDrawingPanel::HdrEglColorspaceAvailable() const
{
#ifdef HAVE_WAYLAND_EGL
    EGLDisplay dpy = GetEGLDisplay();
    if (dpy == EGL_NO_DISPLAY)
        return false;
    const char* exts = eglQueryString(dpy, EGL_EXTENSIONS);
    return exts && strstr(exts, "EGL_EXT_gl_colorspace_bt2020_pq");
#else
    return false;
#endif
}

#ifdef HAVE_WAYLAND_EGL
// wxGLCanvasEGL::GetEGLConfig() returns EGLConfig* on wx 3.2 but EGLConfig on
// wx 3.3; normalize both to a single EGLConfig value.
static inline EGLConfig VbamEglConfig(EGLConfig* c) { return c ? *c : nullptr; }
static inline EGLConfig VbamEglConfig(EGLConfig c) { return c; }
#endif

void GLDrawingPanel::SetupHdrSurface()
{
    DestroyHdrSurface();

#ifdef HAVE_WAYLAND_EGL
    if (!HdrActive() || !IsWayland())
        return;

    // Preferred, driver-independent path: keep rendering PQ-encoded 10-bit
    // content into the ordinary EGL surface and tag that surface as BT.2020 PQ
    // via wp_color_manager_v1. No dedicated PQ EGL surface is involved, so
    // hdr_egl_surface_ stays null and DrawArea() presents through SwapBuffers().
    if (WaylandHdrPqSupported()) {
        if (WaylandSetSurfaceHdrPq(this,
                static_cast<float>(OPTION(kDispHDRReferenceWhite)),
                static_cast<float>(OPTION(kDispHDRPeakBrightness)))) {
            wxLogDebug(wxT("HDR: tagged surface BT.2020 PQ via wp_color_manager_v1."));
            return;
        }
        // Tagging failed unexpectedly; fall through to the EGL extension path.
    }

    EGLDisplay dpy = GetEGLDisplay();
    EGLConfig  cfg = VbamEglConfig(GetEGLConfig());
    if (dpy == EGL_NO_DISPLAY || !cfg || !m_wlEGLWindow) {
        wxLogDebug(wxT("HDR: no EGL display/config/window; falling back to SDR."));
        hdr_encoding_ = hdr::Encoding::kNone;
        return;
    }

    // Already vetted by SupportsHdr() before HDR was enabled; this is a
    // defensive guard, so log at debug level rather than popping a dialog.
    if (!HdrEglColorspaceAvailable()) {
        wxLogDebug(wxT("HDR: EGL_EXT_gl_colorspace_bt2020_pq unavailable; "
                       "falling back to SDR."));
        hdr_encoding_ = hdr::Encoding::kNone;
        return;
    }

    const EGLint attribs[] = {
        EGL_GL_COLORSPACE, EGL_GL_COLORSPACE_BT2020_PQ_EXT, EGL_NONE
    };
    EGLSurface surf = eglCreateWindowSurface(
        dpy, cfg, reinterpret_cast<EGLNativeWindowType>(m_wlEGLWindow), attribs);
    if (surf == EGL_NO_SURFACE) {
        wxLogError(_("HDR: failed to create BT.2020 PQ surface; "
                     "falling back to SDR."));
        hdr_encoding_ = hdr::Encoding::kNone;
        return;
    }
    hdr_egl_surface_ = surf;
    wxLogDebug(wxT("HDR: created BT.2020 PQ EGL surface."));

    // Declare the content luminance envelope on the PQ surface. A bare PQ
    // surface is assumed to carry PQ's full 0..10000-nit range, so the
    // compositor tone-maps that phantom range down to the display and squashes
    // our real highlights -- which only reach peak_nits -- back near reference
    // white, making the luminance settings invisible. The NVIDIA driver carries
    // this via the SMPTE 2086 / CTA-861.3 EGL metadata extensions; mirrors the
    // Wayland wp_color_manager_v1, Vulkan vkSetHdrMetadataEXT and D3D12 paths.
#if defined(EGL_EXT_surface_SMPTE2086_metadata) && \
    defined(EGL_EXT_surface_CTA861_3_metadata)
    {
        const char* surf_exts = eglQueryString(dpy, EGL_EXTENSIONS);
        const bool have_smpte2086 = surf_exts &&
            strstr(surf_exts, "EGL_EXT_surface_SMPTE2086_metadata");
        const bool have_cta861_3 = surf_exts &&
            strstr(surf_exts, "EGL_EXT_surface_CTA861_3_metadata");

        const int peak_nits = static_cast<int>(OPTION(kDispHDRPeakBrightness));
        const int ref_nits  = static_cast<int>(OPTION(kDispHDRReferenceWhite));
        // All metadata values are passed as integers scaled by
        // EGL_METADATA_SCALING_EXT (chromaticities in [0,1], luminance in cd/m²).
        const int S = EGL_METADATA_SCALING_EXT;
        auto attr = [&](EGLint a, EGLint v) {
            eglSurfaceAttrib(dpy, surf, a, v);
        };

        if (have_smpte2086) {
            // BT.2020 primaries + D65 white, matching the PQ color space.
            attr(EGL_SMPTE2086_DISPLAY_PRIMARY_RX_EXT, (EGLint)(0.708f * S));
            attr(EGL_SMPTE2086_DISPLAY_PRIMARY_RY_EXT, (EGLint)(0.292f * S));
            attr(EGL_SMPTE2086_DISPLAY_PRIMARY_GX_EXT, (EGLint)(0.170f * S));
            attr(EGL_SMPTE2086_DISPLAY_PRIMARY_GY_EXT, (EGLint)(0.797f * S));
            attr(EGL_SMPTE2086_DISPLAY_PRIMARY_BX_EXT, (EGLint)(0.131f * S));
            attr(EGL_SMPTE2086_DISPLAY_PRIMARY_BY_EXT, (EGLint)(0.046f * S));
            attr(EGL_SMPTE2086_WHITE_POINT_X_EXT,      (EGLint)(0.3127f * S));
            attr(EGL_SMPTE2086_WHITE_POINT_Y_EXT,      (EGLint)(0.3290f * S));
            attr(EGL_SMPTE2086_MAX_LUMINANCE_EXT,      peak_nits * S);
            attr(EGL_SMPTE2086_MIN_LUMINANCE_EXT,      0);
        }
        if (have_cta861_3) {
            attr(EGL_CTA861_3_MAX_CONTENT_LIGHT_LEVEL_EXT, peak_nits * S);
            // Frame-average light level must not exceed peak.
            attr(EGL_CTA861_3_MAX_FRAME_AVERAGE_LEVEL_EXT,
                 std::min(ref_nits, peak_nits) * S);
        }
        if (!have_smpte2086 && !have_cta861_3)
            wxLogDebug(wxT("HDR: no EGL HDR metadata extension; luminance hints "
                           "not signaled to compositor."));
    }
#endif
#endif
}

void GLDrawingPanel::DestroyHdrSurface()
{
#ifdef HAVE_WAYLAND_EGL
    // Drop any wp_color_manager_v1 tag on our wl_surface (no-op if untagged).
    WaylandClearSurfaceColor(this);

    if (hdr_egl_surface_) {
        EGLDisplay dpy = GetEGLDisplay();
        if (dpy != EGL_NO_DISPLAY)
            eglDestroySurface(dpy, static_cast<EGLSurface>(hdr_egl_surface_));
    }
#endif
    hdr_egl_surface_ = nullptr;
}

void GLDrawingPanel::OnSize(wxSizeEvent& ev)
{
    AdjustViewport();

#ifndef NO_WAYLAND
    // Temporary hack to backport 800d6ed69b from wxWidgets until 3.2.2 is released.
    if (IsWayland())
        MoveWaylandSubsurface(this);
#endif

    ev.Skip();
}
        
void GLDrawingPanel::AdjustViewport()
{
    SetContext();
            
    int x, y;
    widgets::GetRealPixelClientSize(this, &x, &y);
    glViewport(0, 0, x, y);
}
        
void GLDrawingPanel::RefreshGL()
{
    SetContext();
            
    // Rebind any textures or other OpenGL resources here
            
    glBindTexture(GL_TEXTURE_2D, texid);
}
        
void GLDrawingPanel::PaintEv(wxPaintEvent& ev)
{
    (void)ev;
    // Never use wx DC drawing on the NSOpenGLView during paint events.
    // On macOS, any wx DC drawing call (DrawRectangle etc.) on an NSOpenGLView
    // triggers EnsureIsValid() -> wxOSXLockFocus() -> lockFocusIfCanDraw()
    // -> [NSView setNeedsDisplay:YES], causing an infinite repaint loop.
    // DrawArea already handles the !todraw case with glClear, so always
    // go through OpenGL here instead of the base class draw_black_background path.
    // The wxPaintDC is created to acknowledge the paint event but never drawn with.
    wxPaintDC dc(GetWindow());
    DrawArea(dc);
}

void GLDrawingPanel::DrawArea(wxWindowDC& dc)
{
    (void)dc; // unused params
    SetContext();
    RefreshGL();

    if (!did_init)
        DrawingPanelInit();

    // HDR presentation encodes the frame to 10-bit BT.2020 PQ. Two delivery
    // paths share that encoding:
    //  - color-management protocol: render into wx's ordinary surface (already
    //    tagged BT.2020 PQ) and present via SwapBuffers() -- hdr_egl_surface_ is
    //    null;
    //  - NVIDIA EGL_EXT_gl_colorspace_bt2020_pq: present into a dedicated PQ
    //    surface (hdr_egl_surface_). Bind it now, after RefreshGL()/SetContext()
    //    bound wx's surface; texture/viewport bindings are context state and
    //    survive the surface switch since the EGLContext is unchanged.
    const bool hdr     = HdrActive() && panel_color_depth_ == 32;
    [[maybe_unused]] const bool hdr_egl = hdr && hdr_egl_surface_;
#ifdef HAVE_WAYLAND_EGL
    if (hdr_egl)
        eglMakeCurrent(GetEGLDisplay(), static_cast<EGLSurface>(hdr_egl_surface_),
                       static_cast<EGLSurface>(hdr_egl_surface_), eglGetCurrentContext());
#endif

    if (todraw) {
        // Calculate inrb based on panel's color depth, not global systemColorDepth
        int inrb = (panel_color_depth_ == 8) ? 4 : (panel_color_depth_ == 16) ? 2 : (panel_color_depth_ == 24) ? 0 : 1;
        int rowlen = std::ceil((width + inrb) * scale);
        int tex_width = (int)std::ceil(width * scale);
        int tex_height = (int)std::ceil(height * scale);
        int offset = (int)std::ceil(rowlen * (panel_color_depth_ >> 3));
        uint8_t* tex_ptr = todraw + offset;

        if (hdr) {
            // Re-encode the corrected RGBA8 image to 10-bit BT.2020 PQ. The
            // encoder output is tightly packed, so reset the row length.
            int bpp = 0;
            const uint8_t* enc =
                EncodeHdr(tex_ptr, rowlen * 4, tex_width, tex_height, &bpp);
            glPixelStorei(GL_UNPACK_ROW_LENGTH, tex_width);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB10_A2, tex_width, tex_height, 0,
                         GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV, enc);
        } else {
            glPixelStorei(GL_UNPACK_ROW_LENGTH, rowlen);
#if wxBYTE_ORDER == wxBIG_ENDIAN
            // FIXME: is this necessary?
            if (panel_color_depth_ == 16)
                glPixelStorei(GL_UNPACK_SWAP_BYTES, GL_TRUE);
#endif
            // 10-bit "deep color" SDR: keep the normal 8-bit upload but ask GL
            // for a 10-bit internal format, so filtering and the framebuffer
            // stay 10-bit and the compositor gets a 10-bit surface (less
            // banding). No tone mapping -- this is still SDR.
            GLint ifmt = int_fmt;
            if (hdr::DeepColor10Available() && OPTION(kDispDeepColor) && panel_color_depth_ == 32)
                ifmt = GL_RGB10_A2;
            glTexImage2D(GL_TEXTURE_2D, 0, ifmt, tex_width, tex_height,
                         0, tex_fmt, tex_ptr);
        }

        glCallList(vlist);
    } else
        glClear(GL_COLOR_BUFFER_BIT);

#ifdef HAVE_WAYLAND_EGL
    if (hdr_egl) {
        eglSwapBuffers(GetEGLDisplay(), static_cast<EGLSurface>(hdr_egl_surface_));
        return;
    }
#endif
    SwapBuffers();
}

// These GL-only helper macros must not leak into the D3D12 code below, which
// uses a local `tex_fmt` variable of its own.
#undef int_fmt
#undef tex_fmt

#endif // GL support

#if defined(__WXMSW__) && !defined(NO_D3D12)
#define DIRECT3D_VERSION 0x0c00
// Simple HLSL shaders embedded as strings
static const char* g_VertexShaderSrc = R"(
    struct VSIn  { float4 pos : POSITION; float2 uv : TEXCOORD0; };
    struct VSOut { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; };
    VSOut main(VSIn input) { VSOut o; o.pos = input.pos; o.uv = input.uv; return o; }
)";

static const char* g_PixelShaderSrc = R"(
    Texture2D    tex : register(t0);
    SamplerState smp : register(s0);
    struct PSIn { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; };
    float4 main(PSIn input) : SV_TARGET { return tex.Sample(smp, input.uv); }
)";

typedef HRESULT(WINAPI* LPFND3DCompile)(LPCVOID pSrcData, SIZE_T SrcDataSize, LPCSTR pSourceName, const D3D_SHADER_MACRO* pDefines, ID3DInclude* pInclude, LPCSTR pEntrypoint, LPCSTR pTarget, UINT Flags1, UINT Flags2, ID3DBlob** ppCode, ID3DBlob** ppErrorMsgs);
typedef HRESULT(WINAPI* LPFND3D12SerializeRootSignature)(const D3D12_ROOT_SIGNATURE_DESC* pRootSignature, D3D_ROOT_SIGNATURE_VERSION Version, ID3DBlob** ppBlob, ID3DBlob** ppErrorBlob);
typedef HRESULT(WINAPI* LPFNCreateDXGIFactory1)(REFIID riid, void** ppFactory);
typedef HRESULT(WINAPI* LPFND3D12CreateDevice)(IUnknown* pAdapter, D3D_FEATURE_LEVEL MinimumFeatureLevel, REFIID riid, void** ppDevice);
typedef HRESULT(WINAPI* LPFND3D12GetDebugInterface)(REFIID riid, void** ppvDebug);

DX12DrawingPanel::DX12DrawingPanel(wxWindow* parent, int _width, int _height)
    : DrawingPanel(parent, _width, _height)
    , fence_value(0)
    , fence_event(NULL)
    , frame_index(0)
    , rtv_descriptor_size(0)
    , texture_width(0)
    , texture_height(0)
{
    HRESULT hr;
    BOOL using_warp = false;

    memset(delta, 0xff, sizeof(delta));

    // Match what SDL/GL/Metal constructors do:
    if (OPTION(kDispFilter) == config::Filter::kNone &&
        OPTION(kDispIFB) == config::Interframe::kNone) {
        systemColorDepth = (OPTION(kBitDepth) + 1) << 3;
    }

    hD3DCompiler = LoadLibrary(TEXT("d3dcompiler_47.dll"));
    hD3D12 = LoadLibrary(TEXT("d3d12.dll"));
	hDXGI = LoadLibrary(TEXT("dxgi.dll"));

	if (hD3DCompiler == NULL || hD3D12 == NULL || hDXGI == NULL) {
        wxLogError(_("Failed to load D3D12 or D3DCompiler DLLs"));
        return;
    }

    // --- 1. DXGI Factory & Adapter ---
    ComPtr<IDXGIFactory4> factory;
	LPFND3D12CreateDevice CREATEDEVICE = reinterpret_cast<LPFND3D12CreateDevice>(reinterpret_cast<void*>(GetProcAddress(hD3D12, "D3D12CreateDevice")));
    LPFND3D12SerializeRootSignature D3D12SERIALIZEROOTSIGNATURE = reinterpret_cast<LPFND3D12SerializeRootSignature>(reinterpret_cast<void*>(GetProcAddress(hD3D12, "D3D12SerializeRootSignature")));
    LPFND3DCompile D3DCOMPILE = reinterpret_cast<LPFND3DCompile>(reinterpret_cast<void*>(GetProcAddress(hD3DCompiler, "D3DCompile")));
    LPFNCreateDXGIFactory1 CREATEFACTORY = reinterpret_cast<LPFNCreateDXGIFactory1>(reinterpret_cast<void*>(GetProcAddress(hDXGI, "CreateDXGIFactory1")));

#if defined(_DEBUG)
    // Enable the D3D12 debug layer in debug builds
    {
        ComPtr<ID3D12Debug> debug_controller;
        LPFND3D12GetDebugInterface GETDEBUGINTERFACE = reinterpret_cast<LPFND3D12GetDebugInterface>(GetProcAddress(hD3D12, "D3D12GetDebugInterface"));

        if (GETDEBUGINTERFACE) {
            if (SUCCEEDED(GETDEBUGINTERFACE(IID_PPV_ARGS(&debug_controller)))) {
                debug_controller->EnableDebugLayer();
            }
        }
    }
#endif

	if (CREATEFACTORY == nullptr || CREATEDEVICE == nullptr || D3DCOMPILE == nullptr || D3D12SERIALIZEROOTSIGNATURE == nullptr) {
        wxLogError(_("Failed to get D3D12 function pointers"));
        return;
    }

    hr = CREATEFACTORY(IID_PPV_ARGS(&factory));
    if (FAILED(hr)) { wxLogError(_("Failed to create DXGI factory: 0x%08X"), hr); return; }

    // Try hardware adapter first; fall back to WARP software renderer
    ComPtr<IDXGIAdapter1> adapter;
    for (UINT i = 0; factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;

        hr = CREATEDEVICE(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_ID3D12Device, nullptr);
        if (SUCCEEDED(hr)) break;
        adapter = nullptr;
    }
    if (!adapter) {
        wxLogDebug(_("No hardware adapter found, falling back to WARP"));
        factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter));
        using_warp = true;
    }

    // --- 2. D3D12 Device ---
    hr = CREATEDEVICE(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device));
    if (FAILED(hr)) { wxLogError(_("Failed to create D3D12 device: 0x%08X"), hr); return; }
    wxLogDebug(_("D3D12 device created (%s)"), using_warp ? wxT("WARP") : wxT("hardware"));

    // --- 3. Command Queue ---
    D3D12_COMMAND_QUEUE_DESC queue_desc = {};
    queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    hr = device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&command_queue));
    if (FAILED(hr)) { wxLogError(_("Failed to create command queue: 0x%08X"), hr); return; }

    // --- 4. Swap Chain ---
    // For HDR use a 10-bit back buffer; the BT.2020 PQ color space is set on
    // the swapchain below once we have an IDXGISwapChain3.
    rt_format_ = (hdr::HdrAvailable() && OPTION(kDispHDR)) ? DXGI_FORMAT_R10G10B10A2_UNORM
                                                          : DXGI_FORMAT_B8G8R8A8_UNORM;

    wxSize win_size = GetClientSize();
    DXGI_SWAP_CHAIN_DESC1 sc_desc = {};
    sc_desc.BufferCount = FRAME_COUNT;
    sc_desc.Width = (UINT)win_size.GetWidth();
    sc_desc.Height = (UINT)win_size.GetHeight();
    sc_desc.Format = rt_format_;
    sc_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sc_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; // D3D12 requires FLIP_*
    sc_desc.SampleDesc.Count = 1;
    sc_desc.Flags = OPTION(kPrefVsync) ? 0 : DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

    ComPtr<IDXGISwapChain1> sc1;
    hr = factory->CreateSwapChainForHwnd(
        command_queue.Get(),
        (HWND)GetHandle(),
        &sc_desc,
        nullptr, nullptr,
        &sc1);
    if (FAILED(hr)) { wxLogError(_("Failed to create swap chain: 0x%08X"), hr); return; }

    // Disable Alt+Enter fullscreen toggle (handled by the emulator itself)
    factory->MakeWindowAssociation((HWND)GetHandle(), DXGI_MWA_NO_ALT_ENTER);

    hr = sc1.As(&swap_chain);
    if (FAILED(hr)) { wxLogError(_("Failed to get IDXGISwapChain3: 0x%08X"), hr); return; }
    frame_index = swap_chain->GetCurrentBackBufferIndex();

    // --- 4b. HDR10 BT.2020 PQ color space (if requested and supported) ---
    if (hdr::HdrAvailable() && OPTION(kDispHDR)) {
        const DXGI_COLOR_SPACE_TYPE pq_cs =
            DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020;
        UINT cs_support = 0;
        if (SUCCEEDED(swap_chain->CheckColorSpaceSupport(pq_cs, &cs_support)) &&
            (cs_support & DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT)) {
            if (SUCCEEDED(swap_chain->SetColorSpace1(pq_cs))) {
                hdr_swapchain_ = true;

                // Provide HDR10 mastering metadata derived from the user's
                // brightness settings (luminance is in 0.0001-nit units).
                ComPtr<IDXGISwapChain4> sc4;
                if (SUCCEEDED(swap_chain.As(&sc4))) {
                    DXGI_HDR_METADATA_HDR10 md = {};
                    md.MaxMasteringLuminance =
                        (UINT)(OPTION(kDispHDRPeakBrightness) * 10000u);
                    md.MinMasteringLuminance = 0;
                    md.MaxContentLightLevel =
                        (UINT16)OPTION(kDispHDRPeakBrightness);
                    md.MaxFrameAverageLightLevel =
                        (UINT16)OPTION(kDispHDRReferenceWhite);
                    sc4->SetHDRMetaData(DXGI_HDR_METADATA_TYPE_HDR10,
                                        sizeof(md), &md);
                }
            }
        }
        if (!hdr_swapchain_)
            wxLogError(_("HDR: display/swapchain does not support BT.2020 PQ; "
                         "output will be SDR."));
    }

    // --- 5. RTV Descriptor Heap ---
    D3D12_DESCRIPTOR_HEAP_DESC rtv_heap_desc = {};
    rtv_heap_desc.NumDescriptors = FRAME_COUNT;
    rtv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtv_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    hr = device->CreateDescriptorHeap(&rtv_heap_desc, IID_PPV_ARGS(&rtv_heap));
    if (FAILED(hr)) { wxLogError(_("Failed to create RTV heap: 0x%08X"), hr); return; }
    rtv_descriptor_size = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    // --- 6. SRV Descriptor Heap (for texture) ---
    D3D12_DESCRIPTOR_HEAP_DESC srv_heap_desc = {};
    srv_heap_desc.NumDescriptors = 1;
    srv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srv_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    hr = device->CreateDescriptorHeap(&srv_heap_desc, IID_PPV_ARGS(&srv_heap));
    if (FAILED(hr)) { wxLogError(_("Failed to create SRV heap: 0x%08X"), hr); return; }

    // --- 7. Create RTVs for each back buffer ---
    {
        D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle = rtv_heap->GetCPUDescriptorHandleForHeapStart();
        for (UINT i = 0; i < FRAME_COUNT; i++) {
            hr = swap_chain->GetBuffer(i, IID_PPV_ARGS(&render_targets[i]));
            if (FAILED(hr)) { wxLogError(_("Failed to get back buffer %u: 0x%08X"), i, hr); return; }
            device->CreateRenderTargetView(render_targets[i].Get(), nullptr, rtv_handle);
            rtv_handle.ptr += rtv_descriptor_size;
        }
    }

    // --- 8. Command Allocator & Command List ---
    hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&command_allocator));
    if (FAILED(hr)) { wxLogError(_("Failed to create command allocator: 0x%08X"), hr); return; }

    hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
        command_allocator.Get(), nullptr,
        IID_PPV_ARGS(&command_list));
    if (FAILED(hr)) { wxLogError(_("Failed to create command list: 0x%08X"), hr); return; }
    command_list->Close(); // Close immediately; re-opened each frame

    // --- 9. Root Signature (1 descriptor table: SRV + Sampler inline) ---
    {
        D3D12_DESCRIPTOR_RANGE srv_range = {};
        srv_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        srv_range.NumDescriptors = 1;
        srv_range.BaseShaderRegister = 0;
        srv_range.RegisterSpace = 0;
        srv_range.OffsetInDescriptorsFromTableStart = 0;

        D3D12_ROOT_PARAMETER root_param = {};
        root_param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        root_param.DescriptorTable.NumDescriptorRanges = 1;
        root_param.DescriptorTable.pDescriptorRanges = &srv_range;
        root_param.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        D3D12_STATIC_SAMPLER_DESC sampler = {};
        sampler.Filter = OPTION(kDispBilinear) ? D3D12_FILTER_MIN_MAG_MIP_LINEAR
            : D3D12_FILTER_MIN_MAG_MIP_POINT;
        sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampler.ShaderRegister = 0;
        sampler.RegisterSpace = 0;
        sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        D3D12_ROOT_SIGNATURE_DESC rs_desc = {};
        rs_desc.NumParameters = 1;
        rs_desc.pParameters = &root_param;
        rs_desc.NumStaticSamplers = 1;
        rs_desc.pStaticSamplers = &sampler;
        rs_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        ComPtr<ID3DBlob> signature_blob, error_blob;
        hr = D3D12SERIALIZEROOTSIGNATURE(&rs_desc, D3D_ROOT_SIGNATURE_VERSION_1,
            &signature_blob, &error_blob);
        if (FAILED(hr)) {
            wxLogError(_("Failed to serialize root signature: %s"),
                error_blob ? wxString(static_cast<char*>(error_blob->GetBufferPointer())) : wxT("unknown"));
            return;
        }
        hr = device->CreateRootSignature(0,
            signature_blob->GetBufferPointer(), signature_blob->GetBufferSize(),
            IID_PPV_ARGS(&root_signature));
        if (FAILED(hr)) { wxLogError(_("Failed to create root signature: 0x%08X"), hr); return; }
    }

    // --- 10. Compile Shaders & Build PSO ---
    {
        ComPtr<ID3DBlob> vs_blob, ps_blob, error_blob;
        UINT compile_flags = 0;
#if defined(_DEBUG)
        compile_flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
        hr = D3DCOMPILE(g_VertexShaderSrc, strlen(g_VertexShaderSrc),
            nullptr, nullptr, nullptr, "main", "vs_5_0",
            compile_flags, 0, &vs_blob, &error_blob);
        if (FAILED(hr)) {
            wxLogError(_("VS compile failed: %s"),
                error_blob ? wxString(static_cast<char*>(error_blob->GetBufferPointer())) : wxT("unknown"));
            return;
        }
        hr = D3DCOMPILE(g_PixelShaderSrc, strlen(g_PixelShaderSrc),
            nullptr, nullptr, nullptr, "main", "ps_5_0",
            compile_flags, 0, &ps_blob, &error_blob);
        if (FAILED(hr)) {
            wxLogError(_("PS compile failed: %s"),
                error_blob ? wxString(static_cast<char*>(error_blob->GetBufferPointer())) : wxT("unknown"));
            return;
        }

        D3D12_INPUT_ELEMENT_DESC input_layout[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        };

        D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {};
        pso_desc.pRootSignature = root_signature.Get();
        pso_desc.VS = { vs_blob->GetBufferPointer(), vs_blob->GetBufferSize() };
        pso_desc.PS = { ps_blob->GetBufferPointer(), ps_blob->GetBufferSize() };
        pso_desc.InputLayout = { input_layout, _countof(input_layout) };
        pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        pso_desc.NumRenderTargets = 1;
        pso_desc.RTVFormats[0] = rt_format_;
        pso_desc.SampleDesc.Count = 1;
        // Default blend / rasterizer / depth-stencil are fine for a simple 2D blit
        pso_desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        pso_desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        pso_desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        pso_desc.RasterizerState.DepthClipEnable = TRUE;
        pso_desc.DepthStencilState.DepthEnable = FALSE;
        pso_desc.DepthStencilState.StencilEnable = FALSE;
        pso_desc.SampleMask = UINT_MAX;

        hr = device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pipeline_state));
        if (FAILED(hr)) { wxLogError(_("Failed to create PSO: 0x%08X"), hr); return; }
    }

    // --- 11. Vertex Buffer (screen-space quad, updated each frame) ---
    {
        // 4 vertices x (float4 pos + float2 uv) = 4 x 24 bytes
        const UINT vb_size = 4 * sizeof(float) * 6;
        D3D12_HEAP_PROPERTIES heap_props = {};
        heap_props.Type = D3D12_HEAP_TYPE_UPLOAD;
        D3D12_RESOURCE_DESC res_desc = {};
        res_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        res_desc.Width = vb_size;
        res_desc.Height = 1;
        res_desc.DepthOrArraySize = 1;
        res_desc.MipLevels = 1;
        res_desc.SampleDesc.Count = 1;
        res_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        hr = device->CreateCommittedResource(&heap_props, D3D12_HEAP_FLAG_NONE,
            &res_desc, D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr, IID_PPV_ARGS(&vertex_buffer));
        if (FAILED(hr)) { wxLogError(_("Failed to create vertex buffer: 0x%08X"), hr); return; }

        vertex_buffer_view.BufferLocation = vertex_buffer->GetGPUVirtualAddress();
        vertex_buffer_view.StrideInBytes = sizeof(float) * 6; // xyzw + uv
        vertex_buffer_view.SizeInBytes = vb_size;
    }

    // --- 12. Fence for CPU/GPU synchronization ---
    hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
    if (FAILED(hr)) { wxLogError(_("Failed to create fence: 0x%08X"), hr); return; }
    fence_value = 1;
    fence_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!fence_event) { wxLogError(_("Failed to create fence event")); return; }

    wxLogDebug(_("D3D12 pipeline fully initialized"));
}

DX12DrawingPanel::~DX12DrawingPanel()
{
    WaitForGPU(); // Drain the GPU before releasing any resources

    if (fence_event) { CloseHandle(fence_event); fence_event = NULL; }
    // All ComPtr members release automatically
}

// Helper: signal fence and wait for the GPU to reach it
void DX12DrawingPanel::WaitForGPU()
{
    if (!command_queue || !fence || !fence_event) return;
    command_queue->Signal(fence.Get(), fence_value);
    fence->SetEventOnCompletion(fence_value, fence_event);
    WaitForSingleObject(fence_event, INFINITE);
    ++fence_value;
}

void DX12DrawingPanel::DrawingPanelInit()
{
    DrawingPanelBase::DrawingPanelInit();

    // No D3D12 device -> setup (done in the constructor) failed; flag it so
    // GameArea falls back to the next renderer in the priority list.
    if (!device) { init_failed_ = true; return; }

    texture_width = (int)std::ceil(width * scale);
    texture_height = (int)std::ceil(height * scale);

    wxLogDebug(_("DX12DrawingPanel initialized: %dx%d (scale: %f)"),
        texture_width, texture_height, scale);
}

bool DX12DrawingPanel::ResizeSwapChain()
{
    if (!swap_chain || !device) return false;

    WaitForGPU();

    // Release back-buffer references before resize
    for (UINT i = 0; i < FRAME_COUNT; i++)
        render_targets[i].Reset();

    wxSize client_size = GetClientSize();
    UINT flags = OPTION(kPrefVsync) ? 0 : DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

    HRESULT hr = swap_chain->ResizeBuffers(FRAME_COUNT,
        (UINT)client_size.GetWidth(),
        (UINT)client_size.GetHeight(),
        rt_format_,
        flags);
    if (FAILED(hr)) { wxLogError(_("ResizeBuffers failed: 0x%08X"), hr); return false; }

    // Rebuild RTVs
    D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle = rtv_heap->GetCPUDescriptorHandleForHeapStart();
    for (UINT i = 0; i < FRAME_COUNT; i++) {
        swap_chain->GetBuffer(i, IID_PPV_ARGS(&render_targets[i]));
        device->CreateRenderTargetView(render_targets[i].Get(), nullptr, rtv_handle);
        rtv_handle.ptr += rtv_descriptor_size;
    }

    frame_index = swap_chain->GetCurrentBackBufferIndex();
    wxLogDebug(_("Swap chain resized to %dx%d"), client_size.GetWidth(), client_size.GetHeight());
    return true;
}

void DX12DrawingPanel::OnSize(wxSizeEvent& ev)
{
    if (device) ResizeSwapChain();
    ev.Skip();
}

// Helper: upload pixel data to a D3D12 texture via an upload heap
static ComPtr<ID3D12Resource> UploadTexture(
    ID3D12Device* device,
    ID3D12GraphicsCommandList* cmd_list,
    ComPtr<ID3D12Resource>& upload_heap, // kept alive until GPU finishes
    DXGI_FORMAT                format,
    UINT                       tex_w,
    UINT                       tex_h,
    const void* src_pixels,
    UINT                       src_row_bytes)
{
    // --- Destination texture (DEFAULT heap) ---
    D3D12_HEAP_PROPERTIES default_heap = {};
    default_heap.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC tex_desc = {};
    tex_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    tex_desc.Width = tex_w;
    tex_desc.Height = tex_h;
    tex_desc.DepthOrArraySize = 1;
    tex_desc.MipLevels = 1;
    tex_desc.Format = format;
    tex_desc.SampleDesc.Count = 1;
    tex_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

    ComPtr<ID3D12Resource> tex;
    HRESULT hr = device->CreateCommittedResource(
        &default_heap, D3D12_HEAP_FLAG_NONE,
        &tex_desc, D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr, IID_PPV_ARGS(&tex));
    if (FAILED(hr)) return nullptr;

    // --- Upload heap sized for this subresource ---
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;
    UINT num_rows;
    UINT64 row_size_bytes, total_bytes;
    device->GetCopyableFootprints(&tex_desc, 0, 1, 0,
        &footprint, &num_rows, &row_size_bytes, &total_bytes);

    D3D12_HEAP_PROPERTIES upload_heap_props = {};
    upload_heap_props.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC upload_desc = {};
    upload_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    upload_desc.Width = total_bytes;
    upload_desc.Height = 1;
    upload_desc.DepthOrArraySize = 1;
    upload_desc.MipLevels = 1;
    upload_desc.SampleDesc.Count = 1;
    upload_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    hr = device->CreateCommittedResource(
        &upload_heap_props, D3D12_HEAP_FLAG_NONE,
        &upload_desc, D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr, IID_PPV_ARGS(&upload_heap));
    if (FAILED(hr)) return nullptr;

    // --- Map upload heap and copy row by row ---
    uint8_t* mapped = nullptr;
    upload_heap->Map(0, nullptr, reinterpret_cast<void**>(&mapped));
    for (UINT row = 0; row < (UINT)tex_h; ++row) {
        memcpy(mapped + row * footprint.Footprint.RowPitch,
            static_cast<const uint8_t*>(src_pixels) + row * src_row_bytes,
            src_row_bytes);
    }
    upload_heap->Unmap(0, nullptr);

    // --- Copy command ---
    D3D12_TEXTURE_COPY_LOCATION src_loc = {};
    src_loc.pResource = upload_heap.Get();
    src_loc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    src_loc.PlacedFootprint = footprint;

    D3D12_TEXTURE_COPY_LOCATION dst_loc = {};
    dst_loc.pResource = tex.Get();
    dst_loc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dst_loc.SubresourceIndex = 0;

    cmd_list->CopyTextureRegion(&dst_loc, 0, 0, 0, &src_loc, nullptr);

    // Transition to shader-readable state
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = tex.Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    cmd_list->ResourceBarrier(1, &barrier);

    return tex;
}

void DX12DrawingPanel::DrawArea(wxWindowDC& dc)
{
    (void)dc;

    if (!device || !command_queue || !swap_chain) return;
    if (!did_init) DrawingPanelInit();

    // --- Reset command allocator / list for this frame ---
    command_allocator->Reset();
    command_list->Reset(command_allocator.Get(), pipeline_state.Get());

    // --- Transition back buffer: PRESENT -> RENDER TARGET ---
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = render_targets[frame_index].Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    command_list->ResourceBarrier(1, &barrier);

    // --- RTV handle for current back buffer ---
    D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle = rtv_heap->GetCPUDescriptorHandleForHeapStart();
    rtv_handle.ptr += (SIZE_T)frame_index * rtv_descriptor_size;

    const float clear_color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    command_list->ClearRenderTargetView(rtv_handle, clear_color, 0, nullptr);
    command_list->OMSetRenderTargets(1, &rtv_handle, FALSE, nullptr);

    if (!todraw) {
        // No frame data - just clear and present
        goto present;
    }

    // =====================================================================
    //  Pixel conversion to a 32-bit RGBA staging buffer
    //  (mirrors the D3D9 per-format pixel copy, but always targets RGBA8)
    // =====================================================================
    {
        int scaled_width = (int)std::ceil(width * scale);
        int scaled_height = (int)std::ceil(height * scale);

        // Allocate a staging buffer (RGBA8, 4 bytes/pixel)
        std::vector<uint8_t> rgba(scaled_width * scaled_height * 4);
        uint8_t* dst_pixels = rgba.data();

        // Border sizes (same logic as original)
        int inrb = out_8 ? 4 : out_16 ? 2 : out_24 ? 0 : 1;
        int scaled_border = (int)std::ceil(inrb * scale);

        int src_pitch;
        if (out_8)  src_pitch = (scaled_width + scaled_border);
        else if (out_16) src_pitch = (scaled_width + scaled_border) * 2;
        else if (out_24) src_pitch = scaled_width * 3;
        else             src_pitch = (scaled_width + scaled_border) * 4;

        uint8_t* src = todraw;

        // Texture format for this frame; HDR overrides it to a 10-bit PQ format.
        DXGI_FORMAT tex_fmt = DXGI_FORMAT_B8G8R8A8_UNORM;

        // HDR: encode the corrected RGBA8 image to 10-bit BT.2020 PQ matching
        // the HDR10 swapchain. Only the 32-bit source path feeds the encoder.
        const bool hdr = HdrActive() && hdr_swapchain_ &&
                         !out_8 && !out_16 && !out_24;
        if (hdr) {
            const uint8_t* tex_ptr = src + src_pitch;  // skip top border row
            hdr::EncodePQ10(tex_ptr, src_pitch, scaled_width, scaled_height,
                            reinterpret_cast<uint32_t*>(dst_pixels),
                            scaled_width * 4);
            tex_fmt = DXGI_FORMAT_R10G10B10A2_UNORM;
        }
        else if (out_8) {
            src += src_pitch; // skip top border row
            for (int y = 0; y < scaled_height; y++) {
                uint8_t* sr = src;
                uint8_t* dr = dst_pixels + y * scaled_width * 4;
                for (int x = 0; x < scaled_width; x++, sr++) {
                    uint8_t p = *sr;
                    if (p == 0xff) {
                        dr[0] = dr[1] = dr[2] = 0xff;
                    } else {
                        dr[0] = (p & 0x3) << 6;        // R <- B
                        dr[1] = ((p >> 2) & 0x7) << 5; // G
                        dr[2] = ((p >> 5) & 0x7) << 5; // B <- R
                    }
                    dr[3] = 0xff;
                    dr += 4;
                }
                src += src_pitch;
            }
        }
        else if (out_16) {
            // Skip top border and cast to 16-bit source data (typically RGB555)
            uint16_t* src16 = (uint16_t*)src + src_pitch / 2;

            for (int y = 0; y < scaled_height; y++) {
                uint16_t* sr = src16;
                uint8_t* dr = dst_pixels + y * scaled_width * 4;

                for (int x = 0; x < scaled_width; x++, sr++) {
                    uint16_t p = *sr;

                    // Handle transparency/white key: map 0x7FFF (High-bit 0, RGB 31,31,31) to solid white
                    if ((p & 0x7fff) == 0x7fff) {
                        dr[0] = dr[1] = dr[2] = 0xff;
                    }
                    else {
                        // Hardcoded 5-5-5 shifts (10/5/0) to avoid race conditions with 
                        // global 'systemRedShift' overrides during filter transitions.
                        uint8_t r5 = (p >> 10) & 0x1f;
                        uint8_t g5 = (p >> 5) & 0x1f;
                        uint8_t b5 = p & 0x1f;

                        // Expand 5-bit to 8-bit (XRGB/BGRA layout) via bit-duplication
                        // (e.g., 11111 -> 11111111) for full color range intensity.
                        // dr[0]=Blue, dr[1]=Green, dr[2]=Red
                        dr[0] = (b5 << 3) | (b5 >> 2);
                        dr[1] = (g5 << 3) | (g5 >> 2);
                        dr[2] = (r5 << 3) | (r5 >> 2);
                    }
                    dr[3] = 0xff; // Set Alpha channel to fully opaque
                    dr += 4;
                }
                src16 += src_pitch / 2; // Advance source pointer by pitch (adjusted for uint16_t)
            }
        }
        else if (out_24) {
            src += scaled_width * 3; // skip top border row
            for (int y = 0; y < scaled_height; y++) {
                uint8_t* sr = src;
                uint8_t* dr = dst_pixels + y * scaled_width * 4;
                for (int x = 0; x < scaled_width; x++) {
                    dr[0] = sr[2]; // R <- B
                    dr[1] = sr[1]; // G
                    dr[2] = sr[0]; // B <- R
                    dr[3] = 0xff;
                    sr += 3; dr += 4;
                }
                src += src_pitch;
            }
        }
        else {
            // 32-bit BGRA -> RGBA
            src += src_pitch; // skip top border row
            for (int y = 0; y < scaled_height; y++) {
                uint8_t* sr = src;
                uint8_t* dr = dst_pixels + y * scaled_width * 4;
                for (int x = 0; x < scaled_width; x++) {
                    dr[0] = sr[2]; // R <- B
                    dr[1] = sr[1]; // G
                    dr[2] = sr[0]; // B <- R
                    dr[3] = sr[3]; // A
                    sr += 4; dr += 4;
                }
                src += src_pitch;
            }
        }

        // Upload texture (upload_heap kept alive via member until WaitForGPU)
        texture = UploadTexture(device.Get(), command_list.Get(),
            upload_heap,
            tex_fmt,
            (UINT)scaled_width, (UINT)scaled_height,
            dst_pixels, (UINT)scaled_width * 4);
        if (!texture) { wxLogError(_("Texture upload failed")); goto present; }

        // Create / refresh SRV
        D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
        srv_desc.Format = tex_fmt;
        srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srv_desc.Texture2D.MipLevels = 1;
        device->CreateShaderResourceView(texture.Get(), &srv_desc,
            srv_heap->GetCPUDescriptorHandleForHeapStart());

        // ---- Set pipeline state & root signature ----
        command_list->SetGraphicsRootSignature(root_signature.Get());
        command_list->SetPipelineState(pipeline_state.Get());

        ID3D12DescriptorHeap* heaps[] = { srv_heap.Get() };
        command_list->SetDescriptorHeaps(1, heaps);
        command_list->SetGraphicsRootDescriptorTable(0,
            srv_heap->GetGPUDescriptorHandleForHeapStart());

        // ---- Viewport & scissor ----
        wxSize client_size = GetClientSize();
        float win_w = (float)client_size.GetWidth();
        float win_h = (float)client_size.GetHeight();

        D3D12_VIEWPORT vp = { 0.0f, 0.0f, win_w, win_h, 0.0f, 1.0f };
        D3D12_RECT     scissor = { 0, 0, (LONG)win_w, (LONG)win_h };
        command_list->RSSetViewports(1, &vp);
        command_list->RSSetScissorRects(1, &scissor);

        // ---- Calculate letterbox / stretch geometry ----
        float render_w = win_w, render_h = win_h;
        float off_x = 0.0f, off_y = 0.0f;

        if (OPTION(kDispStretch)) {
            float tex_aspect = (float)scaled_width / (float)scaled_height;
            float win_aspect = win_w / win_h;
            if (win_aspect > tex_aspect) {
                render_h = win_h;
                render_w = win_h * tex_aspect;
                off_x = (win_w - render_w) / 2.0f;
            }
            else {
                render_w = win_w;
                render_h = win_w / tex_aspect;
                off_y = (win_h - render_h) / 2.0f;
            }
        }

        // Convert pixel coords to NDC [-1, +1]
        auto toNDC_X = [&](float px) { return  (px / win_w) * 2.0f - 1.0f; };
        auto toNDC_Y = [&](float py) { return -((py / win_h) * 2.0f - 1.0f); };

        float x0 = toNDC_X(off_x), y0 = toNDC_Y(off_y);
        float x1 = toNDC_X(off_x + render_w), y1 = toNDC_Y(off_y + render_h);

        // xyzw (w=1), uv  - two triangles as a strip
        struct Vertex { float x, y, z, w, u, v; };
        Vertex verts[4] = {
            { x0, y0, 0.0f, 1.0f,  0.0f, 0.0f },  // TL
            { x1, y0, 0.0f, 1.0f,  1.0f, 0.0f },  // TR
            { x0, y1, 0.0f, 1.0f,  0.0f, 1.0f },  // BL
            { x1, y1, 0.0f, 1.0f,  1.0f, 1.0f },  // BR
        };

        // Copy vertices to upload VB
        void* vb_mapped = nullptr;
        vertex_buffer->Map(0, nullptr, &vb_mapped);
        memcpy(vb_mapped, verts, sizeof(verts));
        vertex_buffer->Unmap(0, nullptr);

        command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        command_list->IASetVertexBuffers(0, 1, &vertex_buffer_view);
        command_list->DrawInstanced(4, 1, 0, 0);
    }

present:
    // Transition back buffer: RENDER TARGET -> PRESENT
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    command_list->ResourceBarrier(1, &barrier);

    command_list->Close();

    ID3D12CommandList* lists[] = { command_list.Get() };
    command_queue->ExecuteCommandLists(1, lists);

    UINT present_flags = (!OPTION(kPrefVsync)) ? DXGI_PRESENT_ALLOW_TEARING : 0;
    swap_chain->Present(OPTION(kPrefVsync) ? 1 : 0, present_flags);

    WaitForGPU(); // Simple per-frame sync; could be optimised with double-buffered fences
    frame_index = swap_chain->GetCurrentBackBufferIndex();
}
#endif

#if defined(__WXMSW__) && !defined(NO_D3D11)
// ============================================================================
// DX11DrawingPanel -- software-style HDR sub-driver backing Simple on Windows.
// A minimal flip-model DXGI swapchain fed by a CPU-filled STAGING texture
// (CopyResource'd into the back buffer) -- no 3D pipeline. HDR uses a 10-bit
// R10G10B10A2 back buffer tagged BT.2020 PQ (mirroring D3D12); SDR uses 8-bit
// B8G8R8A8. Device: hardware, then WARP; else the base wx software path.
// ============================================================================

DX11DrawingPanel::DX11DrawingPanel(wxWindow* parent, int _width, int _height)
    : BasicDrawingPanel(parent, _width, _height)
{
    if (CreateDeviceAndSwapchain())
        InitPipeline();  // shaders/quad/sampler; on failure -> wx software path
    // Configure the HDR encoder and latch hdr_encoding_ from SupportsHdr().
    UpdateHdrState();
    // Mark initialized so GameArea's EvaluateHdrRenderer runs its "working"
    // check (and falls back to another HDR renderer if HDR didn't come up).
    did_init = true;
}

DX11DrawingPanel::~DX11DrawingPanel()
{
    Teardown();
}

bool DX11DrawingPanel::CreateDeviceAndSwapchain()
{
    // Load d3d11.dll dynamically (matching the D3D12 path) so there is no static
    // import dependency.
    HMODULE d3d11 = LoadLibrary(TEXT("d3d11.dll"));
    if (!d3d11)
        return false;
    auto create = reinterpret_cast<PFN_D3D11_CREATE_DEVICE>(
        reinterpret_cast<void*>(GetProcAddress(d3d11, "D3D11CreateDevice")));
    if (!create) {
        FreeLibrary(d3d11);
        return false;
    }

    const D3D_FEATURE_LEVEL want[] = {
        D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0,
    };
    const UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

    // Hardware first, then WARP (the CPU rasterizer -- the "software" tier).
    HRESULT hr = E_FAIL;
    for (D3D_DRIVER_TYPE dt : {D3D_DRIVER_TYPE_HARDWARE, D3D_DRIVER_TYPE_WARP}) {
        hr = create(nullptr, dt, nullptr, flags, want,
                    (UINT)(sizeof(want) / sizeof(want[0])), D3D11_SDK_VERSION,
                    device_.GetAddressOf(), nullptr, context_.GetAddressOf());
        if (SUCCEEDED(hr))
            break;
        device_.Reset();
        context_.Reset();
    }
    if (FAILED(hr) || !device_) {
        FreeLibrary(d3d11);
        wxLogError(_("D3D11: failed to create device; Simple renderer will use "
                     "the software (wx) path."));
        return false;
    }
    // d3d11.dll intentionally left loaded for the device's lifetime (process
    // lifetime), like the wx app itself; do not FreeLibrary while device_ lives.
    wxLogDebug(wxT("D3D11 device created."));

    // Get the DXGI factory from the device (no separate dxgi load needed).
    Microsoft::WRL::ComPtr<IDXGIDevice> dxgi_dev;
    if (FAILED(device_.As(&dxgi_dev))) return false;
    Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
    if (FAILED(dxgi_dev->GetAdapter(&adapter))) return false;
    Microsoft::WRL::ComPtr<IDXGIFactory2> factory;
    if (FAILED(adapter->GetParent(IID_PPV_ARGS(&factory)))) return false;

    // Tearing (uncapped present) only when vsync is off and the OS supports it.
    allow_tearing_ = false;
    if (!OPTION(kPrefVsync)) {
        Microsoft::WRL::ComPtr<IDXGIFactory5> f5;
        if (SUCCEEDED(factory.As(&f5))) {
            BOOL at = FALSE;
            if (SUCCEEDED(f5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING,
                                                  &at, sizeof(at))))
                allow_tearing_ = (at == TRUE);
        }
    }

    // 10-bit back buffer for HDR (tagged PQ below), 8-bit for SDR.
    rt_format_ = (hdr::HdrAvailable() && OPTION(kDispHDR))
                     ? DXGI_FORMAT_R10G10B10A2_UNORM
                     : DXGI_FORMAT_B8G8R8A8_UNORM;

    wxSize sz = GetClientSize();
    int W = std::max(1, sz.GetWidth());
    int H = std::max(1, sz.GetHeight());

    DXGI_SWAP_CHAIN_DESC1 desc = {};
    desc.Width       = (UINT)W;
    desc.Height      = (UINT)H;
    desc.Format      = rt_format_;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 2;
    desc.SwapEffect  = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    desc.SampleDesc.Count = 1;
    desc.AlphaMode   = DXGI_ALPHA_MODE_IGNORE;
    desc.Flags       = allow_tearing_ ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

    Microsoft::WRL::ComPtr<IDXGISwapChain1> sc1;
    hr = factory->CreateSwapChainForHwnd(device_.Get(), (HWND)GetHandle(),
                                         &desc, nullptr, nullptr, &sc1);
    if (FAILED(hr)) {
        wxLogError(_("D3D11: failed to create swap chain: 0x%08X"), hr);
        device_.Reset(); context_.Reset();
        return false;
    }
    factory->MakeWindowAssociation((HWND)GetHandle(), DXGI_MWA_NO_ALT_ENTER);
    if (FAILED(sc1.As(&swap_chain_))) {
        device_.Reset(); context_.Reset();
        return false;
    }
    sc_w_ = W; sc_h_ = H;

    // HDR10 BT.2020 PQ: the display is in HDR10 mode (hdr::HdrAvailable() already
    // confirmed it via a real renderer at startup) and rt_format_ is the 10-bit
    // PQ-capable back buffer, so treat this panel as HDR. Do NOT make that
    // conditional on SetColorSpace1()/CheckColorSpaceSupport() succeeding right
    // now: at the very first panel creation -- before the window is composited on
    // the HDR output -- those can fail transiently even though HDR presents fine
    // once the window is up, and gating on them made GameArea see this HDR-capable
    // renderer as SDR and bounce the user off Simple to Direct3D 12 on every
    // launch. Apply the color space best-effort here and, if it has not taken yet,
    // keep retrying it per-frame in PresentFrame (ApplyHdrColorSpace).
    if (hdr::HdrAvailable() && OPTION(kDispHDR)) {
        hdr_swapchain_ = true;
        ApplyHdrColorSpace();
    }

    // The render-target view is created lazily by ResizeSwapchain() on the first
    // PresentFrame, once the window is realized at its real client size. It is NOT
    // built here: at startup this constructor runs before the panel is sized, and
    // a transient RTV-creation failure here used to clear hdr_swapchain_ and fail
    // the panel -- making GameArea see Simple as non-HDR and bounce it to Direct3D
    // 12. The device and swapchain are what matter for "did the renderer come up".
    return true;
}

// Set the swapchain's BT.2020 PQ color space (+ HDR10 metadata). This can fail
// at first call before the window is composited on the HDR output, so it is
// retried per-frame from PresentFrame until it takes; once applied it is a no-op.
void DX11DrawingPanel::ApplyHdrColorSpace()
{
    if (colorspace_applied_ || !hdr_swapchain_ || !swap_chain_)
        return;
    if (FAILED(swap_chain_->SetColorSpace1(DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020)))
        return;  // not ready yet -- try again next frame
    colorspace_applied_ = true;
    Microsoft::WRL::ComPtr<IDXGISwapChain4> sc4;
    if (SUCCEEDED(swap_chain_.As(&sc4))) {
        DXGI_HDR_METADATA_HDR10 md = {};
        md.MaxMasteringLuminance     = (UINT)(OPTION(kDispHDRPeakBrightness) * 10000u);
        md.MinMasteringLuminance     = 0;
        md.MaxContentLightLevel      = (UINT16)OPTION(kDispHDRPeakBrightness);
        md.MaxFrameAverageLightLevel = (UINT16)OPTION(kDispHDRReferenceWhite);
        sc4->SetHDRMetaData(DXGI_HDR_METADATA_TYPE_HDR10, sizeof(md), &md);
    }
}

// Compile the passthrough blit shaders, build the full-screen quad and the
// sampler. Returns false (leaving pipeline_ready_ false) if anything fails, in
// which case PresentFrame bails and DrawImage uses the wx software path.
bool DX11DrawingPanel::InitPipeline()
{
    if (!device_)
        return false;

    HMODULE hCompiler = LoadLibrary(TEXT("d3dcompiler_47.dll"));
    if (!hCompiler) {
        wxLogError(_("D3D11: d3dcompiler_47.dll not found; Simple renderer will "
                     "use the software (wx) path."));
        return false;
    }
    auto D3DCompileFn = reinterpret_cast<LPFND3DCompile>(
        reinterpret_cast<void*>(GetProcAddress(hCompiler, "D3DCompile")));
    if (!D3DCompileFn) {
        FreeLibrary(hCompiler);
        return false;
    }

    bool shaders_ok = false;
    {
        // Shader-compiler COM objects (ID3DBlob) live in d3dcompiler_47.dll, so
        // they must be released before FreeLibrary -- keep them in this scope.
        Microsoft::WRL::ComPtr<ID3DBlob> vsb, psb, err;
        UINT cflags = 0;
#if defined(_DEBUG)
        cflags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
        // vs_4_0/ps_4_0: trivial passthrough, valid on feature level 10.0+ (so it
        // also works on the WARP/old-hardware fallback tiers).
        HRESULT hr = D3DCompileFn(g_VertexShaderSrc, strlen(g_VertexShaderSrc),
                                  nullptr, nullptr, nullptr, "main", "vs_4_0",
                                  cflags, 0, &vsb, &err);
        if (SUCCEEDED(hr))
            hr = device_->CreateVertexShader(vsb->GetBufferPointer(),
                                             vsb->GetBufferSize(), nullptr,
                                             vs_.GetAddressOf());
        if (SUCCEEDED(hr)) {
            const D3D11_INPUT_ELEMENT_DESC il[] = {
                { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,  0,
                  D3D11_INPUT_PER_VERTEX_DATA, 0 },
                { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 16,
                  D3D11_INPUT_PER_VERTEX_DATA, 0 },
            };
            hr = device_->CreateInputLayout(il, 2, vsb->GetBufferPointer(),
                                            vsb->GetBufferSize(),
                                            input_layout_.GetAddressOf());
        }
        if (SUCCEEDED(hr)) {
            err.Reset();
            hr = D3DCompileFn(g_PixelShaderSrc, strlen(g_PixelShaderSrc),
                              nullptr, nullptr, nullptr, "main", "ps_4_0",
                              cflags, 0, &psb, &err);
        }
        if (SUCCEEDED(hr))
            hr = device_->CreatePixelShader(psb->GetBufferPointer(),
                                            psb->GetBufferSize(), nullptr,
                                            ps_.GetAddressOf());
        shaders_ok = SUCCEEDED(hr);
        if (!shaders_ok)
            wxLogError(_("D3D11: blit shader setup failed; Simple renderer will "
                         "use the software (wx) path."));
    }
    FreeLibrary(hCompiler);
    if (!shaders_ok)
        return false;

    // Full-screen quad as a triangle strip: float4 position (clip space) + float2
    // texcoord. Texture v grows downward, clip-space y grows upward, so the top
    // edge (y=+1) maps to v=0.
    const float verts[] = {
        -1.f,  1.f, 0.f, 1.f,   0.f, 0.f,
        -1.f, -1.f, 0.f, 1.f,   0.f, 1.f,
         1.f,  1.f, 0.f, 1.f,   1.f, 0.f,
         1.f, -1.f, 0.f, 1.f,   1.f, 1.f,
    };
    D3D11_BUFFER_DESC bd = {};
    bd.ByteWidth = sizeof(verts);
    bd.Usage     = D3D11_USAGE_IMMUTABLE;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA srd = {};
    srd.pSysMem = verts;
    if (FAILED(device_->CreateBuffer(&bd, &srd, vertex_buffer_.GetAddressOf())))
        return false;

    // Point sampling by default (pixel-exact, matching the old nearest blit);
    // linear when the user enables bilinear, like the D3D12 path.
    D3D11_SAMPLER_DESC sd = {};
    sd.Filter = OPTION(kDispBilinear) ? D3D11_FILTER_MIN_MAG_MIP_LINEAR
                                      : D3D11_FILTER_MIN_MAG_MIP_POINT;
    sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sd.MaxLOD = D3D11_FLOAT32_MAX;
    if (FAILED(device_->CreateSamplerState(&sd, sampler_.GetAddressOf())))
        return false;

    // Solid fill, no culling -- the full-screen quad must draw regardless of its
    // winding (the D3D12 path likewise uses CULL_NONE). Without this the default
    // back-face cull can drop the quad and present a black frame.
    D3D11_RASTERIZER_DESC rd = {};
    rd.FillMode = D3D11_FILL_SOLID;
    rd.CullMode = D3D11_CULL_NONE;
    rd.DepthClipEnable = TRUE;
    if (FAILED(device_->CreateRasterizerState(&rd, rasterizer_.GetAddressOf())))
        return false;

    pipeline_ready_ = true;
    return true;
}

bool DX11DrawingPanel::ResizeSwapchain(int W, int H)
{
    if (!swap_chain_) return false;
    if (W < 1) W = 1;
    if (H < 1) H = 1;
    if (W == sc_w_ && H == sc_h_ && rtv_) return true;
    // The RTV holds a reference to the back buffer; it must be released before
    // ResizeBuffers and rebuilt afterward.
    rtv_.Reset();
    const UINT flags = allow_tearing_ ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
    HRESULT hr = swap_chain_->ResizeBuffers(0, (UINT)W, (UINT)H,
                                            DXGI_FORMAT_UNKNOWN, flags);
    if (FAILED(hr)) {
        wxLogDebug(wxT("D3D11: ResizeBuffers failed: 0x%08X"), hr);
        return false;
    }
    Microsoft::WRL::ComPtr<ID3D11Texture2D> back;
    if (FAILED(swap_chain_->GetBuffer(0, IID_PPV_ARGS(&back))) || !back)
        return false;
    if (FAILED(device_->CreateRenderTargetView(back.Get(), nullptr,
                                               rtv_.GetAddressOf())))
        return false;
    sc_w_ = W; sc_h_ = H;
    // ResizeBuffers can drop the swapchain color space; re-apply it next frame.
    colorspace_applied_ = false;
    return true;
}

// (Re)create the DYNAMIC source texture (and its SRV) the corrected frame is
// uploaded into at its own resolution; the GPU then scales it to the window.
bool DX11DrawingPanel::EnsureSourceTexture(int W, int H, DXGI_FORMAT fmt)
{
    if (src_tex_ && W == src_w_ && H == src_h_ && fmt == src_fmt_)
        return true;
    src_srv_.Reset();
    src_tex_.Reset();
    if (W < 1 || H < 1) return false;

    D3D11_TEXTURE2D_DESC td = {};
    td.Width            = (UINT)W;
    td.Height           = (UINT)H;
    td.MipLevels        = 1;
    td.ArraySize        = 1;
    td.Format           = fmt;
    td.SampleDesc.Count = 1;
    td.Usage            = D3D11_USAGE_DYNAMIC;
    td.BindFlags        = D3D11_BIND_SHADER_RESOURCE;
    td.CPUAccessFlags   = D3D11_CPU_ACCESS_WRITE;
    if (FAILED(device_->CreateTexture2D(&td, nullptr, src_tex_.GetAddressOf()))) {
        src_tex_.Reset();
        return false;
    }
    if (FAILED(device_->CreateShaderResourceView(src_tex_.Get(), nullptr,
                                                 src_srv_.GetAddressOf()))) {
        src_srv_.Reset();
        src_tex_.Reset();
        return false;
    }
    src_w_ = W; src_h_ = H; src_fmt_ = fmt;
    return true;
}

bool DX11DrawingPanel::PresentFrame(wxImage* im)
{
    if (!swap_chain_ || !device_ || !context_ || !pipeline_ready_)
        return false;

    int W = 1, H = 1;
    GetClientSize(&W, &H);
    if (W < 1) W = 1;
    if (H < 1) H = 1;

    if (!ResizeSwapchain(W, H)) return false;

    const bool hdr =
        hdr_swapchain_ && HdrActive() && panel_color_depth_ == 32 && todraw;

    // Ensure the BT.2020 PQ color space is set now that the window is up (no-op
    // once it has taken); it can fail at construction before the window is shown.
    if (hdr) ApplyHdrColorSpace();

    // Upload the corrected frame at its own (source) resolution -- not the window
    // size -- into the DYNAMIC texture; the GPU upscales it when sampling. So the
    // CPU only ever touches source-sized pixels and there is no back-buffer copy.
    if (hdr) {
        // 32-bit RGBA8 todraw has a 1px filter border; skip the first row and use
        // the bordered row length as the source stride. Encode straight to PQ10
        // (A2B10G10R10 == DXGI_FORMAT_R10G10B10A2_UNORM).
        const int sw     = (int)std::ceil(width * scale);
        const int sh     = (int)std::ceil(height * scale);
        const int rowlen = (int)std::ceil((width + 1) * scale);
        if (sw < 1 || sh < 1) return false;
        if (!EnsureSourceTexture(sw, sh, DXGI_FORMAT_R10G10B10A2_UNORM))
            return false;
        D3D11_MAPPED_SUBRESOURCE mapped = {};
        if (FAILED(context_->Map(src_tex_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0,
                                 &mapped)))
            return false;
        hdr::EncodePQ10(todraw + (size_t)rowlen * 4, rowlen * 4, sw, sh,
                        reinterpret_cast<uint32_t*>(mapped.pData),
                        (int)mapped.RowPitch);
        context_->Unmap(src_tex_.Get(), 0);
    } else if (im) {
        // SDR: convert the 24-bit RGB image to BGRA8 (B8G8R8A8_UNORM) at its own
        // size, honoring the mapped row pitch.
        const int sw = im->GetWidth();
        const int sh = im->GetHeight();
        if (sw < 1 || sh < 1) return false;
        if (!EnsureSourceTexture(sw, sh, DXGI_FORMAT_B8G8R8A8_UNORM))
            return false;
        D3D11_MAPPED_SUBRESOURCE mapped = {};
        if (FAILED(context_->Map(src_tex_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0,
                                 &mapped)))
            return false;
        const unsigned char* sd = im->GetData();
        uint8_t* base = static_cast<uint8_t*>(mapped.pData);
        for (int y = 0; y < sh; ++y) {
            const unsigned char* srow = sd + (size_t)y * sw * 3;
            uint8_t* drow = base + (size_t)y * mapped.RowPitch;
            for (int x = 0; x < sw; ++x) {
                const unsigned char* p = srow + (size_t)x * 3;
                drow[x * 4 + 0] = p[2];  // B
                drow[x * 4 + 1] = p[1];  // G
                drow[x * 4 + 2] = p[0];  // R
                drow[x * 4 + 3] = 0xff;  // A
            }
        }
        context_->Unmap(src_tex_.Get(), 0);
    } else {
        return false;
    }

    // Draw the full-screen quad: the GPU samples the source texture across the
    // whole client area, doing the upscale.
    ID3D11RenderTargetView* rtv = rtv_.Get();
    context_->OMSetRenderTargets(1, &rtv, nullptr);
    D3D11_VIEWPORT vp = {};
    vp.Width = (float)W; vp.Height = (float)H; vp.MaxDepth = 1.0f;
    context_->RSSetViewports(1, &vp);
    context_->RSSetState(rasterizer_.Get());

    const UINT stride = 6 * sizeof(float), offset = 0;
    ID3D11Buffer* vb = vertex_buffer_.Get();
    context_->IASetInputLayout(input_layout_.Get());
    context_->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
    context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    context_->VSSetShader(vs_.Get(), nullptr, 0);
    context_->PSSetShader(ps_.Get(), nullptr, 0);
    ID3D11ShaderResourceView* srv = src_srv_.Get();
    context_->PSSetShaderResources(0, 1, &srv);
    ID3D11SamplerState* smp = sampler_.Get();
    context_->PSSetSamplers(0, 1, &smp);
    context_->Draw(4, 0);

    const UINT sync = OPTION(kPrefVsync) ? 1u : 0u;
    const UINT pf =
        (allow_tearing_ && !OPTION(kPrefVsync)) ? DXGI_PRESENT_ALLOW_TEARING : 0u;
    swap_chain_->Present(sync, pf);
    return true;
}

void DX11DrawingPanel::DrawImage(wxWindowDC& dc, wxImage* im)
{
    if (swap_chain_ && PresentFrame(im))
        return;
    // Device unavailable (or present failed): plain wx software path.
    BasicDrawingPanel::DrawImage(dc, im);
}

void DX11DrawingPanel::Teardown()
{
    if (context_) context_->ClearState();
    src_srv_.Reset();
    src_tex_.Reset();
    rasterizer_.Reset();
    sampler_.Reset();
    vertex_buffer_.Reset();
    input_layout_.Reset();
    ps_.Reset();
    vs_.Reset();
    rtv_.Reset();
    swap_chain_.Reset();
    context_.Reset();
    device_.Reset();
}
#endif  // __WXMSW__ && !NO_D3D11

#if defined(__WXMSW__) && !defined(NO_D3D)
#undef  DIRECT3D_VERSION
#define DIRECT3D_VERSION 0x0900
#include <d3d9.h> // main include file

typedef HRESULT (WINAPI *LPDIRECT3DCREATE9EX)(UINT, IDirect3D9Ex**);

DXDrawingPanel::DXDrawingPanel(wxWindow* parent, int _width, int _height)
        : DrawingPanel(parent, _width, _height)
        , d3d(NULL)
        , device(NULL)
        , texture(NULL)
        , texture_width(0)
        , texture_height(0)
        , using_d3d9ex(false)
{
    memset(delta, 0xff, sizeof(delta));

    if (OPTION(kDispFilter) == config::Filter::kNone &&
        OPTION(kDispIFB) == config::Interframe::kNone) {
        systemColorDepth = (OPTION(kBitDepth) + 1) << 3;
    }

    HRESULT hr = E_FAIL; // Initialize hr to a failure value

    // Try to create Direct3D 9Ex interface (Vista+) via dynamic loading
    // This is required to support Windows XP, which doesn't export Direct3DCreate9Ex
    HMODULE hD3D9 = LoadLibrary(TEXT("d3d9.dll"));

    memset(delta, 0xff, sizeof(delta));

    // Match what SDL/GL/Metal constructors do:
    if (OPTION(kDispFilter) == config::Filter::kNone &&
        OPTION(kDispIFB) == config::Interframe::kNone) {
        systemColorDepth = (OPTION(kBitDepth) + 1) << 3;
    }

    if (hD3D9) {
        // Attempt to get the function address
        LPDIRECT3DCREATE9EX Direct3DCreate9ExPtr =
             reinterpret_cast<LPDIRECT3DCREATE9EX>(reinterpret_cast<void*>(GetProcAddress(hD3D9, "Direct3DCreate9Ex")));

        if (Direct3DCreate9ExPtr) {
            // Function found: try to create D3D9Ex
            hr = Direct3DCreate9ExPtr(D3D_SDK_VERSION, (IDirect3D9Ex**)&d3d);

            if (SUCCEEDED(hr)) {
                using_d3d9ex = true;
                wxLogDebug(_("Using Direct3D 9Ex (dynamically loaded)"));
            }
        }
    } else {
        wxLogError(_("Failed to load d3d9.dll"));
        return;
    }

    // If XP, or Direct3DCreate9Ex failed
    if (!d3d) {
        // Fall back to regular Direct3D 9
        d3d = Direct3DCreate9(D3D_SDK_VERSION);
        if (!d3d) {
            wxLogError(_("Failed to create Direct3D 9 interface"));
            return;
        }
        wxLogDebug(_("Using Direct3D 9"));
    }

    // Get display mode
    D3DDISPLAYMODE d3ddm;
    hr = d3d->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &d3ddm);
    if (FAILED(hr)) {
        wxLogError(_("Failed to get adapter display mode: 0x%08X"), hr);
        return;
    }

    // Set up present parameters
    D3DPRESENT_PARAMETERS d3dpp;
    ZeroMemory(&d3dpp, sizeof(d3dpp));
    d3dpp.Windowed = TRUE;
    d3dpp.BackBufferFormat = d3ddm.Format;
    d3dpp.BackBufferWidth = 0;  // Use window size
    d3dpp.BackBufferHeight = 0; // Use window size
    d3dpp.PresentationInterval = OPTION(kPrefVsync) ?
                                  D3DPRESENT_INTERVAL_ONE :
                                  D3DPRESENT_INTERVAL_IMMEDIATE;

    const wxChar* final_swap_effect = wxT("UNKNOWN"); // Variable to log the successful swap effect

    // Try up to 2 times: FlipEx first (if D3D9Ex), then Discard
    for (int swap_attempt = 0; swap_attempt < 2; swap_attempt++) {
        // First attempt with D3D9Ex: try FlipEx, otherwise use Discard
        if (swap_attempt == 0 && using_d3d9ex) {
            d3dpp.SwapEffect = D3DSWAPEFFECT_FLIPEX;
            d3dpp.BackBufferCount = 2;
        } else {
            if (swap_attempt == 1) wxLogDebug(_("FlipEx not supported, falling back to Discard swap effect"));
            d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
            d3dpp.BackBufferCount = 0;
        }

        // Create Direct3D device - try hardware vertex processing first
        hr = d3d->CreateDevice(
                D3DADAPTER_DEFAULT,
                D3DDEVTYPE_HAL,
                (HWND)GetHandle(),
                D3DCREATE_HARDWARE_VERTEXPROCESSING,
                &d3dpp,
                &device
            );

        if (FAILED(hr)) {
            wxLogDebug(_("Hardware vertex processing not available, falling back to software"));

            // Fallback to software vertex processing
            hr = d3d->CreateDevice(
                    D3DADAPTER_DEFAULT,
                    D3DDEVTYPE_HAL,
                    (HWND)GetHandle(),
                    D3DCREATE_SOFTWARE_VERTEXPROCESSING,
                    &d3dpp,
                    &device
                );
        }

        // If succeeded or not using D3D9Ex, break out
        if (SUCCEEDED(hr) || !using_d3d9ex) {
            if (SUCCEEDED(hr)) {
                // Record the successful swap effect for logging
                final_swap_effect = (d3dpp.SwapEffect == D3DSWAPEFFECT_FLIPEX) ? wxT("FLIPEX") : wxT("DISCARD");
            }
            break;
        }
    }

    if (FAILED(hr)) {
        wxLogError(_("Failed to create Direct3D device: 0x%08X"), hr);
        return;
    }

    // Log the successful device creation along with the final swap effect
    wxLogDebug(_("Direct3D 9%s device created successfully (SwapEffect: %s)"),
               using_d3d9ex ? wxT("Ex") : wxT(""),
               final_swap_effect);
}

DXDrawingPanel::~DXDrawingPanel()
{
    if (texture) {
        texture->Release();
        texture = NULL;
    }
    if (device) {
        device->Release();
        device = NULL;
    }
    if (d3d) {
        d3d->Release();
        d3d = NULL;
    }
}

void DXDrawingPanel::DrawingPanelInit()
{
    DrawingPanelBase::DrawingPanelInit();

    // No Direct3D 9 device -> setup (done in the constructor) failed; flag it so
    // GameArea falls back to the next renderer in the priority list.
    if (!device) {
        init_failed_ = true;
        return;
    }

    // Calculate texture size (use actual scaled size)
    texture_width = (int)std::ceil(width * scale);
    texture_height = (int)std::ceil(height * scale);

    wxLogDebug(_("DXDrawingPanel initialized: %dx%d (scale: %f)"),
               texture_width, texture_height, scale);
}

bool DXDrawingPanel::ResetDevice()
{
    if (!device || !d3d) {
        return false;
    }

    // Release texture before reset (required for D3DPOOL_DEFAULT resources)
    if (texture) {
        texture->Release();
        texture = NULL;
        texture_width = 0;
        texture_height = 0;
    }

    // Get current display mode
    D3DDISPLAYMODE d3ddm;
    HRESULT hr = d3d->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &d3ddm);
    if (FAILED(hr)) {
        wxLogError(_("Failed to get adapter display mode for reset: 0x%08X"), hr);
        return false;
    }

    // Get current client size
    wxSize client_size = GetClientSize();

    // Set up present parameters
    D3DPRESENT_PARAMETERS d3dpp;
    ZeroMemory(&d3dpp, sizeof(d3dpp));
    d3dpp.Windowed = TRUE;
    d3dpp.BackBufferFormat = d3ddm.Format;
    d3dpp.BackBufferWidth = client_size.GetWidth();
    d3dpp.BackBufferHeight = client_size.GetHeight();
    d3dpp.PresentationInterval = OPTION(kPrefVsync) ?
                                  D3DPRESENT_INTERVAL_ONE :
                                  D3DPRESENT_INTERVAL_IMMEDIATE;

    const wxChar* attempted_swap_effect = wxT("DISCARD"); // Variable to track the attempted/final swap effect

    // Apply SwapEffect logic based on D3D9Ex usage (which was determined in the constructor)
    if (using_d3d9ex) {
        // Try FlipEx first, as it's the preferred mode for D3D9Ex
        d3dpp.SwapEffect = D3DSWAPEFFECT_FLIPEX;
        d3dpp.BackBufferCount = 2;
        attempted_swap_effect = wxT("FLIPEX");
    } else {
        // Fall back to standard Discard for regular D3D9
        d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
        d3dpp.BackBufferCount = 0;
    }

    wxLogDebug(_("Attempting Direct3D device reset (SwapEffect: %s)"), attempted_swap_effect);

    // Reset the device with current window size
    hr = device->Reset(&d3dpp);

    // If Reset with FlipEx fails on D3D9Ex, try Discard as a fallback for the reset
    if (FAILED(hr) && using_d3d9ex && d3dpp.SwapEffect == D3DSWAPEFFECT_FLIPEX) {
        wxLogDebug(_("Reset with FlipEx failed, retrying with Discard swap effect"));
        d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
        d3dpp.BackBufferCount = 0;
        hr = device->Reset(&d3dpp);

        if (SUCCEEDED(hr)) {
            // Update logging variable to reflect the successful fallback
            attempted_swap_effect = wxT("DISCARD (Fallback)");
        }
    }

    if (FAILED(hr)) {
        wxLogError(_("Failed to reset Direct3D device: 0x%08X"), hr);
        return false;
    }

    // Log the successful reset along with the final swap effect used
    wxLogDebug(_("Direct3D device reset successfully (Final SwapEffect: %s)"), attempted_swap_effect);
    return true;
}

void DXDrawingPanel::OnSize(wxSizeEvent& ev)
{
    if (device) {
        // Reset device to apply new back buffer size
        ResetDevice();
    }

    ev.Skip();
}

void DXDrawingPanel::DrawArea(wxWindowDC& dc)
{
    (void)dc; // unused params

    if (!device) {
        return;
    }

    if (!did_init) {
        DrawingPanelInit();
    }

    if (!todraw) {
        // Clear screen if no data to draw
        device->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
        if (using_d3d9ex)
            ((IDirect3DDevice9Ex*)device)->PresentEx(NULL, NULL, NULL, NULL, 0);
        else
            device->Present(NULL, NULL, NULL, NULL);
        return;
    }

    // Determine texture format based on output format
    D3DFORMAT tex_format;

    if (out_16) {
        tex_format = D3DFMT_R5G6B5;
    } else {
        // out_24 or 32-bit - convert to 32-bit XRGB for D3D
        tex_format = D3DFMT_X8R8G8B8;
    }

    int scaled_width = (int)std::ceil(width * scale);
    int scaled_height = (int)std::ceil(height * scale);

    // Create or recreate texture if size changed
    if (!texture || texture_width != scaled_width || texture_height != scaled_height) {
        if (texture) {
            texture->Release();
            texture = NULL;
        }

        HRESULT hr = device->CreateTexture(
            scaled_width,
            scaled_height,
            1,
            D3DUSAGE_DYNAMIC,
            tex_format,
            D3DPOOL_DEFAULT,
            &texture,
            NULL
        );

        if (FAILED(hr)) {
            wxLogError(_("Failed to create texture: 0x%08X"), hr);
            return;
        }

        texture_width = scaled_width;
        texture_height = scaled_height;
    }

    // Lock texture and copy pixel data
    D3DLOCKED_RECT locked_rect;
    HRESULT hr = texture->LockRect(0, &locked_rect, NULL, D3DLOCK_DISCARD);
    if (FAILED(hr)) {
        wxLogError(_("Failed to lock texture: 0x%08X"), hr);
        return;
    }

    // Calculate source pitch (bytes per row)
    // Border is scaled by the filter
    int inrb = out_8 ? 4 : out_16 ? 2 : out_24 ? 0 : 1; // border in pixels
    int scaled_border = (int)std::ceil(inrb * scale);
    int src_pitch = scaled_width;
    if (out_8) {
        src_pitch = (scaled_width + scaled_border) * 1;
    } else if (out_16) {
        src_pitch = (scaled_width + scaled_border) * 2;
    } else if (out_24) {
        src_pitch = scaled_width * 3;
    } else {
        // 32-bit
        src_pitch = (scaled_width + scaled_border) * 4;
    }

    // Copy pixel data
    uint8_t* src = todraw;
    uint8_t* dst = (uint8_t*)locked_rect.pBits;

    if (out_8) {
        // 8-bit palette mode - convert to 32-bit
        // Skip 1 row of top border
        src += src_pitch;
        for (int y = 0; y < scaled_height; y++) {
            uint8_t* src_row = src;
            uint8_t* dst_row = dst;
            for (int x = 0; x < scaled_width; x++) {
                uint8_t palIndex = *src_row++;
                // Simple palette expansion (this may need adjustment)
                if (palIndex == 0xff) {
                    dst_row[0] = dst_row[1] = dst_row[2] = 0xff;
                } else {
                    dst_row[0] = (palIndex & 0x3) << 6;       // B
                    dst_row[1] = ((palIndex >> 2) & 0x7) << 5; // G
                    dst_row[2] = ((palIndex >> 5) & 0x7) << 5; // R
                }
                dst_row[3] = 0;
                dst_row += 4;
            }
            src += src_pitch;
            dst += locked_rect.Pitch;
        }
    }
    else if (out_16) {
        // Convert 16-bit to D3D R5G6B5 format.
        uint16_t* src16 = (uint16_t*)src + src_pitch / 2;
        for (int y = 0; y < scaled_height; y++) {
            uint16_t* src_row = src16;
            uint16_t* dst_row = (uint16_t*)dst;
            // Convert RGB555 (R=14:10, G=9:5, B=4:0) to R5G6B5
            for (int x = 0; x < scaled_width; x++, src_row++) {
                uint16_t pixel = *src_row;
                uint8_t r5 = (pixel >> 10) & 0x1f;
                uint8_t g5 = (pixel >> 5) & 0x1f;
                uint8_t b5 = (pixel >> 0) & 0x1f;
                uint8_t g6 = (g5 << 1) | (g5 >> 4);
                *dst_row++ = (r5 << 11) | (g6 << 5) | b5;
            }
            src16 += src_pitch / 2;
            dst += locked_rect.Pitch;
        }
    } else if (out_24) {
        // Convert 24-bit RGB to 32-bit XRGB
        // Skip 1 row of top border
        src += scaled_width * 3;
        for (int y = 0; y < scaled_height; y++) {
            uint8_t* src_row = src;
            uint8_t* dst_row = dst;
            for (int x = 0; x < scaled_width; x++) {
                dst_row[0] = src_row[2]; // B
                dst_row[1] = src_row[1]; // G
                dst_row[2] = src_row[0]; // R
                dst_row[3] = 0;          // X
                src_row += 3;
                dst_row += 4;
            }
            src += src_pitch;
            dst += locked_rect.Pitch;
        }
    } else {
        // 32-bit - convert RGBA to BGRX (swap R and B)
        // Skip 1 row of top border
        src += src_pitch;
        for (int y = 0; y < scaled_height; y++) {
            uint8_t* src_row = src;
            uint8_t* dst_row = dst;
            for (int x = 0; x < scaled_width; x++) {
                dst_row[0] = src_row[2]; // B
                dst_row[1] = src_row[1]; // G
                dst_row[2] = src_row[0]; // R
                dst_row[3] = src_row[3]; // A/X
                src_row += 4;
                dst_row += 4;
            }
            src += src_pitch;
            dst += locked_rect.Pitch;
        }
    }

    texture->UnlockRect(0);

    // Begin rendering
    device->BeginScene();

    // Clear background
    device->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);

    // Set texture
    device->SetTexture(0, texture);

    // Set texture filtering
    D3DTEXTUREFILTERTYPE filter = OPTION(kDispBilinear) ?
                                   D3DTEXF_LINEAR : D3DTEXF_POINT;
    device->SetSamplerState(0, D3DSAMP_MAGFILTER, filter);
    device->SetSamplerState(0, D3DSAMP_MINFILTER, filter);

    // Disable lighting
    device->SetRenderState(D3DRS_LIGHTING, FALSE);

    // Set up orthographic projection for 2D rendering
    device->SetFVF(D3DFVF_XYZRHW | D3DFVF_TEX1);

    // Get window client size
    wxSize client_size = GetClientSize();
    float win_width = (float)client_size.GetWidth();
    float win_height = (float)client_size.GetHeight();

    // Calculate dimensions based on aspect ratio setting
    float render_width, render_height;
    float offset_x = 0.0f, offset_y = 0.0f;

    if (OPTION(kDispStretch)) {
        // Retain aspect ratio - add letterboxing
        float tex_aspect = (float)scaled_width / (float)scaled_height;
        float win_aspect = win_width / win_height;

        if (win_aspect > tex_aspect) {
            // Window is wider than texture
            render_height = win_height;
            render_width = win_height * tex_aspect;
            offset_x = (win_width - render_width) / 2.0f;
        } else {
            // Window is taller than texture
            render_width = win_width;
            render_height = win_width / tex_aspect;
            offset_y = (win_height - render_height) / 2.0f;
        }
    } else {
        // Stretch to fill - ignore aspect ratio
        render_width = win_width;
        render_height = win_height;
    }

    // Define vertices for textured quad
    struct Vertex {
        float x, y, z, rhw;
        float u, v;
    };

    Vertex vertices[4] = {
        { offset_x,              offset_y,               0.0f, 1.0f, 0.0f, 0.0f },
        { offset_x + render_width, offset_y,               0.0f, 1.0f, 1.0f, 0.0f },
        { offset_x + render_width, offset_y + render_height, 0.0f, 1.0f, 1.0f, 1.0f },
        { offset_x,              offset_y + render_height, 0.0f, 1.0f, 0.0f, 1.0f }
    };

    // Draw the quad
    device->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, 2, vertices, sizeof(Vertex));

    device->EndScene();

    // Present the frame
    if (using_d3d9ex)
        ((IDirect3DDevice9Ex*)device)->PresentEx(NULL, NULL, NULL, NULL, 0);
    else
        device->Present(NULL, NULL, NULL, NULL);
}
#endif
        
#ifndef NO_FFMPEG
static const wxString media_err(recording::MediaRet ret)
{
    switch (ret) {
        case recording::MRET_OK:
            return wxT("");

        case recording::MRET_ERR_NOMEM:
            return _("Memory allocation error");
                    
        case recording::MRET_ERR_NOCODEC:
            return _("Error initializing codec");
                    
        case recording::MRET_ERR_FERR:
            return _("Error writing to output file");
                    
        case recording::MRET_ERR_FMTGUESS:
            return _("Can't guess output format from file name");

        default:
            //    case MRET_ERR_RECORDING:
            //    case MRET_ERR_BUFSIZE:
            return _("Programming error; aborting!");
    }
}
        
void GameArea::StartVidRecording(const wxString& fname)
{
    recording::MediaRet ret;
            
    vid_rec.SetSampleRate(soundGetSampleRate());
    if ((ret = vid_rec.Record(UTF8(fname), basic_width, basic_height,
                                systemColorDepth))
        != recording::MRET_OK)
        wxLogError(_("Unable to begin recording to %s (%s)"), fname.mb_str(),
                    media_err(ret));
    else {
        MainFrame* mf = wxGetApp().frame;
        mf->cmd_enable &= ~(CMDEN_NVREC | CMDEN_NREC_ANY);
        mf->cmd_enable |= CMDEN_VREC;
        mf->enable_menus();
    }
}
        
void GameArea::StopVidRecording()
{
    vid_rec.Stop();
    MainFrame* mf = wxGetApp().frame;
    mf->cmd_enable &= ~CMDEN_VREC;
    mf->cmd_enable |= CMDEN_NVREC;
            
    if (!(mf->cmd_enable & (CMDEN_VREC | CMDEN_SREC)))
        mf->cmd_enable |= CMDEN_NREC_ANY;
            
    mf->enable_menus();
}
        
void GameArea::StartSoundRecording(const wxString& fname)
{
    recording::MediaRet ret;
            
    snd_rec.SetSampleRate(soundGetSampleRate());
    if ((ret = snd_rec.Record(UTF8(fname))) != recording::MRET_OK)
        wxLogError(_("Unable to begin recording to %s (%s)"), fname.mb_str(),
                    media_err(ret));
    else {
        MainFrame* mf = wxGetApp().frame;
        mf->cmd_enable &= ~(CMDEN_NSREC | CMDEN_NREC_ANY);
        mf->cmd_enable |= CMDEN_SREC;
        mf->enable_menus();
    }
}
        
void GameArea::StopSoundRecording()
{
    snd_rec.Stop();
    MainFrame* mf = wxGetApp().frame;
    mf->cmd_enable &= ~CMDEN_SREC;
    mf->cmd_enable |= CMDEN_NSREC;
            
    if (!(mf->cmd_enable & (CMDEN_VREC | CMDEN_SREC)))
        mf->cmd_enable |= CMDEN_NREC_ANY;
            
    mf->enable_menus();
}
        
void GameArea::AddFrame(const uint16_t* data, int length)
{
    recording::MediaRet ret;
            
    if ((ret = vid_rec.AddFrame(data, length)) != recording::MRET_OK) {
        wxLogError(_("Error in audio / video recording (%s); aborting"),
                    media_err(ret));
        vid_rec.Stop();
    }
            
    if ((ret = snd_rec.AddFrame(data, length)) != recording::MRET_OK) {
        wxLogError(_("Error in audio recording (%s); aborting"), media_err(ret));
        snd_rec.Stop();
    }
}
        
void GameArea::AddFrame(const uint8_t* data)
{
    recording::MediaRet ret;
            
    if ((ret = vid_rec.AddFrame(data)) != recording::MRET_OK) {
        wxLogError(_("Error in video recording (%s); aborting"), media_err(ret));
        vid_rec.Stop();
    }
}
#endif
        
void GameArea::MouseEvent(wxMouseEvent& ev)
{
    mouse_active_time = systemGetClock();
            
    wxPoint cur_pos = wxGetMousePosition();
            
    // Ignore small movements.
    if (!ev.Moving() || (std::abs(cur_pos.x - mouse_last_pos.x) >= 11 && std::abs(cur_pos.y - mouse_last_pos.y) >= 11)) {
        ShowPointer();
        ShowMenuBar();
    }
            
    mouse_last_pos = cur_pos;
            
    ev.Skip();
}
        
void GameArea::ShowPointer()
{
    if (!pointer_blanked || fullscreen) return;
            
    pointer_blanked = false;
            
    SetCursor(wxNullCursor);
            
    if (panel)
        panel->GetWindow()->SetCursor(wxNullCursor);
}
        
void GameArea::HidePointer()
{
    if (pointer_blanked || !main_frame) return;
            
    // FIXME: make time configurable
    if ((fullscreen || (systemGetClock() - mouse_active_time) > 3000) &&
        !(main_frame->MenusOpened() || main_frame->DialogOpened())) {
        pointer_blanked = true;
        SetCursor(wxCursor(wxCURSOR_BLANK));
                
        // wxGTK requires that subwindows get the cursor as well
        if (panel)
            panel->GetWindow()->SetCursor(wxCursor(wxCURSOR_BLANK));
    }
}
        
        // We do not hide the menubar on mac, on mac it is not part of the main frame
        // and the user can adjust hiding behavior herself.
void GameArea::HideMenuBar()
{
#ifndef __WXMAC__
    if (!main_frame || menu_bar_hidden || !gopts.hide_menu_bar) return;
            
    if (((systemGetClock() - mouse_active_time) > 3000) && !main_frame->MenusOpened()) {
#ifdef __WXMSW__
        current_hmenu = static_cast<HMENU>(main_frame->GetMenuBar()->GetHMenu());
        ::SetMenu(main_frame->GetHandle(), NULL);
#else
        main_frame->GetMenuBar()->Hide();
#endif
        SendSizeEvent();
        menu_bar_hidden = true;
    }
#endif
}
        
void GameArea::ShowMenuBar()
{
#ifndef __WXMAC__
    if (!main_frame || !menu_bar_hidden) return;
            
#ifdef __WXMSW__
    if (current_hmenu != NULL) {
        ::SetMenu(main_frame->GetHandle(), current_hmenu);
        current_hmenu = NULL;
    }
#else
    main_frame->GetMenuBar()->Show();
#endif
    SendSizeEvent();
    menu_bar_hidden = false;
#endif
}
        
void GameArea::OnGBBorderChanged(config::Option* option) {
    if (game_type() == IMAGE_GB && gbSgbMode) {
        if (option->GetBool()) {
            AddBorder();
            gbSgbRenderBorder();
        } else {
            DelBorder();
        }
    }
}
        
bool GameArea::HdrEffective() const {
    return panel && panel->HdrActive();
}

void GameArea::ApplyAutoColorCorrection() {
    if (!OPTION(kDispColorCorrectionAuto))
        return;
    // Follow the effective output, not the raw preference: HDR may be enabled in
    // the options but unavailable on this display, in which case we output SDR.
    const bool hdr_effective = HdrEffectivelyAvailable() && OPTION(kDispHDR);
    const auto wanted = hdr_effective
                            ? config::ColorCorrectionProfile::kRec2020
                            : config::ColorCorrectionProfile::kSRGB;
    if (OPTION(kDispColorCorrectionProfile) != wanted)
        OPTION(kDispColorCorrectionProfile) = wanted;
}

bool GameArea::IsHdrCapableRenderer(config::RenderMethod m) const {
    using RM = config::RenderMethod;
#ifndef NO_VULKAN
    if (m == RM::kVulkan) return VbamVulkanRuntimeAvailable();
#endif
#if defined(__WXMSW__) && !defined(NO_D3D12)
    if (m == RM::kDirect3d12) return true;
#endif
#if defined(__WXMAC__)
#ifndef NO_METAL
    if (m == RM::kMetal) return true;
#endif
    if (m == RM::kQuartz2d) return true;  // CoreGraphics EDR
    if (m == RM::kSimple) return true;    // Simple is the Quartz2D driver on macOS
#endif
#if defined(__WXGTK__) && defined(HAVE_WAYLAND_CM)
    // On Wayland, Simple is the software shm BT.2020 PQ driver (HDR-capable);
    // on X11 it is the XOrg deep-color driver (not HDR), so gate on Wayland.
    if (m == RM::kSimple && IsWayland()) return true;
#endif
#if defined(__WXMSW__) && !defined(NO_D3D11)
    if (m == RM::kSimple) return true;    // D3D11 software HDR path
#endif
#if !defined(NO_OGL) && defined(HAVE_WAYLAND_EGL)
    if (m == RM::kOpenGL) return true;    // Wayland EGL only
#endif
#ifdef ENABLE_SDL3
    if (m == RM::kSDL) return true;
#endif
    return false;
}

bool GameArea::HdrEffectivelyAvailable() const {
    return hdr::HdrAvailable() && !hdr_reverted_to_sdr_;
}

bool GameArea::RenderMethodHiddenForHdr(config::RenderMethod m) const {
    if (!OPTION(kDispHDR) || !HdrEffectivelyAvailable())
        return false;
    // Hidden if it can't do HDR, or it tried and failed this session.
    return !IsHdrCapableRenderer(m) || hdr_failed_renderers_.count(m) != 0;
}

void GameArea::EvaluateHdrRenderer() {
    if (!OPTION(kDispHDR) || !hdr::HdrAvailable() || !panel)
        return;
    if (panel->HdrActive()) {
        hdr_reverted_to_sdr_ = false;  // working -- clear any stale session revert
        return;
    }
    if (hdr_reverted_to_sdr_)
        return;  // already gave up this session; don't keep retrying/looping

    const config::RenderMethod current = OPTION(kDispRenderMethod);

    // An HDR swapchain's BT.2020 PQ color-space support can't be confirmed until
    // the window is composited on the HDR output, which may not have happened
    // when this panel latched its HDR state at construction -- a freshly shown
    // window, and especially the very first panel at startup. So an HDR-capable
    // renderer can come up reporting SDR purely because it was checked too early.
    // Before abandoning the user's choice for a different renderer, give it one
    // second chance: recreate its panel now that the window is up (this is the
    // same moment a fallback renderer would have been created, by which point the
    // output is ready). If it still can't present HDR after that, fall through to
    // the normal fallback below.
    if (IsHdrCapableRenderer(current) &&
        hdr_retried_renderers_.count(current) == 0) {
        hdr_retried_renderers_.insert(current);
        wxLogDebug(wxT("HDR-capable renderer reported SDR on first init; "
                       "recreating it once before falling back."));
        ResetPanel();  // recreate the same renderer; HDR re-evaluated next idle
        return;
    }

    // This renderer failed to present HDR; record it and try another. Iterate in
    // order of preference (most robust native HDR backend first) rather than
    // RenderMethod enum order: e.g. on Linux that means Vulkan before SDL, since
    // SDL's HDR path is the least reliable and shouldn't be picked over Vulkan
    // just because it has a lower enum value.
    hdr_failed_renderers_.insert(current);

    using RM = config::RenderMethod;
    static const RM kHdrPreference[] = {
#if defined(__WXMSW__) && !defined(NO_D3D12)
        RM::kDirect3d12,
#endif
#if defined(__WXMAC__) && !defined(NO_METAL)
        RM::kMetal,
#endif
#ifndef NO_VULKAN
        RM::kVulkan,
#endif
#if !defined(NO_OGL) && defined(HAVE_WAYLAND_EGL)
        RM::kOpenGL,
#endif
#ifdef ENABLE_SDL3
        RM::kSDL,
#endif
#if defined(__WXMAC__) || (defined(__WXGTK__) && defined(HAVE_WAYLAND_CM)) || \
    (defined(__WXMSW__) && !defined(NO_D3D11))
        // Software HDR last resort: Quartz2D EDR (macOS), shm BT.2020 PQ
        // (Wayland) or D3D11 (Windows). IsHdrCapableRenderer() filters it out
        // where Simple isn't HDR-capable (e.g. X11).
        RM::kSimple,
#endif
    };

    for (const RM m : kHdrPreference) {
        if (m != OPTION(kDispRenderMethod) && IsHdrCapableRenderer(m) &&
            hdr_failed_renderers_.count(m) == 0 &&
            render_init_failed_.count(m) == 0) {
            // Expected, silent fallback (e.g. OpenGL HDR is NVIDIA-only, so on
            // Mesa we transparently move to Vulkan). Debug-level, not a dialog.
            // Skip renderers that failed to initialize at all (render_init_failed_)
            // so an HDR-capable-but-broken renderer isn't picked in a loop with
            // EvaluateRenderer.
            wxLogDebug(wxT("HDR renderer failed; trying another HDR renderer."));
            OPTION(kDispRenderMethod) = m;  // observer schedules a panel reset
            return;
        }
    }

    // Every HDR-capable renderer failed: fall back to SDR for this session.
    // Leave the HDR option in "auto" (true) -- only the user turns it off. The
    // checkbox stays checked and the dialog shows a warning; output is SDR. But
    // reflect SDR in the auto color-correction profile (HDR would have used
    // Rec2020) so colors aren't corrected for a wide gamut we aren't presenting.
    wxLogDebug(wxT("No renderer could present HDR; reverting to SDR for this session."));
    hdr_reverted_to_sdr_ = true;
    if (OPTION(kDispColorCorrectionAuto))
        OPTION(kDispColorCorrectionProfile) = config::ColorCorrectionProfile::kSRGB;
}

void GameArea::EvaluateRenderer() {
    if (!panel || !panel->DrawingInitFailed())
        return;

    // The SDL render method is a meta-renderer that selects and falls back among
    // its own backends internally (and the dialog steers its backend picker).
    // Don't pull the user off an explicitly chosen SDL onto a different native
    // render method on an init hiccup -- a transient failure during a panel
    // reset (e.g. toggling deep color) would otherwise bounce SDL to the first
    // working method in the priority list (Vulkan), abandoning the SDL choice.
    if (OPTION(kDispRenderMethod) == config::RenderMethod::kSDL)
        return;

    using RM = config::RenderMethod;
    // Per-platform renderer priority. Only methods compiled in on this platform
    // appear (the #if guards mirror the RenderMethod enum and NewPanelForRender-
    // Method cases), so the list is exactly:
    //   Windows: DX12, Vulkan, SDL, OpenGL, Direct3D 9, Simple
    //   macOS:   Metal, Vulkan, SDL, OpenGL, Simple
    //   Linux:   Vulkan, SDL, OpenGL, Simple
    static const RM kRendererPriority[] = {
#if defined(__WXMSW__) && !defined(NO_D3D12)
        RM::kDirect3d12,
#endif
#if defined(__WXMAC__) && !defined(NO_METAL)
        RM::kMetal,
#endif
#ifndef NO_VULKAN
        RM::kVulkan,
#endif
        RM::kSDL,
#ifndef NO_OGL
        RM::kOpenGL,
#endif
#if defined(__WXMSW__) && !defined(NO_D3D)
        RM::kDirect3d,  // legacy D3D9, after OpenGL
#endif
        RM::kSimple,
    };

    // Record the failed renderer and switch to the next one not yet tried. The
    // option write fires the render observer, which resets the panel; the next
    // OnIdle recreates it with the new renderer and re-evaluates.
    render_init_failed_.insert(OPTION(kDispRenderMethod));
    for (const RM m : kRendererPriority) {
#ifndef NO_VULKAN
        // Skip Vulkan when its loader is absent at run time (delay-loaded, not
        // present) -- selecting it would fault on the first delay-loaded call.
        if (m == RM::kVulkan && !VbamVulkanRuntimeAvailable())
            continue;
#endif
        if (m != OPTION(kDispRenderMethod) && render_init_failed_.count(m) == 0) {
            wxLogInfo(_("Renderer failed to initialize; trying the next available renderer."));
            OPTION(kDispRenderMethod) = m;  // observer schedules a panel reset
            return;
        }
    }

    // Nothing left to try: keep the (non-working) panel rather than loop. The
    // user will see a blank/error frame, but every fallback was exhausted.
    wxLogError(_("No renderer could be initialized."));
}

bool GameArea::DeepColorEffectivelyAvailable() const {
    return hdr::DeepColor10Available() && !deep_color_reverted_;
}

bool GameArea::IsDeepColorCapableRenderer(config::RenderMethod m) const {
    using RM = config::RenderMethod;
#if !defined(NO_OGL)
    if (m == RM::kOpenGL) return true;
#endif
#ifndef NO_VULKAN
    if (m == RM::kVulkan) return VbamVulkanRuntimeAvailable();
#endif
#ifdef ENABLE_SDL3
    // The SDL3 renderer can present a 10-bit SDR surface on X11 when the chosen
    // backend advertises a 2101010 texture format (verified at panel init). If
    // the selected SDL backend turns out not to, EvaluateDeepColorRenderer
    // records the failure and moves on, like any other capable renderer.
    if (m == RM::kSDL) return true;
#endif
#ifdef __WXGTK__
    // On X11 the Simple renderer is backed by the native XOrg driver, which
    // presents a 10-bit deep-color surface on a depth-30 screen. (On Wayland
    // deep color is never available, so this is moot there.)
    if (m == RM::kSimple) return true;
#endif
    return false;
}

bool GameArea::RenderMethodHiddenForDeepColor(config::RenderMethod m) const {
    if (!OPTION(kDispDeepColor) || !DeepColorEffectivelyAvailable())
        return false;
    // Hidden if it can't present 10-bit, or it tried and failed this session.
    return !IsDeepColorCapableRenderer(m) || deep_color_failed_renderers_.count(m) != 0;
}

void GameArea::EvaluateDeepColorRenderer() {
    if (!OPTION(kDispDeepColor) || !hdr::DeepColor10Available() || !panel)
        return;
    if (panel->DeepColorActive()) {
        deep_color_reverted_ = false;  // working -- clear any stale session revert
        return;
    }
    if (deep_color_reverted_)
        return;  // already gave up this session; don't keep retrying/looping

    // The SDL renderer manages its own 10-bit-capable backend: the panel
    // substitutes a 2101010-capable backend (vulkan) internally and the dialog
    // steers the backend picker to "default". So never switch the render method
    // away from a deep-color-capable SDL build -- it presents 10-bit through its
    // own backend once the source is 32-bit, and a transient inactive state
    // (e.g. a 16-bit source) must not bounce the user onto the native Vulkan
    // renderer. An SDL2 build (where kSDL is not deep-color-capable) still falls
    // through and switches, as it genuinely cannot present 10-bit.
    if (OPTION(kDispRenderMethod) == config::RenderMethod::kSDL &&
        IsDeepColorCapableRenderer(config::RenderMethod::kSDL))
        return;

    // The active renderer did not present 10-bit -- either it isn't capable
    // (simple/wx, SDL) or it is capable but failed to obtain a 10-bit surface.
    // Record it and move to another capable renderer, preferring the most
    // robust native backend, so enabling deep color while on a non-capable
    // renderer transparently switches to one that works.
    deep_color_failed_renderers_.insert(OPTION(kDispRenderMethod));

    using RM = config::RenderMethod;
    static const RM kDeepColorPreference[] = {
#ifndef NO_VULKAN
        RM::kVulkan,
#endif
#if !defined(NO_OGL)
        RM::kOpenGL,
#endif
#if defined(__WXGTK__)
        RM::kSimple,  // software (XOrg 10-bit) -- last resort
#endif
    };

    for (const RM m : kDeepColorPreference) {
        if (m != OPTION(kDispRenderMethod) && IsDeepColorCapableRenderer(m) &&
            deep_color_failed_renderers_.count(m) == 0 &&
            render_init_failed_.count(m) == 0) {  // skip init-failed renderers
            wxLogDebug(wxT("Deep-color renderer failed; trying another renderer."));
            OPTION(kDispRenderMethod) = m;  // observer schedules a panel reset
            return;
        }
    }

    // Every capable renderer failed to present 10-bit: fall back to ordinary
    // 8-bit SDR for this session. Leave the option in "auto" (true) -- only the
    // user turns it off; the checkbox stays checked and the dialog shows a
    // warning.
    wxLogInfo(_("No renderer could present 10-bit deep color; using 8-bit for this session."));
    deep_color_reverted_ = true;
}

DrawingPanelBase* GameArea::NewPanelForRenderMethod(config::RenderMethod method) {
    switch (method) {
        case config::RenderMethod::kSimple:
#if defined(__WXMAC__)
            // Simple is backed by the Quartz2D (CoreGraphics) driver, which
            // can also present EDR HDR.
            return new Quartz2DDrawingPanel(this, basic_width, basic_height);
#elif defined(__WXGTK__)
            // On X11, Simple is backed by the native XOrg driver (10-bit deep
            // color capable). On Wayland it is backed by the software HDR driver
            // (shm BT.2020 PQ subsurface), which falls back to the plain wx path
            // when HDR is off/unavailable.
            if (!IsWayland())
                return new XOrgDrawingPanel(this, basic_width, basic_height);
#if defined(HAVE_WAYLAND_SUPPORT)
            return new WaylandDrawingPanel(this, basic_width, basic_height);
#else
            return new BasicDrawingPanel(this, basic_width, basic_height);
#endif
#elif defined(__WXMSW__) && !defined(NO_D3D11)
            // On Windows, Simple is backed by the D3D11 software-style driver
            // (HDR-capable via a flip swapchain; falls back to the wx path if no
            // device).
            return new DX11DrawingPanel(this, basic_width, basic_height);
#else
            return new BasicDrawingPanel(this, basic_width, basic_height);
#endif
        case config::RenderMethod::kSDL:
            return new SDLDrawingPanel(this, basic_width, basic_height);
#ifdef __WXMAC__
#ifndef NO_METAL
        case config::RenderMethod::kMetal:
            if (is_macosx_1012_or_newer())
                return new MetalDrawingPanel(this, basic_width, basic_height);
            wxLogInfo(_("Metal is unavailable, defaulting to OpenGL"));
            return new GLDrawingPanel(this, basic_width, basic_height);
#endif
        case config::RenderMethod::kQuartz2d:
            return new Quartz2DDrawingPanel(this, basic_width, basic_height);
#endif
#ifndef NO_OGL
        case config::RenderMethod::kOpenGL:
#if !defined(HAVE_WAYLAND_EGL) && !defined(NO_VULKAN)
            // OpenGL needs EGL/GLX, which a native Wayland client without
            // the wx EGL canvas cannot provide. Fall back to Vulkan, which
            // presents natively (and can do HDR).
            if (IsWayland()) {
                wxLogInfo(_("OpenGL is unavailable on Wayland without EGL, using Vulkan"));
                return new VKDrawingPanel(this, basic_width, basic_height);
            }
#endif
            return new GLDrawingPanel(this, basic_width, basic_height);
#endif
#if defined(__WXMSW__) && !defined(NO_D3D)
        case config::RenderMethod::kDirect3d:
            return new DXDrawingPanel(this, basic_width, basic_height);
#endif
#if defined(__WXMSW__) && !defined(NO_D3D12)
        case config::RenderMethod::kDirect3d12:
            return new DX12DrawingPanel(this, basic_width, basic_height);
#endif
#ifndef NO_VULKAN
        case config::RenderMethod::kVulkan:
#ifdef __WXMAC__
            if (is_macosx_11_or_newer())
                return new VKDrawingPanel(this, basic_width, basic_height);
            wxLogInfo(_("Vulkan is unavailable, defaulting to OpenGL"));
            return new GLDrawingPanel(this, basic_width, basic_height);
#else
            return new VKDrawingPanel(this, basic_width, basic_height);
#endif
#endif
        case config::RenderMethod::kLast:
            VBAM_NOTREACHED_RETURN(nullptr);
    }
    return nullptr;
}

void GameArea::ProbeOutputCapabilities() {
    using RM = config::RenderMethod;

    // Startup-only: never disturb a live panel / running game.
    if (panel || emusys)
        return;

    // The cheap startup probe (hdr::DetectAvailability) gates which lane is even
    // worth bringing a GPU up for: HDR on the HDR-capable platforms, 10-bit SDR
    // on X11. If it found nothing plausible there is nothing to confirm.
    const bool try_hdr  = hdr::HdrAvailable();
    const bool try_deep = hdr::DeepColor10Available();
    if (!try_hdr && !try_deep)
        return;

    // Save everything the probe mutates. The feature flags must be on for the
    // renderers to request an HDR / 10-bit surface; filtering is forced off so a
    // single plain black frame drives the simplest (no thread / no upscale) path.
    // NB: snapshot the *values*, not the OptionProxy. `const auto x = OPTION(...)`
    // deduces the proxy type (a live view of the option), so it would track the
    // probe's own mutations below, and the restore would be a no-op proxy-to-proxy
    // copy -- leaving filter/IFB/profile stuck at the forced kNone (and saved that
    // way on exit). Naming the value type forces the proxy's conversion operator,
    // taking a real snapshot.
    const RM saved_method = OPTION(kDispRenderMethod);
    const bool saved_hdr  = OPTION(kDispHDR);
    const bool saved_deep = OPTION(kDispDeepColor);
    const config::Filter saved_filter = OPTION(kDispFilter);
    const config::Interframe saved_ifb = OPTION(kDispIFB);
    const config::ColorCorrectionProfile saved_profile =
        OPTION(kDispColorCorrectionProfile);

    OPTION(kDispFilter) = config::Filter::kNone;
    OPTION(kDispIFB)    = config::Interframe::kNone;

    // Bring `method` up on this panel, render one black frame so DrawingPanelInit()
    // creates the real swapchain/visual and latches its capability flags, and
    // return the panel for inspection. Sets this->panel so the draw path's
    // GetPanel()->panel lookup resolves to it; the caller tears it back down.
    auto bring_up = [this](RM method) -> DrawingPanelBase* {
        DrawingPanelBase* p = NewPanelForRenderMethod(method);
        if (!p)
            return nullptr;
        panel = p;
        wxWindow* w = p->GetWindow();
        w->SetSize(wxSize(basic_width, basic_height));
        // scale is 1 and filtering is off, so a (basic_width+2)x(basic_height+4)
        // 32-bpp zero buffer is a safe upper bound for any panel_color_depth_.
        const int stride = (basic_width + 2) * 4;
        std::vector<uint8_t> black(
            static_cast<size_t>(stride) * (basic_height + 4), 0);
        uint8_t* data = black.data();
        p->DrawArea(&data);
        // On Wayland the draw path only queues a paint; force it now so init runs
        // synchronously before we read the flags.
        w->Update();
        return p;
    };

    auto tear_down = [this](DrawingPanelBase* p) {
        if (!p)
            return;
        p->StopFilterThreads();
#if defined(__WXMSW__) && !defined(NO_D3D12)
        if (auto* dx12 = dynamic_cast<DX12DrawingPanel*>(p)) {
            delete dx12;
        } else
#endif
#if defined(__WXMSW__) && !defined(NO_D3D)
        if (auto* dx9 = dynamic_cast<DXDrawingPanel*>(p)) {
            delete dx9;
        } else
#endif
        {
            p->Destroy();
        }
        panel = nullptr;
    };

    bool hdr_ok = false, deep_ok = false;

    if (try_hdr) {
        OPTION(kDispHDR)       = true;
        OPTION(kDispDeepColor) = false;
        // Same preference order EvaluateHdrRenderer() uses at runtime.
        static const RM kHdrPreference[] = {
#if defined(__WXMSW__) && !defined(NO_D3D12)
            RM::kDirect3d12,
#endif
#if defined(__WXMAC__) && !defined(NO_METAL)
            RM::kMetal,
#endif
#ifndef NO_VULKAN
            RM::kVulkan,
#endif
#if !defined(NO_OGL) && defined(HAVE_WAYLAND_EGL)
            RM::kOpenGL,
#endif
#ifdef ENABLE_SDL3
            RM::kSDL,
#endif
#if defined(__WXMAC__) || (defined(__WXGTK__) && defined(HAVE_WAYLAND_CM)) || \
    (defined(__WXMSW__) && !defined(NO_D3D11))
            // Software HDR last resort: Quartz2D EDR (macOS), shm BT.2020 PQ
            // (Wayland) or D3D11 (Windows). IsHdrCapableRenderer() filters it
            // out where Simple isn't HDR-capable (e.g. X11).
            RM::kSimple,
#endif
        };
        for (const RM m : kHdrPreference) {
            if (!IsHdrCapableRenderer(m))
                continue;
            DrawingPanelBase* p = bring_up(m);
            hdr_ok = p && p->HdrActive();
            tear_down(p);
            if (hdr_ok)
                break;
        }
    } else if (try_deep) {
        OPTION(kDispDeepColor) = true;
        OPTION(kDispHDR)       = false;
        // Same preference order EvaluateDeepColorRenderer() uses at runtime.
        static const RM kDeepColorPreference[] = {
#ifndef NO_VULKAN
            RM::kVulkan,
#endif
#if !defined(NO_OGL)
            RM::kOpenGL,
#endif
#if defined(__WXGTK__)
            RM::kSimple,  // software (XOrg 10-bit) -- last resort
#endif
        };
        for (const RM m : kDeepColorPreference) {
            if (!IsDeepColorCapableRenderer(m))
                continue;
            DrawingPanelBase* p = bring_up(m);
            deep_ok = p && p->DeepColorActive();
            tear_down(p);
            if (deep_ok)
                break;
        }
    }

    // Restore the user's settings. (Toggling options above may have scheduled a
    // panel reset via the render observer; clear it -- there is no panel yet.)
    OPTION(kDispRenderMethod)          = saved_method;
    OPTION(kDispHDR)                   = saved_hdr;
    OPTION(kDispDeepColor)             = saved_deep;
    OPTION(kDispFilter)                = saved_filter;
    OPTION(kDispIFB)                   = saved_ifb;
    OPTION(kDispColorCorrectionProfile) = saved_profile;
    pending_panel_reset_ = false;

    // Publish the definite result: the checkbox now appears only when a renderer
    // actually presented the feature.
    hdr::SetAvailability(hdr_ok, deep_ok);
}

void GameArea::UpdateLcdFilter() {
    int DCCP = 0;

    switch (OPTION(kDispColorCorrectionProfile)) {
        case config::ColorCorrectionProfile::kSRGB:
            DCCP = 0;
            break;

        case config::ColorCorrectionProfile::kDCI:
            DCCP = 1;
            break;

        case config::ColorCorrectionProfile::kRec2020:
            DCCP = 2;
            break;

        case config::ColorCorrectionProfile::kLast:
            DCCP = 0;
            break;
    }

    if (loaded == IMAGE_GBA) {
        gbafilter_set_params(DCCP, (((float)OPTION(kGBADarken)) / 100));
        gbafilter_update_colors(OPTION(kGBALCDFilter));
    } else if (loaded == IMAGE_GB) {
        if (gbHardware & 4) { // Emulated Hardware is SGB
            // Prevent CGB LCD filter from applying
            gbcfilter_update_colors(false);
        } else if (gbHardware & 8) { // Emulated Hardware is GBA
            // Apply GBA LCD filter instead of the GBC LCD Filter
            gbafilter_set_params(DCCP, (((float)OPTION(kGBADarken)) / 100));
            gbafilter_update_colors(OPTION(kGBLCDFilter));
        } else {
            // Apply GBC LCD filter for GB/GBC modes
            gbcfilter_set_params(DCCP, (((float)OPTION(kGBLighten)) / 100));
            gbcfilter_update_colors(OPTION(kGBLCDFilter));
        }
    } else {
        gbafilter_set_params(DCCP, (((float)OPTION(kGBADarken)) / 100));
        gbcfilter_set_params(DCCP, (((float)OPTION(kGBLighten)) / 100));
        gbafilter_update_colors(false);
        gbcfilter_update_colors(false);
    }

    // Brightness/knee sliders and the color-correction profile feed the HDR
    // encoder; refresh its tables live without recreating the panel.
    if (panel)
        panel->UpdateHdrState();
}
        
void GameArea::SuspendScreenSaver() {
#ifdef HAVE_XSS
    if (xscreensaver_suspended || !gopts.suspend_screensaver)
        return;
    // suspend screensaver
#ifndef NO_WAYLAND
    if (emulating && !wxGetApp().UsingWayland()) {
#else
    if (emulating) {
#endif
        auto display = GetX11Display();
        XScreenSaverSuspend(display, true);
        xscreensaver_suspended = true;
    }
#endif // HAVE_XSS
}
        
void GameArea::UnsuspendScreenSaver() {
#ifdef HAVE_XSS
            // unsuspend screensaver
    if (xscreensaver_suspended) {
        auto display = GetX11Display();
        XScreenSaverSuspend(display, false);
        xscreensaver_suspended = false;
    }
#endif // HAVE_XSS
}
        
void GameArea::OnAudioRateChanged() {
    if (loaded == IMAGE_UNKNOWN) {
        return;
    }
            
    switch (game_type()) {
        case IMAGE_UNKNOWN:
            break;
                    
        case IMAGE_GB:
            gbSoundSetSampleRate(GetSampleRate());
            break;
                    
        case IMAGE_GBA:
            soundSetSampleRate(GetSampleRate());
            break;
    }
}
        
void GameArea::OnVolumeChanged(config::Option* option) {
    const int volume = option->GetInt();
    soundSetVolume(wxGetApp().mute ? 0.0 : (float)volume / 100.0);
    systemScreenMessage(wxString::Format(_("Volume: %d %%"), volume));
}
        
#ifdef __WXMAC__
#ifndef NO_METAL
MetalDrawingPanel::MetalDrawingPanel(wxWindow* parent, int _width, int _height)
        : DrawingPanel(parent, _width, _height)
{
    memset(delta, 0xff, sizeof(delta));

    // wxImage is 24-bit RGB, so 24-bit is preferred.  Filters require
    // 16 or 32, though
    if (OPTION(kDispFilter) == config::Filter::kNone &&
        OPTION(kDispIFB) == config::Interframe::kNone &&
        !(hdr::HdrAvailable() && OPTION(kDispHDR))) {
        // changing from 32 to 24 does not require regenerating color tables.
        // HDR needs a 32-bit source for the encoder, so leave it forced to 32.
        systemColorDepth = (OPTION(kBitDepth) + 1) << 3;
    }

    DrawingPanelInit();
}

void MetalDrawingPanel::DrawArea(uint8_t** data)
{
    // double-buffer buffer:
    //   if filtering, this is filter output, retained for redraws
    //   if not filtering, we still retain current image for redraws
    int outbpp = panel_color_depth_ >> 3;
    int inrb = (panel_color_depth_ == 8) ? 4 : (panel_color_depth_ == 16) ? 2 : (panel_color_depth_ == 24) ? 0 : 1;
    int outstride = std::ceil((width + inrb) * outbpp * scale);

    // Use multiple threads for filter processing, but force single-threaded
    // for plugin filters. RPI plugins expect the full frame.
    const int max_threads = (OPTION(kDispFilter) == config::Filter::kPlugin)
        ? 1 : GetMaxFilterThreads();

    // Compute stride for InterframeManager initialization
    int instride = (width + inrb) * (panel_color_depth_ >> 3);

    // Only allocate pixbuf2 when filters are used
    if (OPTION(kDispFilter) != config::Filter::kNone ||
        OPTION(kDispIFB) != config::Interframe::kNone) {
        if (!pixbuf2) {
            int allocstride = outstride, alloch = height;

            // gb may write borders, so allocate enough for them
            if (width == GameArea::GBWidth && height == GameArea::GBHeight) {
                allocstride = std::ceil((GameArea::SGBWidth + inrb) * outbpp * scale);
                alloch = GameArea::SGBHeight;
            }

            pixbuf2 = (uint8_t*)calloc(allocstride, std::ceil((alloch + 2) * scale));
        }
        todraw = pixbuf2;
    } else {
        // No filter: use g_pix directly, don't allocate or swap buffers
        todraw = *data;
    }

    // First, apply filters, if applicable, in parallel, if enabled
    // FIXME: && (gopts.ifb != FF_MOTION_BLUR || !renderer_can_motion_blur)
    if (OPTION(kDispFilter) != config::Filter::kNone ||
        OPTION(kDispIFB) != config::Interframe::kNone) {
        if (nthreads != max_threads) {
            if (nthreads) {
                if (nthreads > 1)
                    for (int i = 0; i < nthreads; i++) {
                        threads[i].lock_.Lock();
                        threads[i].src_ = NULL;
                        threads[i].sig_.Signal();
                        threads[i].lock_.Unlock();
                        threads[i].Wait();
                    }

                delete[] threads;
            }

            nthreads = max_threads;
            threads = new FilterThread[nthreads];

            // Initialize InterframeManager for per-region IFB processing (only if IFB enabled)
            if (OPTION(kDispIFB) != config::Interframe::kNone) {
                InterframeManager::Instance().Init(width, height, instride, nthreads);
            }

            // first time around, no threading in order to avoid
            // static initializer conflicts
            threads[0].threadno_ = 0;
            threads[0].nthreads_ = 1;
            threads[0].width_ = width;
            threads[0].height_ = height;
            threads[0].scale_ = scale;
            threads[0].src_ = *data;
            threads[0].dst_ = todraw;
            threads[0].delta_ = delta;
            threads[0].rpi_ = rpi_;
            threads[0].phase_ = FilterThread::Phase::Filter;
            threads[0].rpi_bpp_ = rpi_bpp_;
            threads[0].rpi_using_rgb565_ = rpi_using_rgb565_;
            threads[0].panel_color_depth_ = panel_color_depth_;
#ifdef VBAM_RPI_PROXY_SUPPORT
            threads[0].rpi_proxy_client_ = rpi_proxy_client_;
#endif
            threads[0].Entry();
            // IFB is handled inside Entry() for single-threaded mode

            // go ahead and start the threads up, though
            if (nthreads > 1) {
                for (int i = 0; i < nthreads; i++) {
                    threads[i].threadno_ = i;
                    threads[i].nthreads_ = nthreads;
                    threads[i].width_ = width;
                    threads[i].height_ = height;
                    threads[i].scale_ = scale;
                    threads[i].dst_ = todraw;
                    threads[i].delta_ = delta;
                    threads[i].rpi_ = rpi_;
                    threads[i].rpi_bpp_ = rpi_bpp_;
                    threads[i].rpi_using_rgb565_ = rpi_using_rgb565_;
                    threads[i].panel_color_depth_ = panel_color_depth_;
#ifdef VBAM_RPI_PROXY_SUPPORT
                    threads[i].rpi_proxy_client_ = rpi_proxy_client_;
#endif
                    threads[i].done_ = &filt_done;
                    threads[i].phase_ = FilterThread::Phase::Filter;
                    threads[i].ready_ = &filt_ready;
                    threads[i].Create();
                    threads[i].Run();
                }
                // Wait for all threads to signal they're ready before returning
                for (int i = 0; i < nthreads; i++) {
                    filt_ready.Wait();
                }
            }
        } else if (nthreads == 1) {
            threads[0].threadno_ = 0;
            threads[0].nthreads_ = 1;
            threads[0].width_ = width;
            threads[0].height_ = height;
            threads[0].scale_ = scale;
            threads[0].src_ = *data;
            threads[0].dst_ = todraw;
            threads[0].delta_ = delta;
            threads[0].rpi_ = rpi_;
            threads[0].phase_ = FilterThread::Phase::Filter;
            threads[0].rpi_bpp_ = rpi_bpp_;
            threads[0].rpi_using_rgb565_ = rpi_using_rgb565_;
            threads[0].panel_color_depth_ = panel_color_depth_;
#ifdef VBAM_RPI_PROXY_SUPPORT
            threads[0].rpi_proxy_client_ = rpi_proxy_client_;
#endif
            threads[0].Entry();
            // IFB is handled inside Entry() for single-threaded mode
        } else {
            // Phase 1: Run IFB threads first (before filters)
            if (OPTION(kDispIFB) != config::Interframe::kNone) {
                for (int i = 0; i < nthreads; i++) {
                    threads[i].phase_ = FilterThread::Phase::IFB;
                    threads[i].lock_.Lock();
                    threads[i].src_ = *data;
                    threads[i].sig_.Signal();
                    threads[i].lock_.Unlock();
                }

                for (int i = 0; i < nthreads; i++)
                    filt_done.Wait();
            }

            // Phase 2: Run filter threads (on IFB-processed source)
            for (int i = 0; i < nthreads; i++) {
                threads[i].phase_ = FilterThread::Phase::Filter;
                threads[i].lock_.Lock();
                threads[i].src_ = *data;
                threads[i].sig_.Signal();
                threads[i].lock_.Unlock();
            }

            for (int i = 0; i < nthreads; i++)
                filt_done.Wait();

            // Fix seams between thread regions by re-processing bands around boundaries
            // This runs in parallel using the existing filter threads
            // Only runs here (not on first frame when threads are just starting)
            if (nthreads > 1 && OPTION(kDispFilter) != config::Filter::kNone) {
                // Collect seam positions and merge overlapping bands
                std::vector<std::pair<int, int>> seamBands;  // (startY, endY) in source coords

                for (int i = 1; i < nthreads; i++) {
                    int seamY = height * i / nthreads;
                    int bandStart = std::max(0, seamY - kFilterContextRadius - kSeamMarginRows);
                    int bandEnd = std::min(height, seamY + kFilterContextRadius + kSeamMarginRows);

                    // Merge with previous band if overlapping
                    if (!seamBands.empty() && bandStart <= seamBands.back().second) {
                        seamBands.back().second = std::max(seamBands.back().second, bandEnd);
                    } else {
                        seamBands.push_back({bandStart, bandEnd});
                    }
                }

                // Assign seam bands to threads and run in parallel
                const int scale_int = static_cast<int>(scale);
                // Source base: skip 1 border row
                uint8_t* src_base = *data + instride;
                // Dest base: skip 1*scale border rows
                uint8_t* dst_base = todraw + outstride * scale_int;

                const int numBands = static_cast<int>(seamBands.size());
                for (int i = 0; i < nthreads; i++) {
                    threads[i].phase_ = FilterThread::Phase::SeamFix;
                    // Assign band to thread (some threads may have no work)
                    if (i < numBands) {
                        threads[i].seamBandStart_ = seamBands[i].first;
                        threads[i].seamBandEnd_ = seamBands[i].second;
                    } else {
                        threads[i].seamBandStart_ = -1;  // No work for this thread
                        threads[i].seamBandEnd_ = -1;
                    }
                    threads[i].seamSrc_ = src_base;
                    threads[i].seamDst_ = dst_base;
                    threads[i].lock_.Lock();
                    threads[i].src_ = src_base;  // Keep src_ non-null to avoid thread exit
                    threads[i].sig_.Signal();
                    threads[i].lock_.Unlock();
                }

                for (int i = 0; i < nthreads; i++)
                    filt_done.Wait();
            }
        }
    }

    // Note: We do NOT modify *data (which is g_pix) here.
    // g_pix must always point to the emulator's allocated buffer.
    // Modifying it would cause the panel destructor to free the wrong buffer,
    // leaving g_pix as a dangling pointer after panel reset.

    // draw OSD text old-style (directly into output buffer), if needed
    // new style flickers too much, so we'll stick to this for now
    if (wxGetApp().frame->IsFullScreen() || !OPTION(kPrefDisableStatus)) {
        GameArea* panel = wxGetApp().frame->GetPanel();

        int scaled_width = (int)std::ceil(width * scale);
        int scaled_height = (int)std::ceil(height * scale);

        if (panel->osdstat.size()) {
            wxString statText = wxString::FromUTF8(panel->osdstat.c_str());
            // Position closer to top-left for better visibility at native resolution
            // At scale=1.0 (native GBA/GB res): (4, 4) instead of (10, 20)
            int x = (int)std::ceil(4 * scale);
            int y = (int)std::ceil(4 * scale);
            drawTextWx(todraw + outstride * (panel_color_depth_ != 24), outstride,
                x, y, statText, scaled_width, scaled_height, scale, panel_color_depth_);
        }

        if (!panel->osdtext.empty()) {
            if (systemGetClock() - panel->osdtime < OSD_TIME) {
                // Position near bottom of screen, scaled with filter
                // At scale=1.0 (native res): positioned to allow 3 lines of text
                int x = (int)std::ceil(4 * scale);
                int osd_offset = (panel->game_type() == IMAGE_GB) ? 106 : 64;
                int y = scaled_height - (int)std::ceil(osd_offset * scale);
                drawTextWx(todraw + outstride * (systemColorDepth != 24), outstride,
                    x, y, panel->osdtext, scaled_width, scaled_height, scale, panel_color_depth_);
            } else
                panel->osdtext.clear();
        }
    }

    // Draw the current frame
    DrawArea();
}

void MetalDrawingPanel::PaintEv(wxPaintEvent& ev)
{
    // FIXME: implement
    if (!did_init) {
        DrawingPanelInit();
    }

    (void)ev; // unused params
            
    if (!todraw) {
        // since this is set for custom background, not drawing anything
        // will cause garbage to be displayed, so draw a black area
        draw_black_background(GetWindow());
        return;
    }

    if (todraw) {
        DrawArea();
    }
}
#endif
#endif

#ifndef NO_VULKAN
 
#include <algorithm>
#include <cstring>
#include <cmath>
#include <cassert>
 
// ─── SPIR-V shaders ─────────────────────────────────────────────────────────
// (unchanged — see original file for GLSL source reference)
 
const uint32_t VKDrawingPanel::kVertSpv[] = {
    0x07230203, 0x00010000, 0x0008000b, 0x0000005e,
    0x00000000, 0x00020011, 0x00000001, 0x0006000b,
    0x00000001, 0x4c534c47, 0x6474732e, 0x3035342e,
    0x00000000, 0x0003000e, 0x00000000, 0x00000001,
    0x0008000f, 0x00000000, 0x00000004, 0x6e69616d,
    0x00000000, 0x0000000c, 0x00000052, 0x0000005c,
    0x00030003, 0x00000002, 0x000001c2, 0x00040005,
    0x00000004, 0x6e69616d, 0x00000000, 0x00030005,
    0x00000009, 0x00736f70, 0x00060005, 0x0000000c,
    0x565f6c67, 0x65747265, 0x646e4978, 0x00007865,
    0x00030005, 0x00000018, 0x00004350, 0x00060006,
    0x00000018, 0x00000000, 0x5f637273, 0x74636572,
    0x00000000, 0x00060006, 0x00000018, 0x00000001,
    0x5f747364, 0x74636572, 0x00000000, 0x00030005,
    0x0000001a, 0x00006370, 0x00030005, 0x00000035,
    0x00007675, 0x00060005, 0x00000050, 0x505f6c67,
    0x65567265, 0x78657472, 0x00000000, 0x00060006,
    0x00000050, 0x00000000, 0x505f6c67, 0x7469736f,
    0x006e6f69, 0x00070006, 0x00000050, 0x00000001,
    0x505f6c67, 0x746e696f, 0x657a6953, 0x00000000,
    0x00070006, 0x00000050, 0x00000002, 0x435f6c67,
    0x4470696c, 0x61747369, 0x0065636e, 0x00070006,
    0x00000050, 0x00000003, 0x435f6c67, 0x446c6c75,
    0x61747369, 0x0065636e, 0x00030005, 0x00000052,
    0x00000000, 0x00040005, 0x0000005c, 0x76755f6f,
    0x00000000, 0x00040047, 0x0000000c, 0x0000000b,
    0x0000002a, 0x00030047, 0x00000018, 0x00000002,
    0x00050048, 0x00000018, 0x00000000, 0x00000023,
    0x00000000, 0x00050048, 0x00000018, 0x00000001,
    0x00000023, 0x00000010, 0x00030047, 0x00000050,
    0x00000002, 0x00050048, 0x00000050, 0x00000000,
    0x0000000b, 0x00000000, 0x00050048, 0x00000050,
    0x00000001, 0x0000000b, 0x00000001, 0x00050048,
    0x00000050, 0x00000002, 0x0000000b, 0x00000003,
    0x00050048, 0x00000050, 0x00000003, 0x0000000b,
    0x00000004, 0x00040047, 0x0000005c, 0x0000001e,
    0x00000000, 0x00020013, 0x00000002, 0x00030021,
    0x00000003, 0x00000002, 0x00030016, 0x00000006,
    0x00000020, 0x00040017, 0x00000007, 0x00000006,
    0x00000002, 0x00040020, 0x00000008, 0x00000007,
    0x00000007, 0x00040015, 0x0000000a, 0x00000020,
    0x00000001, 0x00040020, 0x0000000b, 0x00000001,
    0x0000000a, 0x0004003b, 0x0000000b, 0x0000000c,
    0x00000001, 0x0004002b, 0x0000000a, 0x0000000e,
    0x00000002, 0x0004002b, 0x0000000a, 0x00000010,
    0x00000000, 0x00020014, 0x00000011, 0x00040020,
    0x00000013, 0x00000007, 0x00000006, 0x00040017,
    0x00000017, 0x00000006, 0x00000004, 0x0004001e,
    0x00000018, 0x00000017, 0x00000017, 0x00040020,
    0x00000019, 0x00000009, 0x00000018, 0x0004003b,
    0x00000019, 0x0000001a, 0x00000009, 0x0004002b,
    0x0000000a, 0x0000001b, 0x00000001, 0x00040015,
    0x0000001c, 0x00000020, 0x00000000, 0x0004002b,
    0x0000001c, 0x0000001d, 0x00000000, 0x00040020,
    0x0000001e, 0x00000009, 0x00000006, 0x0004002b,
    0x0000001c, 0x00000022, 0x00000002, 0x0004002b,
    0x0000001c, 0x0000002c, 0x00000001, 0x0004002b,
    0x0000001c, 0x00000030, 0x00000003, 0x0004001c,
    0x0000004f, 0x00000006, 0x0000002c, 0x0006001e,
    0x00000050, 0x00000017, 0x00000006, 0x0000004f,
    0x0000004f, 0x00040020, 0x00000051, 0x00000003,
    0x00000050, 0x0004003b, 0x00000051, 0x00000052,
    0x00000003, 0x0004002b, 0x00000006, 0x00000054,
    0x00000000, 0x0004002b, 0x00000006, 0x00000055,
    0x3f800000, 0x00040020, 0x00000059, 0x00000003,
    0x00000017, 0x00040020, 0x0000005b, 0x00000003,
    0x00000007, 0x0004003b, 0x0000005b, 0x0000005c,
    0x00000003, 0x00050036, 0x00000002, 0x00000004,
    0x00000000, 0x00000003, 0x000200f8, 0x00000005,
    0x0004003b, 0x00000008, 0x00000009, 0x00000007,
    0x0004003b, 0x00000013, 0x00000014, 0x00000007,
    0x0004003b, 0x00000013, 0x00000029, 0x00000007,
    0x0004003b, 0x00000008, 0x00000035, 0x00000007,
    0x0004003b, 0x00000013, 0x00000039, 0x00000007,
    0x0004003b, 0x00000013, 0x00000045, 0x00000007,
    0x0004003d, 0x0000000a, 0x0000000d, 0x0000000c,
    0x000500c7, 0x0000000a, 0x0000000f, 0x0000000d,
    0x0000000e, 0x000500aa, 0x00000011, 0x00000012,
    0x0000000f, 0x00000010, 0x000300f7, 0x00000016,
    0x00000000, 0x000400fa, 0x00000012, 0x00000015,
    0x00000021, 0x000200f8, 0x00000015, 0x00060041,
    0x0000001e, 0x0000001f, 0x0000001a, 0x0000001b,
    0x0000001d, 0x0004003d, 0x00000006, 0x00000020,
    0x0000001f, 0x0003003e, 0x00000014, 0x00000020,
    0x000200f9, 0x00000016, 0x000200f8, 0x00000021,
    0x00060041, 0x0000001e, 0x00000023, 0x0000001a,
    0x0000001b, 0x00000022, 0x0004003d, 0x00000006,
    0x00000024, 0x00000023, 0x0003003e, 0x00000014,
    0x00000024, 0x000200f9, 0x00000016, 0x000200f8,
    0x00000016, 0x0004003d, 0x00000006, 0x00000025,
    0x00000014, 0x0004003d, 0x0000000a, 0x00000026,
    0x0000000c, 0x000500c7, 0x0000000a, 0x00000027,
    0x00000026, 0x0000001b, 0x000500aa, 0x00000011,
    0x00000028, 0x00000027, 0x00000010, 0x000300f7,
    0x0000002b, 0x00000000, 0x000400fa, 0x00000028,
    0x0000002a, 0x0000002f, 0x000200f8, 0x0000002a,
    0x00060041, 0x0000001e, 0x0000002d, 0x0000001a,
    0x0000001b, 0x0000002c, 0x0004003d, 0x00000006,
    0x0000002e, 0x0000002d, 0x0003003e, 0x00000029,
    0x0000002e, 0x000200f9, 0x0000002b, 0x000200f8,
    0x0000002f, 0x00060041, 0x0000001e, 0x00000031,
    0x0000001a, 0x0000001b, 0x00000030, 0x0004003d,
    0x00000006, 0x00000032, 0x00000031, 0x0003003e,
    0x00000029, 0x00000032, 0x000200f9, 0x0000002b,
    0x000200f8, 0x0000002b, 0x0004003d, 0x00000006,
    0x00000033, 0x00000029, 0x00050050, 0x00000007,
    0x00000034, 0x00000025, 0x00000033, 0x0003003e,
    0x00000009, 0x00000034, 0x0004003d, 0x0000000a,
    0x00000036, 0x0000000c, 0x000500c7, 0x0000000a,
    0x00000037, 0x00000036, 0x0000000e, 0x000500aa,
    0x00000011, 0x00000038, 0x00000037, 0x00000010,
    0x000300f7, 0x0000003b, 0x00000000, 0x000400fa,
    0x00000038, 0x0000003a, 0x0000003e, 0x000200f8,
    0x0000003a, 0x00060041, 0x0000001e, 0x0000003c,
    0x0000001a, 0x00000010, 0x0000001d, 0x0004003d,
    0x00000006, 0x0000003d, 0x0000003c, 0x0003003e,
    0x00000039, 0x0000003d, 0x000200f9, 0x0000003b,
    0x000200f8, 0x0000003e, 0x00060041, 0x0000001e,
    0x0000003f, 0x0000001a, 0x00000010, 0x00000022,
    0x0004003d, 0x00000006, 0x00000040, 0x0000003f,
    0x0003003e, 0x00000039, 0x00000040, 0x000200f9,
    0x0000003b, 0x000200f8, 0x0000003b, 0x0004003d,
    0x00000006, 0x00000041, 0x00000039, 0x0004003d,
    0x0000000a, 0x00000042, 0x0000000c, 0x000500c7,
    0x0000000a, 0x00000043, 0x00000042, 0x0000001b,
    0x000500aa, 0x00000011, 0x00000044, 0x00000043,
    0x00000010, 0x000300f7, 0x00000047, 0x00000000,
    0x000400fa, 0x00000044, 0x00000046, 0x0000004a,
    0x000200f8, 0x00000046, 0x00060041, 0x0000001e,
    0x00000048, 0x0000001a, 0x00000010, 0x0000002c,
    0x0004003d, 0x00000006, 0x00000049, 0x00000048,
    0x0003003e, 0x00000045, 0x00000049, 0x000200f9,
    0x00000047, 0x000200f8, 0x0000004a, 0x00060041,
    0x0000001e, 0x0000004b, 0x0000001a, 0x00000010,
    0x00000030, 0x0004003d, 0x00000006, 0x0000004c,
    0x0000004b, 0x0003003e, 0x00000045, 0x0000004c,
    0x000200f9, 0x00000047, 0x000200f8, 0x00000047,
    0x0004003d, 0x00000006, 0x0000004d, 0x00000045,
    0x00050050, 0x00000007, 0x0000004e, 0x00000041,
    0x0000004d, 0x0003003e, 0x00000035, 0x0000004e,
    0x0004003d, 0x00000007, 0x00000053, 0x00000009,
    0x00050051, 0x00000006, 0x00000056, 0x00000053,
    0x00000000, 0x00050051, 0x00000006, 0x00000057,
    0x00000053, 0x00000001, 0x00070050, 0x00000017,
    0x00000058, 0x00000056, 0x00000057, 0x00000054,
    0x00000055, 0x00050041, 0x00000059, 0x0000005a,
    0x00000052, 0x00000010, 0x0003003e, 0x0000005a,
    0x00000058, 0x0004003d, 0x00000007, 0x0000005d,
    0x00000035, 0x0003003e, 0x0000005c, 0x0000005d,
    0x000100fd, 0x00010038
};
const size_t VKDrawingPanel::kVertSpvSize = sizeof(kVertSpv);
         
// Frag: 464 bytes
const uint32_t VKDrawingPanel::kFragSpv[] = {
    0x07230203u, 0x00010300u, 0x000D000Au, 0x0000001Eu, 0x00000000u, 0x00020011u,
    0x00000001u, 0x0003000Eu, 0x00000000u, 0x00000001u, 0x0007000Fu, 0x00000004u,
    0x0000000Eu, 0x6E69616Du, 0x00000000u, 0x0000000Cu, 0x0000000Du, 0x00030010u,
    0x0000000Eu, 0x00000007u, 0x00030003u, 0x00000002u, 0x000001C2u, 0x00040047u,
    0x0000000Bu, 0x00000022u, 0x00000000u, 0x00040047u, 0x0000000Bu, 0x00000021u,
    0x00000000u, 0x00040047u, 0x0000000Cu, 0x0000001Eu, 0x00000000u, 0x00040047u,
    0x0000000Du, 0x0000001Eu, 0x00000000u, 0x00020013u, 0x00000001u, 0x00030021u,
    0x00000002u, 0x00000001u, 0x00030016u, 0x00000003u, 0x00000020u, 0x00040017u,
    0x00000004u, 0x00000003u, 0x00000002u, 0x00040017u, 0x00000005u, 0x00000003u,
    0x00000004u, 0x00090019u, 0x00000006u, 0x00000003u, 0x00000001u, 0x00000000u,
    0x00000000u, 0x00000000u, 0x00000001u, 0x00000000u, 0x0003001Bu, 0x00000007u,
    0x00000006u, 0x00040020u, 0x00000008u, 0x00000000u, 0x00000007u, 0x00040020u,
    0x00000009u, 0x00000001u, 0x00000004u, 0x00040020u, 0x0000000Au, 0x00000003u,
    0x00000005u, 0x0004003Bu, 0x00000008u, 0x0000000Bu, 0x00000000u, 0x0004003Bu,
    0x00000009u, 0x0000000Cu, 0x00000001u, 0x0004003Bu, 0x0000000Au, 0x0000000Du,
    0x00000003u, 0x00050036u, 0x00000001u, 0x0000000Eu, 0x00000000u, 0x00000002u,
    0x000200F8u, 0x00000014u, 0x0004003Du, 0x00000007u, 0x00000015u, 0x0000000Bu,
    0x0004003Du, 0x00000004u, 0x00000016u, 0x0000000Cu, 0x00050057u, 0x00000005u,
    0x00000017u, 0x00000015u, 0x00000016u, 0x0003003Eu, 0x0000000Du, 0x00000017u,
    0x000100FDu, 0x00010038u,
};
const size_t VKDrawingPanel::kFragSpvSize = sizeof(kFragSpv);
         
// ─── Constructor ──────────────────────────────────────────────────────────────
#if !defined(NO_VULKAN) && (defined(__WXMSW__) || defined(__WXGTK__))
// ── Run-time Vulkan loader (volk-style) ──────────────────────────────────────
// On Windows and Linux we do not link the Vulkan import library; vulkan.h is
// included with VK_NO_PROTOTYPES (drawing.h) and the functions we use are
// resolved at run time from the system loader via wxDynamicLibrary +
// vkGetInstanceProcAddr. This keeps the loader out of the import table (never a
// load-time dependency, never bundled / app-local-copied) and lets the app run
// on machines without Vulkan, with the renderer simply reported unavailable.

// Bootstrap entry point, fetched from the loader library by symbol.
static PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = nullptr;

// Global-level functions, resolvable before any instance exists.
#define VBAM_VK_GLOBAL_FUNCS(F) \
    F(vkCreateInstance) \
    F(vkEnumerateInstanceExtensionProperties)

// Instance- and device-level functions, resolved once an instance exists
// (vkGetInstanceProcAddr returns working dispatch for device-level entries too).
#define VBAM_VK_INSTANCE_FUNCS(F) \
    F(vkAcquireNextImageKHR) \
    F(vkAllocateCommandBuffers) \
    F(vkAllocateDescriptorSets) \
    F(vkAllocateMemory) \
    F(vkBeginCommandBuffer) \
    F(vkBindBufferMemory) \
    F(vkBindImageMemory) \
    F(vkCmdBeginRenderPass) \
    F(vkCmdBindDescriptorSets) \
    F(vkCmdBindPipeline) \
    F(vkCmdClearColorImage) \
    F(vkCmdCopyBufferToImage) \
    F(vkCmdDraw) \
    F(vkCmdEndRenderPass) \
    F(vkCmdPipelineBarrier) \
    F(vkCmdPushConstants) \
    F(vkCmdSetScissor) \
    F(vkCmdSetViewport) \
    F(vkCreateBuffer) \
    F(vkCreateCommandPool) \
    F(vkCreateDescriptorPool) \
    F(vkCreateDescriptorSetLayout) \
    F(vkCreateDevice) \
    F(vkCreateFence) \
    F(vkCreateFramebuffer) \
    F(vkCreateGraphicsPipelines) \
    F(vkCreateImage) \
    F(vkCreateImageView) \
    F(vkCreatePipelineLayout) \
    F(vkCreateRenderPass) \
    F(vkCreateSampler) \
    F(vkCreateSemaphore) \
    F(vkCreateShaderModule) \
    F(vkCreateSwapchainKHR) \
    F(vkDestroyBuffer) \
    F(vkDestroyCommandPool) \
    F(vkDestroyDescriptorPool) \
    F(vkDestroyDescriptorSetLayout) \
    F(vkDestroyDevice) \
    F(vkDestroyFence) \
    F(vkDestroyFramebuffer) \
    F(vkDestroyImage) \
    F(vkDestroyImageView) \
    F(vkDestroyInstance) \
    F(vkDestroyPipeline) \
    F(vkDestroyPipelineLayout) \
    F(vkDestroyRenderPass) \
    F(vkDestroySampler) \
    F(vkDestroySemaphore) \
    F(vkDestroyShaderModule) \
    F(vkDestroySurfaceKHR) \
    F(vkDestroySwapchainKHR) \
    F(vkDeviceWaitIdle) \
    F(vkEndCommandBuffer) \
    F(vkEnumerateDeviceExtensionProperties) \
    F(vkEnumeratePhysicalDevices) \
    F(vkFreeMemory) \
    F(vkGetBufferMemoryRequirements) \
    F(vkGetDeviceProcAddr) \
    F(vkGetDeviceQueue) \
    F(vkGetImageMemoryRequirements) \
    F(vkGetPhysicalDeviceMemoryProperties) \
    F(vkGetPhysicalDeviceProperties) \
    F(vkGetPhysicalDeviceQueueFamilyProperties) \
    F(vkGetPhysicalDeviceSurfaceCapabilitiesKHR) \
    F(vkGetPhysicalDeviceSurfaceFormatsKHR) \
    F(vkGetPhysicalDeviceSurfacePresentModesKHR) \
    F(vkGetPhysicalDeviceSurfaceSupportKHR) \
    F(vkGetSwapchainImagesKHR) \
    F(vkMapMemory) \
    F(vkQueuePresentKHR) \
    F(vkQueueSubmit) \
    F(vkResetCommandBuffer) \
    F(vkResetFences) \
    F(vkSetHdrMetadataEXT) \
    F(vkUnmapMemory) \
    F(vkUpdateDescriptorSets) \
    F(vkWaitForFences)

// Platform surface-creation functions (instance-level).
#if defined(__WXMSW__)
#define VBAM_VK_SURFACE_FUNCS(F) F(vkCreateWin32SurfaceKHR)
#elif defined(__WXGTK__)
#define VBAM_VK_SURFACE_FUNCS(F) \
    F(vkCreateWaylandSurfaceKHR) \
    F(vkCreateXlibSurfaceKHR)
#else
#define VBAM_VK_SURFACE_FUNCS(F)
#endif

#define VBAM_VK_DECLARE(name) static PFN_##name name = nullptr;
VBAM_VK_GLOBAL_FUNCS(VBAM_VK_DECLARE)
VBAM_VK_INSTANCE_FUNCS(VBAM_VK_DECLARE)
VBAM_VK_SURFACE_FUNCS(VBAM_VK_DECLARE)
#undef VBAM_VK_DECLARE

// Load the loader library and the global-level functions. Cached; returns false
// (Vulkan unavailable) if the loader or any required global symbol is missing.
static bool VbamVulkanBootstrap() {
    static int state = 0;  // 0 = untried, 1 = ok, -1 = failed
    if (state != 0) return state == 1;

    static wxDynamicLibrary vklib;
    static const wxChar* const kNames[] = {
#if defined(__WXMSW__)
        wxT("vulkan-1.dll"),
#else
        wxT("libvulkan.so.1"), wxT("libvulkan.so"),
#endif
    };
    for (const wxChar* n : kNames) {
        if (vklib.Load(n, wxDL_VERBATIM | wxDL_QUIET))
            break;
    }
    if (!vklib.IsLoaded()) { state = -1; return false; }

    vkGetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
        vklib.GetSymbol(wxT("vkGetInstanceProcAddr")));
    if (!vkGetInstanceProcAddr) { state = -1; return false; }

#define VBAM_VK_LOAD_GLOBAL(name) \
    name = reinterpret_cast<PFN_##name>(vkGetInstanceProcAddr(nullptr, #name)); \
    if (!name) { state = -1; return false; }
    VBAM_VK_GLOBAL_FUNCS(VBAM_VK_LOAD_GLOBAL)
#undef VBAM_VK_LOAD_GLOBAL

    state = 1;
    return true;
}

// Resolve the instance-/device-level functions once an instance exists. Must be
// called immediately after each successful vkCreateInstance.
static void VbamVulkanLoadInstanceFns(VkInstance instance) {
#define VBAM_VK_LOAD_INSTANCE(name) \
    name = reinterpret_cast<PFN_##name>(vkGetInstanceProcAddr(instance, #name));
    VBAM_VK_INSTANCE_FUNCS(VBAM_VK_LOAD_INSTANCE)
    VBAM_VK_SURFACE_FUNCS(VBAM_VK_LOAD_INSTANCE)
#undef VBAM_VK_LOAD_INSTANCE
}
#endif  // dynamic Vulkan (Windows / Linux)

VKDrawingPanel::VKDrawingPanel(wxWindow* parent, int _width, int _height)
    : DrawingPanel(parent, _width, _height)
{
    memset(delta, 0xff, sizeof(delta));
 
    if (OPTION(kDispFilter) == config::Filter::kNone &&
        OPTION(kDispIFB)    == config::Interframe::kNone) {
        systemColorDepth = (OPTION(kBitDepth) + 1) << 3;
    }
 
    vsync_ = OPTION(kPrefVsync);
 
    if (!CreateInstance())    { return; }

#ifdef __WXMSW__
    if (!CreateSurfaceWIN32()){ return; }
#elif defined(__WXMAC__)
    if (!CreateSurfaceMACOS()){ return; }
#elif defined(__WXGTK__)
    if (!CreateSurfaceUNIX()) { return; }
#else
#error "Must be GTK, macOS or Windows"
#endif

    if (!PickPhysicalDevice())          { return; }
    if (!CreateLogicalDevice())         { return; }
    if (!CreateSwapchain())             { return; }
    if (!CreateImageViews())            { return; }
    if (!CreateRenderPass())            { return; }
    if (!CreateDescriptorSetLayout())   { return; }
    if (!CreateGraphicsPipeline())      { return; }
    if (!CreateFramebuffers())          { return; }
    if (!CreateCommandPool())           { return; }
    if (!CreateCommandBuffers())        { return; }
    if (!CreateSyncObjects())           { return; }
    if (!CreateDescriptorPoolAndSet())  { return; }

    wxLogDebug(_("Vulkan device created successfully"));
}
 
// ─── Destructor ───────────────────────────────────────────────────────────────
VKDrawingPanel::~VKDrawingPanel()
{
    if (device_ != VK_NULL_HANDLE)
        vkDeviceWaitIdle(device_);
 
    DestroyTexture();
 
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        if (image_available_sem_[i]) vkDestroySemaphore(device_, image_available_sem_[i], nullptr);
        if (render_finished_sem_[i]) vkDestroySemaphore(device_, render_finished_sem_[i], nullptr);
        if (in_flight_fence_[i])     vkDestroyFence    (device_, in_flight_fence_[i],     nullptr);
    }
 
    if (cmd_pool_) vkDestroyCommandPool(device_, cmd_pool_, nullptr);
 
    DestroySwapchain();
 
    if (desc_pool_)       vkDestroyDescriptorPool     (device_, desc_pool_,       nullptr);
    if (pipeline_)        vkDestroyPipeline            (device_, pipeline_,        nullptr);
    if (pipeline_layout_) vkDestroyPipelineLayout      (device_, pipeline_layout_, nullptr);
    if (desc_set_layout_) vkDestroyDescriptorSetLayout (device_, desc_set_layout_, nullptr);
    if (render_pass_)     vkDestroyRenderPass          (device_, render_pass_,     nullptr);
 
    if (device_)   vkDestroyDevice      (device_,            nullptr);
    if (surface_)  vkDestroySurfaceKHR  (instance_, surface_, nullptr);
    if (instance_) vkDestroyInstance    (instance_,           nullptr);

#if defined(__WXGTK__) && !defined(NO_WAYLAND)
    // Tear down the Wayland subsurface after the Vulkan surface that used it.
    DestroyWaylandSubsurface();
#endif
}
 
// ─── CreateInstance ───────────────────────────────────────────────────────────
bool VKDrawingPanel::CreateInstance()
{
#if !defined(NO_VULKAN) && (defined(__WXMSW__) || defined(__WXGTK__))
    // Resolve the Vulkan loader before any vk* call; bail if it is absent.
    if (!VbamVulkanBootstrap())
        return false;
#endif
    VkApplicationInfo app_info{};
    app_info.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName   = "VBAm";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName        = "VBAm";
    app_info.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion         = VK_API_VERSION_1_0;
 
    std::vector<const char*> extensions = { VK_KHR_SURFACE_EXTENSION_NAME };
 
#ifdef __WXMSW__
    extensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#elif defined(__WXMAC__)
    uint32_t ext_count = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &ext_count, nullptr);
    std::vector<VkExtensionProperties> inst_exts(ext_count);
    vkEnumerateInstanceExtensionProperties(nullptr, &ext_count, inst_exts.data());

    // Modern MoltenVK exposes VK_EXT_metal_surface (via the portability
    // enumeration layer).  The old VK_MVK_macos_surface is deprecated and
    // absent on Apple Silicon / MoltenVK > 1.2.
    extensions.push_back(VK_EXT_METAL_SURFACE_EXTENSION_NAME);

    auto has_ext = [&](const char* name) {
        for (auto& e : inst_exts)
            if (strcmp(e.extensionName, name) == 0) return true;
        return false;
    };

    if (has_ext(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME))
        extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);

    // Required companion for MoltenVK portability
    if (has_ext(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME))
        extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
#elif defined(__WXGTK__)
    // ── Probe which surface extensions the instance actually supports ─────────
    // Requesting an extension that isn't present causes vkCreateInstance to
    // fail with VK_ERROR_EXTENSION_NOT_PRESENT.  We must not blindly add both
    // Wayland and Xlib — only the ones the loader exposes.
    {
        uint32_t ext_count = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &ext_count, nullptr);
        std::vector<VkExtensionProperties> inst_exts(ext_count);
        vkEnumerateInstanceExtensionProperties(nullptr, &ext_count, inst_exts.data());
 
        auto has_ext = [&](const char* name) {
            for (auto& e : inst_exts)
                if (strcmp(e.extensionName, name) == 0) return true;
            return false;
        };
 
#ifndef NO_WAYLAND
        bool have_wayland = has_ext(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME);
#else
        bool have_wayland = false;
#endif

        bool have_xlib    = has_ext(VK_KHR_XLIB_SURFACE_EXTENSION_NAME);
 
        if (!have_wayland && !have_xlib) {
            wxLogError(_("Neither VK_KHR_wayland_surface nor VK_KHR_xlib_surface "
                         "is available — cannot create Vulkan surface on this system"));
            return false;
        }
 
        // Store which backends we can use; CreateSurfaceUNIX() reads these.
        have_wayland_surface_ = have_wayland;
        have_xlib_surface_    = have_xlib;
 
#ifndef NO_WAYLAND
        if (have_wayland) extensions.push_back(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME);
#endif

        if (have_xlib)    extensions.push_back(VK_KHR_XLIB_SURFACE_EXTENSION_NAME);
    }
#endif
 
    // Enable extra swapchain color spaces (HDR10 PQ, scRGB, ...) when the
    // loader exposes them. Required to enumerate HDR surface formats below.
    {
        uint32_t n = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &n, nullptr);
        std::vector<VkExtensionProperties> all(n);
        vkEnumerateInstanceExtensionProperties(nullptr, &n, all.data());
        for (auto& e : all) {
            if (strcmp(e.extensionName,
                       VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME) == 0) {
                extensions.push_back(VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME);
                break;
            }
        }
    }

    VkInstanceCreateInfo ci{};
    ci.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ci.pApplicationInfo        = &app_info;
    ci.enabledExtensionCount   = static_cast<uint32_t>(extensions.size());
    ci.ppEnabledExtensionNames = extensions.data();
 
#ifdef __WXMAC__
    ci.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif
 
    VkResult res = vkCreateInstance(&ci, nullptr, &instance_);
    if (res != VK_SUCCESS) {
        wxLogError(_("Failed to create Vulkan instance: %d"), (int)res);
        return false;
    }
    wxLogDebug(_("Vulkan instance created"));
#if !defined(NO_VULKAN) && (defined(__WXMSW__) || defined(__WXGTK__))
    // Resolve instance-/device-level entry points now that the instance exists.
    VbamVulkanLoadInstanceFns(instance_);
#endif
    return true;
}
 
#ifdef __WXMSW__
// ─── CreateSurface (Win32) ────────────────────────────────────────────────────
bool VKDrawingPanel::CreateSurfaceWIN32()
{
    VkWin32SurfaceCreateInfoKHR ci{};
    ci.sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    ci.hwnd      = (HWND)GetHandle();
    ci.hinstance = GetModuleHandle(nullptr);
 
    VkResult res = vkCreateWin32SurfaceKHR(instance_, &ci, nullptr, &surface_);
    if (res != VK_SUCCESS) {
        wxLogError(_("Failed to create Vulkan Win32 surface: %d"), (int)res);
        return false;
    }
    return true;
}
 
#elif defined(__WXMAC__)
extern "C" void* VKBEnsureMetalLayer(void* ns_view_ptr);

// ─── CreateSurface (macOS / MoltenVK) ────────────────────────────────────────
//
// FIX 1: Was incorrectly using wxGetApp().frame->GetPanel() — i.e. a sibling
//         widget — instead of 'this' (the VKDrawingPanel itself).
// FIX 2: Switched from the deprecated VK_MVK_macos_surface path to the modern
//         VK_EXT_metal_surface path, which works on both Intel and Apple Silicon
//         with MoltenVK ≥ 1.2 and requires only the CAMetalLayer pointer.
// FIX 3: Error message previously said "Win32 surface" (copy-paste).
bool VKDrawingPanel::CreateSurfaceMACOS()
{
    // wxWidgets on macOS: GetHandle() returns the NSView*.
    // MoltenVK will create (or reuse) a CAMetalLayer backing it automatically.
    void* ns_view = VKBEnsureMetalLayer(GetHandle());   // 'this' is the VKDrawingPanel
    if (!ns_view) {
        wxLogError(_("Failed to obtain NSView handle for Vulkan surface"));
        return false;
    }
 
    VkMetalSurfaceCreateInfoEXT ci{};
    ci.sType  = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT;
    // pLayer accepts an NSView* directly; MoltenVK extracts the CAMetalLayer.
    ci.pLayer = ns_view;
 
    VkResult res = vkCreateMetalSurfaceEXT(instance_, &ci, nullptr, &surface_);
    if (res != VK_SUCCESS) {
        wxLogError(_("Failed to create Vulkan Metal surface: %d"), (int)res);
        return false;
    }
    return true;
}
 
#elif defined(__WXGTK__)
// ─── CreateSurface (Xlib) ────────────────────────────────────────────────────
// FIX: Error message previously said "Win32 surface" (copy-paste).
bool VKDrawingPanel::CreateSurfaceXLIB(Window win)
{
    VkXlibSurfaceCreateInfoKHR ci{};
    ci.sType  = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
    ci.dpy    = GetX11Display();
    ci.window = win;
 
    VkResult res = vkCreateXlibSurfaceKHR(instance_, &ci, nullptr, &surface_);
    if (res != VK_SUCCESS) {
        wxLogError(_("Failed to create Vulkan Xlib surface: %d"), (int)res);
        return false;
    }
    return true;
}
 
#ifndef NO_WAYLAND
// ─── Wayland subsurface for Vulkan ────────────────────────────────────────────
// GTK owns and renders to the toplevel wl_surface, so Vulkan must present to a
// dedicated child surface. Bind wl_compositor/wl_subcompositor from the display
// registry, then create a desync subsurface parented under GTK's surface.
namespace {
struct VbamWlGlobals {
    struct wl_compositor*    comp    = nullptr;
    struct wl_subcompositor* subcomp = nullptr;
};
void vbam_wl_global(void* data, struct wl_registry* reg, uint32_t name,
                    const char* iface, uint32_t /*version*/) {
    auto* g = static_cast<VbamWlGlobals*>(data);
    if (strcmp(iface, "wl_compositor") == 0)
        g->comp = static_cast<struct wl_compositor*>(
            wl_registry_bind(reg, name, &wl_compositor_interface, 1));
    else if (strcmp(iface, "wl_subcompositor") == 0)
        g->subcomp = static_cast<struct wl_subcompositor*>(
            wl_registry_bind(reg, name, &wl_subcompositor_interface, 1));
}
void vbam_wl_global_remove(void*, struct wl_registry*, uint32_t) {}
const struct wl_registry_listener kVbamWlRegistryListener = {
    vbam_wl_global, vbam_wl_global_remove
};
}  // namespace

void VKDrawingPanel::PositionWaylandSubsurface()
{
    if (!wl_subsurface_)
        return;
    GtkWidget* widget   = static_cast<GtkWidget*>(GetHandle());
    GtkWidget* toplevel = gtk_widget_get_toplevel(widget);
    int x = 0, y = 0;
    gtk_widget_translate_coordinates(widget, toplevel, 0, 0, &x, &y);
    wl_subsurface_set_position(wl_subsurface_, x, y);
}

struct wl_surface* VKDrawingPanel::CreateWaylandSubsurface(struct wl_display* dpy,
                                                           struct wl_surface* parent)
{
    if (!wl_compositor_ || !wl_subcompositor_) {
        VbamWlGlobals g;
        struct wl_registry* reg = wl_display_get_registry(dpy);
        wl_registry_add_listener(reg, &kVbamWlRegistryListener, &g);
        wl_display_roundtrip(dpy);
        wl_registry_destroy(reg);
        wl_compositor_    = g.comp;
        wl_subcompositor_ = g.subcomp;
    }
    if (!wl_compositor_ || !wl_subcompositor_) {
        wxLogError(_("Wayland: wl_compositor/wl_subcompositor unavailable; "
                     "cannot create Vulkan subsurface"));
        return nullptr;
    }

    wl_child_surface_ = wl_compositor_create_surface(wl_compositor_);
    wl_subsurface_    = wl_subcompositor_get_subsurface(
        wl_subcompositor_, wl_child_surface_, parent);
    if (!wl_child_surface_ || !wl_subsurface_) {
        wxLogError(_("Wayland: failed to create Vulkan subsurface"));
        DestroyWaylandSubsurface();
        return nullptr;
    }

    // Desync so Vulkan present commits take effect without a parent commit.
    wl_subsurface_set_desync(wl_subsurface_);
    PositionWaylandSubsurface();

    // Don't intercept input -- let GTK keep handling pointer/keyboard.
    struct wl_region* empty = wl_compositor_create_region(wl_compositor_);
    wl_surface_set_input_region(wl_child_surface_, empty);
    wl_region_destroy(empty);
    wl_surface_commit(wl_child_surface_);

    return wl_child_surface_;
}

void VKDrawingPanel::DestroyWaylandSubsurface()
{
    if (wl_subsurface_)    { wl_subsurface_destroy(wl_subsurface_);    wl_subsurface_ = nullptr; }
    if (wl_child_surface_) { wl_surface_destroy(wl_child_surface_);    wl_child_surface_ = nullptr; }
    if (wl_subcompositor_) { wl_subcompositor_destroy(wl_subcompositor_); wl_subcompositor_ = nullptr; }
    if (wl_compositor_)    { wl_compositor_destroy(wl_compositor_);    wl_compositor_ = nullptr; }
}

// ─── CreateSurface (Wayland) ──────────────────────────────────────────────────
// FIX: Error message previously said "Win32 surface" (copy-paste).
bool VKDrawingPanel::CreateSurfaceWAYLAND(struct wl_surface* wayland_surface,
                                           struct wl_display* wayland_display)
{
    VkWaylandSurfaceCreateInfoKHR ci{};
    ci.sType   = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
    ci.display = wayland_display;
    ci.surface = wayland_surface;
 
    VkResult res = vkCreateWaylandSurfaceKHR(instance_, &ci, nullptr, &surface_);
    if (res != VK_SUCCESS) {
        wxLogError(_("Failed to create Vulkan Wayland surface: %d"), (int)res);
        return false;
    }
    return true;
}
#endif
 
// ─── CreateSurfaceUNIX ────────────────────────────────────────────────────────
// FIX 1: Was calling wxGetApp().frame->GetPanel() (a sibling widget) instead
//         of 'this'.  The GDK window for 'this' is the one we need to present
//         into — using any other widget's window produces a surface/swapchain
//         mismatch at present time.
// FIX 2: Wayland surface creation is now guarded by have_wayland_surface_,
//         which was set during CreateInstance() by probing the instance
//         extensions.  If the loader doesn't expose VK_KHR_wayland_surface we
//         must not call vkCreateWaylandSurfaceKHR even if GDK says we're on
//         Wayland — the function pointer will be null.
// FIX 3: Added gdk_display_flush() so that the native Wayland wl_surface is
//         fully committed before we hand it to Vulkan.
bool VKDrawingPanel::CreateSurfaceUNIX()
{
    // 'this' is a wxWindow subclass; GetHandle() returns the GtkWidget*.
    GtkWidget* widget = static_cast<GtkWidget*>(GetHandle());
    gtk_widget_realize(widget);
 
    GdkWindow*  gdk_win = gtk_widget_get_window(widget);
    GdkDisplay* gdk_dpy = gtk_widget_get_display(widget);
 
    // Flush so the compositor has processed all pending wl_surface commits.
    gdk_display_flush(gdk_dpy);
 
#ifndef NO_WAYLAND
    if (have_wayland_surface_ && GDK_IS_WAYLAND_WINDOW(gdk_win)) {
        struct wl_display* wl_dpy =
            gdk_wayland_display_get_wl_display(gdk_dpy);
        struct wl_surface* parent =
            gdk_wayland_window_get_wl_surface(gdk_win);

        if (!wl_dpy || !parent) {
            wxLogError(_("Failed to obtain Wayland display/surface handles"));
            return false;
        }
        // Present to a dedicated child subsurface, not GTK's own surface.
        struct wl_surface* child = CreateWaylandSubsurface(wl_dpy, parent);
        if (!child)
            return false;
        return CreateSurfaceWAYLAND(child, wl_dpy);
    } else if (have_xlib_surface_) {
#endif
        XID xid = GDK_WINDOW_XID(gdk_win);
        return CreateSurfaceXLIB(xid);
#ifndef NO_WAYLAND
    }
#endif
 
    // have_wayland_surface_ and have_xlib_surface_ were both false — already
    // caught in CreateInstance(), but be defensive.
    wxLogError(_("No supported Vulkan surface extension available"));
    return false;
}
#endif // platform surface creation
 
// ─── PickPhysicalDevice ───────────────────────────────────────────────────────
bool VKDrawingPanel::PickPhysicalDevice()
{
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(instance_, &count, nullptr);
    if (count == 0) {
        wxLogError(_("No Vulkan physical devices found"));
        return false;
    }
    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(instance_, &count, devices.data());
 
    const char* req_ext = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
 
    for (auto& pd : devices) {
        uint32_t qcount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(pd, &qcount, nullptr);
        std::vector<VkQueueFamilyProperties> qprops(qcount);
        vkGetPhysicalDeviceQueueFamilyProperties(pd, &qcount, qprops.data());
 
        uint32_t gfx = UINT32_MAX, prs = UINT32_MAX;
        for (uint32_t i = 0; i < qcount; ++i) {
            if (qprops[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
                gfx = i;
            VkBool32 present_sup = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(pd, i, surface_, &present_sup);
            if (present_sup)
                prs = i;
            if (gfx != UINT32_MAX && prs != UINT32_MAX)
                break;
        }
        if (gfx == UINT32_MAX || prs == UINT32_MAX)
            continue;
 
        uint32_t ext_count = 0;
        vkEnumerateDeviceExtensionProperties(pd, nullptr, &ext_count, nullptr);
        std::vector<VkExtensionProperties> exts(ext_count);
        vkEnumerateDeviceExtensionProperties(pd, nullptr, &ext_count, exts.data());
        bool has_swapchain = false;
        bool has_hdr_metadata = false;
        for (auto& e : exts) {
            if (strcmp(e.extensionName, req_ext) == 0) has_swapchain = true;
            else if (strcmp(e.extensionName,
                            VK_EXT_HDR_METADATA_EXTENSION_NAME) == 0)
                has_hdr_metadata = true;
        }
        if (!has_swapchain)
            continue;
 
        uint32_t fmt_count = 0, mode_count = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(pd, surface_, &fmt_count, nullptr);
        vkGetPhysicalDeviceSurfacePresentModesKHR(pd, surface_, &mode_count, nullptr);
        if (fmt_count == 0 || mode_count == 0)
            continue;
 
        physical_device_ = pd;
        graphics_family_ = gfx;
        present_family_  = prs;
        // VK_EXT_hdr_metadata lets us hand the compositor the content's
        // luminance envelope (vkSetHdrMetadataEXT) so it tone-maps our peak to
        // the display peak instead of assuming PQ's 10000-nit default range.
        hdr_metadata_ext_ = has_hdr_metadata;

        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(pd, &props);
        wxLogDebug(_("Selected Vulkan device: %s"), props.deviceName);
        return true;
    }
 
    wxLogError(_("No suitable Vulkan physical device found"));
    return false;
}
 
// ─── CreateLogicalDevice ──────────────────────────────────────────────────────
bool VKDrawingPanel::CreateLogicalDevice()
{
    float priority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> queue_cis;
 
    auto add_queue = [&](uint32_t family) {
        VkDeviceQueueCreateInfo ci{};
        ci.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        ci.queueFamilyIndex = family;
        ci.queueCount       = 1;
        ci.pQueuePriorities = &priority;
        queue_cis.push_back(ci);
    };
 
    add_queue(graphics_family_);
    if (present_family_ != graphics_family_)
        add_queue(present_family_);
 
    std::vector<const char*> dev_exts = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    // Enable HDR luminance metadata when the device supports it (detected in
    // PickPhysicalDevice). CreateSwapchain uses it to declare our content
    // luminance envelope on an HDR10 swapchain.
    if (hdr_metadata_ext_)
        dev_exts.push_back(VK_EXT_HDR_METADATA_EXTENSION_NAME);

#ifdef __WXMAC__
    // Required by MoltenVK when the portability enumeration layer is active.
    dev_exts.push_back("VK_KHR_portability_subset");
#endif
 
    VkDeviceCreateInfo ci{};
    ci.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    ci.queueCreateInfoCount    = (uint32_t)queue_cis.size();
    ci.pQueueCreateInfos       = queue_cis.data();
    ci.enabledExtensionCount   = (uint32_t)dev_exts.size();
    ci.ppEnabledExtensionNames = dev_exts.data();
 
    VkResult res = vkCreateDevice(physical_device_, &ci, nullptr, &device_);
    if (res != VK_SUCCESS) {
        wxLogError(_("Failed to create Vulkan logical device: %d"), (int)res);
        return false;
    }
 
    vkGetDeviceQueue(device_, graphics_family_, 0, &graphics_queue_);
    vkGetDeviceQueue(device_, present_family_,  0, &present_queue_);
    return true;
}
 
// ─── CreateSwapchain ──────────────────────────────────────────────────────────
bool VKDrawingPanel::CreateSwapchain()
{
    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device_, surface_, &caps);
 
    uint32_t fmt_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device_, surface_, &fmt_count, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(fmt_count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device_, surface_, &fmt_count, formats.data());
 
    VkSurfaceFormatKHR chosen_fmt = formats[0];
    swapchain_is_hdr_ = false;

    // Prefer a 10-bit BT.2020 PQ (HDR10) swapchain when HDR is requested and the
    // surface advertises it; the encoder produces matching A2B10G10R10 PQ data.
    if (hdr::HdrAvailable() && OPTION(kDispHDR)) {
        for (auto& f : formats) {
            if (f.format     == VK_FORMAT_A2B10G10R10_UNORM_PACK32 &&
                f.colorSpace == VK_COLOR_SPACE_HDR10_ST2084_EXT) {
                chosen_fmt = f;
                swapchain_is_hdr_ = true;
                break;
            }
        }
    }

    // 10-bit SDR "deep color" (X11): when the option is on and the surface
    // advertises a 10-bit UNORM format in the ordinary sRGB color space, present
    // into it so the compositor receives a 10-bit image (less banding). On a
    // depth-30 X screen the surface typically offers ONLY a 10-bit format, so
    // this is the natural pick anyway; selecting it explicitly lets us report
    // DeepColorActive() and keep the choice deliberate.
    vk_deep_color_ = false;
    if (!swapchain_is_hdr_ && OPTION(kDispDeepColor) && hdr::DeepColor10Available()) {
        for (auto& f : formats) {
            if ((f.format == VK_FORMAT_A2B10G10R10_UNORM_PACK32 ||
                 f.format == VK_FORMAT_A2R10G10B10_UNORM_PACK32) &&
                f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                chosen_fmt = f;
                vk_deep_color_ = true;
                break;
            }
        }
    }

    if (!swapchain_is_hdr_ && !vk_deep_color_) {
        for (auto& f : formats) {
            if (f.format     == VK_FORMAT_B8G8R8A8_UNORM &&
                f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                chosen_fmt = f;
                break;
            }
        }
    }

    uint32_t mode_count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device_, surface_, &mode_count, nullptr);
    std::vector<VkPresentModeKHR> modes(mode_count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device_, surface_, &mode_count, modes.data());
 
    VkPresentModeKHR chosen_mode = VK_PRESENT_MODE_FIFO_KHR;
    if (!vsync_) {
        for (auto& m : modes)
            if (m == VK_PRESENT_MODE_IMMEDIATE_KHR) { chosen_mode = m; break; }
        if (chosen_mode == VK_PRESENT_MODE_FIFO_KHR)
            for (auto& m : modes)
                if (m == VK_PRESENT_MODE_MAILBOX_KHR) { chosen_mode = m; break; }
    }
 
    VkExtent2D extent;
    if (caps.currentExtent.width != UINT32_MAX) {
        extent = caps.currentExtent;
    } else {
        wxSize sz = GetClientSize();
        extent.width  = std::clamp((uint32_t)sz.GetWidth(),
                                   caps.minImageExtent.width,
                                   caps.maxImageExtent.width);
        extent.height = std::clamp((uint32_t)sz.GetHeight(),
                                   caps.minImageExtent.height,
                                   caps.maxImageExtent.height);
    }
 
    uint32_t img_count = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && img_count > caps.maxImageCount)
        img_count = caps.maxImageCount;
 
    VkSwapchainCreateInfoKHR ci{};
    ci.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    ci.surface          = surface_;
    ci.minImageCount    = img_count;
    ci.imageFormat      = chosen_fmt.format;
    ci.imageColorSpace  = chosen_fmt.colorSpace;
    ci.imageExtent      = extent;
    ci.imageArrayLayers = 1;
    ci.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                          VK_IMAGE_USAGE_TRANSFER_DST_BIT; // needed for vkCmdClearColorImage
 
    uint32_t qfamilies[] = { graphics_family_, present_family_ };
    if (graphics_family_ != present_family_) {
        ci.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
        ci.queueFamilyIndexCount = 2;
        ci.pQueueFamilyIndices   = qfamilies;
    } else {
        ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }
 
    ci.preTransform   = caps.currentTransform;
    ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    ci.presentMode    = chosen_mode;
    ci.clipped        = VK_TRUE;
 
    VkResult res = vkCreateSwapchainKHR(device_, &ci, nullptr, &swapchain_);
    if (res != VK_SUCCESS) {
        wxLogError(_("Failed to create Vulkan swapchain: %d"), (int)res);
        return false;
    }
 
    swapchain_format_ = chosen_fmt.format;
    swapchain_extent_ = extent;
 
    uint32_t sc_count = 0;
    vkGetSwapchainImagesKHR(device_, swapchain_, &sc_count, nullptr);
    swapchain_images_.resize(sc_count);
    vkGetSwapchainImagesKHR(device_, swapchain_, &sc_count, swapchain_images_.data());

    // Declare the content luminance envelope to the compositor. Without it an
    // HDR10 PQ surface is assumed to carry PQ's full 0..10000-nit range, so the
    // compositor tone-maps that phantom range down to the display and squashes
    // our real highlights -- which only reach peak_nits -- back near reference
    // white, making the luminance settings invisible. Matches the Wayland
    // wp_color_manager_v1 and D3D12 SetHDRMetaData paths.
    if (swapchain_is_hdr_ && hdr_metadata_ext_) {
        auto set_md = reinterpret_cast<PFN_vkSetHdrMetadataEXT>(
            vkGetDeviceProcAddr(device_, "vkSetHdrMetadataEXT"));
        if (set_md) {
            const float peak_nits = (float)(int)OPTION(kDispHDRPeakBrightness);
            const float ref_nits  = (float)(int)OPTION(kDispHDRReferenceWhite);
            VkHdrMetadataEXT md{};
            md.sType = VK_STRUCTURE_TYPE_HDR_METADATA_EXT;
            // BT.2020 primaries + D65 white, matching the swapchain color space.
            md.displayPrimaryRed   = { 0.708f, 0.292f };
            md.displayPrimaryGreen = { 0.170f, 0.797f };
            md.displayPrimaryBlue  = { 0.131f, 0.046f };
            md.whitePoint          = { 0.3127f, 0.3290f };
            md.maxLuminance        = peak_nits;
            md.minLuminance        = 0.0f;
            md.maxContentLightLevel = peak_nits;
            // Frame-average light level must not exceed peak.
            md.maxFrameAverageLightLevel = std::min(ref_nits, peak_nits);
            set_md(device_, 1, &swapchain_, &md);
        }
    }

    wxLogDebug(_("Swapchain created: %ux%u, %u images, vsync=%s"),
               extent.width, extent.height, sc_count,
               vsync_ ? wxT("on") : wxT("off"));
    return true;
}
 
// ─── DestroySwapchain ─────────────────────────────────────────────────────────
void VKDrawingPanel::DestroySwapchain()
{
    for (auto fb : framebuffers_)    vkDestroyFramebuffer(device_, fb, nullptr);
    framebuffers_.clear();
    for (auto iv : swapchain_views_) vkDestroyImageView(device_, iv, nullptr);
    swapchain_views_.clear();
    if (swapchain_) {
        vkDestroySwapchainKHR(device_, swapchain_, nullptr);
        swapchain_ = VK_NULL_HANDLE;
    }
}
 
// ─── RecreateSwapchain ────────────────────────────────────────────────────────
bool VKDrawingPanel::RecreateSwapchain()
{
    vkDeviceWaitIdle(device_);
    DestroySwapchain();
    vsync_ = OPTION(kPrefVsync);
    return CreateSwapchain() && CreateImageViews() && CreateFramebuffers();
}
 
// ─── CreateImageViews ─────────────────────────────────────────────────────────
bool VKDrawingPanel::CreateImageViews()
{
    swapchain_views_.resize(swapchain_images_.size());
    for (size_t i = 0; i < swapchain_images_.size(); ++i) {
        VkImageViewCreateInfo ci{};
        ci.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        ci.image                           = swapchain_images_[i];
        ci.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
        ci.format                          = swapchain_format_;
        ci.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        ci.subresourceRange.levelCount     = 1;
        ci.subresourceRange.layerCount     = 1;
 
        VkResult res = vkCreateImageView(device_, &ci, nullptr, &swapchain_views_[i]);
        if (res != VK_SUCCESS) {
            wxLogError(_("Failed to create swapchain image view %zu: %d"), i, (int)res);
            return false;
        }
    }
    return true;
}
 
// ─── CreateRenderPass ─────────────────────────────────────────────────────────
bool VKDrawingPanel::CreateRenderPass()
{
    VkAttachmentDescription color_att{};
    color_att.format        = swapchain_format_;
    color_att.samples       = VK_SAMPLE_COUNT_1_BIT;
    color_att.loadOp        = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_att.storeOp       = VK_ATTACHMENT_STORE_OP_STORE;
    color_att.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_att.finalLayout   = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
 
    VkAttachmentReference ref{};
    ref.attachment = 0;
    ref.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
 
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments    = &ref;
 
    VkSubpassDependency dep{};
    dep.srcSubpass    = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass    = 0;
    dep.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.srcAccessMask = 0;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
 
    VkRenderPassCreateInfo ci{};
    ci.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    ci.attachmentCount = 1;
    ci.pAttachments    = &color_att;
    ci.subpassCount    = 1;
    ci.pSubpasses      = &subpass;
    ci.dependencyCount = 1;
    ci.pDependencies   = &dep;
 
    VkResult res = vkCreateRenderPass(device_, &ci, nullptr, &render_pass_);
    if (res != VK_SUCCESS) {
        wxLogError(_("Failed to create render pass: %d"), (int)res);
        return false;
    }
    return true;
}
 
// ─── CreateFramebuffers ───────────────────────────────────────────────────────
bool VKDrawingPanel::CreateFramebuffers()
{
    framebuffers_.resize(swapchain_views_.size());
    for (size_t i = 0; i < swapchain_views_.size(); ++i) {
        VkFramebufferCreateInfo ci{};
        ci.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        ci.renderPass      = render_pass_;
        ci.attachmentCount = 1;
        ci.pAttachments    = &swapchain_views_[i];
        ci.width           = swapchain_extent_.width;
        ci.height          = swapchain_extent_.height;
        ci.layers          = 1;
 
        VkResult res = vkCreateFramebuffer(device_, &ci, nullptr, &framebuffers_[i]);
        if (res != VK_SUCCESS) {
            wxLogError(_("Failed to create framebuffer %zu: %d"), i, (int)res);
            return false;
        }
    }
    return true;
}
 
// ─── CreateDescriptorSetLayout ────────────────────────────────────────────────
bool VKDrawingPanel::CreateDescriptorSetLayout()
{
    VkDescriptorSetLayoutBinding binding{};
    binding.binding        = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.descriptorCount = 1;
    binding.stageFlags     = VK_SHADER_STAGE_FRAGMENT_BIT;
 
    VkDescriptorSetLayoutCreateInfo ci{};
    ci.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    ci.bindingCount = 1;
    ci.pBindings    = &binding;
 
    VkResult res = vkCreateDescriptorSetLayout(device_, &ci, nullptr, &desc_set_layout_);
    if (res != VK_SUCCESS) {
        wxLogError(_("Failed to create descriptor set layout: %d"), (int)res);
        return false;
    }
    return true;
}
 
// ─── CreateGraphicsPipeline ───────────────────────────────────────────────────
bool VKDrawingPanel::CreateGraphicsPipeline()
{
    auto make_module = [&](const uint32_t* spv, size_t bytes) -> VkShaderModule {
        VkShaderModuleCreateInfo ci{};
        ci.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        ci.codeSize = bytes;
        ci.pCode    = spv;
        VkShaderModule m = VK_NULL_HANDLE;
        vkCreateShaderModule(device_, &ci, nullptr, &m);
        return m;
    };
 
    VkShaderModule vert_mod = make_module(kVertSpv, kVertSpvSize);
    VkShaderModule frag_mod = make_module(kFragSpv, kFragSpvSize);
 
    if (!vert_mod || !frag_mod) {
        wxLogError(_("Failed to create shader modules"));
        if (vert_mod) vkDestroyShaderModule(device_, vert_mod, nullptr);
        if (frag_mod) vkDestroyShaderModule(device_, frag_mod, nullptr);
        return false;
    }
 
    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vert_mod;
    stages[0].pName  = "main";
    stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = frag_mod;
    stages[1].pName  = "main";
 
    VkPipelineVertexInputStateCreateInfo vert_input{};
    vert_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
 
    VkPipelineInputAssemblyStateCreateInfo ia{};
    ia.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
 
    VkPipelineViewportStateCreateInfo vp_state{};
    vp_state.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vp_state.viewportCount = 1;
    vp_state.scissorCount  = 1;
 
    VkPipelineRasterizationStateCreateInfo rast{};
    rast.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rast.polygonMode = VK_POLYGON_MODE_FILL;
    rast.cullMode    = VK_CULL_MODE_NONE;
    rast.frontFace   = VK_FRONT_FACE_CLOCKWISE;
    rast.lineWidth   = 1.0f;
 
    VkPipelineMultisampleStateCreateInfo ms{};
    ms.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
 
    VkPipelineColorBlendAttachmentState blend_att{};
    blend_att.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                               VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
 
    VkPipelineColorBlendStateCreateInfo blend{};
    blend.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend.attachmentCount = 1;
    blend.pAttachments    = &blend_att;
 
    VkDynamicState dyn_states[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dyn{};
    dyn.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dyn.dynamicStateCount = 2;
    dyn.pDynamicStates    = dyn_states;
 
    VkPushConstantRange pc_range{};
    pc_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pc_range.offset     = 0;
    pc_range.size       = sizeof(float) * 8;
 
    VkPipelineLayoutCreateInfo layout_ci{};
    layout_ci.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout_ci.setLayoutCount         = 1;
    layout_ci.pSetLayouts            = &desc_set_layout_;
    layout_ci.pushConstantRangeCount = 1;
    layout_ci.pPushConstantRanges    = &pc_range;
 
    VkResult res = vkCreatePipelineLayout(device_, &layout_ci, nullptr, &pipeline_layout_);
    if (res != VK_SUCCESS) {
        wxLogError(_("Failed to create pipeline layout: %d"), (int)res);
        vkDestroyShaderModule(device_, vert_mod, nullptr);
        vkDestroyShaderModule(device_, frag_mod, nullptr);
        return false;
    }
 
    VkGraphicsPipelineCreateInfo pipe_ci{};
    pipe_ci.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipe_ci.stageCount          = 2;
    pipe_ci.pStages             = stages;
    pipe_ci.pVertexInputState   = &vert_input;
    pipe_ci.pInputAssemblyState = &ia;
    pipe_ci.pViewportState      = &vp_state;
    pipe_ci.pRasterizationState = &rast;
    pipe_ci.pMultisampleState   = &ms;
    pipe_ci.pColorBlendState    = &blend;
    pipe_ci.pDynamicState       = &dyn;
    pipe_ci.layout              = pipeline_layout_;
    pipe_ci.renderPass          = render_pass_;
    pipe_ci.subpass             = 0;
 
    res = vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipe_ci, nullptr, &pipeline_);
 
    vkDestroyShaderModule(device_, vert_mod, nullptr);
    vkDestroyShaderModule(device_, frag_mod, nullptr);
 
    if (res != VK_SUCCESS) {
        wxLogError(_("Failed to create graphics pipeline: %d"), (int)res);
        return false;
    }
    return true;
}
 
// ─── CreateCommandPool ────────────────────────────────────────────────────────
bool VKDrawingPanel::CreateCommandPool()
{
    VkCommandPoolCreateInfo ci{};
    ci.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    ci.queueFamilyIndex = graphics_family_;
    ci.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
 
    VkResult res = vkCreateCommandPool(device_, &ci, nullptr, &cmd_pool_);
    if (res != VK_SUCCESS) {
        wxLogError(_("Failed to create command pool: %d"), (int)res);
        return false;
    }
    return true;
}
 
// ─── CreateCommandBuffers ─────────────────────────────────────────────────────
bool VKDrawingPanel::CreateCommandBuffers()
{
    cmd_buffers_.resize(MAX_FRAMES_IN_FLIGHT);
    VkCommandBufferAllocateInfo ai{};
    ai.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ai.commandPool        = cmd_pool_;
    ai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = MAX_FRAMES_IN_FLIGHT;
 
    VkResult res = vkAllocateCommandBuffers(device_, &ai, cmd_buffers_.data());
    if (res != VK_SUCCESS) {
        wxLogError(_("Failed to allocate command buffers: %d"), (int)res);
        return false;
    }
    return true;
}
 
// ─── CreateSyncObjects ────────────────────────────────────────────────────────
bool VKDrawingPanel::CreateSyncObjects()
{
    VkSemaphoreCreateInfo sem_ci{};
    sem_ci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
 
    VkFenceCreateInfo fen_ci{};
    fen_ci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fen_ci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
 
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        if (vkCreateSemaphore(device_, &sem_ci, nullptr, &image_available_sem_[i]) != VK_SUCCESS ||
            vkCreateSemaphore(device_, &sem_ci, nullptr, &render_finished_sem_[i]) != VK_SUCCESS ||
            vkCreateFence    (device_, &fen_ci, nullptr, &in_flight_fence_[i])     != VK_SUCCESS) {
            wxLogError(_("Failed to create sync objects for frame %d"), i);
            return false;
        }
    }
    return true;
}
 
// ─── CreateDescriptorPoolAndSet ───────────────────────────────────────────────
bool VKDrawingPanel::CreateDescriptorPoolAndSet()
{
    VkDescriptorPoolSize pool_size{};
    pool_size.type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    pool_size.descriptorCount = 1;
 
    VkDescriptorPoolCreateInfo ci{};
    ci.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    ci.maxSets       = 1;
    ci.poolSizeCount = 1;
    ci.pPoolSizes    = &pool_size;
 
    VkResult res = vkCreateDescriptorPool(device_, &ci, nullptr, &desc_pool_);
    if (res != VK_SUCCESS) {
        wxLogError(_("Failed to create descriptor pool: %d"), (int)res);
        return false;
    }
 
    VkDescriptorSetAllocateInfo ai{};
    ai.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    ai.descriptorPool     = desc_pool_;
    ai.descriptorSetCount = 1;
    ai.pSetLayouts        = &desc_set_layout_;
 
    res = vkAllocateDescriptorSets(device_, &ai, &desc_set_);
    if (res != VK_SUCCESS) {
        wxLogError(_("Failed to allocate descriptor set: %d"), (int)res);
        return false;
    }
    return true;
}
 
// ─── FindMemoryType ───────────────────────────────────────────────────────────
uint32_t VKDrawingPanel::FindMemoryType(uint32_t type_filter,
                                        VkMemoryPropertyFlags props) const
{
    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(physical_device_, &mem_props);
    for (uint32_t i = 0; i < mem_props.memoryTypeCount; ++i)
        if ((type_filter & (1u << i)) &&
            (mem_props.memoryTypes[i].propertyFlags & props) == props)
            return i;
    return UINT32_MAX;
}
 
// ─── CreateTexture ────────────────────────────────────────────────────────────
bool VKDrawingPanel::CreateTexture(uint32_t tex_w, uint32_t tex_h, VkFormat fmt)
{
    DestroyTexture();
 
    VkImageCreateInfo img_ci{};
    img_ci.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    img_ci.imageType     = VK_IMAGE_TYPE_2D;
    img_ci.format        = fmt;
    img_ci.extent        = { tex_w, tex_h, 1 };
    img_ci.mipLevels     = 1;
    img_ci.arrayLayers   = 1;
    img_ci.samples       = VK_SAMPLE_COUNT_1_BIT;
    img_ci.tiling        = VK_IMAGE_TILING_OPTIMAL;
    img_ci.usage         = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    img_ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
 
    VkResult res = vkCreateImage(device_, &img_ci, nullptr, &tex_image_);
    if (res != VK_SUCCESS) {
        wxLogError(_("Failed to create texture image: %d"), (int)res);
        return false;
    }
 
    VkMemoryRequirements mem_req;
    vkGetImageMemoryRequirements(device_, tex_image_, &mem_req);
 
    VkMemoryAllocateInfo alloc_ci{};
    alloc_ci.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_ci.allocationSize  = mem_req.size;
    alloc_ci.memoryTypeIndex = FindMemoryType(mem_req.memoryTypeBits,
                                              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (alloc_ci.memoryTypeIndex == UINT32_MAX ||
        vkAllocateMemory(device_, &alloc_ci, nullptr, &tex_memory_) != VK_SUCCESS) {
        wxLogError(_("Failed to allocate texture memory"));
        return false;
    }
    vkBindImageMemory(device_, tex_image_, tex_memory_, 0);
 
    VkImageViewCreateInfo view_ci{};
    view_ci.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_ci.image                           = tex_image_;
    view_ci.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    view_ci.format                          = fmt;
    view_ci.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    view_ci.subresourceRange.levelCount     = 1;
    view_ci.subresourceRange.layerCount     = 1;
 
    res = vkCreateImageView(device_, &view_ci, nullptr, &tex_view_);
    if (res != VK_SUCCESS) {
        wxLogError(_("Failed to create texture image view: %d"), (int)res);
        return false;
    }
 
    VkFilter vk_filter = OPTION(kDispBilinear) ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
 
    VkSamplerCreateInfo samp_ci{};
    samp_ci.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samp_ci.magFilter    = vk_filter;
    samp_ci.minFilter    = vk_filter;
    samp_ci.mipmapMode   = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samp_ci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samp_ci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samp_ci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samp_ci.maxLod       = 0.0f;
 
    res = vkCreateSampler(device_, &samp_ci, nullptr, &tex_sampler_);
    if (res != VK_SUCCESS) {
        wxLogError(_("Failed to create texture sampler: %d"), (int)res);
        return false;
    }
 
    VkDescriptorImageInfo img_info{};
    img_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    img_info.imageView   = tex_view_;
    img_info.sampler     = tex_sampler_;
 
    VkWriteDescriptorSet write{};
    write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet          = desc_set_;
    write.dstBinding      = 0;
    write.descriptorCount = 1;
    write.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo      = &img_info;
 
    vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);
 
    VkDeviceSize needed = (VkDeviceSize)tex_w * tex_h * 4;
    if (needed > staging_size_) {
        if (staging_buffer_) {
            vkDestroyBuffer(device_, staging_buffer_, nullptr);
            vkFreeMemory   (device_, staging_memory_, nullptr);
            staging_buffer_ = VK_NULL_HANDLE;
            staging_memory_ = VK_NULL_HANDLE;
        }
 
        VkBufferCreateInfo buf_ci{};
        buf_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buf_ci.size  = needed;
        buf_ci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
 
        if (vkCreateBuffer(device_, &buf_ci, nullptr, &staging_buffer_) != VK_SUCCESS) {
            wxLogError(_("Failed to create staging buffer"));
            return false;
        }
 
        VkMemoryRequirements stg_req;
        vkGetBufferMemoryRequirements(device_, staging_buffer_, &stg_req);
 
        VkMemoryAllocateInfo stg_alloc{};
        stg_alloc.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        stg_alloc.allocationSize  = stg_req.size;
        stg_alloc.memoryTypeIndex = FindMemoryType(stg_req.memoryTypeBits,
                                                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        if (stg_alloc.memoryTypeIndex == UINT32_MAX ||
            vkAllocateMemory(device_, &stg_alloc, nullptr, &staging_memory_) != VK_SUCCESS) {
            wxLogError(_("Failed to allocate staging memory"));
            return false;
        }
        vkBindBufferMemory(device_, staging_buffer_, staging_memory_, 0);
        staging_size_ = needed;
    }
 
    tex_format_      = fmt;
    texture_width_   = tex_w;
    texture_height_  = tex_h;
    return true;
}
 
// ─── DestroyTexture ───────────────────────────────────────────────────────────
void VKDrawingPanel::DestroyTexture()
{
    if (tex_sampler_) { vkDestroySampler  (device_, tex_sampler_, nullptr); tex_sampler_ = VK_NULL_HANDLE; }
    if (tex_view_)    { vkDestroyImageView(device_, tex_view_,    nullptr); tex_view_    = VK_NULL_HANDLE; }
    if (tex_image_)   { vkDestroyImage    (device_, tex_image_,   nullptr); tex_image_   = VK_NULL_HANDLE; }
    if (tex_memory_)  { vkFreeMemory      (device_, tex_memory_,  nullptr); tex_memory_  = VK_NULL_HANDLE; }
 
    if (staging_buffer_) { vkDestroyBuffer(device_, staging_buffer_, nullptr); staging_buffer_ = VK_NULL_HANDLE; }
    if (staging_memory_) { vkFreeMemory   (device_, staging_memory_, nullptr); staging_memory_ = VK_NULL_HANDLE; }
    staging_size_   = 0;
    texture_width_  = 0;
    texture_height_ = 0;
}
 
// ─── DrawingPanelInit ─────────────────────────────────────────────────────────
void VKDrawingPanel::DrawingPanelInit()
{
    DrawingPanelBase::DrawingPanelInit();

    // No logical device -> Vulkan setup (instance/device/surface/swapchain, done
    // in the constructor) failed. Flag it so GameArea falls back to the next
    // renderer in the priority list.
    if (!device_) { init_failed_ = true; return; }

    texture_width_  = (uint32_t)std::ceil(width  * scale);
    texture_height_ = (uint32_t)std::ceil(height * scale);
 
    wxLogDebug(_("VKDrawingPanel initialized: %ux%u (scale: %f)"),
               texture_width_, texture_height_, scale);
}
 
// ─── OnSize ───────────────────────────────────────────────────────────────────
void VKDrawingPanel::OnSize(wxSizeEvent& ev)
{
#if defined(__WXGTK__) && !defined(NO_WAYLAND)
    // Keep the Vulkan subsurface aligned with the (possibly moved) panel.
    PositionWaylandSubsurface();
#endif
    if (device_)
        RecreateSwapchain();
    ev.Skip();
}
 
// ─── DrawArea ─────────────────────────────────────────────────────────────────
//
// FIX: The original used a 'goto' that jumped across the declaration of
// 'wait_stage' (a non-trivially-scoped VkPipelineStageFlags local) into the
// submit/present block.  Jumping over a variable initialisation is undefined
// behaviour in C++ (and an error in C).  Restructured to use a bool flag so
// all locals are declared before any branch.
void VKDrawingPanel::DrawArea(wxWindowDC& dc)
{
    (void)dc;
 
    if (!device_) return;
    if (!did_init) DrawingPanelInit();
 
    vkWaitForFences(device_, 1, &in_flight_fence_[current_frame_], VK_TRUE, UINT64_MAX);
 
    uint32_t image_index = 0;
    VkResult res = vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX,
                                          image_available_sem_[current_frame_],
                                          VK_NULL_HANDLE, &image_index);
 
    if (res == VK_ERROR_OUT_OF_DATE_KHR) {
        RecreateSwapchain();
        return;
    }
    if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR) {
        wxLogError(_("Failed to acquire swapchain image: %d"), (int)res);
        return;
    }
 
    vkResetFences(device_, 1, &in_flight_fence_[current_frame_]);
 
    VkCommandBuffer cmd = cmd_buffers_[current_frame_];
    vkResetCommandBuffer(cmd, 0);
 
    VkCommandBufferBeginInfo begin_ci{};
    begin_ci.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_ci.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &begin_ci);
 
    // ── No data: clear to black and fall through to submit ────────────────────
    if (!todraw) {
        VkClearColorValue clear_val = { {0.f, 0.f, 0.f, 1.f} };
 
        VkImageMemoryBarrier to_clear{};
        to_clear.sType            = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        to_clear.oldLayout        = VK_IMAGE_LAYOUT_UNDEFINED;
        to_clear.newLayout        = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        to_clear.image            = swapchain_images_[image_index];
        to_clear.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        to_clear.dstAccessMask    = VK_ACCESS_TRANSFER_WRITE_BIT;
        vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            0, 0, nullptr, 0, nullptr, 1, &to_clear);
 
        VkImageSubresourceRange sub_range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        vkCmdClearColorImage(cmd, swapchain_images_[image_index],
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                             &clear_val, 1, &sub_range);
 
        VkImageMemoryBarrier to_present = to_clear;
        to_present.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        to_present.newLayout     = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        to_present.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        to_present.dstAccessMask = 0;
        vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            0, 0, nullptr, 0, nullptr, 1, &to_present);
 
    } else {
        // ── Pixel data present: upload and draw ───────────────────────────────
        // HDR uploads 10-bit BT.2020 PQ to match the HDR10 swapchain. Only the
        // 32-bit source path feeds the encoder.
        const bool hdr = HdrActive() && swapchain_is_hdr_ &&
                         !out_8 && !out_16 && !out_24;
        VkFormat vk_fmt = hdr     ? VK_FORMAT_A2B10G10R10_UNORM_PACK32
                        : out_16  ? VK_FORMAT_R5G6B5_UNORM_PACK16
                                  : VK_FORMAT_B8G8R8A8_UNORM;
 
        int scaled_width  = (int)std::ceil(width  * scale);
        int scaled_height = (int)std::ceil(height * scale);
 
        if (!tex_image_ ||
            (int)texture_width_  != scaled_width  ||
            (int)texture_height_ != scaled_height ||
            tex_format_          != vk_fmt) {
            if (!CreateTexture((uint32_t)scaled_width, (uint32_t)scaled_height, vk_fmt)) {
                vkEndCommandBuffer(cmd);
                return;
            }
        }
 
        int inrb         = out_8 ? 4 : out_16 ? 2 : out_24 ? 0 : 1;
        int scaled_border = (int)std::ceil(inrb * scale);
 
        int src_pitch = 0;
        if      (out_8)  src_pitch = (scaled_width + scaled_border) * 1;
        else if (out_16) src_pitch = (scaled_width + scaled_border) * 2;
        else if (out_24) src_pitch = scaled_width * 3;
        else             src_pitch = (scaled_width + scaled_border) * 4;
 
        const uint8_t* src = todraw;
 
        void* mapped = nullptr;
        vkMapMemory(device_, staging_memory_, 0, staging_size_, 0, &mapped);
        uint8_t* stg = static_cast<uint8_t*>(mapped);

        if (hdr) {
            // Skip the top border row, then encode the scaled region directly
            // into the staging buffer as A2B10G10R10 PQ (4 bytes/pixel).
            const uint8_t* tex_ptr = src + src_pitch;
            hdr::EncodePQ10(tex_ptr, src_pitch, scaled_width, scaled_height,
                            reinterpret_cast<uint32_t*>(stg), scaled_width * 4);
        } else if (out_8) {
            src += src_pitch;
            for (int y = 0; y < scaled_height; ++y) {
                const uint8_t* sr = src;
                uint8_t*       dr = stg + y * scaled_width * 4;
                for (int x = 0; x < scaled_width; ++x, ++sr, dr += 4) {
                    uint8_t p = *sr;
                    if (p == 0xff) {
                        dr[0] = dr[1] = dr[2] = 0xff;
                    } else {
                        dr[0] = (p & 0x3)        << 6;
                        dr[1] = ((p >> 2) & 0x7) << 5;
                        dr[2] = ((p >> 5) & 0x7) << 5;
                    }
                    dr[3] = 0xFF;
                }
                src += src_pitch;
            }
        } else if (out_16) {
            const uint16_t* src16 = (const uint16_t*)src + src_pitch / 2;
            for (int y = 0; y < scaled_height; ++y) {
                const uint16_t* sr = src16;
                uint16_t*       dr = (uint16_t*)(stg + y * scaled_width * 2);
                for (int x = 0; x < scaled_width; ++x, ++sr, ++dr) {
                    uint16_t px = *sr;
                    uint8_t r5 = (px >> 10) & 0x1f;
                    uint8_t g5 = (px >>  5) & 0x1f;
                    uint8_t b5 = (px >>  0) & 0x1f;
                    uint8_t g6 = (g5 << 1) | (g5 >> 4);
                    *dr = (uint16_t)((r5 << 11) | (g6 << 5) | b5);
                }
                src16 += src_pitch / 2;
            }
        } else if (out_24) {
            src += scaled_width * 3;
            for (int y = 0; y < scaled_height; ++y) {
                const uint8_t* sr = src;
                uint8_t*       dr = stg + y * scaled_width * 4;
                for (int x = 0; x < scaled_width; ++x, sr += 3, dr += 4) {
                    dr[0] = sr[2];
                    dr[1] = sr[1];
                    dr[2] = sr[0];
                    dr[3] = 0xFF;
                }
                src += src_pitch;
            }
        } else {
            src += src_pitch;
            for (int y = 0; y < scaled_height; ++y) {
                const uint8_t* sr = src;
                uint8_t*       dr = stg + y * scaled_width * 4;
                for (int x = 0; x < scaled_width; ++x, sr += 4, dr += 4) {
                    dr[0] = sr[2];
                    dr[1] = sr[1];
                    dr[2] = sr[0];
                    dr[3] = sr[3];
                }
                src += src_pitch;
            }
        }
 
        vkUnmapMemory(device_, staging_memory_);
 
        VkImageMemoryBarrier to_dst{};
        to_dst.sType            = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        to_dst.oldLayout        = VK_IMAGE_LAYOUT_UNDEFINED;
        to_dst.newLayout        = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        to_dst.image            = tex_image_;
        to_dst.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        to_dst.dstAccessMask    = VK_ACCESS_TRANSFER_WRITE_BIT;
        vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            0, 0, nullptr, 0, nullptr, 1, &to_dst);
 
        VkBufferImageCopy copy{};
        copy.bufferRowLength  = texture_width_;
        copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        copy.imageExtent      = {texture_width_, texture_height_, 1};
        vkCmdCopyBufferToImage(cmd, staging_buffer_, tex_image_,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
 
        VkImageMemoryBarrier to_read{};
        to_read.sType            = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        to_read.oldLayout        = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        to_read.newLayout        = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        to_read.image            = tex_image_;
        to_read.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        to_read.srcAccessMask    = VK_ACCESS_TRANSFER_WRITE_BIT;
        to_read.dstAccessMask    = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0, 0, nullptr, 0, nullptr, 1, &to_read);
 
        VkClearValue clear_val{};
        clear_val.color = {{0.f, 0.f, 0.f, 1.f}};
 
        VkRenderPassBeginInfo rp_begin{};
        rp_begin.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rp_begin.renderPass      = render_pass_;
        rp_begin.framebuffer     = framebuffers_[image_index];
        rp_begin.renderArea      = {{0, 0}, swapchain_extent_};
        rp_begin.clearValueCount = 1;
        rp_begin.pClearValues    = &clear_val;
 
        vkCmdBeginRenderPass(cmd, &rp_begin, VK_SUBPASS_CONTENTS_INLINE);
 
        VkViewport viewport{};
        viewport.width    = (float)swapchain_extent_.width;
        viewport.height   = (float)swapchain_extent_.height;
        viewport.minDepth = 0.f;
        viewport.maxDepth = 1.f;
        vkCmdSetViewport(cmd, 0, 1, &viewport);
 
        VkRect2D scissor{};
        scissor.extent = swapchain_extent_;
        vkCmdSetScissor(cmd, 0, 1, &scissor);
 
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                pipeline_layout_, 0, 1, &desc_set_, 0, nullptr);
 
        float win_w = (float)swapchain_extent_.width;
        float win_h = (float)swapchain_extent_.height;
 
        float dst_x0 = -1.f, dst_y0 = -1.f, dst_x1 = 1.f, dst_y1 = 1.f;
 
        if (OPTION(kDispStretch)) {
            float tex_aspect = (float)scaled_width  / (float)scaled_height;
            float win_aspect = win_w / win_h;
            if (win_aspect > tex_aspect) {
                float ndc_w = tex_aspect / win_aspect;
                dst_x0 = -ndc_w;
                dst_x1 =  ndc_w;
            } else {
                float ndc_h = win_aspect / tex_aspect;
                dst_y0 = -ndc_h;
                dst_y1 =  ndc_h;
            }
        }
 
        float pc[8] = { 0.f, 0.f, 1.f, 1.f,
                        dst_x0, dst_y0, dst_x1, dst_y1 };
        vkCmdPushConstants(cmd, pipeline_layout_,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           0, sizeof(pc), pc);
 
        vkCmdDraw(cmd, 4, 1, 0, 0);
        vkCmdEndRenderPass(cmd);
    }
 
    vkEndCommandBuffer(cmd);
 
    // ── Submit ─────────────────────────────────────────────────────────────────
    // Declared here (after all branches) so no goto/jump crosses the initialiser.
    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
 
    VkSubmitInfo submit{};
    submit.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.waitSemaphoreCount   = 1;
    submit.pWaitSemaphores      = &image_available_sem_[current_frame_];
    submit.pWaitDstStageMask    = &wait_stage;
    submit.commandBufferCount   = 1;
    submit.pCommandBuffers      = &cmd;
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores    = &render_finished_sem_[current_frame_];
 
    vkQueueSubmit(graphics_queue_, 1, &submit, in_flight_fence_[current_frame_]);
 
    // ── Present ────────────────────────────────────────────────────────────────
    VkPresentInfoKHR present_info{};
    present_info.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores    = &render_finished_sem_[current_frame_];
    present_info.swapchainCount     = 1;
    present_info.pSwapchains        = &swapchain_;
    present_info.pImageIndices      = &image_index;
 
    res = vkQueuePresentKHR(present_queue_, &present_info);
    if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR)
        RecreateSwapchain();
    else if (res != VK_SUCCESS)
        wxLogError(_("Failed to present swapchain image: %d"), (int)res);
 
    current_frame_ = (current_frame_ + 1) % MAX_FRAMES_IN_FLIGHT;
}

#endif // !defined(NO_VULKAN)

// ============================================================================
// HDR / deep-color availability detection (startup probe).
// ============================================================================

// The DXGI 1.6 output-desc / DisplayConfig advanced-color APIs used below only
// exist on modern Windows SDKs and are not present in the WINXP (WINVER 0x0501)
// mingw32 toolchain, so the whole probe is compiled out there -- the callers
// fall back to "no HDR" stubs. Include the headers directly (rather than relying
// on the D3D11/D3D12 panels having pulled them in) so the probe works even when
// both D3D backends are disabled.
#if defined(__WXMSW__) && !defined(WINXP)
#include <dxgi1_6.h>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;

// True if any DXGI output is currently in HDR10 (ST.2084 BT.2020) mode.
static bool VbamProbeWindowsHdr() {
    bool hdr = false;

    // Resolve CreateDXGIFactory1 dynamically so we don't add a static
    // dxgi.lib link dependency (matching the D3D12 init path).
    typedef HRESULT(WINAPI* LPFNCreateDXGIFactory1)(REFIID, void**);
    HMODULE hDXGI = LoadLibrary(TEXT("dxgi.dll"));
    if (!hDXGI)
        return false;
    auto CreateFactory1 = reinterpret_cast<LPFNCreateDXGIFactory1>(
        reinterpret_cast<void*>(GetProcAddress(hDXGI, "CreateDXGIFactory1")));
    if (!CreateFactory1) {
        FreeLibrary(hDXGI);
        return false;
    }

    // All DXGI COM objects must be released before FreeLibrary, since their
    // vtables live inside dxgi.dll. Keep them in an inner scope.
    {
        ComPtr<IDXGIFactory1> factory;
        if (SUCCEEDED(CreateFactory1(IID_PPV_ARGS(&factory)))) {
            ComPtr<IDXGIAdapter1> adapter;
            for (UINT a = 0; !hdr && factory->EnumAdapters1(a, &adapter) != DXGI_ERROR_NOT_FOUND; ++a) {
                ComPtr<IDXGIOutput> output;
                for (UINT o = 0; !hdr && adapter->EnumOutputs(o, &output) != DXGI_ERROR_NOT_FOUND; ++o) {
                    ComPtr<IDXGIOutput6> output6;
                    if (SUCCEEDED(output.As(&output6))) {
                        DXGI_OUTPUT_DESC1 desc{};
                        if (SUCCEEDED(output6->GetDesc1(&desc)) &&
                            desc.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020)
                            hdr = true;
                    }
                    output.Reset();
                }
                adapter.Reset();
            }
        }
    }

    FreeLibrary(hDXGI);
    return hdr;
}

// The peak luminance (nits) of an HDR-enabled output, from DXGI. Windows PQ is
// absolute-nits, so this is the display peak directly (no reference-white
// scaling, unlike macOS EDR).
//
// Mirrors the macOS EDR probe (VbamMacosMaxEdrHeadroom): prefer the output the
// emulator window is currently on, so a multi-monitor setup reports the peak of
// the panel actually showing the game rather than whichever attached panel is
// brightest. Falls back to the max over all outputs in HDR mode when the window
// is not up yet or is not on an HDR output. 0 when no output is in HDR mode
// (G2084 colorspace) -- callers then keep the fixed default.
static uint32_t VbamWindowsDisplayPeakNits() {
    // The monitor the emulator's main window is on (null before the frame
    // exists), matched against each DXGI output's HMONITOR below.
    HMONITOR window_monitor = nullptr;
    if (wxGetApp().frame) {
        HWND hwnd = reinterpret_cast<HWND>(wxGetApp().frame->GetHWND());
        if (hwnd)
            window_monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    }

    uint32_t peak_any = 0;     // brightest HDR output (fallback)
    uint32_t peak_window = 0;  // the HDR output the window is on, if any

    typedef HRESULT(WINAPI* LPFNCreateDXGIFactory1)(REFIID, void**);
    HMODULE hDXGI = LoadLibrary(TEXT("dxgi.dll"));
    if (!hDXGI)
        return 0;
    auto CreateFactory1 = reinterpret_cast<LPFNCreateDXGIFactory1>(
        reinterpret_cast<void*>(GetProcAddress(hDXGI, "CreateDXGIFactory1")));
    if (!CreateFactory1) {
        FreeLibrary(hDXGI);
        return 0;
    }

    {
        ComPtr<IDXGIFactory1> factory;
        if (SUCCEEDED(CreateFactory1(IID_PPV_ARGS(&factory)))) {
            ComPtr<IDXGIAdapter1> adapter;
            for (UINT a = 0; factory->EnumAdapters1(a, &adapter) != DXGI_ERROR_NOT_FOUND; ++a) {
                ComPtr<IDXGIOutput> output;
                for (UINT o = 0; adapter->EnumOutputs(o, &output) != DXGI_ERROR_NOT_FOUND; ++o) {
                    ComPtr<IDXGIOutput6> output6;
                    if (SUCCEEDED(output.As(&output6))) {
                        DXGI_OUTPUT_DESC1 desc{};
                        if (SUCCEEDED(output6->GetDesc1(&desc)) &&
                            desc.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020) {
                            const uint32_t m = static_cast<uint32_t>(desc.MaxLuminance + 0.5f);
                            if (m > peak_any)
                                peak_any = m;
                            if (window_monitor && desc.Monitor == window_monitor)
                                peak_window = m;
                        }
                    }
                    output.Reset();
                }
                adapter.Reset();
            }
        }
    }

    FreeLibrary(hDXGI);
    return peak_window ? peak_window : peak_any;
}

// True if the current display *supports* HDR but the user has it turned off in
// Windows display settings, as opposed to a display with no HDR support at all.
// VbamProbeWindowsHdr() (the DXGI G2084 colorspace check) returns false for both
// cases, so it cannot tell them apart; this uses the DisplayConfig advanced-
// color API, which reports "supported" and "enabled" separately. Queried live
// (not cached at startup) so toggling Windows HDR while the app runs is seen
// when the dialog reopens.
static bool VbamWindowsHdrSupportedButOff() {
    UINT32 num_paths = 0, num_modes = 0;
    if (GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &num_paths,
                                    &num_modes) != ERROR_SUCCESS)
        return false;

    std::vector<DISPLAYCONFIG_PATH_INFO> paths(num_paths);
    std::vector<DISPLAYCONFIG_MODE_INFO> modes(num_modes);
    if (QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &num_paths, paths.data(),
                           &num_modes, modes.data(), nullptr) != ERROR_SUCCESS)
        return false;

    for (UINT32 i = 0; i < num_paths; ++i) {
        DISPLAYCONFIG_GET_ADVANCED_COLOR_INFO aci = {};
        aci.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_ADVANCED_COLOR_INFO;
        aci.header.size = sizeof(aci);
        aci.header.adapterId = paths[i].targetInfo.adapterId;
        aci.header.id = paths[i].targetInfo.id;
        if (DisplayConfigGetDeviceInfo(&aci.header) != ERROR_SUCCESS)
            continue;
        // advancedColorSupported: the panel/link can do HDR (BT.2020 PQ).
        // advancedColorEnabled:   it is currently in HDR mode.
        // Supported but not enabled => HDR is present but switched off.
        if (aci.advancedColorSupported && !aci.advancedColorEnabled)
            return true;
    }
    return false;
}
#endif

#if defined(__WXMAC__)
// Defined in macsupport.mm.
extern bool VbamProbeMacosHdr();
extern bool VbamMacosHdrSupportedButOff();
extern double VbamMacosMaxEdrHeadroom();
#endif

#if defined(__WXGTK__)
#ifndef NO_OGL
// True if a 10-bit (deep color) RGBA GLX framebuffer config exists on X11.
static bool VbamProbeX11DeepColor() {
    Display* dpy = GDK_DISPLAY_XDISPLAY(gdk_display_get_default());
    if (!dpy)
        return false;
    int attribs[] = {
        GLX_RENDER_TYPE, GLX_RGBA_BIT,
        GLX_RED_SIZE,   10,
        GLX_GREEN_SIZE, 10,
        GLX_BLUE_SIZE,  10,
        None
    };
    int n = 0;
    GLXFBConfig* cfgs = glXChooseFBConfig(dpy, DefaultScreen(dpy), attribs, &n);
    if (cfgs)
        XFree(cfgs);
    return n > 0;
}
#endif  // !NO_OGL

#if !defined(NO_VULKAN) && !defined(NO_WAYLAND)
// True if a Wayland Vulkan surface advertises an HDR10 ST.2084 / A2B10G10R10
// format -- i.e. the compositor/display can present HDR right now.
static bool VbamProbeWaylandVulkanHdr() {
    if (!VbamVulkanBootstrap())
        return false;  // no Vulkan loader -> no Vulkan HDR
    GdkDisplay* gdk_dpy = gdk_display_get_default();
    if (!GDK_IS_WAYLAND_DISPLAY(gdk_dpy))
        return false;
    struct wl_display* wl_dpy = gdk_wayland_display_get_wl_display(gdk_dpy);
    if (!wl_dpy)
        return false;

    // Bind wl_compositor and create a throwaway surface to query against.
    VbamWlGlobals g;
    struct wl_registry* reg = wl_display_get_registry(wl_dpy);
    wl_registry_add_listener(reg, &kVbamWlRegistryListener, &g);
    wl_display_roundtrip(wl_dpy);
    wl_registry_destroy(reg);
    if (!g.comp) {
        if (g.subcomp) wl_subcompositor_destroy(g.subcomp);
        return false;
    }
    struct wl_surface* surf = wl_compositor_create_surface(g.comp);

    bool hdr = false;
    const char* exts[] = { VK_KHR_SURFACE_EXTENSION_NAME,
                           VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME,
                           VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME };
    VkApplicationInfo app{};
    app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.apiVersion = VK_API_VERSION_1_0;
    VkInstanceCreateInfo ici{};
    ici.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ici.pApplicationInfo = &app;
    ici.enabledExtensionCount = 3;
    ici.ppEnabledExtensionNames = exts;

    VkInstance inst = VK_NULL_HANDLE;
    if (vkCreateInstance(&ici, nullptr, &inst) == VK_SUCCESS) {
        VbamVulkanLoadInstanceFns(inst);
        VkWaylandSurfaceCreateInfoKHR sci{};
        sci.sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
        sci.display = wl_dpy;
        sci.surface = surf;
        VkSurfaceKHR vksurf = VK_NULL_HANDLE;
        if (vkCreateWaylandSurfaceKHR(inst, &sci, nullptr, &vksurf) == VK_SUCCESS) {
            uint32_t nd = 0;
            vkEnumeratePhysicalDevices(inst, &nd, nullptr);
            std::vector<VkPhysicalDevice> devs(nd);
            vkEnumeratePhysicalDevices(inst, &nd, devs.data());
            for (auto pd : devs) {
                uint32_t nf = 0;
                vkGetPhysicalDeviceSurfaceFormatsKHR(pd, vksurf, &nf, nullptr);
                std::vector<VkSurfaceFormatKHR> fmts(nf);
                vkGetPhysicalDeviceSurfaceFormatsKHR(pd, vksurf, &nf, fmts.data());
                for (auto& f : fmts) {
                    if (f.format == VK_FORMAT_A2B10G10R10_UNORM_PACK32 &&
                        f.colorSpace == VK_COLOR_SPACE_HDR10_ST2084_EXT) {
                        hdr = true;
                        break;
                    }
                }
                if (hdr) break;
            }
            vkDestroySurfaceKHR(inst, vksurf, nullptr);
        }
        vkDestroyInstance(inst, nullptr);
    }

    wl_surface_destroy(surf);
    if (g.subcomp) wl_subcompositor_destroy(g.subcomp);
    wl_compositor_destroy(g.comp);
    return hdr;
}
#endif  // Vulkan + Wayland
#endif  // __WXGTK__

// True if a native-Wayland Vulkan renderer can actually run: Vulkan is compiled
// in and an instance with VK_KHR_wayland_surface can be created with at least
// one physical device. Needs no display connection, so it is safe to call from
// main() before the GUI toolkit is up (to choose GDK_BACKEND).
bool VbamVulkanRuntimeUsable() {
#if !defined(NO_VULKAN) && !defined(NO_WAYLAND) && defined(__WXGTK__)
    if (!VbamVulkanBootstrap())
        return false;  // no Vulkan loader
    const char* exts[] = { VK_KHR_SURFACE_EXTENSION_NAME,
                           VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME };
    VkApplicationInfo app{};
    app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.apiVersion = VK_API_VERSION_1_0;
    VkInstanceCreateInfo ici{};
    ici.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ici.pApplicationInfo = &app;
    ici.enabledExtensionCount = 2;
    ici.ppEnabledExtensionNames = exts;

    VkInstance inst = VK_NULL_HANDLE;
    if (vkCreateInstance(&ici, nullptr, &inst) != VK_SUCCESS)
        return false;  // loader/driver missing or no Wayland surface extension
    VbamVulkanLoadInstanceFns(inst);

    uint32_t nd = 0;
    vkEnumeratePhysicalDevices(inst, &nd, nullptr);
    vkDestroyInstance(inst, nullptr);
    return nd > 0;     // a Vulkan-capable GPU is present
#else
    return false;
#endif
}

bool VbamVulkanRuntimeAvailable() {
#if defined(NO_VULKAN)
    return false;
#elif defined(__WXMSW__) || defined(__WXGTK__)
    // Vulkan is loaded dynamically on Windows/Linux: report it available only if
    // the loader and the global entry points resolve. Cached by VbamVulkanBootstrap.
    return VbamVulkanBootstrap();
#else
    return true;  // linked normally elsewhere (macOS / MoltenVK)
#endif
}

namespace hdr {
void DetectAvailability() {
    static bool done = false;
    if (done) return;
    done = true;

    bool hdr_avail = false;
    bool deep10 = false;

#if defined(__WXMSW__) && !defined(WINXP)
    hdr_avail = VbamProbeWindowsHdr();
#elif defined(__WXMAC__)
    hdr_avail = VbamProbeMacosHdr();
#elif defined(__WXGTK__)
    if (IsWayland()) {
#if !defined(NO_VULKAN) && !defined(NO_WAYLAND)
        hdr_avail = VbamProbeWaylandVulkanHdr();
#endif
        // On KDE, if the display's HDR mode is off the compositor still accepts
        // PQ content (it tone-maps it), so the protocol probe says "available".
        // But there is no real HDR to present, so treat it as unavailable: the
        // dialog hides the HDR checkbox and (uniquely for this case) shows no
        // warning, since the user deliberately turned HDR off in KDE.
        if (KdeHdrDisabled())
            hdr_avail = false;
    } else {
        // X11/Xorg cannot signal HDR per window; only offer 10-bit deep color.
#ifndef NO_OGL
        deep10 = VbamProbeX11DeepColor();
#endif
    }
#endif

    hdr::SetAvailability(hdr_avail, deep10);
}

bool WindowsHdrDisabled() {
#if defined(__WXMSW__) && !defined(WINXP)
    return VbamWindowsHdrSupportedButOff();
#else
    return false;
#endif
}

bool MacosHdrDisabled() {
#if defined(__WXMAC__)
    return VbamMacosHdrSupportedButOff();
#else
    return false;
#endif
}

uint32_t DisplayPeakNits() {
#if defined(__WXMAC__)
    // EDR headroom is a multiplier over SDR white; our encoder maps the
    // reference-white setting to 1.0, so the display's usable peak in our model
    // is headroom x reference white. Content above that just clips on EDR.
    const double head = VbamMacosMaxEdrHeadroom();
    if (head > 1.0)
        return static_cast<uint32_t>(
            head * static_cast<double>(OPTION(kDispHDRReferenceWhite)) + 0.5);
    return 0;
#elif defined(__WXMSW__) && !defined(WINXP)
    // PQ is absolute-nits, so the DXGI display peak is used directly.
    return VbamWindowsDisplayPeakNits();
#elif defined(__WXGTK__)
#if !defined(NO_WAYLAND)
    // Wayland PQ is absolute-nits too; query the compositor's per-output max
    // luminance. X11 has no per-window HDR, so it stays 0.
    if (IsWayland())
        return WaylandDisplayPeakNits();
#endif
    return 0;
#else
    return 0;
#endif
}
}  // namespace hdr
