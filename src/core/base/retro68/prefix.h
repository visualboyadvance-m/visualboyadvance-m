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

// std::mutex and friends, for a C++ library without gthreads: on the
// pthreads-retro68 Thread Manager threads where the build has them
// (ENABLE_RETRO68_PTHREADS), single-threaded stand-ins otherwise.
#ifdef VBAM_RETRO68_PTHREADS
#include "../pthread_std.h"
#else
#include "../nothreads.h"
#endif

// GCC's and newlib's <limits.h> only define the long long limits for C99
// (__STDC_VERSION__ / __ISO_C_VISIBLE), so C++ sources here never see them.
// Lua's luaconf.h takes a missing LLONG_MAX to mean "no long long" and
// stops with #error; give C++ the C99 values.
#include <limits.h>
#ifndef LLONG_MAX
#define LLONG_MAX __LONG_LONG_MAX__
#endif
#ifndef LLONG_MIN
#define LLONG_MIN (-LLONG_MAX - 1LL)
#endif
#ifndef ULLONG_MAX
#define ULLONG_MAX (LLONG_MAX * 2ULL + 1ULL)
#endif

#endif  // VBAM_CORE_BASE_RETRO68_PREFIX_H_
