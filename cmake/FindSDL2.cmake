# This module defines
# SDL2_LIBRARY, the name of the library to link against
# SDL2_FOUND, if false, do not try to link to SDL2
# SDL2_INCLUDE_DIR, where to find SDL.h
#
# If you have pkg-config, these extra variables are also defined:
# SDL2_DEFINITIONS, extra CFLAGS
# SDL2_EXTRA_LIBS, extra link libs
# SDL2_LINKER_FLAGS, extra link flags
#
# The latter two are automatically added to SDL2_LIBRARY
#
# To use them, add code such as:
#
# # SET(SDL2_STATIC ON) # if you want to link SDL2 statically
# FIND_PACKAGE(SDL2 REQUIRED)
# ADD_DEFINITIONS(${SDL2_DEFINITIONS})
# TARGET_LINK_LIBRARIES(your-executable-target ${SDL2_LIBRARY} ...)
#
# This module responds to the the flag:
# SDL2_BUILDING_LIBRARY
# If this is defined, then no SDL2main will be linked in because
# only applications need main().
# Otherwise, it is assumed you are building an application and this
# module will attempt to locate and set the the proper link flags
# as part of the returned SDL2_LIBRARY variable.
#
# If you want to link SDL2 statically, set SDL2_STATIC to ON.
#
# Don't forget to include SDLmain.h and SDLmain.m your project for the
# OS X framework based version. (Other versions link to -lSDL2main which
# this module will try to find on your behalf.) Also for OS X, this
# module will automatically add the -framework Cocoa on your behalf.
#
#
# Additional Note: If you see an empty SDL2_LIBRARY_TEMP in your configuration
# and no SDL2_LIBRARY, it means CMake did not find your SDL2 library
# (SDL2.dll, libsdl2.so, SDL2.framework, etc).
# Set SDL2_LIBRARY_TEMP to point to your SDL2 library, and configure again.
# Similarly, if you see an empty SDL2MAIN_LIBRARY, you should set this value
# as appropriate. These values are used to generate the final SDL2_LIBRARY
# variable, but when these values are unset, SDL2_LIBRARY does not get created.
#
#
# $SDL2DIR is an environment variable that would
# correspond to the ./configure --prefix=$SDL2DIR
# used in building SDL2.
# l.e.galup  9-20-02
#
# Modified by Eric Wing.
# Added code to assist with automated building by using environmental variables
# and providing a more controlled/consistent search behavior.
# Added new modifications to recognize OS X frameworks and
# additional Unix paths (FreeBSD, etc).
# Also corrected the header search path to follow "proper" SDL guidelines.
# Added a search for SDL2main which is needed by some platforms.
# Added a search for threads which is needed by some platforms.
# Added needed compile switches for MinGW.
#
# On OSX, this will prefer the Framework version (if found) over others.
# People will have to manually change the cache values of
# SDL2_LIBRARY to override this selection or set the CMake environment
# CMAKE_INCLUDE_PATH to modify the search paths.
#
# Note that the header path has changed from SDL3/SDL.h to just SDL.h
# This needed to change because "proper" SDL convention
# is #include "SDL.h", not <SDL2/SDL.h>. This is done for portability
# reasons because not all systems place things in SDL2/ (see FreeBSD).

#=============================================================================
# Copyright 2003-2009 Kitware, Inc.
#
# Distributed under the OSI-approved BSD License (the "License");
# see accompanying file Copyright.txt for details.
#
# This software is distributed WITHOUT ANY WARRANTY; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the License for more information.
#=============================================================================
# (To distribute this file outside of CMake, substitute the full
#  License text for the above reference.)

SET(SDL2_SEARCH_PATHS
	~/Library/Frameworks
	/Library/Frameworks
	/usr/local # Mac Homebrew and local installs
	/usr
	/sw # Fink
	/opt/local # MacPorts
	/opt/csw # OpenCSW (Solaris)
	/opt
	${SDL2_PATH}/include
	${SDL2_PATH}/lib
)

FIND_PATH(SDL2_INCLUDE_DIR SDL.h
	HINTS $ENV{SDL2DIR}
	PATH_SUFFIXES SDL2
	PATHS ${SDL2_SEARCH_PATHS}
)

