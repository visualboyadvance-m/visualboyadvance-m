#include "wx/config/strutils.h"

#include <cstdint>

#include <wx/tokenzr.h>

#include "core/base/check.h"

namespace config {

// From: https://stackoverflow.com/a/7408245/262458
//
// Modified to ignore empty tokens or return sep for them.
wxArrayString str_split(const wxString& text, const wxString& sep, bool empty_token_is_sep) {
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

wxArrayString str_split_with_sep(const wxString& text, const wxString& sep)
{
    return str_split(text, sep, true);
}

std::vector<uint8_t> utf16_to_utf8_vector(const uint16_t* utf16) {
    std::vector<uint8_t> out;
    for (size_t i = 0; utf16[i]; i++) {
        const uint16_t c = utf16[i];
        if (c < 0x80) {
            out.push_back(c);
        } else if (c < 0x800) {
            out.push_back(0xC0 | (c >> 6));
            out.push_back(0x80 | (c & 0x3F));
        } else if (c < 0xD800 || c >= 0xE000) {
            // Regular 3-byte UTF-8 character.
            out.push_back(0xE0 | (c >> 12));
            out.push_back(0x80 | ((c >> 6) & 0x3F));
            out.push_back(0x80 | (c & 0x3F));
        } else {
            // Surrogate pair, construct the original code point.
            const uint32_t high = c;

            // The next code unit must be a low surrogate.
            i++;
            const uint32_t low = utf16[i];
            VBAM_CHECK(low);
            VBAM_CHECK(low >= 0xDC00 && low < 0xE000);

            const uint32_t codepoint = 0x10000 + ((high & 0x3FF) << 10) + (low & 0x3FF);

            // Convert to UTF-8.
            out.push_back(0xF0 | (codepoint >> 18));
            out.push_back(0x80 | ((codepoint >> 12) & 0x3F));
            out.push_back(0x80 | ((codepoint >> 6) & 0x3F));
            out.push_back(0x80 | (codepoint & 0x3F));
        }
    }
    return out;
}

wxString utf16_to_utf8(const uint16_t* utf16) {
    std::vector<uint8_t> output_vector = utf16_to_utf8_vector(utf16);
    return wxString::FromUTF8(reinterpret_cast<const char*>(output_vector.data()),
                              output_vector.size());
}

wxString utf16_to_utf8(const int16_t* utf16) {
    return utf16_to_utf8(reinterpret_cast<const uint16_t*>(utf16));
}

} // namespace config
