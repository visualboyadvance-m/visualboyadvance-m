SDL frontend keymap
===================
These are the keybindings that are hardwired into the source code
(those that are not defined in vbam.cfg):

CTRL-R: reset
CTRL-B: rewind
CTRL-V: unrewind
CTRL-H: restore to last restored rewind (repeat)
CTRL-J: restore to last *stored* rewind (go to top of the stack)
CTRL-P: pause
CTRL-F: toggle fullscreen
CTRL-G: rotate between filters
CTRL-S: toggle sound
NUMPAD /: decrease volume
NUMPAD *: increase volume
CTRL-E: toggle cheats
ESC: quit
F11: debugger
F1..F8: (switchable)
  exact: read state
  SHIFT: write state
 in mode 3:
  F1..F4: nothing
  F5: decrease saveslot number (minimum is 1)
  F6: increase saveslot number (maximum is 8)
  F7: save state to current saveslot
  F8: load state from current saveslot
F9: load backup from last load operation (the state that was overwritten (in the application) by the then loaded state)
F10: load backup from last save operation (the state that was overwritten (on disk) by the then current state)
ALT-1 autofire A toggle (now also configurable on a per-gamepad basis with Joy#_AutoA)
ALT-2 dtto B (now also configurable on a per-gamepad basis with Joy#_AutoB)
ALT-3 dtto R
ALT-4 dtto L

CTRL- 1,2,3,4,5,6,7,8: toggle layers
CTRL-N: pause on next frame

KEYPAD: 4682 (default, cfg: Motion_{Left|Right|Up|Down}): motion sensor



CHEAT CODES
===========
Use the --cheat commandline option. Each --cheat XXXX adds one cheat code (in order
of appearance).
. Up to 100 cheat codes are supported.
. Cheat lists are saved in savestates. Cheats in loaded state should override commandline cheats.
. There are no provisions for toggling individual cheats.
. CTRL-E toggles the global 'cheats enabled' option.
. Note that autofire may not work with cheats enabled (at least for some games).

Two formats are available:
PAR:  --cheat '########:########'
CBA:  --cheat '######## ####'

All # are hexadecimal digits. Only uppercase A-F are accepted.

PAR (action replay, also gameshark): only non-encrypted codes work (if it has a master code, it won't work).
CBA (codebreaker): encrypted codes should work



IPS PATCHES
===========
Use the -i or --ips commandline options. They take one argument - the name of file to be
loaded as an IPS patch.
Maximum of 100 IPS patches is supported.
If you don't specify any IPS patch on the commandline, the VBA-M will look for one
in the file ROMBASENAME.ips, where ROMBASENAME is the name of the rom file without extension
(so if your rom file is named LolRom.gba, VBA-M will look for LolRom.ips).
Any files that don't exist or are not IPS patches will be skipped (with warning).
Patches will be applied in the order you gave them on the command line.
Patches are *not* remembered or saved in savestates. You should specify them each
time you run VBA-M, or you will end up running an unpatched ROM with data from
a patched ROM.



SAVESTATES
==========
There is a new configuration option saveKeysSwitch. It's value has the following meaning:
saveKeysSwitch = 0 ... 'classic' SDL interpretation of F1..F10:
. F1, F2, ... F8:                    load savestate 1, 2, ... 8
. SHIFT+F1, SHIFT+F2, ... SHIFT+F8:  save savestate 1, 2, ... 8
saveKeysSwitch = 1 ... same with 'toggled' SHIFT:
. F1, F2, ... F8:                    SAVE savestate 1, 2, ... 8
. SHIFT+F1, SHIFT+F2, ... SHIFT+F8:  LOAD savestate 1, 2, ... 8
saveKeysSwitch = 2 ... slot-selection scheme:
. F1 .. F4: nothing
. F5: decrease current slot number
. F6: increase current slot number
. F7: save to currently selected slot
. F8: load from currently selected slot
This last scheme has the added benefit that it works without problems even if your
SHIFT key doesn't.
The selected slot starts at number 1, cannot go below 1 and cannot go over 8.



SAVESTATE BACKUPS
=================
You will sometimes load or save a state when you didn't really want to. You might
have pressed the wrong button, saved to a wrong slot, saved a state that leads
to a certain death, etc ...
This feature provides a limited 'undo' functionality.
Every time you load a state from disk, the program first saves your current state
to slot 9. Pressing F9 then loads this 'undo last state load' state.
In the same vein, when you save a state, the previous state from that slot is
first moved to slot 10. Pressing F10 then does a 'undo last state save'
operation.
Note that you can't write states to slots 9 and 10 directly.
This works the same way (F9 undoes last load, F10 undoes last save) regardless
of the value of saveKeysSwitch.



REWINDS
=======
VBA-M has an 'autosave' feature. Try setting rewindTimer in your vbam.cfg.
Keep in mind that this (like all other numbers in the cfg file) is a hexadecimal number.
So rewindTimer=3c means saving every 60 seconds, or one minute.
The maximum value is 258, which is 10 minutes (258 in hex is 2*256 + 5*16 + 8 =
= 512 + 50+30 + 8 = 600 seconds).
Last 8 autosaves are retained in memory.
Autosaves are not considered savestates for 'backup' puproses (see previous section).
They are never saved to disk and so will be lost when the program exits.
Also, when you load a real (on-disk) savestate, *nothing* happens to rewinds. Rewinds
allow you to return to points in *your* chronological past. There is currently no way
of tracking which states/rewinds are "after" which.
Controls:
CTRL-J: go to the rewind that was stored last (the newest one) (and select it)
CTRL-H: 'home' - repeat last 'go to rewind' operation (go to the currently selected rewind)
CTRL-B: select previous rewind and go to it
CTRL-B: select next rewind and go to it

Normally, the currently selected rewind stays selected until it becomes invalid (which
happens when it is the oldes rewind left and a new autosave wants its space).
However, when the newest rewind is selected, the selection will start advancing
with the newest rewind.


AUTOFIRE
========
There are two autofire modes.
The first one toggles autofire on a button for all gamepads:
for example, when you press ALT-1 on your keyboard, then all A buttons on
all gamepads will be 'autofiring' (alternating quickly between pressed and not pressed)
until you press ALT-1 again. This might not be what you want.
Or you can get a gamepad with more buttons and set some to be 'autofire A'.
In the vbam.cfg file, set Joy#_AutoX=$$$$, where # is the number
of the emulated gamepad, X is eighter A or B (autofire on A or autofire on B)
and $$$$ is the code that should trigger this.
For example, I have
Joy0_AutoA=1082
which means:
. Joy0 = the first emulated pad
. AutoA = doing this will start toggling the A button on Joy0 quickly
. 1082 = 'doing this' means:
  .   1000 = the first joystick/gamepad attached to my computer
  . + 0080 = pressing button
  . + 0002 = button with code 2 (on my gamepad, this is the button which has the number 3 engraved into it
             and is in the position where SNES gamepads have the A button and PS controllers have the
	     circle button)
So when I press and hold this button on my gamepad, VBA-M rewards me with autofire on A *on that pad only*.

[2008-11-21] new autofire configuration:
  commandline argument: --autofire NNN
  configfile attribute: autoFireMaxCount=NNN
  default value: NNN=1 (does exactly the same thing as without this new feature)
This controls the "length" of each press (and depress). It is measured in vba-m cycles
(so --autofire 30 means something like "press once every second" - don't forget that
the button is not just "virtually pressed" but also "virtually non-pressed" in between).
This is needed for some games that apparently check whether the button isn't pressed
faster than a human could do it.
For example, autofire doesn't work in Mother 3 with --autofire 1 or 2, but it works with 5.

