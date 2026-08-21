cmake_minimum_required(VERSION 3.19)
cmake_policy(VERSION 3.19...3.28.3)

# Use new link library de-duplication behavior.
if(POLICY CMP0156)
    cmake_policy(SET CMP0156 NEW)
endif()
if(POLICY CMP0179)
    cmake_policy(SET CMP0179 NEW)
endif()
#if(POLICY CMP0181)
#    cmake_policy(SET CMP0181 NEW)
#endif()

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

if(WIN32)
    include(RemoveStrawberryPerlFromPATH)

    find_program(POWERSHELL
        NAMES pwsh.exe powershell.exe
        HINTS
            "/Program Files/PowerShell/7"
            "/Windows/System32/WindowsPowerShell/v1.0"
        REQUIRED)
else()
    find_program(POWERSHELL pwsh)

    # PowerShell drives the vcpkg binary package tooling, and without it
    # Set-Toolchain-vcpkg.cmake stops with "Powershell is required to use
    # vcpkg binaries". Few Linux or macOS systems have it, and some have no
    # package for it at all, so fetch the upstream portable build into the
    # build tree rather than failing.
    #
    # Only when vcpkg is actually in use: Set-Toolchain-vcpkg.cmake returns
    # early off Windows unless VCPKG_TARGET_TRIPLET is set, and every other
    # user of POWERSHELL here is Windows-only, so an ordinary build is not
    # made to pull down an archive it would never run.
    #
    # Upstream publishes .tar.gz for these platforms; the .zip archives are
    # Windows-only.
    if(DEFINED VCPKG_TARGET_TRIPLET
       AND (CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux"
            OR CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin"))
        if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
            set(vbam_pwsh_os osx)
        else()
            set(vbam_pwsh_os linux)
        endif()

        if(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$")
            set(vbam_pwsh_arch x64)
        elseif(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
            set(vbam_pwsh_arch arm64)
        endif()

        set(vbam_pwsh_dir "${CMAKE_BINARY_DIR}/pwsh")
        set(vbam_pwsh_exe "${vbam_pwsh_dir}/pwsh")

        if(NOT POWERSHELL AND NOT EXISTS "${vbam_pwsh_exe}")
            if(NOT vbam_pwsh_arch)
                message(WARNING
                    "No PowerShell build for ${CMAKE_HOST_SYSTEM_PROCESSOR}, "
                    "install pwsh to use vcpkg binary packages.")
            else()
                # Resolve the current release. Upstream's own machine-readable
                # endpoint, rather than the GitHub API, which is rate limited
                # per IP and would be unreliable from CI.
                set(vbam_pwsh_buildinfo "${CMAKE_BINARY_DIR}/pwsh-buildinfo.json")

                file(DOWNLOAD
                    "https://aka.ms/pwsh-buildinfo-stable"
                    "${vbam_pwsh_buildinfo}"
                    STATUS vbam_pwsh_status
                )
                list(GET vbam_pwsh_status 0 vbam_pwsh_rc)

                if(NOT vbam_pwsh_rc EQUAL 0)
                    list(GET vbam_pwsh_status 1 vbam_pwsh_error)
                    file(REMOVE "${vbam_pwsh_buildinfo}")
                    message(FATAL_ERROR
                        "Failed to look up the current PowerShell release: ${vbam_pwsh_error}")
                endif()

                file(READ "${vbam_pwsh_buildinfo}" vbam_pwsh_buildinfo_json)
                file(REMOVE "${vbam_pwsh_buildinfo}")

                string(JSON vbam_pwsh_tag
                       ERROR_VARIABLE vbam_pwsh_json_error
                       GET "${vbam_pwsh_buildinfo_json}" ReleaseTag)

                if(vbam_pwsh_json_error OR NOT vbam_pwsh_tag)
                    message(FATAL_ERROR
                        "Could not read ReleaseTag from the PowerShell build info: ${vbam_pwsh_json_error}")
                endif()

                string(REGEX REPLACE "^v" "" vbam_pwsh_version "${vbam_pwsh_tag}")

                set(vbam_pwsh_archive
                    "powershell-${vbam_pwsh_version}-${vbam_pwsh_os}-${vbam_pwsh_arch}.tar.gz")
                set(vbam_pwsh_tarball "${CMAKE_BINARY_DIR}/${vbam_pwsh_archive}")

                message(STATUS "Downloading ${vbam_pwsh_archive}")

                file(DOWNLOAD
                    "https://github.com/PowerShell/PowerShell/releases/download/${vbam_pwsh_tag}/${vbam_pwsh_archive}"
                    "${vbam_pwsh_tarball}"
                    STATUS vbam_pwsh_status
                )
                list(GET vbam_pwsh_status 0 vbam_pwsh_rc)

                if(NOT vbam_pwsh_rc EQUAL 0)
                    list(GET vbam_pwsh_status 1 vbam_pwsh_error)
                    file(REMOVE "${vbam_pwsh_tarball}")
                    message(FATAL_ERROR
                        "Failed to download PowerShell: ${vbam_pwsh_error}")
                endif()

                file(MAKE_DIRECTORY "${vbam_pwsh_dir}")
                file(ARCHIVE_EXTRACT
                    INPUT "${vbam_pwsh_tarball}"
                    DESTINATION "${vbam_pwsh_dir}")
                file(REMOVE "${vbam_pwsh_tarball}")

                # The archive does not carry a mode bit the extractor keeps.
                file(CHMOD "${vbam_pwsh_exe}"
                     PERMISSIONS
                     OWNER_READ OWNER_WRITE OWNER_EXECUTE
                     GROUP_READ GROUP_EXECUTE
                     WORLD_READ WORLD_EXECUTE)
            endif()
        endif()

        # Also on reconfigure, when the download above is skipped: $ENV{PATH}
        # does not persist across runs.
        if(EXISTS "${vbam_pwsh_exe}")
            set(POWERSHELL "${vbam_pwsh_exe}"
                CACHE FILEPATH "PowerShell executable" FORCE)
            set(ENV{PATH} "${vbam_pwsh_dir}:$ENV{PATH}")
        endif()
    endif()
endif()

if(UPDATE_APPCAST)
    include(UpdateAppcast)
endif()

if(TAG_RELEASE)
    include(MakeReleaseCommitAndTag)
endif()

set(MSYS2 OFF CACHE BOOL "Building in an MSYS2 environment" FORCE)
if(NOT "$ENV{MSYSTEM_PREFIX}" STREQUAL "")
    set(MSYS2 ON CACHE BOOL "Building in an MSYS2 environment" FORCE)
    # Clear env vars from Windows native dev environments that can break the build.
    unset(ENV{LIB})
    unset(ENV{INCLUDE})
endif()

option(VCPKG_BINARY_PACKAGES "Use vcpkg binary packages" TRUE)

# Which frontends are being built decides which dependencies are worth
# installing and probing for. This has to be known before the vcpkg lists
# below, hence a separate module included ahead of project()/Options.cmake.
include(FrontendOptions)

# Core dependencies. vbam-core and its file extractors are built for every
# frontend selection.
set(VCPKG_DEPS zlib bzip2 "liblzma[tools]" pthreads)

# The wxWidgets frontend. Qt is the wx backend on Android (wxQt), and nanosvg
# is what the vcpkg wxWidgets port wants for SVG art.
if(ENABLE_WX)
    list(APPEND VCPKG_DEPS wxwidgets nanosvg lua)

    if(VBAM_TARGET_ANDROID)
        list(APPEND VCPKG_DEPS qtbase qttools)
    endif()

    # The Vulkan renderer. On Apple platforms it goes through MoltenVK, which
    # is not a vcpkg port here.
    if(NOT VBAM_TARGET_APPLE)
        list(APPEND VCPKG_DEPS vulkan)
    endif()
endif()

if(VBAM_NEED_NLS)
    list(APPEND VCPKG_DEPS gettext-libintl)
endif()

unset(VCPKG_DEPS_OPTIONAL)

# SDL is the SDL port's toolkit and the wx port's audio/game controller
# backend, so either frontend pulls it in.
if(VBAM_NEED_SDL)
    list(APPEND VCPKG_DEPS_OPTIONAL "sdl3[vulkan]" ENABLE_SDL3)
endif()

# Sound backends and A/V recording are wxWidgets-only features.
if(ENABLE_WX)
    list(APPEND VCPKG_DEPS_OPTIONAL
        openal-soft         ENABLE_OPENAL
        "ffmpeg[x264,x265]" ENABLE_FFMPEG
        faudio              ENABLE_FAUDIO
    )
endif()

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

    if(EXISTS "${CMAKE_SOURCE_DIR}/.git" AND NOT EXISTS "${CMAKE_SOURCE_DIR}/win32-deps/mingw-xaudio/include")
        set(SUBMODULE_MANUAL_UPDATE TRUE)
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" submodule update --init --recursive -- win32-deps
            RESULT_VARIABLE SUBMODULE_UPDATE_STATUS
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        )
    endif()

    if(NOT EXISTS "${CMAKE_SOURCE_DIR}/win32-deps/mingw-xaudio/include")
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

if(APPLE)
    enable_language(OBJC)
    enable_language(OBJCXX)
    # Suppress "dylib was built for newer macOS version than being linked" and
    # other ld warnings that fire when linking against Homebrew libraries.
    # Apple's linker has no per-warning suppression for these diagnostics.
    add_link_options(-Wl,-w)
endif()

# VCPKG_TOOLCHAIN is set by vcpkg.cmake itself. Testing CMAKE_TOOLCHAIN_FILE
# would miss the case where vcpkg is loaded alongside another toolchain file
# (see VBAM_OUTER_TOOLCHAIN_FILE in Set-Toolchain-vcpkg.cmake).
if(VCPKG_TOOLCHAIN)
    add_compile_definitions(-DVCPKG)
    if(MSVC)
        set(CMAKE_C_FLAGS      "/I\"${VCPKG_ROOT}/installed/${VCPKG_TARGET_TRIPLET}/include\" ${CMAKE_C_FLAGS}")
        set(CMAKE_CXX_FLAGS    "/I\"${VCPKG_ROOT}/installed/${VCPKG_TARGET_TRIPLET}/include\" ${CMAKE_CXX_FLAGS}")
        set(CMAKE_OBJC_FLAGS   "/I\"${VCPKG_ROOT}/installed/${VCPKG_TARGET_TRIPLET}/include\" ${CMAKE_OBJC_FLAGS}")
        set(CMAKE_OBJCXX_FLAGS "/I\"${VCPKG_ROOT}/installed/${VCPKG_TARGET_TRIPLET}/include\" ${CMAKE_OBJCXX_FLAGS}")
    else()
        set(CMAKE_C_FLAGS      "-I\"${VCPKG_ROOT}/installed/${VCPKG_TARGET_TRIPLET}/include\" ${CMAKE_C_FLAGS}")
        set(CMAKE_CXX_FLAGS    "-I\"${VCPKG_ROOT}/installed/${VCPKG_TARGET_TRIPLET}/include\" ${CMAKE_CXX_FLAGS}")
        set(CMAKE_OBJC_FLAGS   "-I\"${VCPKG_ROOT}/installed/${VCPKG_TARGET_TRIPLET}/include\" ${CMAKE_OBJC_FLAGS}")
        set(CMAKE_OBJCXX_FLAGS "-I\"${VCPKG_ROOT}/installed/${VCPKG_TARGET_TRIPLET}/include\" ${CMAKE_OBJCXX_FLAGS}")
    endif()
endif()

# When VS environment was auto-loaded by Set-Toolchain-vcpkg.cmake, embed the
# MSVC standard include and lib paths as explicit flags. ninja runs in a separate
# process that does not inherit cmake's environment, so INCLUDE/LIB would be unset.
if(MSVC AND DEFINED VBAM_VS_INCLUDE_DIRS)
    foreach(inc_dir ${VBAM_VS_INCLUDE_DIRS})
        string(APPEND VBAM_MSVC_INC_FLAGS " /I\"${inc_dir}\"")
    endforeach()
    set(CMAKE_C_FLAGS   "${VBAM_MSVC_INC_FLAGS} ${CMAKE_C_FLAGS}")
    set(CMAKE_CXX_FLAGS "${VBAM_MSVC_INC_FLAGS} ${CMAKE_CXX_FLAGS}")
    # RC compilation uses cmcldeps which runs cl.exe for dependency scanning;
    # it needs the same include paths.
    set(CMAKE_RC_FLAGS  "${VBAM_MSVC_INC_FLAGS} ${CMAKE_RC_FLAGS}")
    unset(VBAM_MSVC_INC_FLAGS)
endif()

if(MSVC AND DEFINED VBAM_VS_LIB_DIRS)
    foreach(lib_dir ${VBAM_VS_LIB_DIRS})
        string(APPEND VBAM_MSVC_LIB_FLAGS " /LIBPATH:\"${lib_dir}\"")
    endforeach()
    set(CMAKE_EXE_LINKER_FLAGS    "${VBAM_MSVC_LIB_FLAGS} ${CMAKE_EXE_LINKER_FLAGS}")
    set(CMAKE_SHARED_LINKER_FLAGS "${VBAM_MSVC_LIB_FLAGS} ${CMAKE_SHARED_LINKER_FLAGS}")
    set(CMAKE_MODULE_LINKER_FLAGS "${VBAM_MSVC_LIB_FLAGS} ${CMAKE_MODULE_LINKER_FLAGS}")
    unset(VBAM_MSVC_LIB_FLAGS)
endif()

# With a static vcpkg triplet there are no DLL dependencies to stage next to the
# executables -- everything is linked statically, and the Vulkan loader is loaded
# dynamically (delay-loaded), not as a normal import. So vcpkg's app-local
# deployment has nothing to copy; turn it off to skip its per-target post-build
# step and the "deploying dependencies" message it prints. Dynamic triplets still
# need it, so this is gated on a static triplet.
if(VCPKG_TARGET_TRIPLET MATCHES "static")
    set(VCPKG_APPLOCAL_DEPS OFF)
endif()

# When VS environment was auto-loaded, generate wrappers that prepend VS tool
# paths (containing dumpbin, etc.) to PATH at build time.  vcpkg's applocal.ps1
# post-build script needs dumpbin, but neither ninja nor the powershell process
# it spawns inherit cmake's configure-time environment.
if(MSVC AND DEFINED VBAM_VS_TOOL_PATHS)
    # VBAM_VS_TOOL_PATHS is stored as a CMake list (semicolons become separators);
    # string(JOIN) reconstitutes the semicolon-separated Windows PATH string.
    string(JOIN ";" VBAM_VS_TOOL_PATHS_STR ${VBAM_VS_TOOL_PATHS})

    # Wrap the build tool (ninja) so cmake --build gets VS tools in PATH.
    if(NOT CMAKE_GENERATOR MATCHES "Visual Studio")
        if(NOT DEFINED VBAM_REAL_NINJA)
            set(VBAM_REAL_NINJA "${CMAKE_MAKE_PROGRAM}" CACHE FILEPATH "" FORCE)
        endif()
        file(TO_NATIVE_PATH "${VBAM_REAL_NINJA}" VBAM_NINJA_NATIVE)
        set(VBAM_BUILD_WRAPPER "${CMAKE_BINARY_DIR}/vbam_build.cmd")
        file(WRITE "${VBAM_BUILD_WRAPPER}"
            "@echo off\r\nset \"PATH=${VBAM_VS_TOOL_PATHS_STR};%PATH%\"\r\n\"${VBAM_NINJA_NATIVE}\" %*\r\n"
        )
        set(CMAKE_MAKE_PROGRAM "${VBAM_BUILD_WRAPPER}" CACHE FILEPATH "" FORCE)
        unset(VBAM_NINJA_NATIVE)
    endif()

    # Override vcpkg's powershell path with a wrapper that prepends VS tool paths.
    # vcpkg's z_vcpkg_set_powershell_path() skips setup when Z_VCPKG_POWERSHELL_PATH
    # is already defined, so pre-defining it here makes every applocal.ps1 invocation
    # run with dumpbin available — even when ninja is called directly.
    file(TO_NATIVE_PATH "${POWERSHELL}" VBAM_PS_NATIVE)
    set(VBAM_PS_WRAPPER "${CMAKE_BINARY_DIR}/vbam_powershell.cmd")
    file(WRITE "${VBAM_PS_WRAPPER}"
        "@echo off\r\nset \"PATH=${VBAM_VS_TOOL_PATHS_STR};%PATH%\"\r\n\"${VBAM_PS_NATIVE}\" %*\r\n"
    )
    set(Z_VCPKG_POWERSHELL_PATH "${VBAM_PS_WRAPPER}" CACHE INTERNAL "" FORCE)

    unset(VBAM_VS_TOOL_PATHS_STR)
    unset(VBAM_PS_NATIVE)
endif()

if(APPLE)
    include(CheckLanguage)
    include(MetalShaderSupport)

    # Locate the Metal compiler via the system xcrun with DEVELOPER_DIR unset.
    # The Nix build ships its own xcrun reimplementation (no metal tool) and
    # points DEVELOPER_DIR at the Nix SDK; using the host /usr/bin/xcrun finds
    # the real Xcode Metal toolchain, which is self-contained for the air64
    # target. The compiler is only used to build the shader metallib.
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E env --unset=DEVELOPER_DIR
                /usr/bin/xcrun -f metal
        OUTPUT_STRIP_TRAILING_WHITESPACE
        OUTPUT_VARIABLE CMAKE_Metal_COMPILER
        ERROR_QUIET
    )

    if(NOT CMAKE_Metal_COMPILER)
        check_language(Metal)
    endif()

    if(CMAKE_Metal_COMPILER)
        enable_language(Metal)
    endif()
endif()

include(CTest)
include(FetchContent)
include(GNUInstallDirs)
include(Architecture)
include(Options)
include(Toolchain)
include(Dependencies)

# We target Windows XP for 32 bit Windows builds. Define WINXP here too so the
# compile macro always tracks the _WIN32_WINNT=0x0501 target; the WINXP CMake
# variable (set in Architecture.cmake) governs build-time logic separately.
if(WIN32 AND X86)
    add_compile_definitions(-DWINVER=0x0501 -D_WIN32_WINNT=0x0501 -DWINXP)
endif()

# Disable tests when not in a git checkout.
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/.git")
    set(BUILD_TESTING OFF)
endif()

# Configure gtest
if(BUILD_TESTING)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)

    if(NOT EXISTS ${CMAKE_SOURCE_DIR}/third_party/googletest/CMakeLists.txt)
        execute_process(
            COMMAND git submodule update --init --recursive -- third_party/googletest
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
    endif()

    if(EXISTS ${CMAKE_SOURCE_DIR}/third_party/googletest/CMakeLists.txt)
        add_subdirectory(./third_party/googletest)

        # Make sure not use fast-math optimizations for gtest
        if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
            target_compile_options(gtest      PRIVATE -O3 -fno-fast-math -Wno-error -w -DGTEST_API_=/**/)
            target_compile_options(gtest_main PRIVATE -O3 -fno-fast-math -Wno-error -w -DGTEST_API_=/**/)
            target_compile_options(gmock      PRIVATE -O3 -fno-fast-math -Wno-error -w -DGTEST_API_=/**/)
            target_compile_options(gmock_main PRIVATE -O3 -fno-fast-math -Wno-error -w -DGTEST_API_=/**/)

            add_compile_definitions(-DGTEST_API_=/**/)
        endif()

        include(GoogleTest)
        include(GTestDiscoverTests)
    else()
        set(BUILD_TESTING OFF)
    endif()
endif()

if(NOT CMAKE_PREFIX_PATH AND (NOT ("$ENV{CMAKE_PREFIX_PATH}" STREQUAL "")))
    set(CMAKE_PREFIX_PATH "$ENV{CMAKE_PREFIX_PATH}")
endif()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build Type" FORCE)
elseif(NOT CMAKE_BUILD_TYPE MATCHES "^(Release|Debug|RelWithDebInfo|MinSizeRel)$")
    message(FATAL_ERROR "Invalid CMAKE_BUILD_TYPE: '${CMAKE_BUILD_TYPE}', must be one of: 'Release', 'Debug', 'RelWithDebInfo' or 'MinSizeRel'")
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
set(LOCALEDIR share/locale)

# Set up the zip compressor program (used for release zips and translations).
if(NOT ZIP_PROGRAM)
    find_program(ZIP_PROGRAM zip DOC "zip compressor executable")
    if(WIN32 AND NOT ZIP_PROGRAM)
        if(NOT DEFINED POWERSHELL)
            message(FATAL_ERROR "Powershell is required for zip file support when the `zip` program is not available.")
        endif()

        # Build zip.exe from zip.ps1 using ps12exe
        set(ZIP_EXE_DIR "${CMAKE_BINARY_DIR}/zip-bin")
        set(ZIP_EXE "${ZIP_EXE_DIR}/zip.exe")
        set(PS12EXE_DIR "${CMAKE_SOURCE_DIR}/third_party/ps12exe")

        if(NOT EXISTS "${ZIP_EXE}")
            file(MAKE_DIRECTORY "${ZIP_EXE_DIR}")

            execute_process(
                COMMAND ${POWERSHELL} -NoProfile -ExecutionPolicy Bypass -Command
                    "& '${PS12EXE_DIR}/ps12exe.ps1' -inputFile '${CMAKE_SOURCE_DIR}/src/wx/zip.ps1' -outputFile '${ZIP_EXE}' -noConsole:$false -architecture 'x86' -localize 'en' -skipVersionCheck:$true"
                RESULT_VARIABLE PS12EXE_BUILD_RESULT
            )

            if(NOT PS12EXE_BUILD_RESULT EQUAL 0)
                message(FATAL_ERROR "Failed to build zip.exe with ps12exe. The zip program is required for building.")
            endif()
        endif()

        set(ZIP_PROGRAM "${ZIP_EXE}")
    endif()
    if(ZIP_PROGRAM)
        set(ZIP_PROGRAM "${ZIP_PROGRAM}" CACHE STRING "zip compressor executable" FORCE)
    else()
        message(FATAL_ERROR "The zip compressor program is required for building.")
    endif()
endif()

# When ZIP_PROGRAM is cached, ENV{PATH} must be set every time.
if(ZIP_PROGRAM)
    get_filename_component(ZIP_PROGRAM_DIR ${ZIP_PROGRAM} DIRECTORY)
    # ENV{PATH} runs host tools, so it is the host that decides the separator.
    # WIN32 is the target, and is true for a MinGW cross build from Unix.
    if(CMAKE_HOST_WIN32)
        set(ENV{PATH} "${ZIP_PROGRAM_DIR};$ENV{PATH}")
    else()
        set(ENV{PATH} "${ZIP_PROGRAM_DIR}:$ENV{PATH}")
    endif()
endif()

if(NOT TRANSLATIONS_ONLY)
    add_subdirectory(third_party/include/nonstd)
    add_subdirectory(third_party/include/stb)
    add_subdirectory(third_party/include/ghc)

    if(ENABLE_LINK)
        include_directories(third_party/sfml/include)
        add_subdirectory(third_party/sfml/src/SFML/System  EXCLUDE_FROM_ALL)
        add_subdirectory(third_party/sfml/src/SFML/Network EXCLUDE_FROM_ALL)
        set(SFML_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/third_party/sfml/include)
        set(SFML_LIBRARIES sfml-system sfml-network)
    endif()

    add_subdirectory(src/core)
    add_subdirectory(src/components)
    # The SDL and libretro frontends are desktop/RetroArch targets; the Android
    # deliverable is the wxQt app, so skip them there.
    if(NOT ANDROID)
        add_subdirectory(src/sdl)
        add_subdirectory(src/libretro)
    endif()
endif()

add_subdirectory(src/wx)
add_subdirectory(po/wxvbam)

set(CPACK_GENERATOR "ZIP")
set(CPACK_SOURCE_GENERATOR "TGZ")
set(CPACK_PACKAGE_VERSION_MAJOR "2")
set(CPACK_PACKAGE_VERSION_MINOR "0")
set(CPACK_PACKAGE_VERSION_PATCH "0-Git-${COMMITHASH}")
list(APPEND CPACK_SOURCE_IGNORE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/win32-deps")
include(CPack)
