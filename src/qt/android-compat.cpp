#include "qt/android-compat.h"

#if defined(__ANDROID__)

#include <algorithm>
#include <vector>

#include <android/log.h>
#include <android/native_window_jni.h>

#include <QByteArray>
#include <QCoreApplication>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJniEnvironment>
#include <QJniObject>
#include <QScreen>
#include <QStandardPaths>
#include <QStringList>
#include <QWidget>

// --- Storage Access Framework file staging -----------------------------------

namespace {

const char kContentScheme[] = "content://";

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
    QJniEnvironment env;
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        return QString();
    }
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

// Shared staging directory for SAF transfers. App data rather than the cache
// dir because formats that write a companion file -- the movie recorder emits
// a .vm0 save state beside its .vmv -- must find that companion again when the
// recording is played back in a later session.
QString SafStagingDir() {
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (dir.isEmpty()) {
        dir = QDir::tempPath();
    }
    dir += QStringLiteral("/saf-staging");
    QDir().mkpath(dir);
    return dir;
}

QString EnsureExtension(const QString& name, const QString& required_ext) {
    if (required_ext.isEmpty()) {
        return name;
    }
    const QString ext = QLatin1Char('.') + required_ext;
    if (name.endsWith(ext, Qt::CaseInsensitive)) {
        return name;
    }
    return name + ext;
}

