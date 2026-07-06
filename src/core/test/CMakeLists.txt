# This defines the `vbam-core-fake` library, which is used for providing a fake
# implementation of the core library for testing purposes.

if(NOT BUILD_TESTING)
    return()
endif()

add_library(vbam-core-fake OBJECT)

target_sources(vbam-core-fake
    PRIVATE
    fake_core.cpp
)

# suite_runner: headless driver for mGBA's suite.gba accuracy tests.
add_executable(suite_runner suite_runner.cpp)
target_link_libraries(suite_runner PRIVATE vbam-core)
if(APPLE)
    # Link iconv by its resolved path (the SDK .tbd) rather than a bare
    # `-liconv`. Non-Apple toolchains (e.g. a Nix/Homebrew Clang) do not
    # implicitly search the macOS SDK's usr/lib for `-l` flags.
    if(NOT LIBICONV_LIB)
        find_library(LIBICONV_LIB NAMES libiconv iconv)
    endif()
    target_link_libraries(suite_runner PRIVATE
        "-framework CoreFoundation" ${LIBICONV_LIB})
endif()

# gb_suite_runner: headless driver for Blargg's Game Boy / Game Boy Color
# test ROMs. Captures pass/fail via the GB serial port (SB/SC).
add_executable(gb_suite_runner gb_suite_runner.cpp)
target_link_libraries(gb_suite_runner PRIVATE vbam-core)
if(APPLE)
    if(NOT LIBICONV_LIB)
        find_library(LIBICONV_LIB NAMES libiconv iconv)
    endif()
    target_link_libraries(gb_suite_runner PRIVATE
        "-framework CoreFoundation" ${LIBICONV_LIB})
endif()

# The test runners must inherit the global LTO setting rather than force it
# off: they link vbam-core, whose archive holds LLVM bitcode when LTO is on.
# Linking a bitcode archive into a non-LTO executable makes the clang driver
# skip the LTO plugin, so GNU ld (bfd) rejects it with "file format not
# recognized". Keeping them consistent with the library avoids the mismatch
# (and with lld/gold the plugin would run at link time anyway, so forcing IPO
# off here saved nothing).

# -----------------------------------------------------------------------------
# Test ROM integration
#
# The runners need third-party ROM blobs that we do *not* bundle in the
# repo for licensing reasons. There are two ways to enable the CTest
# tests:
#
# 1. Auto-fetch from upstream (recommended for CI):
#
#       cmake -DVBAM_FETCH_TEST_ROMS=ON ...
#
#    fetches Blargg's gb-test-roms (`github.com/retrio/gb-test-roms`)
#    and mGBA's suite.gba (`github.com/mgba-emu/suite`) into the build
#    tree at configure time and points the per-suite path variables at
#    them automatically.
#
# 2. Point at a pre-installed local copy:
#
#       VBAM_TEST_GB_ROMS    — directory containing Blargg's gb-test-roms
#                              (https://github.com/retrio/gb-test-roms).
#                              Run with gb_suite_runner.
#
#       VBAM_TEST_GBA_ROM    — full path to mGBA's suite.gba accuracy ROM
#                              (https://github.com/mgba-emu/suite). Run
#                              with suite_runner.
#
#       VBAM_TEST_GBA_BIOS   — optional GBA BIOS for suite_runner. If
#                              unset, `--hle` is used instead.
#
#       VBAM_TEST_GB_CGB_BIOS — optional CGB boot ROM for gb_suite_runner.
#       VBAM_TEST_GB_DMG_BIOS — optional DMG boot ROM for gb_suite_runner.
#
# Each test is registered only if its required ROM(s) exist. If they
# don't, the test is logged as skipped at configure time and CTest
# reports nothing for it.
# -----------------------------------------------------------------------------

option(VBAM_FETCH_TEST_ROMS
       "Download GB / GBA accuracy test ROMs from their upstream repos \
at configure time and point the test infrastructure at them"
       OFF)

