#ifndef VBAM_WX_AUTOUPDATER_WXSPARKLE_INPLACE_UPDATE_H_
#define VBAM_WX_AUTOUPDATER_WXSPARKLE_INPLACE_UPDATE_H_

// In-place updates (Windows and macOS, VBAM_INPLACE_UPDATE): instead of
// opening the downloaded zip for the user to unpack, the update replaces the
// running application where it is installed and starts the new one.
//
// Windows: the running .exe is renamed to <exe>.old (a running image may be
// renamed, not overwritten), the one from the zip takes its name and is
// started; it waits for this process to exit before it reads the
// configuration, and removes <exe>.old (InPlaceUpdateStartup()).
//
// macOS: the sandboxed application cannot write its own bundle, so the
// unsandboxed helper Contents/Helpers/vbam-updater.app (started through
// Launch Services, which does not hand it the sandbox -- nor the arguments,
// hence the request file, see inplace-update-mac.mm) waits for this process
// to exit, unpacks the zip with ditto, makes the bundle's executables
// executable, clears the quarantine flag, swaps the bundles and relaunches.

// wxSparkle's run-installer callback: 1 when the update is being installed in
// place (wxSparkle then closes the application), 0 to fall back to opening
// the downloaded file.
int InPlaceUpdateRunInstaller(const wchar_t* file);

// Called first thing in OnInit(): on Windows, waits for the instance that
// installed this update to exit and removes the executable it ran from.
void InPlaceUpdateStartup();

#endif  // VBAM_WX_AUTOUPDATER_WXSPARKLE_INPLACE_UPDATE_H_
