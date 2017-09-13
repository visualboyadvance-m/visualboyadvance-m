#include <algorithm>
#include <wx/string.h>
#include <string>
#include <vector>
#include "strutils.h"

// From: https://stackoverflow.com/a/7408245/262458
std::vector<wxString> str_split(const wxString& text, const wxString& sep) {
    std::vector<wxString> tokens;
    std::size_t start = 0, end = 0;

    while ((end = text.find(sep, start)) != std::string::npos) {
	tokens.push_back(text.substr(start, end - start));
	start = end + 1;
    }

    tokens.push_back(text.substr(start));

    return tokens;
}

// From: https://stackoverflow.com/a/15099743/262458
std::size_t vec_find(std::vector<wxString>& opts, const wxString& val) {
    auto it = std::find(opts.begin(), opts.end(), val);

    if (it == opts.end())
        return wxNOT_FOUND;

    return std::distance(opts.begin(), it);
}
