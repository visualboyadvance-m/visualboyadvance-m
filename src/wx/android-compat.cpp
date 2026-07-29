// Android/wxQt link compatibility shims.
//
// The wxQt build for Android is configured with wxUSE_CONSOLE_EVENTLOOP=0, which
// compiles out the definition of wxGUIAppTraits::GetEventLoopSourcesManager()
// while the Qt apptraits object still references it. Provide the missing
// definition here. Qt drives the event loop on Android and VBA-M registers no
// wxEventLoopSource file descriptors, so returning nullptr is safe.

#if defined(__WXQT__) && defined(__ANDROID__)

#include <algorithm>
#include <cstring>
#include <memory>
#include <vector>

#include <android/log.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>

#include <QtCore/QByteArray>
#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QJniEnvironment>
#include <QtCore/QJniObject>
#include <QtCore/QStandardPaths>
#include <QtCore/QString>
#include <QtGui/QGuiApplication>
#include <QtGui/QScreen>
#include <QtWidgets/QAbstractScrollArea>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QScroller>
#include <QtWidgets/QWidget>

#include <wx/apptrait.h>
#include <wx/artprov.h>
#include <wx/bitmap.h>
#include <wx/image.h>
#include <wx/log.h>
#include <wx/string.h>

#include "wx/android-compat.h"

wxEventLoopSourcesManagerBase* wxGUIAppTraits::GetEventLoopSourcesManager() {
    return nullptr;
}

namespace {

// On wxQt/Android the built-in art providers have no icon theme and return a
// null bitmap for stock ids (e.g. wxART_FILE_OPEN). wxQt's wxToolBarTool::SetIcon
// then calls QIcon::addPixmap() on a null QPixmap and crashes. This last-resort
// provider guarantees a valid (blank, transparent) bitmap so no stock-art lookup
// is ever null.
class FallbackArtProvider : public wxArtProvider {
protected:
    wxBitmap CreateBitmap(const wxArtID&, const wxArtClient&, const wxSize& size) override {
        wxSize sz = size;
        if (sz.x <= 0 || sz.y <= 0) {
            sz = wxSize(24, 24);
        }
        // Build via wxImage so the result is unconditionally a valid bitmap on
        // wxQt (a fully transparent placeholder).
        wxImage img(sz.x, sz.y);
        img.InitAlpha();
        if (unsigned char* alpha = img.GetAlpha()) {
            std::memset(alpha, 0, static_cast<size_t>(sz.x) * sz.y);
        }
        return wxBitmap(img);
    }
};

// wxQt on Android trips benign debug asserts (e.g. GetWidth() on bitmaps that
// some widgets leave empty). In a debug build each would pop a modal dialog and
// make the app unusable, so log and continue instead of interrupting.
void VbamAndroidAssertHandler(const wxString& file, int line, const wxString& func,
                              const wxString& cond, const wxString& msg) {
    wxLogDebug("Suppressed assert %s(%d) in %s: %s %s", file, line, func, cond, msg);
}

}  // namespace

// Registered once from wxvbamApp::OnInit() (Android only). PushBack makes it the
// lowest-priority provider, so real icons still win when available.
void VbamInstallAndroidArtFallback() {
    wxArtProvider::PushBack(new FallbackArtProvider());
    wxSetAssertHandler(VbamAndroidAssertHandler);
}

// --- Storage Access Framework file staging -----------------------------------
//
// The Android file picker returns Storage-Access-Framework "content://" URIs,
// which none of VBA-M's stdio / ffmpeg / wxFFile based readers and writers can
// open. Everything below translates between such a URI and a real local file
// that those can use, going through Qt (whose Android file engine understands
// content://) and falling back to the ContentResolver over JNI.

namespace {

const char kContentScheme[] = "content://";

wxString FromQString(const QString& s) {
    return wxString::FromUTF8(s.toUtf8().constData());
}

QString ToQString(const wxString& s) {
    return QString::fromUtf8(s.utf8_str().data());
}

// Wraps `uri` as an android.net.Uri, or an invalid object on failure.
QJniObject ParseUri(const QString& uri) {
    QJniObject j_uri_str = QJniObject::fromString(uri);
    QJniObject j_uri = QJniObject::callStaticObjectMethod(
        "android/net/Uri", "parse", "(Ljava/lang/String;)Landroid/net/Uri;", j_uri_str.object());
    QJniEnvironment env;
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        return QJniObject();
    }
    return j_uri;
}

