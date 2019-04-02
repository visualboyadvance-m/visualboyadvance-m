#include "strutils.h"

// From: https://stackoverflow.com/a/7408245/262458
//
// modified to ignore empty tokens
wxArrayString str_split(const wxString& text, const wxString& sep) {
    wxArrayString tokens;
    size_t start = 0, end = 0;

    while ((end = text.find(sep, start)) != std::string::npos) {
        wxString token = text.substr(start, end - start);

        if (token.length())
	    tokens.Add(token);

	start = end + 1;
    }

    tokens.Add(text.substr(start));

    return tokens;
}

size_t vec_find(wxArrayString& opts, const wxString& val) {
    return opts.Index(val);
}
