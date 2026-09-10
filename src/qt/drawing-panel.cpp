// Frame pipeline (filters, interframe blending, plugins, OSD) and the two
// renderers of the Qt frontend: the QWidget software blitter and the
// QOpenGLWidget renderer. Ported from the DrawingPanelBase / BasicDrawingPanel /
// GLDrawingPanel parts of src/wx/panel.cpp.

#include "qt/drawing-panel.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include <QCoreApplication>
#include <QFont>
#include <QFontMetrics>
#include <QOpenGLContext>
#include <QOpenGLShaderProgram>
#include <QPaintEvent>
#include <QPainter>
#include <QSurfaceFormat>
#include <QtEndian>

#include "components/filters/filters.h"
#include "components/filters_interframe/interframe.h"
#include "components/filters_scalefx/scalefx.h"
#include "core/base/check.h"
#include "core/base/system.h"
#include "core/gba/gbaGlobals.h"
#include "qt/app.h"
#include "qt/config/option-proxy.h"
#include "qt/config/option.h"
#include "qt/game-area.h"
#include "qt/log.h"
#include "qt/main-window.h"
#include "qt/widgets/render-plugin.h"

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

// Maximum number of filter threads, capped to avoid diminishing returns.
int GetMaxFilterThreads() {
    unsigned int hw_threads = std::thread::hardware_concurrency();
    if (hw_threads == 0)
        hw_threads = 1;
    const int max_opt = OPTION(kDispMaxThreads);
    int n = static_cast<int>(std::min(hw_threads, 8u));
    if (max_opt > 0)
        n = std::min(n, max_opt);
    return std::max(n, 1);
}

// Filter context radius - max rows above/below a source row that filters examine
constexpr int kFilterContextRadius = 2;
// Extra rows for seam processing margin
constexpr int kSeamMarginRows = 1;

// Serialize RPI plugin calls: most plugins have non-thread-safe global state.
std::mutex g_rpi_output_mutex;

// Apply the currently selected 32-bit filter to the image region.
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
        case config::Filter::kNone:
        case config::Filter::kLast:
            break;
    }
}

// Sets the color shifts to the 32bpp layout the built-in filters expect.
struct ShiftSaver {
    int r, g, b, mask;
    ShiftSaver()
        : r(systemRedShift), g(systemGreenShift), b(systemBlueShift), mask(RGB_LOW_BITS_MASK) {
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
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
    }
    ~ShiftSaver() {
        systemRedShift = r;
        systemGreenShift = g;
        systemBlueShift = b;
        RGB_LOW_BITS_MASK = mask;
    }
};

inline uint32_t Pack32(uint8_t r8, uint8_t g8, uint8_t b8) {
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
    return b8 | (g8 << 8) | (r8 << 16);
#else
    return (r8 << 24) | (g8 << 16) | (b8 << 8);
#endif
}

inline void Unpack32(uint32_t color, uint8_t& r8, uint8_t& g8, uint8_t& b8) {
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
    r8 = (color >> 16) & 0xff;
    g8 = (color >> 8) & 0xff;
    b8 = color & 0xff;
#else
    r8 = (color >> 24) & 0xff;
    g8 = (color >> 16) & 0xff;
    b8 = (color >> 8) & 0xff;
#endif
}

// Converts `rows` rows of `width` pixels at `depth` bpp (8/16/24) from `src`
// (stride `src_stride` bytes) into the 32bpp filter input layout in `dst32`:
// one left border pixel, the row, one right border pixel; `dst32` must hold
// (width + 2) * (rows + 3) entries. Fills the top border row and two bottom
// rows by duplication. Returns a pointer to the first image pixel.
uint8_t* ConvertTo32(const uint8_t* src, int src_stride, int width, int rows, int depth,
                     uint32_t* dst32) {
    const int total_width32 = width + 2;
    int pos = total_width32;  // skip row 0 (top border)
    for (int y = 0; y < rows; y++) {
        const uint8_t* row = src + src_stride * y;
        const int left_border_pos = pos++;
        if (depth == 8) {
            for (int x = 0; x < width; x++) {
                const uint8_t v = row[x];
                const uint8_t r3 = (v >> 5) & 0x7, g3 = (v >> 2) & 0x7, b2 = v & 0x3;
                dst32[pos++] = Pack32((r3 << 5) | (r3 << 2) | (r3 >> 1),
                                      (g3 << 5) | (g3 << 2) | (g3 >> 1),
                                      (b2 << 6) | (b2 << 4) | (b2 << 2) | b2);
            }
        } else if (depth == 16) {
            const uint16_t* row16 = reinterpret_cast<const uint16_t*>(row);
            for (int x = 0; x < width; x++) {
                const uint16_t v = row16[x];
                const uint8_t r5 = (v >> 10) & 0x1f, g5 = (v >> 5) & 0x1f, b5 = v & 0x1f;
                dst32[pos++] = Pack32((r5 << 3) | (r5 >> 2), (g5 << 3) | (g5 >> 2),
                                      (b5 << 3) | (b5 >> 2));
            }
        } else {  // 24
            for (int x = 0; x < width; x++) {
                const int p = x * 3;
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
                dst32[pos++] = row[p] | (row[p + 1] << 8) | (row[p + 2] << 16);
#else
                dst32[pos++] = (row[p] << 24) | (row[p + 1] << 16) | (row[p + 2] << 8);
#endif
            }
        }
        dst32[left_border_pos] = dst32[left_border_pos + 1];
        dst32[pos] = dst32[pos - 1];
        pos++;
    }
    // top border row = row 1
    memcpy(dst32, dst32 + total_width32, total_width32 * sizeof(uint32_t));
    // two extra bottom rows = last row
    const int last = total_width32 * rows;
    memcpy(dst32 + last + total_width32, dst32 + last, total_width32 * sizeof(uint32_t));
    memcpy(dst32 + last + total_width32 * 2, dst32 + last, total_width32 * sizeof(uint32_t));
    return reinterpret_cast<uint8_t*>(dst32 + total_width32 + 1);
}

// Converts one 32bpp row back to `depth` bpp.
void ConvertRowFrom32(const uint32_t* src32, uint8_t* dst, int pixels, int depth) {
    if (depth == 8) {
        for (int x = 0; x < pixels; x++) {
            uint8_t r8, g8, b8;
            Unpack32(src32[x], r8, g8, b8);
            dst[x] = ((r8 >> 5) << 5) | ((g8 >> 5) << 2) | (b8 >> 6);
        }
    } else if (depth == 16) {
        uint16_t* dst16 = reinterpret_cast<uint16_t*>(dst);
        for (int x = 0; x < pixels; x++) {
            uint8_t r8, g8, b8;
            Unpack32(src32[x], r8, g8, b8);
            dst16[x] = ((r8 >> 3) << 10) | ((g8 >> 3) << 5) | (b8 >> 3);
        }
    } else {  // 24
        for (int x = 0; x < pixels; x++) {
            const uint8_t* p = reinterpret_cast<const uint8_t*>(&src32[x]);
            dst[x * 3] = p[0];
            dst[x * 3 + 1] = p[1];
            dst[x * 3 + 2] = p[2];
        }
    }
}

