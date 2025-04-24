<!-- START doctoc generated TOC please keep comment here to allow auto update -->
<!-- DON'T EDIT THIS SECTION, INSTEAD RE-RUN doctoc TO UPDATE -->


- [Visual Boy Advance - M](#visual-boy-advance---m)
  - [System Requirements](#system-requirements)
  - [Building](#building)
  - [Building a Libretro core](#building-a-libretro-core)
  - [Visual Studio Support](#visual-studio-support)
  - [Visual Studio Code Support](#visual-studio-code-support)
  - [Dependencies](#dependencies)
  - [Cross compiling for 32 bit on a 64 bit host](#cross-compiling-for-32-bit-on-a-64-bit-host)
  - [Cross Compiling for Win32](#cross-compiling-for-win32)
  - [CMake Options](#cmake-options)
  - [MSys2 Notes](#msys2-notes)
  - [Debug Messages](#debug-messages)
  - [Reporting Crash Bugs](#reporting-crash-bugs)
  - [Contributing](#contributing)

<!-- END doctoc generated TOC please keep comment here to allow auto update -->

Our bridged Discord server is [Here](https://discord.gg/EpfxEuGMKH).

We are also on *`#vba-m`* on [Libera IRC](https://libera.chat/) which has a [Web
Chat](https://web.libera.chat/).

[![Get it from flathub](https://dl.flathub.org/assets/badges/flathub-badge-en.svg)](https://flathub.org/apps/com.vba_m.visualboyadvance-m)
[![Get it from the Snap Store](https://snapcraft.io/static/images/badges/en/snap-store-black.svg)](https://snapcraft.io/visualboyadvance-m)

***Want to know where you can install visualboyadvance-m in your linux distribution?***

[![Packaging status](https://repology.org/badge/vertical-allrepos/visualboyadvance-m.svg)](https://repology.org/project/visualboyadvance-m/versions)

# Visual Boy Advance - M

Game Boy and Game Boy Advance Emulator

The forums are [here](https://board.visualboyadvance-m.org/).

Windows and Mac builds are in the [releases tab](https://github.com/visualboyadvance-m/visualboyadvance-m/releases).

Nightly builds for Windows and macOS are at [https://nightly.visualboyadvance-m.org/](https://nightly.visualboyadvance-m.org/).

**PLEASE TEST THE NIGHTLY OR MASTER WITH A FACTORY RESET BEFORE REPORTING
ISSUES**

Your distribution may have packages available as well, search for
`visualboyadvance-m` or `vbam`.

It is also generally very easy to build from source, see below.

If you are using the windows binary release and you need localization, unzip
the `translations.zip` to the same directory as the executable.

If you are having issues, try resetting the config file first, go to `Help ->
Factory Reset`.

## System Requirements

Windows XP, Vista, 7, 8.1 or 10/11, Linux distros or macOS.

2Ghz x86 (or x86-64) Intel Core 2 or AMD Athlon processor with SSE, Snapdragon 835 
or newer CPU compatible with Arm for Windows.

- Just a guideline, lower clock speeds and Celeron processors may be able to run at full
speed on lower settings, and Linux based ARM Operating systems have wider CPU support.

DirectX June 2010 Redist
[Full](https://www.microsoft.com/en-au/download/details.aspx?id=8109) /
[Websetup](https://www.microsoft.com/en-au/download/details.aspx?id=35) for
Xaudio (Remember to uncheck Bing on the websetup.)

## Building

The basic formula to build vba-m is:

```bash
cd ~ && mkdir src && cd src
git clone https://github.com/visualboyadvance-m/visualboyadvance-m.git
cd visualboyadvance-m

./installdeps # On Linux or macOS

mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -G Ninja
ninja
```

`./installdeps` is supported on MSYS2, Linux (Debian/Ubuntu, Fedora, Arch,
Solus, OpenSUSE, Gentoo and RHEL/CentOS) and Mac OS X (homebrew, MacPorts or
Fink.)

## Building a Libretro core

Clone this repo and then,

```bash
cd src/libretro
make -j`nproc`
```

Copy `vbam_libretro.so` to your RetroArch cores directory.

## Visual Studio Support

For visual studio, dependency management is handled automatically with vcpkg,
From the Visual Studio GUI, just clone the repository with git and build with
the cmake configurations provided.

If the GUI does not detect cmake, go to `File -> Open -> CMake` and open the
`CMakeLists.txt`.

If you are using 2017, make sure you have all the latest updates, some issues
with cmake projects in the GUI have been fixed.

You can also build from the developer command prompt or powershell with the
environment loaded.

Using your own user-wide installation of vcpkg is supported, just make sure the
environment variable `VCPKG_ROOT` is set.

To build in the Visual Studio `x64 Native Tools Command Prompt`, use something
like this:

```
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -G Ninja
ninja
```
.

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

If your OS is not supported, you will need the following:

- C++ compiler and binutils
- [make](https://en.wikipedia.org/wiki/Make_(software))
- [CMake](https://cmake.org/)
- [git](https://git-scm.com/)
- [nasm](https://www.nasm.us/) (optional, for 32 bit builds)

And the following development libraries:

- [zlib](https://zlib.net/) (required)
- [mesa](https://mesa3d.org/) (if using X11 or any OpenGL otherwise)
- [ffmpeg](https://ffmpeg.org/) (optional, at least version `4.0.4`, for game recording)
- [gettext](https://www.gnu.org/software/gettext/) and gettext-tools
- [SDL2](https://www.libsdl.org/) (required)
- [openal-soft](https://kcat.strangesoft.net/openal.html) (required, a sound interface)
- [wxWidgets](https://wxwidgets.org/) (required for GUI, 2.8 and non-stl builds are no longer supported)

On Linux and similar, you also need the version of GTK your wxWidgets is linked
to (usually 2 or 3) and the xorg development libraries.

Support for more OSes/distributions for `./installdeps` is planned.

## Cross compiling for 32 bit on a 64 bit host

`./installdeps m32` will set things up to build a 32 bit binary.

This is supported on Fedora, Arch, Solus and MSYS2.

## Cross Compiling for Win32

`./installdeps` takes one optional parameter for cross-compiling target, which
may be `win32` which is an alias for `mingw-w64-i686` to target 32 bit Windows,
or `mingw-w64-x86_64` for 64 bit Windows targets.

The target is implicit on MSYS2 depending on which MINGW shell you started (the
value of `$MSYSTEM`.)

On Debian/Ubuntu this uses the MXE apt repository and works quite well.

## CMake Options

The CMake code tries to guess reasonable defaults for options, but you can
override them, for example:

```shell
cmake .. -DCMAKE_BUILD_TYPE=Release -DENABLE_LINK=NO -G Ninja
```
. Here is the complete list:

| **CMake Option**      | **What it Does**                                                     | **Defaults**          |
|-----------------------|----------------------------------------------------------------------|-----------------------|
| `ENABLE_SDL`            | Build the SDL port                                                   | OFF                   |
| `ENABLE_WX`             | Build the wxWidgets port                                             | ON                    |
| `ENABLE_DEBUGGER`       | Enable the debugger                                                  | ON                    |
| `ENABLE_ASM_CORE`       | Enable x86 ASM CPU cores (**BUGGY AND DANGEROUS**)                   | OFF                   |
| `ENABLE_ASM`            | Enable the following two ASM options                                 | ON for 32 bit builds  |
| `ENABLE_ASM_SCALERS`    | Enable x86 ASM graphic filters                                       | ON for 32 bit builds  |
| `ENABLE_MMX`            | Enable MMX                                                           | ON for 32 bit builds  |
| `ENABLE_LINK`           | Enable GBA linking functionality                                     | AUTO                  |
| `ENABLE_LIRC`           | Enable LIRC support                                                  | OFF                   |
| `ENABLE_FFMPEG`         | Enable ffmpeg A/V recording                                          | AUTO                  |
| `ENABLE_ONLINEUPDATES`  | Enable online update checks                                          | ON                    |
| `ENABLE_LTO`            | Compile with Link Time Optimization (gcc and clang only)             | ON for release build  |
| `ENABLE_GBA_LOGGING`    | Enable extended GBA logging                                          | ON                    |
| `ENABLE_XAUDIO2`        | Enable xaudio2 sound output for wxWidgets (Windows only)             | ON                    |
| `ENABLE_FAUDIO`         | Enable faudio sound output for wxWidgets,                            | ON, not 32 bit Win    |
| `ENABLE_ASAN`           | Enable libasan sanitizers (by default address, only in debug mode)   | OFF                   |
| `UPSTREAM_RELEASE`      | Do some release tasks, like codesigning, making zip and gpg sigs.    | OFF                   |
| `BUILD_TESTING`         | Build the tests and enable ctest support.                            | ON                    |
| `VBAM_STATIC`           | Try link all libs statically (the following are set to ON if ON)     | OFF                   |
| `SDL2_STATIC`           | Try to link static SDL2 libraries                                    | OFF                   |
| `FFMPEG_STATIC`         | Try to link static ffmpeg libraries                                  | OFF                   |
| `OPENAL_STATIC`         | Try to link static OpenAL libraries                                  | OFF                   |
| `TRANSLATIONS_ONLY`     | Build only the translations.zip and nothing else                     | OFF                   |

Note for distro packagers, we use the CMake module
[GNUInstallDirs](https://cmake.org/cmake/help/v2.8.12/cmake.html#module:GNUInstallDirs)
to configure installation directories.

On Unix to use a different version of wxWidgets, set
`wxWidgets_CONFIG_EXECUTABLE` to the path to the `wx-config` script you want to
use.

## Reporting Crash Bugs

If the emulator crashes and you wish to report the bug, a backtrace made with
debug symbols would be immensely helpful.

To generate one (on Linux and MSYS2) first build in debug mode by invoking
`cmake` as:

```shell
cmake .. -DCMAKE_BUILD_TYPE=Debug
```

After you've reproduced the crash, you need the core dump file, you may need to
do something such as:

```shell
ulimit -c unlimited
```
, in your shell to enable core files.

[This
post](https://ask.fedoraproject.org/en/question/98776/where-is-core-dump-located/?answer=98779#post-id-98779)
explains how to retrieve core dump on some distributions, when they are managed
by systemd.

Once you have the core file, open it with `gdb`, for example:

```shell
gdb -c core ./visualboyadvance-m
```
. In the `gdb` shell, to start the process and print the backtrace, type:

```
run
bt
```
. This may be a bit of a hassle, but it helps us out immensely.

## Contributing

See the [Developer Manual](/DEVELOPER-MANUAL.md).
