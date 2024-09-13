cmake_minimum_required(VERSION 3.19)
cmake_policy(VERSION 3.19...3.28.3)

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

if(WIN32)
    include(RemoveStrawberryPerlFromPATH)

    find_program(POWERSHELL
        NAMES powershell.exe pwsh.exe
        HINTS "/Windows/System32/WindowsPowerShell/v1.0"
        REQUIRED)
else()
    find_program(POWERSHELL pwsh)
endif()

if(UPDATE_APPCAST)
    include(UpdateAppcast)
endif()

if(TAG_RELEASE)
    include(MakeReleaseCommitAndTag)
endif()

set(VCPKG_DEPS pkgconf zlib pthreads "sdl2[samplerate]" gettext wxwidgets)

set(VCPKG_DEPS_OPTIONAL
    sfml   ENABLE_LINK
    ffmpeg ENABLE_FFMPEG
    faudio ENABLE_FAUDIO
)

include(Set-Toolchain-vcpkg)

# Use ccache if available and not already enabled on the command line.
# This has to be done before the project() call.
if(NOT CMAKE_CXX_COMPILER_LAUNCHER)
    find_program(CCACHE_EXECUTABLE ccache)
    if(CCACHE_EXECUTABLE)
        message(STATUS "Enabling ccache")

        set(CMAKE_C_COMPILER_LAUNCHER        ${CCACHE_EXECUTABLE} CACHE STRING "C compiler launcher"     FORCE)
        set(CMAKE_CXX_COMPILER_LAUNCHER      ${CCACHE_EXECUTABLE} CACHE STRING "C++ compiler launcher"   FORCE)
        set(CMAKE_ASM_NASM_COMPILER_LAUNCHER ${CCACHE_EXECUTABLE} CACHE STRING "nasm assembler launcher" FORCE)
    endif()
endif()

find_package(Git)

# Make sure we pull in the submodules on windows and mingw.
if(GIT_FOUND AND WIN32)
    # Win32 deps submodule
    set(SUBMODULE_MANUAL_UPDATE FALSE)

    if(EXISTS "${CMAKE_SOURCE_DIR}/.git" AND NOT EXISTS "${CMAKE_SOURCE_DIR}/dependencies/mingw-xaudio/include")
        set(SUBMODULE_MANUAL_UPDATE TRUE)
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" submodule update --init --remote --recursive
            RESULT_VARIABLE SUBMODULE_UPDATE_STATUS
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        )
    endif()

    if(NOT EXISTS "${CMAKE_SOURCE_DIR}/dependencies/mingw-xaudio/include")
        if(NOT (SUBMODULE_MANUAL_UPDATE AND SUBMODULE_UPDATE_STATUS EQUAL 0))
            message(FATAL_ERROR "Please pull in git submodules, e.g.\nrun: git submodule update --init --remote --recursive")
        endif()
    endif()
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED True)

project(VBA-M C CXX)

include(CTest)
include(FetchContent)
include(GNUInstallDirs)
include(Architecture)
include(Options)
include(Toolchain)
include(Dependencies)

# Disable tests when not in a git checkout.
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/.git")
    set(BUILD_TESTING OFF)
endif()

# Configure gtest
if(BUILD_TESTING)
    FetchContent_Declare(googletest
        URL https://github.com/google/googletest/archive/2d16ed055d09c3689d44b272adc097393de948a0.zip
        EXCLUDE_FROM_ALL
    )

    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    include(GoogleTest)
endif()

if(NOT CMAKE_PREFIX_PATH AND (NOT ("$ENV{CMAKE_PREFIX_PATH}" STREQUAL "")))
    set(CMAKE_PREFIX_PATH "$ENV{CMAKE_PREFIX_PATH}")
endif()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build Type" FORCE)
elseif(NOT CMAKE_BUILD_TYPE MATCHES "^(Release|Debug|RelWithDebInfo|MinSizeRel)$")
    message(FATAL_ERROR "Invalid CMAKE_BUILD_TYPE: '${CMAKE_BUILD_TYPE}', must be one of: 'Release', 'Debug', 'RelWithDebInfo' or 'MinSizeRel'")
endif()

# Link debug libs for RelWithDebInfo
if(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    set(CMAKE_MAP_IMPORTED_CONFIG_RELWITHDEBINFO "Debug")
endif()

set(MSYS OFF)
if(NOT "$ENV{MSYSTEM_PREFIX}" STREQUAL "")
    set(MSYS ON)
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/.git")
    include(GitTagVersion)
    git_version(VBAM_VERSION VBAM_REVISION VBAM_VERSION_RELEASE)

    # only use the plugin to tie the configure state to the sha to force rebuilds
    # of files that depend on version.h
    include(GetGitRevisionDescription)
    get_git_head_revision(REFSPEC COMMITHASH)

    # Make sure old tags are gone from all clones.
    execute_process(
        COMMAND ${GIT_EXECUTABLE} tag -l
        OUTPUT_VARIABLE git_tags
        OUTPUT_STRIP_TRAILING_WHITESPACE
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )

    string(REGEX REPLACE ";"     "\\\\;" git_tags_lines "${git_tags}")
    string(REGEX REPLACE "\r?\n" ";"     git_tags_lines "${git_tags_lines}")

    set(found_old_tags FALSE)

    foreach(tag ${git_tags_lines})
        if(NOT tag MATCHES "^v[0-9]")
            set(found_old_tags TRUE)
            break()
        endif()
    endforeach()

    if(found_old_tags)
        # Delete all tags and fetch them from the origin.

        foreach(tag ${git_tags_lines})
            execute_process(
                COMMAND ${GIT_EXECUTABLE} tag -d ${tag}
                OUTPUT_QUIET
                ERROR_QUIET
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            )
        endforeach()

        execute_process(
            COMMAND ${GIT_EXECUTABLE} fetch --tags origin
            OUTPUT_QUIET
            ERROR_QUIET
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
    endif()
endif()

# no git or no tags, use ChangeLog
if(NOT VBAM_VERSION)
    include(ChangeLogVersion)
    changelog_version(VBAM_VERSION VBAM_REVISION VBAM_VERSION_RELEASE)
endif()

# Enable internationalization
set(LOCALEDIR ${CMAKE_INSTALL_PREFIX}/share/locale)

if(NOT TRANSLATIONS_ONLY)
    add_subdirectory(third_party/include/nonstd)
    add_subdirectory(third_party/include/stb)
    add_subdirectory(src/core)
    add_subdirectory(src/components)
    add_subdirectory(src/sdl)
endif()

add_subdirectory(src/wx)
add_subdirectory(po/wxvbam)

set(CPACK_GENERATOR "ZIP")
set(CPACK_SOURCE_GENERATOR "TGZ")
set(CPACK_PACKAGE_VERSION_MAJOR "2")
set(CPACK_PACKAGE_VERSION_MINOR "0")
set(CPACK_PACKAGE_VERSION_PATCH "0-Git-${COMMITHASH}")
list(APPEND CPACK_SOURCE_IGNORE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/dependencies")
include(CPack)