// Re-process a band around a seam with full source context to eliminate the
// artifacts left at thread boundaries. `src`/`dst` point at the first image
// row (after the top border row(s)); `bandStart`/`bandEnd` are source rows.
void ProcessSeamBand(uint8_t* src, int srcPitch, uint8_t* dst, int dstPitch, int width,
                     int fullHeight, int bandStart, int bandEnd, int scale, int depth) {
    (void)fullHeight;
    const int bandHeight = bandEnd - bandStart;
    if (bandHeight <= 0)
        return;

    const int outBandHeight = bandHeight * scale;
    const int outRowBytes = width * 4 * scale;
    std::vector<uint8_t> tempBuf(static_cast<size_t>(outRowBytes) * outBandHeight);

    const int skipRows = kFilterContextRadius * scale;
    const int copyRows = outBandHeight - 2 * skipRows;
    const int dstStartY = (bandStart + kFilterContextRadius) * scale;
    const int scaled_width = width * scale;

    if (depth == 32) {
        ApplyFilter32(src + srcPitch * bandStart, srcPitch, nullptr, tempBuf.data(), outRowBytes,
                      width, bandHeight);
        if (copyRows > 0) {
            uint8_t* dstRow = dst + dstPitch * dstStartY;
            const uint8_t* srcRow = tempBuf.data() + outRowBytes * skipRows;
            for (int y = 0; y < copyRows; y++) {
                memcpy(dstRow, srcRow, width * 4 * scale);
                dstRow += dstPitch;
                srcRow += outRowBytes;
            }
        }
        return;
    }

    ShiftSaver shifts;
    const int total_width32 = width + 2;
    std::vector<uint32_t> src32(static_cast<size_t>(total_width32) * (bandHeight + 3));
    uint8_t* filter_src =
        ConvertTo32(src + srcPitch * bandStart, srcPitch, width, bandHeight, depth, src32.data());
    ApplyFilter32(filter_src, total_width32 * 4, nullptr, tempBuf.data(), outRowBytes, width,
                  bandHeight);

    if (copyRows > 0) {
        const uint32_t* src32_row =
            reinterpret_cast<const uint32_t*>(tempBuf.data() + outRowBytes * skipRows);
        uint8_t* dstRow = dst + dstPitch * dstStartY;
        for (int y = 0; y < copyRows; y++) {
            ConvertRowFrom32(src32_row, dstRow, scaled_width, depth);
            dstRow += dstPitch;
            src32_row += scaled_width;
        }
    }
}

// Draw Unicode text on the raw pixel buffer: render with QPainter into a
// QImage, threshold, and stamp pure red pixels into the frame buffer at its
// color depth. Equivalent of drawTextWx.
void drawTextQt(uint8_t* buffer, int pitch, int x, int y, const QString& text, int buffer_width,
                int buffer_height, double scale, int color_depth) {
    if (text.isEmpty())
        return;

    const int bpp = color_depth >> 3;
    const int fontSize = static_cast<int>(std::ceil(11 * scale));

    QFont font = QCoreApplication::instance() ? QApplication::font() : QFont();
    font.setPointSize(std::max(fontSize, 1));
    font.setWeight(QFont::Normal);
    QFontMetrics fm(font);

    const int maxWidth = buffer_width - x - static_cast<int>(std::ceil(5 * scale));
    if (maxWidth <= 0)
        return;

    // Character-wrap the text into lines.
    QStringList lines;
    QString currentLine;
    for (int i = 0; i < text.size(); i++) {
        const QChar ch = text[i];
        if (ch == QLatin1Char('\n')) {
            lines.append(currentLine);
            currentLine.clear();
            continue;
        }
        const QString testLine = currentLine + ch;
        if (fm.horizontalAdvance(testLine) > maxWidth && !currentLine.isEmpty()) {
            lines.append(currentLine);
            currentLine = ch;
        } else {
            currentLine += ch;
        }
    }
    if (!currentLine.isEmpty())
        lines.append(currentLine);

    int maxLineWidth = 0;
    const int lineHeight = fm.height();
    const int lineSpacing = lineHeight + 1;
    for (const QString& l : lines)
        maxLineWidth = std::max(maxLineWidth, fm.horizontalAdvance(l));

    const int textWidth = maxLineWidth + 4;
    const int textHeight = static_cast<int>(lines.size() * lineSpacing) + 4;
    if (textWidth <= 0 || textHeight <= 0)
        return;

    QImage textImg(textWidth, textHeight, QImage::Format_RGB32);
    textImg.fill(Qt::black);
    {
        QPainter p(&textImg);
        p.setFont(font);
        p.setPen(Qt::red);
        int currentY = 1;
        for (const QString& l : lines) {
            p.drawText(1, currentY + fm.ascent(), l);
            currentY += lineSpacing;
        }
    }

    for (int ty = 0; ty < textHeight; ty++) {
        const QRgb* row = reinterpret_cast<const QRgb*>(textImg.constScanLine(ty));
        for (int tx = 0; tx < textWidth; tx++) {
            const int bufX = x + tx;
            const int bufY = y + ty;
            if (bufX < 0 || bufY < 0 || bufX >= buffer_width || bufY >= buffer_height)
                continue;
            // Threshold anti-aliased text to create crisp pixels.
            if (qRed(row[tx]) > 100) {
                uint8_t* bufPtr = buffer + (bufY * pitch) + (bufX * bpp);
                if (color_depth == 8) {
                    *bufPtr = 0xE0;
                } else if (color_depth == 16) {
                    *reinterpret_cast<uint16_t*>(bufPtr) = 0x7C00;
                } else if (color_depth == 24) {
                    bufPtr[0] = 255;
                    bufPtr[1] = 0;
                    bufPtr[2] = 0;
                } else {
                    *reinterpret_cast<uint32_t*>(bufPtr) = (0xffu << systemRedShift);
                }
            }
        }
    }
}

}  // namespace

// ---------------------------------------------------------------------------
// FilterThread
// ---------------------------------------------------------------------------

// In order to run filters in parallel, each band of the image is processed by
// its own thread. Threads are created once (per panel / thread count) and
// signalled every frame: three phases -- IFB first, then Filter, then an
// optional SeamFix pass that re-processes the rows around each band boundary.
class FilterThread {
public:
    enum class Phase { IFB, Filter, SeamFix };

    FilterThread() = default;
    ~FilterThread() {
        if (src2_) free(src2_);
        if (dst2_) free(dst2_);
    }

    // Starts the worker thread (multi-threaded mode only).
    void Start() { thread_ = std::thread([this] { Entry(); }); }

