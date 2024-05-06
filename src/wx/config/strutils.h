#ifndef VBAM_WX_CONFIG_STRUTILS_H_
#define VBAM_WX_CONFIG_STRUTILS_H_

#include <wx/string.h>
#include <wx/arrstr.h>

namespace config {

// From: https://stackoverflow.com/a/7408245/262458
wxArrayString str_split(const wxString& text, const wxString& sep, bool empty_token_is_sep=false);

// Same as above, but it includes the sep dir.
// If "A,,,B" is the text and "," is sep, then
// 'A', ',' and 'B' will be in the output.
wxArrayString str_split_with_sep(const wxString& text, const wxString& sep);

} // namespace config

#endif  // VBAM_WX_CONFIG_STRUTILS_H_
