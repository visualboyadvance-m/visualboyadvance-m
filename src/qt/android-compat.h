#ifndef VBAM_QT_ANDROID_COMPAT_H_
#define VBAM_QT_ANDROID_COMPAT_H_

// Android runtime glue for the Qt port. Port of the wx port's
// src/wx/android-compat.* minus the wx toolkit shims.
//
// The Android file picker hands back Storage-Access-Framework "content://"
// URIs rather than filesystem paths, which none of VBA-M's stdio / ffmpeg based
// readers and writers can open. The helpers below translate between a URI and
// a real local file: input URIs are copied out to a staging file, output URIs
// get a staging file that is copied back through the ContentResolver once the
// writer has closed it. On every other platform they are pass-throughs, so
// call sites need no #ifdef.
//
// The Java side lives in src/qt/android/src/org/visualboyadvance_m (shared
// with the wx port): VbamWakeLock, VbamMenuBar, VbamVideoSurface (only its
// content-size watcher is used here), VbamGamepad, VbamActivity.

#include <QString>

class QDialog;
class QWidget;

#if defined(__ANDROID__)

// Copies a content:// URI into the app cache under its display name (so file
// type detection, which keys off the extension, still works) and returns that
// path. Non-content paths are returned unchanged. Used for one-shot reads such
// as loading a ROM or a BIOS image.
QString VbamResolveAndroidContentUri(const QString& path);

// Like VbamResolveAndroidContentUri(), but stages into the same directory the
// output helpers below use. Formats that write a companion file next to their
// main file -- the movie recorder emits a .vm0 save state beside its .vmv --
// need both halves to end up in one place, in this session and in later ones.
// `required_ext` works as in VbamStageAndroidOutputFile().
QString VbamStageAndroidInputFile(const QString& path, const QString& required_ext);

// Returns a local path to write to. For a content:// URI this is a staging
// file named after the URI's display name; the mapping back to the URI is
// remembered until the file is committed or discarded. When `required_ext` is
// non-empty it is appended unless the display name already ends in it.
// Non-content paths pass through and need no commit.
QString VbamStageAndroidOutputFile(const QString& path, const QString& required_ext);

// Copies a staged output file back to the content:// URI it came from and
// removes the staging file. Call once the writer has closed the file. Returns
// false only if a known staging file failed to transfer; paths that were never
// staged report success.
bool VbamCommitAndroidOutputFile(const QString& staged_path);

// Commits every staged output file that is still pending except the ones in
// `keep` (files a writer still has open, e.g. a running recording). Used at
// the end of a command so the synchronous writers (save state, screenshot,
// battery export ...) need no per-call-site commit.
void VbamCommitPendingAndroidOutputFiles(const QStringList& keep);

// Drops a staged output file without transferring it, for failed starts.
void VbamDiscardAndroidOutputFile(const QString& staged_path);

// Holds the device awake (screen-on window flag plus, when the WAKE_LOCK
// permission is granted, a partial wake lock) so the display never dims or
// locks while a game runs. Driven from GameArea::SuspendScreenSaver() /
// UnsuspendScreenSaver(). Idempotent; safe off the UI thread.
void VbamSetAndroidWakeLock(bool enable);

// Hides or shows the activity's top bar (the action bar carrying the overflow
// menu the QMenuBar is mapped into) together with the system bars, so the game
// takes the whole screen. Driven by the Hide Menu Bar option, which on Android
// is an immediate toggle rather than the desktop mouse-idle auto-hide.
void VbamSetAndroidMenuBarHidden(bool hidden);

// Size of the activity's content view in Qt logical pixels, i.e. the area a
// top-level window can actually occupy. Unlike QScreen::availableGeometry()
// this excludes the action bar, so a dialog clamped to it always fits. Returns
// false (leaving the outputs alone) before the first layout has been measured.
bool VbamAndroidScreenClientSize(int* w, int* h);

// The part of `widget` that is actually on screen, in Qt logical pixels. Qt's
// Android window can be taller than the activity's content view (the action
// bar is not subtracted), so a widget laid out to fill it overhangs the bottom
// of the display; widgets that must stay visible clamp themselves to this.
bool VbamAndroidVisibleClientSize(QWidget* widget, int* w, int* h);

// Sizes `dialog` to the screen: Android dialogs that are sized to their
// content routinely end up larger than the display and unreachable. Clamps the
// dialog to the content view and moves it to the origin. Call from showEvent.
void VbamAdaptDialogToScreen(QDialog* dialog);

// Overlay SurfaceView (org.visualboyadvance_m.VbamVideoSurface) hosted in the
// Qt activity for renderers that need a native window of their own -- the
// Vulkan panel (VK_KHR_android_surface takes an ANativeWindow*, which Qt's
// Android QPA exposes for no QWindow). The view is stacked over the Qt content
// at `widget`'s on-screen rect; touches still reach the widgets under it.
//
// VbamCreateAndroidVideoSurface() returns the view's ANativeWindow* (as void*)
// carrying one reference the caller owns and must ANativeWindow_release(), or
// nullptr on failure. Only one overlay exists at a time; creating it again
// returns the same window.
void* VbamCreateAndroidVideoSurface(QWidget* widget);
// Moves / resizes the overlay onto `widget`'s current on-screen rect.
void VbamSetAndroidVideoSurfaceGeometry(QWidget* widget);
// Removes the overlay view. Call after the Vulkan surface built on it is gone.
void VbamDestroyAndroidVideoSurface();

// SDL's Android backend caches its Java classes and method IDs from
// SDLActivity.nativeSetupJNI(), which the SDLActivity lifecycle normally
// invokes. Under the Qt activity that never happens, so call this once before
// SDL_Init() (the SDL Java glue is bundled under org.libsdl.app).
void VbamSetupSdlActivityJni();

#else  // !__ANDROID__

inline QString VbamResolveAndroidContentUri(const QString& path) { return path; }
inline QString VbamStageAndroidInputFile(const QString& path, const QString&) { return path; }
inline QString VbamStageAndroidOutputFile(const QString& path, const QString&) { return path; }
inline bool VbamCommitAndroidOutputFile(const QString&) { return true; }
inline void VbamCommitPendingAndroidOutputFiles(const QStringList&) {}
inline void VbamDiscardAndroidOutputFile(const QString&) {}
inline void VbamSetAndroidWakeLock(bool) {}
inline void VbamSetAndroidMenuBarHidden(bool) {}
inline bool VbamAndroidScreenClientSize(int*, int*) { return false; }
inline bool VbamAndroidVisibleClientSize(QWidget*, int*, int*) { return false; }
inline void VbamAdaptDialogToScreen(QDialog*) {}
inline void VbamSetupSdlActivityJni() {}
inline void* VbamCreateAndroidVideoSurface(QWidget*) { return nullptr; }
inline void VbamSetAndroidVideoSurfaceGeometry(QWidget*) {}
inline void VbamDestroyAndroidVideoSurface() {}

#endif  // __ANDROID__

#endif  // VBAM_QT_ANDROID_COMPAT_H_