QJniObject ContentResolver() {
    QJniObject context = QNativeInterface::QAndroidApplication::context();
    if (!context.isValid()) {
        return QJniObject();
    }
    return context.callObjectMethod("getContentResolver", "()Landroid/content/ContentResolver;");
}

// The provider's human-readable file name for `uri`, empty if unavailable. This
// is the only place an extension can be recovered from a content:// URI, and
// both ROM type detection and ffmpeg's output format guessing need one.
QString ContentUriDisplayName(const QString& uri) {
    QJniObject j_uri = ParseUri(uri);
    QJniObject resolver = ContentResolver();
    if (!j_uri.isValid() || !resolver.isValid()) {
        return QString();
    }

    QString display_name;
    QJniObject cursor = resolver.callObjectMethod(
        "query",
        "(Landroid/net/Uri;[Ljava/lang/String;Ljava/lang/String;[Ljava/lang/String;"
        "Ljava/lang/String;)Landroid/database/Cursor;",
        j_uri.object(), nullptr, nullptr, nullptr, nullptr);
    if (cursor.isValid() && cursor.callMethod<jboolean>("moveToFirst")) {
        QJniObject col = QJniObject::fromString(QStringLiteral("_display_name"));
        const jint idx =
            cursor.callMethod<jint>("getColumnIndex", "(Ljava/lang/String;)I", col.object());
        if (idx >= 0) {
            QJniObject name_obj = cursor.callObjectMethod("getString", "(I)Ljava/lang/String;", idx);
            if (name_obj.isValid()) {
                display_name = name_obj.toString();
            }
        }
        cursor.callMethod<void>("close");
    }
    // A provider may report a name with directory components; keep the leaf so
    // it can be appended to a staging directory.
    return QFileInfo(display_name).fileName();
}

// Shared staging directory for SAF transfers. This is app data rather than the
// cache dir because formats that write a companion file -- the movie recorder
// emits a .vm0 save state beside its .vmv -- must find that companion again
// when the recording is played back in a later session.
QString SafStagingDir() {
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (dir.isEmpty()) {
        dir = QDir::tempPath();
    }
    dir += QStringLiteral("/saf-staging");
    QDir().mkpath(dir);
    return dir;
}

// Appends `.<required_ext>` to `name` unless it already ends in it. Callers
// that derive companion file names from the extension, or that need a
// particular container, cannot trust a provider to report a name with one.
QString EnsureExtension(const QString& name, const wxString& required_ext) {
    if (required_ext.empty()) {
        return name;
    }
    const QString ext = QLatin1Char('.') + QString::fromUtf8(required_ext.utf8_str().data());
    if (name.endsWith(ext, Qt::CaseInsensitive)) {
        return name;
    }
    return name + ext;
}

// Copies the content of `uri` into `dir` under its display name (falling back
// to `fallback_name`) and returns the local path, or an empty string on error.
QString CopyContentUriToDir(const QString& uri, const QString& dir, const QString& fallback_name,
                            const wxString& required_ext) {
    QString name = ContentUriDisplayName(uri);
    if (name.isEmpty()) {
        name = fallback_name;
    }
    name = EnsureExtension(name, required_ext);

    QFile in(uri);
    if (!in.open(QIODevice::ReadOnly)) {
        return QString();
    }
    const QByteArray bytes = in.readAll();
    in.close();

    QDir().mkpath(dir);
    const QString out_path = dir + QLatin1Char('/') + name;

    QFile out(out_path);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return QString();
    }
    const bool ok = out.write(bytes) == bytes.size();
    out.close();
    return ok ? out_path : QString();
}

