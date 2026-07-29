#ifndef VBAM_WX_BUILTIN_TRANSLATIONS_H_
#define VBAM_WX_BUILTIN_TRANSLATIONS_H_

// Gettext catalogs served out of the binary itself, for Android.
//
// Every other port reads the compiled .mo catalogs from a filesystem prefix:
// share/locale, the Resources dir of the .app bundle, or -- on Windows -- an
// embedded RC resource read by wxResourceTranslationsLoader. An APK has no
// such prefix; its payload is assets inside the package rather than files, and
// nothing installs share/locale on the device. So the Android build embeds the
// same translations.zip that is shipped with the desktop ports directly in the
// executable (bin2c -> builtin-translations-data.h) and reads the catalogs back
// out of that blob at run time.

#if defined(__ANDROID__)

// Installs the built-in catalog loader on the current wxTranslations object.
// Call after wxLocale::Init() -- which installs a fresh wxTranslations and so
// drops any loader set before it -- and before AddCatalog().
void VbamInstallBuiltinTranslations();

#else

inline void VbamInstallBuiltinTranslations() {}

#endif  // defined(__ANDROID__)

#endif  // VBAM_WX_BUILTIN_TRANSLATIONS_H_