    // Hands a work item (or, with src == nullptr, the exit request) to the
    // worker and wakes it.
    void Signal(uint8_t* src) {
        {
            std::lock_guard<std::mutex> lock(lock_);
            src_ = src;
            work_pending_ = true;
        }
        sig_.notify_one();
    }

    void Join() {
        if (thread_.joinable())
            thread_.join();
    }

    Semaphore* done_ = nullptr;
    Semaphore* ready_ = nullptr;

    // Set these params before running
    int nthreads_ = 1;
    int threadno_ = 0;
    int width_ = 0;
    int height_ = 0;
    double scale_ = 1.0;
    const RENDER_PLUGIN_INFO* rpi_ = nullptr;
    int rpi_bpp_ = 4;
    bool rpi_using_rgb565_ = false;
    int panel_color_depth_ = 32;
    uint8_t* dst_ = nullptr;
    uint8_t* delta_ = nullptr;

    // set this param every round; if nullptr, end thread
    uint8_t* src_ = nullptr;

    Phase phase_ = Phase::IFB;

    // Seam fix parameters (set when phase_ == SeamFix); -1 means no work.
    int seamBandStart_ = -1;
    int seamBandEnd_ = -1;
    uint8_t* seamSrc_ = nullptr;
    uint8_t* seamDst_ = nullptr;

    // Thread body. In single-threaded mode (nthreads_ == 1) this runs one
    // round on the caller's thread and returns.
    void Entry() {
        const int height_real = height_;
        const int procy = height_ * threadno_ / nthreads_;
        height_ = height_ * (threadno_ + 1) / nthreads_ - procy;

        const bool is_8bit = (panel_color_depth_ == 8);
        const bool is_16bit = (panel_color_depth_ == 16);
        const bool is_24bit = (panel_color_depth_ == 24);
        const bool is_32bit = !is_8bit && !is_16bit && !is_24bit;

        const bool using_plugin = (rpi_ != nullptr);
        const int inbpp = using_plugin ? rpi_bpp_ : (panel_color_depth_ >> 3);
        const int inrb = using_plugin
                             ? ((rpi_bpp_ == 1) ? 4 : (rpi_bpp_ == 2) ? 2 : (rpi_bpp_ == 3) ? 0 : 1)
                             : (is_8bit ? 4 : is_16bit ? 2 : is_24bit ? 0 : 1);
        const int instride = (width_ + inrb) * inbpp;

        const int total_width32 = width_ + 2;
        const int instride32 = total_width32 * 4;

        const int outbpp = using_plugin ? rpi_bpp_ : (panel_color_depth_ >> 3);
        const int scale_int = static_cast<int>(scale_);
        const int outstride = (width_ + inrb) * outbpp * scale_int;
        const int outstride32 = width_ * 4 * scale_int;

        const int src_offset = instride * (procy + 1);

        delta_ += instride * procy;
        dst_ += outstride * (procy + 1) * scale_int;
        uint8_t* const dest = dst_;

        if (!is_32bit) {
            const size_t src2_required = static_cast<size_t>(total_width32) * (height_real + 3);
            const size_t dst2_required = static_cast<size_t>(width_ * scale_int) *
                                         (height_real * scale_int + scale_int);
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

        const int scaled_height = height_ * scale_int;
        const int scaled_width = width_ * scale_int;
        const int scaled_border_dest = inrb * scale_int;

        std::unique_lock<std::mutex> lock(lock_, std::defer_lock);
        if (nthreads_ > 1) {
            lock.lock();
            ready_->Post();
        }

        for (;;) {
            if (nthreads_ > 1) {
                sig_.wait(lock, [this] { return work_pending_; });
                work_pending_ = false;
            }

            if (!src_) {
                return;
            }

            if (phase_ == Phase::IFB) {
                const config::Interframe ifb_option = OPTION(kDispIFB);
                if (ifb_option != config::Interframe::kNone) {
                    uint8_t* ifb_src = src_ + instride;  // skip top border row
                    if (ifb_option == config::Interframe::kSmart) {
                        InterframeManager::Instance().ApplySmartIBRegion(
                            ifb_src, instride, width_, procy, height_, systemColorDepth,
                            threadno_);
                    } else if (ifb_option == config::Interframe::kMotionBlur) {
                        InterframeManager::Instance().ApplyMotionBlurRegion(
                            ifb_src, instride, width_, procy, height_, systemColorDepth,
                            threadno_);
                    }
                }
                if (nthreads_ == 1)
                    return;
                done_->Post();
                continue;
            }

            if (phase_ == Phase::SeamFix) {
                if (seamBandStart_ >= 0 && seamBandEnd_ > seamBandStart_) {
                    if (OPTION(kDispFilter) == config::Filter::kPlugin && rpi_) {
                        SeamFixPlugin(outstride, scale_int);
                    } else {
                        ProcessSeamBand(seamSrc_, instride, seamDst_, outstride, width_,
                                        height_real, seamBandStart_, seamBandEnd_, scale_int,
                                        panel_color_depth_);
                    }
                }
                done_->Post();
                continue;
            }

            // Phase 2: Filter.
            src_ += src_offset;

            if (nthreads_ == 1) {
                const config::Interframe ifb_option = OPTION(kDispIFB);
                if (ifb_option != config::Interframe::kNone) {
                    if (ifb_option == config::Interframe::kSmart) {
                        InterframeManager::Instance().ApplySmartIBRegion(
                            src_, instride, width_, procy, height_, systemColorDepth, threadno_);
                    } else if (ifb_option == config::Interframe::kMotionBlur) {
                        InterframeManager::Instance().ApplyMotionBlurRegion(
                            src_, instride, width_, procy, height_, systemColorDepth, threadno_);
                    }
                }
            }

            const config::Filter filter_option = OPTION(kDispFilter);

            if (filter_option == config::Filter::kNone) {
                for (int y = 0; y < height_; y++)
                    memcpy(dest + y * outstride, src_ + y * instride, width_ * outbpp);
                if (nthreads_ == 1)
                    return;
                done_->Post();
                continue;
            }

            if (is_32bit || filter_option == config::Filter::kPlugin) {
                ApplyFilterOptimized(instride, outstride, filter_option);
            } else {
                // Non-32bpp with built-in filter: convert to 32bpp, filter, convert back.
                ShiftSaver shifts;
                uint8_t* filter_src =
                    ConvertTo32(src_, instride, width_, height_, panel_color_depth_, src2_);
                const int dst_offset = scaled_width * scale_int;
                uint8_t* filter_dst = reinterpret_cast<uint8_t*>(dst2_ + dst_offset);
                ApplyFilter32(filter_src, instride32, delta_, filter_dst, outstride32, width_,
                              height_);

                const uint32_t* dst32 = dst2_ + dst_offset;
                uint8_t* out = dest;
                for (int y = 0; y < scaled_height; y++) {
                    ConvertRowFrom32(dst32, out, scaled_width, panel_color_depth_);
                    dst32 += scaled_width;
                    out += (scaled_width + (is_24bit ? 0 : scaled_border_dest)) * outbpp;
                }
            }

            if (nthreads_ == 1)
                return;
            done_->Post();
        }
    }

private:
    void SeamFixPlugin(int outstride, int scale_int) {
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
                uint8_t r = (p >> 10) & 0x1f, g = (p >> 5) & 0x1f, b = p & 0x1f;
                d[j] = (r << 11) | (((g << 1) | (g >> 4)) << 5) | b;
            }
            outdesc.SrcPtr = src_seam_converted.data();
        }

