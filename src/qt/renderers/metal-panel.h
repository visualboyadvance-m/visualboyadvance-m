#ifndef VBAM_QT_RENDERERS_METAL_PANEL_H_
#define VBAM_QT_RENDERERS_METAL_PANEL_H_

#include "qt/drawing-panel.h"

#if defined(__APPLE__) && !defined(NO_METAL)

// Metal renderer: a CAMetalLayer on this panel's NSView, a textured quad
// pipeline compiled from source at run time, one streaming texture per frame.
// Port of the wx MetalDrawingPanel without its EDR/HDR path. Implemented in
// metal-panel.mm; the Objective-C objects are kept behind an opaque struct so
// this header stays plain C++.
class MetalDrawingPanel final : public NativeDrawingPanel {
    Q_OBJECT

public:
    MetalDrawingPanel(QWidget* parent, int _width, int _height);
    ~MetalDrawingPanel() override;

protected:
    void DrawingPanelInit() override;
    void Present() override;
    void OnNativeResize(const QSize& device_pixels) override;

private:
    struct Impl;
    Impl* impl_ = nullptr;
};

#endif  // __APPLE__ && !NO_METAL

#endif  // VBAM_QT_RENDERERS_METAL_PANEL_H_
