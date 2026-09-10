#include "qt/renderers/sdl-panel.h"

#include <cmath>
#include <cstring>

#include <QGuiApplication>
#include <QStringList>

#include "core/base/system.h"
#include "qt/config/option-proxy.h"
#include "qt/log.h"

#if defined(Q_OS_MACOS)
#include "qt/renderers/mac-support.h"
#endif

namespace {

QString Tr(const char* s) {
    return QCoreApplication::translate("vbam", s);
}

}  // namespace

SDLDrawingPanel::SDLDrawingPanel(QWidget* parent, int _width, int _height)
    : NativeDrawingPanel(parent, _width, _height) {
    DrawingPanelInit();
}

SDLDrawingPanel::~SDLDrawingPanel() {
    StopFilterThreads();
    Teardown();
}

void SDLDrawingPanel::Teardown() {
    // Tear down in reverse order of creation: texture and renderer (which own
    // the swapchain / CAMetalLayer attached to the borrowed host view) before
    // the window.
    if (texture_) {
        SDL_DestroyTexture(texture_);
        texture_ = nullptr;
    }
    if (renderer_) {
        SDL_DestroyRenderer(renderer_);
        renderer_ = nullptr;
    }
    if (sdlwindow_) {
        SDL_DestroyWindow(sdlwindow_);
        sdlwindow_ = nullptr;
    }
#if defined(Q_OS_MACOS)
    VbamQtRemoveSdlMetalViews(NativeHandle());
#endif
    if (did_init) {
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
    }
}

QStringList SDLDrawingPanel::AvailableRenderers() {
    QStringList names;
    names << QStringLiteral("default");
    const int n = SDL_GetNumRenderDrivers();
    for (int i = 0; i < n; i++) {
#ifdef ENABLE_SDL3
        const char* name = SDL_GetRenderDriver(i);
        if (name)
            names << QString::fromUtf8(name);
#else
        SDL_RendererInfo info;
        if (SDL_GetRenderDriverInfo(i, &info) == 0 && info.name)
            names << QString::fromUtf8(info.name);
#endif
    }
    return names;
}

bool SDLDrawingPanel::CreateSdlWindow() {
#ifdef ENABLE_SDL3
    SDL_PropertiesID props = SDL_CreateProperties();
    bool ok = true;
#if defined(Q_OS_MACOS)
    ok = SDL_SetPointerProperty(props, SDL_PROP_WINDOW_CREATE_COCOA_VIEW_POINTER, NativeHandle());
#elif defined(Q_OS_WIN)
    ok = SDL_SetPointerProperty(props, SDL_PROP_WINDOW_CREATE_WIN32_HWND_POINTER, NativeHandle());
#else
    const QString platform = QGuiApplication::platformName();
    if (platform == QLatin1String("xcb")) {
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "x11");
        ok = SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_X11_WINDOW_NUMBER,
                                   static_cast<Sint64>(winId()));
    } else {
        // Wayland: Qt exposes no public wl_surface for a widget, so SDL cannot
        // present into this panel. GameArea falls back to the next renderer.
        vbam::LogDebug(QStringLiteral("SDL renderer: no native window on the %1 platform")
                           .arg(platform));
        SDL_DestroyProperties(props);
        return false;
    }
#endif
    if (!ok) {
        vbam::LogError(Tr("Failed to set parent window"));
        SDL_DestroyProperties(props);
        return false;
    }
    SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_OPENGL_BOOLEAN, true);
    SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_HIGH_PIXEL_DENSITY_BOOLEAN, true);
    // No size / position properties: the window wraps an existing native view
    // and takes its geometry; handing SDL a size makes it move and resize the
    // host window (Cocoa puts it at the screen origin).

    // Joysticks keep working while SDL owns a video window.
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");

#if defined(Q_OS_MACOS)
    VbamQtSdlCaptureViewState(NativeHandle(), &saved_window_, &saved_content_view_,
                              &saved_superview_, saved_frame_);
#endif
    sdlwindow_ = SDL_CreateWindowWithProperties(props);
    SDL_DestroyProperties(props);
#else
#if defined(Q_OS_MACOS) || defined(Q_OS_WIN)
    sdlwindow_ = SDL_CreateWindowFrom(NativeHandle());
#else
    if (QGuiApplication::platformName() != QLatin1String("xcb"))
        return false;
    SDL_SetHint(SDL_HINT_VIDEODRIVER, "x11");
    sdlwindow_ = SDL_CreateWindowFrom(reinterpret_cast<void*>(winId()));
#endif
#endif
    if (!sdlwindow_) {
        vbam::LogError(Tr("Failed to create SDL window"));
        vbam::LogDebug(QStringLiteral("SDL Error: %1").arg(QString::fromUtf8(SDL_GetError())));
        return false;
    }
    return true;
}