        std::vector<uint8_t> tempBuf(static_cast<size_t>(plugin_outstride) * outdesc.DstH);
        outdesc.DstPtr = tempBuf.data();
        {
            std::lock_guard<std::mutex> lock(g_rpi_output_mutex);
            rpi_->Output(&outdesc);
        }

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
                uint8_t r = (p >> 11) & 0x1f, g6 = (p >> 5) & 0x3f, b = p & 0x1f;
                d[j] = (r << 10) | ((g6 >> 1) << 5) | b;
            }
        }

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
    }

    void ApplyFilterOptimized(int instride, int outstride, config::Filter filter_option) {
        if (filter_option != config::Filter::kPlugin) {
            ApplyFilter32(src_, instride, delta_, dst_, outstride, width_, height_);
            return;
        }

        // Stage source into a padded buffer suitable for RPI plugins; the
        // padding depends on the plugin flavor (see the wx port for details).
        const unsigned int pluginVersion = rpi_->Flags & 0xff;
        const bool pluginIsMMX = (rpi_->Flags & RPI_MMX_USED) != 0;
        const int kPluginCtx = pluginIsMMX ? 0 : (pluginVersion == 1 ? 1 : 2);
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

        const size_t srcBytes = static_cast<size_t>(paddedSrcPitch) * paddedHeight;
        std::vector<uint8_t> src_fallback(srcBytes);
        uint8_t* src_buffer = src_fallback.data();
        uint8_t* dst_buffer = dst_;

        auto convert_pixel_888 = [](uint32_t p) -> uint32_t {
            return 0xFF000000u | (p & 0x0000FF00u) | ((p >> 16) & 0xFFu) | ((p & 0xFFu) << 16);
        };
        auto convert_pixel_565 = [](uint16_t p) -> uint16_t {
            uint8_t r = (p >> 10) & 0x1f, g = (p >> 5) & 0x1f, b = p & 0x1f;
            return (r << 11) | (((g << 1) | (g >> 4)) << 5) | b;
        };

        if (bpp == 4) {
            for (int y = 0; y < height_; y++) {
                const uint32_t* s = reinterpret_cast<const uint32_t*>(src_ + y * instride);
                uint32_t* d = reinterpret_cast<uint32_t*>(
                                  src_buffer + (y + kPluginCtx) * paddedSrcPitch) +
                              kPluginCtx;
                for (int x = 0; x < width_; x++) d[x] = convert_pixel_888(s[x]);
                for (int k = 1; k <= kPluginCtx; k++) { d[-k] = d[0]; d[width_ - 1 + k] = d[width_ - 1]; }
            }
        } else if (rpi_using_rgb565_) {
            outdesc.Flags = (outdesc.Flags & ~RPI_555_SUPP) | RPI_565_SUPP;
            for (int y = 0; y < height_; y++) {
                const uint16_t* s = reinterpret_cast<const uint16_t*>(src_ + y * instride);
                uint16_t* d = reinterpret_cast<uint16_t*>(
                                  src_buffer + (y + kPluginCtx) * paddedSrcPitch) +
                              kPluginCtx;
                for (int x = 0; x < width_; x++) d[x] = convert_pixel_565(s[x]);
                for (int k = 1; k <= kPluginCtx; k++) { d[-k] = d[0]; d[width_ - 1 + k] = d[width_ - 1]; }
            }
        } else if (bpp == 2) {
            for (int y = 0; y < height_; y++) {
                const uint16_t* s = reinterpret_cast<const uint16_t*>(src_ + y * instride);
                uint16_t* d = reinterpret_cast<uint16_t*>(
                                  src_buffer + (y + kPluginCtx) * paddedSrcPitch) +
                              kPluginCtx;
                memcpy(d, s, width_ * 2);
                for (int k = 1; k <= kPluginCtx; k++) { d[-k] = d[0]; d[width_ - 1 + k] = d[width_ - 1]; }
            }
        }

        for (int k = 1; k <= kPluginCtx; k++) {
            memcpy(src_buffer + (kPluginCtx - k) * paddedSrcPitch,
                   src_buffer + kPluginCtx * paddedSrcPitch, paddedSrcPitch);
            memcpy(src_buffer + (kPluginCtx + height_ - 1 + k) * paddedSrcPitch,
                   src_buffer + (kPluginCtx + height_ - 1) * paddedSrcPitch, paddedSrcPitch);
        }

        outdesc.SrcPtr = src_buffer + kPluginCtx * paddedSrcPitch + kPluginCtx * bpp;
        outdesc.DstPtr = dst_buffer;
        {
            std::lock_guard<std::mutex> lock(g_rpi_output_mutex);
            rpi_->Output(&outdesc);
        }

        // Convert plugin output to VBA-M's native format in place.
        if (bpp == 4) {
            for (int y = 0; y < scaled_height; y++) {
                uint32_t* d = reinterpret_cast<uint32_t*>(dst_ + y * outstride);
                for (int x = 0; x < scaled_width; x++) {
                    uint32_t p = d[x];
                    d[x] = (p & 0x0000FF00u) | ((p >> 16) & 0xFFu) | ((p & 0xFFu) << 16);
                }
            }
        } else if (rpi_using_rgb565_) {
            for (int y = 0; y < scaled_height; y++) {
                uint16_t* d = reinterpret_cast<uint16_t*>(dst_ + y * outstride);
                for (int x = 0; x < scaled_width; x++) {
                    uint16_t p = d[x];
                    uint8_t r = (p >> 11) & 0x1f, g6 = (p >> 5) & 0x3f, b = p & 0x1f;
                    d[x] = (r << 10) | ((g6 >> 1) << 5) | b;
                }
            }
        }
    }

    std::thread thread_;
    std::mutex lock_;
    std::condition_variable sig_;
    bool work_pending_ = false;

    // Pre-allocated conversion buffers (reused across frames)
    uint32_t* src2_ = nullptr;
    uint32_t* dst2_ = nullptr;
    size_t src2_size_ = 0;
    size_t dst2_size_ = 0;
};

// ---------------------------------------------------------------------------
// DrawingPanelBase
// ---------------------------------------------------------------------------

