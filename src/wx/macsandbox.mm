// macOS App Sandbox support: security-scoped bookmarks.
// See macsandbox.h for the overview. Built without ARC, like the other
// ObjC++ sources of the wx port.
//
// Diagnostics go through NSLog rather than wxLogDebug: sandbox trouble is
// investigated on release builds launched from the Finder, where the
// unified log (Console.app, `log show --predicate 'process ==
// "visualboyadvance-m"'`) is the only place output is seen.

#include "wx/macsandbox.h"

#import <Foundation/Foundation.h>

#include <cstdlib>
#include <vector>

#include <wx/base64.h>
#include <wx/buffer.h>
#include <wx/config.h>
#include <wx/filefn.h>
#include <wx/filename.h>

namespace macsandbox {

namespace {

// Config group holding the bookmarks: PathN (for display / de-duplication)
// and BookmarkN (base64 bookmark data), N = 0.. in least-recently-added order.
const wxChar kConfigGroup[] = wxT("/MacSandbox");
const size_t kMaxBookmarks = 64;

struct Entry {
    wxString path;
    wxMemoryBuffer bookmark;
};

// URLs whose security-scoped access we started; retained for the process
// lifetime (the kernel releases the scopes with the process).
NSMutableArray<NSURL*>* g_accessed = nil;

NSURL* UrlFor(const wxString& path) {
    wxFileName fn(path);
    fn.MakeAbsolute();
    NSString* s = [NSString stringWithUTF8String:fn.GetFullPath().utf8_str()];
    return s ? [NSURL fileURLWithPath:s] : nil;
}

std::vector<Entry> LoadEntries(wxConfigBase* cfg) {
    std::vector<Entry> entries;
    const wxString old_path = cfg->GetPath();
    cfg->SetPath(kConfigGroup);
    for (size_t i = 0;; i++) {
        wxString path, data;
        if (!cfg->Read(wxString::Format(wxT("Path%zu"), i), &path) ||
            !cfg->Read(wxString::Format(wxT("Bookmark%zu"), i), &data)) {
            break;
        }
        Entry e;
        e.path = path;
        e.bookmark = wxBase64Decode(data);
        if (!e.path.empty() && e.bookmark.GetDataLen() > 0) {
            entries.push_back(e);
        }
    }
    cfg->SetPath(old_path);
    return entries;
}

void SaveEntries(wxConfigBase* cfg, const std::vector<Entry>& entries) {
    const wxString old_path = cfg->GetPath();
    cfg->DeleteGroup(kConfigGroup);
    cfg->SetPath(kConfigGroup);
    for (size_t i = 0; i < entries.size(); i++) {
        cfg->Write(wxString::Format(wxT("Path%zu"), i), entries[i].path);
        cfg->Write(wxString::Format(wxT("Bookmark%zu"), i),
                   wxBase64Encode(entries[i].bookmark.GetData(),
                                  entries[i].bookmark.GetDataLen()));
    }
    cfg->SetPath(old_path);
    cfg->Flush();
}

// Bookmark data for a path the process currently has access to, or an
// empty buffer.
wxMemoryBuffer MakeBookmark(NSURL* url) {
    wxMemoryBuffer out;
    if (!url)
        return out;
    NSError* err = nil;
    NSData* data = [url bookmarkDataWithOptions:NSURLBookmarkCreationWithSecurityScope
                 includingResourceValuesForKeys:nil
                                  relativeToURL:nil
                                          error:&err];
    if (!data) {
        NSLog(@"macsandbox: no bookmark for %@: %@", url.path, err.localizedDescription);
        return out;
    }
    out.AppendData(data.bytes, data.length);
    return out;
}

}  // namespace

bool Active() {
    static const bool active = std::getenv("APP_SANDBOX_CONTAINER_ID") != nullptr;
    return active;
}

void RememberPath(const wxString& path) {
    if (!Active() || path.empty())
        return;
    wxConfigBase* cfg = wxConfigBase::Get(false);
    if (!cfg)
        return;

    @autoreleasepool {
        NSURL* url = UrlFor(path);
        wxMemoryBuffer bookmark = MakeBookmark(url);
        if (bookmark.GetDataLen() == 0)
            return;

        wxFileName fn(path);
        fn.MakeAbsolute();
        const wxString full = fn.GetFullPath();

        std::vector<Entry> entries = LoadEntries(cfg);
        for (auto it = entries.begin(); it != entries.end();) {
            if (it->path == full)
                it = entries.erase(it);
            else
                ++it;
        }
        Entry e;
        e.path = full;
        e.bookmark = bookmark;
        entries.push_back(e);
        while (entries.size() > kMaxBookmarks)
            entries.erase(entries.begin());
        SaveEntries(cfg, entries);
    }
}

void RestoreAccess() {
    if (!Active())
        return;
    wxConfigBase* cfg = wxConfigBase::Get(false);
    if (!cfg)
        return;

    @autoreleasepool {
        if (!g_accessed)
            g_accessed = [[NSMutableArray alloc] init];

        std::vector<Entry> entries = LoadEntries(cfg);
        std::vector<Entry> kept;
        bool changed = false;
        for (Entry& e : entries) {
            NSData* data = [NSData dataWithBytes:e.bookmark.GetData()
                                          length:e.bookmark.GetDataLen()];
            BOOL stale = NO;
            NSError* err = nil;
            NSURL* url = [NSURL URLByResolvingBookmarkData:data
                                                   options:NSURLBookmarkResolutionWithSecurityScope
                                             relativeToURL:nil
                                       bookmarkDataIsStale:&stale
                                                     error:&err];
            if (!url) {
                // The file or folder is gone (or the bookmark is from another
                // machine); nothing to restore, forget it.
                NSLog(@"macsandbox: dropping bookmark for %s: %@", e.path.utf8_str().data(),
                      err.localizedDescription);
                changed = true;
                continue;
            }
            if (![url startAccessingSecurityScopedResource]) {
                NSLog(@"macsandbox: cannot access %s", e.path.utf8_str().data());
                changed = true;
                continue;
            }
            [g_accessed addObject:url];
            if (stale) {
                // Refresh while we hold access, so the next launch works too.
                wxMemoryBuffer fresh = MakeBookmark(url);
                if (fresh.GetDataLen() > 0) {
                    e.bookmark = fresh;
                    changed = true;
                }
            }
            kept.push_back(e);
        }
        if (changed)
            SaveEntries(cfg, kept);
    }
}

namespace {

// <container home>/<name>, created if needed; empty when not sandboxed or
// on failure. NSHomeDirectory() is the container's Data directory for a
// sandboxed process.
wxString ContainerSubdir(const wxString& name) {
    if (!Active())
        return wxString();
    @autoreleasepool {
        NSString* home = NSHomeDirectory();
        if (!home)
            return wxString();
        wxFileName dir(wxString::FromUTF8(home.UTF8String), wxEmptyString);
        dir.AppendDir(name);
        if (!dir.DirExists() && !dir.Mkdir(0777, wxPATH_MKDIR_FULL)) {
            NSLog(@"macsandbox: cannot create %s", dir.GetPath().utf8_str().data());
            return wxString();
        }
        return dir.GetPath();
    }
}

}  // namespace

wxString SavesDir() {
    return ContainerSubdir(wxT("Saves"));
}

wxString ImportBios(const wxString& path) {
    if (!Active() || path.empty())
        return path;
    const wxString dir = ContainerSubdir(wxT("BIOS"));
    if (dir.empty())
        return path;

    wxFileName src(path);
    src.MakeAbsolute();
    if (!src.FileExists())
        return path;
    wxFileName dst(dir, src.GetFullName());
    if (dst.GetFullPath() == src.GetFullPath())
        return path;  // already the imported copy
    if (!wxCopyFile(src.GetFullPath(), dst.GetFullPath(), true)) {
        NSLog(@"macsandbox: cannot copy BIOS %s to %s", src.GetFullPath().utf8_str().data(),
              dst.GetFullPath().utf8_str().data());
        return path;
    }
    return dst.GetFullPath();
}

}  // namespace macsandbox
