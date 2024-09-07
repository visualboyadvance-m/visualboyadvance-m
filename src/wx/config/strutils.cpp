#include "wx/config/strutils.h"

#include <cerrno>
#include <cstdint>

#include <iconv.h>
#include <wx/tokenzr.h>

#include "core/base/check.h"

namespace config {

namespace {

class ScopedIconv {
public:
    explicit ScopedIconv(iconv_t converter) : converter_(converter) {
        VBAM_CHECK(converter_ != reinterpret_cast<iconv_t>(-1));
    }
    ~ScopedIconv() { iconv_close(converter_); }

    iconv_t get() const { return converter_; }

private:
    iconv_t converter_;
};

}  // namespace

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

std::vector<uint8_t> utf16_to_utf8_vector(char16_t* utf16) {
    ScopedIconv converter(iconv_open("UTF-8", "UTF-16LE"));
    size_t input_remaining = std::char_traits<char16_t>::length(utf16) * sizeof(char16_t);
    char* input_buffer = reinterpret_cast<char*>(utf16);
    size_t output_remaining = input_remaining;
    std::vector<uint8_t> output_vector(output_remaining, 0);
    char* output_buffer = reinterpret_cast<char*>(output_vector.data());
    size_t output_size = 0;
    while (input_remaining != 0) {
        const size_t original_output_remaining = output_remaining;
        const size_t original_output_size = output_vector.size();
        const size_t result = iconv(converter.get(), &input_buffer, &input_remaining,
                                    &output_buffer, &output_remaining);
        if (result == static_cast<size_t>(-1)) {
            switch (errno) {
                case E2BIG:
                    // Output buffer is full, resize it.
                    output_vector.resize(original_output_size * 2);
                    output_buffer = reinterpret_cast<char*>(output_vector.data()) +
                                    original_output_size - output_remaining;
                    output_size += original_output_remaining - output_remaining;
                    output_remaining =
                        output_remaining + output_vector.size() - original_output_size;
                    break;
                case EILSEQ:
                    // Invalid input sequence.
                    VBAM_CHECK(false);
                    break;
                case EINVAL:
                    // Incomplete input sequence.
                    VBAM_CHECK(false);
                    break;
                default:
                    VBAM_NOTREACHED();
            }
        } else {
            output_size += original_output_remaining - output_remaining;
        }
    }
    output_vector.resize(output_size);
    return output_vector;
}

wxString utf16_to_utf8(char16_t* utf16) {
    std::vector<uint8_t> output_vector = utf16_to_utf8_vector(utf16);
    return wxString::FromUTF8(reinterpret_cast<const char*>(output_vector.data()),
                              output_vector.size());
}

} // namespace config
