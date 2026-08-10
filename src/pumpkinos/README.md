# VBA-M PumpkinOS port

A native [PumpkinOS](https://github.com/migueletto/PumpkinOS) (PalmOS
re-implementation) frontend for VBA-M, derived from the libretro port: the
emulator core is compiled with the same defines as `src/libretro` (most
importantly `__LIBRETRO__`, which selects the in-memory savestate API and the
packed framebuffer stride), and `vbam_pumpkin.cpp` fills the role of
`libretro.cpp`.

## Building

```sh
cd src/pumpkinos
make PUMPKIN=/path/to/PumpkinOS
```

`PUMPKIN` must point at a PumpkinOS checkout that has already been built
(`bin/libpumpkin`, `bin/libpit` and `tools/pilrc`, `tools/prcbuild` must
exist). The build produces `VBAM.prc` and installs it into
`$(PUMPKIN)/vfs/app_install/`; PumpkinOS deploys it on next boot and it shows
up in the Launcher.

The `dlib` resource ID inside the PRC encodes the host OS/CPU/word-size
triple and must match the values baked into `libpumpkin` when it was built
(`dlib` id = `SYS_OS*64 + SYS_CPU*8 + SYS_SIZE`). The makefile computes it
from `uname -m` with 64-bit defaults; override `SYS_CPU`/`SYS_SIZE` on the
make command line if your PumpkinOS build differs.

### Classic 68k build

`Makefile.m68k` carries the build parameters for the `m68k-palmos-elf`
cross toolchain (prc-tools style), producing a plain 68k code PRC that
runs through the PumpkinOS m68k emulator (or classic PalmOS):

```sh
cd src/pumpkinos
make -f Makefile.m68k [TOOLPREFIX=m68k-palmos-elf-] [PALMDEV=/opt/palmdev]
make -f Makefile.m68k install   # copy PRC into $(PUMPKIN)/vfs/app_install
```

It defines `WORDS_BIGENDIAN`/`MSB_FIRST` for the big-endian target and
`PALMOS_68K` for the glue. See the caveats at the top of the file: code
segments are limited to 64 KiB (the generated `.def` declares a
multi-segment application), and the native `pumpkin_*` calls in the glue
need 68k equivalents.

## Usage

- Put ROMs in the PumpkinOS VFS at `/PALM/Programs/VBAM/`
  (host path: `<PumpkinOS>/vfs/app_card/PALM/Programs/VBAM/`).
  Recognized extensions: `.gba .agb .bin .elf .mb` (GBA, HLE BIOS),
  `.gb .gbc .cgb .sgb .dmg` (GB/GBC/SGB).
- Launch VBAM, pick a ROM, hit **Run**.
- Battery saves are written next to the ROM as `<name>.sav` (raw, same
  format as the libretro port) whenever the game stops writing cart RAM,
  and on exit. One savestate slot per game (`<name>.ss0`) via the Game menu.

### Controls

| GBA        | Key                        |
|------------|----------------------------|
| D-pad      | Arrow keys                 |
| A          | X                          |
| B          | Z                          |
| L / R      | A / S                      |
| Start      | Enter or Space             |
| Select     | Backspace or Tab           |

## Implementation notes

- **Video**: the core renders BGR555 through `systemColorMap16`, which is
  built with the byte swap to the window bitmap's big-endian RGB565 baked
  in, so `systemDrawScreen()` is a straight copy (1x or pixel-doubled 2x)
  into `BmpGetBits(WinGetBitmap(WinGetDisplayWindow()))` followed by
  `pumpkin_screen_dirty()`. The `wind` resource requests a 512x400 window:
  GBA (240x160) and GB (160x144) run pixel-doubled, SGB borders (256x224)
  fall back to 1x automatically.
- **Audio**: `SoundPumpkin` implements the core `SoundDriver` interface on
  top of `SndStreamCreate` (44100 Hz, 16-bit stereo — must match the rate
  liblsdl3 opens the SDL3 device with, because its resample path produces
  frame-misaligned buffers that SDL3 rejects). The stream callback
  runs on the host audio thread and drains a mutex-protected ring buffer.
  The frame loop paces emulation against the ring fill level (audio-clocked)
  and falls back to wall-clock pacing when no stream is available.
- **Input**: polled once per frame with `pumpkin_status()` — hardware key
  bits for the d-pad, the 128-bit ASCII bitmap for buttons — matching the
  ChocoDoom port's approach, since PalmOS events carry no key-up edges.
- **Files**: everything goes through the PalmOS VFS volume
  (`VFSFileOpen`/`VFSFileRead`/...); the core never touches files because
  the BIOS is HLE (`CPUInit(NULL, false)`) and battery/savestate I/O uses
  the in-memory pointers, as in the libretro port.
- **Not included** (same cuts as a minimal libretro build): link cable,
  debugger, GB printer, interframe blending filters, solar/tilt sensors
  (stubbed to neutral values), cheats UI.
