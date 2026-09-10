// macOS Quartz 2D renderer for the Qt frontend. See QuartzDrawingPanel in
// drawing-panel.h.
//
// The wx port's Quartz2DDrawingPanel drew a CGImage of the frame into the
// NSView from inside the view's drawRect:. Qt owns the NSView's drawing (its
// widgets are painted into a backing store and flushed to the view's layer), so
// there is no CGContext of ours to draw into at paint time. Instead the frame is
// presented the CoreAnimation way: the CGImage becomes the contents of a
// dedicated CALayer under the view's root layer, and the window server scales
// and composites it. Qt is told not to paint this widget at all
// (paintEngine() == nullptr, WA_PaintOnScreen), so the layer is never
// overdrawn.

#include "qt/drawing-panel.h"

#import <AppKit/AppKit.h>
#import <QuartzCore/QuartzCore.h>

#include <cmath>

#include <QPaintEvent>
#include <QResizeEvent>
#include <QWindow>

#include "qt/config/option-proxy.h"
#include "qt/log.h"

namespace {

// Manual retain/release: this target is built without ARC.
CALayer* Layer(void* layer) {
    return static_cast<CALayer*>(layer);
}

}  // namespace

QuartzDrawingPanel::QuartzDrawingPanel(QWidget* parent, int _width, int _height)
    : QWidget(parent), DrawingPanelBase(_width, _height) {
    // A native NSView of our own (not a Qt "alien" widget) so it has a layer
    // to attach to, and no Qt painting over it.
    setAttribute(Qt::WA_NativeWindow);
    setAttribute(Qt::WA_PaintOnScreen);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAttribute(Qt::WA_NoSystemBackground);
    setAutoFillBackground(false);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);

    DrawingPanelInit();

    if (!EnsureLayer()) {
        vbam::LogDebug(QStringLiteral("Quartz2D: no native view for the panel"));
        init_failed_ = true;
    }
}

QuartzDrawingPanel::~QuartzDrawingPanel() {
    StopFilterThreads();
    if (layer_) {
        CALayer* layer = Layer(layer_);
        [layer removeFromSuperlayer];
        [layer release];
        layer_ = nullptr;
    }
}

bool QuartzDrawingPanel::EnsureLayer() {
    if (layer_)
        return true;

    NSView* view = reinterpret_cast<NSView*>(winId());
    if (!view)
        return false;

    // Qt's views are layer-backed on every supported macOS; make sure.
    view.wantsLayer = YES;
    CALayer* root = view.layer;
    if (!root)
        return false;

    CALayer* layer = [CALayer layer];
    layer.anchorPoint = CGPointMake(0, 0);
    layer.backgroundColor = CGColorGetConstantColor(kCGColorBlack);
    layer.contentsGravity = kCAGravityResize;
    layer.opaque = YES;
    // The frame is emulator output: never animate a contents/size change.
    layer.actions = @{
        @"contents" : [NSNull null],
        @"bounds" : [NSNull null],
        @"position" : [NSNull null],
        @"contentsScale" : [NSNull null]
    };
    [root addSublayer:layer];
    layer_ = [layer retain];

    SyncLayer();
    return true;
}

void QuartzDrawingPanel::SyncLayer() {
    if (!layer_)
        return;
    NSView* view = reinterpret_cast<NSView*>(winId());
    CALayer* layer = Layer(layer_);

    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    layer.frame = view ? view.bounds : CGRectMake(0, 0, DrawingPanelBase::width, DrawingPanelBase::height);
    layer.contentsScale = view && view.window ? view.window.backingScaleFactor
                                              : devicePixelRatioF();
    NSString* const filter = OPTION(kDispBilinear) ? kCAFilterLinear : kCAFilterNearest;
    layer.magnificationFilter = filter;
    layer.minificationFilter = filter;
    [CATransaction commit];
}

void QuartzDrawingPanel::PresentFrame() {
    if (!todraw || !EnsureLayer())
        return;

    // BuildImage() yields the filtered frame (OSD included) as RGB32:
    // 0xffRRGGBB words, i.e. B,G,R,X bytes in memory on this little-endian
    // platform, which is CoreGraphics' 32-bit little-endian "skip first" layout.
    const QImage im = BuildImage();
    if (im.isNull())
        return;

    // Copy the pixels: the QImage goes away at the end of this call, and the
    // CGImage (kept alive by the layer until the next frame) must own its data.
    const size_t bytes = static_cast<size_t>(im.bytesPerLine()) * im.height();
    CFDataRef data = CFDataCreate(kCFAllocatorDefault, im.constBits(), bytes);
    CGDataProviderRef provider = CGDataProviderCreateWithCFData(data);
    CGColorSpaceRef color_space = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
    const CGBitmapInfo info = kCGBitmapByteOrder32Little | kCGImageAlphaNoneSkipFirst;
    CGImageRef image = CGImageCreate(im.width(), im.height(), 8, 32, im.bytesPerLine(),
                                     color_space, info, provider, nullptr, false,
                                     kCGRenderingIntentDefault);

    if (image) {
        SyncLayer();
        [CATransaction begin];
        [CATransaction setDisableActions:YES];
        Layer(layer_).contents = (id)image;
        [CATransaction commit];
        CGImageRelease(image);
    }

    CGColorSpaceRelease(color_space);
    CGDataProviderRelease(provider);
    CFRelease(data);
}

void QuartzDrawingPanel::paintEvent(QPaintEvent* event) {
    // Nothing to paint: the layer holds the last frame. Qt still needs the
    // event accepted so it does not try to fill the background.
    event->accept();
}

void QuartzDrawingPanel::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    SyncLayer();
}
