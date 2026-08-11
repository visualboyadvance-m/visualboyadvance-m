// Embedder-provided globals for the save state test executable.
//
// vbam-core declares `coreOptions` but leaves it to whoever links the library
// to instantiate it (see core/base/system.h). The other test runners each
// define their own; the save state tests share this one so the definition
// lives in exactly one translation unit of the executable.

#include "core/base/system.h"

struct CoreOptions coreOptions;
