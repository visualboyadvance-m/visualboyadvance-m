# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](http://keepachangelog.com/en/1.0.0/)
and this project adheres to [Semantic Versioning](http://semver.org/spec/v2.0.0.html).

## [2.1.5] - 2022-12-15
=======================
* ea2a929f - Backport 800d6ed69b from wx to fix Wayland EGL pos [rkitover]
* c40a1417 - Write vsync toggle msg to debug log not OSD [rkitover]
* 5d2dd01e - Call Layout() after DrawingPanel OnSize event [rkitover]
* da686a48 - Reinit panel on vsync toggle. [rkitover]
* ec27885c - Support EGL VSync on Wayland. [rkitover]
* c27d384a - Enable GL on Wayland for wx 3.2+, no vsync. [rkitover]
* c249115b - build: set -mtune=generic [rkitover]
* 297d7c06 - [hidpi] Use wxWidgets 3.2 on Windows [steelskin]
* 0d86432a - [hidpi] Implement full HiDPI support [steelskin]
* 69d24fb5 - [build][msvc] Fix debug information and set hidpi support [steelskin]
* 90fc3e12 - Default CTRL+I to change interframe blender [rkitover]
* a57d81e1 - Show name of interframe blender not num in OSD [rkitover]
* 13c558ac - Disable wireless link type [rkitover]
* 639003a8 - Show pixel filter name not number on cycle [rkitover]
* 236cffb4 - Make another adjustment to glXQueryExtensions [rkitover]
* 643ec0f7 - Pass DefaultScreen(display) to glXQueryExtensions [rkitover]
* 60d7ead5 - Fix menu state after playing a movie [laqieer]
* d2fee771 - Add VBA Movie Version 2 [laqieer]
* 3f2d3c13 - Remove manual joystick polling [steelskin]
* 7726d26f - Enable full optimizations and fix x86 builds [danialhorton]
* 73223445 - MSVC opt /fp:fast /Oi, intrinsic sqrt() w/ XBRZ. [rkitover]
* 972f1513 - Enable GB colorization hack support in GUI. [danialhorton]
* 1741fa32 - Fix autosaveloadcheat and rename pref [danialhorton]
* a31a7a30 - in Wx, the Option is Enable MMX, make the variable also enableMMX and set its default to on. [danialhorton]
* 983ea451 - Flash : Erased memory is FFh-filled [negativeExponent]
* 18e387d1 - Fix Flash chip's erase operation: erased memory is FFh-filled, not zero-filled. [laqieer]
* c05056ab - HuC3: Update [negativeExponent]
* 93084990 - HuC3: Refactoring to avoid breaking savestates [negativeExponent]
* 05a046e7 - HuC3: Enable battery save, update RTC support [negativeExponent]
* 70477dcd - GBA:Fix serial emulation if compiled with NO_LINK [negativeExponent]
* 15bcad13 - GB:Fix serial emulation when compiled with NO_LINK [negativeExponent]
* 025f3079 - GBA:Fix a few memory read/write issues [negativeExponent]
* 210e79bb - Add descriptions for the 'Interframe Blending' and 'LCD Color Filter' options [jrb22x]
* 8a3456f8 - Default to not change battery on state load. [rkitover]
* b50ff858 - add a missing break in half word reads [danialhorton]
* 064fa7b0 - Fix SDL audio delay [Clownacy]
* 7db3c817 - Fixed SDL audio using the wrong audio specification [Clownacy]
* cb0933bf - Stop changing plane ptrs in rec. audio frames. [rkitover]
* 80634683 - Force a panel update when configuring bilinear. [briansrls]
* 6a714281 - Differentiate between SDL joy index/instance_id. [rkitover]
* 11693d83 - Clean up GB MBC30 check. [rkitover]
* 020daad8 - Fix joy accels when emulation is paused. [rkitover]
* def5d3e4 - Stop timer when game is loaded again. [rkitover]
* e0816b0e - Fix weird input/buttons memory leak. [edenisfa]
* 7031c1d1 - [LINUX,WINDOWS] Key/Joy background input. [edenisfa]
* 84f3e8ce - Allow joystick background input. [edenisfa]
* 40741580 - Add GUI cmd line option for config file. (#724) [Steelskin]
* c169420f - Turbo/throttle config and DirectSound fixes. [rkitover]
* 378c9493 - Fix joystick config when game is loaded. [rkitover]
* 95213a6d - GB: Only use mapperLastTime to see if RTC data was loaded [negativeExponent]
* 86fcffef - GB: Add support for 4MB MBC30 [negativeExponent]
* 2f9933ae - GBA BIOS: ArcTan/ArcTan2 fixes for HLE bios [negativeExponent]
* d9732f68 - Fix clock cycle count for MUL, MLA instructions...  (https://github.com/libretro/vba-next/commit/b91f39c787cc99ace97546cb94985f3b658c8b57) - backport Fix clock cycle count for some arm/thumb instructions (https://github.com/libretro/vba-next/commit/5243b2d8064dd2145888ced7cbfe127594ecf6db) - backport Fix base cycle count for MUL, MLA series (https://github.com/libretro/vba-next/commit/b91f39c787cc99ace97546cb94985f3b658c8b57) [negativeExponent]
* c07d5b65 - Fix vram 16/32 bit unaligned reads(backport https://github.com/libretro/vba-next/commit/7f18771165dd4a6d71a598c51d91668920572eaa) [negativeExponent]
* 18c2ea61 - Further fixing Multi Buffer formatting. [briansrls]
* 1e569300 - Addressing audio plugin crash [briansrls]
* 9c314a15 - Use proper string `"wglGetExtensionsStringEXT"`. [edenisfa]
* 3ba05295 - Improve BIOS Math tests score from 425 to 625. [edenisfa]
* 6f2e320d - Improve clock cycle count for THUMB operations. [edenisfa]
* 9e537a8f - Improve cycle count for MUL, MLA. [edenisfa]
* 306f92f5 - Check game kbd input with wxGetKeyState() again. [rkitover]
* 6b257d52 - Avoid override of variable `throttle` for turbo. [edenisfa]
* bd2483ca - Allow changing connection type without restart. [edenisfa]
* d379a076 - Decrease mouse sensitivity for menu hiding. [rkitover]
* 0c5ee994 - Fix PNG capture screen. [edenisfa]
* 1347026d - Turbo config refactor followup. [rkitover]
* 1bced474 - Codesign fix for mac. [rkitover]
* 07064c87 - [Windows,MacOS] Auto-updater refactoring. (#639) [denisfa]
* 09e8da43 - Joystick refactor pt. Deux: Support Joysticks. [rkitover]
* 504ca08b - Hide menubar + minor UI refactor. [rkitover]
* 2868c1b7 - Fix changing aspect ratio option. [rkitover]
* 045e95ab - GB: Support 4MiB MBC30 ROM bank select. [rkitover]
* 0b14e9f8 - [WINDOWS] Add function to open unicode files. (#644) [denisfa]
* 8e3978b3 - Speedup/Turbo/Throttle fixes. [rkitover]
* 166cf446 - Disable menu LCD filter unless playing GB or GBA. [edenisfa]
* 06001069 - Init systemColorMaps after output module init. [edenisfa]
* cd029ae6 - Fix video recording with frame skip enabled. [edenisfa]
* 008aecde - [LINUX] Fix display of `RGB` values on viewers. [edenisfa]
* 834d1537 - Fix accel shortcuts when using some menu options. [edenisfa]
* 994726dd - Do not pause while opening configuration windows. [edenisfa]
* 54335dc6 - [WINDOWS] Fix weird WX errors messages. [edenisfa]
* 7bac34ed - Remove duplicate variable `winFlashSize`. [edenisfa]
* 8d62a790 - Change default options for GB. [edenisfa]
* 99b66e13 - Expose LCD filter for GBA and GB color saturation. [edenisfa]
* c789020e - Fix `Save` button on OAM Viewer for GB and GBA. [edenisfa]
* dcaa1b9a - Fix `Save` button on Tile Viewer for GB and GBA. [edenisfa]
* d05e8fc1 - Make AGBPrint output to our Logging. [edenisfa]
* abb82341 - Allow binding specific server IP. (#633) [rkitover]
* 6c64db07 - cmake: Windows codesigning refactor. [rkitover]
* 4f1a351d - Try to configure vsync for multiple options. [edenisfa]
* ed8a9af2 - Use proper functions and checks for OpenGL VSYNC. [edenisfa]
* 6092082e - Fix Save Failed error for Super Monkey Ball Jr. [54053706+negativeExponent]
* f1099a21 - Check return value of soundInit(). [rkitover]
* 410ede54 - Check for null pointer in soundReset(). [alexjnewt]
* 9d1d707c - Limit ROM size to 32 MiB. [negativeExponent]
* fd319d21 - GB: Check header for a valid ROM file. [negativeExponent]
* 513af13d - Replace `libpng` code for `stb_image` one. [edenisfa]
* 8f48c1f0 - Fix weird `wxWidgets>=3.0` error message. [edenisfa]
* 2f065382 - Enable XAudio2 for Windows vcpkg build. [rkitover]
* a3203770 - Fix: a issue with loading e-Reader dot code (#608) [sony.pcv.s520]
* 83614290 - Turn on winsparkle update checking support. [rkitover]
* 4aab08c4 - Set link timeout default to 500ms, fix prev. dflt. [rkitover]
* 136c094f - Add option to set link network port. [rkitover]
* f69febfd - Add `.wav` support for sound recording. [edenisfa]
* 6a8a9e62 - Fix integer overflow in cheatsImportGSACodeFile length check. [zack.ref]
* b50d4845 - GBA: Fix crash when new size is larger than rom size after soft-patching [negativeExponent]
* 9e1a63af - support for patches in BPS format [ArtiiP]
* 72d0725b - Allow game window to resize on-the-fly when enabling/disabling borders [negativeExponent]
* 225e4dc9 - Remove top border black line when using filters. [edenisfa]
* 0535d5c7 - Optimize CRC32. [mudlord]
* 4bc9bb84 - Implement joystick rumble. #522 [rkitover]
* 384f4487 - Fix key accel overriding menus and dialogs. [edenisfa]
* af0de1c4 - xbrz: fix inline asm check [rkitover]

## [2.1.4]
=======================
* c93e017d - builder: minor changes to support mac 32 bit (4 days ago)
* c1636d1b - builder: fixes for ffmpeg + misc. (4 days ago)
* aec23e35 - builder: re-enable ffmpeg, update dists (2 weeks ago)
* 806dd7da - Fix circular `--config` for SDL port. (7 days ago)
* 1aa8afb4 - Drop dead code entirely. (7 days ago)
* 531172c4 - Fix displaying of link menu when `ENABLE_LINK=NO`. (2 weeks ago)
* 07107096 - [Windows] Fix crash when setting weekly updates. (2 weeks ago)
* efa09bdb - Add faster sqrt code for xBRZ filters. (2 weeks ago)
* 10f5576e - Fix config file when removing system accels. (3 weeks ago)
* 1c6b0168 - [Windows] Fix remaining UTF-8 issues. (3 weeks ago)
* a2f3126e - transifex pull (3 weeks ago)
* 1e9d1675 - builder: pause before codesigning for win targets (3 weeks ago)
* 0af5965e - builder: work on linux static binary (3 weeks ago)
* 33c43e3a - add -p:BuildInParallel=true to appveyor.yml (3 weeks ago)
* 61141ebd - add -p:BuildInParallel=true to VS instructions (3 weeks ago)
* 9ee6b54a - cmake: remove invalid opt /std:c++11 for MSVC (3 weeks ago)
* 0f2c8cab - win: check APPDATA if LOCALAPPDATA is not found (3 weeks ago)
* 32fbfec4 - transifex pull (3 weeks ago)
* cc516229 - transifex push/pull (4 weeks ago)
* 528c37f9 - Allow "Browse" to be translated. (4 weeks ago)
* d8c981fb - Fix toggles for video/audio layer options on menu. (4 weeks ago)
* c0f96425 - Show all commands available on menu. (4 weeks ago)
* 51fa150f - Adjust MMX menu variable and position. (4 weeks ago)
* 27b038b3 - Show messages about operations of state slot. (4 weeks ago)
* 2e248457 - Allow translation of the words `Open` and `Quit`. (4 weeks ago)
* 54991c5b - Add extra keys default input when using GUI. (4 weeks ago)
* b358c051 - Fix compilation issue when `ENABLE_DEBUGGER=Off`. (4 months ago)
* 2a2be953 - builder: msys2 fixes (4 weeks ago)
* f1438e0f - Check ASAN support when `ENABLE_ASAN=ON`. (4 weeks ago)
* ade64db7 - transifex pull (4 weeks ago)
* 8b347050 - cmake: check for ffmpeg >= 4.0.4 (4 weeks ago)
* 0b9c7710 - travis: more ubuntu dists, fix mac cache (5 weeks ago)
* d9e4a087 - build fixes: ffmpeg, xbrz, travis, C++ version (5 weeks ago)
* 23224fba - update factory reset instructions in README.md (5 weeks ago)
* 381a7bfe - /std:c++latest for MSVC, fix bool flip, warn opts (5 weeks ago)
* 234f1e9b - Update to xBRZ `1.7`. (6 weeks ago)
* 290012d1 - transifex pull (6 weeks ago)
* 06a9d786 - Allow UPS patch for GBA games. (6 weeks ago)
* 1949cdb3 - transifex push/pull (6 weeks ago)
* 03121c69 - Add support for autohold buttons. (6 weeks ago)
* e87213e3 - Fix indentation. (6 weeks ago)
* cf47942f - Fix Autofire {L,R}. (6 weeks ago)
* 4993cb88 - remove tea-ci .drone.yml and installer.nsi (6 weeks ago)
* d9a7e0c9 - cmake: auto-detect sfml, ffmpeg and openal (6 weeks ago)
* c64dd259 - Update README.md (6 weeks ago)
* 8ceef90f - fix appveyor visual studio build (6 weeks ago)
* a8b7b72d - transifex pull (6 weeks ago)
* f5eabb35 - builder: disable LTO on 64 bit mingw (6 weeks ago)
* fc2eb38f - Fix behaviour of `Tools` > `Memory Viewer` (#486) (6 weeks ago)
* 2958af01 - builder: mingw improvements + misc. (6 weeks ago)
* 3923b28f - transifex pull/push (6 weeks ago)
* 51f61a3a - Merge remote-tracking branch 'vbam-libretro/master' (6 weeks ago)
* 6bdd6d1b - Merge pull request #77 from negativeExponent/fix_cheat_search (7 weeks ago)
* dc8d36a0 - minor fix for stl wx builds (7 weeks ago)
* 51a4f74b - Merge remote-tracking branch 'vbam-libretro/master' (7 weeks ago)
* 9ccdeac3 - Add missing break; lines for switch statements (7 weeks ago)
* b8afa8c5 - Merge pull request #76 from RobLoach/intl (7 weeks ago)
* a17eb471 - Merge branch 'master' of github.com:fpscan/vbam-libretro into intl (7 weeks ago)
* 9b043daa - libretro: Update core options API (7 weeks ago)
* 6b18c1f9 - Fix warning about frames left in queue. (8 weeks ago)
* f5b19475 - [IDEA] Fix recording when using `speedup/turbo`. (8 weeks ago)
* 738494a2 - Support all sample rate available on GUI. (8 weeks ago)
* aa3ecaf7 - Add new output formats for recording audio/video. (9 weeks ago)
* 5848feae - Fix video/audio recording. (2 months ago)
* 9e3c2615 - Update libretro_core_options.h (7 weeks ago)
* 6c60ac33 - Turkish Language option (7 weeks ago)
* d92ca688 - Merge pull request #74 from retro-wertz/update (7 weeks ago)
* ad43b32b - Fix "index out of bounds" runtime error (7 weeks ago)
* 36f3a739 - Merge pull request #73 from retro-wertz/libretro_cleanup (8 weeks ago)
* 82e723a5 - MBC3: Update mapper to check if RTC is present before running RTC functions (8 weeks ago)
* 03184dd5 - libretro: Use GB RTC data when available (8 weeks ago)
* b3744b3b - Simplify GB border core option (8 weeks ago)
* 9757a880 - Core options cleanup and a few readability-related updates (8 weeks ago)
* cfb03d8b - Set default keybinding when key does not exist. (9 weeks ago)
* 1ef09126 - Merge pull request #72 from retro-wertz/enable_rtc (8 weeks ago)
* 496b372d - Allow enabling GBA RTC regardless of rom types (8 weeks ago)
* 4dbdf87d - Merge pull request #71 from retro-wertz/patch-3 (8 weeks ago)
* bb109544 - Update Makefile (8 weeks ago)
* 3e640443 - Merge pull request #70 from retro-wertz/update_makefiles (8 weeks ago)
* 98313652 - Update Makefile (8 weeks ago)
* 34de0579 - Merge pull request #67 from retro-wertz/update_makefiles (8 weeks ago)
* 71e4b7b3 - Merge pull request #69 from retro-wertz/master (8 weeks ago)
* d9f8396c - Cleanup (8 weeks ago)
* 9a134497 - Update Makefiles (8 weeks ago)
* 38b7613e - Setting up basic appveyor (9 weeks ago)
* 408cab71 - Merge pull request #66 from retro-wertz/libretro (9 weeks ago)
* d8982881 - Automatically hide some core options depending on rom image type (9 weeks ago)
* c3edd6d4 - Merge pull request #65 from retro-wertz/libretro (9 weeks ago)
* 2084a63e - Update GB/GBC memory maps (retro_memory_descriptors) (9 weeks ago)
* 90b051ee - Update libretro_core_options.h (9 weeks ago)
* a727e222 - GB: Rename this variable to isolate from a global variable of the same name (10 weeks ago)
* 0d423b98 - Cleanup GB palette struct and update retro_get_memory_data/size (10 weeks ago)
* bc80ecdb - UtilRetro.cpp: Cleanup (10 weeks ago)
* 855db11d - Fix wxWidgets assertion error on `Logging...` menu. (10 weeks ago)
* c886b6a1 - remove wrong and unnecessary #define (10 weeks ago)
* 38561b57 - Merge remote-tracking branch 'vbam-libretro/master' (2 months ago)
* 770b9c1c - Add left analog bind to D-PAD for joystick. (2 months ago)
* 6de45d4e - Set default binding for keyboard. (3 months ago)
* 78bbe148 - Add individual clear buttons for input config. (3 months ago)
* 53002e54 - Use hats from joystick as default D-PAD. (3 months ago)
* 57b7ddf5 - Fix small memory leak. (3 months ago)
* 060da968 - [LINUX] Add option to disable the online updater. (3 months ago)
* dbb5914f - Show which BIOS file is currently selected. (3 months ago)
* 76756ef9 - Fix key shortcuts for toggle menu actions. (3 months ago)
* 4622f1b8 - Whitespace fix. (3 months ago)
* 3dc738b5 - Set default keys for input on first run. (3 months ago)
* 9bc8f33c - Disable using volume {+,-} actions in main menu. (3 months ago)
* d1fcffc8 - Merge pull request #63 from retro-wertz/fix_gba_siocnt (2 months ago)
* 7df7b190 - Merge pull request #64 from retro-wertz/patch-2 (2 months ago)
* 7f105da3 - Update README.md (2 months ago)
* 53ce6fb0 - libretro: add SIO Control register (GBA) (2 months ago)
* 2e96f59e - Merge pull request #62 from retro-wertz/gb_timing_updates (2 months ago)
* e50812dc - GB: input, audio, video timing updates (2 months ago)
* 1532f555 - GB: Reduce input lag by 1 frame, video and audio timing updates... (2 months ago)
* f09a9aa9 - builder: add python3 to msys2 deps (2 months ago)
* 2fcad3f8 - joysticks: hack to better support 360 triggers (3 months ago)
* 352eb149 - Merge remote-tracking branch 'vbam-libretro/master' (3 months ago)
* 2e97b27f - Use --config option to load configuration file. (3 months ago)
* bdd43868 - Merge pull request #61 from retro-wertz/gb (3 months ago)
* bd5ba431 - Cleanup: Remove previous core option struct and unused variables etc (3 months ago)
* aac30c72 - Add GB Color Correction core option (3 months ago)
* 02a7a48b - GB: Fix selected palette not applied when loading content... (3 months ago)
* 8462ca76 - Merge pull request #59 from retro-wertz/gb (3 months ago)
* 1fa78e69 - Fix android build (3 months ago)
* 6dcf016d - GB: Allow support for colorizer patched games - a new core option is provided since enabling this support means allowing invalid access to vram and palette - works in GBC/GBA hardware mode only (3 months ago)
* 85bd86cf - GB: Send LCD interrupt only if there is no INT 48h signal (3 months ago)
* b147e912 - Merge pull request #57 from retro-wertz/sync (3 months ago)
* d808297b - Fix crash caused by incorrect variable index for analog sticks (3 months ago)
* 7e882898 - libretro: Add core option sublabels (3 months ago)
* ce11403b - builder: updates for mingw (3 months ago)
* c5e6a340 - Libretro: Update GBC memory map (3 months ago)
* fa45ccdf - builder: fix command(), update c2man url (3 months ago)
* e64b9904 - builder: fix msys2 (3 months ago)
* 58022078 - builder: make zip + sig on win + minor changes (3 months ago)
* 296255d4 - handle SDL joystick query errors #451 (3 months ago)
* 95658f57 - transifex sync (3 months ago)
* 86c4dc7d - transifex sync (3 months ago)
* bcc8b540 - transifex sync (3 months ago)
* 7569242f - Fix pause behaviour with `Pause when inactive`. (3 months ago)
* 030740ee - Fix some memory leak. (3 months ago)
* 8ea607e1 - remove translation from joystick keys (3 months ago)
* cec32a61 - travis: mac: disable --help check, it segfaults (3 months ago)
* f54dd25b - travis: fix gettext on mac + brew (3 months ago)
* e2f06210 - update Visual Studio support (3 months ago)
* 91501bec - Fix small memory leak. (3 months ago)
* c366907f - Temporary fix for #65. (3 months ago)
* d585a610 - followup on kbd trans issues fix in 30b92724 (3 months ago)
* eaf1ad24 - Merge remote-tracking branch 'vbam-libretro/master' (3 months ago)
* 30b92724 - hopefully fix kbd control translation issues (3 months ago)
* 90b3f354 - Add hotkeys for changing sizes (1x ... 6x). (3 months ago)
* fa77010f - Fix pause toggle keybinding when unmuting. (3 months ago)
* 8cb42003 - Merge pull request #56 from webgeek1234/master (3 months ago)
* 8b8efa1b - fix key shortcuts for save/load states (3 months ago)
* 9fa20b49 - joystick cleanups #444 (3 months ago)
* 684b1bb7 - fix mem error for config global `homeDir` (3 months ago)
* ccc91fa0 - quick fix for unicode filenames on windows (3 months ago)
* dfa4fec3 - Fix missing shortcuts from key bindings. (4 months ago)
* 69b581f7 - Handle key{bindings,shortcuts} correctly on frame. (4 months ago)
* cec1dbfd - Split configuration for default keys. (4 months ago)
* d655031a - Fix removing multiple system keys shortcuts. (4 months ago)
* 996d5b9d - Do not set these options on `MainMenu.xrc`. (4 months ago)
* 019ab5a7 - Fix "Joybus host invalid; disabling" warning. (3 months ago)
* f6bfe676 - Have better support for maximized window. (4 months ago)
* dcc0afa0 - Fix window size when {en,dis}abling status bar. (4 months ago)
* bf6f2d4a - Change where the config dir is created. (4 months ago)
* 3490620e - travis: use Mac OS 10.14 and Xcode 10.2.1 (4 months ago)
* b0ec8469 - fix stuck keys on ubuntu and on focus loss #142 (4 months ago)
* 3d57750c - Follow-up work on #142. (4 months ago)
* 055b7c4d - libretro: jni: Switch to clang compiler in preparation for ndk r20 (4 months ago)
* 2e1333e1 - regparm is an x86 only attribute, only define it there (4 months ago)
* 500506cb - travis: disable link/SFML for mac (4 months ago)
* 22307de6 - Fix continuous sprinting when using a modifier key as a mapping key (#142). (4 months ago)
* 50459712 - wx: only rename config if new file doesn't exist (5 months ago)
* 0a95486f - Sound.cpp: Update (5 months ago)
* a4eae802 - cmake: use LEGACY OpenGL on FreeBSD only 2fedaa63 (5 months ago)
* 240257a0 - wx: Change max volume from 400 to 200 (5 months ago)
* 57ed465e - builder: better mac automation (5 months ago)
* 15f040f3 - cmake: missing space in wx util function (5 months ago)
* 2fedaa63 - support FreeBSD (5 months ago)
* a0c375b6 - Merge remote-tracking branch 'vbam-libretro/master' (6 months ago)
* c013d499 - Merge pull request #55 from Classicmods/master (6 months ago)
* a8b38681 - remove some old visual studio project files (6 months ago)
* b6bb24e0 - Merge pull request #1 from libretro/master (6 months ago)
* 5012969a - Bump version to 2.1.1 (7 months ago)
* 755f4767 - Fix #52 (8 months ago)
* f87e0850 - added a8a35 (7 months ago)
* 286c8ff4 - Fix #52 (8 months ago)

## [2.1.3] Windows Geometry Fix
=======================
* ac35e37c - fix game geometry on windows (4 hours ago)
* 9fa544d1 - cmake: wxWidgets Release config for visual studio (4 hours ago)
* 3cacb363 - libretro: put " " between version and git sha (25 hours ago)
* c2656f13 - libretro: remove "-" between version and git sha (26 hours ago)
* 4203bb53 - libretro: set version with git sha (27 hours ago)
* 8abbb070 - cmake: use Win32Deps when mingw cross compiling (28 hours ago)
* ffc93092 - builder: freetype 2.9.1 -> 2.10.0, verbose make (28 hours ago)
* d08dd6cd - builder: opensuse update 2 (2 days ago)
* 9b38a384 - builder: updates for opensuse, refactor cross deps (2 days ago)
* d28fd302 - cmake: only link SetupAPI on win32 if it's found (2 days ago)
* af63a119 - cmake: add -lSetupAPI to SDL2 libs for win32 (3 days ago)
* 04c77a26 - update win32 dependencies submodule (4 days ago)
* 5dfb36ad - remove .clang-format, update .travis.yml (4 days ago)
* ed16d625 - add unistd.h compat header for visual studio (4 days ago)
* 5019a201 - [SDL Front] MSVC doesn't have an unistd.h system file, closest is io.h, this will fix building the SDL frontend with msvc again. (7 days ago)
* 2ae72f38 - fix starting game pos and geometry #406 (8 days ago)
* 2beb5618 - cmake: move wx funcs and macros into separate file (9 days ago)
* 468fe266 - fix lang in changelog, add bios to issue template (11 days ago)

## [2.1.2] Analog stick fix
=======================
* 95433f6c - release v2.1.2 (81 minutes ago)
* 5b9d1a71 - cmake: improve finding wx utils (19 hours ago)
* 836b74a1 - remove DOS line-ends from src/wx/xrc/*.xrc (2 days ago)
* 49205bf2 - remove unused "multithread filter" menu item (2 days ago)
* 331d9d33 - Fix compilation warnings for MacOS build. (3 days ago)
* 5540790f - fix some compilation warnings (12 days ago)
* d9197281 - Fix for resizing window geometry when loading games. (3 days ago)
* 64a9c094 - fix analog stick regression from e57beed8 #400 (5 days ago)
* 0f0d2400 - more specific check for 3.1.2 xrc error (5 days ago)
* d9a7df61 - fix if statement in SDL sound driver #396 (5 days ago)
* 41ee35f2 - add *.dll to .gitignore (5 days ago)
* 6ec46678 - installdeps: centos: do not install wx 2.8 (9 days ago)
* 2097b5aa - wx 2.8 compat fixes, centos support, cmake fixes (9 days ago)
* b69fced7 - cmake: refactor, better clang support (11 days ago)
* 7fb27c4d - cmake: check for broken LTO (11 days ago)
* f2e9dc55 - cmake: check for policy CMP0077 existence (11 days ago)
* ae38a70d - cmake: gcc/clang colors with ninja, fix warnings (12 days ago)
* 23fe13d8 - cmake: fix syntax error in Win32Deps.cmake (2 weeks ago)
* 59e9c690 - reset Xorg screensaver on joy events (2 weeks ago)
* a0283ead - fix game panel size on wxGTK #325 (2 weeks ago)
* f1ecd7c3 - auto deps for visual studio, take 1 (2 weeks ago)
* 3da07f40 - detect llvm toolchain utilities #392 (3 weeks ago)
* c714ff82 - fix problems of command line parameters (3 weeks ago)
* a1f0c34a - XDG followup work #94 (3 weeks ago)
* 2142a46d - Revert "travis: try re-enabling binary check" (3 weeks ago)
* 101fac59 - travis: try re-enabling binary sanity check (3 weeks ago)
* 6ca59412 - travis: disable mxe pkg key (3 weeks ago)
* a57e51f6 - Return non-error code for given command line parameters. (3 weeks ago)
* 3fd444da - XDG related cleanups #94 (3 weeks ago)
* 513b0559 - minor travis update for mingw jobs (4 weeks ago)
* 1ebb8efd - Fix to set correctly position Y. (4 weeks ago)
* 944c263e - auto save/load geometry for wx GUI #94 (4 weeks ago)
* 36fbf715 - travis update attempt 5 (4 weeks ago)
* 77bcbbf4 - travis update attempt 4 (4 weeks ago)
* 7b3a3a0c - travis update attempt 3 (4 weeks ago)
* 8f0a578a - travis update attempt 2 (4 weeks ago)
* dbd1d8c1 - attempt to re-enable travis mingw slaves (4 weeks ago)
* 530af140 - rpi plugins fix + paths enhancements #94 (4 weeks ago)
* 6a98f3c2 - define S_ISDIR for win32 #94 (4 weeks ago)
* 7373da15 - Fix segmentation fault issue when using WX port command line on Linux and MacOS. (4 weeks ago)
* 36453885 - XDG Base Dir Spec followup #383 (4 weeks ago)
* 39fd3f65 - add note to README.md about translations (5 weeks ago)
* 24fd4e91 - move factory reset to help menu (5 weeks ago)
* 5835dafe - translate battery save error #318 (5 weeks ago)
* d2922cfc - stop showing "wrote battery" msg on panel #318 (5 weeks ago)
* 646557e2 - Add support for the XDG Base Dir Spec for Linux with legacy support. (5 weeks ago)

## [2.1.1] Various fixes to core and GUI
=======================
* 286d7ee9 - builder: fix 32bit mac build (16 hours ago) <Rafael Kitover>
* eaa9b6c5 - Map Viewer: Fix crash when running  a gb/gbc game, (26 hours ago) <retro-wertz>
* 91ee8cad - installdeps: check gtk3-classic on arch/manjaro (2 days ago) <Rafael Kitover>
* 98cb298e - GB: fix 32/64 bit save/state incompatibility (2 days ago) <Rafael Kitover>
* b9d6f35f - add build32/ to .gitignore (for 32bit builds) (3 days ago) <Rafael Kitover>
* 6e76fcef - cmake: fix finding 32 bit wxWidgets on gentoo (3 days ago) <Rafael Kitover>
* 0674b41b - cmake: remove <INCLUDES> from nasm definition (4 days ago) <Rafael Kitover>
* 7dda5809 - cmake: do not use -fPIC on 32 bit x86, breaks asm (4 days ago) <Rafael Kitover>
* 3c28a189 - installdeps: support -m32 builds on opensuse (4 days ago) <Rafael Kitover>
* d7cf15e0 - implement factory reset option #368 (5 days ago) <Rafael Kitover>
* 12fa61af - cmake: refactor FindSSP.cmake (9 days ago) <Rafael Kitover>
* 5a77d8f4 - cmake: don't use ccache on msys2+ninja (11 days ago) <Rafael Kitover>
* ed29b9c4 - Merge pull request #364 from laqieer/master (3 weeks ago) <Zach Bacon>
* 7b350c09 - bugfix: crash when loading elf (3 weeks ago) <laqieer>
* eb6dfb4b - fix libretro build broken in 16dd5d40 #339 (3 weeks ago) <Rafael Kitover>
* 16dd5d40 - make speedup/turbo configurable + misc #339 (3 weeks ago) <Rafael Kitover>
* 5379708f - I guess I'll try the gtk2 build of wxwidgets instead (3 weeks ago) <ZachBacon>
* d70dd373 - Let's use the proper wxwidgets package (3 weeks ago) <ZachBacon>
* ede6b371 - Fix snap deps (3 weeks ago) <ZachBacon>
* 44208c82 - Add basic snapcraft yaml for building a snap (3 weeks ago) <ZachBacon>
* ba678f4f - GB: Make gbTimerOn an INT type instead of BOOL (4 weeks ago) <retro-wertz>
* 43647d32 - GB: Prevent gbSpritesTicks from going out-of-bounds (4 weeks ago) <retro-wertz>
* f8c69531 - fix drawing panel alignment in frame #325 (4 weeks ago) <Rafael Kitover>
* c6fa7246 - cmake: use color gcc/clang output when possible (4 weeks ago) <Rafael Kitover>
* e912c359 - GBA: Remove some magic numbers for main pointers and save types size (4 weeks ago) <retro-wertz>
* 06979221 - Update libretro.cpp (4 weeks ago) <retro-wertz>
* 4700a2c1 - libretro: Enable mirroring for classic/famicom games for GBA and update (4 weeks ago) <retro-wertz>
* f2b34962 - GB: Add missing battery save for MMM01 cart (4 weeks ago) <retro-wertz>
* a0cec107 - Update GBA save type detection and cleanup... (4 weeks ago) <retro-wertz>
* 2a796d48 - libretro: Add GB color palettes (4 weeks ago) <retro-wertz>
* 4f900311 - persist chosen audio device in config file #353 (5 weeks ago) <Rafael Kitover>
* d94d6d53 - osx builder: add -stdlib=libc++ to CFLAGS/LDFLAGS (5 weeks ago) <Rafael Kitover>
* 3eb591ca - Update wxwidgets to 3.1.2 (5 weeks ago) <ZachBacon>
* 3b87576e - GB: Fix rumble support (MBC5) - Fix missing call to rumble function on MBC5 - fix rumble flag gets disabled causing rumble not to work at all. (5 weeks ago) <retro-wertz>
* 089d7a40 - libretro: Add support for tilt, gyro sensors and rumble pak (WIP) - Uses analog stick to simulate tilt and gyro hw. By default, tilt uses the right analog stick while gyro uses the left. The analog stick can be swapped using a core option provided and with separate sensitivity level for both sensors. WIP and will be fine tuned later (Kirby was fun to play at least) - Minor retro_run() cleanup and some minor stuff i forgot. (5 weeks ago) <retro-wertz>
* 6330555c - Merge pull request #350 from retro-wertz/libretro (5 weeks ago) <Zach Bacon>
* a2b3dd76 - libretro: Update input descriptors for 4-player SGB and cleanup... - Updates descriptors for 4-player SGB - Remove alternate gamepad layouts for GBA - Prevent crash when SGB border option executes at startup when GB is not initialized yet - Update input turbo function for 4-player support - Minor cleanups (texts, style nits, etc)... (5 weeks ago) <retro-wertz>
* 9d058abb - libretro: don't include getopt.h in configmanager (6 weeks ago) <Rafael Kitover>
* d5642fa3 - libretro: Android buildfix (#348) (6 weeks ago) <retro-wertz>
* 093818a1 - GBA: Resolve shifting negative value issue in some thumb/arm opcodes (6 weeks ago) <retro-wertz>
* 59f76d05 - libretro: Use gbWram[] for $C000 in CGB mode (6 weeks ago) <retro-wertz>
* f9efb79a - libretro: Fix GB games that uses serial (WIP) (6 weeks ago) <retro-wertz>
* af3fe018 - libretro: Update GB's memory map, expose all usuable ram (6 weeks ago) <retro-wertz>
* 470d86f5 - libretro: Cleanup (6 weeks ago) <retro-wertz>
* ad432a6f - libretro: Silence warning (6 weeks ago) <retro-wertz>
* bff08eaf - libretro: Update Makefile, fix ASAN (6 weeks ago) <retro-wertz>
* 8628db13 - Revert faudio inclusion, causing builder to fail because I didn't properly hook up the build instructions, will try and fix later (7 weeks ago) <ZachBacon>
* c2b31635 - GBA: Only use eepromReset/flashReset during reset event (CPUReset) (7 weeks ago) <retro-wertz>
* 0d73da01 - GBA: Get rid of blip_time() (7 weeks ago) <retro-wertz>
* 83b3ebd7 - fix audio api radio buttons (7 weeks ago) <Rafael Kitover>
* 327611b7 - installdeps: add gcc-libgfortran to msys2 deps (7 weeks ago) <Rafael Kitover>
* f6ad9a8c - remove bad hardcoded keybinds #298 #334 (7 weeks ago) <Rafael Kitover>
* 6462ce59 - pull transifex updates (8 weeks ago) <Rafael Kitover>
* ab3d9236 - add vim undo files to .gitignore (8 weeks ago) <Rafael Kitover>
* a7773bc9 - Bump FAudio to 19.01 (8 weeks ago) <ZachBacon>
* de0e8d6b - cmake: support libasan/-fsanitize (8 weeks ago) <Rafael Kitover>
* ff2d31bf - faudio: minor change (8 weeks ago) <Rafael Kitover>
* 964f086b - fix audioapi opt enum, reorder xrc (8 weeks ago) <Rafael Kitover>
* 8cb3f5a7 - fix sound api config on linux/mac (8 weeks ago) <Rafael Kitover>
* 55a60e3e - only block key event propagation for game keys #88 (8 weeks ago) <Rafael Kitover>
* f8b5627b - fix support for old SDL versions (9 weeks ago) <Rafael Kitover>
* e57beed8 - ignore depressed gamepad triggers #88 (9 weeks ago) <Rafael Kitover>
* 979ef8eb - cmake: fix building without FAudio (9 weeks ago) <Rafael Kitover>
* a91f0664 - disable travis mingw jobs for now (9 weeks ago) <Rafael Kitover>
* edf2c0c4 - fix xaudio2 when openal is disabled (9 weeks ago) <Rafael Kitover>
* 3ed08e8d - finish connecting new faudio driver (9 weeks ago) <Rafael Kitover>
* 296e8e16 - fix valid sound driver config values + faudio fix (9 weeks ago) <Rafael Kitover>
* 1f4487b8 - faudio: add gui code for selecting driver (9 weeks ago) <Rafael Kitover>
* 39622766 - add some missing faudio initialization code (9 weeks ago) <Rafael Kitover>
* 0c2906d0 - fix SDL sound defaulting code (9 weeks ago) <Rafael Kitover>
* 580a11e3 - Let's not force FAudio just yet (9 weeks ago) <ZachBacon>
* 969046ea - Add faudio to the build script (9 weeks ago) <ZachBacon>
* d6f3fd23 - Finish hooking up FAudio to the rest of the frontend (9 weeks ago) <ZachBacon>
* 539027ca - remove problematic default joy binds #88 (9 weeks ago) <Rafael Kitover>
* 5da48769 - fixed a typo and added faudio, but there's still persisting issues (9 weeks ago) <ZachBacon>
* 514f3556 - Merge pull request #337 from visualboyadvance-m/light-weight (9 weeks ago) <Zach Bacon>
* 429b8ceb - I'm pretty sure some of this is very hacky and needs correcting, but it compiles at least. (9 weeks ago) <ZachBacon>
* 53e16e04 - Need to hook up the effects chain parameters (9 weeks ago) <ZachBacon>
* 8939455b - Next on the list is adding a few more arguments for certain functions (9 weeks ago) <ZachBacon>
* a8c44364 - Next on the list is correcting the incomplete types (9 weeks ago) <ZachBacon>
* 4b664c69 - Still not quite ready for d3d, but this one header mingw has anyways. (9 weeks ago) <ZachBacon>
* 14815135 - we need to release with the proper function in faudio (9 weeks ago) <ZachBacon>
* 0bfbcfa3 - Needed the FAudio Processor in FAudioCreate (9 weeks ago) <ZachBacon>
* af98f532 - Inbound FAudio fixes (9 weeks ago) <ZachBacon>
* 5f38c0da - cmake: static: check for link file when editing (9 weeks ago) <Rafael Kitover>
* ebd2e74a - installdeps: fix for a01deb28: use msys2 ccache (9 weeks ago) <Rafael Kitover>
* a01deb28 - installdeps: also install ccache (9 weeks ago) <Rafael Kitover>
* ba563c71 - Add FAudio to the xrc (9 weeks ago) <ZachBacon>
* 5d7dfa49 - Merge pull request #335 from retro-wertz/fix_crash (9 weeks ago) <Zach Bacon>
* 0c579b20 - Revert to a default audio api (SDL) when config is invalid (9 weeks ago) <retro-wertz>
* 4361c45b - Fixed a few things, still have lots to fix though before it's a usable state. (10 weeks ago) <ZachBacon>
* 99795b27 - cmake hookup is done, there are some issues that I'll be trying to fix within faudio.cpp before it's ready for mainstream (2 months ago) <ZachBacon>
* eab039cd - This should allow faudio to be supported in vba-m, next is to further modify cmake to find faudio (2 months ago) <ZachBacon>
* e00aca18 - Initial work on switching to faudio, WIP (2 months ago) <ZachBacon>
* 0a40ca7a - initial inclusion of stb_image to begin migration from libpng to stb, let's trim some fat (2 months ago) <ZachBacon>
* 0d1b23c5 - Merge pull request #331 from retro-wertz/gba_timings (3 months ago) <Zach Bacon>
* 85891fc7 - Reduce input delay by 1 frame and audio timing fix (3 months ago) <retro-wertz>
* 3cb38420 - builder: add patch for glibc 2.28 compat to m4 (3 months ago) <Rafael Kitover>
* 61b3084e - builder: set host cc for libgpg-error to gcc (3 months ago) <Rafael Kitover>
* 00b04692 - add travis hook for gitter (3 months ago) <Rafael Kitover>
* b60a6343 - Merge pull request #326 from knightsc/tasks/add-lldb-support (3 months ago) <Rafael Kitover>
* 27a874e3 - Merge branch 'master' into tasks/add-lldb-support (3 months ago) <Rafael Kitover>
* a52eddb5 - Handle debugger disconnect and reconnect properly (3 months ago) <Scott Knight>
* 6ba3b779 - Set correct register number in gdb stop reply (3 months ago) <Scott Knight>
* f385fb2f - Update gdb remote query support (3 months ago) <Scott Knight>
* 3b185e23 - builder: libvorbis fix (3 months ago) <Rafael Kitover>
* c68f372e - fix wrong copy-pasta in 36e412df (3 months ago) <Rafael Kitover>
* 16ccad07 - Merge pull request #330 from retro-wertz/libretro_updates (3 months ago) <Zach Bacon>
* 36e412df - builder: mingw: fix libffi for i686 + improvemnts (3 months ago) <Rafael Kitover>
* 5b0f2e8a - builder: msys2: fix links to host binaries (3 months ago) <Rafael Kitover>
* 72760642 - Update ISSUE_TEMPLATE.md (3 months ago) <retro-wertz>
* ca56ccff - libretro: Simplify cheats, add multiline support for GB/GBC (3 months ago) <retro-wertz>
* a2d5c260 - libretro: Add turbo buttons (3 months ago) <retro-wertz>
* 3484ecc4 - Add support for LLDB qHostInfo packet (3 months ago) <Scott Knight>
* dd2a1d9b - Fix stack overflow in remoteMemoryRead (3 months ago) <Scott Knight>
* 4f28e846 - Fix stack overflow in remotePutPacket (3 months ago) <Scott Knight>
* db8aaeca - builder: mingw: build zlib-target after cmake (3 months ago) <Rafael Kitover>
* beaf9340 - builder: bump libxslt 1.1.33-rc1 -> 1.1.33-rc2 (3 months ago) <Rafael Kitover>
* afbe647a - builder: catgets fix for msys2 + minor changes (3 months ago) <Rafael Kitover>
* a6034ddf - builder: disable building openssl tests (3 months ago) <Rafael Kitover>
* 9ebc3fc2 - builder: build mingw dlfcn after cmake (3 months ago) <Rafael Kitover>
* 115fce69 - builder: do not defer env eval for msys2 host hook (3 months ago) <Rafael Kitover>
* b9911a57 - builder: fix regressions from 37869441..a3ec309b (3 months ago) <Rafael Kitover>
* a3ec309b - builder: more minor mingw cross fixes (4 months ago) <Rafael Kitover>
* d725978a - builder: fix openssl parallel make patch (4 months ago) <Rafael Kitover>
* cf3ed8f3 - builder: fix quoting issues introduced in 37869441 (4 months ago) <Rafael Kitover>
* 37869441 - builder: msys2 fixes + misc improvements (4 months ago) <Rafael Kitover>
* 975a1866 - cmake: support linuxbrew mingw toolchain (4 months ago) <Rafael Kitover>
* 453fa0de - add visual studio .vs/ directory to .gitignore (4 months ago) <Rafael Kitover>
* eee4add6 - Add localizations to installer, next will be adding portable mode so users can install to a custom location without the shortcuts being installed (4 months ago) <Zach Bacon>
* 2e5235af - Initial rework of the installer framework, this is very incomplete, but it'll hopefully allow individual selection of translations as well as offer a portable mode installer (4 months ago) <Zach Bacon>
* 6f1df2dd - rename mingw include dir mingw-include in deps (4 months ago) <Rafael Kitover>
* 5e58e4c3 - when it comes to cross compiling, Most unices like linux are case sensitive (4 months ago) <Zach Bacon>
* 26b15b2c - add mingw dependencies/include to include path (4 months ago) <Rafael Kitover>
* 9cb9ce86 - fix Windows XP Compatibility #315 (4 months ago) <Rafael Kitover>
* 1bf51ec1 - builder: 32 bit mingw fixes (4 months ago) <Rafael Kitover>
* ed8c928a - builder: support gentoo crossdev + misc fixes (5 months ago) <Rafael Kitover>
* b60cd332 - Update openal to use github url (5 months ago) <ZachBacon>
* aebda1b7 - debian: update dependency (5 months ago) <retro-wertz>
* 58083d9d - Gonna use universaldxsdk for xaudio (5 months ago) <ZachBacon>
* 721c1b7c - Revert "hopefully fix bin2c for msvc" (5 months ago) <Rafael Kitover>
* 01a75e8e - hopefully fix bin2c for msvc (5 months ago) <Rafael Kitover>
* b9d0f818 - builder: fix ccache on msys2 (5 months ago) <Rafael Kitover>
* be0d49a3 - builder: msys2 fixes (6 months ago) <Rafael Kitover>
* 3aa00bfb - builder: fix libuuid_mingw for mingw cross (6 months ago) <Rafael Kitover>
* 5b5e3193 - builder: don't install cpanm with local::lib (6 months ago) <Rafael Kitover>
* 88f66ef6 - builder: fix building ccache for win targets (6 months ago) <Rafael Kitover>
* d1c82cac - fix typo in builder core (6 months ago) <Rafael Kitover>
* dcd7d5e0 - support 32/64 bit mac builds, build improvements (6 months ago) <Rafael Kitover>
* b4dd06a1 - Merge pull request #302 from retro-wertz/libretro (6 months ago) <Zach Bacon>
* 916c091a - Libretro: Add GB/GBC cheat support... (6 months ago) <retro-wertz>
* 14086d00 - Libretro: Fix crash on some linux systems (6 months ago) <retro-wertz>
* 0e338617 - update translations, add new langs from transifex (7 months ago) <Rafael Kitover>
* e67b513e - rename mac-localizations to mac-translations.cmake (7 months ago) <Rafael Kitover>
* bf4606fc - install translations into mac .app (7 months ago) <Rafael Kitover>
* 0092dc16 - msys2: don't try to link msys librt and libpthread (7 months ago) <Rafael Kitover>
* 65e1ab04 - disable gcc stack protector, segfault on 8.2.0 (7 months ago) <Rafael Kitover>
* 6cbad61f - fix cmake regression introduced in bfe21aee (7 months ago) <Rafael Kitover>
* bfe21aee - remove -fpermissive compiler flag (7 months ago) <Rafael Kitover>
* 571ecbe3 - support mac-hosted mingw builds, misc. fixes (7 months ago) <Rafael Kitover>
* e32e7c5d - installdeps: Add zip to openSuse dependency (7 months ago) <retro-wertz>
* f45935af - Add vbam_libretro.info (7 months ago) <retro-wertz>
* be508eb2 - simplify check for renamed wx-config, fix gentoo (7 months ago) <Rafael Kitover>
* 3b44a299 - cmake: fix wrong unset syntax #295 from f78d45c0 (7 months ago) <Rafael Kitover>
* 56443391 - installdeps gentoo: don't eselect wxwidgets (7 months ago) <Rafael Kitover>
* 7a054b45 - installdeps: support gentoo (7 months ago) <Rafael Kitover>
* f78d45c0 - cmake: fix regression in finding wx from 2efcb620 (7 months ago) <Rafael Kitover>
* 594ecc39 - msys2 builder: redo fontconfig patch, bumb wx (7 months ago) <Rafael Kitover>
* 2cece6ac - Updated Translations, finally added transifex support to pull in new translations (7 months ago) <ZachBacon>
* 6bc30101 - update msys2 builder (7 months ago) <Rafael Kitover>
* 14d13153 - Merge pull request #292 from retro-wertz/updates (7 months ago) <Zach Bacon>
* ddea50d3 - GB: Cleanup sound registers (7 months ago) <retro-wertz>
* faf01db2 - GB: Backport STAT register behavior (7 months ago) <retro-wertz>
* d9e0d0f8 - GB: Remove references to gbReadOpcode (7 months ago) <retrowertz>
* eb20bb4a - We don't have a forum anymore (7 months ago) <Zach Bacon>
* fca7e175 - Libretro: Prevent crash when loading an incompatible state file (7 months ago) <retro-wertz>
* 1289e08c - Libretro: Enable battery save ram support for MBC2 and MBC7 (7 months ago) <retro-wertz>
* a9ab09f7 - Libretro: Fix realtime clock not updating in GB/GBC... (7 months ago) <retro-wertz>
* 6cda6c0c - Libretro: Show basic details in log window during rom loading (7 months ago) <retro-wertz>
* de25e9d7 - include zip for arch based systems (7 months ago) <Zach Bacon>
* 5016fd6c - Merge pull request #286 from retro-wertz/libretro_gb (7 months ago) <Zach Bacon>
* 6ef938fc - Libretro: Add memory descriptors for GB/GBC (7 months ago) <retro-wertz>
* bb64e8d8 - Libretro: Use retro_get_memory_data/size for battery-enabled roms (7 months ago) <retro-wertz>
* 119e1f5c - Libretro: Add core options for GB border and hardware overrides (7 months ago) <retro-wertz>
* 76ad84fd - Opps, accidentally broke borders in standalone (7 months ago) <retro-wertz>
* bf447bf8 - Libretro: Add GB/GBC core (7 months ago) <retro-wertz>
* f05a05e6 - Libretro: Refactoring for adding GB/GBC core (8 months ago) <retro-wertz>
* 0e60c34a - Fix this (8 months ago) <retro-wertz>
* 52f5a02b - fix installdeps for Ubuntu 18 (8 months ago) <Rafael Kitover>
* cc43db35 - fix installdeps for Ubuntu (8 months ago) <Rafael Kitover>
* 3f903cf0 - Merge pull request #278 from retro-wertz/patch-4 (8 months ago) <Zach Bacon>
* 02e5f0bd - Libretro: Bump version number (8 months ago) <retro-wertz>
* fc42f88b - GB: Fix SIO related issue (8 months ago) <retro-wertz>
* a8d0508c - use GetWindow()->Refresh() in Wayland only (8 months ago) <Rafael Kitover>
* 459a1fbe - builder: fix ccache, mingw-cross (8 months ago) <Rafael Kitover>
* f937aa72 - builder: disable ccache for openssl (8 months ago) <Rafael Kitover>

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