SET(CURRENT_FIND_LIBRARY_SUFFIXES ${CMAKE_FIND_LIBRARY_SUFFIXES})

if(SDL2_STATIC)
    if(MSVC)
	set(CMAKE_FIND_LIBRARY_SUFFIXES .lib)
    else()
	set(CMAKE_FIND_LIBRARY_SUFFIXES .a)
    endif()
endif()

unset(lib_suffixes)
if(MSVC)
    if(VCPKG_TARGET_TRIPLET MATCHES "-static$")
	list(APPEND lib_suffixes -static)
    endif()

    if(CMAKE_BUILD_TYPE MATCHES "^(Debug|RelWithDebInfo)$")
	list(APPEND lib_suffixes d)
    endif()
endif()

# Calculate combination of possible name+suffixes.
unset(names)
set(lib_name SDL2)
set(current ${lib_name})
foreach(suffix ${lib_suffixes})
    list(APPEND names "${current}${suffix}" "${lib_name}${suffix}")
    set(current "${current}${suffix}")
endforeach()

# Fallback to name by itself.
list(APPEND names ${lib_name})

FIND_LIBRARY(SDL2_LIBRARY_TEMP
    NAMES ${names}
    HINTS $ENV{SDL2DIR}
    PATH_SUFFIXES lib64 lib lib/x64 lib/x86
    PATHS ${SDL2_SEARCH_PATHS}
)

if(NOT (SDL2_BUILDING_LIBRARY OR ${SDL2_INCLUDE_DIR} MATCHES ".framework"))
    # Non-OS X framework versions expect you to also dynamically link to
    # SDL2main. This is mainly for Windows and OS X. Other (Unix) platforms
    # seem to provide SDL2main for compatibility even though they don't
    # necessarily need it.
    find_library(SDL2MAIN_LIBRARY
	NAMES SDL2main${lib_suffix}
	HINTS $ENV{SDL2DIR}
	PATH_SUFFIXES lib64 lib lib/x64 lib/x86
	PATHS ${SDL2_SEARCH_PATHS}
    )
endif()

SET(CMAKE_FIND_LIBRARY_SUFFIXES ${CURRENT_FIND_LIBRARY_SUFFIXES})
UNSET(CURRENT_FIND_LIBRARY_SUFFIXES)

# SDL2 may require threads on your system.
# The Apple build may not need an explicit flag because one of the
# frameworks may already provide it.
# But for non-OSX systems, I will use the CMake Threads package.
IF(NOT APPLE)
	FIND_PACKAGE(Threads)
ENDIF(NOT APPLE)

# MinGW needs an additional link flag, -mwindows (to make a GUI app)
# but we only add it when not making a Debug build.
# It's total link flags should look like -lmingw32 -lSDL2main -lSDL2 -mwindows
IF(MINGW)
	SET(MINGW32_LIBRARY -lmingw32 CACHE STRING "MinGW library")

        IF(NOT CMAKE_BUILD_TYPE STREQUAL Debug)
            SET(MINGW32_LIBRARY ${MINGW32_LIBRARY} -mwindows)
        ENDIF()
ENDIF(MINGW)