option(VBAM_INSTALL_DEVKITPRO
       "If devkitPro isn't already installed, run the upstream \
dkp-pacman bootstrap (with sudo) and pull in `gba-dev` so mGBA's \
suite.gba can be built from source. Only effective when \
VBAM_FETCH_TEST_ROMS is also enabled."
       OFF)

if(VBAM_FETCH_TEST_ROMS)
    set(_vbam_roms_dir "${CMAKE_BINARY_DIR}/_test_roms")
    file(MAKE_DIRECTORY "${_vbam_roms_dir}")

    # Blargg's gb-test-roms (retrio mirror — official Blargg site is dead).
    # We use FetchContent to clone the repo so a single git index update
    # picks up new test additions; the populated source dir holds the
    # ROMs directly (no build needed).
    include(FetchContent)
    FetchContent_Declare(
        vbam_blargg_gb_test_roms
        GIT_REPOSITORY https://github.com/retrio/gb-test-roms.git
        GIT_TAG        master
        SOURCE_DIR     "${_vbam_roms_dir}/gb-test-roms"
    )
    FetchContent_MakeAvailable(vbam_blargg_gb_test_roms)
    if(NOT DEFINED VBAM_TEST_GB_ROMS)
        set(VBAM_TEST_GB_ROMS "${_vbam_roms_dir}/gb-test-roms" CACHE PATH
            "Auto-fetched Blargg gb-test-roms")
    endif()

    # mGBA's suite — source repo only; the suite.gba binary has to be
    # built from C with devkitPro arm-none-eabi-gcc + libgba. We clone
    # the upstream repo regardless, then probe several sensible places
    # for a prebuilt suite.gba. CI workflows that want to run the GBA
    # test should either:
    #   (a) install devkitPro and run `make -C _test_roms/mgba-suite` so
    #       the build artifact lands at <SOURCE_DIR>/suite.gba, OR
    #   (b) supply a pre-built suite.gba via -DVBAM_TEST_GBA_ROM=...
    FetchContent_Declare(
        vbam_mgba_suite
        GIT_REPOSITORY https://github.com/mgba-emu/suite.git
        GIT_TAG        master
        SOURCE_DIR     "${_vbam_roms_dir}/mgba-suite"
    )
    FetchContent_MakeAvailable(vbam_mgba_suite)

    set(_vbam_suite_gba "")
    foreach(_cand
            # Vendored copy in the source tree takes priority over any
            # rebuild attempt. Cycle-sensitive sub-tests (hblankBit,
            # dmaPrefetch::Break) are tuned to a specific compiler
            # output, so a fresh devkitARM build with newer GCC will
            # drift expected vs measured by 1-2 cycles per polling
            # iteration. Vendoring keeps the test deterministic across
            # hosts and CI runners. Delete suite.gba/suite.elf and
            # invoke the Docker/devkitARM rebuild path below to
            # regenerate when upstream test source changes.
            "${CMAKE_CURRENT_SOURCE_DIR}/suite.gba"
            "${_vbam_roms_dir}/mgba-suite/suite.gba"
            "${_vbam_roms_dir}/mgba-suite/build/suite.gba"
            "${_vbam_roms_dir}/suite.gba")
        if(EXISTS "${_cand}")
            set(_vbam_suite_gba "${_cand}")
            break()
        endif()
    endforeach()

    # Probe for a usable devkitPro install. The user can pre-set
    # DEVKITPRO/DEVKITARM env vars; if not, we fall back to the
    # standard /opt/devkitpro layout that dkp-pacman uses.
    set(_vbam_dkp "")
    if(DEFINED ENV{DEVKITPRO} AND IS_DIRECTORY "$ENV{DEVKITPRO}")
        set(_vbam_dkp "$ENV{DEVKITPRO}")
    elseif(IS_DIRECTORY "/opt/devkitpro/devkitARM")
        set(_vbam_dkp "/opt/devkitpro")
    endif()

    # If no devkitPro, try to install it. Linux has a pacman bootstrap
    # script (apt.devkitpro.org); macOS has a .pkg installer. Both
    # require sudo, so we ask the user to opt in via -DVBAM_INSTALL_DEVKITPRO.
    if(NOT _vbam_dkp AND VBAM_INSTALL_DEVKITPRO)
        if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
            message(STATUS "vbam-core test: installing devkitPro (Linux)…")
            execute_process(
                COMMAND bash -c "
                    set -e
                    if ! command -v sudo >/dev/null 2>&1; then SUDO=; else SUDO=sudo; fi
                    tmp=$(mktemp)
                    curl -sSL -o \$tmp https://apt.devkitpro.org/install-devkitpro-pacman
                    chmod +x \$tmp
                    \$SUDO \$tmp
                    \$SUDO /opt/devkitpro/pacman/bin/dkp-pacman -S --noconfirm gba-dev
                "
                RESULT_VARIABLE _vbam_dkp_install_rc
                OUTPUT_QUIET ERROR_QUIET)
            if(_vbam_dkp_install_rc EQUAL 0 AND IS_DIRECTORY "/opt/devkitpro/devkitARM")
                set(_vbam_dkp "/opt/devkitpro")
            endif()
        elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
            message(STATUS "vbam-core test: installing devkitPro (macOS)…")
            execute_process(
                COMMAND bash -c "
                    set -e
                    pkg=$(mktemp -t dkp).pkg
                    curl -sSL -o \$pkg https://github.com/devkitPro/pacman/releases/latest/download/devkitpro-pacman.pkg \
                      || curl -sSL -o \$pkg https://apt.devkitpro.org/install-devkitpro-pacman.pkg
                    sudo installer -pkg \$pkg -target /
                    sudo /opt/devkitpro/pacman/bin/dkp-pacman -Sy --noconfirm gba-dev
                "
                RESULT_VARIABLE _vbam_dkp_install_rc
                OUTPUT_QUIET ERROR_QUIET)
            if(_vbam_dkp_install_rc EQUAL 0 AND IS_DIRECTORY "/opt/devkitpro/devkitARM")
                set(_vbam_dkp "/opt/devkitpro")
            endif()
        endif()
        if(_vbam_dkp)
            message(STATUS "vbam-core test: devkitPro installed at ${_vbam_dkp}")
        else()
            message(WARNING "vbam-core test: devkitPro auto-install failed")
        endif()
    endif()

    # First try a one-shot build via local devkitPro (`make` with
    # DEVKITARM/DEVKITPRO env vars — same toolchain the upstream
    # Makefile expects).
    if(NOT _vbam_suite_gba AND _vbam_dkp)
        find_program(_vbam_devkitarm_make NAMES make
            HINTS "${_vbam_dkp}/devkitARM/bin" "${_vbam_dkp}/tools/bin")
        if(NOT _vbam_devkitarm_make)
            find_program(_vbam_devkitarm_make NAMES make)
        endif()
        if(_vbam_devkitarm_make)
            message(STATUS "vbam-core test: building mGBA suite.gba via devkitPro…")
            execute_process(
                COMMAND ${CMAKE_COMMAND} -E env
                        "DEVKITPRO=${_vbam_dkp}"
                        "DEVKITARM=${_vbam_dkp}/devkitARM"
                        "PATH=${_vbam_dkp}/devkitARM/bin:${_vbam_dkp}/tools/bin:$ENV{PATH}"
                        ${_vbam_devkitarm_make}
                WORKING_DIRECTORY "${_vbam_roms_dir}/mgba-suite"
                RESULT_VARIABLE _vbam_suite_build_rc
                OUTPUT_QUIET ERROR_QUIET)
            if(_vbam_suite_build_rc EQUAL 0 AND
               EXISTS "${_vbam_roms_dir}/mgba-suite/suite.gba")
                set(_vbam_suite_gba "${_vbam_roms_dir}/mgba-suite/suite.gba")
            endif()
        endif()
    endif()

    # Fall back to the official Docker build (mgba-emu/suite ships a
    # `docker-build.sh` that runs `devkitpro/devkitarm make` in a
    # container). This is what the upstream maintainer uses to publish
    # suite.gba and what GitHub-Actions runners have available out of
    # the box, so it's the right CI path.
    if(NOT _vbam_suite_gba)
        find_program(_vbam_docker NAMES docker)
        if(_vbam_docker)
            message(STATUS
                "vbam-core test: building mGBA suite.gba via Docker (devkitpro/devkitarm)…")
            execute_process(
                COMMAND ${_vbam_docker} run --rm
                        -v "${_vbam_roms_dir}/mgba-suite:/root/suite"
                        -w /root/suite devkitpro/devkitarm make
                WORKING_DIRECTORY "${_vbam_roms_dir}/mgba-suite"
                RESULT_VARIABLE _vbam_suite_dock_rc
                OUTPUT_QUIET ERROR_QUIET)
            if(_vbam_suite_dock_rc EQUAL 0 AND
               EXISTS "${_vbam_roms_dir}/mgba-suite/suite.gba")
                set(_vbam_suite_gba "${_vbam_roms_dir}/mgba-suite/suite.gba")
            endif()
        endif()
    endif()

    if(_vbam_suite_gba AND NOT DEFINED VBAM_TEST_GBA_ROM)
        set(VBAM_TEST_GBA_ROM "${_vbam_suite_gba}" CACHE FILEPATH
            "Auto-fetched mGBA suite.gba")

        # Extract the per-suite TestSuite struct addresses from the
        # accompanying suite.elf via arm-none-eabi-objdump so the
        # runner doesn't depend on a specific upstream commit /
        # toolchain version. We emit a header that suite_runner.cpp
        # picks up via __has_include — falling back to its hardcoded
        # table if the header is absent.
        get_filename_component(_vbam_suite_dir "${_vbam_suite_gba}" DIRECTORY)
        set(_vbam_suite_elf "${_vbam_suite_dir}/suite.elf")
        if(EXISTS "${_vbam_suite_elf}")
            find_program(_vbam_arm_objdump
                NAMES arm-none-eabi-objdump
                HINTS "${_vbam_dkp}/devkitARM/bin")
            if(_vbam_arm_objdump)
                execute_process(
                    COMMAND ${_vbam_arm_objdump} -t "${_vbam_suite_elf}"
                    OUTPUT_VARIABLE _vbam_objdump_out
                    ERROR_QUIET)
                # objdump rows look like:
                #   0802cc84 g     O .rodata  0000001c biosMathTestSuite
                # Build a list of (name, addr) pairs for the 14 suites.
                set(_vbam_suite_pairs "")
                # Order MUST match the suites[] declaration in
                # mgba-suite's main.c (the runner navigates the menu by
                # pressing DOWN, so a mismatched order means the runner
                # picks the wrong suite at each row).
                set(_vbam_known_syms
                    memory   memory
                    ioRead   io-read
                    timing   timing
                    timers   timers
                    timerIRQ timer-irq
                    shifter  shifter
                    carry    carry
                    multiplyLong multiply-long
                    biosMath bios-math
                    dma      dma
                    sioRead  sio-read
                    sioTiming sio-timing
                    miscEdge misc-edge
                    video    video)
                # Escape ';' so CMake's list-vs-string conversion
                # doesn't eat them when we join the header pieces.
                set(_vbam_HEADER_PRELUDE
                    "// Generated by src/core/test/CMakeLists.txt — do not edit.\n// Maps suite name → TestSuite struct address in suite.elf,\n// extracted via arm-none-eabi-objdump for the freshly-built\n// suite.gba so suite_runner doesn't drift against upstream.\n#pragma once\n#define VBAM_SUITE_ADDRESSES_AUTO 1\nstatic const struct { const char* name@@@ uint32_t addr@@@ } kSuitesAuto[] = {\n")
                set(_vbam_header "${_vbam_HEADER_PRELUDE}")
                # Split objdump's output into individual lines so the
                # CMake regex engine — which treats `.` as
                # newline-matching by default — has a clean per-line
                # scope to anchor on.
                string(REPLACE ";" "\\;" _vbam_objdump_lines "${_vbam_objdump_out}")
                string(REPLACE "\n" ";" _vbam_objdump_lines "${_vbam_objdump_lines}")
                set(_vbam_count 0)
                while(_vbam_known_syms)
                    list(POP_FRONT _vbam_known_syms _vbam_sym _vbam_lbl)
                    set(_vbam_addr "")
                    foreach(_vbam_line IN LISTS _vbam_objdump_lines)
                        if(_vbam_line MATCHES
                            "^([0-9a-fA-F]+)[ \t].*[ \t]${_vbam_sym}TestSuite$")
                            set(_vbam_addr "0x${CMAKE_MATCH_1}")
                            break()
                        endif()
                    endforeach()
                    if(_vbam_addr)
                        list(APPEND _vbam_header
                            "    { \"${_vbam_lbl}\", ${_vbam_addr} },\n")
                        math(EXPR _vbam_count "${_vbam_count} + 1")
                    endif()
                endwhile()
                list(APPEND _vbam_header "}@@@\n")
                if(_vbam_count GREATER_EQUAL 10)
                    string(JOIN "" _vbam_header_str ${_vbam_header})
                    # Restore the @@@ placeholders we used to smuggle
                    # ';' through CMake's list semantics back to real
                    # semicolons in the emitted C source.
                    string(REPLACE "@@@" ";" _vbam_header_str
                        "${_vbam_header_str}")
                    set(_vbam_addr_h "${_vbam_roms_dir}/suite_addresses.h")
                    file(WRITE "${_vbam_addr_h}" "${_vbam_header_str}")
                    target_include_directories(suite_runner PRIVATE
                        "${_vbam_roms_dir}")
                    target_compile_definitions(suite_runner PRIVATE
                        VBAM_HAS_SUITE_ADDRESSES=1)
                    message(STATUS
                        "vbam-core test: extracted ${_vbam_count} TestSuite \
addresses from suite.elf — runner will use the matched build")
                else()
                    message(STATUS
                        "vbam-core test: could only extract ${_vbam_count}/14 \
suite addresses; runner will fall back to its hardcoded table")
                endif()
            endif()
        endif()
    elseif(NOT _vbam_suite_gba)
        message(STATUS
            "vbam-core test: cloned mgba-emu/suite but could not locate \
or build suite.gba — install devkitPro + libgba (sets \$DEVKITARM) \
and reconfigure, or pass -DVBAM_TEST_GBA_ROM=<path> to use a prebuilt \
copy. gba.suite will be skipped this run.")
    endif()
