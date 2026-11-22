// vbam-updater.app: installs a downloaded update of the application whose
// Contents/Helpers directory it is in, in place (see inplace-update.h).
//
// It is not sandboxed and is started through Launch Services, so it can
// write where the sandboxed application cannot. The application leaves the
// request (process id, the downloaded zip, its bundle path) in
// Library/Caches/<bundle id>/vbam-inplace-update.plist under its home: the
// sandbox container's, or the user's own home when not sandboxed. Only the
// bundle this helper lives in is ever replaced.
//
//  1. wait for the application to exit (it saves its configuration then)
//  2. unpack the zip with ditto next to the installed bundle (same volume)
//  3. check that it holds one .app with the same bundle identifier
//  4. make its executables executable and clear the quarantine flag
//  5. swap it with the installed bundle and remove the old one
//  6. relaunch
//
// When the bundle cannot be replaced (a read-only location, a translocated
// app), the update is unpacked into ~/Downloads instead, shown in the Finder
// and the user is told.

#import <CoreServices/CoreServices.h>
#import <Foundation/Foundation.h>

#include <errno.h>
#include <mach-o/fat.h>
#include <mach-o/loader.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/xattr.h>
#include <unistd.h>

static NSString* const kRequestName = @"vbam-inplace-update.plist";
static const NSTimeInterval kExitTimeout = 600;    // the app may ask to save first
static const NSTimeInterval kRequestMaxAge = 300;  // a stale request is ignored

static NSString* RealHome(void) {
    const struct passwd* pw = getpwuid(getuid());
    if (pw && pw->pw_dir)
        return [NSString stringWithUTF8String:pw->pw_dir];
    return NSHomeDirectory();
}

static void Alert(NSString* title, NSString* message) {
    NSLog(@"vbam-updater: %@: %@", title, message);
    CFOptionFlags response;
    CFUserNotificationDisplayAlert(0, kCFUserNotificationCautionAlertLevel, NULL, NULL, NULL,
                                   (__bridge CFStringRef)title, (__bridge CFStringRef)message,
                                   NULL, NULL, NULL, &response);
}

static int Run(NSString* tool, NSArray<NSString*>* args) {
    NSTask* task = [[NSTask alloc] init];
    task.launchPath = tool;
    task.arguments = args;
    @try {
        [task launch];
    } @catch (NSException* e) {
        NSLog(@"vbam-updater: cannot run %@: %@", tool, e);
        return -1;
    }
    [task waitUntilExit];
    return task.terminationStatus;
}

// The application bundle this helper is in: <app>/Contents/Helpers/<helper>.app.
static NSString* ParentApp(void) {
    NSString* parent = [[[[[NSBundle mainBundle] bundlePath] stringByDeletingLastPathComponent]
        stringByDeletingLastPathComponent] stringByDeletingLastPathComponent];
    if (![[parent pathExtension] isEqualToString:@"app"])
        return nil;
    return [parent stringByStandardizingPath];
}

static NSString* BundleIdentifier(NSString* app) {
    NSDictionary* info = [NSDictionary
        dictionaryWithContentsOfFile:[app stringByAppendingPathComponent:@"Contents/Info.plist"]];
    id bid = info[@"CFBundleIdentifier"];
    return [bid isKindOfClass:[NSString class]] ? bid : nil;
}

// The newest valid request for `app`, removed from disk once read.
static NSDictionary* TakeRequest(NSString* app, NSString* bundleId) {
    NSString* home = RealHome();
    NSString* caches = [@"Library/Caches" stringByAppendingPathComponent:bundleId];
    NSArray<NSString*>* candidates = @[
        [[[[home stringByAppendingPathComponent:@"Library/Containers"]
            stringByAppendingPathComponent:bundleId] stringByAppendingPathComponent:@"Data"]
            stringByAppendingPathComponent:[caches stringByAppendingPathComponent:kRequestName]],
        [[home stringByAppendingPathComponent:caches] stringByAppendingPathComponent:kRequestName],
    ];

    NSDictionary* best = nil;
    for (NSString* path in candidates) {
        NSDictionary* req = [NSDictionary dictionaryWithContentsOfFile:path];
        if (req == nil)
            continue;
        [[NSFileManager defaultManager] removeItemAtPath:path error:nil];

        NSDate* created = req[@"created"];
        if (![req[@"pid"] isKindOfClass:[NSNumber class]] ||
            ![req[@"archive"] isKindOfClass:[NSString class]] ||
            ![req[@"app"] isKindOfClass:[NSString class]] || ![created isKindOfClass:[NSDate class]])
            continue;
        if (-[created timeIntervalSinceNow] > kRequestMaxAge)
            continue;
        if (![[req[@"app"] stringByStandardizingPath] isEqualToString:app])
            continue;
        if (best == nil || [created compare:best[@"created"]] == NSOrderedDescending)
            best = req;
    }
    return best;
}

