# The frontend selection drives which ports end up in VCPKG_DEPS and
# VCPKG_DEPS_OPTIONAL. The top-level CMakeLists.txt includes this module and
# builds those lists from the same options, but include it here as well so this
# file does not silently depend on that ordering.
include(FrontendOptions)

if(TRANSLATIONS_ONLY)
    return()
endif()

# On Windows, if cl.exe is not already in the PATH but Visual Studio is installed,
# automatically load the VS build environment for the host architecture.
# Skip this when running inside an MSYS2 shell (MinGW/UCRT64/etc.).
if(WIN32 AND "$ENV{MSYSTEM}" STREQUAL "")
    find_program(VBAM_CL_EXE_CHECK NAME cl.exe HINTS ENV PATH)

    if(NOT VBAM_CL_EXE_CHECK)
        # Locate vswhere.exe, which ships with VS 2017+ installer.
        # $ENV{ProgramFiles(x86)} is not valid CMake syntax; use cmd to expand it.
        execute_process(
            COMMAND cmd /c "echo %ProgramFiles(x86)%"
            OUTPUT_VARIABLE VBAM_PROG_FILES_X86
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )

        find_program(VBAM_VSWHERE
            NAME vswhere.exe
            HINTS "${VBAM_PROG_FILES_X86}/Microsoft Visual Studio/Installer"
            NO_DEFAULT_PATH
        )

        unset(VBAM_PROG_FILES_X86)

        unset(VBAM_VS_INSTALL_PATH)

        if(VBAM_VSWHERE)
            execute_process(
                COMMAND "${VBAM_VSWHERE}" -latest -property installationPath
                OUTPUT_VARIABLE VBAM_VS_INSTALL_PATH
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET
            )
        endif()

        unset(VBAM_VSWHERE CACHE)

        # Fallback: check registry for VS 2015.
        if(NOT VBAM_VS_INSTALL_PATH)
            get_filename_component(VBAM_VS2015_INSTALLDIR
                "[HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Microsoft\\VisualStudio\\14.0;InstallDir]"
                ABSOLUTE CACHE
            )

            if(VBAM_VS2015_INSTALLDIR AND EXISTS "${VBAM_VS2015_INSTALLDIR}")
                # InstallDir is the IDE directory; go up three levels to reach the install root.
                get_filename_component(VBAM_VS_INSTALL_PATH "${VBAM_VS2015_INSTALLDIR}/../../.." ABSOLUTE)
            endif()

            unset(VBAM_VS2015_INSTALLDIR CACHE)
        endif()

        if(VBAM_VS_INSTALL_PATH)
            # VS 2017+: vcvarsall.bat lives under VC/Auxiliary/Build/.
            set(VBAM_VCVARSALL "${VBAM_VS_INSTALL_PATH}/VC/Auxiliary/Build/vcvarsall.bat")

            # VS 2015 and earlier: vcvarsall.bat lives directly under VC/.
            if(NOT EXISTS "${VBAM_VCVARSALL}")
                set(VBAM_VCVARSALL "${VBAM_VS_INSTALL_PATH}/VC/vcvarsall.bat")
            endif()

            if(EXISTS "${VBAM_VCVARSALL}")
                # Select the native host architecture for the VS toolchain.
                if("$ENV{PROCESSOR_ARCHITECTURE}" STREQUAL "AMD64"
                        OR DEFINED ENV{PROCESSOR_ARCHITEW6432})
                    set(VBAM_VS_ARCH x64)
                elseif("$ENV{PROCESSOR_ARCHITECTURE}" STREQUAL "ARM64")
                    set(VBAM_VS_ARCH arm64)
                else()
                    set(VBAM_VS_ARCH x86)
                endif()

                message(STATUS "Loading Visual Studio ${VBAM_VS_ARCH} environment from: ${VBAM_VCVARSALL}")

                # Write a temporary batch file so cmd quoting is unambiguous.
                set(VBAM_VS_ENV_SCRIPT "${CMAKE_BINARY_DIR}/vbam_vs_env.bat")
                file(WRITE "${VBAM_VS_ENV_SCRIPT}"
                    "@echo off\r\ncall \"${VBAM_VCVARSALL}\" ${VBAM_VS_ARCH}\r\nset\r\n"
                )

                execute_process(
                    COMMAND cmd /c "${VBAM_VS_ENV_SCRIPT}"
                    OUTPUT_VARIABLE VBAM_VS_ENV
                    OUTPUT_STRIP_TRAILING_WHITESPACE
                    RESULT_VARIABLE VBAM_VS_ENV_RESULT
                    ERROR_QUIET
                )

                file(REMOVE "${VBAM_VS_ENV_SCRIPT}")
                unset(VBAM_VS_ENV_SCRIPT)

                if(VBAM_VS_ENV_RESULT EQUAL 0 AND VBAM_VS_ENV)
                    # Save the original PATH so tools like git are not lost when
                    # vcvarsall.bat replaces it with VS-only directories.
                    set(VBAM_ORIGINAL_PATH "$ENV{PATH}")

                    # Write to a temp file and use file(STRINGS) to parse line-by-line.
                    # This avoids the CMake list-separator bug: PATH, LIB, INCLUDE, etc.
                    # all contain semicolons in their values, which string(REPLACE) would
                    # turn into CMake list separators and shred the values.
                    set(VBAM_VS_ENV_FILE "${CMAKE_BINARY_DIR}/vbam_vs_env_output.txt")
                    file(WRITE "${VBAM_VS_ENV_FILE}" "${VBAM_VS_ENV}")
                    unset(VBAM_VS_ENV)

                    file(STRINGS "${VBAM_VS_ENV_FILE}" VBAM_VS_ENV_LINES)
                    file(REMOVE "${VBAM_VS_ENV_FILE}")
                    unset(VBAM_VS_ENV_FILE)

                    foreach(line IN LISTS VBAM_VS_ENV_LINES)
                        if(line MATCHES "^([^=]+)=(.*)")
                            set(ENV{${CMAKE_MATCH_1}} "${CMAKE_MATCH_2}")
                        endif()
                    endforeach()

                    unset(VBAM_VS_ENV_LINES)

                    # Save VS-only tool paths (before restoring original PATH) so
                    # the build-time wrapper can prepend them to PATH for tools
                    # like dumpbin that vcpkg post-build scripts depend on.
                    set(VBAM_VS_TOOL_PATHS "$ENV{PATH}" CACHE STRING "" FORCE)

                    # Append the original PATH so pre-existing tools remain reachable.
                    set(ENV{PATH} "$ENV{PATH};${VBAM_ORIGINAL_PATH}")
                    unset(VBAM_ORIGINAL_PATH)

                    message(STATUS "Visual Studio environment loaded successfully.")

                    # Cache INCLUDE and LIB so CMakeLists.txt can embed them as
                    # explicit compile/link flags. The build step (ninja) runs in a
                    # separate process that does not inherit cmake's environment.
                    set(VBAM_VS_INCLUDE_DIRS $ENV{INCLUDE} CACHE STRING "" FORCE)
                    set(VBAM_VS_LIB_DIRS     $ENV{LIB}     CACHE STRING "" FORCE)
                else()
                    message(WARNING "Failed to load Visual Studio environment (exit code: ${VBAM_VS_ENV_RESULT}).")
                endif()

                unset(VBAM_VS_ENV)
                unset(VBAM_VS_ENV_RESULT)
                unset(VBAM_VS_ARCH)
            endif()

            unset(VBAM_VCVARSALL)
            unset(VBAM_VS_INSTALL_PATH)
        endif()
    endif()

    unset(VBAM_CL_EXE_CHECK CACHE)
endif()

if(NOT DEFINED VCPKG_TARGET_TRIPLET)
    if(NOT WIN32)
        return()
    endif()

    # Check if we are in an MSVC environment.
    find_program(cl_exe_path NAME cl.exe HINTS ENV PATH)

    if(ENV{CXX} MATCHES "cl.exe$" OR cl_exe_path)
        # Infer the architecture from the LIB folders.
        foreach(lib $ENV{LIB})
            if(lib MATCHES "x64$")
                set(VBAM_VCPKG_PLATFORM "x64-windows-static")
                break()
            endif()
            if(lib MATCHES "x86$")
                set(VBAM_VCPKG_PLATFORM "x86-windows-static")
                break()
            endif()
            if(lib MATCHES "ARM64$")
                set(VBAM_VCPKG_PLATFORM "arm64-windows-static")

                foreach(path $ENV{PATH})
                    if(path MATCHES "[Hh]ost[Xx]64")
                        set(VCPKG_HOST_TRIPLET "x64-windows" CACHE STRING "Vcpkg host triplet" FORCE)
                        set(VCPKG_USE_HOST_TOOLS ON CACHE BOOL "Use vcpkg host tools" FORCE)
                    endif()
                endforeach()

                break()
            endif()
        endforeach()

        # If all else fails, try to use a sensible default.
        if(NOT DEFINED VBAM_VCPKG_PLATFORM)
            set(VBAM_VCPKG_PLATFORM "x64-windows-static")
        endif()

        unset(cl_exe_path)
    elseif (NOT DEFINED CMAKE_CXX_COMPILER)
        # No way to infer the compiler.
        return()

    elseif(${CMAKE_CXX_COMPILER} MATCHES "clang-cl.exe$" OR ${CMAKE_CXX_COMPILER} MATCHES "clang-cl$")
        # For stand-alone clang-cl, assume x64.
        set(VBAM_VCPKG_PLATFORM "x64-windows-static")
    endif()

    if (NOT DEFINED VBAM_VCPKG_PLATFORM)
        # Probably not an MSVC environment.
        return()
    endif()

    set(VCPKG_TARGET_TRIPLET ${VBAM_VCPKG_PLATFORM} CACHE STRING "Vcpkg target triplet (ex. x64-windows-static)" FORCE)
    message(STATUS "Inferred VCPKG_TARGET_TRIPLET=${VCPKG_TARGET_TRIPLET}")
endif()

