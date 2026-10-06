// macOS App Sandbox support: security-scoped bookmarks.
// See macsandbox.h for the overview. Built without ARC, like the other
// ObjC++ sources of the wx port.
//
// Diagnostics go through NSLog rather than wxLogDebug: sandbox trouble is
// investigated on release builds launched from the Finder, where the
// unified log (Console.app, `log show --predicate 'process ==
// "visualboyadvance-m"'`) is the only place output is seen.

#include "wx/macsandbox.h"

#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>

#include <cerrno>
#include <cstdlib>
#include <fcntl.h>
#include <unistd.h>
#include <vector>

#include <wx/app.h>
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

wxString RequestAccess(const wxString& path, const wxString& message,
                       const wxString& prompt) {
    if (!Active() || path.empty())
        return path;

    wxFileName fn(path);
    fn.MakeAbsolute();
    const wxString full = fn.GetFullPath();

    // Try the open itself rather than access(2): EPERM is how the sandbox
    // says no, EACCES a permission it may be behind too, and anything else
    // (ENOENT) is left to the loader to report.
    const int fd = open(full.utf8_str(), O_RDONLY);
    if (fd >= 0) {
        close(fd);
        return path;
    }
    if (errno != EPERM && errno != EACCES)
        return path;

    @autoreleasepool {
        NSString* dir = [NSString stringWithUTF8String:fn.GetPath().utf8_str()];
        if (!dir)
            return path;

        // Opened in the ROM's folder with directories choosable, the button
        // with nothing selected picks that folder -- the one click that makes
        // the whole folder, its .sav files included, reachable from now on.
        NSOpenPanel* panel = [NSOpenPanel openPanel];
        panel.canChooseDirectories = YES;
        panel.canChooseFiles = YES;
        panel.allowsMultipleSelection = NO;
        panel.directoryURL = [NSURL fileURLWithPath:dir isDirectory:YES];
        panel.message = [NSString stringWithUTF8String:message.utf8_str()];
        panel.prompt = [NSString stringWithUTF8String:prompt.utf8_str()];

        if ([panel runModal] != NSModalResponseOK || !panel.URL) {
            NSLog(@"macsandbox: access to %s not granted", full.utf8_str().data());
            return path;
        }

        const wxString chosen = wxString::FromUTF8(panel.URL.fileSystemRepresentation);
        RememberPath(chosen);

        // A folder: the original path, now reachable if it is inside it. A
        // file: the one the user picked instead.
        return wxDirExists(chosen) ? path : chosen;
    }
}

namespace {

// The hand-over of a sandboxed relaunch's arguments: in the container's own
// temporary directory, which the old and the new instance share.
NSString* const kRelaunchArgs = @"Arguments";
NSString* const kRelaunchTime = @"Time";
const NSTimeInterval kRelaunchMaxAge = 60;

NSURL* RelaunchFile() {
    return [NSURL fileURLWithPath:[NSTemporaryDirectory()
                                      stringByAppendingPathComponent:@"vbam-relaunch-args.plist"]];
}

}  // namespace

void LaunchNewInstance(const wxArrayString& args, std::function<void(bool)> done) {
    // Always answered from the event loop, so the caller sees the same order
    // of events whether the launch is synchronous or not.
    auto answer = [done](bool launched) {
        if (!launched && Active())
            [[NSFileManager defaultManager] removeItemAtURL:RelaunchFile() error:nil];
        wxTheApp->CallAfter([done, launched] { done(launched); });
    };

    @autoreleasepool {
        NSURL* bundle = [[NSBundle mainBundle] bundleURL];
        if (!bundle || ![bundle.pathExtension isEqualToString:@"app"]) {
            answer(false);
            return;
        }

        NSMutableArray<NSString*>* arguments = [NSMutableArray array];
        for (const wxString& arg : args) {
            // The process serial number Launch Services gave this launch on
            // older systems; the new one gets its own.
            if (arg.StartsWith("-psn_"))
                continue;
            wxString out = arg;
            if (!arg.StartsWith("-")) {
                wxFileName fn(arg);
                if (fn.Exists()) {
                    fn.MakeAbsolute();
                    out = fn.GetFullPath();
                    RememberPath(out);
                }
            }
            NSString* s = [NSString stringWithUTF8String:out.utf8_str()];
            if (s)
                [arguments addObject:s];
        }

        if (Active() && arguments.count != 0) {
            NSDictionary* handover = @{kRelaunchArgs : arguments, kRelaunchTime : [NSDate date]};
            NSError* error = nil;
            NSData* data = [NSPropertyListSerialization dataWithPropertyList:handover
                                                                      format:NSPropertyListBinaryFormat_v1_0
                                                                     options:0
                                                                       error:&error];
            if (!data || ![data writeToURL:RelaunchFile() options:NSDataWritingAtomic error:&error])
                NSLog(@"macsandbox: cannot hand the arguments over: %@", error);
        }

        NSWorkspace* ws = [NSWorkspace sharedWorkspace];
        if (@available(macOS 10.15, *)) {
            NSWorkspaceOpenConfiguration* config = [NSWorkspaceOpenConfiguration configuration];
            config.createsNewApplicationInstance = YES;
            config.arguments = arguments;
            [ws openApplicationAtURL:bundle
                       configuration:config
                   completionHandler:^(NSRunningApplication* app, NSError* error) {
                       if (error)
                           NSLog(@"macsandbox: relaunch failed: %@", error);
                       answer(app != nil);
                   }];
            return;
        }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        NSError* error = nil;
        NSRunningApplication* app =
            [ws launchApplicationAtURL:bundle
                               options:NSWorkspaceLaunchNewInstance
                         configuration:@{NSWorkspaceLaunchConfigurationArguments : arguments}
                                 error:&error];
#pragma clang diagnostic pop
        if (!app)
            NSLog(@"macsandbox: relaunch failed: %@", error);
        answer(app != nil);
    }
}

std::vector<std::string> TakeRelaunchArguments() {
    std::vector<std::string> out;
    if (!Active())
        return out;

    @autoreleasepool {
        NSURL* file = RelaunchFile();
        NSData* data = [NSData dataWithContentsOfURL:file];
        if (!data)
            return out;
        [[NSFileManager defaultManager] removeItemAtURL:file error:nil];

        NSDictionary* handover = [NSPropertyListSerialization propertyListWithData:data
                                                                           options:NSPropertyListImmutable
                                                                            format:nil
                                                                             error:nil];
        if (![handover isKindOfClass:[NSDictionary class]])
            return out;
        NSDate* time = handover[kRelaunchTime];
        NSArray* args = handover[kRelaunchArgs];
        if (![time isKindOfClass:[NSDate class]] || ![args isKindOfClass:[NSArray class]])
            return out;
        const NSTimeInterval age = -[time timeIntervalSinceNow];
        if (age < 0 || age > kRelaunchMaxAge) {
            NSLog(@"macsandbox: ignoring a relaunch hand-over %.0f s old", age);
            return out;
        }

        for (id arg in args) {
            if ([arg isKindOfClass:[NSString class]])
                out.push_back([arg UTF8String]);
        }
    }
    return out;
}

}  // namespace macsandbox