bool SDLDrawingPanel::CreateRenderer(const QString& requested) {
    QString name = requested;
#if defined(Q_OS_MACOS)
    // macOS has no native OpenGL ES, so SDL's "opengles2" renderer fails to
    // create; desktop "opengl" is the working equivalent.
    if (name == QLatin1String("opengles2"))
        name = QStringLiteral("opengl");
#endif
    const bool use_default = name.isEmpty() || name == QLatin1String("default");

#ifdef ENABLE_SDL3
    // The SDL_GPU-backed "gpu" renderer only acquires its swapchain for a window
    // SDL considers "shown"; a window wrapping an already-visible host view starts
    // hidden in SDL's bookkeeping.
    if (name == QLatin1String("gpu"))
        SDL_ShowWindow(sdlwindow_);

    if (use_default) {
        renderer_ = SDL_CreateRenderer(sdlwindow_, nullptr);
    } else {
        renderer_ = SDL_CreateRenderer(sdlwindow_, name.toUtf8().constData());
        if (!renderer_) {
            vbam::LogWarning(Tr("Renderer creating failed, using default renderer"));
            vbam::LogDebug(QStringLiteral("SDL Error: %1").arg(QString::fromUtf8(SDL_GetError())));
            renderer_ = SDL_CreateRenderer(sdlwindow_, nullptr);
        }
    }
    if (!renderer_)
        return false;

    if (!SDL_SetRenderVSync(renderer_, OPTION(kPrefVsync) ? 1 : 0))
        vbam::LogDebug(QStringLiteral("Failed to set vsync for SDL renderer"));
    renderername_ = QString::fromUtf8(SDL_GetRendererName(renderer_));
#else
    const int vsync_flag = OPTION(kPrefVsync) ? SDL_RENDERER_PRESENTVSYNC : 0;
    if (use_default) {
        renderer_ = SDL_CreateRenderer(sdlwindow_, -1, vsync_flag);
        renderername_ = QStringLiteral("default");
    } else {
        const QByteArray utf8 = name.toUtf8();
        for (int i = 0; i < SDL_GetNumRenderDrivers(); i++) {
            SDL_RendererInfo info;
            SDL_GetRenderDriverInfo(i, &info);
            if (strcmp(utf8.constData(), info.name) != 0)
                continue;
            const Uint32 flags = strcmp(info.name, "software") == 0 ? SDL_RENDERER_SOFTWARE
                                                                      : SDL_RENDERER_ACCELERATED;
            renderer_ = SDL_CreateRenderer(sdlwindow_, i, flags | vsync_flag);
            renderername_ = name;
        }
        if (!renderer_) {
            vbam::LogWarning(Tr("Renderer creating failed, using default renderer"));
            renderer_ = SDL_CreateRenderer(sdlwindow_, -1, vsync_flag);
            renderername_ = QStringLiteral("default");
        }
    }
    if (!renderer_)
        return false;
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, OPTION(kDispBilinear) ? "1" : "0");
#endif
    vbam::LogDebug(QStringLiteral("SDL renderer: %1").arg(renderername_));
    return true;
}

bool SDLDrawingPanel::CreateTexture() {
    if (texture_) {
        SDL_DestroyTexture(texture_);
        texture_ = nullptr;
    }
    const int w = ScaledWidth(), h = ScaledHeight();
    const bool d3d = renderername_ == QLatin1String("direct3d");
    Uint32 fmt;
    switch (panel_color_depth_) {
        case 8:
#ifdef ENABLE_SDL3
            fmt = d3d ? SDL_PIXELFORMAT_RGB332 : SDL_GetPixelFormatForMasks(8, 0xE0, 0x1C, 0x03, 0);
#else
            fmt = d3d ? SDL_PIXELFORMAT_RGB332 : SDL_MasksToPixelFormatEnum(8, 0xE0, 0x1C, 0x03, 0);
#endif
            break;
        case 16:
#ifdef ENABLE_SDL3
            fmt = d3d ? SDL_PIXELFORMAT_RGB565
                      : SDL_GetPixelFormatForMasks(16, 0x7C00, 0x03E0, 0x001F, 0);
#else
            fmt = d3d ? SDL_PIXELFORMAT_RGB565
                      : SDL_MasksToPixelFormatEnum(16, 0x7C00, 0x03E0, 0x001F, 0);
#endif
            break;
        case 24:
#ifdef ENABLE_SDL3
            fmt = d3d ? SDL_PIXELFORMAT_RGB24
                      : SDL_GetPixelFormatForMasks(24, 0x0000FF, 0x00FF00, 0xFF0000, 0);
#else
            fmt = d3d ? SDL_PIXELFORMAT_RGB24
                      : SDL_MasksToPixelFormatEnum(24, 0x0000FF, 0x00FF00, 0xFF0000, 0);
#endif
            break;
        default:
#ifdef ENABLE_SDL3
            fmt = d3d ? SDL_PIXELFORMAT_ARGB8888
                      : SDL_GetPixelFormatForMasks(32, 0x000000FF, 0x0000FF00, 0x00FF0000, 0);
#else
            fmt = d3d ? SDL_PIXELFORMAT_ARGB8888
                      : SDL_MasksToPixelFormatEnum(32, 0x000000FF, 0x0000FF00, 0x00FF0000, 0);
#endif
            break;
    }
#ifdef ENABLE_SDL3
    texture_ = SDL_CreateTexture(renderer_, static_cast<SDL_PixelFormat>(fmt),
                                 SDL_TEXTUREACCESS_STREAMING, w, h);
#else
    texture_ = SDL_CreateTexture(renderer_, fmt, SDL_TEXTUREACCESS_STREAMING, w, h);
#endif
    if (!texture_) {
        vbam::LogError(Tr("Failed to create SDL texture"));
        vbam::LogDebug(QStringLiteral("SDL Error: %1").arg(QString::fromUtf8(SDL_GetError())));
        return false;
    }
#ifdef ENABLE_SDL3
    SDL_ScaleMode mode = OPTION(kDispBilinear) ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST;
#if SDL_VERSION_ATLEAST(3, 4, 0)
    if (OPTION(kDispSDLPixelArt))
        mode = SDL_SCALEMODE_PIXELART;
#endif
    SDL_SetTextureScaleMode(texture_, mode);
#endif
    texture_w_ = w;
    texture_h_ = h;
    return true;
}

