// Storage for the script-hook trampoline pointers. See script_hooks.h.

#include "core/base/script_hooks.h"

#ifdef __cplusplus
extern "C" {
#endif

VbamScriptMemReadFn  g_vbam_script_mem_read  = nullptr;
VbamScriptMemWriteFn g_vbam_script_mem_write = nullptr;
VbamScriptMemExecFn  g_vbam_script_mem_exec  = nullptr;

int g_vbam_script_has_read_hooks  = 0;
int g_vbam_script_has_write_hooks = 0;
int g_vbam_script_has_exec_hooks  = 0;

#ifdef __cplusplus
}
#endif
