// Entry point of the Qt frontend.

#include <cstdio>

#include <QCoreApplication>
#include <QGuiApplication>
#include <QSurfaceFormat>

#ifndef ENABLE_SDL3
#include <SDL.h>
#else
#include <SDL3/SDL.h>
#endif

#include "qt/app.h"

int main(int argc, char** argv) {
#if defined(__APPLE__)
    // Developer aid: with VBAM_QT_NO_ACTIVATE set, a test instance launched from
    // a terminal must not take the keyboard focus. Qt turns the process into a
    // foreground app (and activates it) in the Cocoa platform plugin unless
    // told otherwise, so this has to be decided before QApplication exists.
    if (qEnvironmentVariableIsSet("VBAM_QT_NO_ACTIVATE")) {
        qputenv("QT_MAC_DISABLE_FOREGROUND_APPLICATION_TRANSFORM", "1");
    }
#endif
    // Consistent names for the configuration paths. DO NOT TRANSLATE.
    QCoreApplication::setOrganizationName("visualboyadvance-m");
    QCoreApplication::setOrganizationDomain("visualboyadvance-m.org");
    QCoreApplication::setApplicationName("vbam-qt");
    QGuiApplication::setApplicationDisplayName("VisualBoyAdvance-M");

    // Default surface format for the OpenGL renderer (QOpenGLWidget). No depth
    // or stencil buffer is needed for a textured quad. The swap interval is
    // re-applied per panel from the VSync option (see drawing-panel.cpp).
    QSurfaceFormat fmt = QSurfaceFormat::defaultFormat();
    fmt.setDepthBufferSize(0);
    fmt.setStencilBufferSize(0);
    fmt.setSwapInterval(0);
    fmt.setRenderableType(QSurfaceFormat::OpenGL);
    QSurfaceFormat::setDefaultFormat(fmt);

#if defined(__APPLE__) || defined(_WIN32)
    // Share GL contexts between QOpenGLWidgets so a renderer rebuild does not
    // lose the driver's compiled state.
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
#endif

    VbamApp app(argc, argv);

    if (!app.Init()) {
        // --help, --version, an option listing, or an error already reported.
        return app.console_status();
    }

    return app.exec();
}
