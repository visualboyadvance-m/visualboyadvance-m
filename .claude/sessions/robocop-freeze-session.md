# RoboCop Freeze Bug - Debug Session

## Problem
RoboCop (USA, Europe) (Rev 01) ROM freezes at the first level intro screen "on patrol in Detroit" with music playing. The game never progressed past this screen in VBA-M.

## ROM Location
`C:\Users\rkito\Games\gb\RoboCop.gb`

## Root Cause Identified
The game uses an unusual joypad polling method where it writes 0x00 to register $FF00, selecting BOTH P14 and P15 simultaneously (both direction and action button groups). VBA-M was returning 0xFF (no buttons) in this case instead of ORing both button groups together like real hardware.

### Specific Issue Location
**File**: `src/core/gb/gb.cpp`

**Original buggy code** (lines 2281-2287 before fix):
```cpp
} else {
    if (gbSgbMode && gbSgbMultiplayer) {
        gbMemory[0xff00] = 0xf0 | gbSgbNextController;
    } else {
        gbMemory[0xff00] = 0xff;  // BUG: Should OR both button groups
    }
}
```

## Current Fix Attempt
Added handling for `(b & 0x30) == 0x00` case (lines 2291-2338):

```cpp
} else if ((b & 0x30) == 0x00) {
    // Both P14 and P15 selected (value 0x00)
    // OR both direction and action buttons together (like real hardware)
    b &= 0xf0;
    b |= 0x0f; // Start with all buttons unpressed (bits = 1)

    int joy = 0;
    if (gbSgbMode && gbSgbMultiplayer) {
        switch (gbSgbNextController) {
        case 0x0f: joy = 0; break;
        case 0x0e: joy = 1; break;
        case 0x0d: joy = 2; break;
        case 0x0c: joy = 3; break;
        default: joy = 0;
        }
    }
    int joystate = gbJoymask[joy];

    // Check direction buttons (Down, Up, Left, Right) - pressed = clear bit
    if (!(joystate & 128)) b &= ~0x08;
    if (!(joystate & 64))  b &= ~0x04;
    if (!(joystate & 32))  b &= ~0x02;
    if (!(joystate & 16))  b &= ~0x01;

    // Check action buttons (A, B, Select, Start) - pressed = clear bit
    if (!(joystate & 8))   b &= ~0x08;
    if (!(joystate & 4))   b &= ~0x04;
    if (!(joystate & 2))   b &= ~0x02;
    if (!(joystate & 1))   b &= ~0x01;

    gbMemory[0xff00] = (uint8_t)b;
}
```

## Debug Infrastructure Added
- Debug log file: `build-x64-windows-static-release/gb_debug.txt`
- V-Blank interrupt logging (lines 4311-4322)
- PC tracking (lines 4169-4197)
- Joypad read logging (lines 2215-2223)
- HALT instruction logging in `src/core/gb/internal/gbCodes.h` (lines 557-571)

## Key Findings
1. Game runs in DMG mode (gbHardware=0x01, gbCgbMode=0)
2. Interrupts work correctly (IE=11, IFF=01, music plays)
3. Game executes joypad polling loop at PC=0x3121-0x3126
4. Joypad read shows: `value=CF (b&0x30)=00` - confirming both groups selected
5. Game writes 0x00 to $FF00, which becomes 0xCF in memory (lower nibble 0x0F = all buttons unpressed)

## Reference Implementation (mGBA)
File: `C:\Users\rkito\source\repos\mgba\src\gb\io.c` lines 85-87:
```c
case 0x00:
    keys |= keys >> 4;  // ORs direction and action buttons together
    break;
```

## Status
**STILL FREEZING** - The fix has been implemented and built but the game still doesn't progress. The logic appears correct:
- Start with all buttons unpressed (0x0F)
- Clear bits for pressed buttons (pressed = 0)
- OR together direction and action button states

## Next Steps to Try
1. Verify the button state is actually being read from the correct source (`gbJoymask[joy]`)
2. Add more detailed logging to see what button values are being returned
3. Check if there's a timing issue with joypad reads
4. Verify the game actually expects THIS behavior (might need to disassemble more code)
5. Test with other games that use the same polling method to verify the fix doesn't break anything
6. Consider that the issue might not be joypad-related at all - the polling loop might be waiting for something else

## Build Commands
```bash
cd build-x64-windows-static-release
ninja
timeout 15 ./visualboyadvance-m.exe "C:\Users\rkito\Games\gb\RoboCop.gb"
```

## Git Status
Current branch: master
Recent commit: ce363a35 GB: Generate V-Blank interrupt for all hardware types

## Additional Notes
- Previous commits (ce363a35, 1432bbbd) modified V-Blank interrupt handling but that wasn't the issue
- Nintendo logo VRAM copying was implemented but also wasn't the fix
- The game has NEVER worked in VBA-M (confirmed by user)
