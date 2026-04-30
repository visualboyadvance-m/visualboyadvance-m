// Cross-frontend script-hook trampolines.
//
// The emulator core invokes these from its memory-access fast paths
// when a hook is installed; the host frontend (currently wx Lua)
// points each pointer at the dispatcher inside its scripting engine.
// They start out as null — calling them is gated by simple boolean
// "any hook?" flags so the no-script case is a single branch.

#ifndef VBAM_CORE_BASE_SCRIPT_HOOKS_H_
#define VBAM_CORE_BASE_SCRIPT_HOOKS_H_

#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

// Read of `size` bytes (1, 2, or 4) at `addr`, with the value the CPU
// observes. Address is always a CPU-bus address: 0..0xFFFF on GB,
// 32-bit on GBA.
typedef void (*VbamScriptMemReadFn) (uint32_t addr, int size, uint32_t value);
typedef void (*VbamScriptMemWriteFn)(uint32_t addr, int size, uint32_t value);
typedef void (*VbamScriptMemExecFn) (uint32_t addr);

extern VbamScriptMemReadFn  g_vbam_script_mem_read;
extern VbamScriptMemWriteFn g_vbam_script_mem_write;
extern VbamScriptMemExecFn  g_vbam_script_mem_exec;

// Quick "anyone listening?" gates — the core checks these first to
// skip a function-pointer call when no script has any hooks
// installed. They're set by the engine at register time.
extern int g_vbam_script_has_read_hooks;
extern int g_vbam_script_has_write_hooks;
extern int g_vbam_script_has_exec_hooks;

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VBAM_CORE_BASE_SCRIPT_HOOKS_H_