# Remember a toolchain file the caller already selected.
#
# vcpkg_set_toolchain() below points CMAKE_TOOLCHAIN_FILE at vcpkg.cmake, which
# would throw away the caller's choice - and with it the cross-compilation
# setup. Qt's qt-cmake wrapper passes qt.toolchain.cmake through the
# CMAKE_TOOLCHAIN_FILE *environment* variable rather than -D, so without this
# an Android configure silently falls back to the host compiler.
#
# When one is found it stays as CMAKE_TOOLCHAIN_FILE and vcpkg.cmake is included
# next to it at the bottom of this file, which is all vcpkg needs for its
# find_package() integration and prefix paths.
if(NOT DEFINED VBAM_OUTER_TOOLCHAIN_FILE)
    set(vbam_outer_toolchain "")

    if(NOT "${CMAKE_TOOLCHAIN_FILE}" STREQUAL "")
        set(vbam_outer_toolchain "${CMAKE_TOOLCHAIN_FILE}")
    elseif(NOT "$ENV{CMAKE_TOOLCHAIN_FILE}" STREQUAL "")
        set(vbam_outer_toolchain "$ENV{CMAKE_TOOLCHAIN_FILE}")
    endif()

    # vcpkg's own toolchain is not an "outer" one, we load it unconditionally.
    if(vbam_outer_toolchain MATCHES "buildsystems/vcpkg\\.cmake$")
        set(vbam_outer_toolchain "")
    endif()

    # Cached because CMAKE_TOOLCHAIN_FILE gets rewritten below, and re-configures
    # of an existing build directory are usually run through plain cmake rather
    # than the qt-cmake wrapper that set the environment variable.
    set(VBAM_OUTER_TOOLCHAIN_FILE "${vbam_outer_toolchain}"
        CACHE INTERNAL "Caller-supplied toolchain file loaded alongside vcpkg")

    unset(vbam_outer_toolchain)
endif()

if(VBAM_OUTER_TOOLCHAIN_FILE)
    message(STATUS "Using caller-supplied toolchain file: ${VBAM_OUTER_TOOLCHAIN_FILE}")
endif()

# Android: hand the NDK toolchain to vcpkg. Nothing else selects a cross
# compiler - the vcpkg triplet only sets VCPKG_CMAKE_SYSTEM_NAME for vcpkg's own
# port builds, and the Qt toolchain chainloads whatever
# VCPKG_CHAINLOAD_TOOLCHAIN_FILE names, so setting it here serves both.
# ANDROID_SDK_ROOT is part of what this block exports, so a build directory
# configured before it was set here has to be able to pick it up: run again when
# it is missing, even though VCPKG_CHAINLOAD_TOOLCHAIN_FILE is already cached.
# Rediscovery is only environment lookups and one glob.
if(VCPKG_TARGET_TRIPLET MATCHES "-android$"
        AND (NOT VCPKG_CHAINLOAD_TOOLCHAIN_FILE OR NOT ANDROID_SDK_ROOT))
    set(vbam_android_ndk "")

    foreach(ndk_var CMAKE_ANDROID_NDK ANDROID_NDK_ROOT ANDROID_NDK ANDROID_NDK_HOME)
        if(NOT "${${ndk_var}}" STREQUAL "")
            set(vbam_android_ndk "${${ndk_var}}")
            break()
        endif()

        if(NOT "$ENV{${ndk_var}}" STREQUAL "")
            set(vbam_android_ndk "$ENV{${ndk_var}}")
            break()
        endif()
    endforeach()

    # The SDK root is wanted in its own right, not just as somewhere to look for
    # an NDK: Qt's androiddeployqt support reads ANDROID_SDK_ROOT to find
    # build-tools and the platform jars, and fails with 'Could not locate
    # Android SDK build tools under "/build-tools"' when it is unset.
    set(vbam_android_sdk "")

    foreach(sdk_var ANDROID_SDK_ROOT ANDROID_HOME ANDROID_SDK_HOME)
        if(NOT "${${sdk_var}}" STREQUAL "")
            set(vbam_android_sdk "${${sdk_var}}")
            break()
        endif()

        if(NOT "$ENV{${sdk_var}}" STREQUAL "")
            set(vbam_android_sdk "$ENV{${sdk_var}}")
            break()
        endif()
    endforeach()

    # Fall back to the newest NDK installed under the SDK root.
    if(vbam_android_ndk STREQUAL "" AND vbam_android_sdk)
        file(GLOB vbam_android_ndks "${vbam_android_sdk}/ndk/*")
        list(FILTER vbam_android_ndks INCLUDE REGEX "/[0-9]")
        list(SORT  vbam_android_ndks COMPARE NATURAL)
        list(POP_BACK vbam_android_ndks vbam_android_ndk)
        unset(vbam_android_ndks)
    endif()

    # And the other way round: an NDK installed at <sdk>/ndk/<version> names the
    # SDK it belongs to, which covers pointing only ANDROID_NDK_HOME at it.
    if(vbam_android_sdk STREQUAL "" AND vbam_android_ndk MATCHES "^(.+)/ndk/[^/]+/?$")
        set(vbam_android_sdk "${CMAKE_MATCH_1}")
    endif()

    if(NOT vbam_android_ndk OR NOT EXISTS "${vbam_android_ndk}/build/cmake/android.toolchain.cmake")
        message(FATAL_ERROR
            "Triplet '${VCPKG_TARGET_TRIPLET}' needs the Android NDK, but no NDK was found. "
            "Set ANDROID_NDK_HOME (or -DANDROID_NDK_ROOT=<path>) to an NDK containing "
            "build/cmake/android.toolchain.cmake."
        )
    endif()

    set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE "${vbam_android_ndk}/build/cmake/android.toolchain.cmake"
        CACHE FILEPATH "Toolchain file chainloaded by vcpkg" FORCE)

    message(STATUS "Android NDK toolchain: ${VCPKG_CHAINLOAD_TOOLCHAIN_FILE}")

    # Qt's Android support and androiddeployqt look the SDK and NDK up under
    # these names, and the NDK toolchain itself reads ANDROID_ABI.
    if(NOT ANDROID_NDK_ROOT)
        set(ANDROID_NDK_ROOT "${vbam_android_ndk}" CACHE PATH "Path to the Android NDK" FORCE)
    endif()

    if(NOT ANDROID_SDK_ROOT AND vbam_android_sdk)
        set(ANDROID_SDK_ROOT "${vbam_android_sdk}" CACHE PATH "Path to the Android SDK" FORCE)
    endif()

    # ANDROID_ABI is what the NDK toolchain reads, and it has no idea about the
    # vcpkg triplet: left unset it defaults to armeabi-v7a, so an arm64-android
    # configure would quietly build 32-bit ARM code against 64-bit vcpkg
    # packages. This runs before project(), so CMAKE_ANDROID_ARCH_ABI is not set
    # yet either and the triplet is the only thing naming the target.
    if(NOT ANDROID_ABI)
        if(CMAKE_ANDROID_ARCH_ABI)
            set(vbam_android_abi "${CMAKE_ANDROID_ARCH_ABI}")
        elseif(VCPKG_TARGET_TRIPLET STREQUAL "arm64-android")
            set(vbam_android_abi "arm64-v8a")
        elseif(VCPKG_TARGET_TRIPLET STREQUAL "arm-android")
            set(vbam_android_abi "armeabi-v7a")
        elseif(VCPKG_TARGET_TRIPLET STREQUAL "x64-android")
            set(vbam_android_abi "x86_64")
        elseif(VCPKG_TARGET_TRIPLET STREQUAL "x86-android")
            set(vbam_android_abi "x86")
        elseif(VCPKG_TARGET_TRIPLET STREQUAL "riscv64-android")
            set(vbam_android_abi "riscv64")
        else()
            set(vbam_android_abi "")
        endif()

        if(vbam_android_abi)
            set(ANDROID_ABI "${vbam_android_abi}" CACHE STRING "Android ABI" FORCE)
            message(STATUS "Android ABI for triplet ${VCPKG_TARGET_TRIPLET}: ${ANDROID_ABI}")
        endif()

        unset(vbam_android_abi)
    endif()

    # The API level to compile against. Every vcpkg *-android triplet builds its
    # ports against API 28 (VCPKG_CMAKE_SYSTEM_VERSION), which is also the app's
    # own minimum (QT_ANDROID_MIN_SDK_VERSION in src/wx/CMakeLists.txt), but the
    # NDK toolchain defaults to the oldest API it still supports. Compiling at
    # that older level against API-28 ports breaks at link time on symbols the
    # older headers define as macros -- stdin, stderr, __fread_chk and friends.
    if(NOT ANDROID_PLATFORM AND NOT ANDROID_NATIVE_API_LEVEL AND NOT CMAKE_SYSTEM_VERSION)
        set(ANDROID_PLATFORM "android-28" CACHE STRING "Android API level" FORCE)
        message(STATUS "Android API level: ${ANDROID_PLATFORM}")
    endif()

    unset(vbam_android_ndk)
    unset(vbam_android_sdk)
endif()

if(WIN32 AND VCPKG_TARGET_TRIPLET MATCHES "^x86-mingw")
    find_program(make_path NAME mingw32-make.exe)

    if(NOT make_path)
        # Assume MSYS2 MinGW32 toolchain.
        set(ENV{PATH} "C:/msys64/mingw32/bin;$ENV{PATH}")
    endif()
endif()

if(WIN32 AND VCPKG_TARGET_TRIPLET MATCHES "^x64-mingw")
    find_program(make_path NAME mingw32-make.exe)

    if(NOT make_path)
        # Assume MSYS2 MinGW32 toolchain.
        set(ENV{PATH} "C:/msys64/clang64/bin;$ENV{PATH}")
    endif()
endif()

# Detect MSVC toolkit version for binary package URL path.
# v143 = VS 2022 (VCToolsVersion env var starts with 14.3).
# No trailing separator: vcpkg_binary_package_dir() appends one only when there
# is a subdirectory, so a URL never ends up with an empty path part.
set(VBAM_VCPKG_TOOLKIT_SUBDIR "")
if("$ENV{VCToolsVersion}" MATCHES "^14\\.[34]")
    set(VBAM_VCPKG_TOOLKIT_SUBDIR "v143")
endif()

function(vcpkg_check_git_status git_status)
    # The VS vcpkg component cannot be written to without elevation.
    if(NOT git_status EQUAL 0 AND NOT VCPKG_ROOT MATCHES "Visual Studio")
        message(FATAL_ERROR "Error updating vcpkg from git, please make sure git for windows is installed correctly, it can be installed from Visual Studio components")
    endif()
