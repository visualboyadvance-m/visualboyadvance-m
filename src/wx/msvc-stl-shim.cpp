// Link shim for the MSVC STL vectorized-algorithm helpers (__std_search_1
// and friends). The vcpkg prebuilt wxWidgets and openal-soft libraries were
// produced with a newer MSVC toolset whose static runtime exports these;
// the local 14.40 libcpmt.lib does not have them, so the GUI exe fails to
// link with LNK2001. The implementations below mirror the observable
// semantics of microsoft/STL stl/src/vector_algorithms.cpp, scalar edition.

#include <cstdint>

extern "C" {

// Returns the index of the first element of [_Haystack, _Haystack +
// _Haystack_length) that compares equal to any element of the needle, or
// (size_t)-1 when there is no such element.
__declspec(noalias) size_t __stdcall __std_find_first_of_trivial_pos_1(const void* const _Haystack,
    const size_t _Haystack_length, const void* const _Needle, const size_t _Needle_length) noexcept {
    const uint8_t* const first = static_cast<const uint8_t*>(_Haystack);
    const uint8_t* const nfirst = static_cast<const uint8_t*>(_Needle);
    const uint8_t* const nlast = nfirst + _Needle_length;

    for (size_t i = 0; i < _Haystack_length; ++i) {
        for (const uint8_t* n = nfirst; n != nlast; ++n) {
            if (*n == first[i]) {
                return i;
            }
        }
    }

    return static_cast<size_t>(-1);
}

__declspec(noalias) size_t __stdcall __std_find_first_of_trivial_pos_2(const void* const _Haystack,
    const size_t _Haystack_length, const void* const _Needle, const size_t _Needle_length) noexcept {
    const uint16_t* const first = static_cast<const uint16_t*>(_Haystack);
    const uint16_t* const nfirst = static_cast<const uint16_t*>(_Needle);
    const uint16_t* const nlast = nfirst + _Needle_length;

    for (size_t i = 0; i < _Haystack_length; ++i) {
        for (const uint16_t* n = nfirst; n != nlast; ++n) {
            if (*n == first[i]) {
                return i;
            }
        }
    }

    return static_cast<size_t>(-1);
}

// Same as above, but for the last matching element.
__declspec(noalias) size_t __stdcall __std_find_last_of_trivial_pos_2(const void* const _Haystack,
    const size_t _Haystack_length, const void* const _Needle, const size_t _Needle_length) noexcept {
    const uint16_t* const first = static_cast<const uint16_t*>(_Haystack);
    const uint16_t* const nfirst = static_cast<const uint16_t*>(_Needle);
    const uint16_t* const nlast = nfirst + _Needle_length;

    for (size_t i = _Haystack_length; i != 0;) {
        --i;
        for (const uint16_t* n = nfirst; n != nlast; ++n) {
            if (*n == first[i]) {
                return i;
            }
        }
    }

    return static_cast<size_t>(-1);
}

// Returns a pointer to the first occurrence of the needle subsequence
// [_First2, _First2 + _Count2) inside [_First1, _Last1), or _Last1 when
// there is no occurrence. An empty needle matches at _First1.
const void* __stdcall __std_search_1(
    const void* const _First1, const void* const _Last1, const void* const _First2, const size_t _Count2) noexcept {
    const uint8_t* const first1 = static_cast<const uint8_t*>(_First1);
    const uint8_t* const first2 = static_cast<const uint8_t*>(_First2);
    const size_t count1 = static_cast<const uint8_t*>(_Last1) - first1;

    if (_Count2 > count1) {
        return _Last1;
    }

    for (size_t i = 0; i + _Count2 <= count1; ++i) {
        size_t j = 0;
        while (j < _Count2 && first1[i + j] == first2[j]) {
            ++j;
        }
        if (j == _Count2) {
            return first1 + i;
        }
    }

    return _Last1;
}

const void* __stdcall __std_search_2(
    const void* const _First1, const void* const _Last1, const void* const _First2, const size_t _Count2) noexcept {
    const uint16_t* const first1 = static_cast<const uint16_t*>(_First1);
    const uint16_t* const first2 = static_cast<const uint16_t*>(_First2);
    const size_t count1 = static_cast<const uint16_t*>(_Last1) - first1;

    if (_Count2 > count1) {
        return _Last1;
    }

    for (size_t i = 0; i + _Count2 <= count1; ++i) {
        size_t j = 0;
        while (j < _Count2 && first1[i + j] == first2[j]) {
            ++j;
        }
        if (j == _Count2) {
            return first1 + i;
        }
    }

    return _Last1;
}

// Eliminates every byte equal to _Val from [_First, _Last) by shifting the
// remaining bytes left; returns the new logical end pointer.
void* __stdcall __std_remove_1(void* _First, void* const _Last, const uint8_t _Val) noexcept {
    uint8_t* dest = static_cast<uint8_t*>(_First);

    for (const uint8_t* src = dest; src != _Last; ++src) {
        if (*src != _Val) {
            *dest = *src;
            ++dest;
        }
    }

    return dest;
}

} // extern "C"