// Streams `staged` into an already-opened java.io.OutputStream.
bool WriteFileToStream(const QString& staged, QJniObject& stream) {
    QFile in(staged);
    if (!in.open(QIODevice::ReadOnly)) {
        return false;
    }

    static const qint64 kChunk = 1 << 20;
    QByteArray buf(kChunk, '\0');
    QJniEnvironment env;
    bool ok = true;
    while (ok) {
        const qint64 n = in.read(buf.data(), kChunk);
        if (n < 0) {
            ok = false;
            break;
        }
        if (n == 0) {
            break;
        }
        jbyteArray arr = env->NewByteArray(static_cast<jsize>(n));
        if (!arr) {
            ok = false;
            break;
        }
        env->SetByteArrayRegion(arr, 0, static_cast<jsize>(n),
                                reinterpret_cast<const jbyte*>(buf.constData()));
        stream.callMethod<void>("write", "([B)V", arr);
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            ok = false;
        }
        env->DeleteLocalRef(arr);
    }
    in.close();
    return ok;
}

// Copies `staged` back to `uri` through the ContentResolver. Used when Qt's own
// content:// write path is unavailable, which is the case for providers that
// reject the truncating open mode Qt asks for.
bool WriteFileToContentUriViaJni(const QString& staged, const QString& uri) {
    QJniObject j_uri = ParseUri(uri);
    QJniObject resolver = ContentResolver();
    if (!j_uri.isValid() || !resolver.isValid()) {
        return false;
    }

    QJniEnvironment env;
    // "wt" truncates first; a provider is only required to support "w", which
    // can leave a tail of the previous contents behind, so try "wt" first.
    QJniObject mode = QJniObject::fromString(QStringLiteral("wt"));
    QJniObject stream = resolver.callObjectMethod(
        "openOutputStream", "(Landroid/net/Uri;Ljava/lang/String;)Ljava/io/OutputStream;",
        j_uri.object(), mode.object());
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        stream = QJniObject();
    }
    if (!stream.isValid()) {
        stream = resolver.callObjectMethod(
            "openOutputStream", "(Landroid/net/Uri;)Ljava/io/OutputStream;", j_uri.object());
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            return false;
        }
    }
    if (!stream.isValid()) {
        return false;
    }

    const bool ok = WriteFileToStream(staged, stream);
    stream.callMethod<void>("flush");
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
    }
    stream.callMethod<void>("close");
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
    }
    return ok;
}

bool WriteFileToContentUri(const QString& staged, const QString& uri) {
    QFile out(uri);
    if (out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QFile in(staged);
        if (!in.open(QIODevice::ReadOnly)) {
            out.close();
            return false;
        }
        static const qint64 kChunk = 1 << 20;
        QByteArray buf(kChunk, '\0');
        bool ok = true;
        while (ok) {
            const qint64 n = in.read(buf.data(), kChunk);
            if (n < 0) {
                ok = false;
                break;
            }
            if (n == 0) {
                break;
            }
            ok = out.write(buf.constData(), n) == n;
        }
        in.close();
        ok = out.flush() && ok;
        out.close();
        if (ok) {
            return true;
        }
    }
    return WriteFileToContentUriViaJni(staged, uri);
}

// Staging files handed out by VbamStageAndroidOutputFile() that have not been
// committed or discarded yet, keyed by the local path the writer was given.
struct SafOutputTarget {
    QString staged;
    QString uri;
};

std::vector<SafOutputTarget>& SafOutputTargets() {
    static std::vector<SafOutputTarget> targets;
    return targets;
}

}  // namespace

wxString VbamResolveAndroidContentUri(const wxString& path) {
    if (!path.StartsWith(kContentScheme)) {
        return path;
    }

    QString cache_dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (cache_dir.isEmpty()) {
        cache_dir = QDir::tempPath();
    }

    const QString out_path =
        CopyContentUriToDir(ToQString(path), cache_dir, QStringLiteral("rom.gba"), wxEmptyString);
    if (out_path.isEmpty()) {
        return path;  // let the caller report the load failure
    }
    return FromQString(out_path);
}