DrawingPanelBase::DrawingPanelBase(int _width, int _height)
    : width(_width), height(_height), scale(1) {
    memset(delta, 0xff, sizeof(delta));
    memset(&rpi_info_, 0, sizeof(rpi_info_));

    if (OPTION(kDispFilter) == config::Filter::kPlugin) {
        const QString pluginPath = OPTION(kDispFilterPlugin);
        RENDER_PLUGIN_INFO* plugin_info =
            widgets::MaybeLoadFilterPlugin(pluginPath, &filter_plugin_);
        if (plugin_info) {
            // Copy to local storage so the panel owns its own flags.
            rpi_info_ = *plugin_info;
            rpi_ = &rpi_info_;

            // Select the best color format the plugin supports: 565 > 555 > 888.
            bool using_rgb565 = false;
            if (rpi_->Flags & RPI_565_SUPP) {
                rpi_->Flags &= ~(RPI_555_SUPP | RPI_888_SUPP);
                panel_color_depth_ = 16;
                systemColorDepth = 16;
                rpi_bpp_ = 2;
                using_rgb565 = true;
            } else if (rpi_->Flags & RPI_555_SUPP) {
                rpi_->Flags &= ~(RPI_565_SUPP | RPI_888_SUPP);
                panel_color_depth_ = 16;
                systemColorDepth = 16;
                rpi_bpp_ = 2;
            } else {
                rpi_->Flags &= ~(RPI_555_SUPP | RPI_565_SUPP);
                panel_color_depth_ = 32;
                systemColorDepth = 32;
                rpi_bpp_ = 4;
            }
            rpi_using_rgb565_ = using_rgb565;

            const unsigned int pluginVersion = rpi_->Flags & 0xff;
            const uint32_t scaleFlag = (rpi_->Flags & RPI_OUT_SCLMSK) >> RPI_OUT_SCLSH;
            rpi_is_mt_ = (strstr(rpi_->Name, " MT") != nullptr) || (pluginVersion == 1) ||
                         (scaleFlag >= 3) || (rpi_->Flags & RPI_MMX_USED) != 0;

            if ((rpi_->Flags & RPI_MMX_USED) && !(rpi_->Flags & RPI_MMX_REQD)) {
                rpi_->Flags &= ~RPI_MMX_USED;
            }

            if (!rpi_->Output) {
                rpi_->Output = reinterpret_cast<RENDPLUG_Output>(
                    filter_plugin_.resolve("RenderPluginOutput"));
            }
            uint32_t pluginScale = (rpi_->Flags & RPI_OUT_SCLMSK) >> RPI_OUT_SCLSH;
            if (pluginScale == 0) {
                pluginScale = (pluginVersion == 1) ? 2 : 1;
            }
            if (scale < 1.0)
                scale = 1.0;
            scale *= pluginScale;
        } else {
            // Plugin failed to load - fall back to no filter.
            OPTION(kDispFilterPlugin) = QString();
            OPTION(kDispFilter) = config::Filter::kNone;
        }
    }

    if (OPTION(kDispFilter) != config::Filter::kPlugin) {
        scale *= GetFilterScale();
        panel_color_depth_ = (OPTION(kBitDepth) + 1) << 3;
        systemColorDepth = panel_color_depth_;
    }

    // Initialize color shifts based on the panel's color depth.
    if (panel_color_depth_ == 24 || panel_color_depth_ == 32) {
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
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
        if (rpi_ && rpi_using_rgb565_) {
            systemRedShift = 11;
            systemGreenShift = 5;
            systemBlueShift = 0;
            RGB_LOW_BITS_MASK = 0x0821;
        } else {
            systemRedShift = 10;
            systemGreenShift = 5;
            systemBlueShift = 0;
            RGB_LOW_BITS_MASK = 0x0421;
        }
    }
}

void DrawingPanelBase::DrawingPanelInit() {
    did_init = true;
}

void DrawingPanelBase::PresentFrame() {
    if (QWidget* w = GetWindow())
        w->update();
}

void DrawingPanelBase::Destroy() {
    StopFilterThreads();
    if (QWidget* w = GetWindow()) {
        w->hide();
        w->setParent(nullptr);
        w->deleteLater();
    }
}

