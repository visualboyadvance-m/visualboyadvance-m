// newlib's <strings.h>, for Retro68.
//
// On a case-insensitive file system the Retro68 install leaves one file for
// both <strings.h> and the Toolbox's <Strings.h>, and it is the Toolbox one:
// newlib's <string.h> then pulls in Multiverse.h (and MacTypes' UInt32,
// Size, Ptr...) instead of declaring strcasecmp() and the rest. libc.a still
// has the functions. The Retro68 build puts this directory ahead of the
// sysroot, so both <string.h>'s include and the code's own find these
// declarations. Nothing in VBA-M needs the Toolbox header's Pascal string
// helpers; code that does includes the Toolbox headers it uses directly.

#ifndef VBAM_RETRO68_STRINGS_H_
#define VBAM_RETRO68_STRINGS_H_

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int bcmp(const void*, const void*, size_t);
void bcopy(const void*, void*, size_t);
void bzero(void*, size_t);
void explicit_bzero(void*, size_t);
int ffs(int);
char* index(const char*, int);
char* rindex(const char*, int);
int strcasecmp(const char*, const char*);
int strncasecmp(const char*, const char*, size_t);

#ifdef __cplusplus
}
#endif

#endif  // VBAM_RETRO68_STRINGS_H_