endif()

# Resolve a path from CMake cache → env var → empty.
function(_vbam_resolve_test_path var_name env_name doc)
    if(NOT DEFINED ${var_name})
        set(_v "$ENV{${env_name}}")
        if(NOT "${_v}" STREQUAL "")
            set(${var_name} "${_v}" CACHE PATH "${doc}")
        endif()
    endif()
endfunction()

_vbam_resolve_test_path(VBAM_TEST_GB_ROMS    VBAM_TEST_GB_ROMS
    "Directory holding Blargg gb-test-roms for the gb_suite_runner test")
_vbam_resolve_test_path(VBAM_TEST_GBA_ROM    VBAM_TEST_GBA_ROM
    "Path to mGBA suite.gba for the suite_runner test")
_vbam_resolve_test_path(VBAM_TEST_GBA_BIOS   VBAM_TEST_GBA_BIOS
    "Optional GBA BIOS for suite_runner (otherwise HLE)")
_vbam_resolve_test_path(VBAM_TEST_GB_CGB_BIOS VBAM_TEST_GB_CGB_BIOS
    "Optional CGB boot ROM (2304 bytes) for gb_suite_runner")
_vbam_resolve_test_path(VBAM_TEST_GB_DMG_BIOS VBAM_TEST_GB_DMG_BIOS
    "Optional DMG boot ROM (256 bytes) for gb_suite_runner")

