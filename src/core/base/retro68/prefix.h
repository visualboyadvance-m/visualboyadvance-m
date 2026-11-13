// Included ahead of every source file in the Retro68 (classic Mac OS) build.

#ifndef VBAM_CORE_BASE_RETRO68_PREFIX_H_
#define VBAM_CORE_BASE_RETRO68_PREFIX_H_

// The Retro68 SDL3's <SDL3/SDL_stdinc.h> renames DrawText, MeasureText and
// Palette (#define DrawText DrawTextSDL ...) to keep them clear of QuickDraw,
// which renames wxDC::DrawText() and friends in every file that includes SDL
// after wx. Mapping the new names back makes the pair cancel out, whichever
// is included first: DrawText expands to DrawTextSDL, that to DrawText, and a
// macro is not expanded again inside its own expansion. SDL has no functions
// or types of these names, so nothing it exports changes.
#define DrawTextSDL DrawText
#define MeasureTextSDL MeasureText
#define PaletteSDL Palette

// std::mutex and friends, for a C++ library without gthreads.
#include "../nothreads.h"

#endif  // VBAM_CORE_BASE_RETRO68_PREFIX_H_
