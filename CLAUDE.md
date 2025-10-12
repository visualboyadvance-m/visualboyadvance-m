# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

VisualBoyAdvance-M (VBA-M) is a Game Boy and Game Boy Advance emulator written in C++ with multiple frontends. The project has evolved from the original VisualBoyAdvance emulator with enhanced features and cross-platform support.

## Build System

This project uses CMake as the primary build system. The libretro core has a separate Makefile-based build.

### Standard Build Commands

```bash
# Configure and build (Release)
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -G Ninja
ninja

# Configure and build (Debug)
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug -G Ninja
ninja

# Install dependencies (Linux/macOS/MSYS2)
./installdeps

# Run tests
cd build
ctest -j --output-on-failure

# Build libretro core
cd src/libretro
make -j$(nproc)
```

### Important CMake Options

- `ENABLE_SDL`: Build the SDL port (default: OFF)
- `ENABLE_WX`: Build the wxWidgets port (default: ON)
- `ENABLE_DEBUGGER`: Enable the debugger (default: ON)
- `ENABLE_LINK`: Enable GBA linking functionality (default: AUTO)
- `ENABLE_FFMPEG`: Enable ffmpeg A/V recording (default: AUTO)
- `BUILD_TESTING`: Build tests and enable ctest support (default: ON)
- `ENABLE_ASAN`: Enable libasan sanitizers (debug mode only, default: OFF)

## Architecture

### Core Structure

The codebase is organized into distinct layers:

**src/core/** - Emulator core (platform-independent)
- `gb/` - Game Boy emulator implementation
- `gba/` - Game Boy Advance emulator implementation
- `apu/` - Audio Processing Unit shared code
- `base/` - Core utilities and base classes
- `fex/` - File extraction utilities

The `vbam-core` library contains both GB and GBA emulators. These are tightly coupled through shared Link and Sound emulation code. Future refactoring may separate these into independent targets.

**src/wx/** - wxWidgets GUI frontend (primary frontend)
- Main GUI application with cross-platform support
- Platform-specific renderers (OpenGL, Metal on macOS, etc.)
- Handles configuration, input, and display

**src/sdl/** - SDL-based frontend (command-line/minimal GUI)
- Lightweight alternative to wxWidgets
- Includes debugger support

**src/libretro/** - Libretro core implementation
- RetroArch/libretro API glue layer
- Independent build system (Makefile)

**src/components/** - Reusable components

### Key Design Patterns

1. **Frontend/Core Separation**: The emulator cores (GB/GBA) are separate from the UI frontends. Cores should remain platform-agnostic.

2. **Modular Audio**: The APU (Audio Processing Unit) is a separate module linked by both GB and GBA cores.

3. **Build Target Structure**:
   - `vbam-core`: Static library containing GB and GBA emulators
   - `vbam-core-apu`: Audio processing
   - `vbam-core-base`: Base utilities
   - Frontend executables link against these libraries

## Development Workflow

### Commit Message Standards

**CRITICAL**: Follow the commit message format from DEVELOPER-MANUAL.md:

- Title line: max 50 characters, no period at end
- Body: wrapped at 72 characters
- Write in imperative mood (e.g., "fix", not "fixes" or "fixed")
- Always include a body; it must be independent of the title
- Sign commits with GPG (`git commit -S`)

**Commit prefixes** (only for non-wxWidgets GUI commits):
- `doc:` - Documentation changes
- `build:` - CMake, installdeps, build system changes
- `gb:` - Game Boy core changes
- `gba:` - Game Boy Advance core changes
- `libretro:` - Libretro core and build
- `sdl:` - SDL port (not SDL functionality in wxWidgets)
- `translations:` - Translation-related changes

wxWidgets GUI commits should NOT have a prefix.

**For regressions**:
```
Fix regression <PROBLEM> in <SHORT-SHA>

Description must reference the breaking change in git reference format.
```

**Closing issues**: Use `Fix #999.` in commit body to auto-close issues.

### Testing

Tests use Google Test framework:
- Core tests: `src/core/test/`
- wxWidgets tests: `src/wx/test/`
- Run with: `ctest -j --output-on-failure` from build directory

Tests are only built when `BUILD_TESTING=ON` (default) and in a git checkout.

### Debug Messages

**wxWidgets code**:
```cpp
int foo = 42;
wxLogDebug(wxT("the value of foo = %d"), foo);
```

**Core code**:
```cpp
fprintf(stderr, "debug message: %s\n", value);
```

Debug output requires `-DCMAKE_BUILD_TYPE=Debug`.

### Platform-Specific Notes

**Windows (MSVC)**:
- vcpkg handles dependencies automatically
- Use Visual Studio cmake support or `x64 Native Tools Command Prompt`
- Build with: `cmake .. -G Ninja && ninja`

**Windows (MinGW/MSYS2)**:
- Use `./installdeps` in MINGW32/MINGW64 shell
- Required for 32-bit builds (Windows XP compatibility)

**macOS**:
- Requires Xcode
- Metal shader support available on modern systems
- Use `./installdeps` with Homebrew/MacPorts

**Linux**:
- Supported: Debian/Ubuntu, Fedora, Arch, Solus, OpenSUSE, Gentoo, RHEL/CentOS
- Run `./installdeps` for automatic dependency setup

### Cross-Compilation

**32-bit on 64-bit**: `./installdeps m32`
**Windows from Linux**: `./installdeps win32` or `./installdeps mingw-w64-x86_64`

## Branch Workflow

- Main branch: `master`
- Never push directly to `master` without CI validation
- Test commits on a branch first, check CI status on GitHub
- Use `git merge --ff-only` when merging to master
- Never `git push -f` on `master`
- Keep work branches rebased on `master` during active development

## Code Standards

- C++17 standard (enforced by CMake)
- C11 standard for C code
- Use `nullptr` not `NULL`
- Follow existing code style in each subsystem
- Avoid breaking changes to save file formats or core emulation behavior without discussion

## Critical Files

- `CMakeLists.txt` - Root build configuration
- `cmake/Options.cmake` - CMake build options
- `DEVELOPER-MANUAL.md` - Development policies and release process
- `src/core/CMakeLists.txt` - Core library target definition
- `src/libretro/Makefile` - Libretro build system

## Dependencies

Core dependencies (required):
- zlib, SDL2, wxWidgets (for GUI), gettext

Optional dependencies:
- ffmpeg (recording)
- openal-soft (audio backend)
- faudio (audio backend)

Use vcpkg on Windows, `./installdeps` on supported platforms.
