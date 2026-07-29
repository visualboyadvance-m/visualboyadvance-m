#ifndef VBAM_WX_ANDROID_COMPAT_H_
#define VBAM_WX_ANDROID_COMPAT_H_

// Storage-Access-Framework glue for the Android (wxQt) build.
//
// The Android file picker hands back "content://" URIs rather than filesystem
// paths, which none of VBA-M's stdio/ffmpeg/wxFFile based readers and writers
// can open. The helpers below translate between a URI and a real local file:
// input URIs are copied out to a staging file, output URIs get a staging file
// that is copied back through the ContentResolver once the writer has closed
// it. On every other platform they are pass-throughs.

#include <functional>

#include <wx/string.h>

#if defined(__WXQT__) && defined(__ANDROID__)

// Copies a content:// URI into the app cache under its display name (so file
// type detection, which keys off the extension, still works) and returns that
// path. Non-content paths are returned unchanged. Used for one-shot reads such
// as loading a ROM or a BIOS image.
wxString VbamResolveAndroidContentUri(const wxString& path);

// Like VbamResolveAndroidContentUri(), but stages into the same directory the
// output helpers below use. Formats that write a companion file next to their
// main file -- the movie recorder emits a .vm0 save state beside its .vmv --
// need both halves to end up in one place, in this session and in later ones.
// `required_ext` works as in VbamStageAndroidOutputFile().
wxString VbamStageAndroidInputFile(const wxString& path, const wxString& required_ext);

// Returns a local path to write to. For a content:// URI this is a staging
// file named after the URI's display name; the mapping back to the URI is
// remembered until the file is committed or discarded. When `required_ext` is
// non-empty it is appended unless the display name already ends in it, so
// callers that derive companion names from the extension stay correct even if
// the provider reports a name without one. Non-content paths pass through and
// need no commit.
wxString VbamStageAndroidOutputFile(const wxString& path, const wxString& required_ext);

// Copies a staged output file back to the content:// URI it came from and
// removes the staging file. Call once the writer has closed the file. Returns
// false only if a known staging file failed to transfer; paths that were never
// staged report success, so call sites need no Android-specific branch.
bool VbamCommitAndroidOutputFile(const wxString& staged_path);

// Drops a staged output file without transferring it, for failed starts.
void VbamDiscardAndroidOutputFile(const wxString& staged_path);

// Holds the device awake (screen-on window flag plus, when the WAKE_LOCK
// permission is granted, a partial wake lock) so the display never dims or
// locks while a game runs. This is Android's equivalent of suspending the
// screensaver, and is driven from the same place: GameArea::SuspendScreenSaver()
// and GameArea::UnsuspendScreenSaver(). Idempotent; safe off the UI thread.
void VbamSetAndroidWakeLock(bool enable);

// Size of the activity's content view in Qt logical pixels, i.e. the area a
// top-level window can actually occupy. Unlike Qt's screen availableGeometry()
// this excludes the action bar, so a dialog clamped to it always fits. Returns
// false (leaving the outputs alone) before the first layout has been measured.
bool VbamAndroidScreenClientSize(int* w, int* h);

// Makes a scrolling wx window usable with a finger: widens its scrollbars to a
// touch target and enables Qt's kinetic drag-to-scroll on its viewport. Takes
// the wxWindow's Qt handle (wxWindow::GetHandle()); does nothing when that
// handle is not a scroll area.
//
// `on_scroll` is called with the new scrollbar positions, in the scroll units
// wx set the scrollbars up with, whenever Qt moves them. wxQt only forwards
// scrollbar *actions* -- a dragged thumb, a clicked arrow -- to wx, and a
// kinetic drag is neither: it sets the scrollbar values directly, so without
// this callback the scrollbars move while the content stays where it was. The
// callback has to scroll the wx window to the reported position; it may be
// empty for scroll areas wx does not manage itself.
void VbamEnableAndroidTouchScrolling(void* qwidget,
                                     std::function<void(int, int)> on_scroll = {});

// Puts a wx window that was reparented into a scrolling window where wxQt's own
// scrolling expects it: inside the scroll area's viewport.
//
// wxQt keeps the children of a scrolling window in the QScrollArea's viewport --
// wxWindowQt::AddChild() reparents every child there -- and scrolls by scrolling
// that viewport, which moves its children with it. wxWindow::Reparent() undoes
// that: after the base class has added the child to the viewport it reparents the
// Qt widget to wxWindowQt::QtGetParentWidget(), which for a scrolling window is
// the scroll area itself, not its viewport. The content then sits outside the
// widget wx scrolls, so scrolling moves the scrollbars and nothing else. Call
// this for each window reparented into a scroller.
//
// Takes the two wxWindow::GetHandle() pointers; does nothing when the second is
// not a scroll area.
void VbamReparentIntoAndroidViewport(void* child_qwidget, void* scrollarea_qwidget);

#else  // !(__WXQT__ && __ANDROID__)

inline wxString VbamResolveAndroidContentUri(const wxString& path) { return path; }
inline wxString VbamStageAndroidInputFile(const wxString& path, const wxString&) { return path; }
inline wxString VbamStageAndroidOutputFile(const wxString& path, const wxString&) { return path; }
inline bool VbamCommitAndroidOutputFile(const wxString&) { return true; }
inline void VbamDiscardAndroidOutputFile(const wxString&) {}
inline void VbamSetAndroidWakeLock(bool) {}
inline bool VbamAndroidScreenClientSize(int*, int*) { return false; }
inline void VbamEnableAndroidTouchScrolling(void*, std::function<void(int, int)> = {}) {}
inline void VbamReparentIntoAndroidViewport(void*, void*) {}

#endif  // defined(__WXQT__) && defined(__ANDROID__)

#endif  // VBAM_WX_ANDROID_COMPAT_H_
