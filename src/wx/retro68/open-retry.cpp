// A second try at creating a file, for classic Mac OS (Retro68).
//
// Retro68's open() creates a missing file with HCreate() and then opens it
// with HOpenDF(). On the volume BasiliskII and SheepShaver share from the host
// (extfs, the "Unix" volume), HCreate() makes the file but answers fnfErr,
// and the open after it fails the same way; open() then returns -1 and leaves
// an empty file behind. A second open() works: its HCreate() finds the file
// already there and the open goes through. Battery saves next to the ROMs on
// that volume came out empty, and the next load refused them.
//
// The linker sends open() here (--wrap, see CMakeLists.txt and wrap-glue.h);
// when creating a file fails, it is tried once more.

#include <fcntl.h>

#include "wx/retro68/wrap-glue.h"

extern "C" {

struct _reent;

int vbam_real__open_r(struct _reent* r, const char* name, int flags, int mode);

int vbam_wrap__open_r(struct _reent* r, const char* name, int flags, int mode) {
    const int fd = vbam_real__open_r(r, name, flags, mode);
    if (fd >= 0 || !(flags & O_CREAT))
        return fd;
    return vbam_real__open_r(r, name, flags, mode);
}

}  // extern "C"

VBAM_WRAP_GLUE_BEGIN
VBAM_WRAP_GLUE(_open_r)
VBAM_WRAP_GLUE_END
