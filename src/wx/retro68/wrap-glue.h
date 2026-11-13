#ifndef VBAM_WX_RETRO68_WRAP_GLUE_H_
#define VBAM_WX_RETRO68_WRAP_GLUE_H_

// Glue for replacing a C library function through the linker's --wrap
// (Retro68). For each wrapped <name>, CMakeLists.txt passes --wrap, the C
// code defines vbam_wrap_<name> and calls the original as vbam_real_<name>,
// and VBAM_WRAP_GLUE(<name>) supplies the two symbols in between:
// __wrap_<name> jumps to vbam_wrap_<name>, and vbam_real_<name> jumps to
// __real_<name>, which the linker makes the original. Jumps, so the arguments
// and the return address stay as the caller left them.
//
// On the PowerPC (XCOFF) a call goes to the function's code symbol, which is
// its name with a dot in front, and those are what --wrap renames there
// (CMakeLists.txt wraps ".<name>"); C cannot define a symbol with a dot in
// the middle, hence the assembly.
//
// Use: VBAM_WRAP_GLUE_BEGIN VBAM_WRAP_GLUE(a) VBAM_WRAP_GLUE(b) VBAM_WRAP_GLUE_END

#if defined(__powerpc__) || defined(__ppc__)
#define VBAM_WRAP_GLUE_BEGIN __asm__("\t.csect .text[PR]\n"
#define VBAM_WRAP_GLUE(name)            \
    "\t.globl __wrap_." #name "\n"      \
    "__wrap_." #name ":\n"              \
    "\tb .vbam_wrap_" #name "\n"        \
    "\t.globl .vbam_real_" #name "\n"   \
    ".vbam_real_" #name ":\n"           \
    "\tb __real_." #name "\n"
#else
#define VBAM_WRAP_GLUE_BEGIN __asm__("\t.text\n"
#define VBAM_WRAP_GLUE(name)            \
    "\t.globl __wrap_" #name "\n"       \
    "__wrap_" #name ":\n"               \
    "\tjra vbam_wrap_" #name "\n"       \
    "\t.globl vbam_real_" #name "\n"    \
    "vbam_real_" #name ":\n"            \
    "\tjra __real_" #name "\n"
#endif
#define VBAM_WRAP_GLUE_END );

#endif  // VBAM_WX_RETRO68_WRAP_GLUE_H_