endfunction()



function(vcpkg_is_installed pkg_name pkg_ver pkg_triplet powershell outvar)
    set(${outvar} FALSE PARENT_SCOPE)

    unset(CMAKE_MATCH_1)
    string(REGEX REPLACE "-r([0-9]+)\$" "" pkg_ver ${pkg_ver})
    set(pkg_rev ${CMAKE_MATCH_1})

    string(REPLACE "-" "." pkg_ver ${pkg_ver})

    if(NOT DEFINED VCPKG_INSTALLED_COUNT)
        execute_process(
            COMMAND ${powershell}
                -executionpolicy bypass -noprofile
                -command "import-module '${CMAKE_BINARY_DIR}/vcpkg-binpkg/vcpkg-binpkg.psm1'; vcpkg-list"
            OUTPUT_VARIABLE vcpkg_list_text
        )

        string(REGEX REPLACE "\r?\n" ";" vcpkg_list_raw "${vcpkg_list_text}")

        set(VCPKG_INSTALLED_COUNT 0 CACHE INTERNAL "Number of installed vcpkg packages" FORCE)
        set(VCPKG_INSTALLED_FEATURES "")

        foreach(pkg ${vcpkg_list_raw})
            # A feature is listed under its port as a row of its own, carrying
            # a description where the port carries a version -- so the match
            # below, which wants a version, never sees one. Which features a
            # port was built with is the one thing its name does not say, and
            # the only place it is written down.
            if(pkg MATCHES "^([A-Za-z0-9_.+-]+)\\[([^]]+)\\]:([^ ]+)")
                list(APPEND VCPKG_INSTALLED_FEATURES
                     "${CMAKE_MATCH_1}[${CMAKE_MATCH_2}]:${CMAKE_MATCH_3}")
                continue()
            endif()

            if(NOT pkg MATCHES "^([^:[]+)[^:]*:([^ ]+) +([0-9][^ ]*) +.*\$")
                continue()
            endif()
            set(inst_pkg_name    ${CMAKE_MATCH_1})
            set(inst_pkg_ver     ${CMAKE_MATCH_3})
            set(inst_pkg_triplet ${CMAKE_MATCH_2})

            unset(CMAKE_MATCH_1)
            string(REGEX REPLACE "#([0-9]+)\$" "" inst_pkg_ver ${inst_pkg_ver})
            if(CMAKE_MATCH_1)
                set(inst_pkg_rev ${CMAKE_MATCH_1})
            else()
                set(inst_pkg_rev FALSE)
            endif()

            string(REPLACE "-" "." inst_pkg_ver ${inst_pkg_ver})

            list(APPEND VCPKG_INSTALLED ${inst_pkg_name} ${inst_pkg_ver} ${inst_pkg_rev} ${inst_pkg_triplet})
            math(EXPR VCPKG_INSTALLED_COUNT "${VCPKG_INSTALLED_COUNT} + 1")
        endforeach()
        set(VCPKG_INSTALLED           ${VCPKG_INSTALLED}           CACHE INTERNAL "List of installed vcpkg packages"   FORCE)
        set(VCPKG_INSTALLED_COUNT     ${VCPKG_INSTALLED_COUNT}     CACHE INTERNAL "Number of installed vcpkg packages" FORCE)
        set(VCPKG_INSTALLED_FEATURES "${VCPKG_INSTALLED_FEATURES}" CACHE INTERNAL "Installed vcpkg port features"     FORCE)
    endif()

    if(NOT VCPKG_INSTALLED_COUNT GREATER 0)
        return()
    endif()

    math(EXPR idx_max "(${VCPKG_INSTALLED_COUNT} - 1) * 4")

    foreach(idx RANGE 0 ${idx_max} 4)
        math(EXPR idx_ver     "${idx} + 1")
        math(EXPR idx_rev     "${idx} + 2")
        math(EXPR idx_triplet "${idx} + 3")
        list(GET VCPKG_INSTALLED ${idx}         inst_pkg_name)
        list(GET VCPKG_INSTALLED ${idx_ver}     inst_pkg_ver)
        list(GET VCPKG_INSTALLED ${idx_rev}     inst_pkg_rev)
        list(GET VCPKG_INSTALLED ${idx_triplet} inst_pkg_triplet)

        if(NOT inst_pkg_triplet STREQUAL pkg_triplet)
            continue()
        endif()

        # The name has to hold for either arm. AND binds tighter than OR, so
        # written without the outer parentheses this asked for the name on the
        # first arm only, and any package of the triplet carrying the same
        # version satisfied the second.
        if(inst_pkg_name STREQUAL pkg_name
            AND (pkg_ver VERSION_LESS inst_pkg_ver
                OR (pkg_ver VERSION_EQUAL inst_pkg_ver
                    AND ((NOT pkg_rev AND NOT inst_pkg_rev)
                        OR (pkg_rev AND inst_pkg_rev AND (NOT pkg_rev GREATER inst_pkg_rev))))))

            set(${outvar} TRUE PARENT_SCOPE)
            return()
        endif()
    endforeach()
endfunction()

# Path under the binary package host for one triplet. The toolkit
# subdirectory applies only to the triplet being built for, and is appended
# only when there is one.
function(vcpkg_binary_package_dir triplet outvar)
    set(pkg_dir "${triplet}")

    if(triplet STREQUAL VCPKG_TARGET_TRIPLET AND VBAM_VCPKG_TOOLKIT_SUBDIR)
        set(pkg_dir "${triplet}/${VBAM_VCPKG_TOOLKIT_SUBDIR}")
    endif()

    set(${outvar} "${pkg_dir}" PARENT_SCOPE)
endfunction()

function(get_triplet_package_list triplet)
    if(EXISTS "${CMAKE_BINARY_DIR}/binary_package_list_${triplet}.html")
        return()
    endif()

    vcpkg_binary_package_dir("${triplet}" pkg_dir)

    # A file(DOWNLOAD) with no timeout waits for the other end forever, and a
    # configure that has reached this point has already said what it is doing,
    # so a server that accepts the connection and then goes quiet reads as a
    # build hung after the overlay checkout. A directory listing is small enough
    # for a plain ceiling.
    file(
        DOWNLOAD "https://nightly.visualboyadvance-m.org/vcpkg/${pkg_dir}" "${CMAKE_BINARY_DIR}/binary_package_list_${triplet}.html"
        STATUS pkg_list_status
        INACTIVITY_TIMEOUT 30
        TIMEOUT 60
    )
    list(GET pkg_list_status 1 pkg_list_error)
    list(GET pkg_list_status 0 pkg_list_status)

    if(NOT pkg_list_status EQUAL 0)
        message(STATUS "Failed to download vcpkg binary package list: ${pkg_list_status} - ${pkg_list_error}")
        file(REMOVE "${CMAKE_BINARY_DIR}/binary_package_list_${triplet}.html")
        return()
    endif()
endfunction()

function(download_package pkg pkgs_dir)
    string(REGEX REPLACE "^[^_]+_[^_]+_([^.]+)[.]zip\$" "\\1" pkg_triplet ${pkg})

    vcpkg_binary_package_dir("${pkg_triplet}" pkg_dir)

    message(STATUS "Downloading https://nightly.visualboyadvance-m.org/vcpkg/${pkg_dir}/${pkg} ...")

    # INACTIVITY_TIMEOUT and not TIMEOUT: a package runs to tens of megabytes,
    # and no ceiling fits both a fast link and a slow one. What is wanted is an
    # end to a transfer that has stopped moving, which is the one this measures.
    #
    # Tried more than once, because what follows a package that did not arrive
    # is the port built from source, and losing wxWidgets to a single stalled
    # socket costs a great deal more than asking again. A failed download also
    # leaves the part of the file that did arrive, and the caller reads the file
    # being there as the download having worked, so it goes.
    set(pkg_attempts 3)

    foreach(pkg_attempt RANGE 1 ${pkg_attempts})
        file(
            DOWNLOAD "https://nightly.visualboyadvance-m.org/vcpkg/${pkg_dir}/${pkg}" "${pkgs_dir}/${pkg}"
            STATUS pkg_download_status
            INACTIVITY_TIMEOUT 30
        )
        list(GET pkg_download_status 1 pkg_download_error)
        list(GET pkg_download_status 0 pkg_download_code)

        if(pkg_download_code EQUAL 0)
            message(STATUS "done.")
            return()
        endif()

        file(REMOVE "${pkgs_dir}/${pkg}")

        if(pkg_attempt LESS pkg_attempts)
            message(STATUS
                "Download of '${pkg}' failed (${pkg_download_error}), "
                "attempt ${pkg_attempt} of ${pkg_attempts}; trying again.")
        endif()
    endforeach()

    message(STATUS "Failed to download vcpkg binary package '${pkg}': ${pkg_download_code} - ${pkg_download_error}")
endfunction()

# The features of a port spec that the status database does not have installed
# for a triplet.
#
# `core` is never among them: it is not a feature but vcpkg's way of asking for
# a port without its default ones, so it is never installed under that name and
# looking for it would report every such port as short a feature forever.
#
# Reading the status database is the only way to ask. A package is named for
# its port, its version and its triplet, and nothing in that says which of the
# port's features are built into it, so two builds of one version that differ
# only in features are indistinguishable until one is installed.
function(vcpkg_missing_features spec triplet outvar)
    set(${outvar} "" PARENT_SCOPE)

    if(NOT spec MATCHES "^([^[]+)\\[([^]]+)\\]\$")
        return()
    endif()

    set(port "${CMAKE_MATCH_1}")

    string(REPLACE "," ";" features "${CMAKE_MATCH_2}")

    set(missing "")

    foreach(feature ${features})
        if(feature STREQUAL "core")
            continue()
        endif()

        if(NOT "${port}[${feature}]:${triplet}" IN_LIST VCPKG_INSTALLED_FEATURES)
            list(APPEND missing "${feature}")
        endif()
    endforeach()

    set(${outvar} "${missing}" PARENT_SCOPE)
endfunction()