wxString VbamStageAndroidInputFile(const wxString& path, const wxString& required_ext) {
    if (!path.StartsWith(kContentScheme)) {
        return path;
    }

    const QString out_path = CopyContentUriToDir(ToQString(path), SafStagingDir(),
                                                 QStringLiteral("movie"), required_ext);
    if (out_path.isEmpty()) {
        return path;  // let the caller report the load failure
    }
    return FromQString(out_path);
}

wxString VbamStageAndroidOutputFile(const wxString& path, const wxString& required_ext) {
    if (!path.StartsWith(kContentScheme)) {
        return path;
    }

    const QString uri = ToQString(path);
    QString name = ContentUriDisplayName(uri);
    if (name.isEmpty()) {
        name = QStringLiteral("recording");
    }
    name = EnsureExtension(name, required_ext);

    // The staging name is derived from the display name rather than made unique
    // so that a companion file written beside a previous staging of the same
    // document is still found; drop any leftover content so a failed transfer
    // can never masquerade as this recording.
    const QString staged = SafStagingDir() + QLatin1Char('/') + name;
    QFile::remove(staged);

    for (SafOutputTarget& target : SafOutputTargets()) {
        if (target.staged == staged) {
            target.uri = uri;
            return FromQString(staged);
        }
    }
    SafOutputTargets().push_back({staged, uri});
    return FromQString(staged);
}

bool VbamCommitAndroidOutputFile(const wxString& staged_path) {
    const QString staged = ToQString(staged_path);
    std::vector<SafOutputTarget>& targets = SafOutputTargets();
    for (size_t i = 0; i < targets.size(); i++) {
        if (targets[i].staged != staged) {
            continue;
        }
        const QString uri = targets[i].uri;
        targets.erase(targets.begin() + i);
        const bool ok = WriteFileToContentUri(staged, uri);
        if (!ok) {
            __android_log_print(ANDROID_LOG_ERROR, "VBAM", "failed to write %s back to %s",
                                staged.toUtf8().constData(), uri.toUtf8().constData());
        }
        // The staging copy is disposable either way: recordings can be large,
        // and playback re-stages from the URI.
        QFile::remove(staged);
        return ok;
    }
    return true;  // not a staged path; the writer already wrote where it should
}

void VbamDiscardAndroidOutputFile(const wxString& staged_path) {
    const QString staged = ToQString(staged_path);
    std::vector<SafOutputTarget>& targets = SafOutputTargets();
    for (size_t i = 0; i < targets.size(); i++) {
        if (targets[i].staged == staged) {
            targets.erase(targets.begin() + i);
            QFile::remove(staged);
            return;
        }
    }
}

// --- Wake lock ---------------------------------------------------------------

void VbamSetAndroidWakeLock(bool enable) {
    // Remember the last state we asked for: this is called from the emulator
    // thread on every pause/resume and each JNI round trip posts to the UI
    // thread, so skip the no-op transitions here as well as in Java.
    static int last = -1;
    if (last == static_cast<int>(enable)) {
        return;
    }
    QJniObject activity = QNativeInterface::QAndroidApplication::context();
    if (!activity.isValid()) {
        return;
    }
    QJniObject::callStaticMethod<void>(
        "org/visualboyadvance_m/VbamWakeLock", "setEnabled",
        "(Landroid/app/Activity;Z)V", activity.object(), static_cast<jboolean>(enable));
    QJniEnvironment env;
    if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
        __android_log_print(ANDROID_LOG_ERROR, "VBAM", "VbamWakeLock.setEnabled threw");
        return;
    }
    last = static_cast<int>(enable);
}

// --- Android video SurfaceView glue (for SDL video into the Qt activity) -----

