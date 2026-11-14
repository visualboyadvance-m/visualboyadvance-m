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

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "SleepImpl.hpp"

#include "../../../../include/SFML/System/Time.hpp"

#include <unistd.h>


namespace sf
{
////////////////////////////////////////////////////////////
void sleepImpl(Time time)
{
    // newlib on Retro68 declares usleep() but its libc has none: the
    // application supplies it (the wx port's posix-compat.cpp waits through
    // SDL, which keeps the sound mixing on this, the only thread, and lets
    // other applications run). Long waits are split so that a 16-bit-minded
    // implementation never sees an overflowing count.
    std::int64_t usecs = time.asMicroseconds();
    while (usecs > 0)
    {
        const std::int64_t chunk = usecs > 1'000'000 ? 1'000'000 : usecs;
        usleep(static_cast<useconds_t>(chunk));
        usecs -= chunk;
    }
}

} // namespace sf
