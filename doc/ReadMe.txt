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
New Project Homepage: vba-m.ngemu.com

The Qt build uses icons from the KDE project:
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

OS:  Windows 2000 - Vista (x86 or x64), Linux
CPU: 700MHz minimum for GBA emulation, CPU requirements increase if filters are to be used.
RAM: ~64MB free
GFX: ~32MB VRAM, DirectX9 drivers


===================
File Dependencies
===================

"D3DX9_37.DLL" requires an update to DirectX.
You can use the following tool to update it:
http://vba-m.ngemu.com/vbam/vbacompiles/msvc2008/Directx_9c_webupdater.zip

In order to use OpenAL sound, the OpenAL runtime has to be downloaded and installed from http://openal.org/downloads.html


==============
To Do List
==============
Important:
- Many games show emulation warnings in the log window (unaligned read, bad read/write address)
 - Test: Metroid Fusion, Advance Wars 2

- Gfx.cpp/h optimization
 - Test: Final Fantasy 4 airship intro

- Improve automatic 64k/128k flash save detection

- Add support for byuu's UPS patching format to replace IPS

- Remove 16 bit hack for filters
 - Not compatible to software motion blur (display corruption)

- Add selection for compressed archives with more than one ROM in them

- Finalise Qt4 GUI system

- Fix LCD colouring

- Game Bugs:
* Drymouth - screen flashes black after certain scanline
* World Reborn - 2 graphics bugs



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

- Add device enumeration & buffer count selection for XAudio2
  - Current buffer count: 4



Performance:
- Apply HQ3x/4x optimizations from C version to ASM version

- Apply pixel filter only to changed parts of the image

- Make even more use of multi-core CPUs

- Make use of 64 bit CPUs

- Have a look at the liboil optimization library
  - http://liboil.freedesktop.org/wiki/


====================
Currently Known Bugs
====================
Known Bugs:

- Linking: Doesnt work quite right yet.
- Audio core: assertation error occurs when disabling sound in GB mode
  - I think its best we mute sound instead, since some games rely on audio for timing.
    Plus, blargg's GB_Snd_Emu is extremely optimized stuff. (Mudlord)
  - blargg's core implementation broke Dwedit's GBC emu.
- Wrong bit depth image is displayed for 2 frames when switching from/to HQ3x/4x ASM
 - This is caused by the 16bit hack which does not re-process the emulated image.
   It results in the display devices treating the image at pix with the wrong bit depth.


=======
Credits
=======

VisualBoy and VisualBoy Advance Developers:
Forgotten

VisualBoy Advance-M Developers:
Mudlord
DJRobX
Nach
Jonas Quinn
Spacy


==============
Special Thanks
==============

chrono:
- Fixed a bug in the HQ3x/4x filters
- Made HQ3x/4x and Bilinear filters (ASM versions) thread-safe

bgKu:
- GTK GUI port
- Various assistance with Linux issues

blargg:
- Assistance with the implementation of his highly accurate GB audio core
- Implemented his unreleased File_Extractor library
