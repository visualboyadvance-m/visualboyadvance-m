#ifndef VBAM_QT_RENDERERS_MAC_SUPPORT_H_
#define VBAM_QT_RENDERERS_MAC_SUPPORT_H_

// Objective-C++ helpers for the macOS renderers (implemented in
// mac-support.mm). Every handle is an opaque pointer so the callers stay plain
// C++: `ns_view` is the NSView* behind QWidget::winId().

// Guarantees `ns_view` is backed by a CAMetalLayer (installing one if needed)
// and returns it, or nullptr. Used by MoltenVK's VK_EXT_metal_surface and by
// the Metal renderer.
void* VbamQtEnsureMetalLayer(void* ns_view);

// Sizes a CAMetalLayer's drawable to `w` x `h` device pixels at `scale`.
void VbamQtMetalLayerResize(void* metal_layer, int w, int h, double scale);

// SDL turns the NSView it is handed into its window's contentView, detaching
// the Qt view hierarchy. Capture the state before SDL_CreateWindow and put it
// back afterwards.
// `frame` receives / restores the window's frame (x, y, w, h in screen
// points): SDL also repositions and resizes the host window to its own
// defaults when it adopts the view.
void VbamQtSdlCaptureViewState(void* ns_view, void** out_window, void** out_content_view,
                               void** out_superview, double out_frame[4]);
void VbamQtSdlReattachViewState(void* ns_view, void* window, void* content_view,
                                void* superview, const double frame[4]);

// Removes the CAMetalLayer-backed subviews SDL leaves on `ns_view` at teardown.
void VbamQtRemoveSdlMetalViews(void* ns_view);

// Developer aid (VBAM_QT_NO_ACTIVATE=1): makes this an accessory app, so a
// test instance launched from a terminal neither takes keyboard focus away
// from the foreground app nor shows in the Dock. Its window still renders.
void VbamQtSetAccessoryActivationPolicy();

#endif  // VBAM_QT_RENDERERS_MAC_SUPPORT_H_
