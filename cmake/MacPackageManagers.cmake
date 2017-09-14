# This module checks for Mac Homebrew, MacPorts, or Fink, in that order, on Mac
# OS X.
#
# It will prepend the active package manager's paths to:
#
# CMAKE_FRAMEWORK_PATH
# CMAKE_INCLUDE_PATH
# CMAKE_LIBRARY_PATH
# CMAKE_PROGRAM_PATH
#
# In addition, the following commands are called with the package manager's
# paths:
#
# INCLUDE_DIRECTORIES()
# LINK_DIRECTORIES()
#
# The paths of package managers not currently in $ENV{PATH} are added to
# CMAKE_IGNORE_PATH .
#
# Copyright (c) 2016, Rafael Kitover
# All rights reserved.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
# 
# * Redistributions of source code must retain the above copyright notice, this
#   list of conditions and the following disclaimer.
# 
# * Redistributions in binary form must reproduce the above copyright notice,
#   this list of conditions and the following disclaimer in the documentation
#   and/or other materials provided with the distribution.
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

IF(NOT APPLE)
    RETURN()
ENDIF()

IF(EXISTS /usr/local/bin/brew AND $ENV{PATH} MATCHES "(^|:)/usr/local/bin/?(:|$)")
    MESSAGE("-- Configuring for Mac Homebrew")

    SET(MAC_HOMEBREW ON)

    SET(CMAKE_IGNORE_PATH /opt/local /opt/local/bin /opt/local/include /opt/local/Library/Frameworks /opt/local/lib ${CMAKE_IGNORE_PATH})
    SET(CMAKE_IGNORE_PATH /sw /sw/bin /sw/include /sw/Library/Frameworks /sw/lib ${CMAKE_IGNORE_PATH})

    SET(CMAKE_FRAMEWORK_PATH /usr/local/Frameworks ${CMAKE_FRAMEWORK_PATH})

    SET(CMAKE_INCLUDE_PATH /usr/local/include ${CMAKE_INCLUDE_PATH})
    INCLUDE_DIRECTORIES("/usr/local/include")

    SET(CMAKE_LIBRARY_PATH /usr/local/lib ${CMAKE_LIBRARY_PATH})
    LINK_DIRECTORIES("/usr/local/lib")

    SET(CMAKE_PROGRAM_PATH /usr/local/bin ${CMAKE_PROGRAM_PATH})
ELSEIF(EXISTS /opt/local/bin/port AND $ENV{PATH} MATCHES "(^|:)/opt/local/bin/?(:|$)")
    MESSAGE("-- Configuring for MacPorts")

    SET(MACPORTS ON)

    SET(CMAKE_IGNORE_PATH /sw /sw/bin /sw/include /sw/Library/Frameworks /sw/lib ${CMAKE_IGNORE_PATH})

    SET(CMAKE_FRAMEWORK_PATH /opt/local/Library/Frameworks ${CMAKE_FRAMEWORK_PATH})

    SET(CMAKE_INCLUDE_PATH /opt/local/include ${CMAKE_INCLUDE_PATH})
    INCLUDE_DIRECTORIES("/opt/local/include")

    SET(CMAKE_LIBRARY_PATH /opt/local/lib ${CMAKE_LIBRARY_PATH})
    LINK_DIRECTORIES("/opt/local/lib")

    SET(CMAKE_PROGRAM_PATH /opt/local/bin ${CMAKE_PROGRAM_PATH})
ELSEIF(EXISTS /sw/bin/fink AND $ENV{PATH} MATCHES "(^|:)/sw/bin/?(:|$)")
    MESSAGE("-- Configuring for Fink")

    SET(FINK ON)

    SET(CMAKE_IGNORE_PATH /opt/local /opt/local/bin /opt/local/include /opt/local/Library/Frameworks /opt/local/lib ${CMAKE_IGNORE_PATH})

    SET(CMAKE_FRAMEWORK_PATH /sw/Library/Frameworks ${CMAKE_FRAMEWORK_PATH})

    SET(CMAKE_INCLUDE_PATH /sw/include ${CMAKE_INCLUDE_PATH})
    INCLUDE_DIRECTORIES("/sw/include")

    SET(CMAKE_LIBRARY_PATH /sw/lib ${CMAKE_LIBRARY_PATH})
    LINK_DIRECTORIES("/sw/lib")

    SET(CMAKE_PROGRAM_PATH /sw/bin ${CMAKE_PROGRAM_PATH})
ELSE()
    # no package manager found or active, do nothing
    RETURN()
ENDIF()

# only ignore /usr/local if brew is installed and not in the PATH
# in other cases, it is the user's personal installations
IF(NOT MAC_HOMEBREW AND EXISTS /usr/local/bin/brew)
    SET(CMAKE_IGNORE_PATH /usr/local /usr/local/bin /usr/local/include /usr/local/Library/Frameworks /usr/local/lib ${CMAKE_IGNORE_PATH})
ENDIF()
