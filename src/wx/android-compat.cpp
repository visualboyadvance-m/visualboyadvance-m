// Android/wxQt link compatibility shims.
//
// The wxQt build for Android is configured with wxUSE_CONSOLE_EVENTLOOP=0, which
// compiles out the definition of wxGUIAppTraits::GetEventLoopSourcesManager()
// while the Qt apptraits object still references it. Provide the missing
// definition here. Qt drives the event loop on Android and VBA-M registers no
// wxEventLoopSource file descriptors, so returning nullptr is safe.

#if defined(__WXQT__) && defined(__ANDROID__)

#include <cstring>

#include <android/log.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>

#include <QtCore/QByteArray>
#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QJniEnvironment>
#include <QtCore/QJniObject>
#include <QtCore/QStandardPaths>
#include <QtCore/QString>
#include <QtWidgets/QWidget>

#include <wx/apptrait.h>
#include <wx/artprov.h>
#include <wx/bitmap.h>
#include <wx/image.h>
#include <wx/log.h>
#include <wx/string.h>

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

// The Android file picker returns Storage-Access-Framework "content://" URIs,
// which VBA-M's stdio-based ROM loader can't open. Resolve such a URI to a real
// local file: read the stream through Qt (which understands content:// on
// Android) and copy it into the app cache under its display name, so the ROM
// type detection (which keys off the file extension) still works. Non-content
// paths are returned unchanged.
wxString VbamResolveAndroidContentUri(const wxString& path) {
    if (!path.StartsWith("content://")) {
        return path;
    }

    const QString uri = QString::fromStdString(path.ToStdString());

    // Query the human-readable display name for its extension.
    QString display_name;
    QJniObject j_uri_str = QJniObject::fromString(uri);
    QJniObject j_uri = QJniObject::callStaticObjectMethod(
        "android/net/Uri", "parse", "(Ljava/lang/String;)Landroid/net/Uri;", j_uri_str.object());
    QJniObject context = QNativeInterface::QAndroidApplication::context();
    if (j_uri.isValid() && context.isValid()) {
        QJniObject resolver =
            context.callObjectMethod("getContentResolver", "()Landroid/content/ContentResolver;");
        if (resolver.isValid()) {
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
                    QJniObject name_obj =
                        cursor.callObjectMethod("getString", "(I)Ljava/lang/String;", idx);
                    if (name_obj.isValid()) {
                        display_name = name_obj.toString();
                    }
                }
                cursor.callMethod<void>("close");
            }
        }
    }

    if (display_name.isEmpty()) {
        display_name = QStringLiteral("rom.gba");
    }

    QFile in(uri);
    if (!in.open(QIODevice::ReadOnly)) {
        return path;  // let the caller report the load failure
    }
    const QByteArray bytes = in.readAll();
    in.close();

    QString out_dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (out_dir.isEmpty()) {
        out_dir = QDir::tempPath();
    }
    QDir().mkpath(out_dir);
    const QString out_path = out_dir + QLatin1Char('/') + display_name;

    QFile out(out_path);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return path;
    }
    out.write(bytes);
    out.close();

    return wxString::FromUTF8(out_path.toUtf8().constData());
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

// Creates the overlay SurfaceView (org.visualboyadvance_m.VbamVideoSurface)
// positioned over `qwidget` (the SDL render panel) and returns its
// ANativeWindow* (retained by SDL), or nullptr on failure.
void* VbamCreateAndroidVideoSurface(void* qwidget) {
    QJniObject activity = QNativeInterface::QAndroidApplication::context();
    int x, y, w, h;
    VbamWidgetScreenRectPx(qwidget, &x, &y, &w, &h);
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

void VbamSetAndroidVideoSurfaceGeometry(void* qwidget) {
    QJniObject activity = QNativeInterface::QAndroidApplication::context();
    if (!activity.isValid()) {
        return;
    }
    int x, y, w, h;
    VbamWidgetScreenRectPx(qwidget, &x, &y, &w, &h);
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
