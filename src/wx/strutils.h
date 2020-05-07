#ifndef STRUTILS_H
#define STRUTILS_H

#include <wx/string.h>
#include <wx/arrstr.h>

// From: https://stackoverflow.com/a/7408245/262458
wxArrayString str_split(const wxString& text, const wxString& sep);

// Same as above, but it includes the sep dir.
// If "A,,,B" is the text and "," is sep, then
// 'A', ',' and 'B' will be in the output.
wxArrayString str_split_with_sep(const wxString& text, const wxString& sep);

// From: https://stackoverflow.com/a/15099743/262458
size_t vec_find(wxArrayString& opts, const wxString& val);

#endif
