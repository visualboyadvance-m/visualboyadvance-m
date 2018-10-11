SET(CMAKE_SYSTEM_NAME Windows)

set(CROSS_ARCH x86_64)
set(COMPILER_PREFIX "${CROSS_ARCH}-w64-mingw32")

unset(CMAKE_RC_COMPILER     CACHE)
unset(CMAKE_C_COMPILER      CACHE)
unset(CMAKE_CXX_COMPILER    CACHE)
unset(PKG_CONFIG_EXECUTABLE CACHE)

# which compilers to use for C and C++
find_program(CMAKE_RC_COMPILER NAMES ${COMPILER_PREFIX}-windres)
find_program(CMAKE_C_COMPILER NAMES ${COMPILER_PREFIX}-gcc)
find_program(CMAKE_CXX_COMPILER NAMES ${COMPILER_PREFIX}-g++)

SET(CMAKE_FIND_ROOT_PATH /usr/${COMPILER_PREFIX} /usr/${COMPILER_PREFIX}/usr /usr/${COMPILER_PREFIX}/sys-root/mingw /usr/local/opt/mingw-w64/toolchain-${CROSS_ARCH}/${COMPILER_PREFIX})

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
