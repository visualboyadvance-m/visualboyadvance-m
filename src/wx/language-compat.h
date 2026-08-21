#ifndef VBAM_WX_LANGUAGE_COMPAT_H_
#define VBAM_WX_LANGUAGE_COMPAT_H_

#include <wx/intl.h>
#include <wx/version.h>

// wxWidgets 3.2 gave every language an explicit
// wxLANGUAGE_<LANGUAGE>_<COUNTRY> constant and made the plain constant mean
// the bare "xx" locale. Before that the plain constant *is* the country
// locale -- wxLANGUAGE_FRENCH is "fr_FR" -- so map the new names onto it.
//
// That covers every catalog in po/wxvbam exactly, except es_419: wx 3.0 has no
// Latin American Spanish at all, and it cannot borrow wxLANGUAGE_SPANISH
// because the es catalog has its own menu entry. It gets a user-defined id
// registered with wx instead, by VbamRegisterCustomLanguages() below.
#if !wxCHECK_VERSION(3, 2, 0)

#define wxLANGUAGE_FRENCH_FRANCE     wxLANGUAGE_FRENCH
#define wxLANGUAGE_HEBREW_ISRAEL     wxLANGUAGE_HEBREW
#define wxLANGUAGE_HUNGARIAN_HUNGARY wxLANGUAGE_HUNGARIAN
#define wxLANGUAGE_ITALIAN_ITALY     wxLANGUAGE_ITALIAN
#define wxLANGUAGE_KOREAN_KOREA      wxLANGUAGE_KOREAN
#define wxLANGUAGE_POLISH_POLAND     wxLANGUAGE_POLISH
#define wxLANGUAGE_CHINESE_CHINA     wxLANGUAGE_CHINESE_SIMPLIFIED

// Ids for the languages wx does not know. These are persisted in the kLocale
// option, so keep them stable: append, never renumber.
enum {
    wxLANGUAGE_SPANISH_LATIN_AMERICA = wxLANGUAGE_USER_DEFINED + 1,
};

// Adds those languages to wx's language table. wxLocale::AddLanguage() only
// takes effect before the first wxLocale::Init(), so call this once, early in
// OnInit(), before any locale is constructed.
// wxLocale::Init() then derives its canonical name from the registered
// wxLanguageInfo, and wxTranslations resolves the catalog from that, so
// nothing further is needed at language-switch time.
void VbamRegisterCustomLanguages();

#else  // wxCHECK_VERSION(3, 2, 0)

inline void VbamRegisterCustomLanguages() {}

#endif  // !wxCHECK_VERSION(3, 2, 0)

#endif  // VBAM_WX_LANGUAGE_COMPAT_H_