static BOOL WaitForExit(pid_t pid) {
    NSDate* deadline = [NSDate dateWithTimeIntervalSinceNow:kExitTimeout];
    while (kill(pid, 0) == 0 || errno == EPERM) {
        if ([deadline timeIntervalSinceNow] < 0)
            return NO;
        usleep(100 * 1000);
    }
    return YES;
}

static BOOL IsMachO(NSString* path) {
    FILE* f = fopen([path fileSystemRepresentation], "rb");
    if (!f)
        return NO;
    uint32_t magic = 0;
    const size_t n = fread(&magic, sizeof(magic), 1, f);
    fclose(f);
    if (n != 1)
        return NO;
    switch (magic) {
        case MH_MAGIC: case MH_CIGAM: case MH_MAGIC_64: case MH_CIGAM_64:
        case FAT_MAGIC: case FAT_CIGAM: case FAT_MAGIC_64: case FAT_CIGAM_64:
            return YES;
    }
    return NO;
}

static void ClearQuarantine(NSString* path) {
    removexattr([path fileSystemRepresentation], "com.apple.quarantine", XATTR_NOFOLLOW);
}

// Gives every file in a MacOS directory, and every Mach-O file, execute
// permission where it has read permission, and clears the quarantine flag
// from the whole bundle.
static void FixBundle(NSString* app) {
    ClearQuarantine(app);
    NSDirectoryEnumerator* e = [[NSFileManager defaultManager] enumeratorAtPath:app];
    for (NSString* rel in e) {
        NSString* path = [app stringByAppendingPathComponent:rel];
        ClearQuarantine(path);

        struct stat st;
        if (lstat([path fileSystemRepresentation], &st) != 0 || !S_ISREG(st.st_mode))
            continue;
        const BOOL inMacOS =
            [[[rel stringByDeletingLastPathComponent] lastPathComponent] isEqualToString:@"MacOS"];
        if (!inMacOS && !IsMachO(path))
            continue;
        const mode_t r = st.st_mode & (S_IRUSR | S_IRGRP | S_IROTH);
        const mode_t mode = (st.st_mode & 07777) | S_IXUSR | (r >> 2);
        if (mode != (st.st_mode & 07777))
            chmod([path fileSystemRepresentation], mode);
    }
}

// Unpacks `archive` into a new directory in `dir` and returns the one .app in
// it with identifier `bundleId`; *staging is the directory to remove later.
static NSString* Unpack(NSString* archive, NSString* dir, NSString* bundleId, NSString** staging,
                        NSString** error) {
    NSString* tmpl = [dir stringByAppendingPathComponent:@".vbam-update-XXXXXX"];
    char* buf = strdup([tmpl fileSystemRepresentation]);
    if (!mkdtemp(buf)) {
        *error = [NSString stringWithFormat:@"%@ cannot be written to (%s).", dir, strerror(errno)];
        free(buf);
        return nil;
    }
    *staging = [[NSFileManager defaultManager] stringWithFileSystemRepresentation:buf
                                                                           length:strlen(buf)];
    free(buf);

    if (Run(@"/usr/bin/ditto", @[ @"-x", @"-k", archive, *staging ]) != 0) {
        *error = @"The update could not be unpacked.";
        return nil;
    }

    NSString* found = nil;
    for (NSString* name in [[NSFileManager defaultManager] contentsOfDirectoryAtPath:*staging
                                                                               error:nil]) {
        if (![[name pathExtension] isEqualToString:@"app"])
            continue;
        if (found) {
            *error = @"The update holds more than one application.";
            return nil;
        }
        found = [*staging stringByAppendingPathComponent:name];
    }
    if (!found) {
        *error = @"The update holds no application.";
        return nil;
    }
    if (![BundleIdentifier(found) isEqualToString:bundleId]) {
        *error = @"The update is not for this application.";
        return nil;
    }
    FixBundle(found);
    return found;
}

