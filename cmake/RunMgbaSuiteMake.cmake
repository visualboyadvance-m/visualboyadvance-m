# Build-time driver for the mGBA test suite ROM, invoked via `cmake -P` from the
# custom command created in GbaSuiteRom.cmake. It sets up the build-local
# devkitPro environment (no system install) and runs the suite's devkitPro
# template Makefile, then copies the resulting suite.gba into the build tree.
#
# Inputs (passed with -D):
#   DEVKITPRO   build-local devkitPro root (<build>/devkitpro/opt/devkitpro)
#   DEVKITARM   ${DEVKITPRO}/devkitARM
#   MAKE        path to GNU make
#   SUITE_SRC   mgba-emu/suite checkout (holds the Makefile + gfx sources)
#   OUT_DIR     directory to place the built suite.gba in
cmake_minimum_required(VERSION 3.19)

set(ENV{DEVKITPRO} "${DEVKITPRO}")
set(ENV{DEVKITARM} "${DEVKITARM}")

# The suite's Makefile rules resolve the toolchain and host tools (gbafix,
# bin2s, grit) via PATH, so prepend both devkitPro bin dirs. Use the host path
# separator; $ENV{PATH} here is the build-time environment, not a baked-in one.
if(CMAKE_HOST_WIN32)
    set(_sep ";")
else()
    set(_sep ":")
endif()
set(ENV{PATH} "${DEVKITPRO}/tools/bin${_sep}${DEVKITARM}/bin${_sep}$ENV{PATH}")

execute_process(
    COMMAND "${MAKE}" -C "${SUITE_SRC}"
    RESULT_VARIABLE _rc)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "mGBA suite build failed (make returned ${_rc})")
endif()

if(NOT EXISTS "${SUITE_SRC}/suite.gba")
    message(FATAL_ERROR "mGBA suite build produced no suite.gba")
endif()

file(COPY "${SUITE_SRC}/suite.gba" DESTINATION "${OUT_DIR}")
