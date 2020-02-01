SET(CMAKE_SYSTEM_NAME Windows)

set(COMPILER_PREFIX "${CROSS_ARCH}-w64-mingw32")

unset(CMAKE_RC_COMPILER     CACHE)
unset(CMAKE_C_COMPILER      CACHE)
unset(CMAKE_CXX_COMPILER    CACHE)
unset(PKG_CONFIG_EXECUTABLE CACHE)

# which compilers to use for C and C++
find_program(CMAKE_RC_COMPILER NAMES ${COMPILER_PREFIX}-windres)
find_program(CMAKE_C_COMPILER NAMES ${COMPILER_PREFIX}-gcc)
find_program(CMAKE_CXX_COMPILER NAMES ${COMPILER_PREFIX}-g++)

SET(CMAKE_FIND_ROOT_PATH
    /usr/${COMPILER_PREFIX}
    /usr/${COMPILER_PREFIX}/usr
    /usr/${COMPILER_PREFIX}/sys-root/mingw
    /usr/local/opt/mingw-w64/toolchain-${CROSS_ARCH}/${COMPILER_PREFIX}
    /home/linuxbrew/.linuxbrew/opt/mingw-w64/toolchain-${CROSS_ARCH}/${COMPILER_PREFIX}
    $ENV{HOME}/.linuxbrew/opt/mingw-w64/toolchain-${CROSS_ARCH}/${COMPILER_PREFIX}
)

# find wx-config
foreach(p ${CMAKE_FIND_ROOT_PATH})
    file(GLOB paths ${p}/lib/wx/config/${COMPILER_PREFIX}-msw-*)

    list(APPEND wx_confs ${paths})
endforeach()

foreach(p ${wx_confs})
    if(CMAKE_TOOLCHAIN_FILE MATCHES -static)
        string(REGEX MATCH "(static-)?([0-9]+\\.?)+$" wx_conf_ver ${p})
    else()
        string(REGEX MATCH "([0-9]+\\.?)+$" wx_conf_ver ${p})
    endif()

    list(APPEND wx_conf_vers ${wx_conf_ver})
endforeach()

list(SORT    wx_conf_vers)
list(REVERSE wx_conf_vers)
list(GET     wx_conf_vers 0 wx_conf_ver)

foreach(p ${wx_confs})
    if(p MATCHES "${wx_conf_ver}$")
        set(wx_conf ${p})
        break()
    endif()
endforeach()

set(wxWidgets_CONFIG_EXECUTABLE ${wx_conf} CACHE FILEPATH "path to wx-config script for the desired wxWidgets configuration" FORCE)

# find the right static zlib
if(CMAKE_TOOLCHAIN_FILE MATCHES -static)
    foreach(p ${CMAKE_FIND_ROOT_PATH})
        if(EXISTS ${p}/lib/libz.a)
            set(ZLIB_ROOT ${p} CACHE FILEPATH "where to find zlib" FORCE)
            break()
        endif()
    endforeach()
endif()

if(CMAKE_PREFIX_PATH)
    set(CMAKE_FIND_ROOT_PATH ${CMAKE_FIND_ROOT_PATH} ${CMAKE_PREFIX_PATH})
endif()

# adjust the default behaviour of the FIND_XXX() commands:
# search headers and libraries in the target environment, search
# programs in the host environment too
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# pkg-config may be under the prefix
find_program(PKG_CONFIG_EXECUTABLE NAMES ${COMPILER_PREFIX}-pkg-config)