void SDLDrawingPanel::DrawingPanelInit() {
    DrawingPanelBase::DrawingPanelInit();

#ifdef ENABLE_SDL3
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
#else
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0) {
#endif
        vbam::LogError(Tr("Failed to initialize SDL video subsystem"));
        vbam::LogDebug(QStringLiteral("SDL Error: %1").arg(QString::fromUtf8(SDL_GetError())));
        init_failed_ = true;
        return;
    }

    if (!CreateSdlWindow()) {
        init_failed_ = true;
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return;
    }

    if (!CreateRenderer(OPTION(kSDLRenderer))) {
        vbam::LogError(Tr("Failed to create SDL renderer"));
        vbam::LogDebug(QStringLiteral("SDL Error: %1").arg(QString::fromUtf8(SDL_GetError())));
        init_failed_ = true;
        Teardown();
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return;
    }

    if (!CreateTexture()) {
        init_failed_ = true;
        Teardown();
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return;
    }

#if defined(Q_OS_MACOS)
    // SDL has finished taking over the window's contentView; undo it so the Qt
    // widget tree (menu bar, status bar, dialogs) stays live.
    VbamQtSdlReattachViewState(NativeHandle(), saved_window_, saved_content_view_,
                               saved_superview_, saved_frame_);
    saved_window_ = saved_content_view_ = saved_superview_ = nullptr;
#endif

    did_init = true;
}

void SDLDrawingPanel::Present() {
    if (!renderer_)
        return;

    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 0xFF);
    SDL_RenderClear(renderer_);

    if (todraw && texture_) {
        const int w = ScaledWidth(), h = ScaledHeight();
        if (w != texture_w_ || h != texture_h_) {
            if (!CreateTexture())
                return;
        }
        const int pitch = SourcePitch();
        const uint8_t* src = SourcePixels();

        if (renderername_ == QLatin1String("direct3d") && panel_color_depth_ == 32) {
            // The core's R,G,B,X byte order swizzled to ARGB8888 for the D3D backend.
            convert_buf_.resize(static_cast<size_t>(pitch) * h);
            const uint32_t* s = reinterpret_cast<const uint32_t*>(src);
            uint32_t* d = reinterpret_cast<uint32_t*>(convert_buf_.data());
            const int stride = pitch / 4;
            for (int y = 0; y < h; y++)
                for (int x = 0; x < w; x++) {
                    const uint32_t p = s[y * stride + x];
                    d[y * stride + x] = 0xFF000000u | ((p & 0xFF) << 16) | (p & 0xFF00) |
                                        ((p & 0xFF0000) >> 16);
                }
            SDL_UpdateTexture(texture_, nullptr, convert_buf_.data(), pitch);
        } else if (renderername_ == QLatin1String("direct3d") && panel_color_depth_ == 16) {
            // RGB555 -> RGB565.
            convert_buf_.resize(static_cast<size_t>(pitch) * h);
            const uint16_t* s = reinterpret_cast<const uint16_t*>(src);
            uint16_t* d = reinterpret_cast<uint16_t*>(convert_buf_.data());
            const int stride = pitch / 2;
            for (int y = 0; y < h; y++)
                for (int x = 0; x < w; x++) {
                    const uint16_t p = s[y * stride + x];
                    d[y * stride + x] = static_cast<uint16_t>(((p & 0x7FE0) << 1) | (p & 0x1F));
                }
            SDL_UpdateTexture(texture_, nullptr, convert_buf_.data(), pitch);
        } else {
            SDL_UpdateTexture(texture_, nullptr, src, pitch);
        }

#ifdef ENABLE_SDL3
        SDL_RenderTexture(renderer_, texture_, nullptr, nullptr);
#else
        SDL_RenderCopy(renderer_, texture_, nullptr, nullptr);
#endif
    }

    SDL_RenderPresent(renderer_);
}
