#include "strutils.h"

#include <wx/tokenzr.h>

namespace strutils {

// From: https://stackoverflow.com/a/7408245/262458
//
// Modified to ignore empty tokens or return sep for them.
wxArrayString split(const wxString& text, const wxString& sep, bool empty_token_is_sep) {
    wxArrayString tokens;
    size_t start = 0, end = 0;

    while ((end = text.find(sep, start)) != std::string::npos) {
        wxString token = text.substr(start, end - start);

        if (token.length())
	    tokens.Add(token);
        else if (empty_token_is_sep)
            tokens.Add(sep);

	start = end + sep.length();
    }

    // Last token.
    wxString token = text.substr(start);
    if (token.length())
        tokens.Add(token);
    else if (empty_token_is_sep)
        tokens.Add(sep);

    return tokens;
}

wxArrayString split_with_sep(const wxString& text, const wxString& sep)
{
    return split(text, sep, true);
}

} // namespace strutils
