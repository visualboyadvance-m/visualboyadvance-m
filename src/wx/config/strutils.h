#ifndef VBAM_WX_CONFIG_STRUTILS_H_
#define VBAM_WX_CONFIG_STRUTILS_H_

#include <cstdint>
#include <vector>

#include <wx/string.h>
#include <wx/arrstr.h>

namespace config {

// From: https://stackoverflow.com/a/7408245/262458
wxArrayString str_split(const wxString& text, const wxString& sep, bool empty_token_is_sep=false);

// Same as above, but it includes the sep dir.
// If "A,,,B" is the text and "," is sep, then
// 'A', ',' and 'B' will be in the output.
wxArrayString str_split_with_sep(const wxString& text, const wxString& sep);

// Converts a null-terminated array of UTF-16 code units to a vector of UTF-8 code units.
// This will assert if the input is not a valid UTF-16 string.
std::vector<uint8_t> utf16_to_utf8_vector(const uint16_t* utf16);

// Convenience functions to convert a null-terminated array of UTF-16 code units to a wxString.
wxString utf16_to_utf8(const uint16_t* utf16);
wxString utf16_to_utf8(const int16_t* utf16);

} // namespace config

#endif  // VBAM_WX_CONFIG_STRUTILS_H_
