# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](http://keepachangelog.com/en/1.0.0/)
and this project adheres to [Semantic Versioning](http://semver.org/spec/v2.0.0.html).

## [2.1.0] Vulnerability fixes
=======================
* 68028b50 - (HEAD -> master, origin/master, origin/HEAD) minor fix for mingw cross build (9 hours ago) <Rafael Kitover>
* 498019a3 - (origin/osx-32bit) support older 32 bit macs running 10.7, fix build (11 hours ago) <Rafael Kitover>
* 6b486258 - fix some ELF parsing vulnerabilities #255 (30 hours ago) <Rafael Kitover>
* c63d3640 - travis: add job for libretro module (2 days ago) <Rafael Kitover>
* ef8c89fc - Libretro: Do not compile arm disassembly module (2 days ago) <retro-wertz>
* 63431916 - fix 2 broken URLs in builder script (3 days ago) <Rafael Kitover>
* 48af3829 - travis: move cache key for ccache into jobs (3 days ago) <Rafael Kitover>
* 21b718b8 - Add Coverity Badge (4 days ago) <ZachBacon>
* 8778837c - add Travis CI support (4 days ago) <Rafael Kitover>
* fde9d731 - installdeps: add ccache to list of deps (4 days ago) <Rafael Kitover>
* 77a3673d - installdeps: install wx-common for mxe for wxrc (4 days ago) <Rafael Kitover>
* 90188e88 - installdeps: fix build instructions for mxe (4 days ago) <Rafael Kitover>
* 816aab99 - fix installdeps for ubuntu:trusty (4 days ago) <Rafael Kitover>
* e4923e72 - fix save dialogs on Mac #268 (4 days ago) <Rafael Kitover>
* 66a50e35 - note how to reset config in windows in README.md (7 days ago) <Rafael Kitover>
* ed00dc77 - use GetWindow()->Refresh() on non-GTK too #260 (12 days ago) <Rafael Kitover>
* 89228b06 - fix high CPU usage under wxgtk2 #260 (12 days ago) <Rafael Kitover>
* d1603218 - fix $ENV{WX_CONFIG} check yet again (12 days ago) <Rafael Kitover>
* 0489756d - fix $ENV{WX_CONFIG} check in 46f52941 (12 days ago) <Rafael Kitover>
* 46f52941 - do not prefer wxgtk3 if $ENV{WX_CONFIG} is set (12 days ago) <Rafael Kitover>
* 437b366e - clean up wxgtk3 finding cmake code (12 days ago) <Rafael Kitover>
* 2efcb620 - support and default to wxgtk3 not gtk2 on arch (13 days ago) <Rafael Kitover>
* 430b5d63 - fix Mac OS 10.13 build issues (13 days ago) <Rafael Kitover>
* 9222894c - Merge pull request #263 from retro-wertz/patch-2 (2 weeks ago) <Zach Bacon>
* 02e1f49a - Add couple of file ext on .gitignore (2 weeks ago) <retro-wertz>
* fad1dd15 - Merge pull request #262 from retro-wertz/libretro (2 weeks ago) <Zach Bacon>
* 9c859917 - Opps (2 weeks ago) <retro-wertz>
* 009c09ef - GBA: Show log when rom uses SRAM of FLASH save types (2 weeks ago) <retro-wertz>
* 90bc79f2 - Libretro: implement vbam logging using libretro logging api (2 weeks ago) <retro-wertz>
* f4b88ba6 - Libretro: Remove GBA LCD filter (2 weeks ago) <retro-wertz>
* 76389d8e - RTC: Change this #ifdef to GBA_LOGGING (2 weeks ago) <retro-wertz>
* 710d2f3e - UtilRetro: Remove LCD filter (2 weeks ago) <retro-wertz>
* e0fe8365 - Makefile: Add option for sanitizer, add -DNO_DEBUGGER (2 weeks ago) <retro-wertz>
* 94d07676 - Makefile: Remove sources we dont need (2 weeks ago) <retro-wertz>
* 50e91f79 - Remove unrelated function during rom load (2 weeks ago) <retro-wertz>
* 3c0e88bc - Remove wrong #ifdef decleration (2 weeks ago) <retro-wertz>
* 3791b0a0 - Libretro: Fix samplerate not passed correctly to gba core (2 weeks ago) <retro-wertz>
* 3385be25 - Silence some warnings (2 weeks ago) <retro-wertz>
* 4d4819f0 - Libretro: Add core options for Sound Interpolation and Filtering (2 weeks ago) <retro-wertz>
* 94f11023 - Libretro: Add core option to mute sound channels (2 weeks ago) <retro-wertz>
* 5a4c788d - Merge pull request #256 from retro-wertz/patch-1 (2 weeks ago) <Zach Bacon>
* bdb164bd - Set mirroringEnable to false (2 weeks ago) <retro-wertz>
* 0047fa1a - Merge pull request #253 from retro-wertz/libretro (2 weeks ago) <Zach Bacon>
* 17b681b8 - ereader.cpp: Silence warning (2 weeks ago) <retro-wertz>
* 27fa30b4 - Libretro: Disable cheats by default, update to bios loading (2 weeks ago) <retro-wertz>
* 01c5f465 - Libretro: Cleanup controller layout binds (2 weeks ago) <retro-wertz>
* 319a4869 - Libretro: Do not allow opposing directions (2 weeks ago) <retro-wertz>
* 2c46522e - Libretro: Add Solar Sensor (3 weeks ago) <U-DESKTOP-UVBJEGH\Cloud>
* 037e3771 - let's place these in the correct spot shall we? (3 weeks ago) <ZachBacon>
* 90d2f5c8 - Added retro-wertz for his contributions in bringing in libretro back to speed and other things (3 weeks ago) <ZachBacon>
* 03bc7c24 - Merge pull request #249 from retro-wertz/updates (3 weeks ago) <Zach Bacon>
* 991fc749 - Do this for GBA sound enhancements too (3 weeks ago) <retro-wertz>
* 4e4424e8 - GB: Fix sound options not working (3 weeks ago) <retro-wertz>
* abb62df6 - pause on menu pulldown on windows only (FIXED) (3 weeks ago) <Rafael Kitover>
* cb3e9e32 - pause on menu pulldown on windows only (3 weeks ago) <Rafael Kitover>
* 1115be12 - Merge pull request #248 from retro-wertz/updates (3 weeks ago) <Zach Bacon>
* bc0e169d - Add missing file (3 weeks ago) <U-DESKTOP-UVBJEGH\Cloud>
* edf939e9 - Gfx: Add #ifdef _MSC_VER, fix tiled rendering on windows (3 weeks ago) <retro-wertz>
* 57dc0c25 - Move gfxDrawTextScreen() into GBAGfx.cpp (3 weeks ago) <retro-wertz>
* 27aeb6dc - Re-add Types.h, remove some more #ifdef (3 weeks ago) <retro-wertz>
* 9ec142da - Add header guard (3 weeks ago) <U-DESKTOP-UVBJEGH\Cloud>
* 7a194fb2 - do not pause games when menus are pulled down (3 weeks ago) <Rafael Kitover>
* 6e18c3c2 - exclude headers for now (3 weeks ago) <ZachBacon>
* 947cd10e - minor OSD code cleanup (3 weeks ago) <Rafael Kitover>
* 0dc3e06c - update installdeps for opensuse (3 weeks ago) <Rafael Kitover>
* cafe905b - Fix a cast for msvc compilers (4 weeks ago) <ZachBacon>
*   07bc2ee4 - Merge pull request #246 from retro-wertz/libretro (4 weeks ago) <Zach Bacon>
* 1606ea74 - libretro: Cleanup some #ifdefs (4 weeks ago) <retro-wertz>
* 1d4dacc6 - Merge pull request #244 from retro-wertz/fix_gba_cheats (4 weeks ago) <Zach Bacon>
* e2dff89d - Fix cheats not working in GBA (4 weeks ago) <retro-wertz>
* a3510c90 - Merge pull request #243 from retro-wertz/fix_opcode (4 weeks ago) <Zach Bacon>
* 9ab2ee67 - Fix some opcodes in arm (4 weeks ago) <retro-wertz>
* a5e717a2 - add brace so it doesn't break compilation (4 weeks ago) <Zach Bacon>
* 4a615ab3 - Gonna take my time and actually work on a D3D9 panel. Documentation here I come. (4 weeks ago) <ZachBacon>
* fdb39a1c - Merge pull request #242 from retro-wertz/fix_alignments (4 weeks ago) <Zach Bacon>
* 26c8c61b - Fix some formatting alignments in arm/thumb opcodes (4 weeks ago) <retro-wertz>
* 7c3d8d02 - Merge pull request #241 from retro-wertz/libretro (4 weeks ago) <Zach Bacon>
* 8edd4ce5 - libretro: Fix save types not properly set using overrides (4 weeks ago) <retro-wertz>
* 893269d8 - Merge pull request #239 from retro-wertz/libretro (4 weeks ago) <Zach Bacon>
* 3411aa33 - libretro: Use stdint.h instead in most cases (4 weeks ago) <retro-wertz>
* 1f20ba81 - libretro: Update (4 weeks ago) <retro-wertz>
* b0982ac8 - Always apply map masks (4 weeks ago) <retro-wertz>
* ff4f1235 - libretro: Fix wrong file pointer (4 weeks ago) <retro-wertz>
* 8f6e5de5 - libretro: Fix error : narrowing conversion (4 weeks ago) <retro-wertz>
* 8a1fd587 - libretro: Add header guard to UtilRetro.h (4 weeks ago) <retro-wertz>
* 85dea8a5 - add cygwin cross build support (9 weeks ago) <Rafael Kitover>
* 8638c769 - require pkg-config in cmake for gtk checks #227 (9 weeks ago) <Rafael Kitover>
* 60f2bd3f - remove ffmpeg from suse installdeps (9 weeks ago) <Rafael Kitover>
* 9933b6bb - if cross compiling openssl, need to add --cross-compile-prefix=- (2 months ago) <ZachBacon>
* 55caf62e - win cross script: pass RANLIB to openssl make (2 months ago) <Rafael Kitover>
* 9cd26b25 - windows cross build script: use autoconf for bzip2 (2 months ago) <Rafael Kitover>
* d5f90f0a - fix SDL sound on windows (3 months ago) <Rafael Kitover>
* fe9f0641 - fix msys2 builder script (3 months ago) <Rafael Kitover>
* 70c8dee8 - cmake: default ENABLE_OPENAL to OFF (3 months ago) <Rafael Kitover>
* d992cfa2 - Finalize vertical draw fixes and code cleanup (3 months ago) <Eleuin>
* 62e8098f - Fix forbidden conversion in remote.cpp (3 months ago) <Lucas>
* edb2fd26 - Merge pull request #214 from Eleuin/name-fixes (3 months ago) <Zach Bacon>
* 9843af2b - Fix cmake directory in installdeps (3 months ago) <Eleuin>
* 80bcdab9 - Fix draw height across rendering modes (3 months ago) <Eleuin>
* f3f6ee7b - fix compile errors with ffmpeg git (3 months ago) <Rafael Kitover>
* 5ae853a9 - fix installdeps for OpenSUSE (3 months ago) <Rafael Kitover>
* 8540860a - adjust changelog version regex for cur. version (4 months ago) <Rafael Kitover>
* 61e2f3b2 - Merge pull request #204 from Eleuin/filter-bounds-fix (4 months ago) <Zach Bacon>
* d7a4eddb - Fixed filter draw bounds (4 months ago) <Eleuin>  


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


