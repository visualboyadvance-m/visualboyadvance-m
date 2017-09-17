# UseGCCBinUtilsWrappers.cmake
#
# Use gcc binutils wrappers such as gcc-ar, this may be necessary for LTO.
#
# To use:
#
# put a copy into your <project_root>/CMakeScripts/
#
# In your main CMakeLists.txt add the command:
#
# INCLUDE(UseGCCBinUtilsWrappers)
#
# BSD 2-Clause License
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

# only do this when compiling with gcc/g++
IF(NOT (CMAKE_COMPILER_IS_GNUCXX OR CMAKE_COMPILER_IS_GNUC))
    RETURN()
ENDIF()

# first try appending -util to basename of compiler
STRING(REGEX MATCH   "\\.(exe|EXE)$"    GCC_EXE_SUFFIX ${CMAKE_C_COMPILER})
STRING(REGEX REPLACE "\\.(exe|EXE)$" "" GCC_BASENAME   ${CMAKE_C_COMPILER})

SET(GCC_AR      "${GCC_BASENAME}-ar${GCC_EXE_SUFFIX}")
SET(GCC_NM      "${GCC_BASENAME}-nm${GCC_EXE_SUFFIX}")
SET(GCC_RANLIB  "${GCC_BASENAME}-ranlib${GCC_EXE_SUFFIX}")

# if that does not work, try looking for gcc-util in the compiler directory,
# and failing that in the PATH

GET_FILENAME_COMPONENT(GCC_DIRNAME ${CMAKE_C_COMPILER} DIRECTORY)

IF(NOT EXISTS ${GCC_AR})
    UNSET(GCC_AR)

    FIND_PROGRAM(GCC_AR NAMES gcc-ar gcc-ar.exe GCC-AR.EXE HINTS ${GCC_DIRNAME})
ENDIF()

IF(NOT EXISTS ${GCC_NM})
    UNSET(GCC_NM)

    FIND_PROGRAM(GCC_NM NAMES gcc-nm gcc-nm.exe GCC-NM.EXE HINTS ${GCC_DIRNAME})
ENDIF()

IF(NOT EXISTS ${GCC_RANLIB})
    UNSET(GCC_RANLIB)

    FIND_PROGRAM(GCC_RANLIB NAMES gcc-ranlib gcc-ranlib.exe GCC-RANLIB.EXE HINTS ${GCC_DIRNAME})
ENDIF()

INCLUDE(PathRun)

IF(EXISTS ${GCC_AR})
    MESSAGE("-- Found gcc-ar: ${GCC_AR}")

    SET(target "${CMAKE_BINARY_DIR}/gcc-ar-wrap")
    MAKE_PATH_RUN_WRAPPER("${GCC_AR}" "${target}")
    SET(CMAKE_AR "${target}")
ENDIF()

IF(EXISTS ${GCC_NM})
    MESSAGE("-- Found gcc-nm: ${GCC_NM}")

    SET(target "${CMAKE_BINARY_DIR}/gcc-nm-wrap")
    MAKE_PATH_RUN_WRAPPER("${GCC_NM}" "${target}")
    SET(CMAKE_NM "${target}")
ENDIF()

IF(EXISTS ${GCC_RANLIB})
    MESSAGE("-- Found gcc-ranlib: ${GCC_RANLIB}")

    SET(target "${CMAKE_BINARY_DIR}/gcc-ranlib-wrap")
    MAKE_PATH_RUN_WRAPPER("${GCC_RANLIB}" "${target}")
    SET(CMAKE_RANLIB "${target}")
ENDIF()

FOREACH(VAR "GCC_AR" "GCC_NM" "GCC_RANLIB" "GCC_DIRNAME" "GCC_BASENAME" "GCC_EXE_SUFFIX" "target")
    UNSET(${VAR})
ENDFOREACH()
