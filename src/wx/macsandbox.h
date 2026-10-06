#ifndef VBAM_WX_MACSANDBOX_H_
#define VBAM_WX_MACSANDBOX_H_

// macOS App Sandbox support for the wx port.
//
// A sandboxed process only sees its own container plus whatever the user
// hands it through a file dialog, drag and drop or Launch Services -- and
// that access is per file and lasts for the process lifetime only. So:
//
//  - Security-scoped bookmarks: every ROM the user opens and every
//    directory / BIOS file picked in a dialog is remembered as a bookmark in
//    the config file, and the next launch resolves them all up front. That
//    is what keeps the Recent menu, the Directories settings and the BIOS
//    paths working across launches.
//
//  - Command-line ROMs: a sandboxed process gets no access to the paths in
//    its argv, so LoadGame() asks through an open dialog at the ROM's folder
//    (RequestAccess()); granting the folder is bookmarked, and the same
//    command line works on later launches without asking.
//
//  - Battery saves: a ROM opened from a dialog grants access to that file
//    alone, not to its folder, so the ROM's directory is not a usable
//    default unless the folder itself was granted (Directories option,
//    RequestAccess()) and is writable. Otherwise .sav files go to
//    <container home>/Saves (SavesDir()) unless the user configured a
//    battery directory. Sidecar
//    files next to the ROM (an old .sav, patches, cheats) are only
//    reachable when the user points the matching Directories option at
//    that folder, which bookmarks it. (Apple's related-items mechanism,
//    NSFilePresenter with primaryPresentedItemURL, was tried for the .sav
//    and is refused with EPERM on macOS 26 for a Launch Services-opened
//    ROM, so it is not used.)
//
//  - BIOS images: a BIOS picked in a dialog is copied into
//    <container home>/BIOS (ImportBios()) and the option points at the
//    copy, so it needs neither a bookmark nor the original to stay put.
//
//  - Other fallbacks (recompute_dirs for states, GetGamePath for
//    screenshots and recordings) already send output to the data dir when
//    the ROM's directory is not writable.
//
// All of this is a no-op when the process is not sandboxed (the
// APP_SANDBOX_CONTAINER_ID environment variable is absent) and on every
// other platform.

#include <functional>
#include <string>
#include <vector>

#include <wx/arrstr.h>
#include <wx/string.h>

namespace macsandbox {

#if defined(__WXMAC__)

// True when this process runs inside the macOS App Sandbox.
bool Active();

// Store a security-scoped bookmark for `path` (a file or a directory the
// process currently has access to) so RestoreAccess() can reopen it on the
// next launch. Silently does nothing when not sandboxed or when no bookmark
// can be made (no access to the path).
void RememberPath(const wxString& path);

// Resolve every stored bookmark and start accessing it for the rest of the
// process lifetime. Call once, after the config file has been loaded.
void RestoreAccess();

// <container home>/Saves, created on demand: where battery saves go when
// sandboxed and no battery directory is configured. Empty when not sandboxed
// or when the directory cannot be created.
wxString SavesDir();

// Copy a BIOS image the user just picked into <container home>/BIOS (created
// on demand) and return the copy's path, so the option can point at a file
// inside the container. Returns `path` unchanged when not sandboxed, when
// `path` already lives in that directory, or when the copy fails.
wxString ImportBios(const wxString& path);

// Make `path` readable when the sandbox denies it -- a ROM named on the
// command line (a sandboxed process gets no access to its argv paths, only
// to what a dialog, drag and drop or Launch Services hands it), or a Recent
// entry whose bookmark was lost. Asks through an open dialog at the file's
// folder, showing `message` and with `prompt` on its button: choosing the
// folder grants every file in it, and is bookmarked, so the same command line
// works on every later launch without asking. Returns the path to load: `path`
// itself, or the file the user chose instead. A no-op returning `path` when not
// sandboxed, when the path is readable, or when it fails for a reason the
// sandbox is not behind (it does not exist).
wxString RequestAccess(const wxString& path, const wxString& message,
                       const wxString& prompt);

// Start a new instance of this application bundle through Launch Services,
// for a restart, with `args` as its command line (argv[1] on). Executing the
// bundle's binary directly (wxExecute) makes it a child of this process: it
// inherits the sandbox, and libsecinit kills it with SIGILL before main() when
// its own entitlement asks for the sandbox again. A Launch Services launch
// starts in "/", so relative arguments naming existing files are made
// absolute. The launch finishes asynchronously, and needs the main thread to
// do it, so the result comes to `done` on the main thread: true when the new
// instance is running, false when the launch failed or this is not an
// application bundle.
//
// Launch Services ignores the arguments of a sandboxed caller, so when
// sandboxed they go through a file in the container instead, which the new
// instance reads with TakeRelaunchArguments(); and every file argument this
// process can reach is bookmarked again (RememberPath()), so the new instance
// can open it even after a factory reset deleted the stored bookmarks.
void LaunchNewInstance(const wxArrayString& args, std::function<void(bool)> done);

// In a sandboxed process started by LaunchNewInstance(): the arguments the old
// instance handed over, read once and removed. Empty when there are none, when
// the hand-over is more than a minute old, or when not sandboxed. Call from
// main(), before wx parses the command line, and only when the process was
// started without arguments of its own.
std::vector<std::string> TakeRelaunchArguments();

#else  // !__WXMAC__

inline bool Active() { return false; }
inline void RememberPath(const wxString&) {}
inline void RestoreAccess() {}
inline wxString SavesDir() { return wxString(); }
inline wxString ImportBios(const wxString& path) { return path; }
inline wxString RequestAccess(const wxString& path, const wxString&,
                              const wxString&) { return path; }
inline void LaunchNewInstance(const wxArrayString&, std::function<void(bool)> done) {
    done(false);
}
inline std::vector<std::string> TakeRelaunchArguments() { return {}; }

#endif  // __WXMAC__

}  // namespace macsandbox

#endif  // VBAM_WX_MACSANDBOX_H_