// SDL's Android backend caches the SDLActivity jclass + method IDs and creates
// its activity mutexes in SDLActivity.nativeSetupJNI(), normally invoked by the
// SDLActivity Java lifecycle. Under a QtActivity that never happens, so many
// SDL JNI calls hit a NULL jclass and abort. The SDLActivity class IS bundled
// (org.libsdl.app), and JNI_OnLoad registered its native methods, so we can
// invoke nativeSetupJNI() ourselves once before SDL_Init to populate all of it.
void VbamSetupSdlActivityJni() {
    static bool done = false;
    if (done) {
        return;
    }
    done = true;

    // SDL caches per-manager jclasses + method IDs in each class's
    // nativeSetupJNI(); the SDLActivity lifecycle normally calls all three.
    // Under QtActivity we invoke them ourselves so gamepad/sensor/audio init
    // (SdlPoller, SdlMotion) don't hit a NULL jclass.
    static const char* const kClasses[] = {
        "org/libsdl/app/SDLActivity",
        "org/libsdl/app/SDLControllerManager",
        "org/libsdl/app/SDLAudioManager",
    };
    QJniEnvironment env;
    for (const char* name : kClasses) {
        jclass cls = env->FindClass(name);
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            cls = nullptr;
        }
        if (!cls) {
            __android_log_print(ANDROID_LOG_ERROR, "VBAM", "class not found: %s", name);
            continue;
        }
        jmethodID mid = env->GetStaticMethodID(cls, "nativeSetupJNI", "()V");
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            mid = nullptr;
        }
        if (mid) {
            env.jniEnv()->CallStaticVoidMethod(cls, mid);
            if (env->ExceptionCheck()) {
                env->ExceptionClear();
            }
            __android_log_print(ANDROID_LOG_INFO, "VBAM", "%s.nativeSetupJNI() done", name);
        }
    }
}


// Computes a wx/Qt widget's on-screen rect in physical pixels (Qt logical
// coords x devicePixelRatio), which is what Android view LayoutParams use.
static void VbamWidgetScreenRectPx(void* qwidget, int* x, int* y, int* w, int* h) {
    *x = *y = 0;
    *w = *h = 0;
    QWidget* widget = reinterpret_cast<QWidget*>(qwidget);
    if (!widget) {
        return;
    }
    const qreal dpr = widget->devicePixelRatio();
    const QPoint g = widget->mapToGlobal(QPoint(0, 0));
    const QSize sz = widget->size();
    *x = static_cast<int>(g.x() * dpr);
    *y = static_cast<int>(g.y() * dpr);
    *w = static_cast<int>(sz.width() * dpr);
    *h = static_cast<int>(sz.height() * dpr);
}

// Size of the activity's content view in physical pixels, or false if no
// layout has been measured yet.
//
// Make sure something is measuring the content view for us first. Laying out
// the overlay SurfaceView records its size, but the in-tree GLES2 renderer never
// creates that overlay, which used to leave the size unknown (and every caller
// unclamped) for the whole session. One call installs a layout listener on the
// Java side that keeps the size current across rotations.
static bool AndroidContentSizePx(int* w, int* h) {
    static bool watching = false;
    if (!watching) {
        QJniObject activity = QNativeInterface::QAndroidApplication::context();
        if (activity.isValid()) {
            QJniObject::callStaticMethod<void>(
                "org/visualboyadvance_m/VbamVideoSurface", "watchContentSize",
                "(Landroid/app/Activity;)V", activity.object());
            QJniEnvironment env;
            if (env->ExceptionCheck()) {
                env->ExceptionClear();
            } else {
                watching = true;
            }
        }
    }
    const int content_w_px = QJniObject::callStaticMethod<jint>(
        "org/visualboyadvance_m/VbamVideoSurface", "contentWidthPx", "()I");
    const int content_h_px = QJniObject::callStaticMethod<jint>(
        "org/visualboyadvance_m/VbamVideoSurface", "contentHeightPx", "()I");
    if (content_w_px <= 0 || content_h_px <= 0) {
        return false;
    }
    *w = content_w_px;
    *h = content_h_px;
    return true;
}

