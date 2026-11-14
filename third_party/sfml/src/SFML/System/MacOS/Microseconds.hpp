////////////////////////////////////////////////////////////
//
// SFML - Simple and Fast Multimedia Library
// Copyright (C) 2007-2025 Laurent Gomila (laurent@sfml-dev.org)
//
// This software is provided 'as-is', without any express or implied warranty.
// In no event will the authors be held liable for any damages arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it freely,
// subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented;
//    you must not claim that you wrote the original software.
//    If you use this software in a product, an acknowledgment
//    in the product documentation would be appreciated but is not required.
//
// 2. Altered source versions must be plainly marked as such,
//    and must not be misrepresented as being the original software.
//
// 3. This notice may not be removed or altered from any source distribution.
//
////////////////////////////////////////////////////////////

#pragma once

////////////////////////////////////////////////////////////
// The Time Manager's Microseconds() for classic Mac OS (Retro68).
//
// Multiversal Interfaces do not declare it. On 68k it is trap A193, which
// answers in A0:D0 (high:low); the glue stores both through the argument,
// as Universal Interfaces' FOURWORDINLINE does. On PowerPC it is an
// InterfaceLib export.
////////////////////////////////////////////////////////////
#include <cstdint>

namespace sf::priv
{
struct MacUnsignedWide
{
    std::uint32_t hi;
    std::uint32_t lo;
};
} // namespace sf::priv

extern "C"
{
#if defined(__m68k__)
    pascal void sfmlMacMicroseconds(sf::priv::MacUnsignedWide* microTickCount) = {0xA193, 0x225F, 0x22C8, 0x2280};
#else
    void Microseconds(sf::priv::MacUnsignedWide* microTickCount);
#define sfmlMacMicroseconds Microseconds
#endif
}

namespace sf::priv
{
////////////////////////////////////////////////////////////
/// \brief Microseconds since startup
///
////////////////////////////////////////////////////////////
inline std::uint64_t macMicroseconds()
{
    MacUnsignedWide now{};
    sfmlMacMicroseconds(&now);
    return (static_cast<std::uint64_t>(now.hi) << 32) | now.lo;
}
} // namespace sf::priv