# Pass-count baselines for CI. The runners exit 0 if the actual PASS
# count is >= the floor below, else exit 2 (regression). These floors
# are the empirically-stable PASS counts in the current tree; bump
# them in lockstep with any improvement so a regression actually fails
# CI. They can be overridden with -DVBAM_TEST_GBA_MIN_PASS=… etc.
if(NOT DEFINED VBAM_TEST_GBA_MIN_PASS)
    # 6760 of 7017 after the timer prescaler-phase / IRQ-latency-split /
    # GamePak-prefetch work (see the test-suite-accuracy commits).
    # Remaining known-failing groups: timer count-up value freezes and
    # part of the sums (~219), Timing C-loop/CpuSet (32), misc-edge
    # DMA-prefetch + H-blank flips (8), one period-1 Timer IRQ boundary.
    set(VBAM_TEST_GBA_MIN_PASS 6760 CACHE STRING
        "Floor PASS count for the GBA accuracy suite")
endif()
if(NOT DEFINED VBAM_TEST_GB_MIN_PASS)
    # 57 of 58: everything passes except oam_bug 7-timing_effect, whose
    # single-ROM print buffer is unbounded and self-corrupts under
    # correct OAM-bug emulation (excluded upstream as a broken ROM; its
    # test-7 logic passes inside the combined oam_bug.gb).
    set(VBAM_TEST_GB_MIN_PASS 57 CACHE STRING
        "Floor PASS count for the Blargg gb-test-roms suite")
