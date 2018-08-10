<!-- START doctoc generated TOC please keep comment here to allow auto update -->
<!-- DON'T EDIT THIS SECTION, INSTEAD RE-RUN doctoc TO UPDATE -->


- [Visual Boy Advance - M](#visual-boy-advance---m)
  - [Building](#building)
  - [Cross compiling for 32 bit on a 64 bit host](#cross-compiling-for-32-bit-on-a-64-bit-host)
  - [Cross Compiling for Win32](#cross-compiling-for-win32)
  - [CMake Options](#cmake-options)
  - [MSys2 Notes](#msys2-notes)
  - [Debug Messages](#debug-messages)
  - [Reporting Crash Bugs](#reporting-crash-bugs)
  - [Contributing](#contributing)

<!-- END doctoc generated TOC please keep comment here to allow auto update -->

[![Join the chat at https://gitter.im/visualboyadvance-m/Lobby](https://badges.gitter.im/visualboyadvance-m/Lobby.svg)](https://gitter.im/visualboyadvance-m/Lobby)
[![Build Status](https://travis-ci.org/visualboyadvance-m/visualboyadvance-m.svg?branch=master)](https://travis-ci.org/visualboyadvance-m/visualboyadvance-m)
[![Coverity Scan Build Status](https://scan.coverity.com/projects/16096/badge.svg)](https://scan.coverity.com/projects/visualboyadvance-m-visualboyadvance-m)

# Visual Boy Advance - M

Game Boy Advance Emulator

Homepage and Forum: http://vba-m.com

Windows and Mac builds are in the [releases tab](https://github.com/visualboyadvance-m/visualboyadvance-m/releases).

Daily Ubuntu packages here: https://code.launchpad.net/~sergio-br2/+archive/ubuntu/vbam-trunk

Your distribution may have packages available as well, search for "vbam".

It is also generally very easy to build from source, see below.

## Note for Windows Users

If you are having issues, try resetting your config file first.

- open file explorer
- in the location bar, type `%USERPROFILE%\AppData\Local` and press enter
- delete the directory called `visualboyadvance-m`

## Building

The basic formula to build vba-m is:

```shell
cd ~ && mkdir src && cd src
git clone https://github.com/visualboyadvance-m/visualboyadvance-m.git
cd visualboyadvance-m
./installdeps

# ./installdeps will give you build instructions, which will be similar to:

mkdir build && cd build
cmake ..
make -j`nproc`
```

`./installdeps` is supported on MSys2, Linux (Debian/Ubuntu, Fedora, Arch or
Solus) and Mac OS X (homebrew, macports or fink.)

The Ninja cmake generator is also now supported, including on msys2.

If your OS is not supported, you will need the following:

- C++ compiler and binutils
- [make](https://en.wikipedia.org/wiki/Make_(software))
- [CMake](https://cmake.org/)
- [git](https://git-scm.com/)
- nasm (optional, for 32 bit builds)

And the following development libraries:

- [zlib](https://zlib.net/) (required)
- [mesa](https://mesa3d.org/) (if using X11 or any OpenGL otherwise)
- ffmpeg (optional, for game recording)
- gettext and gettext tools (optional, with ENABLE_NLS)
- png (required)
- [SDL](https://www.libsdl.org/)2 (required)
- [SFML](https://www.sfml-dev.org/) (optional, for link)
- OpenAL (optional, a sound interface)
- [wxWidgets](https://wxwidgets.org/) (required, 2.8 is still supported, --enable-stl is supported)

On Linux and similar, you also need the version of GTK your wxWidgets is linked
to (usually 2 or 3).

Support for more OSes/distributions for `./installdeps` is planned.

## Cross compiling for 32 bit on a 64 bit host

`./installdeps m32` will set things up to build a 32 bit binary.

This is supported on Fedora, Arch, Solus and MSYS2.

## Cross Compiling for Win32

`./installdeps` takes one optional parameter for cross-compiling target, which
may be `win32` which is an alias for `mingw-w64-i686` to target 32 bit Windows,
or `mingw-gw64-x86_64` for 64 bit Windows targets.

The target is implicit on MSys2 depending on which MINGW shell you started (the
value of `$MSYSTEM`.) It will not run in the MSys shell.

On Debian/Ubuntu this uses the MXE apt repository and works really well.

On Fedora it can build using the Fedora MinGW packages, albeit with wx 2.8, no
OpenGL support, and no Link support for lack of SFML.

On Arch it currently doesn't work at all because the AUR stuff is completely
broken, I will at some point redo the arch stuff to use MXE as well.

## CMake Options

The CMake code tries to guess reasonable defaults for options, but you can
override them on the cmake command with e.g.:

```shell
cmake .. -DENABLE_LINK=NO
```

Of particular interest is making **RELEASE** or **DEBUG** builds, the default
mode is **RELEASE**, to make a **DEBUG** build use something like:

```shell
cmake .. -DCMAKE_BUILD_TYPE=Debug
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
| ENABLE_LINK           | Enable GBA linking functionality (requires SFML)                     | ON                    |
| ENABLE_LIRC           | Enable LIRC support                                                  | OFF                   |
| ENABLE_FFMPEG         | Enable ffmpeg A/V recording                                          | OFF                   |
| ENABLE_LTO            | Compile with Link Time Optimization (gcc and clang only)             | ON for release build  |
| ENABLE_GBA_LOGGING    | Enable extended GBA logging                                          | ON                    |
| ENABLE_DIRECT3D       | Direct3D rendering for wxWidgets (Windows, **NOT IMPLEMENTED!!!**)   | ON                    |
| ENABLE_XAUDIO2        | Enable xaudio2 sound output for wxWidgets (Windows only)             | ON                    |
| ENABLE_OPENAL         | Enable OpenAL for the wxWidgets port                                 | OFF                   |
| ENABLE_SSP            | Enable gcc stack protector support (gcc only)                        | OFF                   |
| VBAM_STATIC           | Try link all libs statically (the following are set to ON if ON)     | OFF                   |
| SDL2_STATIC           | Try to link static SDL2 libraries                                    | OFF                   |
| SFML_STATIC_LIBRARIES | Try to link static SFML libraries                                    | OFF                   |
| FFMPEG_STATIC         | Try to link static ffmpeg libraries                                  | OFF                   |
| SSP_STATIC            | Try to link static gcc stack protector library (gcc only)            | OFF except Win32      |
| OPENAL_STATIC         | Try to link static OpenAL libraries                                  | OFF                   |
| SSP_STATIC            | Link gcc stack protecter libssp statically (gcc, with ENABLE_SSP)    | OFF                   |

Note for distro packagers, we use the CMake module
[GNUInstallDirs](https://cmake.org/cmake/help/v2.8.12/cmake.html#module:GNUInstallDirs)
to configure installation directories.

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

For our own builds, we use MXE to make static builds.

## Debug Messages

We have an override for `wxLogDebug()` to make it work even in non-debug builds
of wx and on windows, even in mintty. Using this function for console debug
messages is recommended.

It works like `printf()`, e.g.:

```cpp
int foo = 42;
wxLogDebug(wxT("the value of foo = %d"), foo);
```

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

Please keep in mind that this app needs to run on Windows, Linux and macOS at
the very least, so code should be portable and/or use the appropriate `#ifdef`s
and the like when needed.

Please try to craft a good commit message, this post by the great tpope explains
how to do so:
http://tbaggery.com/2008/04/19/a-note-about-git-commit-messages.html

If you have multiple small commits for a change, please try to use `git rebase
-i` (interactive rebase) to squash them into one or a few logical commits (with
good commit messages!) See:
https://git-scm.com/book/en/v2/Git-Tools-Rewriting-History if you are new to
this.
