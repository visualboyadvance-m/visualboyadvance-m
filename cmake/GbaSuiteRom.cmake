# GbaSuiteRom.cmake -- provision the mGBA accuracy-test ROM (mgba-emu/suite ->
# suite.gba) for the gba.suite test, using a fully self-contained, build-local
# devkitPro. No system install, no sudo/admin, no pacman:
#
#   * configure time: download the devkitPro GBA toolchain packages directly
#     from pkg.devkitpro.org and extract them into the build tree
#     (<build>/devkitpro), resolving exact package file names from the repo
#     database so it keeps working as upstream bumps versions.
#   * build time:      compile the latest mgba-emu/suite checkout with that
#     toolchain via a normal add_custom_command, dropping suite.gba into the
#     build tree.
#
# devkitPro publishes host toolchains for Linux, macOS and Windows only. On any
# other host (e.g. FreeBSD, which has no devkitPro packages) provisioning is
# skipped and the caller simply does not register the GBA suite test.
#
# Public entry point:
#   vbam_provision_mgba_suite(<out-rom-var>)
#     On success sets <out-rom-var> (PARENT_SCOPE) to the suite.gba path that
#     will exist after the build, and creates an ALL target `mgba_suite_rom`
#     that builds it. On any failure it sets <out-rom-var> to "" and logs a
#     STATUS line explaining why the test will be skipped.

# Captured at include time -- CMAKE_CURRENT_LIST_DIR is not reliable inside a
# function body (it reflects the file being processed when the function runs).
set(_GBA_SUITE_ROM_MODULE_DIR "${CMAKE_CURRENT_LIST_DIR}")

# Arch-independent library repo. Host toolchain binaries live in a per-host,
# per-arch subtree of this same base.
set(_DKP_BASE_URL "https://pkg.devkitpro.org/packages")

# pkg.devkitpro.org sits behind Cloudflare, which 403s the default libcurl
# User-Agent that file(DOWNLOAD) sends. Present the pacman UA (what a normal
# `pacman -Sy` sends) so the fetch is allowed.
set(_DKP_HTTP_UA "pacman/6.1.0 libalpm/14.0.0")

# Packages seeding the dependency closure. Their transitive (in-repo) depends
# pull in everything needed to compile+link a GBA ROM, run gbafix, and build
# the suite's grit graphics. No versions here -- file names come from the DB.
set(_DKP_SEED_PACKAGES
    devkitarm-gcc devkitarm-binutils devkitarm-newlib devkitarm-crtls
    devkitarm-rules general-tools gba-tools grit libgba dkp-toolchain-vars)

