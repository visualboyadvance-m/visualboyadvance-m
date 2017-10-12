# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](http://keepachangelog.com/en/1.0.0/)
and this project adheres to [Semantic Versioning](http://semver.org/spec/v2.0.0.html).

## [Unreleased]

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

