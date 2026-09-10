#ifndef VBAM_QT_RENDERERS_SDL_PANEL_H_
#define VBAM_QT_RENDERERS_SDL_PANEL_H_

#include <QString>

#ifdef ENABLE_SDL3
#include <SDL3/SDL.h>
#else
#include <SDL.h>
#endif

#include "qt/drawing-panel.h"

// SDL_Renderer presenting into this panel's native window (the "SDL" render
// method). The backend is chosen with the kSDLRenderer option ("default" lets
// SDL pick: Metal on macOS, Direct3D on Windows, OpenGL elsewhere), so this one
// render method covers whatever SDL can drive on the machine. Port of the wx
// SDLDrawingPanel without its HDR / deep-color / Wayland-subsurface paths.
class SDLDrawingPanel final : public NativeDrawingPanel {
    Q_OBJECT

public:
    SDLDrawingPanel(QWidget* parent, int _width, int _height);
    ~SDLDrawingPanel() override;

    // Names of the SDL render drivers available in this SDL build ("default"
    // first), for the display dialog's backend picker.
    static QStringList AvailableRenderers();

protected:
    void DrawingPanelInit() override;
    void Present() override;

private:
    bool CreateSdlWindow();
    bool CreateRenderer(const QString& requested);
    bool CreateTexture();
    void Teardown();

    SDL_Window* sdlwindow_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    SDL_Texture* texture_ = nullptr;
    QString renderername_;
    int texture_w_ = 0, texture_h_ = 0;
    // Scratch for the byte-order conversions the direct3d backend needs.
    std::vector<uint8_t> convert_buf_;

#if defined(Q_OS_MACOS)
    // SDL makes the view it is given the window's contentView; these undo it
    // (see the mac-support helpers).
    void* saved_window_ = nullptr;
    void* saved_content_view_ = nullptr;
    void* saved_superview_ = nullptr;
    double saved_frame_[4] = {0.0, 0.0, 0.0, 0.0};
#endif
};

#endif  // VBAM_QT_RENDERERS_SDL_PANEL_H_