void DrawingPanelBase::DrawArea(uint8_t** data) {
    ApplyPendingFilterChange();

    const int outbpp = panel_color_depth_ >> 3;
    const int inrb = (panel_color_depth_ == 8) ? 4 : (panel_color_depth_ == 16) ? 2
                     : (panel_color_depth_ == 24) ? 0 : 1;
    const int outstride = static_cast<int>(std::ceil((width + inrb) * outbpp * scale));

    const bool filtering = OPTION(kDispFilter) != config::Filter::kNone ||
                           OPTION(kDispIFB) != config::Interframe::kNone;

    if (filtering) {
        if (!pixbuf2) {
            int allocstride = outstride, alloch = height;
            // gb may write borders, so allocate enough for them
            if (width == GameArea::GBWidth && height == GameArea::GBHeight) {
                allocstride = static_cast<int>(std::ceil((GameArea::SGBWidth + inrb) * outbpp * scale));
                alloch = GameArea::SGBHeight;
            }
            pixbuf2 = static_cast<uint8_t*>(
                calloc(allocstride, static_cast<int>(std::ceil((alloch + 2) * scale))));
        }
        todraw = pixbuf2;
    } else {
        // No filter: use g_pix directly.
        todraw = *data;
    }

    const int max_threads = rpi_is_mt_ ? 1 : GetMaxFilterThreads();
    const int instride = (width + inrb) * (panel_color_depth_ >> 3);

    if (filtering) {
        auto setup = [&](FilterThread& t, int i, int n) {
            t.threadno_ = i;
            t.nthreads_ = n;
            t.width_ = width;
            t.height_ = height;
            t.scale_ = scale;
            t.src_ = *data;
            t.dst_ = todraw;
            t.delta_ = delta;
            t.rpi_ = rpi_;
            t.rpi_bpp_ = rpi_bpp_;
            t.rpi_using_rgb565_ = rpi_using_rgb565_;
            t.panel_color_depth_ = panel_color_depth_;
            t.phase_ = FilterThread::Phase::Filter;
            t.done_ = &filt_done;
            t.ready_ = &filt_ready;
        };

        if (nthreads != max_threads) {
            StopFilterThreads();
            nthreads = max_threads;
            threads = new FilterThread[nthreads];

            if (OPTION(kDispIFB) != config::Interframe::kNone) {
                InterframeManager::Instance().Init(width, height, instride, nthreads);
            }

            // first time around, no threading in order to avoid static
            // initializer conflicts
            setup(threads[0], 0, 1);
            threads[0].Entry();

            if (nthreads > 1) {
                for (int i = 0; i < nthreads; i++) {
                    setup(threads[i], i, nthreads);
                    threads[i].Start();
                }
                for (int i = 0; i < nthreads; i++)
                    filt_ready.Wait();
            }
        } else if (nthreads == 1) {
            setup(threads[0], 0, 1);
            threads[0].Entry();
        } else {
            // Phase 1: IFB
            if (OPTION(kDispIFB) != config::Interframe::kNone) {
                for (int i = 0; i < nthreads; i++) {
                    threads[i].phase_ = FilterThread::Phase::IFB;
                    threads[i].Signal(*data);
                }
                for (int i = 0; i < nthreads; i++)
                    filt_done.Wait();
            }

            // Phase 2: Filter
            for (int i = 0; i < nthreads; i++) {
                threads[i].phase_ = FilterThread::Phase::Filter;
                threads[i].Signal(*data);
            }
            for (int i = 0; i < nthreads; i++)
                filt_done.Wait();

            // Phase 3: fix seams between thread regions.
            if (nthreads > 1 && OPTION(kDispFilter) != config::Filter::kNone) {
                std::vector<std::pair<int, int>> seamBands;
                for (int i = 1; i < nthreads; i++) {
                    int seamY = height * i / nthreads;
                    int bandStart = std::max(0, seamY - kFilterContextRadius - kSeamMarginRows);
                    int bandEnd = std::min(height, seamY + kFilterContextRadius + kSeamMarginRows);
                    if (!seamBands.empty() && bandStart <= seamBands.back().second) {
                        seamBands.back().second = std::max(seamBands.back().second, bandEnd);
                    } else {
                        seamBands.push_back({bandStart, bandEnd});
                    }
                }

                const int scale_int = static_cast<int>(scale);
                uint8_t* src_base = *data + instride;
                uint8_t* dst_base = todraw + outstride * scale_int;
                const int numBands = static_cast<int>(seamBands.size());
                for (int i = 0; i < nthreads; i++) {
                    threads[i].phase_ = FilterThread::Phase::SeamFix;
                    if (i < numBands) {
                        threads[i].seamBandStart_ = seamBands[i].first;
                        threads[i].seamBandEnd_ = seamBands[i].second;
                    } else {
                        threads[i].seamBandStart_ = -1;
                        threads[i].seamBandEnd_ = -1;
                    }
                    threads[i].seamSrc_ = src_base;
                    threads[i].seamDst_ = dst_base;
                    threads[i].Signal(src_base);
                }
                for (int i = 0; i < nthreads; i++)
                    filt_done.Wait();
            }
        }
    }

    // Draw OSD text directly into the output buffer.
    MainWindow* mf = vbamApp().frame;
    if (mf && (mf->isFullScreen() || !OPTION(kPrefDisableStatus))) {
        GameArea* panel = mf->GetPanel();
        const int scaled_width = static_cast<int>(std::ceil(width * scale));
        const int scaled_height = static_cast<int>(std::ceil(height * scale));

        if (panel && !panel->osdstat.isEmpty()) {
            const int x = static_cast<int>(std::ceil(4 * scale));
            const int y = static_cast<int>(std::ceil(4 * scale));
            drawTextQt(todraw + outstride * (panel_color_depth_ != 24), outstride, x, y,
                       panel->osdstat, scaled_width, scaled_height, scale, panel_color_depth_);
        }

        if (panel && !panel->osdtext.isEmpty()) {
            if (systemGetClock() - panel->osdtime < OSD_TIME) {
                const int x = static_cast<int>(std::ceil(4 * scale));
                const int osd_offset = (panel->game_type() == IMAGE_GB) ? 106 : 64;
                const int y = scaled_height - static_cast<int>(std::ceil(osd_offset * scale));
                drawTextQt(todraw + outstride * (panel_color_depth_ != 24), outstride, x, y,
                           panel->osdtext, scaled_width, scaled_height, scale,
                           panel_color_depth_);
            } else {
                panel->osdtext.clear();
            }
        }
    }

    if (!todraw)
        return;

    PresentFrame();
}

void DrawingPanelBase::DrawOSD(QPainter& painter, int w, int h) {
    // Fallback OSD drawn with the toolkit (not used by default: the OSD is
    // rendered into the frame buffer so every renderer shows it the same way).
    MainWindow* mf = vbamApp().frame;
    if (!mf)
        return;
    GameArea* panel = mf->GetPanel();
    if (!panel)
        return;
    painter.setPen(Qt::red);
    if (!panel->osdstat.isEmpty())
        painter.drawText(10, 20, panel->osdstat);
    if (!panel->osdtext.isEmpty() && systemGetClock() - panel->osdtime >= OSD_TIME)
        panel->osdtext.clear();
    if (!OPTION(kPrefDisableStatus) && !panel->osdtext.isEmpty()) {
        QRect r(10, 0, w - 20, h - 14);
        painter.drawText(r, Qt::AlignLeft | Qt::AlignBottom | Qt::TextWrapAnywhere,
                         panel->osdtext);
    }
}

QImage DrawingPanelBase::BuildImage() const {
    const int scaled_width = static_cast<int>(std::ceil(width * scale));
    const int scaled_height = static_cast<int>(std::ceil(height * scale));
    if (!todraw || scaled_width <= 0 || scaled_height <= 0)
        return QImage();

    QImage im(scaled_width, scaled_height, QImage::Format_RGB32);
    const bool filtered = OPTION(kDispFilter) != config::Filter::kNone;

    if (panel_color_depth_ == 24) {
        // 24-bit: no borders, RGB byte order, scaled by filters (if any).
        const uint8_t* src = todraw;
        for (int y = 0; y < scaled_height; y++) {
            QRgb* dst = reinterpret_cast<QRgb*>(im.scanLine(y));
            for (int x = 0; x < scaled_width; x++, src += 3)
                dst[x] = qRgb(src[0], src[1], src[2]);
        }
    } else if (panel_color_depth_ == 8) {
        const int inrb = 4;
        const int scaled_stride = static_cast<int>(std::ceil((width + inrb) * scale));
        const uint8_t* src = todraw + scaled_stride;  // skip top border row
        for (int y = 0; y < scaled_height; y++) {
            QRgb* dst = reinterpret_cast<QRgb*>(im.scanLine(y));
            for (int x = 0; x < scaled_width; x++, src++) {
                if (*src == 0xff) {
                    dst[x] = qRgb(255, 255, 255);
                } else {
                    dst[x] = qRgb(((*src >> 5) & 0x7) << 5, ((*src >> 2) & 0x7) << 5,
                                  (*src & 0x3) << 6);
                }
            }
            src += static_cast<int>(std::ceil(inrb * scale));
        }
    } else if (panel_color_depth_ == 16) {
        const int inrb = 2;
        const int scaled_stride = static_cast<int>(std::ceil((width + inrb) * scale));
        const uint16_t* src = reinterpret_cast<const uint16_t*>(todraw) + scaled_stride;
        for (int y = 0; y < scaled_height; y++) {
            QRgb* dst = reinterpret_cast<QRgb*>(im.scanLine(y));
            for (int x = 0; x < scaled_width; x++, src++) {
                dst[x] = qRgb(((*src >> 10) & 0x1f) << 3, ((*src >> 5) & 0x1f) << 3,
                              (*src & 0x1f) << 3);
            }
            src += static_cast<int>(std::ceil(inrb * scale));
        }
    } else if (filtered) {
        const int inrb = 1;
        const int scaled_stride = static_cast<int>(std::ceil((width + inrb) * scale));
        const uint32_t* src = reinterpret_cast<const uint32_t*>(todraw) + scaled_stride;
        for (int y = 0; y < scaled_height; y++) {
            QRgb* dst = reinterpret_cast<QRgb*>(im.scanLine(y));
            for (int x = 0; x < scaled_width; x++, src++) {
                dst[x] = qRgb((*src >> (systemRedShift - 3)) & 0xff,
                              (*src >> (systemGreenShift - 3)) & 0xff,
                              (*src >> (systemBlueShift - 3)) & 0xff);
            }
            src += static_cast<int>(std::ceil(inrb * scale));
        }
    } else {  // 32 bit, unscaled
        const uint32_t* src = reinterpret_cast<const uint32_t*>(todraw) +
                              static_cast<int>(std::ceil((width + 1) * scale));
        for (int y = 0; y < scaled_height; y++) {
            QRgb* dst = reinterpret_cast<QRgb*>(im.scanLine(y));
            for (int x = 0; x < scaled_width; x++, src++) {
                dst[x] = qRgb((*src >> (systemRedShift - 3)) & 0xff,
                              (*src >> (systemGreenShift - 3)) & 0xff,
                              (*src >> (systemBlueShift - 3)) & 0xff);
            }
            ++src;  // skip rhs border
        }
    }
    return im;
}