# Of the packages named after one port, the one with the highest version.
#
# A directory can hold more than one version of a port -- an upload that failed
# to clear the previous one is enough -- and then taking whichever was listed
# first, or refusing to choose between them, leaves the newest unused. The
# version carries no underscore, so the three fields of the name are
# unambiguous.
function(vcpkg_newest_package port packages outvar)
    set(${outvar} "" PARENT_SCOPE)

    set(best     "")
    set(best_ver "")
    set(best_rev "")

    foreach(pkg ${packages})
        if(NOT pkg MATCHES "^${port}_([^_]+)_[^_]+[.]zip$")
            continue()
        endif()

        set(ver "${CMAKE_MATCH_1}")

        unset(CMAKE_MATCH_1)
        string(REGEX REPLACE "-r([0-9]+)$" "" ver_num "${ver}")
        set(rev "${CMAKE_MATCH_1}")

        if(NOT rev)
            set(rev 0)
        endif()

        # A version can carry dashes of its own (3.3.4-17); compare it as the
        # dotted number it stands for.
        string(REPLACE "-" "." ver_num "${ver_num}")

        if(NOT best
                OR ver_num VERSION_GREATER best_ver
                OR (ver_num VERSION_EQUAL best_ver AND rev GREATER best_rev))
            set(best     "${pkg}")
            set(best_ver "${ver_num}")
            set(best_rev "${rev}")
        endif()
    endforeach()

    set(${outvar} "${best}" PARENT_SCOPE)
endfunction()

# The packages installed for one triplet, read from the listing
# vcpkg_is_installed() caches.
function(vcpkg_installed_ports triplet outvar)
    set(${outvar} "" PARENT_SCOPE)

    if(NOT DEFINED VCPKG_INSTALLED_COUNT)
        # Any query populates the cache; the answer is not wanted.
        vcpkg_is_installed(vcpkg-cmake 0 ${triplet} ${POWERSHELL} vcpkg_installed_ports_ignored)
    endif()

    if(NOT VCPKG_INSTALLED_COUNT GREATER 0)
        return()
    endif()

    math(EXPR idx_max "(${VCPKG_INSTALLED_COUNT} - 1) * 4")
    set(ports "")

    foreach(idx RANGE 0 ${idx_max} 4)
        math(EXPR idx_triplet "${idx} + 3")
        list(GET VCPKG_INSTALLED ${idx}         inst_pkg_name)
        list(GET VCPKG_INSTALLED ${idx_triplet} inst_pkg_triplet)

        if(inst_pkg_triplet STREQUAL triplet)
            list(APPEND ports "${inst_pkg_name}")
        endif()
    endforeach()

    list(REMOVE_DUPLICATES ports)
    set(${outvar} "${ports}" PARENT_SCOPE)
endfunction()

# Of a set of ports, those with nothing installed for a triplet.
#
# Read from installed/vcpkg/info, which carries one .list per installed
# package. Neither vcpkg nor vcpkg-binpkg is run to read it, because the caller
# asks at a point where reaching the server has already failed and the tool may
# never have been fetched.
#
# By name, not by version or by feature: the question is whether a port is
# there at all. Which version ought to be there is a question about what the
# server has, and this is asked precisely when the server could not be asked.
function(vcpkg_ports_not_installed ports triplet outvar)
    set(absent "")

    foreach(port ${ports})
        file(GLOB port_lists
             "${VCPKG_ROOT}/installed/vcpkg/info/${port}_*_${triplet}.list")

        if(NOT port_lists)
            list(APPEND absent "${port}")
        endif()
    endforeach()

    set(${outvar} "${absent}" PARENT_SCOPE)
endfunction()

function(zip_is_installed zip outvar)
    if(NOT zip MATCHES "([^_]+)_([^_]+)_([^.]+)[.]zip")
        return()
    endif()
    set(pkg_name    ${CMAKE_MATCH_1})
    set(pkg_version ${CMAKE_MATCH_2})
    set(pkg_triplet ${CMAKE_MATCH_3})

    vcpkg_is_installed(${pkg_name} ${pkg_version} ${pkg_triplet} ${POWERSHELL} pkg_installed)

    set(${outvar} ${pkg_installed} PARENT_SCOPE)
endfunction()

function(cleanup_binary_packages)
    file(REMOVE_RECURSE "${CMAKE_BINARY_DIR}/vcpkg-binary-packages")

    unset(VCPKG_INSTALLED       CACHE)
    unset(VCPKG_INSTALLED_COUNT CACHE)
endfunction()

# The triplet vcpkg builds host tools for: the machine doing the building, which
# is only the target triplet when not cross-compiling.
function(vcpkg_host_triplet outvar)
    if(VCPKG_HOST_TRIPLET)
        set(${outvar} "${VCPKG_HOST_TRIPLET}" PARENT_SCOPE)
        return()
    endif()

    if(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "^([aA][aA][rR][cC][hH]64|[aA][rR][mM]64)$")
        set(host_arch arm64)
    else()
        set(host_arch x64)
    endif()

    if(CMAKE_HOST_WIN32)
        set(host_os windows)
    elseif(CMAKE_HOST_APPLE)
        set(host_os osx)
    else()
        set(host_os linux)
    endif()

    set(${outvar} "${host_arch}-${host_os}" PARENT_SCOPE)
endfunction()

# Install the host tools the target packages ask for from binary packages rather
# than leaving vcpkg to build them. Cross-compiling for Android needs a host Qt
# to run moc and androiddeployqt, and building one from source costs more than
# the rest of the dependency set together, once per ABI.
#
# Android asks for the direct host dependencies only, matching how the nightly
# packages them. The transitive closure is the right answer for a host expected
# to build those tools from nothing, and the wrong one there: a Qt in the set
# drags its whole desktop stack in behind it -- fontconfig, dbus, libsystemd --
# which the host already has and no APK put on the server. Every other cross
# target keeps the closure.
#
# Called after the target set is installed because vcpkg-listhostdeps reads
# those packages' entries out of the vcpkg status file.
function(get_host_binary_packages wanted_ports outvar)
    set(${outvar} TRUE PARENT_SCOPE)

    vcpkg_host_triplet(host_triplet)

    if(host_triplet STREQUAL VCPKG_TARGET_TRIPLET)
        return()
    endif()

    # Ask only about ports that actually made it in; listhostdeps errors out on
    # a package that is not installed.
    set(qualified "")

    foreach(port ${wanted_ports})
        vcpkg_is_installed(${port} 0 ${VCPKG_TARGET_TRIPLET} ${POWERSHELL} port_installed)

        if(port_installed)
            list(APPEND qualified "${port}:${VCPKG_TARGET_TRIPLET}")
        endif()
    endforeach()

    if(NOT qualified)
        return()
    endif()

    string(REPLACE ";" " " qualified_args "${qualified}")

    if(VCPKG_TARGET_TRIPLET MATCHES "-android$")
        set(host_deps_scope "-Direct ")
    else()
        set(host_deps_scope "")
    endif()

    execute_process(
        COMMAND ${POWERSHELL}
            -executionpolicy bypass -noprofile
            -command "import-module '${CMAKE_BINARY_DIR}/vcpkg-binpkg/vcpkg-binpkg.psm1'; vcpkg-listhostdeps ${host_deps_scope}${qualified_args}"
        OUTPUT_VARIABLE host_deps
        RESULT_VARIABLE host_deps_status
        ERROR_VARIABLE  host_deps_error
    )

    if(NOT host_deps_status EQUAL 0)
        string(STRIP "${host_deps_error}" host_deps_error)
        message(STATUS "Could not list host dependencies (${host_deps_error}); vcpkg will build any that are missing.")
        set(${outvar} FALSE PARENT_SCOPE)
        return()
    endif()

    string(REGEX REPLACE "\r?\n" ";" host_deps "${host_deps}")
    list(FILTER host_deps EXCLUDE REGEX "^ *$")

    if(NOT host_deps)
        return()
    endif()

    list(REMOVE_DUPLICATES host_deps)

    get_triplet_package_list(${host_triplet})

    if(NOT EXISTS "${CMAKE_BINARY_DIR}/binary_package_list_${host_triplet}.html")
        message(STATUS "No binary package list for host triplet '${host_triplet}'; vcpkg will build the host tools.")
        set(${outvar} FALSE PARENT_SCOPE)
        return()
    endif()

    file(READ "${CMAKE_BINARY_DIR}/binary_package_list_${host_triplet}.html" raw_html)

    set(host_pkgs_dir ${CMAKE_BINARY_DIR}/vcpkg-host-binary-packages)
    file(REMOVE_RECURSE ${host_pkgs_dir})
    file(MAKE_DIRECTORY ${host_pkgs_dir})

    set(host_all_found TRUE)
    set(host_to_install "")

    foreach(dep ${host_deps})
        vcpkg_is_installed(${dep} 0 ${host_triplet} ${POWERSHELL} dep_installed)

        if(dep_installed)
            continue()
        endif()

        string(REGEX MATCHALL "<a href=\"${dep}_[^\"]+[.]zip\"" links "${raw_html}")
        list(LENGTH links links_count)

        if(NOT links_count EQUAL 1)
            message(STATUS "No single binary package for host dependency '${dep}:${host_triplet}', will build from source.")
            set(host_all_found FALSE)
            continue()
        endif()

        string(REGEX REPLACE "<a href=\"([^\"]+[.]zip)\"" "\\1" pkg ${links})

        download_package("${pkg}" "${host_pkgs_dir}")

        if(EXISTS "${host_pkgs_dir}/${pkg}")
            list(APPEND host_to_install ${pkg})
        else()
            message(STATUS "Failed to download host dependency '${pkg}', will build from source.")
            set(host_all_found FALSE)
        endif()
    endforeach()

    if(host_to_install)
        execute_process(
            COMMAND ${POWERSHELL}
                -executionpolicy bypass -noprofile
                -command "import-module '${CMAKE_BINARY_DIR}/vcpkg-binpkg/vcpkg-binpkg.psm1'; vcpkg-instpkg ."
            WORKING_DIRECTORY ${host_pkgs_dir}
        )
    endif()

    file(REMOVE_RECURSE ${host_pkgs_dir})

    if(NOT host_all_found)
        set(${outvar} FALSE PARENT_SCOPE)
    endif()
endfunction()

