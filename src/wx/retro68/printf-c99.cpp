// C99 length modifiers for the printf family on classic Mac OS (Retro68).
//
// Retro68's newlib is built without C99 formats, so it does not know the z,
// t and j length modifiers: "%zu" comes out as "zu" and swallows no argument.
// wxString::Format() ends up in the same newlib code (vswprintf), so this
// broke names the program looks things up by -- the joypad sections of the
// configuration file were all written as "Joypad/zu", and the GB palette
// panels' "Color%zu" controls could not be found, which crashed the program.
//
// The linker sends newlib's printf workers here (--wrap, see CMakeLists.txt),
// and the format is rewritten in the modifiers newlib does know before it
// gets it: size_t and ptrdiff_t are long on both the 68k and the PowerPC
// target and intmax_t is long long, so z and t become l and j becomes ll,
// which read the same arguments.

#include <stdarg.h>
#include <stdio.h>
#include <wchar.h>

#include <memory>

#include "wx/retro68/wrap-glue.h"

namespace {

// Room for the formats a program normally has; longer ones get a heap copy.
constexpr size_t kStackFormat = 256;

template <typename Char>
bool NeedsRewrite(const Char* fmt) {
    for (const Char* p = fmt; *p; ++p) {
        if (*p != '%')
            continue;
        ++p;
        while (*p && (*p == '-' || *p == '+' || *p == ' ' || *p == '#' || *p == '*' || *p == '.' ||
                      *p == '$' || *p == '\'' || (*p >= '0' && *p <= '9')))
            ++p;
        if (*p == 'z' || *p == 't' || *p == 'j')
            return true;
        if (!*p)
            break;
    }
    return false;
}

// Writes the rewritten format to out, which has room for 2 * strlen(fmt) + 1
// characters (j is the only modifier that grows, by one).
template <typename Char>
void Rewrite(const Char* fmt, Char* out) {
    const Char* p = fmt;
    while (*p) {
        if (*p != '%') {
            *out++ = *p++;
            continue;
        }
        *out++ = *p++;
        while (*p && (*p == '-' || *p == '+' || *p == ' ' || *p == '#' || *p == '*' || *p == '.' ||
                      *p == '$' || *p == '\'' || (*p >= '0' && *p <= '9')))
            *out++ = *p++;
        if (*p == 'z' || *p == 't') {
            *out++ = 'l';
            ++p;
        } else if (*p == 'j') {
            *out++ = 'l';
            *out++ = 'l';
            ++p;
        }
        // The conversion character, or the second '%' of "%%".
        if (*p)
            *out++ = *p++;
    }
    *out = 0;
}

template <typename Char>
size_t Length(const Char* s) {
    size_t n = 0;
    while (s[n])
        ++n;
    return n;
}

// Calls print with fmt, or with a rewritten copy of it when it has a C99
// length modifier.
template <typename Char, typename Print>
int WithC99Format(const Char* fmt, Print print) {
    if (!fmt || !NeedsRewrite(fmt))
        return print(fmt);

    const size_t room = 2 * Length(fmt) + 1;
    Char stack_buf[kStackFormat];
    std::unique_ptr<Char[]> heap_buf;
    Char* buf = stack_buf;
    if (room > kStackFormat) {
        heap_buf.reset(new (std::nothrow) Char[room]);
        if (!heap_buf)
            return print(fmt);
        buf = heap_buf.get();
    }
    Rewrite(fmt, buf);
    return print(buf);
}

}  // namespace

extern "C" {

struct _reent;

// The linker sends newlib's calls to these through the glue below, and they
// call the real workers through vbam_real_*. A va_list is passed on as it is:
// the rewritten format reads the same arguments in the same order.

int vbam_real__svfprintf_r(struct _reent*, FILE*, const char*, va_list);
int vbam_real__vfprintf_r(struct _reent*, FILE*, const char*, va_list);
int vbam_real__svfwprintf_r(struct _reent*, FILE*, const wchar_t*, va_list);
int vbam_real__vfwprintf_r(struct _reent*, FILE*, const wchar_t*, va_list);
int vbam_real_vfprintf(FILE*, const char*, va_list);
int vbam_real_vfwprintf(FILE*, const wchar_t*, va_list);

int vbam_wrap__svfprintf_r(struct _reent* r, FILE* f, const char* fmt, va_list ap) {
    return WithC99Format(fmt, [&](const char* x) { return vbam_real__svfprintf_r(r, f, x, ap); });
}

int vbam_wrap__vfprintf_r(struct _reent* r, FILE* f, const char* fmt, va_list ap) {
    return WithC99Format(fmt, [&](const char* x) { return vbam_real__vfprintf_r(r, f, x, ap); });
}

int vbam_wrap__svfwprintf_r(struct _reent* r, FILE* f, const wchar_t* fmt, va_list ap) {
    return WithC99Format(fmt, [&](const wchar_t* x) { return vbam_real__svfwprintf_r(r, f, x, ap); });
}

int vbam_wrap__vfwprintf_r(struct _reent* r, FILE* f, const wchar_t* fmt, va_list ap) {
    return WithC99Format(fmt, [&](const wchar_t* x) { return vbam_real__vfwprintf_r(r, f, x, ap); });
}

// These two call their _r worker inside newlib's own object, out of reach of
// --wrap, so they are wrapped themselves.
int vbam_wrap_vfprintf(FILE* f, const char* fmt, va_list ap) {
    return WithC99Format(fmt, [&](const char* x) { return vbam_real_vfprintf(f, x, ap); });
}

int vbam_wrap_vfwprintf(FILE* f, const wchar_t* fmt, va_list ap) {
    return WithC99Format(fmt, [&](const wchar_t* x) { return vbam_real_vfwprintf(f, x, ap); });
}

}  // extern "C"

// See wrap-glue.h.
VBAM_WRAP_GLUE_BEGIN
VBAM_WRAP_GLUE(_svfprintf_r)
VBAM_WRAP_GLUE(_vfprintf_r)
VBAM_WRAP_GLUE(_svfwprintf_r)
VBAM_WRAP_GLUE(_vfwprintf_r)
VBAM_WRAP_GLUE(vfprintf)
VBAM_WRAP_GLUE(vfwprintf)
VBAM_WRAP_GLUE_END
