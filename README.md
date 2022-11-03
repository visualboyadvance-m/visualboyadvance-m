<!-- START doctoc generated TOC please keep comment here to allow auto update -->
<!-- DON'T EDIT THIS SECTION, INSTEAD RE-RUN doctoc TO UPDATE -->


- [Visual Boy Advance - M](#visual-boy-advance---m)
  - [Building](#building)
  - [Building a Libretro core](#building-a-libretro-core)
  - [Visual Studio Support](#visual-studio-support)
  - [Visual Studio Code Support](#visual-studio-code-support)
    - [Optional: clangd](#optional-clangd)
  - [Dependencies](#dependencies)
  - [Cross compiling for 32 bit on a 64 bit host](#cross-compiling-for-32-bit-on-a-64-bit-host)
  - [Cross Compiling for Win32](#cross-compiling-for-win32)
  - [CMake Options](#cmake-options)
  - [MSys2 Notes](#msys2-notes)
  - [Debug Messages](#debug-messages)
  - [Reporting Crash Bugs](#reporting-crash-bugs)
  - [Contributing](#contributing)

<!-- END doctoc generated TOC please keep comment here to allow auto update -->

[![Join the chat at https://gitter.im/visualboyadvance-m/Lobby](https://badges.gitter.im/visualboyadvance-m/Lobby.svg)](https://gitter.im/visualboyadvance-m/Lobby)

Our bridged Discord server is [Here](https://discord.gg/EpfxEuGMKH).

We are also on *`#vba-m`* on [Libera IRC](https://libera.chat/) which has a [Web
Chat](https://web.libera.chat/).

[![Get it from the Snap Store](https://snapcraft.io/static/images/badges/en/snap-store-black.svg)](https://snapcraft.io/visualboyadvance-m)

# Visual Boy Advance - M

Game Boy and Game Boy Advance Emulator

The forums are [here](https://board.vba-m.com/).

Windows and Mac builds are in the [releases tab](https://github.com/visualboyadvance-m/visualboyadvance-m/releases).

Nightly builds for Windows and macOS are at [https://nightly.vba-m.com/](https://nightly.vba-m.com/).

**PLESE TEST THE NIGHTLY OR MASTER WITH A FACTORY RESET BEFORE REPORTING
ISSUES**

Your distribution may have packages available as well, search for
`visualboyadvance-m` or `vbam`.

It is also generally very easy to build from source, see below.

If you are using the windows binary release and you need localization, unzip
the `translations.zip` to the same directory as the executable.

If you are having issues, try resetting the config file first, go to `Help ->
Factory Reset`.

## Building

The basic formula to build vba-m is:

```shell
cd ~ && mkdir src && cd src
git clone https://github.com/visualboyadvance-m/visualboyadvance-m.git
cd visualboyadvance-m
./installdeps

# ./installdeps will give you build instructions, which will be similar to:

mkdir build && cd build
cmake .. -G Ninja
ninja
```

`./installdeps` is supported on MSys2, Linux (Debian/Ubuntu, Fedora, Arch,
Solus, OpenSUSE, Gentoo and RHEL/CentOS) and Mac OS X (homebrew, macports or
fink.)

## Building a Libretro core

Clone this repo and then,

```bash
cd src/libretro
make -j`nproc`
```

Copy vbam_libretro.so to your RetroArch cores directory.

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

To build in the visual studio command prompt, use something like this:

```
mkdir build
cd build
cmake .. -DVCPKG_TARGET_TRIPLET=x64-windows-static -DCMAKE_BUILD_TYPE=Debug -G Ninja
ninja
```

## Visual Studio Code Support

On most platforms, Visual Studio Code should work as-is, as long as the
[CMake Tools extension](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cmake-tools)
is installed.

There is a recommended configuration in the `vscode/settings.json` file. To use
it, copy the file to a `.vscode/` folder.

By default, this will publish builds in the `build-vscode/` directory. In the
`vscode/settings.json` file, there is an alternate configuration for the
`"cmake.buildDirectory"` option that will use different build directories for
different toolchains and build configurations.

### Optional: clangd

The [clangd extension](https://marketplace.visualstudio.com/items?itemName=llvm-vs-code-extensions.vscode-clangd)
uses clangd to provide powerful code completion, errors and warnings and
references on click in VS Code.

With the recommended configuration, the build configuration will generate a
`compile_commands.json` file that can be used with clangd. After configuration,
you can copy that file to the root directory with a command similar to this one:

```shell
cp build/build-vscode/compile_commands.json .
```

Then, select "clangd: Restart language server" from the command palette to get
completion in the IDE.

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
- [gettext](https://www.gnu.org/software/gettext/) and gettext-tools (optional, with ENABLE_NLS)
- [SDL2](https://www.libsdl.org/) (required)
- [SFML](https://www.sfml-dev.org/) (optional, for link)
- [OpenAL](https://www.openal.org/) or [openal-soft](https://kcat.strangesoft.net/openal.html) (optional, a sound interface)
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
or `mingw-gw64-x86_64` for 64 bit Windows targets.

The target is implicit on MSys2 depending on which MINGW shell you started (the
value of `$MSYSTEM`.)

On Debian/Ubuntu this uses the MXE apt repository and works quite well.

On Fedora it can build using the Fedora MinGW packages, albeit with wx 2.8, no
OpenGL support, and no Link support for lack of SFML.

On Arch it currently doesn't work at all because the AUR stuff is completely
broken, I will at some point redo the arch stuff to use MXE as well.

## CMake Options

The CMake code tries to guess reasonable defaults for options, but you can
override them, for example:

```shell
cmake .. -DENABLE_LINK=NO -G Ninja
```

Of particular interest is making **Release** or **Debug** builds, the default
mode is **Release**, to make a **Debug** build use something like:

```shell
cmake .. -DCMAKE_BUILD_TYPE=Debug -G Ninja
```

Here is the complete list:

| **CMake Option**      | **What it Does**                                                     | **Defaults**          |
|-----------------------|----------------------------------------------------------------------|-----------------------|
| ENABLE_SDL            | Build the SDL port                                                   | OFF                   |
| ENABLE_WX             | Build the wxWidgets port                                             | ON                    |
| ENABLE_DEBUGGER       | Enable the debugger                                                  | ON                    |
| ENABLE_NLS            | Enable translations                                                  | ON                    |
| ENABLE_ASM_CORE       | Enable x86 ASM CPU cores (**BUGGY AND DANGEROUS**)                   | OFF                   |
| ENABLE_ASM            | Enable the following two ASM options                                 | ON for 32 bit builds  |
| ENABLE_ASM_SCALERS    | Enable x86 ASM graphic filters                                       | ON for 32 bit builds  |
| ENABLE_MMX            | Enable MMX                                                           | ON for 32 bit builds  |
| ENABLE_LINK           | Enable GBA linking functionality (requires SFML)                     | AUTO                  |
| ENABLE_LIRC           | Enable LIRC support                                                  | OFF                   |
| ENABLE_FFMPEG         | Enable ffmpeg A/V recording                                          | AUTO                  |
| ENABLE_ONLINEUPDATES  | Enable online update checks                                          | ON                    |
| ENABLE_LTO            | Compile with Link Time Optimization (gcc and clang only)             | ON for release build  |
| ENABLE_GBA_LOGGING    | Enable extended GBA logging                                          | ON                    |
| ENABLE_DIRECT3D       | Direct3D rendering for wxWidgets (Windows, **NOT IMPLEMENTED!!!**)   | ON                    |
| ENABLE_XAUDIO2        | Enable xaudio2 sound output for wxWidgets (Windows only)             | ON                    |
| ENABLE_OPENAL         | Enable OpenAL for the wxWidgets port                                 | AUTO                  |
| ENABLE_SSP            | Enable gcc stack protector support (gcc only)                        | OFF                   |
| ENABLE_ASAN           | Enable libasan sanitizers (by default address, only in debug mode)   | OFF                   |
| UPSTREAM_RELEASE      | Do some release tasks, like codesigning, making zip and gpg sigs.    | OFF                   |
| BUILD_TESTING         | Build the tests and enable ctest support.                            | ON                    |
| VBAM_STATIC           | Try link all libs statically (the following are set to ON if ON)     | OFF                   |
| SDL2_STATIC           | Try to link static SDL2 libraries                                    | OFF                   |
| SFML_STATIC_LIBRARIES | Try to link static SFML libraries                                    | OFF                   |
| FFMPEG_STATIC         | Try to link static ffmpeg libraries                                  | OFF                   |
| SSP_STATIC            | Try to link static gcc stack protector library (gcc only)            | OFF except Win32      |
| OPENAL_STATIC         | Try to link static OpenAL libraries                                  | OFF                   |
| SSP_STATIC            | Link gcc stack protecter libssp statically (gcc, with ENABLE_SSP)    | OFF                   |
| TRANSLATIONS_ONLY     | Build only the translations.zip and nothing else                     | OFF                   |

Note for distro packagers, we use the CMake module
[GNUInstallDirs](https://cmake.org/cmake/help/v2.8.12/cmake.html#module:GNUInstallDirs)
to configure installation directories.

On Unix to use a different version of wxWidgets, set
`wxWidgets_CONFIG_EXECUTABLE` to the path to the `wx-config` script you want to
use.

## MSys2 Notes

To run the resulting binary, you can simply type:

```shell
./visualboyadvance-m
```

in the shell where you built it.

If you built with `-DCMAKE_BUILD_TYPE=Debug`, you will get a console app and
will see debug messages, even in mintty.

If you want to start the binary from e.g. a shortcut or Explorer, you will need
to put `c:\msys64\mingw32\bin` for 32 bit builds and `c:\msys64\mingw64\bin`
for 64 bit builds in your PATH (to edit system PATH, go to Control Panel ->
System -> Advanced system settings -> Environment Variables.)

If you want to package the binary, you will need to include the MinGW DLLs it
depends on, they can install to the same directory as the binary.

Our own builds are static.

## Debug Messages

We have an override for `wxLogDebug()` to make it work even in non-debug builds
of wx and on windows, even in mintty.

It works like `printf()`, e.g.:

```cpp
int foo = 42;
wxLogDebug(wxT("the value of foo = %d"), foo);
```

From the core etc. the usual:

```cpp
fprintf(stderr, "...", ...);
```

will work fine.

You need a debug build for this to work or to even have a console on Windows.
Pass `-DCMAKE_BUILD_TYPE=Debug` to cmake.

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

in your shell to enable coredump files.

[This
post](https://ask.fedoraproject.org/en/question/98776/where-is-core-dump-located/?answer=98779#post-id-98779)
explains how to retrieve core dump on Fedora Linux (and possibly other
distributions.)

Once you have the core dump file, open it with `gdb`, for example:

```shell
gdb -c core ./visualboyadvance-m
```

In the `gdb` shell, to print the backtrace, type:

```
bt
```

This may be a bit of a hassle, but it helps us out immensely.

## Contributing

See the [Developer Manual](/DEVELOPER-MANUAL.md).
