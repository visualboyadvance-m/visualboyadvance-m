SET(CMAKE_SYSTEM_NAME Windows)

set(COMPILER_PREFIX "i686-w64-mingw32")

# which compilers to use for C and C++
find_program(CMAKE_RC_COMPILER NAMES ${COMPILER_PREFIX}-windres)
find_program(CMAKE_C_COMPILER NAMES ${COMPILER_PREFIX}-gcc)
find_program(CMAKE_CXX_COMPILER NAMES ${COMPILER_PREFIX}-g++)

SET(CMAKE_FIND_ROOT_PATH /usr/${COMPILER_PREFIX} /usr/${COMPILER_PREFIX}/sys-root/mingw)

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
