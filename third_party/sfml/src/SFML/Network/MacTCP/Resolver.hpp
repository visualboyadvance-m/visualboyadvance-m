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

#include <cstdint>

namespace sf::priv
{
////////////////////////////////////////////////////////////
/// \brief Look a host name up through the MacTCP resolver
///
/// \param name    Host name (or dotted address)
/// \param address Receives the address, host byte order
///
/// \return 0, or an EAI_* code
///
////////////////////////////////////////////////////////////
int resolveName(const char* name, std::uint32_t& address);

////////////////////////////////////////////////////////////
/// \brief Close the resolver if it was opened
///
////////////////////////////////////////////////////////////
void closeResolver();

////////////////////////////////////////////////////////////
/// \brief Nap between polls of the network (Sockets.cpp)
///
/// \return false when the idle function (mactcp_set_idle_func)
///         cancels the wait
///
////////////////////////////////////////////////////////////
bool idleNap();

} // namespace sf::priv