void DrawingPanelBase::StopFilterThreads() {
    if (nthreads) {
        if (nthreads > 1) {
            for (int i = 0; i < nthreads; i++) {
                threads[i].Signal(nullptr);
                threads[i].Join();
            }
        }
        delete[] threads;
        threads = nullptr;
        nthreads = 0;
    }
}

void DrawingPanelBase::ApplyInPlaceFilterChange() {
    // Only arm the change here; DrawArea() applies it on the next frame so the
    // scale and the buffers stay mutually consistent.
    pending_filter_change_ = true;
}

void DrawingPanelBase::ApplyPendingFilterChange() {
    if (!pending_filter_change_)
        return;
    pending_filter_change_ = false;
    scale = GetFilterScale();
    StopFilterThreads();
    if (pixbuf2 && pixbuf2 != g_pix && pixbuf1 != pixbuf2)
        free(pixbuf2);
    pixbuf2 = nullptr;
    memset(delta, 0xff, sizeof(delta));
}

DrawingPanelBase::~DrawingPanelBase() {
    StopFilterThreads();

    // pixbuf2 may be our allocated filter output buffer, or point to g_pix when
    // no filters are used. Only free if it's our buffer.
    if (pixbuf2 && pixbuf2 != g_pix && pixbuf1 != pixbuf2) {
        free(pixbuf2);
        pixbuf2 = nullptr;
    }

    InterframeCleanup();

    if (filter_plugin_.isLoaded())
        filter_plugin_.unload();
}

// ---------------------------------------------------------------------------
// SoftwareDrawingPanel
// ---------------------------------------------------------------------------

SoftwareDrawingPanel::SoftwareDrawingPanel(QWidget* parent, int _width, int _height)
    : QWidget(parent), DrawingPanelBase(_width, _height) {
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAttribute(Qt::WA_NoSystemBackground);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setAutoFillBackground(false);
    DrawingPanelInit();
}

SoftwareDrawingPanel::~SoftwareDrawingPanel() {
    StopFilterThreads();
}

void SoftwareDrawingPanel::paintEvent(QPaintEvent* event) {
    (void)event;
    QPainter painter(this);
    if (!todraw) {
        painter.fillRect(rect(), Qt::black);
        return;
    }
    DrawArea(painter);
}

void SoftwareDrawingPanel::DrawArea(QPainter& painter) {
    const QImage im = BuildImage();
    if (im.isNull()) {
        painter.fillRect(rect(), Qt::black);
        return;
    }
    painter.setRenderHint(QPainter::SmoothPixmapTransform, OPTION(kDispBilinear));
    painter.drawImage(rect(), im);
}

// ---------------------------------------------------------------------------
// GLDrawingPanel
// ---------------------------------------------------------------------------

namespace {

const char* kGlVertexShader = R"(
attribute vec2 a_pos;
attribute vec2 a_tex;
varying vec2 v_tex;
void main() {
    v_tex = a_tex;
    gl_Position = vec4(a_pos, 0.0, 1.0);
}
)";

const char* kGlFragmentShader = R"(
#ifdef GL_ES
precision mediump float;
#endif
uniform sampler2D u_tex;
uniform bool u_swizzle;
varying vec2 v_tex;
void main() {
    vec4 c = texture2D(u_tex, v_tex);
    gl_FragColor = u_swizzle ? vec4(c.b, c.g, c.r, 1.0) : vec4(c.rgb, 1.0);
}
)";

QOpenGLShaderProgram* g_gl_program = nullptr;

}  // namespace

GLDrawingPanel::GLDrawingPanel(QWidget* parent, int _width, int _height)
    : QOpenGLWidget(parent), DrawingPanelBase(_width, _height) {
    QSurfaceFormat fmt = QSurfaceFormat::defaultFormat();
    fmt.setSwapInterval(OPTION(kPrefVsync) ? 1 : 0);
    setFormat(fmt);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setUpdateBehavior(QOpenGLWidget::NoPartialUpdate);
}

GLDrawingPanel::~GLDrawingPanel() {
    StopFilterThreads();
    makeCurrent();
    if (did_init && texid) {
        glDeleteTextures(1, &texid);
        texid = 0;
    }
    delete g_gl_program;
    g_gl_program = nullptr;
    doneCurrent();
}

void GLDrawingPanel::PresentFrame() {
    texture_dirty_ = true;
    update();
}

void GLDrawingPanel::initializeGL() {
    if (!QOpenGLContext::currentContext()) {
        init_failed_ = true;
        return;
    }
    initializeOpenGLFunctions();
    DrawingPanelInit();
}

void GLDrawingPanel::DrawingPanelInit() {
    if (!QOpenGLContext::currentContext()) {
        init_failed_ = true;
        return;
    }

    delete g_gl_program;
    g_gl_program = new QOpenGLShaderProgram();
    if (!g_gl_program->addShaderFromSourceCode(QOpenGLShader::Vertex, kGlVertexShader) ||
        !g_gl_program->addShaderFromSourceCode(QOpenGLShader::Fragment, kGlFragmentShader) ||
        !g_gl_program->link()) {
        vbam::LogDebug(QStringLiteral("OpenGL shader setup failed: %1").arg(g_gl_program->log()));
        delete g_gl_program;
        g_gl_program = nullptr;
        init_failed_ = true;
        return;
    }

    DrawingPanelBase::DrawingPanelInit();

    const bool bilinear = OPTION(kDispBilinear);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glGenTextures(1, &texid);
    glBindTexture(GL_TEXTURE_2D, texid);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, bilinear ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, bilinear ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    tex_w_ = tex_h_ = 0;
    texture_dirty_ = true;
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    AdjustViewport();
}

