#ifndef VBAM_WX_WXLOGDEBUG_H_
#define VBAM_WX_WXLOGDEBUG_H_

#include <wx/log.h>
#include <wx/datetime.h>

// make wxLogDebug work on non-debug builds of Wx, and make it use the console
// on Windows
// (this works on 2.8 too!)
#if !defined(NDEBUG) && (!wxDEBUG_LEVEL || defined(__WXMSW__))
    #ifdef __WXMSW__
        #define VBAM_DEBUG_STREAM stdout
    #else
        #define VBAM_DEBUG_STREAM stderr
    #endif
    #undef  wxLogDebug
    #define wxLogDebug(...)                                                                                                           \
    do {                                                                                                                              \
        fputs(wxString::Format(wxDateTime::UNow().Format(wxT("%X")) + wxT(": Debug: ") + __VA_ARGS__).utf8_str(), VBAM_DEBUG_STREAM); \
        fputc('\n', VBAM_DEBUG_STREAM);                                                                                               \
    } while(0)
#endif

#endif // VBAM_WX_WXLOGDEBUG_H_
