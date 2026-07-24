// Android/wxQt in-tree GLES2 video renderer.
//
// On Android the wx build has no wxGLCanvas (wxUSE_GLCANVAS=0) and the legacy
// GLDrawingPanel uses fixed-function desktop GL that GLES2 cannot run. A
// separate SDL SurfaceView is also unusable: SurfaceFlinger composites it
// behind Qt's own rendering surface, so nothing drawn into it is ever visible.
//
// The reliable path is to render *inside* Qt's scene graph. QOpenGLWidget draws
// into an FBO that the Qt compositor blits into the widget's backing store, so
// it composites correctly with the rest of the wx/Qt UI (toolbar, status bar,
// the on-screen controller) with no separate surface to be occluded. It uses a
// GLES2 context on Android, and QOpenGLFunctions exposes exactly the GLES2
// common subset, so every GL call here is GLES2-clean.
//
// The emulator hands us a finished 24-bit RGB frame (from BasicDrawingPanel's
// filter/scale pipeline) via VbamAndroidGLPresent(); we upload it to a texture
// and draw a single aspect-preserving (letterboxed) quad.

#if defined(__WXQT__) && defined(__ANDROID__)

#include <cstdint>
#include <cstring>
#include <vector>

#include <android/log.h>

#include <QtCore/QMutex>
#include <QtCore/QMutexLocker>
#include <QtGui/QOpenGLFunctions>
#include <QtOpenGLWidgets/QOpenGLWidget>
#include <QtWidgets/QWidget>

namespace {

const char kVertexShader[] =
    "attribute vec2 aPos;\n"
    "attribute vec2 aUV;\n"
    "varying vec2 vUV;\n"
    "void main() {\n"
    "    vUV = aUV;\n"
    "    gl_Position = vec4(aPos, 0.0, 1.0);\n"
    "}\n";

const char kFragmentShader[] =
    "precision mediump float;\n"
    "varying vec2 vUV;\n"
    "uniform sampler2D uTex;\n"
    "void main() {\n"
    "    gl_FragColor = texture2D(uTex, vUV);\n"
    "}\n";

}  // namespace

// A QOpenGLWidget that displays the latest emulator frame as a textured quad.
// No Q_OBJECT: it declares no new signals/slots, only overrides QOpenGLWidget's
// GL virtuals, so it needs no moc pass.
class VbamGLWidget : public QOpenGLWidget, protected QOpenGLFunctions {
public:
    // `parent` is the wx render panel's QWidget. We are a child of it and fill
    // it; the on-screen controller is a sibling stacked above us and, since we
    // are all children of the same panel backing store, its translucent regions
    // composite over our GL content. The viewport is derived from the
    // device-pixel framebuffer size in paintGL(), so a hi-DPI resize is handled
    // even if Qt's logical resizeGL() size lags.
    explicit VbamGLWidget(QWidget* parent)
        : QOpenGLWidget(parent), parent_(parent) {
        SyncGeometry();
    }

    // Fill the parent panel.
    void SyncGeometry() {
        if (!parent_) {
            return;
        }
        const QSize sz = parent_->size();
        const QRect r(0, 0, sz.width(), sz.height());
        if (r != geometry() && sz.width() > 0 && sz.height() > 0) {
            setGeometry(r);
        }
    }

    ~VbamGLWidget() override {
        // Free GL objects on the widget's own context.
        if (context()) {
            makeCurrent();
            if (tex_) glDeleteTextures(1, &tex_);
            if (vbo_) glDeleteBuffers(1, &vbo_);
            if (prog_) glDeleteProgram(prog_);
            doneCurrent();
        }
    }

    // Called on the GUI thread with a tightly packed 24-bit RGB frame. Copies it
    // into a staging buffer and schedules a repaint.
    void Present(const uint8_t* rgb, int w, int h) {
        if (!rgb || w <= 0 || h <= 0) {
            return;
        }
        SyncGeometry();
        {
            QMutexLocker lock(&mutex_);
            src_w_ = w;
            src_h_ = h;
            const size_t n = static_cast<size_t>(w) * h * 3;
            buf_.resize(n);
            std::memcpy(buf_.data(), rgb, n);
            dirty_ = true;
        }
        update();
    }

protected:
    void initializeGL() override {
        initializeOpenGLFunctions();
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

        prog_ = BuildProgram();
        if (prog_) {
            pos_loc_ = glGetAttribLocation(prog_, "aPos");
            uv_loc_ = glGetAttribLocation(prog_, "aUV");
            tex_loc_ = glGetUniformLocation(prog_, "uTex");
        }

        glGenBuffers(1, &vbo_);

        glGenTextures(1, &tex_);
        glBindTexture(GL_TEXTURE_2D, tex_);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    void resizeGL(int, int) override {
        // Viewport is recomputed from the device-pixel framebuffer size in
        // paintGL(); the logical w/h Qt passes here would only cover a corner of
        // the (larger) framebuffer on a hi-DPI display.
    }

    void paintGL() override {
        // QOpenGLWidget's default framebuffer is sized in DEVICE pixels
        // (logical size x devicePixelRatio). glViewport must use device pixels or
        // it fills only the lower-left corner.
        const qreal dpr = devicePixelRatioF();
        view_w_ = static_cast<int>(width() * dpr);
        view_h_ = static_cast<int>(height() * dpr);
        if (view_w_ < 1) view_w_ = 1;
        if (view_h_ < 1) view_h_ = 1;
        glViewport(0, 0, view_w_, view_h_);
        glClear(GL_COLOR_BUFFER_BIT);

        if (!prog_ || !tex_) {
            return;
        }

        {
            QMutexLocker lock(&mutex_);
            if (buf_.empty()) {
                return;
            }
            glBindTexture(GL_TEXTURE_2D, tex_);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            if (dirty_) {
                if (src_w_ != tex_w_ || src_h_ != tex_h_) {
                    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, src_w_, src_h_, 0,
                                 GL_RGB, GL_UNSIGNED_BYTE, buf_.data());
                    tex_w_ = src_w_;
                    tex_h_ = src_h_;
                } else {
                    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, src_w_, src_h_,
                                    GL_RGB, GL_UNSIGNED_BYTE, buf_.data());
                }
                dirty_ = false;
            }
        }