// Copies the content of `uri` into `dir` under its display name (falling back
// to `fallback_name`) and returns the local path, or an empty string on error.
QString CopyContentUriToDir(const QString& uri, const QString& dir, const QString& fallback_name,
                            const QString& required_ext) {
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

QString VbamResolveAndroidContentUri(const QString& path) {
    if (!path.startsWith(QLatin1String(kContentScheme))) {
        return path;
    }

    QString cache_dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (cache_dir.isEmpty()) {
        cache_dir = QDir::tempPath();
    }

    const QString out_path =
        CopyContentUriToDir(path, cache_dir, QStringLiteral("rom.gba"), QString());
    if (out_path.isEmpty()) {
        return path;  // let the caller report the load failure
    }
    return out_path;
}

QString VbamStageAndroidInputFile(const QString& path, const QString& required_ext) {
    if (!path.startsWith(QLatin1String(kContentScheme))) {
        return path;
    }

    const QString out_path =
        CopyContentUriToDir(path, SafStagingDir(), QStringLiteral("movie"), required_ext);
    if (out_path.isEmpty()) {
        return path;  // let the caller report the load failure
    }
    return out_path;
}

QString VbamStageAndroidOutputFile(const QString& path, const QString& required_ext) {
    if (!path.startsWith(QLatin1String(kContentScheme))) {
        return path;
    }

    QString name = ContentUriDisplayName(path);
    if (name.isEmpty()) {
        name = QStringLiteral("output");
    }
    name = EnsureExtension(name, required_ext);

    // The staging name is derived from the display name rather than made unique
    // so that a companion file written beside a previous staging of the same
    // document is still found; drop any leftover content so a failed transfer
    // can never masquerade as this output.
    const QString staged = SafStagingDir() + QLatin1Char('/') + name;
    QFile::remove(staged);

    for (SafOutputTarget& target : SafOutputTargets()) {
        if (target.staged == staged) {
            target.uri = path;
            return staged;
        }
    }
    SafOutputTargets().push_back({staged, path});
    return staged;
}

bool VbamCommitAndroidOutputFile(const QString& staged) {
    std::vector<SafOutputTarget>& targets = SafOutputTargets();
    for (size_t i = 0; i < targets.size(); i++) {
        if (targets[i].staged != staged) {
            continue;
        }
        const QString uri = targets[i].uri;
        targets.erase(targets.begin() + i);
        // A writer that never produced the file (a failed start) has nothing
        // to transfer; report that rather than a transfer failure.
        if (!QFile::exists(staged)) {
            return true;
        }
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

void VbamCommitPendingAndroidOutputFiles(const QStringList& keep) {
    // Copy the list: committing mutates it.
    std::vector<QString> pending;
    for (const SafOutputTarget& target : SafOutputTargets()) {
        if (!keep.contains(target.staged)) {
            pending.push_back(target.staged);
        }
    }
    for (const QString& staged : pending) {
        VbamCommitAndroidOutputFile(staged);
    }
}

void VbamDiscardAndroidOutputFile(const QString& staged) {
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
    // Remember the last state we asked for: this is called on every
    // pause/resume and each JNI round trip posts to the UI thread.
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

// --- Menu bar (action bar) visibility -----------------------------------------

void VbamSetAndroidMenuBarHidden(bool hidden) {
    // Called from the idle loop on every frame to re-assert the option state,
    // so skip the JNI round trip when nothing changed.
    static int last = -1;
    if (last == static_cast<int>(hidden)) {
        return;
    }
    QJniObject activity = QNativeInterface::QAndroidApplication::context();
    if (!activity.isValid()) {
        return;
    }
    QJniObject::callStaticMethod<void>(
        "org/visualboyadvance_m/VbamMenuBar", "setHidden",
        "(Landroid/app/Activity;Z)V", activity.object(), static_cast<jboolean>(hidden));
    QJniEnvironment env;
    if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
        __android_log_print(ANDROID_LOG_ERROR, "VBAM", "VbamMenuBar.setHidden threw");
        return;
    }
    last = static_cast<int>(hidden);
}

// --- Screen geometry ------------------------------------------------------------

namespace {

// Size of the activity's content view in physical pixels, or false if no
// layout has been measured yet. The first call installs a layout listener on
// the Java side (VbamVideoSurface.watchContentSize) that keeps the size
// current across rotations.
bool AndroidContentSizePx(int* w, int* h) {
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
    QJniEnvironment env;
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        return false;
    }
    if (content_w_px <= 0 || content_h_px <= 0) {
        return false;
    }
    *w = content_w_px;
    *h = content_h_px;
    return true;
}

qreal ScreenDpr() {
    if (const QScreen* screen = QGuiApplication::primaryScreen()) {
        if (screen->devicePixelRatio() > 0) {
            return screen->devicePixelRatio();
        }
    }
    return 1;
}

}  // namespace

bool VbamAndroidVisibleClientSize(QWidget* widget, int* w, int* h) {
    if (!widget) {
        return false;
    }
    int content_w_px = 0, content_h_px = 0;
    if (!AndroidContentSizePx(&content_w_px, &content_h_px)) {
        return false;
    }
    const qreal dpr = widget->devicePixelRatio() > 0 ? widget->devicePixelRatio() : 1;
    *w = std::min(widget->width(), static_cast<int>(content_w_px / dpr));
    *h = std::min(widget->height(), static_cast<int>(content_h_px / dpr));
    return *w > 0 && *h > 0;
}

bool VbamAndroidScreenClientSize(int* w, int* h) {
    int content_w_px = 0, content_h_px = 0;
    if (!AndroidContentSizePx(&content_w_px, &content_h_px)) {
        return false;
    }
    const qreal dpr = ScreenDpr();
    *w = static_cast<int>(content_w_px / dpr);
    *h = static_cast<int>(content_h_px / dpr);
    return *w > 0 && *h > 0;
}

void VbamAdaptDialogToScreen(QDialog* dialog) {
    if (!dialog) {
        return;
    }
    int w = 0, h = 0;
    if (!VbamAndroidScreenClientSize(&w, &h)) {
        if (const QScreen* screen = QGuiApplication::primaryScreen()) {
            const QRect avail = screen->availableGeometry();
            w = avail.width();
            h = avail.height();
        }
    }
    if (w <= 0 || h <= 0) {
        return;
    }
    // Fill the content view: a dialog laid out for a desktop is almost always
    // larger than a phone screen, and a dialog smaller than the screen gains
    // finger room from the extra space.
    dialog->setMinimumSize(0, 0);
    dialog->setMaximumSize(w, h);
    dialog->resize(w, h);
    dialog->move(0, 0);
}

// --- SDL Java glue -------------------------------------------------------------

void VbamSetupSdlActivityJni() {
    static bool done = false;
    if (done) {
        return;
    }
    done = true;

    // SDL caches per-manager jclasses + method IDs in each class's
    // nativeSetupJNI(); the SDLActivity lifecycle normally calls all three.
    // Under the Qt activity we invoke them ourselves so the joystick, sensor
    // and audio backends do not hit a NULL jclass.
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

// --- Overlay SurfaceView for the Vulkan renderer -----------------------------

// A widget's on-screen rect in physical pixels (Qt logical coordinates times the
// device pixel ratio), which is what Android view LayoutParams use.
static void WidgetScreenRectPx(QWidget* widget, int* x, int* y, int* w, int* h) {
    *x = *y = 0;
    *w = *h = 0;
    if (!widget) {
        return;
    }
    const qreal dpr = widget->devicePixelRatio() > 0 ? widget->devicePixelRatio() : 1;
    const QPoint g = widget->mapToGlobal(QPoint(0, 0));
    const QSize sz = widget->size();
    *x = static_cast<int>(g.x() * dpr);
    *y = static_cast<int>(g.y() * dpr);
    *w = static_cast<int>(sz.width() * dpr);
    *h = static_cast<int>(sz.height() * dpr);
}

void* VbamCreateAndroidVideoSurface(QWidget* widget) {
    int x, y, w, h;
    WidgetScreenRectPx(widget, &x, &y, &w, &h);

    QJniObject activity = QNativeInterface::QAndroidApplication::context();
    __android_log_print(ANDROID_LOG_INFO, "VBAM",
                        "CreateAndroidVideoSurface activityValid=%d rect=%d,%d %dx%d",
                        static_cast<int>(activity.isValid()), x, y, w, h);
    if (!activity.isValid()) {
        return nullptr;
    }
    QJniObject surface = QJniObject::callStaticObjectMethod(
        "org/visualboyadvance_m/VbamVideoSurface", "create",
        "(Landroid/app/Activity;IIII)Landroid/view/Surface;", activity.object(), x, y, w, h);
    QJniEnvironment env;
    if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
        __android_log_print(ANDROID_LOG_ERROR, "VBAM", "VbamVideoSurface.create threw");
    }
    if (!surface.isValid()) {
        return nullptr;
    }
    return ANativeWindow_fromSurface(env.jniEnv(), surface.object());
}

void VbamSetAndroidVideoSurfaceGeometry(QWidget* widget) {
    QJniObject activity = QNativeInterface::QAndroidApplication::context();
    if (!activity.isValid()) {
        return;
    }
    int x, y, w, h;
    WidgetScreenRectPx(widget, &x, &y, &w, &h);
    QJniObject::callStaticMethod<void>("org/visualboyadvance_m/VbamVideoSurface", "setGeometry",
                                       "(Landroid/app/Activity;IIII)V", activity.object(), x, y,
                                       w, h);
    QJniEnvironment env;
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
    }
}

void VbamDestroyAndroidVideoSurface() {
    QJniObject activity = QNativeInterface::QAndroidApplication::context();
    if (!activity.isValid()) {
        return;
    }
    QJniObject::callStaticMethod<void>("org/visualboyadvance_m/VbamVideoSurface", "destroy",
                                       "(Landroid/app/Activity;)V", activity.object());
    QJniEnvironment env;
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
    }
}

#endif  // __ANDROID__
