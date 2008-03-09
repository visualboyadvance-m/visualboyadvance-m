 _   _ _                 _______                ___      _                                 ___  ___
| | | (_)               | | ___ \              / _ \    | |                                |  \/  |
| | | |_ ___ _   _  __ _| | |_/ / ___  _   _  / /_\ \ __| |_   ____ _ _ __   ___ ___ ______| .  . |
| | | | / __| | | |/ _` | | ___ \/ _ \| | | | |  _  |/ _` \ \ / / _` | '_ \ / __/ _ \______| |\/| |
\ \_/ / \__ \ |_| | (_| | | |_/ / (_) | |_| | | | | | (_| |\ V / (_| | | | | (_|  __/      | |  | |
 \___/|_|___/\__,_|\__,_|_\____/ \___/ \__, | \_| |_/\__,_| \_/ \__,_|_| |_|\___\___|      \_|  |_/
                                        __/ |                                                      
                                       |___/   

VisualBoyAdvance-M
Nintendo Game Boy / Game Boy Advance Emulator

Based on official VBA
Original Project Homepage: vba.ngemu.com
New Project Homepage: vbam.ngemu.com

The Qt build uses Icons from the KDE project:
http://kde.org/
svn://anonsvn.kde.org/home/kde/trunk/KDE/kdebase/runtime/pics/oxygen

This program is distributed under the GNU General Public License
http://www.gnu.org/licenses/gpl.html


Aim:
Combine the best features of the VBA forks in one build.
Multi-platform support.


===================
System Requirements
===================

OS:  Windows 2000 - Vista (x86 or x64)
CPU: 700MHz minimum for GBA emulation, CPU requirements increase if Filters are to be used.
RAM: ~64MB free
GFX: ~32MB VRAM, DirectX9 drivers


===================
File Dependencies
===================

VBA-M Requires the following files, in order to run.

libpng13.dll
zlib1.dll
D3DX9_37.DLL

The first two are available from http://vba-m.ngemu.com/vbam/vbacompiles/msvc2008/dll.7z,
While the last requires an update to DirectX, you can use the following tool to update - http://vba-m.ngemu.com/vbam/vbacompiles/msvc2008/Directx_9c_webupdater.zip


==============
To Do List
==============
Important:
- Many games show emulation warnings in the log window (unaligned read, bad read/write address)
 - Test: Metroid Fusion, Advance Wars 2

- Gfx.cpp/h optimization
 - Test: Final Fantasy 4 airship intro

- Improve automatic 64k/128k flash save detection

- HQ3x/4x ASM implementation produces wrong interpolation on the image's border
 - This has already been fixed in the C version; look at hq_base.h / line 343 - 372. The ASM version most likely only has something like skipLine instead of skipLinePlus and skipLineMinus, which is however necessary in order to work correctly.

- Fix OpenGL issues

- Remove 16 bit hack for filters
 - Not compatible to software motion blur (display corruption)

- Add selection for compressed archives with more than one ROM in them



Less important:
- Add GBA cheat editing support (GB already has)
 - Look at Cheats.cpp (Core) and GBACheats.cpp (GUI)

- Support D3DFMT_A2R10G10B10 (10 bit per color component) color format

- Add documentation for VBA-M (configuration guide)

- Improve AVI recording (produces asynchronous files)

- Enable audio stream compression for AVI files

- Add stereo upmixing support to OpenAL

- Verify BIOS files by checksum instead by file extension

- Merge HQ2x/LQ2x C code into code for HQ3x/4x

- Apply pixel filter to sprites and BG seperately for better image quality

- Create Visual Studio project using SDL makefile



Performance:
- Apply HQ3x/4x optimizations from C version to ASM version

- Apply pixel filter only to changed parts of the image

- Make use of multi-core CPUs

- Make use of 64 bit CPUs


====================
Currently Known Bugs
====================
Known Bugs:

- Linking: Doesnt work quite right yet.
- OpenGL: Custom fragment/vertex shaders do not work quite as well as original test build (Mudlord)
- OpenGL: Custom fullscreen resolutions will not work when filters are enabled. 
  - Since other fullscreen OGL reses are disabled, I should disable custom OGL resolution
    switching (Mudlord).
- OpenGL: Various context handling issues with OGL mode. The emulator should restart whenever is
  enabled, thus eliminating these issues when switching graphics APIs. I don't why this didnt happen
  in the first place, when it should. (Mudlord)
- Audio core: assertation error occurs when disabling GB sound
  - I think its best we mute sound instead, since some games rely on audio for timing.
    Plus, blargg's GB_Snd_Emu is extremely optimized stuff. (Mudlord)
- Wrong bit depth image is displayed for 2 frames when switching from/to HQ3x/4x ASM
 - This is caused by the 16bit hack which does not re-process the emulated image.
   It results in the display devices treating the image at pix with the wrong bit depth.


=================
Credits
=================

VisualBoy and VisualBoy Advance Developers:
Forgotten

VisualBoy Advance-M Developers:
Mudlord
DJRobX
Nach
Jonas Quinn
Spacy