endif()

# GBA accuracy suite (mGBA suite.gba)
if(VBAM_TEST_GBA_ROM AND EXISTS "${VBAM_TEST_GBA_ROM}")
    set(_gba_args "--min-pass" "${VBAM_TEST_GBA_MIN_PASS}")
    if(VBAM_TEST_GBA_BIOS AND EXISTS "${VBAM_TEST_GBA_BIOS}")
        list(APPEND _gba_args "--bios" "${VBAM_TEST_GBA_BIOS}")
    else()
        list(APPEND _gba_args "--hle")
    endif()
    list(APPEND _gba_args "${VBAM_TEST_GBA_ROM}")
    add_test(NAME gba.suite
             COMMAND suite_runner ${_gba_args})
    # 1800s (30 min) accommodates Ubuntu/clang/debug CI runners with
    # sanitizers enabled — the suite's ~8M memory accesses make it
    # significantly slower under ASAN/UBSAN. Mac/clang/debug runs in
    # under 5s, so this is purely a CI safety margin.
    set_tests_properties(gba.suite PROPERTIES
        LABELS "gba;accuracy"
        TIMEOUT 1800)
    message(STATUS "vbam-core test: gba.suite enabled "
        "(rom=${VBAM_TEST_GBA_ROM} floor=${VBAM_TEST_GBA_MIN_PASS})")
