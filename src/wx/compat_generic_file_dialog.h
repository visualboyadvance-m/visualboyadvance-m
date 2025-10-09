#ifdef USE_GENERIC_FILE_DIALOGS
#include "wx/generic/filedlgg.h"

#undef  wxFileDialog
#define wxFileDialog wxGenericFileDialog

#define SetGenericPath(X,Y) // X.SetDirectory(Y); X.SetPath(Y)
#else
#define SetGenericPath(X,Y)
#endif
