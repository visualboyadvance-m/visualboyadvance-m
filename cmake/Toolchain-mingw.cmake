# this has to run after the toolchain is initialized so it can't be in
# Win32deps.cmake
if(MINGW)
    include_directories("${CMAKE_SOURCE_DIR}/dependencies/mingw-include")
    include_directories("${CMAKE_SOURCE_DIR}/dependencies/mingw-xaudio/include")
endif()

if(MINGW)
    # Win32 deps submodule
    set(git_checkout FALSE)
    if(EXISTS "${CMAKE_SOURCE_DIR}/.git")
        set(git_checkout TRUE)
        execute_process(
            COMMAND ${GIT_EXECUTABLE} submodule update --init --remote --recursive
            RESULT_VARIABLE git_status
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        )
    endif()

    if(NOT EXISTS "${CMAKE_SOURCE_DIR}/dependencies/mingw-xaudio/include")
        if(NOT (git_checkout AND git_status EQUAL 0))
            message(FATAL_ERROR "Please pull in git submodules, e.g.\nrun: git submodule update --init --remote --recursive")
        endif()
    endif()
endif()

# hack for ninja in msys2
if(WIN32 AND CMAKE_GENERATOR STREQUAL Ninja AND NOT "$ENV{MSYSTEM_PREFIX}" STREQUAL "")
    set(MSYS ON)
endif()

if(MSYS AND CMAKE_CXX_COMPILER_ID STREQUAL Clang)
    if($ENV{MSYSTEM} STREQUAL CLANG64)
        cygpath(prefix "$ENV{MSYSTEM_PREFIX}/x86_64-w64-mingw32")
        list(APPEND CMAKE_PREFIX_PATH "${prefix}")
    elseif($ENV{MSYSTEM} STREQUAL CLANG32)
        cygpath(prefix "$ENV{MSYSTEM_PREFIX}/i686-w64-mingw32")
        list(APPEND CMAKE_PREFIX_PATH "${prefix}")
    endif()

    set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH}" CACHE INTERNAL "prefix search path for find_XXXX" FORCE)
endif()

# link libgcc/libstdc++ statically on mingw
# and adjust link command when making a static binary
if(CMAKE_COMPILER_IS_GNUCXX AND VBAM_STATIC)
    # some dists don't have a static libpthread
    set(CMAKE_CXX_LINK_EXECUTABLE "${CMAKE_CXX_LINK_EXECUTABLE} -static-libgcc -static-libstdc++ -Wl,-Bstatic -lstdc++ -lpthread ")

    if(WIN32)
        add_custom_command(
            TARGET visualboyadvance-m PRE_LINK
            COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_SOURCE_DIR}/msys-link-static.cmake
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        )
    else()
        add_custom_command(
            TARGET visualboyadvance-m PRE_LINK
            COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_SOURCE_DIR}/link-static.cmake
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        )
    endif()
endif()