# git, told not to ask. A repository that is private, renamed or misspelled is
# one the server declines to describe rather than one it says is absent, so git
# asks for a username -- and execute_process() has the terminal, so configure
# stops dead on a prompt for credentials to a repository nobody meant to name.
# GIT_TERMINAL_PROMPT covers git's own asking, an empty credential.helper the
# helper that would otherwise answer for it, and the unset ASKPASS variables
# the program that would put up a window.
function(git_no_prompt outvar)
    set(${outvar}
        ${CMAKE_COMMAND} -E env GIT_TERMINAL_PROMPT=0
                                --unset=GIT_ASKPASS
                                --unset=SSH_ASKPASS
        ${GIT_EXECUTABLE} -c credential.helper=
        PARENT_SCOPE
    )
endfunction()

function(get_binary_packages)
    set(binary_packages_installed FALSE PARENT_SCOPE)

    # Build the list of wanted port names from VCPKG_DEPS and VCPKG_DEPS_OPTIONAL.
    set(wanted_ports "")

    # Add core dependencies. A package is named for its port alone, so the
    # features come off whatever is matched against the listing; the spec is
    # kept beside the name for what gets handed back to vcpkg, where they still
    # mean something.
    foreach(dep ${VCPKG_DEPS})
        string(REGEX REPLACE "\\[.*\\]" "" port_name "${dep}")
        list(APPEND wanted_ports "${port_name}")
        set(wanted_spec_${port_name} "${dep}")
    endforeach()

    # Add optional dependencies unless explicitly turned off.
    list(LENGTH VCPKG_DEPS_OPTIONAL optionals_list_len)
    if(optionals_list_len GREATER 0)
        math(EXPR optionals_list_last "${optionals_list_len} - 1")

        foreach(i RANGE 0 ${optionals_list_last} 2)
            list(GET VCPKG_DEPS_OPTIONAL ${i} dep)
            math(EXPR var_idx "${i} + 1")
            list(GET VCPKG_DEPS_OPTIONAL ${var_idx} var)

            if(NOT DEFINED ${var} OR ${var})
                string(REGEX REPLACE "\\[.*\\]" "" port_name "${dep}")
                list(APPEND wanted_ports "${port_name}")
                set(wanted_spec_${port_name} "${dep}")
            endif()
        endforeach()
    endif()

    # Handed back for the caller's use whatever happens below: when this
    # function cannot reach the packages at all it has nothing to say about
    # which ports are missing, and the ports that were wanted are then the only
    # thing left to go on.
    set(VCPKG_WANTED_PORTS "${wanted_ports}" PARENT_SCOPE)

    if(NOT wanted_ports)
        message(STATUS "No packages to install.")
        return()
    endif()

    # Download the package listing for the target triplet.
    get_triplet_package_list(${VCPKG_TARGET_TRIPLET})

    if(NOT EXISTS "${CMAKE_BINARY_DIR}/binary_package_list_${VCPKG_TARGET_TRIPLET}.html")
        message(STATUS "Failed to download binary package list for triplet '${VCPKG_TARGET_TRIPLET}'.")
        return()
    endif()

    file(READ "${CMAKE_BINARY_DIR}/binary_package_list_${VCPKG_TARGET_TRIPLET}.html" raw_html)
    string(REGEX MATCHALL "<a href=\"[^\"]+[.]zip\"" links "${raw_html}")
    set(all_packages "")
    foreach(link ${links})
        string(REGEX REPLACE "<a href=\"([^\"]+[.]zip)\"" "\\1" pkg "${link}")
        list(APPEND all_packages ${pkg})
    endforeach()

    if(NOT all_packages)
        message(STATUS "No binary packages available for triplet '${VCPKG_TARGET_TRIPLET}'.")
        return()
    endif()

    # Match wanted ports against available binary packages.
    # Skip ports with no binary package instead of aborting; record
    # which ones missed so the caller can install just those from
    # source rather than re-running vcpkg install on the whole dep
    # set (which can cascade into rebuilding already-binary-installed
    # ports when their port-tree versions differ from the binary
    # cache versions).
    set(binary_packages "")
    set(missing_ports "")
    set(all_ports_found TRUE)
    foreach(port ${wanted_ports})
        vcpkg_newest_package("${port}" "${all_packages}" pkg)

        if(pkg)
            list(APPEND binary_packages "${pkg}")
        else()
            message(STATUS "No binary package found for port '${port}', will build from source.")

            # The spec, not the name: this list is what a source install is
            # given, and a port built without the features a frontend asked for
            # is not the port that was asked for.
            list(APPEND missing_ports "${wanted_spec_${port}}")
            set(all_ports_found FALSE)
        endif()
    endforeach()

    # Expose missing ports to the caller for surgical source-installs.
    set(VCPKG_MISSING_PORTS "${missing_ports}" PARENT_SCOPE)


    if(NOT binary_packages)
        return()
    endif()

    # Fetch vcpkg-binpkg tool. Download only when the remote head commit differs
    # from the extracted one. Not FetchContent: it stamps the URL and never
    # re-fetches a branch archive, so tool fixes never reach an existing build.
    set(vcpkg_binpkg_dir ${CMAKE_BINARY_DIR}/vcpkg-binpkg)
    set(vcpkg_binpkg_zip ${CMAKE_BINARY_DIR}/vcpkg-binpkg.zip)
    set(vcpkg_binpkg_tmp ${CMAKE_BINARY_DIR}/vcpkg-binpkg-extract)
    set(vcpkg_binpkg_url "https://github.com/rkitover/vcpkg-binpkg-prototype")

    set(vcpkg_binpkg_head "")

    find_package(Git QUIET)

    if(GIT_FOUND)
        git_no_prompt(git_cmd)

        execute_process(
            COMMAND ${git_cmd} ls-remote "${vcpkg_binpkg_url}.git" refs/heads/master
            OUTPUT_VARIABLE vcpkg_binpkg_ls_remote
            ERROR_QUIET
            RESULT_VARIABLE vcpkg_binpkg_ls_error
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )

        if(vcpkg_binpkg_ls_error EQUAL 0)
            string(REGEX MATCH "^[0-9a-f]+" vcpkg_binpkg_head "${vcpkg_binpkg_ls_remote}")
        endif()
    endif()

    set(vcpkg_binpkg_have_tool FALSE)
    if(EXISTS "${vcpkg_binpkg_dir}/vcpkg-binpkg.psm1")
        set(vcpkg_binpkg_have_tool TRUE)
    endif()

    if(vcpkg_binpkg_head AND vcpkg_binpkg_have_tool
        AND vcpkg_binpkg_head STREQUAL "${VCPKG_BINPKG_COMMIT}")
        # Up to date.
    else()
        if(vcpkg_binpkg_head)
            set(vcpkg_binpkg_archive "${vcpkg_binpkg_url}/archive/${vcpkg_binpkg_head}.zip")
        else()
            # No usable remote answer; keep what is there, or fall back to
            # master if there is nothing to keep.
            if(vcpkg_binpkg_have_tool)
                message(STATUS "Could not reach vcpkg-binpkg remote; using the extracted copy.")
                set(vcpkg_binpkg_archive "")
            else()
                set(vcpkg_binpkg_archive "${vcpkg_binpkg_url}/archive/refs/heads/master.zip")
            endif()
        endif()

        if(vcpkg_binpkg_archive)
            file(DOWNLOAD "${vcpkg_binpkg_archive}" "${vcpkg_binpkg_zip}"
                STATUS vcpkg_binpkg_status
                INACTIVITY_TIMEOUT 30
                TIMEOUT 60
            )
            list(GET vcpkg_binpkg_status 0 vcpkg_binpkg_error)

            if(vcpkg_binpkg_error EQUAL 0)
                file(REMOVE_RECURSE "${vcpkg_binpkg_tmp}")
                file(MAKE_DIRECTORY "${vcpkg_binpkg_tmp}")
                file(ARCHIVE_EXTRACT INPUT "${vcpkg_binpkg_zip}" DESTINATION "${vcpkg_binpkg_tmp}")

                # Strip the archive's top-level directory.
                file(GLOB_RECURSE vcpkg_binpkg_module "${vcpkg_binpkg_tmp}/*/vcpkg-binpkg.psm1")

                if(vcpkg_binpkg_module)
                    list(GET vcpkg_binpkg_module 0 vcpkg_binpkg_module)
                    get_filename_component(vcpkg_binpkg_root "${vcpkg_binpkg_module}" DIRECTORY)
                    file(REMOVE_RECURSE "${vcpkg_binpkg_dir}")
                    file(RENAME "${vcpkg_binpkg_root}" "${vcpkg_binpkg_dir}")
                    set(vcpkg_binpkg_have_tool TRUE)
                    set(VCPKG_BINPKG_COMMIT "${vcpkg_binpkg_head}" CACHE INTERNAL
                        "Commit of the extracted vcpkg-binpkg tool")
                    if(vcpkg_binpkg_head)
                        message(STATUS "Updated vcpkg-binpkg tool to ${vcpkg_binpkg_head}.")
                    else()
                        message(STATUS "Updated vcpkg-binpkg tool from master.")
                    endif()
                else()
                    message(WARNING "vcpkg-binpkg archive has no vcpkg-binpkg.psm1.")
                endif()

                file(REMOVE_RECURSE "${vcpkg_binpkg_tmp}")
                file(REMOVE "${vcpkg_binpkg_zip}")
            elseif(NOT vcpkg_binpkg_have_tool)
                list(GET vcpkg_binpkg_status 1 vcpkg_binpkg_message)
                message(STATUS "Could not fetch vcpkg-binpkg (${vcpkg_binpkg_message}); binary packages unavailable.")
                return()
            endif()
        endif()
    endif()

    if(NOT vcpkg_binpkg_have_tool)
        message(STATUS "vcpkg-binpkg tool unavailable; binary packages unavailable.")
        return()
    endif()

    # An installed package can fall behind the server with nothing to notice.
    # A port reached only as a dependency is not in VCPKG_DEPS, so the matching
    # above never considers it, and the missing-dependency walk further down
    # sees only the dependencies of zips it has downloaded -- so when the
    # dependent is itself current, and its zip is therefore never fetched, its
    # stale dependency is invisible from every direction.
    #
    # What that leaves is a tree whose packages no longer agree with each other:
    # an fmt older than the openal-soft built against it, an x264 whose
    # X264_BUILD is a release behind the ffmpeg built against it. Neither is
    # missing, so nothing complains until the link, which then reports
    # undefined references to symbols in libraries that are right there.
    #
    # So offer everything installed for this triplet whatever the server has.
    # zip_is_installed() below keeps the ones already current.
    vcpkg_installed_ports(${VCPKG_TARGET_TRIPLET} installed_ports)

    foreach(port ${installed_ports})
        if(port IN_LIST wanted_ports)
            continue()
        endif()

        vcpkg_newest_package("${port}" "${all_packages}" pkg)

        if(pkg)
            list(APPEND binary_packages "${pkg}")
        endif()
    endforeach()

    # Filter out already-installed packages.
    foreach(pkg ${binary_packages})
        zip_is_installed(${pkg} pkg_installed)

        if(pkg_installed)
            # The same version can still be the wrong build of it. Unpack the
            # package again when the port is installed without a feature that
            # was asked for: a package's CONTROL carries its features, and
            # installing is what writes them to the status database, so one
            # that has the feature settles this in a single download.
            #
            # One that does not have it says so again every configure, which is
            # what a package built from a port list that no longer agrees with
            # this one looks like from here. Only the report repeats -- the
            # port is not sent to be built from source over a feature, because
            # a disagreement about wxWidgets would then rebuild wxWidgets every
            # time rather than saying which feature it is short.
            string(REGEX REPLACE "_.*" "" pkg_port "${pkg}")

            vcpkg_missing_features("${wanted_spec_${pkg_port}}"
                                   "${VCPKG_TARGET_TRIPLET}" pkg_features_missing)

            if(pkg_features_missing)
                message(STATUS
                    "Port '${pkg_port}' is installed without feature(s) "
                    "${pkg_features_missing}, reinstalling it.")

                set(pkg_installed FALSE)
            endif()
        endif()

        if(NOT pkg_installed)
            list(APPEND to_install ${pkg})
        endif()
    endforeach()

    if(to_install)
        set(bin_pkgs_dir ${CMAKE_BINARY_DIR}/vcpkg-binary-packages)
        file(MAKE_DIRECTORY ${bin_pkgs_dir})

        foreach(pkg ${to_install})
            download_package("${pkg}" "${bin_pkgs_dir}")
            if(NOT EXISTS "${bin_pkgs_dir}/${pkg}")
                message(STATUS "Failed to download package '${pkg}', will build from source.")
                set(all_ports_found FALSE)
            endif()
        endforeach()

        # Install any missing dependencies not in the wanted list.
        # Use progress_made to terminate: if no new deps are downloaded
        # in an iteration, stop (remaining gaps handled by source install).
        while(TRUE)
            execute_process(
                COMMAND ${POWERSHELL}
                    -executionpolicy bypass -noprofile
                    -command "import-module '${CMAKE_BINARY_DIR}/vcpkg-binpkg/vcpkg-binpkg.psm1'; vcpkg-listmissing ."
                WORKING_DIRECTORY ${bin_pkgs_dir}
                OUTPUT_VARIABLE missing_deps
                RESULT_VARIABLE missing_deps_status
            )

            if(NOT missing_deps_status EQUAL 0)
                message(STATUS "Failed to determine missing dependencies, will build remaining from source.")
                set(all_ports_found FALSE)
                break()
            endif()

            string(REGEX REPLACE "\r?\n"   ";" missing_deps "${missing_deps}")
            string(REGEX REPLACE " *;+ *$" ""  missing_deps "${missing_deps}")
            list(FILTER missing_deps EXCLUDE REGEX "^$")

            if(NOT missing_deps)
                break()
            endif()

            set(progress_made FALSE)

            foreach(dep ${missing_deps})
                if(NOT dep MATCHES "^([^:]+):([^:]+)\$")
                    continue()
                endif()
                set(dep_name    ${CMAKE_MATCH_1})
                set(dep_triplet ${CMAKE_MATCH_2})

                # A dependency for another triplet is a host tool the target
                # packages were built with, which only changes what a failure to
                # find one means: vcpkg builds it if something still needs it,
                # rather than this configure building the port itself.
                set(host_dep FALSE)

                if(NOT dep_triplet STREQUAL VCPKG_TARGET_TRIPLET)
                    set(host_dep TRUE)
                endif()

                get_triplet_package_list(${dep_triplet})

                if(NOT EXISTS "${CMAKE_BINARY_DIR}/binary_package_list_${dep_triplet}.html")
                    message(STATUS "No package list for triplet '${dep_triplet}', cannot resolve missing dependency '${dep_name}'.")
                    set(all_ports_found FALSE)
                    continue()
                endif()

                file(READ "${CMAKE_BINARY_DIR}/binary_package_list_${dep_triplet}.html" raw_html)
                string(REGEX MATCHALL "<a href=\"[^\"]+[.]zip\"" links "${raw_html}")

                set(dep_packages "")

                foreach(link ${links})
                    string(REGEX REPLACE "<a href=\"([^\"]+[.]zip)\"" "\\1" dep_pkg "${link}")
                    list(APPEND dep_packages "${dep_pkg}")
                endforeach()

                vcpkg_newest_package("${dep_name}" "${dep_packages}" pkg)

                if(NOT pkg)
                    # Nothing on offer. Only worth saying so when the port is
                    # not there at all: one that is installed and simply has no
                    # package needs nothing from anybody.
                    vcpkg_is_installed(${dep_name} 0 ${dep_triplet} ${POWERSHELL} pkg_installed)

                    if(NOT pkg_installed)
                        if(host_dep)
                            message(STATUS "No binary package for host tool '${dep_name}:${dep_triplet}'; vcpkg will build it if it is needed.")
                        else()
                            message(STATUS "No package found for missing dependency '${dep_name}' for triplet '${dep_triplet}', will build from source.")
                        endif()

                        set(all_ports_found FALSE)
                    endif()

                    continue()
                endif()

                # Compare against the version on offer rather than asking
                # whether some version of the port is installed. A port reached
                # only as a dependency is never named in VCPKG_DEPS, so nothing
                # else ever revisits it, and treating any version as good
                # enough pinned it at whatever landed first: an fmt from before
                # the openal-soft that needs it, an x264 whose X264_BUILD is a
                # release behind the ffmpeg built against it. Both turn up as
                # undefined references at link, a long way from here.
                zip_is_installed("${pkg}" pkg_installed)

                if(pkg_installed)
                    continue()
                endif()

                # Skip if already downloaded.
                if(EXISTS "${bin_pkgs_dir}/${pkg}")
                    continue()
                endif()

                download_package("${pkg}" "${bin_pkgs_dir}")

                if(EXISTS "${bin_pkgs_dir}/${pkg}")
                    set(progress_made TRUE)
                elseif(host_dep)
                    message(STATUS "Failed to download host tool '${pkg}'; vcpkg will build it if it is needed.")
                    set(all_ports_found FALSE)
                else()
                    message(STATUS "Failed to download missing dependency '${pkg}', will build from source.")
                    set(all_ports_found FALSE)
                endif()
            endforeach()

            if(NOT progress_made)
                break()
            endif()
        endwhile()

        # Log any packages that will be skipped due to incomplete dependencies.
        execute_process(
            COMMAND ${POWERSHELL}
                -executionpolicy bypass -noprofile
                -command "import-module '${CMAKE_BINARY_DIR}/vcpkg-binpkg/vcpkg-binpkg.psm1'; vcpkg-pruneincomplete ."
            WORKING_DIRECTORY ${bin_pkgs_dir}
            OUTPUT_VARIABLE incomplete_pkgs
        )
        if(incomplete_pkgs)
            string(STRIP "${incomplete_pkgs}" incomplete_pkgs)
            message(STATUS "Binary packages: skipping (incomplete dependencies): ${incomplete_pkgs}")
            set(all_ports_found FALSE)
        endif()

        # Install packages, skipping any with incomplete dependencies.
        execute_process(
            COMMAND ${POWERSHELL}
                -executionpolicy bypass -noprofile
                -command "import-module '${CMAKE_BINARY_DIR}/vcpkg-binpkg/vcpkg-binpkg.psm1'; vcpkg-instpkg ."
            WORKING_DIRECTORY ${bin_pkgs_dir}
        )

        file(REMOVE_RECURSE ${bin_pkgs_dir})
    endif()

    get_host_binary_packages("${wanted_ports}" host_packages_installed)

    if(NOT host_packages_installed)
        set(all_ports_found FALSE)
    endif()

    cleanup_binary_packages()

    if(all_ports_found)
        set(binary_packages_installed TRUE PARENT_SCOPE)
    endif()
