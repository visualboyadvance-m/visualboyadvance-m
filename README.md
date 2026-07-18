<!-- START doctoc generated TOC please keep comment here to allow auto update -->
<!-- DON'T EDIT THIS SECTION, INSTEAD RE-RUN doctoc TO UPDATE -->

- [Visual Boy Advance - M](#visual-boy-advance---m)
  - [System Requirements](#system-requirements)
  - [Building](#building)
  - [Visual Studio Code Support](#visual-studio-code-support)
  - [Dependencies](#dependencies)
  - [CMake Options](#cmake-options)
  - [Contributing](#contributing)

<!-- END doctoc generated TOC please keep comment here to allow auto update -->

# Visual Boy Advance - M

Game Boy and Game Boy Advance Emulator

Our Discord server is [here](https://discord.gg/EpfxEuGMKH).

The forums are [here](https://board.visualboyadvance-m.org/).

Windows and Mac builds are in the [releases tab](https://github.com/visualboyadvance-m/visualboyadvance-m/releases).

Nightly builds for Windows and macOS are at [https://nightly.visualboyadvance-m.org/](https://nightly.visualboyadvance-m.org/).

<!--
[![Get it from flathub](https://dl.flathub.org/assets/badges/flathub-badge-en.svg)](https://flathub.org/apps/com.vba_m.visualboyadvance-m)
[![Get it from the Snap Store](https://snapcraft.io/static/images/badges/en/snap-store-black.svg)](https://snapcraft.io/visualboyadvance-m)
-->

**PLEASE TEST THE NIGHTLY OR MASTER WITH A FACTORY RESET BEFORE REPORTING
ISSUES**

Your distribution may have packages available as well, search for
`visualboyadvance-m` or `vbam`.

To build from source, see below.

If you are having issues, try resetting the config file first, go to `Help ->
Factory Reset`.

## System Requirements

Windows XP+, macOS or Linux.

Older and weaker systems may not be able to run the higher level xBRZ and
ScaleFX scalers.

For Windows XP install:

- [DirectX June 2010 Redist](https://www.microsoft.com/en-au/download/details.aspx?id=8109)
- [Visual C++ runtimes all-on-one pack for January 2021](https://cachemiss.com/files/Visual-C-Runtimes-All-in-One-Jan-2021.zip)

## Building

The basic formula to build vba-m is:

```bash
git clone https://github.com/visualboyadvance-m/visualboyadvance-m
cd visualboyadvance-m

./installdeps # On Linux, macOS or MSYS2

mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -G Ninja
ninja
```

If `./installdeps` does not work, you may need to install dependencies manually,
see below for the list. `./installdeps` also works on MSYS2.

For builds using automatically downloaded vcpkg packages for dependencies on
macOS or Linux, make sure PowerShell Core is installed, and use:

```bash
cmake .. -DVCPKG_TARGET_TRIPLET=**<SYSTEM>** -DCMAKE_BUILD_TYPE=Release -G Ninja
```

, where **SYSTEM** is one of:

- x64-linux,
- arm64-macos,
- x64-macos,

. On Windows, vcpkg is the default and that is handled automatically.

If you are in an MSYS2 shell, the MSYS2 package dependencies installed via
`./installdeps` are the default, unless you pass
`-DVCPKG_TARGET_TRIPLET=**<MSYS2-ENVIRONMENT>** which is one of:

- x64-mingw-static (for MINGW64, UCRT64 or CLANG64),
- x32-mingw-static (for Windows XP builds using a MINGW32 toolchain.)

You can download a MINGW32 toolchain
[here](https://cachemiss.com/files/winxp-mingw32.7z) and extract it to
`C:\msys64` or wherever your MSYS2 is installed. After which `C:\msys64\mingw32`
has to be in your `PATH` for the vcpkg build.

A `VCPKG_ROOT` is automatically created as a sibling of the project directory,
unless you set it to something else in your environment.

## Visual Studio Code Support

Make sure the
[C/C++](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools)
and [CMake
Tools](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cmake-tools)
extensions are installed.

Add the following to your `settings.json`:

```json
{
    "cmake.configureOnOpen": true,
    "cmake.preferredGenerators": [ "Ninja" ]
}
```
.

## Dependencies

You will need the following:

- C++ and C compiler and binutils,
- CMake.

And the following development libraries,

all platforms, required:

- zlib,
- Gettext and gettext-tools,
- SDL3 or SDL2,
- wxWidgets,
- bzip2,
- xz/lzma,

optional:

- FFmpeg,
- OpenAL-Soft,
- Lua,
- FAudio,
- Vulkan,

on Linux you will also need:

- glibc-devel,
- mesa,
- GTK3,
- XOrg,
- Wayland,

on macOS you will need MoltenVK for Vulkan support.

## CMake Options

The CMake code tries to guess reasonable defaults for options, but you can
override them, for example:

```shell
cmake .. -DCMAKE_BUILD_TYPE=Release -DENABLE_LINK=NO -G Ninja
```

. Here is the list:

| **CMake Option**                          | **What it Does**                                                     | **Defaults**          |
|-------------------------------------------|----------------------------------------------------------------------|-----------------------|
| `ENABLE_SDL`                              | Build the SDL port                                                   | OFF - Win, ON - rest  |
| `ENABLE_WX`                               | Build the wxWidgets port                                             | ON                    |
| `ENABLE_LINK`                             | Enable GBA/GB linking functionality                                  | ON                    |
| `ENABLE_LIBRETRO`                         | Build the libretro core                                              | ON                    |
| `BUILD_TESTING`                           | Build the tests                                                      | ON                    |
| `UPSTREAM_RELEASE`                        | Do some release tasks, like codesigning and making zips              | OFF                   |
| `TRANSLATIONS_ONLY`                       | Build only the translations.zip and nothing else                     | OFF                   |
| `ENABLE_LTO`                              | Compile with Link Time Optimization                                  | ON for release build  |
| `VCPKG_BINARY_PACKAGES`                   | Download/use vcpkg binary packages                                   | ON - Win, OFF - rest  |
| `VCPKG_TARGET_TRIPLET`                    | Enable vcpkg download/use of vcpkg binary packages for that triplet  | ON - Win, OFF - rest  |
| `VCPKG_SOURCE_PACKAGES`                   | Enable builds of vcpkg deps when binary packageas are not available  | ON - Win, OFF - rest  |
| `NO_VCPKG_UPDATES`                        | Do not update vcpkg ports from either binaries or source             | OFF                   |
| `VBAM_STATIC`                             | Try to link as many libraries statically as possible                 | ON - Win/MSYS2 or OFF |
| `DISABLE_MACOS_PACKAGE_MANAGERS`          | Do not use the detected brew/Nix/MacPorts on macOS                   | OFF                   |
| `ENABLE_SDL3`                             | Use SDL3 not SDL2                                                    | ON if detected        |
| `ENABLE_GENERIC_FILE_DIALOGS`             | Use generic file open/selection dialogs on macOS                     | OFF                   |
| `DISABLE_OPENGL`                          | Disable OpenGL support                                               | OFF                   |
| `ENABLE_DEBUGGER`                         | Enable the debugger                                                  | ON                    |
| `ENABLE_LUA`                              | Enable Lua scripting support for wxWidgets port                      | ON                    |
| `ENABLE_ASAN`                             | Use `-fsanitize=address` for GCC/Clang Debug builds                  | OFF                   |
| `ENABLE_BZ2`                              | Enable bzip2 .bz2 archive support                                    | ON                    |
| `ENABLE_LZMA`                             | Enable lzma .xz archive support                                      | ON                    |
| `ENABLE_LIRC`                             | Enable LIRC support                                                  | OFF                   |
| `ENABLE_FFMPEG`                           | Enable ffmpeg A/V recording                                          | ON                    |
| `ENABLE_ONLINEUPDATES`                    | Enable online update checks                                          | ON (release builds)   |
| `ENABLE_GBA_LOGGING`                      | Enable extended GBA logging                                          | ON                    |
| `ENABLE_OPENAL`                           | Enable openal-soft sound output for wxWidgets                        | ON                    |
| `ENABLE_XAUDIO2`                          | Enable xaudio2 sound output for wxWidgets (Windows only)             | ON                    |
| `ENABLE_FAUDIO`                           | Enable faudio sound output for wxWidgets                             | ON                    |
| `ENABLE_VULKAN`                           | Enable Vulkan video output                                           | ON                    |
| `ENABLE_DIRECT3D`                         | Enable Direct3D 9 support (Windows only)                             | ON - Win              |
| `ENABLE_DIRECT3D12`                       | Enable Direct3D 12 support (Windows only)                            | ON - Win              |
| `ENABLE_DIRECT3D11`                       | Enable Direct3D 11 support (Windows only)                            | ON - Win              |
| `ENABLE_WAYLAND_PROTOCOLS`                | Generate Wayland protocol client glue (HDR, viewporter)              | ON if detected        |

Note for distro packagers, we use the CMake module
[GNUInstallDirs](https://cmake.org/cmake/help/v2.8.12/cmake.html#module:GNUInstallDirs)
to configure installation directories.

On Linux or macOS, to use a different version of wxWidgets, set
`wxWidgets_CONFIG_EXECUTABLE` to the path to the `wx-config` script you want to
use.

## Contributing

See the [Developer Manual](/DEVELOPER-MANUAL.md).
