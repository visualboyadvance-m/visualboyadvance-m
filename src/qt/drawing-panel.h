#ifndef VBAM_QT_DRAWING_PANEL_H_
#define VBAM_QT_DRAWING_PANEL_H_

#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <vector>

#include <QImage>
#include <QLibrary>
#include <QOpenGLFunctions>
#include <QOpenGLWidget>
#include <QString>
#include <QWidget>

#include "qt/rpi.h"

class QPainter;
class FilterThread;

// Counting semaphore (C++17; std::counting_semaphore is C++20).
class Semaphore {
public:
    explicit Semaphore(int count = 0) : count_(count) {}
    void Post(int n = 1) {
        std::lock_guard<std::mutex> lock(m_);
        count_ += n;
        if (n == 1) cv_.notify_one(); else cv_.notify_all();
    }
    void Wait() {
        std::unique_lock<std::mutex> lock(m_);
        cv_.wait(lock, [this] { return count_ > 0; });
        --count_;
    }
    bool TryWait() {
        std::lock_guard<std::mutex> lock(m_);
        if (count_ <= 0) return false;
        --count_;
        return true;
    }
private:
    std::mutex m_;
    std::condition_variable cv_;
    int count_;
};

// Shared implementation of the frame pipeline used by every renderer:
// receives the core's frame (DrawArea(uint8_t**)), runs the display filter
// (optionally multi-threaded, or through a filter plugin), the interframe
// blender, and hands the result (`todraw`, `width*scale` x `height*scale`
// pixels at the panel's color depth) to the renderer-specific DrawArea().
// Also draws the OSD (status text, screen messages).
//
// Equivalent of the wx port's DrawingPanelBase. The concrete panels below are
// QWidgets; GetWindow() returns the widget for layout/event purposes.
class DrawingPanelBase {
public:
    DrawingPanelBase(int _width, int _height);
    virtual ~DrawingPanelBase();

    // Called once per emulated frame with the core's pixel buffer. Filters the
    // frame into `todraw` and presents it (PresentFrame()).
    void DrawArea(uint8_t** pixels);

    // Synchronously stop filter threads - call before destroying the panel.
    void StopFilterThreads();

    // In-place display filter change (no panel rebuild); see the wx port.
    virtual bool SupportsInPlaceFilterChange() const { return true; }
    void ApplyInPlaceFilterChange();
    void ApplyPendingFilterChange();
    bool IsUsingFilterPlugin() const { return rpi_ != nullptr; }

    // Requests a repaint of the widget presenting `todraw`.
    virtual void PresentFrame();

    virtual QWidget* GetWindow() = 0;
    virtual void Destroy();

    // True once DrawingPanelInit() has run.
    bool DrawingInitialized() const { return did_init; }
    // True if this renderer failed to come up (no GL context ...). GameArea
    // falls back to the software renderer.
    bool DrawingInitFailed() const { return init_failed_; }

    int panel_color_depth() const { return panel_color_depth_; }
    double scale_factor() const { return scale; }

    // Paints the OSD (speed/status at top-left, screen message at the bottom)
    // over the presented frame. `w`/`h` is the presented area size in widget
    // pixels. Called by the concrete panels from their paint path.
    void DrawOSD(QPainter& painter, int w, int h);

    // Fills the current `todraw` buffer into a QImage (RGB32) of size
    // width*scale x height*scale. Used by the software renderer and for the
    // GL texture upload.
    QImage BuildImage() const;

protected:
    // Renderer-specific presentation of `todraw`; called from the widget's
    // paint path.
    virtual void DrawArea(QPainter& painter) = 0;
    virtual void DrawingPanelInit();

    int width, height;
    double scale;
    bool did_init = false;
    bool init_failed_ = false;
    bool pending_filter_change_ = false;
    uint8_t* todraw = nullptr;
    uint8_t *pixbuf1 = nullptr, *pixbuf2 = nullptr;
    FilterThread* threads = nullptr;
    int nthreads = 0;
    Semaphore filt_done;
    Semaphore filt_ready;  // Posted by threads when they're ready to receive work
    QLibrary filter_plugin_;
    RENDER_PLUGIN_INFO* rpi_ = nullptr; // also flag indicating plugin loaded
    RENDER_PLUGIN_INFO rpi_info_;
    bool rpi_using_rgb565_ = false;
    bool rpi_is_mt_ = false;
    int rpi_bpp_ = 4;
    int panel_color_depth_ = 16; // Color depth for this panel (may differ from systemColorDepth)
    // largest buffer required is 32-bit * (max width + 1) * (max height + 2)
    uint8_t delta[257 * 4 * 226];
};

