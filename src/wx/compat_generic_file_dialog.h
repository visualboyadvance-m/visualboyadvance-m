#ifdef USE_GENERIC_FILE_DIALOGS
#include "wx/generic/filedlgg.h"

#undef  wxFileDialog
#define wxFileDialog wxGenericFileDialog
#endif