# Map the *host* (devkitPro tools run on the build machine, even when cross
# compiling VBA-M) to the devkitPro repo os/arch labels. Empty os => no
# packages for this host.
function(_dkp_host_repo out_os out_arch)
    set(_os "")
    set(_arch "")
    if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
        set(_os linux)
        if(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
            set(_arch aarch64)
        else()
            set(_arch x86_64)
        endif()
    elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
        set(_os osx)
        # devkitPro labels Apple silicon "arm64" (not "aarch64").
        if(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
            set(_arch arm64)
        else()
            set(_arch x86_64)
        endif()
    elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
        set(_os windows)
        set(_arch x86_64)
    endif()
    set(${out_os} "${_os}" PARENT_SCOPE)
    set(${out_arch} "${_arch}" PARENT_SCOPE)
endfunction()

# Parse a pacman `desc` entry into name, package file name, and bare dependency
# names (version constraints like ">=1.2" stripped).
function(_dkp_parse_desc desc_text out_name out_filename out_depends)
    string(REGEX REPLACE "\r" "" _t "${desc_text}")
    string(REPLACE ";" "\\;" _t "${_t}")
    string(REPLACE "\n" ";" _lines "${_t}")
    set(_field "")
    set(_name "")
    set(_filename "")
    set(_depends "")
    foreach(_ln IN LISTS _lines)
        if(_ln MATCHES "^%([A-Z0-9]+)%$")
            set(_field "${CMAKE_MATCH_1}")
        elseif(_ln STREQUAL "")
            set(_field "")
        elseif(_field STREQUAL "NAME")
            set(_name "${_ln}")
        elseif(_field STREQUAL "FILENAME")
            set(_filename "${_ln}")
        elseif(_field STREQUAL "DEPENDS")
            string(REGEX REPLACE "^([^<>=: ]+).*$" "\\1" _dep "${_ln}")
            list(APPEND _depends "${_dep}")
        endif()
    endforeach()
    set(${out_name} "${_name}" PARENT_SCOPE)
    set(${out_filename} "${_filename}" PARENT_SCOPE)
    set(${out_depends} "${_depends}" PARENT_SCOPE)
endfunction()

# Download a pacman .db (a gzip tarball) and extract it into destdir.
function(_dkp_fetch_db url destdir out_ok)
    set(${out_ok} FALSE PARENT_SCOPE)
    set(_tmp "${destdir}/pacman.db")
    file(DOWNLOAD "${url}" "${_tmp}" STATUS _st
         HTTPHEADER "User-Agent: ${_DKP_HTTP_UA}")
    list(GET _st 0 _code)
    if(NOT _code EQUAL 0)
        message(STATUS "vbam-core test: cannot download ${url} (${_st})")
        return()
    endif()
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E tar xf "${_tmp}"
        WORKING_DIRECTORY "${destdir}"
        RESULT_VARIABLE _rc)
    if(_rc EQUAL 0)
        set(${out_ok} TRUE PARENT_SCOPE)
    endif()
endfunction()

function(vbam_provision_mgba_suite out_rom)
    set(${out_rom} "" PARENT_SCOPE)

    _dkp_host_repo(_os _arch)
    if(NOT _os)
        message(STATUS "vbam-core test: no devkitPro packages for host "
            "'${CMAKE_HOST_SYSTEM_NAME}'; gba.suite skipped")
        return()
    endif()

    if(CMAKE_HOST_WIN32)
        set(_exe ".exe")
    else()
        set(_exe "")
    endif()

    set(_dkp_root "${CMAKE_BINARY_DIR}/devkitpro")
    set(_devkitpro "${_dkp_root}/opt/devkitpro")
    set(_devkitarm "${_devkitpro}/devkitARM")
    set(_gcc "${_devkitarm}/bin/arm-none-eabi-gcc${_exe}")

    # ---- configure-time: fetch + extract the toolchain into the build tree ---
    if(NOT EXISTS "${_gcc}")
        set(_libs_url "${_DKP_BASE_URL}")
        set(_host_url "${_DKP_BASE_URL}/${_os}/${_arch}")

        set(_dbwork "${CMAKE_BINARY_DIR}/devkitpro-db")
        file(REMOVE_RECURSE "${_dbwork}")
        file(MAKE_DIRECTORY "${_dbwork}/libs" "${_dbwork}/host")
        _dkp_fetch_db("${_libs_url}/dkp-libs.db" "${_dbwork}/libs" _ok_libs)
        _dkp_fetch_db("${_host_url}/dkp-${_os}.db" "${_dbwork}/host" _ok_host)
        if(NOT _ok_libs OR NOT _ok_host)
            message(STATUS "vbam-core test: could not fetch devkitPro package "
                "databases; gba.suite skipped")
            return()
        endif()

        # Build name -> (file, url, deps) maps from both repos' desc entries.
        file(GLOB _descs "${_dbwork}/libs/*/desc" "${_dbwork}/host/*/desc")
        foreach(_d IN LISTS _descs)
            file(READ "${_d}" _txt)
            _dkp_parse_desc("${_txt}" _n _f _deps)
            if(_n STREQUAL "")
                continue()
            endif()
            if(_d MATCHES "/libs/")
                set(_pkg_${_n}_url "${_libs_url}")
            else()
                set(_pkg_${_n}_url "${_host_url}")
            endif()
            set(_pkg_${_n}_file "${_f}")
            set(_pkg_${_n}_deps "${_deps}")
        endforeach()

        # Transitive dependency closure over packages present in our repos;
        # deps we don't publish (system libs) are simply ignored.
        set(_want ${_DKP_SEED_PACKAGES})
        set(_resolved "")
        while(_want)
            list(POP_FRONT _want _cur)
            if(_cur IN_LIST _resolved)
                continue()
            endif()
            if(NOT DEFINED _pkg_${_cur}_file)
                continue()
            endif()
            list(APPEND _resolved "${_cur}")
            foreach(_dep IN LISTS _pkg_${_cur}_deps)
                if(NOT _dep IN_LIST _resolved)
                    list(APPEND _want "${_dep}")
                endif()
            endforeach()
        endwhile()

        if(NOT _resolved)
            message(STATUS "vbam-core test: no devkitPro GBA packages resolved; "
                "gba.suite skipped")
            return()
        endif()

        # Download + extract each package. Packages are laid out as
        # opt/devkitpro/... so extracting into _dkp_root yields _devkitpro.
        set(_cache "${CMAKE_BINARY_DIR}/devkitpro-pkgs")
        file(MAKE_DIRECTORY "${_cache}" "${_dkp_root}")
        message(STATUS "vbam-core test: fetching devkitPro GBA toolchain "
            "(${_os}/${_arch}, ${_resolved})")
        foreach(_p IN LISTS _resolved)
            set(_fn "${_pkg_${_p}_file}")
            set(_dl "${_cache}/${_fn}")
            if(NOT EXISTS "${_dl}")
                file(DOWNLOAD "${_pkg_${_p}_url}/${_fn}" "${_dl}" STATUS _st
                     HTTPHEADER "User-Agent: ${_DKP_HTTP_UA}")
                list(GET _st 0 _code)
                if(NOT _code EQUAL 0)
                    message(STATUS "vbam-core test: failed to download ${_fn} "
                        "(${_st}); gba.suite skipped")
                    return()
                endif()
            endif()
            # .pkg.tar.zst and .pkg.tar.xz both occur; `cmake -E tar` autodetects
            # via libarchive. A failure here usually means libarchive lacks zstd.
            execute_process(
                COMMAND "${CMAKE_COMMAND}" -E tar xf "${_dl}"
                WORKING_DIRECTORY "${_dkp_root}"
                RESULT_VARIABLE _xrc)
            if(NOT _xrc EQUAL 0)
                message(STATUS "vbam-core test: failed to extract ${_fn} "
                    "(libarchive may lack zstd/xz support); gba.suite skipped")
                return()
            endif()
        endforeach()
    endif()

    if(NOT EXISTS "${_gcc}")
        message(STATUS "vbam-core test: devkitARM gcc missing after "
            "provisioning; gba.suite skipped")
        return()
    endif()

    # Smoke-test the extracted compiler so a broken toolchain fails here (test
    # skipped) rather than breaking the build later.
    execute_process(
        COMMAND "${_gcc}" -dumpversion
        RESULT_VARIABLE _grc OUTPUT_QUIET ERROR_QUIET)
    if(NOT _grc EQUAL 0)
        message(STATUS "vbam-core test: devkitARM gcc is not runnable on this "
            "host; gba.suite skipped")
        return()
    endif()

    # GNU make drives the suite's devkitPro template Makefile.
    find_program(_DKP_MAKE NAMES make gmake mingw32-make)
    if(NOT _DKP_MAKE)
        message(STATUS "vbam-core test: GNU make not found; cannot build "
            "suite.gba; gba.suite skipped")
        return()
    endif()

    # ---- latest suite checkout -------------------------------------------
    include(FetchContent)
    FetchContent_Declare(mgba_suite
        GIT_REPOSITORY https://github.com/mgba-emu/suite.git
        GIT_TAG        master
        SOURCE_DIR     "${CMAKE_BINARY_DIR}/mgba-suite")
    FetchContent_MakeAvailable(mgba_suite)

    # ---- build-time: compile the ROM as a normal build artifact ----------
    set(_rom "${CMAKE_CURRENT_BINARY_DIR}/suite.gba")
    add_custom_command(
        OUTPUT "${_rom}"
        COMMAND "${CMAKE_COMMAND}"
            -D "DEVKITPRO=${_devkitpro}"
            -D "DEVKITARM=${_devkitarm}"
            -D "MAKE=${_DKP_MAKE}"
            -D "SUITE_SRC=${mgba_suite_SOURCE_DIR}"
            -D "OUT_DIR=${CMAKE_CURRENT_BINARY_DIR}"
            -P "${_GBA_SUITE_ROM_MODULE_DIR}/RunMgbaSuiteMake.cmake"
        DEPENDS "${mgba_suite_SOURCE_DIR}/Makefile"
        COMMENT "Building mGBA suite.gba with build-local devkitPro"
        VERBATIM)
    add_custom_target(mgba_suite_rom ALL DEPENDS "${_rom}")

    set(${out_rom} "${_rom}" PARENT_SCOPE)
endfunction()