// Software renderer: converts `todraw` to a QImage and paints it scaled with
// QPainter (nearest or bilinear per kDispBilinear). "Simple" render method.
class SoftwareDrawingPanel final : public QWidget, public DrawingPanelBase {
    Q_OBJECT

public:
    SoftwareDrawingPanel(QWidget* parent, int _width, int _height);
    ~SoftwareDrawingPanel() override;

    QWidget* GetWindow() override { return this; }

protected:
    void paintEvent(QPaintEvent* event) override;
    void DrawArea(QPainter& painter) override;
};

// OpenGL renderer: uploads `todraw` as a texture and draws a textured quad
// with QOpenGLWidget (nearest or linear sampling, vsync per kPrefVsync via the
// swap interval of the surface format).
class GLDrawingPanel final : public QOpenGLWidget,
                             protected QOpenGLFunctions,
                             public DrawingPanelBase {
    Q_OBJECT

public:
    GLDrawingPanel(QWidget* parent, int _width, int _height);
    ~GLDrawingPanel() override;

    QWidget* GetWindow() override { return this; }
    void PresentFrame() override;

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;
    void DrawArea(QPainter& painter) override;
    void DrawingPanelInit() override;

private:
    void AdjustViewport();
    unsigned texid = 0;
    int tex_w_ = 0, tex_h_ = 0;
    bool texture_dirty_ = true;
    bool swizzle_cache_ = false;  // shader swizzle needed for the last upload
};

// Base of the renderers that present through a native window handle (SDL,
// Vulkan, Direct3D, Metal) rather than through Qt's paint system. The widget is
// forced to have its own native window (NSView / HWND / X11 window), Qt is told
// not to paint it, and each frame is presented synchronously from
// PresentFrame() -- like the wx port, where DrawArea() ran the API's present
// right after the filter pass. An expose (paintEvent) re-presents the last
// frame; a resize hands the new size to the renderer first.
class NativeDrawingPanel : public QWidget, public DrawingPanelBase {
    Q_OBJECT

public:
    ~NativeDrawingPanel() override;

    QWidget* GetWindow() override { return this; }
    void PresentFrame() override;

    // The native window handle: NSView* on macOS, HWND on Windows, the X11
    // window id (as a pointer-sized integer) on xcb.
    void* NativeHandle();

    // The panel size in device pixels (size() * devicePixelRatio), which is what
    // swapchains and drawables are sized in.
    QSize DevicePixelSize() const;

protected:
    NativeDrawingPanel(QWidget* parent, int _width, int _height);

    // Presents `todraw` (or clears to black when there is no frame yet).
    virtual void Present() = 0;
    // Called with the new device-pixel size before the frame is re-presented.
    virtual void OnNativeResize(const QSize& device_pixels) { (void)device_pixels; }

    // Bytes per source row of `todraw` for the current color depth and scale,
    // and the first real row (the filters leave a border row on top for every
    // depth but 24-bit). The renderers fill their whole surface: GameArea
    // already sizes this widget to the aspect-fitted rectangle when "retain
    // aspect ratio" is on.
    int SourcePitch() const;
    const uint8_t* SourcePixels() const;
    int ScaledWidth() const;
    int ScaledHeight() const;

    QPaintEngine* paintEngine() const override { return nullptr; }
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void DrawArea(QPainter& painter) override { (void)painter; }
};

#if defined(__APPLE__)
// macOS Quartz 2D renderer (the wx port's "Quartz2D" output module): a
// software renderer like Simple, but presented natively. Each frame the
// filtered `todraw` buffer is wrapped in a CGImage and handed to a CALayer
// stacked on the panel's NSView, which CoreAnimation scales to the view
// (nearest or linear per kDispBilinear) and composites without going through
// Qt's backing store. Implemented in drawing-panel-quartz.mm.
class QuartzDrawingPanel final : public QWidget, public DrawingPanelBase {
    Q_OBJECT

public:
    QuartzDrawingPanel(QWidget* parent, int _width, int _height);
    ~QuartzDrawingPanel() override;

    QWidget* GetWindow() override { return this; }
    void PresentFrame() override;

protected:
    QPaintEngine* paintEngine() const override { return nullptr; }
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void DrawArea(QPainter& painter) override { (void)painter; }

private:
    // Creates the CALayer under the view's root layer on first use. False if
    // the native view is not available.
    bool EnsureLayer();
    // Sizes the layer to the view and refreshes its scale / filter settings.
    void SyncLayer();

    void* layer_ = nullptr;  // CALayer*, retained
};
#endif  // __APPLE__

#endif  // VBAM_QT_DRAWING_PANEL_H_
