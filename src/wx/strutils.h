#ifndef STRUTILS_H
#define STRUTILS_H

#include <algorithm>
#include <wx/string.h>
#include <string>
#include <vector>

// From: https://stackoverflow.com/a/7408245/262458
std::vector<wxString> str_split(const wxString& text, const wxString& sep);

// From: https://stackoverflow.com/a/15099743/262458
std::size_t vec_find(std::vector<wxString>& opts, const wxString& val);

#endif
