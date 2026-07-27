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

#else  // !(__WXQT__ && __ANDROID__)

inline wxString VbamResolveAndroidContentUri(const wxString& path) { return path; }
inline wxString VbamStageAndroidInputFile(const wxString& path, const wxString&) { return path; }
inline wxString VbamStageAndroidOutputFile(const wxString& path, const wxString&) { return path; }
inline bool VbamCommitAndroidOutputFile(const wxString&) { return true; }
inline void VbamDiscardAndroidOutputFile(const wxString&) {}

#endif  // defined(__WXQT__) && defined(__ANDROID__)

#endif  // VBAM_WX_ANDROID_COMPAT_H_
