// dummy webupdatedef.h for checklistctrl
#ifndef WXDLLIMPEXP_WEBUPDATE
// this will never be part of a DLL
#define WXDLLIMPEXP_WEBUPDATE
// why would I include it and not enable it?
#define wxUSE_CHECKEDLISTCTRL 1
// enable customizations:
//   - include wx/settings.h (bugfix; always enabled)
//   - make a dynamic class so it can be subclass in xrc
//   - only make col0 checkable
//   - use "native" checkbox (also requires CLC_USE_SYSICONS)
#define CLC_VBAM_USAGE 1
#define CLC_USE_SYSICONS 1
#endif
