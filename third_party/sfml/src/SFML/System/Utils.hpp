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
// Headers
////////////////////////////////////////////////////////////
#include "../../../include/SFML/System/Export.hpp"

#include <array>
#include "filesystem.hpp"
#include <string>

#include <cstddef>
#include <cstdio>


namespace sf
{
[[nodiscard]] SFML_SYSTEM_API std::string toLower(std::string str);
[[nodiscard]] SFML_SYSTEM_API std::string formatDebugPathInfo(const ghc::filesystem::path& path);

// Convert byte sequence into integer
// toInteger<int>(0x12, 0x34, 0x56) == 0x563412
template <typename IntegerType>
[[nodiscard]] constexpr IntegerType toInteger(std::array<unsigned char,8> bytes)
{
    return (((IntegerType)bytes[0] <<  0)+
            ((IntegerType)bytes[1] <<  8)+
            ((IntegerType)bytes[2] << 16)+
            ((IntegerType)bytes[3] << 24)+
            ((IntegerType)bytes[4] << 32)+
            ((IntegerType)bytes[5] << 40)+
            ((IntegerType)bytes[6] << 48)+
            ((IntegerType)bytes[7] << 56));
}

[[nodiscard]] SFML_SYSTEM_API std::FILE* openFile(const ghc::filesystem::path& filename, std::string mode);
} // namespace sf