void GLDrawingPanel::resizeGL(int w, int h) {
    (void)w;
    (void)h;
    AdjustViewport();
}

void GLDrawingPanel::AdjustViewport() {
    if (!QOpenGLContext::currentContext())
        return;
    const qreal dpr = devicePixelRatioF();
    glViewport(0, 0, static_cast<int>(std::lround(QWidget::width() * dpr)),
               static_cast<int>(std::lround(QWidget::height() * dpr)));
}

void GLDrawingPanel::paintGL() {
    if (init_failed_ || !did_init || !g_gl_program) {
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        return;
    }

    glClear(GL_COLOR_BUFFER_BIT);
    if (!todraw)
        return;

    const int tex_width = static_cast<int>(std::ceil(DrawingPanelBase::width * scale));
    const int tex_height = static_cast<int>(std::ceil(DrawingPanelBase::height * scale));
    if (tex_width <= 0 || tex_height <= 0)
        return;

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texid);

    bool swizzle = false;
    if (texture_dirty_) {
        texture_dirty_ = false;
        if (panel_color_depth_ == 32) {
            // Direct upload: the 32-bit frame is R,G,B,x bytes (systemRedShift 3).
            const int inrb = 1;
            const int rowlen = static_cast<int>(std::ceil((DrawingPanelBase::width + inrb) * scale));
            const uint8_t* tex_ptr = todraw + rowlen * 4;  // skip top border row
            if (tex_w_ != tex_width || tex_h_ != tex_height) {
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tex_width, tex_height, 0, GL_RGBA,
                             GL_UNSIGNED_BYTE, nullptr);
                tex_w_ = tex_width;
                tex_h_ = tex_height;
            }
            // Row length: upload row by row (portable to GL ES, which lacks
            // GL_UNPACK_ROW_LENGTH in 2.0).
            for (int y = 0; y < tex_height; y++) {
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, y, tex_width, 1, GL_RGBA, GL_UNSIGNED_BYTE,
                                tex_ptr + static_cast<size_t>(y) * rowlen * 4);
            }
            swizzle_cache_ = false;
        } else {
            // Other depths: convert to RGB32 (B,G,R,A bytes on LE) and swizzle
            // in the shader.
            const QImage im = BuildImage();
            if (!im.isNull()) {
                if (tex_w_ != tex_width || tex_h_ != tex_height) {
                    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tex_width, tex_height, 0, GL_RGBA,
                                 GL_UNSIGNED_BYTE, nullptr);
                    tex_w_ = tex_width;
                    tex_h_ = tex_height;
                }
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, tex_width, tex_height, GL_RGBA,
                                GL_UNSIGNED_BYTE, im.constBits());
            }
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
            swizzle_cache_ = true;
#else
            swizzle_cache_ = false;
#endif
        }
    }
    swizzle = swizzle_cache_;

    static const GLfloat verts[] = {
        // x, y, u, v
        -1.0f, 1.0f, 0.0f, 0.0f,
        1.0f, 1.0f, 1.0f, 0.0f,
        -1.0f, -1.0f, 0.0f, 1.0f,
        1.0f, -1.0f, 1.0f, 1.0f,
    };

    g_gl_program->bind();
    g_gl_program->setUniformValue("u_tex", 0);
    g_gl_program->setUniformValue("u_swizzle", swizzle);
    const int pos_loc = g_gl_program->attributeLocation("a_pos");
    const int tex_loc = g_gl_program->attributeLocation("a_tex");
    glEnableVertexAttribArray(pos_loc);
    glEnableVertexAttribArray(tex_loc);
    glVertexAttribPointer(pos_loc, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), verts);
    glVertexAttribPointer(tex_loc, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), verts + 2);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisableVertexAttribArray(pos_loc);
    glDisableVertexAttribArray(tex_loc);
    g_gl_program->release();
}

void GLDrawingPanel::DrawArea(QPainter& painter) {
    // Presentation happens in paintGL(); nothing to do with a QPainter here.
    (void)painter;
}

// ---------------------------------------------------------------------------
// NativeDrawingPanel

NativeDrawingPanel::NativeDrawingPanel(QWidget* parent, int _width, int _height)
    : QWidget(parent), DrawingPanelBase(_width, _height) {
    // Own native window, no Qt painting: the API owns every pixel of it.
    setAttribute(Qt::WA_NativeWindow, true);
    setAttribute(Qt::WA_PaintOnScreen, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setAutoFillBackground(false);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    // Realize the native window right away so the constructor of the concrete
    // renderer can hand its handle to the graphics API.
    (void)winId();
}

NativeDrawingPanel::~NativeDrawingPanel() {
    StopFilterThreads();
}

void* NativeDrawingPanel::NativeHandle() {
    return reinterpret_cast<void*>(winId());
}

QSize NativeDrawingPanel::DevicePixelSize() const {
    const qreal dpr = devicePixelRatioF();
    // QWidget::width()/height() (the window size); DrawingPanelBase::width/
    // height are the emulated frame size and would be ambiguous unqualified.
    return QSize(std::max(1, static_cast<int>(std::ceil(QWidget::width() * dpr))),
                 std::max(1, static_cast<int>(std::ceil(QWidget::height() * dpr))));
}

void NativeDrawingPanel::PresentFrame() {
    Present();
}

void NativeDrawingPanel::paintEvent(QPaintEvent* event) {
    (void)event;
    Present();
}

void NativeDrawingPanel::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    OnNativeResize(DevicePixelSize());
    if (todraw)
        Present();
}

int NativeDrawingPanel::SourcePitch() const {
    const int inrb = (panel_color_depth_ == 8) ? 4 : (panel_color_depth_ == 16) ? 2
                     : (panel_color_depth_ == 24) ? 0 : 1;
    switch (panel_color_depth_) {
        case 8:
            return static_cast<int>(std::ceil((DrawingPanelBase::width + inrb) * scale));
        case 16:
            return static_cast<int>(std::ceil((DrawingPanelBase::width + inrb) * scale * 2));
        case 24:
            return static_cast<int>(std::ceil(DrawingPanelBase::width * scale * 3));
        default:
            return static_cast<int>(std::ceil((DrawingPanelBase::width + inrb) * scale * 4));
    }
}

const uint8_t* NativeDrawingPanel::SourcePixels() const {
    if (!todraw)
        return nullptr;
    // Every depth but 24-bit carries a border row on top (the OSD text is drawn
    // at `todraw + outstride * (depth != 24)` for the same reason).
    return panel_color_depth_ == 24 ? todraw : todraw + SourcePitch();
}

int NativeDrawingPanel::ScaledWidth() const {
    return static_cast<int>(std::ceil(DrawingPanelBase::width * scale));
}

int NativeDrawingPanel::ScaledHeight() const {
    return static_cast<int>(std::ceil(DrawingPanelBase::height * scale));
}
