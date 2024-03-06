 _   _ _                 _______                ___      _                                 ___  ___
| | | (_)               | | ___ \              / _ \    | |                                |  \/  |
| | | |_ ___ _   _  __ _| | |_/ / ___  _   _  / /_\ \ __| |_   ____ _ _ __   ___ ___ ______| .  . |
| | | | / __| | | |/ _` | | ___ \/ _ \| | | | |  _  |/ _` \ \ / / _` | '_ \ / __/ _ \______| |\/| |
\ \_/ / \__ \ |_| | (_| | | |_/ / (_) | |_| | | | | | (_| |\ V / (_| | | | | (_|  __/      | |  | |
 \___/|_|___/\__,_|\__,_|_\____/ \___/ \__, | \_| |_/\__,_| \_/ \__,_|_| |_|\___\___|      \_|  |_/
                                        __/ |                                                      
                                       |___/   

VisualBoyAdvance-M
Nintendo Game Boy & Game Boy Advance Emulator
Project Homepage: http://vba-m.com

This program is distributed under the GNU General Public License
http://www.gnu.org/licenses/gpl.html



Based on the official VisualBoyAdvance by Forgotten & the VBA development team
Original Project Homepage: vba.ngemu.com


===================
System Requirements
===================

OS: Windows 2000 - Vista (x86 or x64)
CPU: min. 700 MHz for GBA emulation, CPU requirements increase if filters or other improvements are enabled
RAM: ~64MB free
GFX: ~32MB VRAM, latest drivers supporting DirectX9
SND: Anything with working Windows drivers


===========================
Required Runtimes and components
===========================
This is a list of runtimes that are NOT included with any release version of windows to date (Windows 8 may contain newer D3DX components released since Windows 7's release).


D3DX Components require the DirectX End-User Runtime Installer.
http://www.microsoft.com/en-us/download/details.aspx?id=35

MSVC 2010 SP1 Runtime
(x86) http://www.microsoft.com/en-us/download/details.aspx?id=8328

(x64) http://www.microsoft.com/en-us/download/details.aspx?id=13523  (x64 versions aren't really a requirement since there are no supported 64bit versions of the emulator)

zLib
http://www.zlib.net/zlib123-dll.zip

=================
Optional Runtimes (Not required if you have a Creative Sound Card)
=================

OpenAL Installer for Windows:
http://connect.creativelabs.com/openal/Downloads/Forms/DispForm.aspx?ID=1&Source=http%3A%2F%2Fconnect.creativelabs.com%2Fopenal%2FDownloads%2FForms%2FAllItems. aspx&RootFolder=%2Fopenal%2FDownloads

=================
Optional Filters
=================
check the sourceforge project page.

==============
To Do List
==============
Important:
- Many games show emulation warnings in the log window (unaligned read, bad read/write address)
- Test: Metroid Fusion, Advance Wars 2

- Gfx.cpp/h optimization
- Test: Final Fantasy 4 airship intro / Pokemon Fire Red

- Improve automatic 64k/128k flash save detection
- Pokémon Emerald hangs with white screen when no save state exists and wrong flash size is selected (Flash 128)

- Remove 16 bit hack for filters
- Not compatible to software motion blur (display corruption)

- Add selection for compressed archives with more than one ROM in them

- Game Bugs:
- Drymouth - screen flashes black after certain scanline

Less important:
- Add GBA cheat editing support (GB already has)
- Look at Cheats.cpp (Core) and GBACheats.cpp (GUI)

- Add documentation for VBA-M (configuration guide)

- Improve AVI recording (produces asynchronous files)

- Enable audio stream compression for AVI files

- Add stereo upmixing support to OpenAL

- Verify BIOS files by checksum instead by file extension

- Merge HQ2x/LQ2x C code into code for HQ3x/4x

- Apply pixel filter to sprites and BG separately for better image quality

- Add CGB Bios support

Performance:
- Apply HQ3x/4x optimizations from C version to ASM version

- Apply pixel filter only to changed parts of the image

- Make even more use of multi-core CPUs

- Make use of 64 bit CPUs

- Have a look at the liboil optimization library
- http://liboil.freedesktop.org/wiki/


==========
Known Bugs
==========
- Linking: Doesnt work quite right yet.

- blargg's core implementation broke Dwedit's GBC emu.
- Audio Sync + Auto frame skip break frames, causing emulation to drop to as low as 40fps even though enough performance is available to maintain full speed. Use one, or the other, not both. Vsync is not effected, and can be used with either setting.

- Wrong bit depth image is displayed for 2 frames when switching from/to HQ3x/4x ASM
- This is caused by the 16bit hack which does not re-process the emulated image.
It results in the display devices treating the image at pix with the wrong bit depth.


=======
Credits
=======

Mudlord
Nach
Squall Leonhart
Spacy
DJRobX
Jonas Quinn
kode54
Normmatt

==============
Special Thanks
==============

Forgotten & the VBA development team:
- For creating VisualBoyAdvance
- Couldn't you have written cleaner code???

chrono:
- Fixed a bug in the HQ3x/4x filters
- Made HQ3x/4x and Bilinear filters (ASM versions) thread-safe
- Many other fixes....

bgKu:
- GTK GUI port
- Various assistance with Linux issues

xKiv
- Assistance with Linux SDL port

blargg:
- Assistance with the implementation of his highly accurate GB audio core
- Implemented his unreleased File_Extractor library
- Cleanup of the audio core interface

kode64:
- Implemented LZMA2 in FEX
- GB_APU tweaks and fixes
- cmake corrections

shuffle2:
- SFML library
- various build fixes

Squarepusher
- Libretro merge
