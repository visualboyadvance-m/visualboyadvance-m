#include "wx/language-compat.h"

#if !wxCHECK_VERSION(3, 2, 0)

#ifdef __WINDOWS__
// Strictly after the wx headers: windows.h turns GetClassInfo, CreateDialog and
// friends into macros, which rewrite member names in wx's declarations.
#include <windows.h>
#endif

namespace {

struct CustomLanguage {
    int language;
    const char* canonical_name;
    const char* description;
};

// Keep in sync with the ids declared in language-compat.h.
const CustomLanguage kCustomLanguages[] = {
    {wxLANGUAGE_SPANISH_LATIN_AMERICA, "es_419", "Spanish (Latin America)"},
};

}  // namespace

void VbamRegisterCustomLanguages() {
    for (const CustomLanguage& lang : kCustomLanguages) {
        wxLanguageInfo info;
        info.Language = lang.language;
        info.CanonicalName = lang.canonical_name;
        info.Description = lang.description;
        info.LayoutDirection = wxLayout_LeftToRight;
#ifdef __WINDOWS__
        // Windows has no LCID for the region-neutral es-419, so use the default
        // Spanish sublanguage. This only feeds the C runtime locale that
        // wxLocale::Init() tries to set -- which it already fails at harmlessly
        // for other languages -- and not the catalog lookup.
        info.WinLang = LANG_SPANISH;
        info.WinSublang = SUBLANG_DEFAULT;
#endif
        wxLocale::AddLanguage(info);
    }
}


#endif  // !wxCHECK_VERSION(3, 2, 0)