// The part of `qwidget` that is actually on screen, in Qt logical pixels.
//
// Qt's Android window can be taller than the activity's content view -- the
// action bar above it is not subtracted -- so a widget laid out to fill that
// window overhangs the bottom of the screen. Callers that must stay visible
// (the video overlay, the on-screen controller) clamp themselves to this.
// Returns false when the content size is not known yet, leaving *w and *h alone.
bool VbamAndroidVisibleClientSize(void* qwidget, int* w, int* h) {
    QWidget* widget = reinterpret_cast<QWidget*>(qwidget);
    if (!widget) {
        return false;
    }
    int content_w_px = 0, content_h_px = 0;
    if (!AndroidContentSizePx(&content_w_px, &content_h_px)) {
        return false;
    }
    const qreal dpr = widget->devicePixelRatio() > 0 ? widget->devicePixelRatio() : 1;
    *w = std::min(widget->width(),  static_cast<int>(content_w_px / dpr));
    *h = std::min(widget->height(), static_cast<int>(content_h_px / dpr));
    return *w > 0 && *h > 0;
}

bool VbamAndroidScreenClientSize(int* w, int* h) {
    int content_w_px = 0, content_h_px = 0;
    if (!AndroidContentSizePx(&content_w_px, &content_h_px)) {
        return false;
    }
    // No widget to ask, so scale by the screen: on Android every window shares
    // the one screen's device pixel ratio.
    qreal dpr = 1;
    if (const QScreen* screen = QGuiApplication::primaryScreen()) {
        if (screen->devicePixelRatio() > 0) {
            dpr = screen->devicePixelRatio();
        }
    }
    *w = static_cast<int>(content_w_px / dpr);
    *h = static_cast<int>(content_h_px / dpr);
    return *w > 0 && *h > 0;
}

void VbamReparentIntoAndroidViewport(void* child_qwidget, void* scrollarea_qwidget) {
    QWidget* const child = reinterpret_cast<QWidget*>(child_qwidget);
    QAbstractScrollArea* const area = qobject_cast<QAbstractScrollArea*>(
        reinterpret_cast<QWidget*>(scrollarea_qwidget));
    if (!child || !area) {
        return;
    }
    QWidget* const viewport = area->viewport();
    if (!viewport || child->parentWidget() == viewport) {
        return;
    }
    // setParent() hides the widget and does not promise to keep its geometry, and
    // the caller's wx layout has already run by now, so restore both. isHidden()
    // rather than isVisible(): the dialog itself is usually still hidden here, in
    // which case none of its children are visible either.
    const bool hidden = child->isHidden();
    const QRect geometry = child->geometry();
    child->setParent(viewport, child->windowFlags());
    child->setGeometry(geometry);
    child->setHidden(hidden);
}

void VbamEnableAndroidTouchScrolling(void* qwidget, std::function<void(int, int)> on_scroll) {
    QAbstractScrollArea* area = qobject_cast<QAbstractScrollArea*>(
        reinterpret_cast<QWidget*>(qwidget));
    if (!area) {
        return;
    }
    // A desktop scrollbar is a few pixels wide, which on a phone is far below a
    // usable touch target. Style the two scrollbars directly rather than the
    // scroll area, so the stylesheet cannot cascade into the dialog's controls.
    if (QScrollBar* bar = area->verticalScrollBar()) {
        bar->setStyleSheet("QScrollBar:vertical { width: 22px; }");
    }
    if (QScrollBar* bar = area->horizontalScrollBar()) {
        bar->setStyleSheet("QScrollBar:horizontal { height: 22px; }");
    }
    // Kinetic drag-to-scroll, so the content follows the finger the way a
    // native list does instead of only moving via the scrollbars. Qt replays
    // presses that turn out not to be drags, so taps on the controls inside
    // still arrive (with a short press delay).
    QScroller::grabGesture(area->viewport(), QScroller::TouchGesture);

    if (!on_scroll) {
        return;
    }
    // Report every scrollbar movement to wx.
    //
    // wxQt's scroll area only translates QAbstractSlider::actionTriggered and
    // sliderReleased into wx scroll events, and neither is emitted when the
    // value is set programmatically -- which is exactly how the kinetic
    // scrolling just grabbed above moves the view: QScroller hands the scroll
    // area a QScrollEvent, which sets the scrollbar values. wx would never learn
    // about it, so its scroll position would stay at zero and the child widgets
    // it positions would never move: the scrollbars slide and nothing else does.
    //
    // The callback runs the scroll on the wx side, which repositions the content
    // and sets the scrollbar position back to where it already is (a no-op for
    // Qt, and guarded against recursion below regardless).
    const auto guard = std::make_shared<bool>(false);
    const auto notify = [area, on_scroll, guard] {
        if (*guard) {
            return;
        }
        *guard = true;
        const int x = area->horizontalScrollBar() ? area->horizontalScrollBar()->value() : 0;
        const int y = area->verticalScrollBar() ? area->verticalScrollBar()->value() : 0;
        on_scroll(x, y);
        *guard = false;
    };
    if (QScrollBar* bar = area->horizontalScrollBar()) {
        QObject::connect(bar, &QScrollBar::valueChanged, area, notify);
    }
    if (QScrollBar* bar = area->verticalScrollBar()) {
        QObject::connect(bar, &QScrollBar::valueChanged, area, notify);
    }
}

