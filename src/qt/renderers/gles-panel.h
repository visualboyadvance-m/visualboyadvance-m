#ifndef VBAM_QT_RENDERERS_GLES_PANEL_H_
#define VBAM_QT_RENDERERS_GLES_PANEL_H_

#if defined(VBAM_ENABLE_GLES)

#include <cstdint>
#include <vector>

#include <QOpenGLFunctions>
#include <QOpenGLWidget>

#include "qt/drawing-panel.h"

// OpenGL ES 2 renderer, the Android output module (kGLES). Port of the wx
// port's GLESDrawingPanel / widgets/android-gl.cpp.
//
// On Android the desktop OpenGL panel's context requirements are not met and a
// separate SDL SurfaceView is composited behind Qt's own surface, so nothing
// drawn into it is ever visible. Rendering inside Qt's scene graph is the
// reliable path: QOpenGLWidget draws into an FBO that Qt composites with the
// rest of the UI. Every GL call here is in the GLES2 common subset exposed by
// QOpenGLFunctions, so the same code also runs on a desktop GL context, which
// is how it is exercised off Android (-DENABLE_GLES=ON).
//
// The finished frame (filter pipeline output, OSD included) is taken from
// BuildImage() as tightly packed RGBA8888, uploaded to a texture and drawn as
// one aspect-preserving, letterboxed quad.
class GLESDrawingPanel final : public QOpenGLWidget,
                               protected QOpenGLFunctions,
                               public DrawingPanelBase {
    Q_OBJECT

public:
    GLESDrawingPanel(QWidget* parent, int _width, int _height);
    ~GLESDrawingPanel() override;

    QWidget* GetWindow() override { return this; }
    void PresentFrame() override;

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;
    void DrawArea(QPainter& painter) override;
    void DrawingPanelInit() override;

private:
    unsigned CompileShader(unsigned type, const char* src);
    bool BuildProgram();

    std::vector<uint8_t> staging_;  // RGBA8888 copy of the last frame
    int src_w_ = 0, src_h_ = 0;
    int tex_w_ = 0, tex_h_ = 0;
    bool dirty_ = false;

    unsigned prog_ = 0, tex_ = 0, vbo_ = 0;
    int pos_loc_ = -1, uv_loc_ = -1, tex_loc_ = -1;
};

#endif  // VBAM_ENABLE_GLES

#endif  // VBAM_QT_RENDERERS_GLES_PANEL_H_