        if (tex_w_ <= 0 || tex_h_ <= 0) {
            return;
        }

        // Aspect-preserving letterbox: scale the quad in NDC so the source
        // aspect ratio is kept, centered, with black bars on the long axis.
        const float src_aspect = static_cast<float>(tex_w_) / static_cast<float>(tex_h_);
        const float dst_aspect = static_cast<float>(view_w_) / static_cast<float>(view_h_);
        float sx = 1.0f;
        float sy = 1.0f;
        if (dst_aspect > src_aspect) {
            sx = src_aspect / dst_aspect;
        } else {
            sy = dst_aspect / src_aspect;
        }

        // Interleaved x,y,u,v. V is flipped (texture row 0 is the top of the
        // frame, but GL texture origin is bottom-left).
        const float verts[] = {
            -sx, -sy, 0.0f, 1.0f,
             sx, -sy, 1.0f, 1.0f,
            -sx,  sy, 0.0f, 0.0f,
             sx,  sy, 1.0f, 0.0f,
        };

        glUseProgram(prog_);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STREAM_DRAW);

        glEnableVertexAttribArray(pos_loc_);
        glVertexAttribPointer(pos_loc_, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                              reinterpret_cast<const void*>(0));
        glEnableVertexAttribArray(uv_loc_);
        glVertexAttribPointer(uv_loc_, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                              reinterpret_cast<const void*>(2 * sizeof(float)));

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tex_);
        glUniform1i(tex_loc_, 0);

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        glDisableVertexAttribArray(pos_loc_);
        glDisableVertexAttribArray(uv_loc_);
    }

private:
    GLuint CompileShader(GLenum type, const char* src) {
        GLuint sh = glCreateShader(type);
        glShaderSource(sh, 1, &src, nullptr);
        glCompileShader(sh);
        GLint ok = 0;
        glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char log[512] = {0};
            glGetShaderInfoLog(sh, sizeof(log), nullptr, log);
            __android_log_print(ANDROID_LOG_ERROR, "VBAM", "shader compile failed: %s", log);
            glDeleteShader(sh);
            return 0;
        }
        return sh;
    }

    GLuint BuildProgram() {
        GLuint vs = CompileShader(GL_VERTEX_SHADER, kVertexShader);
        GLuint fs = CompileShader(GL_FRAGMENT_SHADER, kFragmentShader);
        if (!vs || !fs) {
            if (vs) glDeleteShader(vs);
            if (fs) glDeleteShader(fs);
            return 0;
        }
        GLuint prog = glCreateProgram();
        glAttachShader(prog, vs);
        glAttachShader(prog, fs);
        glLinkProgram(prog);
        glDeleteShader(vs);
        glDeleteShader(fs);
        GLint ok = 0;
        glGetProgramiv(prog, GL_LINK_STATUS, &ok);
        if (!ok) {
            char log[512] = {0};
            glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
            __android_log_print(ANDROID_LOG_ERROR, "VBAM", "program link failed: %s", log);
            glDeleteProgram(prog);
            return 0;
        }
        return prog;
    }

    QWidget* parent_ = nullptr;  // the wx render panel QWidget (our parent)
    QMutex mutex_;
    std::vector<uint8_t> buf_;  // staging RGB frame
    bool dirty_ = false;
    int src_w_ = 0, src_h_ = 0;
    int tex_w_ = 0, tex_h_ = 0;
    int view_w_ = 1, view_h_ = 1;

    GLuint prog_ = 0, tex_ = 0, vbo_ = 0;
    GLint pos_loc_ = -1, uv_loc_ = -1, tex_loc_ = -1;
};

// ── C ABI used by GLESDrawingPanel in panel.cpp ─────────────────────────────

// Create the GL widget as a child of `parent_qwidget` (the render panel's
// QWidget, from wxWindow::GetHandle()), filling it. Returns the widget pointer.
extern "C" void* VbamAndroidGLCreate(void* parent_qwidget) {
    QWidget* parent = reinterpret_cast<QWidget*>(parent_qwidget);
    VbamGLWidget* w = new VbamGLWidget(parent);
    // Keep it at the bottom so the translucent on-screen controller (a sibling
    // stacked above) shows through onto the GL content.
    w->lower();
    w->show();
    return w;
}

extern "C" void VbamAndroidGLResize(void* widget, int, int) {
    // Geometry is derived from the source panel's rect (see SyncGeometry); the
    // explicit w/h are ignored. Kept for the stable C ABI / call sites.
    if (!widget) return;
    reinterpret_cast<VbamGLWidget*>(widget)->SyncGeometry();
}

extern "C" void VbamAndroidGLPresent(void* widget, const void* rgb, int w, int h) {
    if (!widget) return;
    reinterpret_cast<VbamGLWidget*>(widget)->Present(
        reinterpret_cast<const uint8_t*>(rgb), w, h);
}

extern "C" void VbamAndroidGLDestroy(void* widget) {
    if (!widget) return;
    delete reinterpret_cast<VbamGLWidget*>(widget);
}

#endif  // defined(__WXQT__) && defined(__ANDROID__)
