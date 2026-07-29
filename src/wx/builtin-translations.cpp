#include "wx/builtin-translations.h"

#if defined(__ANDROID__)

#include <memory>

#include <wx/mstream.h>
#include <wx/translation.h>
#include <wx/zipstrm.h>

// The embedded copy of translations.zip: `builtin_translations`.
#include "wx/builtin-translations-data.h"

namespace {

// Layout inside translations.zip, as written by make-translations-zip.cmake:
// "<locale>/LC_MESSAGES/<domain>.mo".
wxString CatalogSuffix(const wxString& domain) {
    return "/LC_MESSAGES/" + domain + ".mo";
}

class BuiltinTranslationsLoader final : public wxTranslationsLoader {
public:
    wxMsgCatalog* LoadCatalog(const wxString& domain, const wxString& lang) override {
        const wxString locale = ResolveLanguage(domain, lang);
        if (locale.empty())
            return nullptr;

        const wxString wanted = locale + CatalogSuffix(domain);

        wxMemoryInputStream data(builtin_translations, sizeof(builtin_translations));
        wxZipInputStream zip(data);

        for (std::unique_ptr<wxZipEntry> entry(zip.GetNextEntry()); entry;
             entry.reset(zip.GetNextEntry())) {
            if (entry->GetInternalName() != wanted)
                continue;

            const wxFileOffset size = entry->GetSize();
            if (size <= 0)
                return nullptr;

            wxCharBuffer catalog(static_cast<size_t>(size));
            if (!catalog.data())
                return nullptr;

            // wxZipInputStream inflates in chunks, so a single Read() is not
            // guaranteed to return the whole entry.
            size_t got = 0;
            while (got < static_cast<size_t>(size)) {
                zip.Read(catalog.data() + got, static_cast<size_t>(size) - got);
                const size_t chunk = zip.LastRead();
                if (chunk == 0)
                    return nullptr;
                got += chunk;
            }

            return wxMsgCatalog::CreateFromData(catalog, domain);
        }

        return nullptr;
    }

    wxArrayString GetAvailableTranslations(const wxString& domain) const override {
        const wxString suffix = CatalogSuffix(domain);
        wxArrayString locales;

        wxMemoryInputStream data(builtin_translations, sizeof(builtin_translations));
        wxZipInputStream zip(data);

        for (std::unique_ptr<wxZipEntry> entry(zip.GetNextEntry()); entry;
             entry.reset(zip.GetNextEntry())) {
            const wxString name = entry->GetInternalName();
            if (!name.EndsWith(suffix))
                continue;

            const wxString locale = name.Left(name.length() - suffix.length());
            if (!locale.empty() && !locale.Contains("/"))
                locales.Add(locale);
        }

        return locales;
    }

private:
    // wx normally asks for one of the names GetAvailableTranslations() returned,
    // but AddCatalog() can also be called with an explicit language, so match
    // the way the file loader's lookup does: exact name, then the base language
    // ("pt" for "pt_PT"), then any regional variant of it ("pt_BR" for "pt").
    wxString ResolveLanguage(const wxString& domain, const wxString& lang) const {
        const wxArrayString available = GetAvailableTranslations(domain);
        if (lang.empty() || available.empty())
            return wxEmptyString;

        for (const wxString& locale : available) {
            if (locale.IsSameAs(lang, /*caseSensitive=*/false))
                return locale;
        }

        const wxString base = lang.BeforeFirst('_');
        for (const wxString& locale : available) {
            if (locale.IsSameAs(base, /*caseSensitive=*/false))
                return locale;
        }

        for (const wxString& locale : available) {
            if (locale.BeforeFirst('_').IsSameAs(base, /*caseSensitive=*/false))
                return locale;
        }

        return wxEmptyString;
    }
};

}  // namespace

void VbamInstallBuiltinTranslations() {
    wxTranslations* const translations = wxTranslations::Get();
    if (translations)
        translations->SetLoader(new BuiltinTranslationsLoader);
}

#endif  // defined(__ANDROID__)
