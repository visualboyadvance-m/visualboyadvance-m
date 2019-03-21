#ifndef STRUTILS_H
#define STRUTILS_H

#include <wx/string.h>
#include <wx/arrstr.h>

// From: https://stackoverflow.com/a/7408245/262458
wxArrayString str_split(const wxString& text, const wxString& sep);

// From: https://stackoverflow.com/a/15099743/262458
size_t vec_find(wxArrayString& opts, const wxString& val);

#endif
