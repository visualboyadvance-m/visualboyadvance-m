#include "qt/config/strutils.h"

#include <cstdint>

#include "core/base/check.h"

namespace config {

// From: https://stackoverflow.com/a/7408245/262458
//
// Modified to ignore empty tokens or return sep for them.
QStringList str_split(const QString& text, const QString& sep, bool empty_token_is_sep) {
    QStringList tokens;
    int start = 0, end = 0;

    while ((end = text.indexOf(sep, start)) != -1) {
        const QString token = text.mid(start, end - start);

        if (!token.isEmpty())
            tokens.append(token);
        else if (empty_token_is_sep)
            tokens.append(sep);

        start = end + sep.length();
    }

    // Last token.
    const QString token = text.mid(start);
    if (!token.isEmpty())
        tokens.append(token);
    else if (empty_token_is_sep)
        tokens.append(sep);

    return tokens;
}

QStringList str_split_with_sep(const QString& text, const QString& sep) {
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

QString utf16_to_utf8(const uint16_t* utf16) {
    std::vector<uint8_t> output_vector = utf16_to_utf8_vector(utf16);
    return QString::fromUtf8(reinterpret_cast<const char*>(output_vector.data()),
                             static_cast<int>(output_vector.size()));
}

QString utf16_to_utf8(const int16_t* utf16) {
    return utf16_to_utf8(reinterpret_cast<const uint16_t*>(utf16));
}

}  // namespace config
