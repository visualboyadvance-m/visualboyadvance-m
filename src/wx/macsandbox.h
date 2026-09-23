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
//  - Battery saves: a ROM opened from a dialog grants access to that file
//    alone, never to its folder, so the ROM's directory is never a usable
//    default. When sandboxed, .sav files go to <container home>/Saves
//    (SavesDir()) unless the user configured a battery directory. Sidecar
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

#else  // !__WXMAC__

inline bool Active() { return false; }
inline void RememberPath(const wxString&) {}
inline void RestoreAccess() {}
inline wxString SavesDir() { return wxString(); }
inline wxString ImportBios(const wxString& path) { return path; }

#endif  // __WXMAC__

}  // namespace macsandbox

#endif  // VBAM_WX_MACSANDBOX_H_
