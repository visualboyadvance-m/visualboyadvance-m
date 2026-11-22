# wxSparkle

wxSparkle is a port of [WinSparkle](https://winsparkle.org) to wxWidgets: a
software update library for Windows, macOS and Linux. It keeps
WinSparkle's C API (`win_sparkle_*`, `winsparkle.h`), appcast format and UI,
but all platform-specific parts were replaced:

| WinSparkle (Windows only)              | wxSparkle                                                        |
|----------------------------------------|------------------------------------------------------------------|
| WinInet downloads                      | **libcurl**, verified against the **embedded CA bundle** (`cert.pem`) |
| UI in its own thread with private wxApp| UI in the host wxApp's main thread (required by macOS/GTK); the private-thread mode is kept for non-wx Windows hosts |
| Registry                               | Registry on Windows, INI file elsewhere                          |
| VERSIONINFO / custom resources         | Same on Windows; `Info.plist` (Sparkle keys) on macOS; API/wxApp elsewhere |
| WinCrypt SHA1 + legacy OpenSSL DSA API | OpenSSL EVP API (DSA is optional), bundled ed25519 for EdDSA     |
| Translations as Win32 resources        | `.mo` files embedded via generated headers, private catalog      |
| `ShellExecuteEx` installer launch      | Same on Windows; executables are run, other files are opened with the default application elsewhere |
| Win32 threads, rpc, SHFileOperation    | Native Win32 primitives on Windows, C++ std elsewhere; `std::filesystem` |


## Embedded CA certificates

HTTPS certificates are verified **only** against the certificates in
`cert.pem` in the root of the source tree. At build time,
`cmake/bin2c.cmake` extracts the PEM certificate blocks from it (dropping the
human-readable dumps) and writes them, bin2c-style, into
`build/generated/wxsparkle_cacert.h`:

```c
static const unsigned char wxsparkle_cacert_pem[] = { 0x2d,0x2d,0x2d, ... , 0x00 };
static const size_t wxsparkle_cacert_pem_size = 192502;
```

which is passed to libcurl with `CURLOPT_CAINFO_BLOB` (libcurl ≥ 7.77 is
required). Neither the system store nor libcurl's built-in CA path is used.
To update the certificates, replace `cert.pem` (e.g. with
<https://curl.se/ca/cacert.pem>) and rebuild. To use a different file, pass
`-DWXSPARKLE_CA_BUNDLE=/path/to/bundle.pem`. Setting
`-DWXSPARKLE_TRUST_NATIVE_CA=ON` additionally trusts the OS store, which is
useful for corporate TLS-intercepting proxies.

Other transfer details: HTTP/2 when available, transparent compression,
redirects are followed (but never from HTTPS to HTTP), only `http`/`https`
protocols are allowed, and proxies are taken from the usual
`http_proxy`/`https_proxy`/`no_proxy` environment variables.


## Building

Requirements: CMake ≥ 3.16, a C++17 compiler, wxWidgets ≥ 3.2 (with
`webview` for HTML release notes, optional), libcurl ≥ 7.77, expat, and
optionally OpenSSL (legacy DSA signatures) and gettext's `msgfmt`
(translations).

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
WXSPARKLE_TEST_NETWORK=1 ./build/wxsparkle-tests   # optional
```

On Windows, the dependencies are most easily obtained from vcpkg:

```bat
vcpkg install wxwidgets curl expat openssl
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release
```

| CMake option                    | Default | Meaning |
|---------------------------------|---------|---------|
| `BUILD_SHARED_LIBS`             | OFF     | Build a shared library / DLL instead of a static one |
| `WXSPARKLE_CA_BUNDLE`           | `cert.pem` | CA bundle embedded into the library |
| `WXSPARKLE_TRUST_NATIVE_CA`     | OFF     | Also trust the OS certificate store |
| `WXSPARKLE_USE_WEBVIEW`         | ON      | Show release notes with wxWebView (falls back to wxHtmlWindow) |
| `WXSPARKLE_WITH_DSA`            | ON      | Support legacy DSA signatures (needs OpenSSL) |
| `WXSPARKLE_TRANSLATIONS`        | ON      | Embed translations (needs `msgfmt`) |
| `WXSPARKLE_CURL_PKGCONFIG_STATIC` | ON    | Use `pkg-config --static` for a static libcurl's dependencies |
| `WXSPARKLE_BUILD_EXAMPLE` / `_TOOL` / `_TESTS` | ON | Build the example app, `winsparkle-tool`, unit tests |

When linking wxSparkle into a wxWidgets application, it must use the *same*
wxWidgets build as the application. The default static library guarantees
that. A shared build with wxWidgets linked statically into the DLL gives a
self-contained `wxsparkle.dll` for non-wxWidgets Windows applications, like
WinSparkle.


## Using it

```cmake
add_subdirectory(wxSparkle)             # or: find_package(wxSparkle)
target_link_libraries(myapp PRIVATE wxSparkle::wxsparkle)
```

```cpp
#include <winsparkle.h>

bool MyApp::OnInit()
{
    // ... create and show the main window ...

    win_sparkle_set_appcast_url("https://example.com/appcast.xml");
    win_sparkle_set_eddsa_public_key("base64 public key from winsparkle-tool");
    win_sparkle_set_app_details(L"My Company", L"My App", L"1.0");
    win_sparkle_init();
    return true;
}

int MyApp::OnExit()
{
    win_sparkle_cleanup();
    return wxApp::OnExit();
}

// "Check for Updates..." menu item:
void MyFrame::OnCheckForUpdates(wxCommandEvent&) { win_sparkle_check_update_with_ui(); }
```

See `examples/wx/example.cpp` for a complete application, and `winsparkle.h`
for the full API documentation.

**Threading:** on macOS and Linux the UI requires a GUI `wxApp` and runs in its
main thread, so call `win_sparkle_init()` after the application object exists
(e.g. in `OnInit()`). Callbacks may be invoked from background threads, just
like in WinSparkle. Use `wxTheApp->CallAfter()` in them to touch the GUI.

### Application metadata

If not set using the API, the metadata is taken from:

| Value            | Windows                         | macOS (`Info.plist`)                       | Linux / other |
|------------------|---------------------------------|--------------------------------------------|---------------|
| Appcast URL      | `FeedURL` `APPCAST` resource    | `SUFeedURL`                                | API only      |
| EdDSA public key | `EdDSAPub` `EDDSA` resource     | `SUPublicEDKey`                            | API only      |
| DSA public key   | `DSAPub` `DSAPEM` resource      | `SUPublicDSAKey` / `SUPublicDSAKeyFile`    | API only      |
| App name         | VERSIONINFO `ProductName`       | `CFBundleDisplayName` / `CFBundleName`     | `wxApp::GetAppDisplayName()` |
| App version      | VERSIONINFO `ProductVersion`    | `CFBundleShortVersionString`               | API only (`win_sparkle_set_app_details`) |
| Build version    | `win_sparkle_set_app_build_version` | `CFBundleVersion` (unless the version was set via the API) | API only |
| Company name     | VERSIONINFO `CompanyName`       | `wxApp::GetVendorDisplayName()` (optional) | same as macOS |

### Settings storage

Windows uses the registry, as WinSparkle does (`HKCU\Software\<Company>\<App>\WinSparkle`).
Elsewhere, the same path is mapped to an INI file:
`~/Library/Application Support/<Company>/<App>/WinSparkle.ini` on macOS and
`$XDG_CONFIG_HOME/<Company>/<App>/WinSparkle.ini` (default `~/.config`) on
Linux. `win_sparkle_set_registry_path()` changes the relative path, or takes an
absolute file path. `win_sparkle_set_config_methods()` works as before.

### Appcast

The appcast format is unchanged. The `sparkle:os` enclosure attribute accepts
`windows`, `macos` and `linux`, each optionally with an architecture
(`windows-x64`, `windows-arm64`, `windows-x86`, `macos-arm64`, `macos-x64`,
`linux-x64`, `linux-arm64`, ...). Architecture-specific enclosures are
preferred, so one feed can serve all platforms. `sparkle:minimumSystemVersion`
is the Windows version on Windows and the macOS version on macOS; it is ignored
on Linux.

### Installing updates

After the download is verified, the update file is launched:

* **Windows:** `ShellExecuteEx` with `sparkle:installerArguments`, as in WinSparkle.
* **macOS / Linux:** files without an extension and `.AppImage`, `.run`,
  `.sh` and `.bin` files are made executable and run with the installer
  arguments, in their own process group. Anything else (`.dmg`, `.pkg`,
  `.zip`, `.deb`, ...) is opened with the default application.

As before, `win_sparkle_set_user_run_installer_callback()` lets the
application handle the file itself. The application is then asked to shut
down, using the can-shutdown/shutdown-request callbacks.

### Phones and tablets

On Android (`WXSPARKLE_FILL_SCREEN`) the dialogs fill the application's part
of the screen, with text and buttons fitted to its width. Qt for Android
reports a screen area that includes the activity's action bar, which would
push the dialogs' buttons off the bottom of the screen, so the application
should report the area it really has with
`win_sparkle_set_screen_area_callback()`.

### Signing updates

`winsparkle-tool` now builds on all platforms:

```sh
winsparkle-tool generate-key -f private.key
winsparkle-tool sign -v -f private.key MyApp-2.0.dmg
winsparkle-tool verify -p PUBKEY -s SIGNATURE MyApp-2.0.dmg
```

### Translations

All WinSparkle translations from `translations/*.po` are compiled and
embedded into the library, and loaded into a private message catalog. This
works whether or not the host application uses `wxTranslations`. The language
is chosen from `win_sparkle_set_lang()` / `win_sparkle_set_langid()`, then
the host's current `wxUILocale`, then the user's preferred UI languages.


## Known limitations

* No in-place replacement of a running macOS `.app` bundle (as Sparkle does):
  the downloaded `.dmg`/`.pkg`/`.zip` is opened for the user to install. Use
  the run-installer callback for custom installation logic.
* On macOS, right-to-left translations are shown in the correct language, but
  the dialog layout isn't mirrored (wxOSX doesn't support per-window layout
  direction).
* System proxy settings (Windows Internet Options, macOS network settings)
  aren't read; libcurl uses the `*_proxy` environment variables.


## License

MIT, like WinSparkle; see `COPYING`. Bundled code: ed25519 (zlib license,
`3rdparty/ed25519/license.txt`) and argparse (MIT, `3rdparty/argparse/LICENSE`).