endfunction()


# The overlay from a GitHub repository's archive of a branch.
#
# Downloaded only when the remote head differs from the extracted one, the way
# the vcpkg-binpkg tool above is handled: a branch archive has no version to
# stamp, so nothing else would ever refresh it.
function(fetch_vcpkg_overlay_archive repo overlay_dir)
    string(REGEX REPLACE "/+$"      "" repo "${repo}")
    string(REGEX REPLACE "[.]git$"  "" repo "${repo}")

    set(head "")

    find_package(Git QUIET)

    if(GIT_FOUND)
        git_no_prompt(git_cmd)

        execute_process(
            COMMAND ${git_cmd} ls-remote "${repo}.git" refs/heads/master
            OUTPUT_VARIABLE ls_remote
            ERROR_QUIET
            RESULT_VARIABLE ls_error
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )

        if(ls_error EQUAL 0)
            string(REGEX MATCH "^[0-9a-f]+" head "${ls_remote}")
        endif()
    endif()

    file(GLOB portfiles "${overlay_dir}/*/portfile.cmake")

    if(portfiles AND head AND head STREQUAL "${VCPKG_OVERLAY_COMMIT}")
        return()
    endif()

    if(NOT head AND portfiles)
        message(STATUS "Could not reach ${repo}; using the extracted vcpkg overlay.")
        return()
    endif()

    if(head)
        set(ref "${head}")
    else()
        set(ref "refs/heads/master")
    endif()

    # GitHub serves a ref as .zip and as .tar.gz, and file(ARCHIVE_EXTRACT)
    # reads either from the bytes rather than the name. It does not serve .tgz,
    # so .tar.gz is the second name to try.
    set(fetched FALSE)

    foreach(ext zip tar.gz)
        set(archive "${CMAKE_BINARY_DIR}/vcpkg-overlay.${ext}")

        file(DOWNLOAD "${repo}/archive/${ref}.${ext}" "${archive}"
            STATUS download_status
            INACTIVITY_TIMEOUT 30
            TIMEOUT 60
        )

        list(GET download_status 0 download_error)

        if(download_error EQUAL 0)
            set(fetched TRUE)
            break()
        endif()

        list(GET download_status 1 download_message)
        file(REMOVE "${archive}")
    endforeach()

    if(NOT fetched)
        if(NOT portfiles)
            message(STATUS "Could not fetch the vcpkg overlay (${download_message}).")
        endif()

        return()
    endif()

    set(tmp "${CMAKE_BINARY_DIR}/vcpkg-overlay-extract")

    file(REMOVE_RECURSE "${tmp}")
    file(MAKE_DIRECTORY "${tmp}")
    file(ARCHIVE_EXTRACT INPUT "${archive}" DESTINATION "${tmp}")

    # Strip the archive's top-level directory.
    file(GLOB roots LIST_DIRECTORIES true "${tmp}/*")
    set(root "")

    foreach(candidate ${roots})
        if(IS_DIRECTORY "${candidate}")
            set(root "${candidate}")
            break()
        endif()
    endforeach()

    if(root)
        file(REMOVE_RECURSE "${overlay_dir}")
        file(RENAME "${root}" "${overlay_dir}")

        set(VCPKG_OVERLAY_COMMIT "${head}" CACHE INTERNAL
            "Commit of the extracted vcpkg overlay")

        if(head)
            message(STATUS "Updated the vcpkg overlay to ${head}.")
        else()
            message(STATUS "Updated the vcpkg overlay from master.")
        endif()
    else()
        message(WARNING "The vcpkg overlay archive has no top-level directory.")
    endif()

    file(REMOVE_RECURSE "${tmp}")
    file(REMOVE "${archive}")