IF(SDL2_LIBRARY_TEMP)
	# For SDL2main
	if(SDL2MAIN_LIBRARY AND NOT WIN32)
	    SET(SDL2_LIBRARY_TEMP ${SDL2MAIN_LIBRARY} ${SDL2_LIBRARY_TEMP})
	endif()

	# For OS X, SDL2 uses Cocoa as a backend so it must link to Cocoa.
	# CMake doesn't display the -framework Cocoa string in the UI even
	# though it actually is there if I modify a pre-used variable.
	# I think it has something to do with the CACHE STRING.
	# So I use a temporary variable until the end so I can set the
	# "real" variable in one-shot.
	IF(APPLE)
		SET(SDL2_LIBRARY_TEMP ${SDL2_LIBRARY_TEMP} "-framework Cocoa")
	ENDIF(APPLE)

	# For threads, as mentioned Apple doesn't need this.
	# In fact, there seems to be a problem if I used the Threads package
	# and try using this line, so I'm just skipping it entirely for OS X.
	IF(NOT APPLE)
		SET(SDL2_LIBRARY_TEMP ${SDL2_LIBRARY_TEMP} ${CMAKE_THREAD_LIBS_INIT})
	ENDIF(NOT APPLE)

	# For MinGW library
	IF(MINGW)
		SET(SDL2_LIBRARY_TEMP ${MINGW32_LIBRARY} ${SDL2_LIBRARY_TEMP} -lversion -limm32)
	ENDIF(MINGW)

        # Add some stuff from pkg-config, if available
        IF(NOT PKG_CONFIG_EXECUTABLE)
            FIND_PACKAGE(PkgConfig QUIET)
        ENDIF(NOT PKG_CONFIG_EXECUTABLE)

        IF(PKG_CONFIG_EXECUTABLE)
            # get any definitions
            EXECUTE_PROCESS(COMMAND ${PKG_CONFIG_EXECUTABLE} --cflags-only-other sdl2 OUTPUT_VARIABLE SDL2_DEFINITIONS ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)

            SET(SDL2_DEFINITIONS ${SDL2_DEFINITIONS} CACHE STRING "Extra CFLAGS for SDL2 from pkg-config")

            # get any extra stuff needed for linking
            IF(NOT SDL2_STATIC)
                EXECUTE_PROCESS(COMMAND ${PKG_CONFIG_EXECUTABLE}          --libs-only-other sdl2 OUTPUT_VARIABLE SDL2_LINKER_FLAGS_RAW    ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)

                EXECUTE_PROCESS(COMMAND ${PKG_CONFIG_EXECUTABLE}          --libs-only-l     sdl2 OUTPUT_VARIABLE SDL2_EXTRA_LIBS_RAW      ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
            ELSE(NOT SDL2_STATIC)
                EXECUTE_PROCESS(COMMAND ${PKG_CONFIG_EXECUTABLE} --static --libs-only-other sdl2 OUTPUT_VARIABLE SDL2_LINKER_FLAGS_RAW    ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
                EXECUTE_PROCESS(COMMAND ${PKG_CONFIG_EXECUTABLE} --static --libs-only-l     sdl2 OUTPUT_VARIABLE SDL2_EXTRA_LIBS_RAW      ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
            ENDIF(NOT SDL2_STATIC)

            STRING(REGEX REPLACE "[^ ]+SDL2[^ ]*" ""  SDL2_EXTRA_LIBS_RAW2 "${SDL2_EXTRA_LIBS_RAW}")
            STRING(REGEX REPLACE " +"             ";" SDL2_EXTRA_LIBS      "${SDL2_EXTRA_LIBS_RAW2}")
            STRING(REGEX REPLACE " +"             ";" SDL2_LINKER_FLAGS    "${SDL2_LINKER_FLAGS_RAW}")

            SET(SDL2_LINKER_FLAGS ${SDL2_LINKER_FLAGS} CACHE STRING "Linker flags for linking SDL2")
            SET(SDL2_EXTRA_LIBS   ${SDL2_EXTRA_LIBS}   CACHE STRING "Extra libraries for linking SDL2")

            SET(SDL2_LIBRARY_TEMP ${SDL2_LIBRARY_TEMP} ${SDL2_EXTRA_LIBS} ${SDL2_LINKER_FLAGS})
        ENDIF(PKG_CONFIG_EXECUTABLE)

	# Set the final string here so the GUI reflects the final state.
	SET(SDL2_LIBRARY ${SDL2_LIBRARY_TEMP} CACHE STRING "Where the SDL2 Library can be found")
	# Set the temp variable to INTERNAL so it is not seen in the CMake GUI
	SET(SDL2_LIBRARY_TEMP "${SDL2_LIBRARY_TEMP}" CACHE INTERNAL "")
ENDIF(SDL2_LIBRARY_TEMP)

INCLUDE(FindPackageHandleStandardArgs)

FIND_PACKAGE_HANDLE_STANDARD_ARGS(SDL2 REQUIRED_VARS SDL2_LIBRARY SDL2_INCLUDE_DIR)
