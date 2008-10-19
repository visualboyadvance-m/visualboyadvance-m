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
Project Homepage: vba-m.ngemu.com

This program is distributed under the GNU General Public License
http://www.gnu.org/licenses/gpl.html



Based on the official VisualBoyAdvance by Forgotten & the VBA development team
Original Project Homepage: vba.ngemu.com


===================
System Requirements
===================

OS:  Windows 2000 - Vista (x86 or x64)
CPU: min. 700 MHz for GBA emulation, CPU requirements increase if filters or other improvements are enabled
RAM: ~64MB free
GFX: ~32MB VRAM, latest drivers supporting DirectX9
SND: Anything with working Windows drivers


=================
Required Runtimes
=================

DirectX Runtime Web-Updater:
http://www.microsoft.com/downloads/details.aspx?FamilyID=2da43d38-db71-4c1b-bc6a-9b6652cd92a3&DisplayLang=en


=================
Optional Runtimes
=================

OpenAL Installer for Windows:
http://connect.creativelabs.com/openal/Downloads/Forms/DispForm.aspx?ID=1&Source=http%3A%2F%2Fconnect.creativelabs.com%2Fopenal%2FDownloads%2FForms%2FAllItems.aspx&RootFolder=%2Fopenal%2FDownloads


==============
To Do List
==============
Important:
- Many games show emulation warnings in the log window (unaligned read, bad read/write address)
 - Test: Metroid Fusion, Advance Wars 2

- Gfx.cpp/h optimization
 - Test: Final Fantasy 4 airship intro

- Improve automatic 64k/128k flash save detection
 - Pokémon Emerald hangs with white screen when no save state exists and wrong flash size is selected

- Remove 16 bit hack for filters
 - Not compatible to software motion blur (display corruption)

- Add selection for compressed archives with more than one ROM in them

- Fix LCD colouring

- Game Bugs:
 - Drymouth - screen flashes black after certain scanline
 - World Reborn - 2 graphics bugs


Less important:
- Add GBA cheat editing support (GB already has)
 - Look at Cheats.cpp (Core) and GBACheats.cpp (GUI)

- Add documentation for VBA-M (configuration guide)

- Improve AVI recording (produces asynchronous files)

- Enable audio stream compression for AVI files

- Add stereo upmixing support to OpenAL

- Verify BIOS files by checksum instead by file extension

- Merge HQ2x/LQ2x C code into code for HQ3x/4x

- Apply pixel filter to sprites and BG seperately for better image quality

- Add support for byuu's UPS patching format to replace IPS


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

Mudlord
Nach
Squall Leonhart
Spacy
DJRobX
Jonas Quinn


==============
Special Thanks
==============

Forgotten & the VBA development team:
- For creating VisualBoyAdvance
- Couldn't you have written cleaner code???

chrono:
- Fixed a bug in the HQ3x/4x filters
- Made HQ3x/4x and Bilinear filters (ASM versions) thread-safe

bgKu:
- GTK GUI port
- Various assistance with Linux issues

blargg:
- Assistance with the implementation of his highly accurate GB audio core
- Implemented his unreleased File_Extractor library
- Cleanup of the audio core interface