// Creates (or returns the already-created) overlay SurfaceView covering the
// given physical-pixel rect and returns its ANativeWindow*, or nullptr.
static void* VbamCreateAndroidVideoSurfaceRect(int x, int y, int w, int h) {
    QJniObject activity = QNativeInterface::QAndroidApplication::context();
    __android_log_print(ANDROID_LOG_INFO, "VBAM",
                        "CreateAndroidVideoSurface activityValid=%d rect=%d,%d %dx%d",
                        (int)activity.isValid(), x, y, w, h);
    if (!activity.isValid()) {
        return nullptr;
    }
    QJniObject surface = QJniObject::callStaticObjectMethod(
        "org/visualboyadvance_m/VbamVideoSurface", "create",
        "(Landroid/app/Activity;IIII)Landroid/view/Surface;",
        activity.object(), x, y, w, h);
    QJniEnvironment jenv;
    if (jenv->ExceptionCheck()) {
        jenv->ExceptionDescribe();
        jenv->ExceptionClear();
        __android_log_print(ANDROID_LOG_ERROR, "VBAM", "VbamVideoSurface.create threw");
    }
    __android_log_print(ANDROID_LOG_INFO, "VBAM", "VbamVideoSurface.create surfaceValid=%d",
                        (int)surface.isValid());
    if (!surface.isValid()) {
        return nullptr;
    }
    return ANativeWindow_fromSurface(jenv.jniEnv(), surface.object());
}

// Creates the overlay SurfaceView (org.visualboyadvance_m.VbamVideoSurface)
// positioned over `qwidget` (the render panel) and returns its ANativeWindow*,
// or nullptr on failure. Used by both the SDL renderer (which retains it itself)
// and the Vulkan renderer, which needs an ANativeWindow for
// VK_KHR_android_surface; the returned window carries one reference the caller
// owns and must ANativeWindow_release().
void* VbamCreateAndroidVideoSurface(void* qwidget) {
    int x, y, w, h;
    VbamWidgetScreenRectPx(qwidget, &x, &y, &w, &h);
    return VbamCreateAndroidVideoSurfaceRect(x, y, w, h);
}

void VbamSetAndroidVideoSurfaceGeometry(void* qwidget) {
    QJniObject activity = QNativeInterface::QAndroidApplication::context();
    if (!activity.isValid()) {
        return;
    }
    int x, y, w, h;
    VbamWidgetScreenRectPx(qwidget, &x, &y, &w, &h);
    __android_log_print(ANDROID_LOG_INFO, "VBAM",
                        "SetAndroidVideoSurfaceGeometry rect=%d,%d %dx%d", x, y, w, h);
    QJniObject::callStaticMethod<void>(
        "org/visualboyadvance_m/VbamVideoSurface", "setGeometry",
        "(Landroid/app/Activity;IIII)V", activity.object(), x, y, w, h);
}

void VbamDestroyAndroidVideoSurface() {
    QJniObject activity = QNativeInterface::QAndroidApplication::context();
    if (!activity.isValid()) {
        return;
    }
    QJniObject::callStaticMethod<void>(
        "org/visualboyadvance_m/VbamVideoSurface", "destroy",
        "(Landroid/app/Activity;)V", activity.object());
}

#endif  // defined(__WXQT__) && defined(__ANDROID__)