else()
    message(STATUS
        "vbam-core test: gba.suite SKIPPED — set VBAM_TEST_GBA_ROM to enable")
endif()

# GB / GBC / SGB suite (Blargg's gb-test-roms)
if(VBAM_TEST_GB_ROMS AND IS_DIRECTORY "${VBAM_TEST_GB_ROMS}")
    set(_gb_args "--min-pass" "${VBAM_TEST_GB_MIN_PASS}")
    if(VBAM_TEST_GB_CGB_BIOS AND EXISTS "${VBAM_TEST_GB_CGB_BIOS}")
        list(APPEND _gb_args "--cgb-bios" "${VBAM_TEST_GB_CGB_BIOS}")
    endif()
    if(VBAM_TEST_GB_DMG_BIOS AND EXISTS "${VBAM_TEST_GB_DMG_BIOS}")
        list(APPEND _gb_args "--dmg-bios" "${VBAM_TEST_GB_DMG_BIOS}")
    endif()
    list(APPEND _gb_args "${VBAM_TEST_GB_ROMS}")
    add_test(NAME gb.blargg
             COMMAND gb_suite_runner ${_gb_args})
    set_tests_properties(gb.blargg PROPERTIES
        LABELS "gb;accuracy"
        TIMEOUT 1200)
    message(STATUS "vbam-core test: gb.blargg enabled "
        "(roms=${VBAM_TEST_GB_ROMS} floor=${VBAM_TEST_GB_MIN_PASS})")
else()
    message(STATUS
        "vbam-core test: gb.blargg SKIPPED — set VBAM_TEST_GB_ROMS to enable")
endif()