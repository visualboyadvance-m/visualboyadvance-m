// In-place updates on macOS, see inplace-update.h: the application's side,
// which hands the update to Contents/Helpers/vbam-updater.app.
//
// Launch Services drops the arguments of an application started from a
// sandboxed one, so the request goes in a property list instead, at
// Library/Caches/<bundle id>/vbam-inplace-update.plist under this process's
// home: the sandbox container's in a sandboxed build, which the unsandboxed
// helper finds by its parent's bundle identifier (vbam-updater.m).

#include "wx/autoupdater/wxsparkle/inplace-update.h"

#include <unistd.h>

#include <wx/string.h>

#import <AppKit/AppKit.h>

namespace {

constexpr double kLaunchTimeoutSeconds = 15.0;

NSString* RequestPath(NSString* bundleId) {
    return [[[NSHomeDirectory() stringByAppendingPathComponent:@"Library/Caches"]
        stringByAppendingPathComponent:bundleId]
        stringByAppendingPathComponent:@"vbam-inplace-update.plist"];
}

// Starts the helper and waits until it has taken the request at `request`
// (it removes the file as it reads it). Launch Services' completion handler
// is no use for that: it comes only once a background-only application that
// does not check in with AppKit has exited, and the helper waits for this
// process to exit first.
bool LaunchHelper(NSURL* helper, NSString* request) {
    __block bool failed = false;

    if (@available(macOS 10.15, *)) {
        NSWorkspaceOpenConfiguration* cfg = [NSWorkspaceOpenConfiguration configuration];
        cfg.activates = NO;
        cfg.addsToRecentItems = NO;
        cfg.createsNewApplicationInstance = YES;
        [[NSWorkspace sharedWorkspace]
            openApplicationAtURL:helper
                   configuration:cfg
               completionHandler:^(NSRunningApplication*, NSError* error) {
                 if (error) {
                     NSLog(@"vbam: cannot start the updater: %@", error);
                     failed = true;
                 }
               }];
    } else {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        NSError* error = nil;
        if (![[NSWorkspace sharedWorkspace]
                launchApplicationAtURL:helper
                               options:NSWorkspaceLaunchWithoutActivation |
                                       NSWorkspaceLaunchNewInstance |
                                       NSWorkspaceLaunchWithoutAddingToRecents
                         configuration:@{}
                                 error:&error]) {
            NSLog(@"vbam: cannot start the updater: %@", error);
            return false;
        }
#pragma clang diagnostic pop
    }

    NSFileManager* fm = [NSFileManager defaultManager];
    NSDate* deadline = [NSDate dateWithTimeIntervalSinceNow:kLaunchTimeoutSeconds];
    while ([deadline timeIntervalSinceNow] > 0) {
        if (![fm fileExistsAtPath:request])
            return true;
        if (failed)
            break;
        usleep(50 * 1000);
    }

    // Withdrawn, unless the helper took it in the meantime.
    return ![fm removeItemAtPath:request error:nil];
}

}  // namespace

int InPlaceUpdateRunInstaller(const wchar_t* file) {
    @autoreleasepool {
        NSString* archive = [NSString stringWithUTF8String:wxString(file).utf8_str()];
        if (archive == nil || [[archive pathExtension] caseInsensitiveCompare:@"zip"] != NSOrderedSame)
            return 0;

        NSBundle* main = [NSBundle mainBundle];
        NSURL* bundle = [main bundleURL];
        NSString* bundleId = [main bundleIdentifier];
        if (bundleId == nil || ![[bundle pathExtension] isEqualToString:@"app"])
            return 0;

        NSURL* helper = [bundle URLByAppendingPathComponent:@"Contents/Helpers/vbam-updater.app"];
        if (![[NSFileManager defaultManager] fileExistsAtPath:[helper path]])
            return 0;

        NSString* path = RequestPath(bundleId);
        [[NSFileManager defaultManager] createDirectoryAtPath:[path stringByDeletingLastPathComponent]
                                  withIntermediateDirectories:YES
                                                   attributes:nil
                                                        error:nil];
        NSDictionary* request = @{
            @"pid" : @(getpid()),
            @"archive" : archive,
            @"app" : [[bundle path] stringByStandardizingPath],
            @"bundleIdentifier" : bundleId,
            @"created" : [NSDate date],
        };
        if (![request writeToFile:path atomically:YES])
            return 0;

        if (!LaunchHelper(helper, path))
            return 0;

        // The helper waits for this process to exit: wxSparkle closes the
        // application when this returns 1.
        return 1;
    }
}

void InPlaceUpdateStartup() {}