static BOOL Swap(NSString* fresh, NSString* app, NSString* staging, NSString** error) {
    if (renamex_np([fresh fileSystemRepresentation], [app fileSystemRepresentation], RENAME_SWAP) ==
        0)
        return YES;

    // Without RENAME_SWAP (not APFS/HFS+): two renames.
    NSString* old = [staging stringByAppendingPathComponent:@"old.app"];
    if (rename([app fileSystemRepresentation], [old fileSystemRepresentation]) != 0) {
        *error = [NSString stringWithFormat:@"%@ cannot be replaced (%s).", app, strerror(errno)];
        return NO;
    }
    if (rename([fresh fileSystemRepresentation], [app fileSystemRepresentation]) != 0) {
        *error = [NSString stringWithFormat:@"%@ cannot be replaced (%s).", app, strerror(errno)];
        rename([old fileSystemRepresentation], [app fileSystemRepresentation]);
        return NO;
    }
    return YES;
}

static void Relaunch(NSString* app) {
    LSRegisterURL((__bridge CFURLRef)[NSURL fileURLWithPath:app], true);
    Run(@"/usr/bin/open", @[ app ]);
}

// Not replaceable in place: unpack into ~/Downloads and show it there.
static void FallBack(NSString* archive, NSString* bundleId, NSString* app, NSString* why) {
    NSString* downloads = [RealHome() stringByAppendingPathComponent:@"Downloads"];
    NSString* staging = nil;
    NSString* error = nil;
    NSString* fresh = Unpack(archive, downloads, bundleId, &staging, &error);

    NSString* dest = nil;
    if (fresh) {
        NSString* base = [[fresh lastPathComponent] stringByDeletingPathExtension];
        dest = [downloads stringByAppendingPathComponent:[fresh lastPathComponent]];
        for (int i = 2; [[NSFileManager defaultManager] fileExistsAtPath:dest]; ++i)
            dest = [downloads
                stringByAppendingPathComponent:[NSString stringWithFormat:@"%@ %d.app", base, i]];
        if (![[NSFileManager defaultManager] moveItemAtPath:fresh toPath:dest error:nil])
            dest = nil;
    }
    if (staging)
        [[NSFileManager defaultManager] removeItemAtPath:staging error:nil];

    if (dest) {
        Alert(@"VisualBoyAdvance-M could not be updated in place",
              [NSString stringWithFormat:@"%@\n\nThe new version is in your Downloads folder: "
                                         @"move it over %@ to finish the update.",
                                         why, [app lastPathComponent]]);
        Run(@"/usr/bin/open", @[ @"-R", dest ]);
    } else {
        Alert(@"VisualBoyAdvance-M could not be updated",
              [NSString stringWithFormat:@"%@\n\n%@", why, error ?: @""]);
        Relaunch(app);
    }
}

int main(void) {
    @autoreleasepool {
        NSString* app = ParentApp();
        NSString* bundleId = app ? BundleIdentifier(app) : nil;
        if (!bundleId) {
            NSLog(@"vbam-updater: not inside an application bundle");
            return 1;
        }

        NSDictionary* req = TakeRequest(app, bundleId);
        if (!req) {
            NSLog(@"vbam-updater: no update request for %@", app);
            return 1;
        }
        NSString* archive = req[@"archive"];

        if (!WaitForExit((pid_t)[req[@"pid"] intValue])) {
            NSLog(@"vbam-updater: %@ did not exit, update abandoned", app);
            return 1;
        }

        if ([app rangeOfString:@"/AppTranslocation/"].location != NSNotFound) {
            FallBack(archive, bundleId, app,
                     @"macOS runs this copy from a temporary read-only location (it was opened "
                     @"where it was downloaded).");
            return 1;
        }

        NSString* staging = nil;
        NSString* error = nil;
        NSString* fresh =
            Unpack(archive, [app stringByDeletingLastPathComponent], bundleId, &staging, &error);
        BOOL ok = fresh && Swap(fresh, app, staging, &error);
        if (staging)
            [[NSFileManager defaultManager] removeItemAtPath:staging error:nil];

        if (!ok) {
            FallBack(archive, bundleId, app, error);
            return 1;
        }

        // The download's directory (wxSparkle's, in the app's temporary
        // directory) is cleaned up by wxSparkle on the next start.
        Relaunch(app);
        return 0;
    }
}
