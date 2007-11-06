Welcome to VisualBoyAdvance version 1.8.0.

Compiling the sources
---------------------

See the INSTALL file for compiling instructions. Please note the following
requisites to compile:

- GCC must be 3.x or greater in order to compile GBA.cpp with -O2. Earlier
  versions have a problem during optimization that requires an absurd
  ammount of memory and usually ends up crashing the compiler/computer
- On Windows, Microsoft Visual C++ 6 or later is needed. Please note that
  some of the source code will not compile with the shipped header files.
  You will need to install the most recent Platform SDK from Microsoft.

Support
-------

Please support VisualBoyAdvance by making a donation. You can donate money
using PayPal (www.paypal.com). Use the contact form to find how you can
send donations. Also, it is recommended that you use the VisualBoyAdvance
forum on www.ngemu.com message board.

Default keys (can be edited in the Options menu)
------------------------------------------------

Arrow keys - direction
Z          - Button A
X          - Button B
A          - Button L
S          - Button R
Enter      - Start
Backspace  - Select
Speedup    - Space
Capture    - F12

You can change the configuration above to use a joystick. Go to
Options->Joypad->Configure... menu.

The 1 thorugh 4 joypads allow you to have different settings which can be
easily switched.

Recommended System Requirements
-------------------------------

CPU: 800MHz with MMX & SSE
RAM: 64MB free
GPU: Graphics card with drivers supporting DirectX9
OS:  Windows 2000 with DirectX9 runtime

Translations
------------

Translations can be done as long as you have Microsoft Visual VC++ on
your computer.

If you just want to use a translation, place the translation .DLL on
the same directory as the emulator. From the Options->Language menu,
select Other... and type the three letter (or two) language name from
.DLL. For example, VBA_PTB.DLL: type PTB on the dialog.

These translation files are only for VisualBoyAdvance GUI and messages.
Games will not be translated and cannot be translated by the emulator.

Skins
-----

Skins consist of a bitmap (.bmp), a region file (.rgn), a draw rectangle
on the region and an INI file.

Once you have the bitmap, you the region creator which can be found at
the downloads section of emulator website along with a sample skin.
This allows for irregular skins with holes or any shape.

Create the INI file like this:
[skin]
image=<relative path from ini to image bitmap>
region=<relative path from ini to image region>
draw=<draw rectangle defined as x,y,width,height separated by commas>
buttons=<number of buttons in the skin> (optional)

Then, for each button with n starting a 0:

[button-<n>]
normal=<relative path to button normal bitmap>
down=<relative path to button pressed bitmap>
over=<relative path to button hover bitmap - mouse over the button> (optional)
id=<id of button action, menu or emulator joypad button - see below)
rect=<rectangle where the button is to be drawn>
region=<region to create a non rectangular button> (optional)

The id member can be one of the values found under Tools->Customize to have an
action button.

If the intended use for the button is to open a menu, it can be one of the
following values:

MENUFILE    - The File Menu
MENUOPTIONS - The Options Menu
MENUCHEAT   - The Cheat Menu
MENUTOOLS   - The Tools Menu
MENUHELP    - The Help Menu

If the intended use for the button is to provide a joypad button, then the
it can be one of the following values:

A        - A button
B        - B button
SEL      - SELECT button 
START    - START button
R        - right
L        - left
U        - up
D        - down
BR       - RIGHT button (shoulder)
BL       - LEFT button (shoulder)
SPEED    - speed up button (emulator)
CAPTURE  - screen capture (emulator)
GS       - GS/AR button (cheating)
UR       - up and right combination
UL       - up and left combination
DR       - down and right combination
DL       - down and left combination

Example:

[skin]
image=gbc.bmp
regions=gbc.rgn
draw=20,20,144,160

Skins are only supported in DirectDraw and GDI modes and are also not supported
in fullscreen mode.

To avoid scaling problems, please not the following:

GBA screen size: 240x160
GBC screen size without border: 160x144
GBC screen size with border: 256x224

Not using multiples to these values will cause distortion on the image drawn
by the emulator. This is not a BUG on the emulator and rather a problem of
the skin size.

Per game settings
-----------------

Version 1.5 introduced the support for per game settings for GBA games. You
can defined the following settings on a per game basis by using an INI file
called vba-over.ini in the same directory as the emulator:

comment=You can add any text you like here, for example the full name of the game, but not exceeding 255 characters
rtcEnabled=<0 for false, anything else for true>
flashSize=<65536 or 131072>
saveType=<0 for automatic, 1 for EEPROM, 2 for SRAM, 3 for Flash or 4 for
EEPROM+Sensor>

Use the 4 letter game code to separate settings for each game. Example:

[ABCD]
rtcEnabled=0
flashSize=65536
saveType=0

[ABC2]
rtcEnabled=1
flashSize=131072
saveType=0

An easier way to change the per game settings is to use the Game overrides dialog in the MFC version of VBA.
Select Menu>Options>Emulator>Game Overrides... to open the dialog. Just make your changes and click OK.

FAQ
---

See online FAQ for more information: http://vba.ngemu.com/faq.shtml

Please don't email about what you think it is problem before consulting
the FAQ.

Reporting a crash
-----------------

If VisualBoyAdvance crashes, please do the following:

1. Win 95/98/ME: start DrWatson (drwatson.exe) and reproduce the crash.
DrWatson will capture the crash information in a log file (.wlg) file that
needs to be sent to me. Please also open the .wlg file on your machine by
double-clicking and copy the details section into the email. Microsoft
made life harder when you migrate to WinXP (or NT or 2000) by not allowing
DrWatson to read its old file format.

2. Win NT/2000/XP: make sure DrWatson is the default debugger by executing
drwtsn32.exe -i and then recreate the crash. DrWatson will generate a log file
that needs to be sent to me (usually in c:\Documents and Settings\All Users\
Documents\DrWatson). Depending on your system configuration, you may be asked
if you want to generate a log file. If so, please click on yes.

LICENSE
-------

    VisualBoyAdvance - a Gameboy and GameboyAdvance emulator
    Copyright (C) 1999-2003 Forgotten
    Copyright (C) 2004 Forgotten and the VBA development team

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

Special Thanks
--------------

PokemonHacker for all his help improving the emulator.
Costis for his help fixing some of the graphics bugs.
Snes9x developers for the great emulator and source code.
Gollum for some help and tips.
Kreed for his great graphic filters.
And all users who kindly reported problems.

Contact
-------

Please don't email unless you found some bug. Requests will be ignored and
deleted. Also, be descriptive when emailing. You have to tell me what version
of the emulator you are writing about and a good description of the problem.
Remember, there are several interfaces (Windows, SDL and GTK+) and
several systems (Windows, Linux, MacOS X and BeOS).

Also, there are still people writing about the old VisualBoy which is no longer
supported. Also remember I am not paid to work on VisualBoyAdvance.

This is just a hobby.

Forgotten (http://vba.ngemu.com/contact.shtml)
kxu <kxu@users.sourceforge.net>

http://vba.ngemu.com
http://sourceforge.net/projects/vba
