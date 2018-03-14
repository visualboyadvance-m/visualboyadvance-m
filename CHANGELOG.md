# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](http://keepachangelog.com/en/1.0.0/)
and this project adheres to [Semantic Versioning](http://semver.org/spec/v2.0.0.html).

## [2.0.2] Bug fix release 03/13/2018
=======================
1. 480541a - Eleuin - Modify max_threads sanity check to better handle erroneous values
2. d5c9c6b - rkitover - change default audio driver to SDL

## [67c6ad6] developmental release 02/25/2018
========================

1. 67c6ad6 - ZachBacon - implemented the fix from nhdailey, should
resolve #153. Will put up a test build soon - 02/25/2018
2. 669893 - rkitover - add multi-platform build system - 02/24/2018
3. 3e052c9 - obea and rkitover - cmake: Fix -DENABLE_FFMPEG=OFF -
01/18/2018
4. df0bd43 - rkitover - fix deps submodule init on windows - 10/25/2017
5. 7dca069 - ZachBacon - Added some windows specific stuff to builder
10/22/2017
    ..+ f527d0a - ZachBacon - The not so finished builder script for
windows
    ..+ 4204502 - rkitover - mac build/builder improvements
6. c3fc4e7 - ZachBacon - Added basic windows script. 10/21/2017
7. 21926d6 - rkitover - add -mfpmath=sse -msse2 compiler flags:
x86/amd64 10/17/2017
8. d7ff2af - rkitover - add mac release builder script 10/12/2017
    ..+ 5e63398 - rkitover - mac builder refactor/improvements
    ..+ afb1cd3 - automate codesign/zip for mac build, add xz dep
9. 30b6ecf - ZachBacon - add a changelog 10/11/2017
    ..+ 4648638 - rkitover - read version from CHANGELOG.md if no git
## [2.0.1] - 10/11/2017
### Added
- initial installer script
- gitter.im badge in readme
- 32bit cross compiling support on 64bit linux
- added support for GNUInstallDirs to cmake
- Include new translations from transifex
- Added WxWidgets ABI compiler check
- Add linux Joystick reference to issue template
- Add #ubckyde <cmath> fir std::cell()
- 

### Changed
- Fix 2xSaImmx.asm linking issue
- cmake: default to ENABLE_ASM_CORE=OFF
- libretro merging from upstream libretro fork.
- better fix for clipped video in GL Fullscreen.
- Fix flibc crash: add log message on fopen failure
- fix gameboy header-detection in libretro interface
- hotfix for potential buffer-overflow
- delete memory in common/array.h on destruction
- SDL: improve error msg for unwritable config
- SDL: fix deflt bat saving, improve dir checking
- fix portability issue with strerror_r()
- Windows doesn't have sterror_r
- wx/wxvbam: fix GetAbsolutePath 
- fix a memory leak due to wrong syntax
- regenerate translation files
- fix errors reported by SUSE's post build linter
- SUSE Lint: fix a few classes of warnings
- let cmake escape -D preproc. definitions
- fix memory viewer xrc on wx 3.1+
- Improving README
- SoundSDL: lock conditional code cleanup
- remove the default F11 keybinding for save state
- hopefully fix resize artifacts on game panel
- soundSDL: write silence when paused
- cleanup SoundSDL  #139 #130 #97 #67 #65 #46 #47
- document how to provice symbolic backtraces
- "no throttle" fixes/cleanup
- only use -mtune=generic on x86/amd64
- installdeps: use -j$(nproc) not -j8 in info
- use -fabi-version=2 for GCC turn off LTO
- installdeps: minor refactoring/cleanup
- improve Wx GCC ABI check
- minor improvements for Wx Compile tests
- fix wx ABI check for Win32/MinGW
- installdeps: fedora fixes, including m32
- cmake: wx and cross compiling fixes
- restore wx 2.7 compat, improve string processing
- rename CMakeScripts/ to cmake/
- work around gcc lto wrappers bug with gcc 7.x
- update README.md
- remove doctoc title from README.md TOC
- improve win32 dependencies git submodule handling
- use num cpu cores to parallelize LTO link with gcc
- fix huge app icon in Win volume settings
- fix deadlock in SoundSDL:deinit()
- added executable extension for sdl binary
- default LTO to off on 64bit MinGW
- minor code cleanup
- deps: dont check result of apt-get update 
- read version and subversion info from Git
- fix error and version info in shallow git clones
- refactor/cleanup GitTagVersion.cmake

### Removed
- Cairo Renderer: it never performed well to begin with



[2.0.1]: https://github.com/olivierlacan/keep-a-changelog/compare/throttle...v2.0.1


