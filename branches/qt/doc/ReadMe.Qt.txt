 _   _ _                 _______                ___      _                                 ___  ___
| | | (_)               | | ___ \              / _ \    | |                                |  \/  |
| | | |_ ___ _   _  __ _| | |_/ / ___  _   _  / /_\ \ __| |_   ____ _ _ __   ___ ___ ______| .  . |
| | | | / __| | | |/ _` | | ___ \/ _ \| | | | |  _  |/ _` \ \ / / _` | '_ \ / __/ _ \______| |\/| |
\ \_/ / \__ \ |_| | (_| | | |_/ / (_) | |_| | | | | | (_| |\ V / (_| | | | | (_|  __/      | |  | |
 \___/|_|___/\__,_|\__,_|_\____/ \___/ \__, | \_| |_/\__,_| \_/ \__,_|_| |_|\___\___|      \_|  |_/
                                        __/ |                                                      
==== Qt ReadMe ====                    |___/

VisualBoyAdvance-M (VBA-M) is a Game Boy and Game Boy Advance emulator
for Windows, Linux and Mac OS
using the Qt Cross-Platform Application Framework by Trolltech
(Further informations about Qt at http://trolltech.com/products/qt/)

Links:
Homepage:    http://vba-m.ngemu.com/
Forum:       http://vba-m.ngemu.com/forum/
SourceForge: https://sourceforge.net/projects/vbam/


VBA-M is based on the official VisualBoyAdvance emulator by Forgotten.
Original VBA project homepage: http://vba.ngemu.com/


This build uses icons from the KDE project (http://kde.org/)
svn://anonsvn.kde.org/home/kde/trunk/KDE/kdebase/runtime/pics/oxygen

This program is distributed under the GNU General Public License v2
http://www.gnu.org/licenses/gpl.html


=============================
Required Runtimes for Windows
=============================

There are two common compilers for Windows: MinGW and Visual C++

When the executable was compiled with MinGW, you need the MinGW C runtime:
http://sourceforge.net/project/showfiles.php?group_id=2435&package_id=11598
Simply extract the dll from the bin folder to either your Windows/system32 folder or to the exe's folder.

When the executable was compiled with Visual C++, you need the Microsoft C runtime:
Microsoft Visual C++ 2008 SP1 Redistributable Package (x86):
http://www.microsoft.com/downloads/details.aspx?FamilyID=a5c84275-3b97-4ab7-a40d-3802b2af5fc2&DisplayLang=en

Qt 4.4.1 Runtime:
http://vba-m.ngemu.com/vbam/vbacompiles/msvc2008sp1/qt/qt441.7z
[Qt build]


====================
Currently Known Bugs
====================
Known Bugs:

- Audio core: assertation error occurs when disabling sound in GB mode
  - I think its best we mute sound instead, since some games rely on audio for timing.
    Plus, blargg's GB_Snd_Emu is extremely optimized stuff. (Mudlord)
  - blargg's core implementation broke Dwedit's GBC emu.


=======
Credits
=======

VisualBoy and VisualBoyAdvance Developers:
Forgotten
pokemonhacker
kxu

VisualBoyAdvance-M Developers:
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
