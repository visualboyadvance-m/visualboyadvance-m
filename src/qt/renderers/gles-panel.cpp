#include "qt/renderers/gles-panel.h"

#if defined(VBAM_ENABLE_GLES)

#include <algorithm>
#include <cstring>

#include <QImage>
#include <QOpenGLContext>
#include <QSurfaceFormat>

#include "qt/config/option-proxy.h"
#include "qt/log.h"

namespace {

const char kGlesVertexShader[] =
    "attribute vec2 aPos;\n"
    "attribute vec2 aUV;\n"
    "varying vec2 vUV;\n"
    "void main() {\n"
    "    vUV = aUV;\n"
    "    gl_Position = vec4(aPos, 0.0, 1.0);\n"
    "}\n";

const char kGlesFragmentShader[] =
    "#ifdef GL_ES\n"
    "precision mediump float;\n"
    "#endif\n"
    "varying vec2 vUV;\n"
    "uniform sampler2D uTex;\n"
    "void main() {\n"
    "    gl_FragColor = vec4(texture2D(uTex, vUV).rgb, 1.0);\n"
    "}\n";

}  // namespace

GLESDrawingPanel::GLESDrawingPanel(QWidget* parent, int _width, int _height)
    : QOpenGLWidget(parent), DrawingPanelBase(_width, _height) {
    QSurfaceFormat fmt = QSurfaceFormat::defaultFormat();
    fmt.setSwapInterval(OPTION(kPrefVsync) ? 1 : 0);
#if defined(__ANDROID__)
    fmt.setRenderableType(QSurfaceFormat::OpenGLES);
    fmt.setVersion(2, 0);
#endif
    setFormat(fmt);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setUpdateBehavior(QOpenGLWidget::NoPartialUpdate);
}

GLESDrawingPanel::~GLESDrawingPanel() {
    StopFilterThreads();
    // Free the GL objects on the widget's own context.
    if (context()) {
        makeCurrent();
        if (tex_) glDeleteTextures(1, &tex_);
        if (vbo_) glDeleteBuffers(1, &vbo_);
        if (prog_) glDeleteProgram(prog_);
        tex_ = vbo_ = prog_ = 0;
        doneCurrent();
    }
}

void GLESDrawingPanel::PresentFrame() {
    // Stage the frame now (the filter output buffer is reused for the next
    // frame) and repaint on the GL side.
    const QImage im = BuildImage().convertToFormat(QImage::Format_RGBA8888);
    if (!im.isNull()) {
        src_w_ = im.width();
        src_h_ = im.height();
        const size_t n = static_cast<size_t>(src_w_) * src_h_ * 4;
        staging_.resize(n);
        if (im.bytesPerLine() == src_w_ * 4) {
            std::memcpy(staging_.data(), im.constBits(), n);
        } else {
            for (int y = 0; y < src_h_; y++) {
                std::memcpy(staging_.data() + static_cast<size_t>(y) * src_w_ * 4,
                            im.constScanLine(y), static_cast<size_t>(src_w_) * 4);
            }
        }
        dirty_ = true;
    }
    update();
}

void GLESDrawingPanel::initializeGL() {
    if (!QOpenGLContext::currentContext()) {
        init_failed_ = true;
        return;
    }
    initializeOpenGLFunctions();
    DrawingPanelInit();
}

void GLESDrawingPanel::DrawingPanelInit() {
    if (!QOpenGLContext::currentContext()) {
        init_failed_ = true;
        return;
    }

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    if (!BuildProgram()) {
        init_failed_ = true;
        return;
    }
    pos_loc_ = glGetAttribLocation(prog_, "aPos");
    uv_loc_ = glGetAttribLocation(prog_, "aUV");
    tex_loc_ = glGetUniformLocation(prog_, "uTex");

    glGenBuffers(1, &vbo_);

    const bool bilinear = OPTION(kDispBilinear);
    glGenTextures(1, &tex_);
    glBindTexture(GL_TEXTURE_2D, tex_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, bilinear ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, bilinear ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    tex_w_ = tex_h_ = 0;

    DrawingPanelBase::DrawingPanelInit();
}

void GLESDrawingPanel::resizeGL(int, int) {
    // The viewport is recomputed from the device-pixel framebuffer size in
    // paintGL(); the logical size Qt passes here would only cover a corner of
    // the (larger) framebuffer on a hi-DPI display.
}

void GLESDrawingPanel::paintGL() {
    // QOpenGLWidget's framebuffer is sized in device pixels.
    const qreal dpr = devicePixelRatioF();
    const int view_w = std::max(1, static_cast<int>(QWidget::width() * dpr));
    const int view_h = std::max(1, static_cast<int>(QWidget::height() * dpr));
    glViewport(0, 0, view_w, view_h);
    glClear(GL_COLOR_BUFFER_BIT);

    if (init_failed_ || !did_init || !prog_ || !tex_ || staging_.empty())
        return;

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex_);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    if (dirty_) {
        if (src_w_ != tex_w_ || src_h_ != tex_h_) {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, src_w_, src_h_, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                         staging_.data());
            tex_w_ = src_w_;
            tex_h_ = src_h_;
        } else {
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, src_w_, src_h_, GL_RGBA, GL_UNSIGNED_BYTE,
                            staging_.data());
        }
        dirty_ = false;
    }
    if (tex_w_ <= 0 || tex_h_ <= 0)
        return;

    // Aspect-preserving letterbox unless the user asked for a stretched panel:
    // scale the quad in NDC so the source aspect is kept, centred, with black
    // bars on the long axis.
    float sx = 1.0f, sy = 1.0f;
    if (OPTION(kDispStretch)) {
        const float src_aspect = static_cast<float>(tex_w_) / static_cast<float>(tex_h_);
        const float dst_aspect = static_cast<float>(view_w) / static_cast<float>(view_h);
        if (dst_aspect > src_aspect) {
            sx = src_aspect / dst_aspect;
        } else {
            sy = dst_aspect / src_aspect;
        }
    }

    // Interleaved x, y, u, v. V is flipped: texture row 0 is the top of the
    // frame while the GL texture origin is bottom-left.
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

    glUniform1i(tex_loc_, 0);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glDisableVertexAttribArray(pos_loc_);
    glDisableVertexAttribArray(uv_loc_);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glUseProgram(0);
}

void GLESDrawingPanel::DrawArea(QPainter& painter) {
    // Presentation happens in paintGL().
    (void)painter;
}

unsigned GLESDrawingPanel::CompileShader(unsigned type, const char* src) {
    GLuint sh = glCreateShader(type);
    glShaderSource(sh, 1, &src, nullptr);
    glCompileShader(sh);
    GLint ok = 0;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512] = {0};
        glGetShaderInfoLog(sh, sizeof(log), nullptr, log);
        vbam::LogDebug(QStringLiteral("GLES shader compile failed: %1").arg(QString::fromUtf8(log)));
        glDeleteShader(sh);
        return 0;
    }
    return sh;
}

bool GLESDrawingPanel::BuildProgram() {
    const GLuint vs = CompileShader(GL_VERTEX_SHADER, kGlesVertexShader);
    const GLuint fs = CompileShader(GL_FRAGMENT_SHADER, kGlesFragmentShader);
    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return false;
    }
    const GLuint prog = glCreateProgram();
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
        vbam::LogDebug(QStringLiteral("GLES program link failed: %1").arg(QString::fromUtf8(log)));
        glDeleteProgram(prog);
        return false;
    }
    prog_ = prog;
    return true;
}

#endif  // VBAM_ENABLE_GLES