endfunction()

# The overlay from a git repository, for a host that serves no archive of a
# branch. Shallow, and kept shallow: what is wanted is the tip of the default
# branch and none of the history behind it.
function(clone_vcpkg_overlay repo overlay_dir)
    find_package(Git QUIET)

    if(NOT GIT_FOUND)
        message(STATUS "Git is needed to fetch the vcpkg overlay from ${repo}.")
        return()
    endif()

    git_no_prompt(git_cmd)

    if(IS_DIRECTORY "${overlay_dir}/.git")
        # Fetch and reset rather than pull: this is a mirror of a remote branch
        # that nothing here commits to, so a rewritten upstream should be taken
        # as it now stands instead of refusing to fast-forward onto it.
        execute_process(
            COMMAND ${git_cmd} -C "${overlay_dir}" fetch --depth 1 --quiet origin
            RESULT_VARIABLE fetch_error
            ERROR_VARIABLE  fetch_message
        )

        if(NOT fetch_error EQUAL 0)
            string(STRIP "${fetch_message}" fetch_message)
            message(STATUS
                "Could not update the vcpkg overlay (${fetch_message}); "
                "using the checkout at ${overlay_dir}.")
            return()
        endif()

        execute_process(
            COMMAND ${git_cmd} -C "${overlay_dir}" reset --hard --quiet FETCH_HEAD
            RESULT_VARIABLE reset_error
            ERROR_QUIET
        )

        if(reset_error EQUAL 0)
            message(STATUS "Updated the vcpkg overlay from ${repo}.")
        endif()

        return()
    endif()

    file(REMOVE_RECURSE "${overlay_dir}")

    execute_process(
        COMMAND ${git_cmd} clone --depth 1 --quiet "${repo}" "${overlay_dir}"
        RESULT_VARIABLE clone_error
        ERROR_VARIABLE  clone_message
    )

    if(clone_error EQUAL 0)
        message(STATUS "Cloned the vcpkg overlay from ${repo}.")
    else()
        string(STRIP "${clone_message}" clone_message)
        message(STATUS "Could not clone the vcpkg overlay (${clone_message}).")
    endif()
endfunction()

# Point vcpkg at the overlay the binary packages were built from.
#
# The packages on the server are built from a set of ports that is not vcpkg's,
# so a build consuming them has to resolve against the same ports. Against
# vcpkg's own tree every port the overlay carries reads as a different version,
# and the upgrade that follows an incomplete binary install rebuilds it from
# source -- wxWidgets above all, which the overlay holds at a master snapshot no
# vcpkg release matches.
#
# Which ports those are is VCPKG_OVERLAY_PORTS, from the top-level
# CMakeLists.txt: a directory to use as it stands, or a repository to fetch into
# the build directory beside the other tools. Nothing happens without it, and
# nothing happens when the environment already names an overlay -- someone with
# a checkout of it keeps theirs.
function(setup_vcpkg_overlay)
    if(NOT VCPKG_OVERLAY_PORTS)
        return()
    endif()

    if(NOT "$ENV{VCPKG_OVERLAY_PORTS}" STREQUAL "")
        return()
    endif()

    if(IS_DIRECTORY "${VCPKG_OVERLAY_PORTS}")
        set(overlay_dir "${VCPKG_OVERLAY_PORTS}")
    else()
        set(overlay_dir "${CMAKE_BINARY_DIR}/vcpkg-overlay")

        if(VCPKG_OVERLAY_PORTS MATCHES "^https?://github[.]com/")
            fetch_vcpkg_overlay_archive("${VCPKG_OVERLAY_PORTS}" "${overlay_dir}")
        else()
            clone_vcpkg_overlay("${VCPKG_OVERLAY_PORTS}" "${overlay_dir}")
        endif()
    endif()

    # An overlay is directories of portfiles. Anything else -- an extraction
    # that did not happen, a clone that failed, a directory named by mistake --
    # is one vcpkg fails on rather than ignores.
    file(GLOB overlay_portfiles "${overlay_dir}/*/portfile.cmake")

    if(NOT overlay_portfiles)
        if(IS_DIRECTORY "${overlay_dir}")
            message(STATUS
                "No vcpkg overlay ports under ${overlay_dir}; "
                "building against vcpkg's own ports.")
        else()
            message(STATUS "No vcpkg overlay; building against vcpkg's own ports.")
        endif()

        return()
    endif()

    set(ENV{VCPKG_OVERLAY_PORTS} "${overlay_dir}")

    # triplets/community, not triplets: vcpkg gives its own tree that split for
    # free but does not recurse into an overlay's, and community is where the
    # overlay's only triplet, riscv64-android, lives.
    if("$ENV{VCPKG_OVERLAY_TRIPLETS}" STREQUAL ""
            AND IS_DIRECTORY "${overlay_dir}/triplets/community")
        set(ENV{VCPKG_OVERLAY_TRIPLETS} "${overlay_dir}/triplets/community")
    endif()

    message(STATUS "Using the vcpkg overlay at ${overlay_dir}")
endfunction()

