VisualBoyAdvance S1.7.6
Nintendo Game Boy / Game Boy Advance Emulator

This program is distributed under the GNU General Public License
http://www.gnu.org/licenses/gpl.html

VBA Official Version 1.7.2 with changes by Spacy
Spacy51@gmx.de (Write in english or in german)
Special Build Aturhors Homepage: www.spacyhacks.de.vu
Original Project Homepage: vba.ngemu.com

My aim:
I want to make this emulator fit my needs and hopefully the needs of others,
which want a light-weight small, fast and multimedial emulator for the GBA
that makes as much use of the power of modern PCs as possible (but with sense).
If you need one of the removed features, just use the original VBA emulator.

Thanks go to:
suanyuan    For help in compilation and other fixes
Tauwasser   For help in assembler
WingX       For fixing a linker error


The following changes have been made:

S1.7.6:
Emu:
- Readded MMX macro
- Updated zlib to 1.2.3
- Changed some first start options
- Other small changes
- Put zlib & libpng in seperate Projects
- Added some changes from the latest CVS source
- Small changes to ROM Header Info (just4fun)
- Fixed the linker error (new&delete defined twice)

Filters:
- Speeded up HQ3X code
- Fixed LQ2X using HQ2X functions

Display:
- Added extended display mode selection
  (Display Adapter, Resolution, Bit Depth, Frequency)
- No more unnecessary black borders in full screen
- Direct3D doesn't take the whole screen (only if you want)
- Direct3D shows menu and windows correct
- Direct3D doesn't show a black screen if left fullscreen to Windows
- Changes on max scale are applied immediately

Sound:
- Updated sound to DirectSound8


S1.7.5:
- Removed screen flickering when switching to GDI mode.
- Changed some first start options.
- Rearranged Menu
- Added HQ3X in 32 bit mode
- Changed App Icon
- Added FINAL_VERSION definition again.
- Added 3x/4x filter support to OpenGL mode
- Some minor fixes


S1.7.4:
- optimized build: (many thanks to suanyuan)
	- libpng, zlib, MFC linked static
	- Target OS: Windows 2000
- Keep in mind that HQ3X/HQ4X is NOT added
  at the moment, but everything is ready for it


S1.7.3:
- Optimized build and project file
	- Removed Skin support
	- Removed SDL support
	- Removed Linux support
	- Removed Motion Blur Experimental Filter (the none-IFB version)
- Reworked GDI
	- 3x / 4x filter support
	- Fullscreen modes available