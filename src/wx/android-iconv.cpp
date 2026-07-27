// Workaround for a bionic iconv() bug that hangs wxWidgets on Android.
//
// Android's iconv() decodes a UTF-8 source with mbrtoc32(), which returns 0 --
// not the byte count -- when the decoded character is U+0000. bionic uses that
// return value as "bytes consumed", so on an embedded or trailing NUL it never
// advances the input: it spins internally until the output buffer is full, then
// returns (size_t)-1/E2BIG having consumed no input and emitted only garbage.
//
// wxMBConv_iconv::ToWChar() sizes its destination with
//     do { ... cres = iconv(...); } while (cres == (size_t)-1 && errno == E2BIG);
// which assumes iconv() makes progress whenever it reports E2BIG. It doesn't
// here, so the loop never terminates. wx converts strings including their
// terminating NUL, so *every* wxString conversion through iconv hangs the app
// at 100% CPU -- on Android that happens during startup and the window never
// paints (blank screen).
//
// Only the UTF-8 *decoder* is affected; every other source encoding bionic
// supports (ASCII, UTF-16LE/BE, UTF-32LE/BE, WCHAR_T) handles U+0000 correctly,
// and no target encoding is affected.
//
// These are linked in via -Wl,--wrap=iconv{,_open,_close} (see CMakeLists.txt),
// which rewrites the references in the wxWidgets static libraries to call the
// wrappers below. __real_* refer to bionic's originals.

#if defined(__ANDROID__)

#include <ctype.h>
#include <errno.h>
#include <iconv.h>
#include <stdlib.h>
#include <string.h>

extern "C" {

iconv_t __real_iconv_open(const char* tocode, const char* fromcode);
size_t __real_iconv(iconv_t cd, char** inbuf, size_t* inbytesleft, char** outbuf,
                    size_t* outbytesleft);
int __real_iconv_close(iconv_t cd);

}  // extern "C"

namespace {

struct VbamIconv {
    iconv_t real;
    bool src_is_utf8;    // source encoding hits the bionic NUL bug
    size_t dst_nul_len;  // bytes U+0000 occupies in the target encoding
};

// True for the names bionic accepts as UTF-8. bionic matches encoding names
// case-insensitively and ignores '-'/'_', and allows a "//TRANSLIT"-style
// suffix, so normalize the same way before comparing.
bool IsUtf8Name(const char* name) {
    if (!name) {
        return false;
    }
    char buf[8];
    size_t n = 0;
    for (const char* p = name; *p; ++p) {
        if (*p == '/') {
            break;  // strip //TRANSLIT, //IGNORE
        }
        if (*p == '-' || *p == '_') {
            continue;
        }
        if (n + 1 >= sizeof(buf)) {
            return false;  // longer than "utf8", cannot match
        }
        buf[n++] = static_cast<char>(tolower(static_cast<unsigned char>(*p)));
    }
    buf[n] = '\0';
    return strcmp(buf, "utf8") == 0;
}

// Measures how many bytes U+0000 occupies in `tocode` by converting a single
// NUL from UTF-32LE, a source encoding bionic decodes correctly. Falls back to
// one byte, which is right for every single-byte target.
size_t ProbeNulLen(const char* tocode) {
    const iconv_t probe = __real_iconv_open(tocode, "UTF-32LE");
    if (probe == reinterpret_cast<iconv_t>(-1)) {
        return 1;
    }

    char in[4] = {0, 0, 0, 0};
    char out[16];
    char* in_ptr = in;
    char* out_ptr = out;
    size_t in_left = sizeof(in);
    size_t out_left = sizeof(out);

    size_t len = 1;
    if (__real_iconv(probe, &in_ptr, &in_left, &out_ptr, &out_left) != static_cast<size_t>(-1) &&
        in_left == 0) {
        const size_t emitted = sizeof(out) - out_left;
        if (emitted > 0) {
            len = emitted;
        }
    }

    __real_iconv_close(probe);
    return len;
}

}  // namespace

extern "C" {

iconv_t __wrap_iconv_open(const char* tocode, const char* fromcode) {
    const iconv_t real = __real_iconv_open(tocode, fromcode);
    if (real == reinterpret_cast<iconv_t>(-1)) {
        return real;
    }

    VbamIconv* cd = static_cast<VbamIconv*>(malloc(sizeof(VbamIconv)));
    if (!cd) {
        __real_iconv_close(real);
        errno = ENOMEM;
        return reinterpret_cast<iconv_t>(-1);
    }

    cd->real = real;
    cd->src_is_utf8 = IsUtf8Name(fromcode);
    cd->dst_nul_len = cd->src_is_utf8 ? ProbeNulLen(tocode) : 1;
    return reinterpret_cast<iconv_t>(cd);
}

int __wrap_iconv_close(iconv_t cd) {
    if (cd == reinterpret_cast<iconv_t>(-1) || !cd) {
        errno = EBADF;
        return -1;
    }
    VbamIconv* conv = reinterpret_cast<VbamIconv*>(cd);
    const int result = __real_iconv_close(conv->real);
    free(conv);
    return result;
}

size_t __wrap_iconv(iconv_t cd, char** inbuf, size_t* inbytesleft, char** outbuf,
                    size_t* outbytesleft) {
    if (cd == reinterpret_cast<iconv_t>(-1) || !cd) {
        errno = EBADF;
        return static_cast<size_t>(-1);
    }
    VbamIconv* conv = reinterpret_cast<VbamIconv*>(cd);

    // A null input buffer is a state-reset request, and non-UTF-8 sources decode
    // NULs correctly; both go straight through.
    if (!conv->src_is_utf8 || !inbuf || !*inbuf || !inbytesleft) {
        return __real_iconv(conv->real, inbuf, inbytesleft, outbuf, outbytesleft);
    }

    size_t nonreversible = 0;
    for (;;) {
        // Hand bionic only the run of bytes before the next NUL. A NUL byte is
        // never part of a multi-byte UTF-8 sequence, so splitting there always
        // lands on a character boundary.
        const char* nul = static_cast<const char*>(memchr(*inbuf, '\0', *inbytesleft));
        const size_t chunk = nul ? static_cast<size_t>(nul - *inbuf) : *inbytesleft;

        if (chunk > 0) {
            size_t chunk_left = chunk;
            const size_t result =
                __real_iconv(conv->real, inbuf, &chunk_left, outbuf, outbytesleft);
            *inbytesleft -= chunk - chunk_left;
            if (result == static_cast<size_t>(-1)) {
                // errno is bionic's (E2BIG/EILSEQ/EINVAL) and the caller's
                // pointers have been advanced past what was converted.
                return result;
            }
            nonreversible += result;
        }

        if (!nul) {
            return nonreversible;
        }

        // Encode U+0000 ourselves: it is all-zero bytes in every encoding
        // bionic supports.
        if (!outbuf || !*outbuf || !outbytesleft || *outbytesleft < conv->dst_nul_len) {
            errno = E2BIG;
            return static_cast<size_t>(-1);
        }
        memset(*outbuf, 0, conv->dst_nul_len);
        *outbuf += conv->dst_nul_len;
        *outbytesleft -= conv->dst_nul_len;
        *inbuf += 1;
        *inbytesleft -= 1;
    }
}

}  // extern "C"

#endif  // defined(__ANDROID__)