function(vcpkg_set_toolchain)
    get_filename_component(preferred_root ${CMAKE_SOURCE_DIR}/../vcpkg ABSOLUTE)

    if(NOT DEFINED VCPKG_BINARY_PACKAGES)
        set(VCPKG_BINARY_PACKAGES TRUE)
    endif()

    if(NOT DEFINED VCPKG_SOURCE_PACKAGES)
        set(VCPKG_SOURCE_PACKAGES TRUE)
    endif()

    if(NOT DEFINED POWERSHELL AND VCPKG_BINARY_PACKAGES)
        message(FATAL_ERROR "Powershell is required to use vcpkg binaries.")
    endif()

    if(NOT DEFINED ENV{VCPKG_ROOT} OR ENV{VCPKG_ROOT} MATCHES "^ *$")
        if(WIN32)
            if(EXISTS /vcpkg)
                set(VCPKG_ROOT /vcpkg)
            elseif(EXISTS c:/vcpkg)
                set(VCPKG_ROOT c:/vcpkg)
            # Prefer the preferred root to the VS default which is more difficult to deal with, if it exists.
            elseif(EXISTS ${preferred_root})
                set(VCPKG_ROOT ${preferred_root})
            else()
                find_program(vcpkg_exe_path NAME vcpkg.exe HINTS ENV PATH)

                if(vcpkg_exe_path)
                    get_filename_component(VCPKG_ROOT ${vcpkg_exe_path} DIRECTORY)
                    get_filename_component(VCPKG_ROOT ${VCPKG_ROOT}     ABSOLUTE)
                endif()

                unset(vcpkg_exe_path)
            endif()
        endif()

        if(NOT DEFINED VCPKG_ROOT)
            set(VCPKG_ROOT ${preferred_root})
        endif()

        set(ENV{VCPKG_ROOT} ${VCPKG_ROOT})
    else()
        set(VCPKG_ROOT $ENV{VCPKG_ROOT})
    endif()

    # Avoid using Visual Studio default vcpkg, because that requires elevaction.
    if(VCPKG_ROOT MATCHES "Visual Studio")
        set(mkdir_status 0)
        if(NOT EXISTS "${preferred_root}")
            file(
                MAKE_DIRECTORY "${preferred_root}"
                RESULT mkdir_status
            )
        endif()

        if(mkdir_status EQUAL 0)
            set(VCPKG_ROOT "${preferred_root}")
            set(ENV{VCPKG_ROOT} ${VCPKG_ROOT})
        endif()
    endif()

    set(VCPKG_ROOT ${VCPKG_ROOT} CACHE FILEPATH "vcpkg installation root path" FORCE)

    if(NO_VCPKG_UPDATES AND EXISTS ${VCPKG_ROOT})
        # Leave the existing checkout exactly as-is: no fetch, no pull, no
        # re-bootstrap. A vcpkg tree with local port modifications makes
        # `git pull --rebase` fail and would otherwise break every
        # reconfigure. git_up_to_date also gates the bootstrap step below.
        set(git_up_to_date TRUE)
    elseif(NOT EXISTS ${VCPKG_ROOT})
        get_filename_component(root_parent ${VCPKG_ROOT}/.. ABSOLUTE)

        execute_process(
            COMMAND git clone https://github.com/microsoft/vcpkg.git
            RESULT_VARIABLE git_status
            WORKING_DIRECTORY ${root_parent}
        )

        vcpkg_check_git_status(${git_status})
    else()
        if(NOT EXISTS ${VCPKG_ROOT}/.git)
            set(git_commands
                "git init"
                "git remote add origin https://github.com/microsoft/vcpkg.git"
                "git fetch --all --prune"
                "git reset --hard origin/master"
                "git branch --set-upstream-to=origin/master master"
            )
            foreach(git_command ${git_commands})
                separate_arguments(git_command)

                execute_process(
                    COMMAND ${git_command}
                    RESULT_VARIABLE git_status
                    WORKING_DIRECTORY ${VCPKG_ROOT}
                )

                vcpkg_check_git_status(${git_status})
            endforeach()
        else()
            execute_process(
                COMMAND git fetch origin
                RESULT_VARIABLE git_status
                WORKING_DIRECTORY ${VCPKG_ROOT}
            )
            vcpkg_check_git_status(${git_status})

            execute_process(
                COMMAND git status
                RESULT_VARIABLE git_status
                OUTPUT_VARIABLE git_status_text
                WORKING_DIRECTORY ${VCPKG_ROOT}
            )
            vcpkg_check_git_status(${git_status})

            set(git_up_to_date FALSE)

            if(git_status_text MATCHES "Your branch is up to date with")
                set(git_up_to_date TRUE)
            endif()

            if(NOT git_up_to_date)
                execute_process(
                    COMMAND git pull --rebase
                    RESULT_VARIABLE git_status
                    WORKING_DIRECTORY ${VCPKG_ROOT}
                )

                vcpkg_check_git_status(${git_status})
            endif()
        endif()
    endif()

    # build latest vcpkg, if needed
    if(NOT git_up_to_date)
        if(WIN32)
            execute_process(
                COMMAND bootstrap-vcpkg.bat
                WORKING_DIRECTORY ${VCPKG_ROOT}
            )
        else()
            execute_process(
                COMMAND ./bootstrap-vcpkg.sh
                WORKING_DIRECTORY ${VCPKG_ROOT}
            )
        endif()
    endif()

    if(WIN32)
        set(VCPKG_PROGRAM_EXECUTABLE "${VCPKG_ROOT}/vcpkg.exe" CACHE FILEPATH "vcpkg executable" FORCE)
    else()
        set(VCPKG_PROGRAM_EXECUTABLE "${VCPKG_ROOT}/vcpkg" CACHE FILEPATH "vcpkg executable" FORCE)
    endif()

    # Report the port set the frontend selection resolved to; which ports are
    # missing from it is the usual thing to check when a dependency turns up
    # unexpectedly absent.
    set(deps_report ${VCPKG_DEPS})

    list(LENGTH VCPKG_DEPS_OPTIONAL optionals_report_len)
    if(optionals_report_len GREATER 0)
        math(EXPR optionals_report_last "${optionals_report_len} - 1")

        foreach(i RANGE 0 ${optionals_report_last} 2)
            list(GET VCPKG_DEPS_OPTIONAL ${i} dep)
            list(APPEND deps_report "${dep}?")
        endforeach()
    endif()

    message(STATUS
        "vcpkg ports for the selected frontends (ENABLE_WX=${ENABLE_WX} "
        "ENABLE_SDL=${ENABLE_SDL} ENABLE_LIBRETRO=${ENABLE_LIBRETRO}): "
        "${deps_report}")

    setup_vcpkg_overlay()

    if (NOT (NO_VCPKG_UPDATES OR (NOT VCPKG_BINARY_PACKAGES)))
        get_binary_packages()
    endif()

    # Whether anything is to be installed from source at all.
    #
    # Beyond the conditions that always governed it there is the case of the
    # binary packages having been out of reach altogether: no listing, nothing
    # in the listing, no tool to install with. Nothing then says which ports are
    # missing, and what used to be made of that was that all of them were --
    # the whole dependency set installed from source and then `vcpkg upgrade`,
    # which rebuilds every port whose version in the port tree differs from the
    # installed one. That is a long way round to the state already in place,
    # which is where a connection to the package listing that timed out would
    # leave a tree that had everything.
    #
    # A tree that has everything is left alone. One that does not still gets
    # the whole set, there being nothing better to go on.
    set(vcpkg_source_install TRUE)

    if(binary_packages_installed OR NO_VCPKG_UPDATES OR (NOT VCPKG_SOURCE_PACKAGES))
        set(vcpkg_source_install FALSE)
    elseif(NOT (DEFINED VCPKG_MISSING_PORTS AND VCPKG_MISSING_PORTS))
        vcpkg_ports_not_installed("${VCPKG_WANTED_PORTS}" "${VCPKG_TARGET_TRIPLET}"
                                  vcpkg_ports_absent)

        if(VCPKG_WANTED_PORTS AND NOT vcpkg_ports_absent)
            message(STATUS
                "Every port for ${VCPKG_TARGET_TRIPLET} is installed and none "
                "can be checked against the server; leaving them as they are.")
            set(vcpkg_source_install FALSE)
        endif()
    endif()

    if(vcpkg_source_install)
        # If get_binary_packages exposed a list of missing ports, install
        # only those from source. Running `vcpkg install ${VCPKG_DEPS}`
        # over the entire dep set would re-resolve every port against
        # the current local port tree and rebuild ports we already
        # binary-installed if their port-tree versions don't match the
        # binary-cache versions (the cascading rebuild). Restricting
        # the install to ports actually missing from the binary cache
        # keeps the others as they are. Same idea for the upgrade step:
        # skip it when we have already-installed binaries we don't want
        # touched.
        if(DEFINED VCPKG_MISSING_PORTS AND VCPKG_MISSING_PORTS)
            message(STATUS "Source-installing missing ports only: ${VCPKG_MISSING_PORTS}")
            set(_install_targets ${VCPKG_MISSING_PORTS})
            set(_skip_upgrade TRUE)
        else()
            set(_install_targets ${VCPKG_DEPS})
            set(_skip_upgrade FALSE)
        endif()

        # Install core deps (or just the missing ones).
        execute_process(
            COMMAND ${VCPKG_PROGRAM_EXECUTABLE} --triplet ${VCPKG_TARGET_TRIPLET} install ${_install_targets} --allow-unsupported --recurse --keep-going
            WORKING_DIRECTORY ${VCPKG_ROOT}
        )

        # Upgrade any outdated ports - but only when we did a full
        # source install. A surgical install of just the missing
        # ports must not be followed by `vcpkg upgrade` because that
        # walks the whole installed set and would rebuild ports we
        # were trying to keep.
        if(NOT _skip_upgrade)
            execute_process(
                COMMAND ${VCPKG_PROGRAM_EXECUTABLE} upgrade --no-dry-run --allow-unsupported --keep-going
                WORKING_DIRECTORY ${VCPKG_ROOT}
            )
        endif()

        # Install optional deps. The list is empty when no frontend that uses
        # any of them was selected, and foreach(RANGE 0 -1 2) is an error.
        list(LENGTH VCPKG_DEPS_OPTIONAL optionals_list_len)

        unset(optional_deps)

        if(optionals_list_len GREATER 0)
            math(EXPR optionals_list_last "${optionals_list_len} - 1")

            foreach(i RANGE 0 ${optionals_list_last} 2)
                list(GET VCPKG_DEPS_OPTIONAL ${i} dep)

                math(EXPR var_idx "${i} + 1")
                list(GET VCPKG_DEPS_OPTIONAL ${var_idx} var)

                if(NOT DEFINED ${var} OR ${var})
                    list(APPEND optional_deps ${dep})
                    set(${var} ON)
                else()
                    set(${var} OFF)
                endif()
            endforeach()
        endif()

        if(optional_deps)
            execute_process(
                COMMAND ${VCPKG_PROGRAM_EXECUTABLE} --triplet ${VCPKG_TARGET_TRIPLET} install ${optional_deps}
                WORKING_DIRECTORY ${VCPKG_ROOT}
            )
        endif()
    endif()

    if(WIN32 AND VCPKG_TARGET_TRIPLET MATCHES x64 AND CMAKE_GENERATOR MATCHES "Visual Studio")
        set(CMAKE_GENERATOR_PLATFORM x64 CACHE STRING "visual studio build architecture" FORCE)
    endif()

    if(WIN32 AND NOT CMAKE_GENERATOR MATCHES "Visual Studio" AND NOT DEFINED CMAKE_CXX_COMPILER)
        if(VCPKG_TARGET_TRIPLET MATCHES "-windows-")
            # set toolchain to VS for e.g. Ninja or jom
            set(CMAKE_C_COMPILER   cl CACHE STRING "Microsoft C/C++ Compiler" FORCE)
            set(CMAKE_CXX_COMPILER cl CACHE STRING "Microsoft C/C++ Compiler" FORCE)
        endif()
    endif()

    # Keep a caller-supplied toolchain (e.g. Qt's qt.toolchain.cmake, which
    # chainloads the Android NDK toolchain) as the project toolchain. vcpkg.cmake
    # is included directly at the bottom of this file either way, so vcpkg
    # integration is not lost by leaving CMAKE_TOOLCHAIN_FILE alone.
    if(VBAM_OUTER_TOOLCHAIN_FILE)
        set(CMAKE_TOOLCHAIN_FILE "${VBAM_OUTER_TOOLCHAIN_FILE}"                        CACHE FILEPATH  "toolchain"             FORCE)
    else()
        set(CMAKE_TOOLCHAIN_FILE "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"       CACHE FILEPATH  "vcpkg toolchain"       FORCE)
    endif()

    set(CMAKE_PREFIX_PATH       "${VCPKG_ROOT}/installed/${VCPKG_TARGET_TRIPLET}/"      CACHE STRING    "vcpkg prefix path"     FORCE)

    # These may be set in an MSYS2 environment and interfere with finding packages.
    unset(ENV{PKG_CONFIG_PATH})
    unset(ENV{PKG_CONFIG_SYSTEM_LIBRARY_PATH})
    unset(ENV{PKG_CONFIG_SYSTEM_INCLUDE_PATH})
endfunction()

vcpkg_set_toolchain()

include(${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake)
