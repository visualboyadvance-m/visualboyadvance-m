# Helper module for gtest_discover_tests that handles MinGW PATH issues.
#
# When building 32-bit MinGW from a 64-bit shell (or vice versa), the test
# executables may fail to run because they find the wrong architecture DLLs
# in PATH. This module detects the MinGW bin directory from the compiler path
# and creates a wrapper script that sets the correct PATH before running tests.

# Detect MinGW bin directory from compiler path.
if(MINGW AND CMAKE_CXX_COMPILER)
    get_filename_component(MINGW_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
    # Convert to native path format for Windows.
    file(TO_NATIVE_PATH "${MINGW_BIN_DIR}" MINGW_BIN_DIR_NATIVE)

    # Create a wrapper script that sets the PATH and runs the test executable.
    # This is used as CROSSCOMPILING_EMULATOR to handle test discovery and execution.
    set(MINGW_TEST_WRAPPER "${CMAKE_BINARY_DIR}/mingw-test-wrapper.cmd")
    file(WRITE "${MINGW_TEST_WRAPPER}"
"@echo off
set \"PATH=${MINGW_BIN_DIR_NATIVE};%PATH%\"
%*
")
endif()

# Wrapper function for gtest_discover_tests that sets up the correct
# environment for MinGW builds.
#
# Usage:
#   vbam_gtest_discover_tests(<target>)
#
function(vbam_gtest_discover_tests TARGET)
    if(MINGW AND MINGW_TEST_WRAPPER)
        # Use the wrapper script as CROSSCOMPILING_EMULATOR to ensure the
        # correct PATH is set during both test discovery and execution.
        set_target_properties(${TARGET} PROPERTIES
            CROSSCOMPILING_EMULATOR "${MINGW_TEST_WRAPPER}"
        )
    endif()

    gtest_discover_tests(${TARGET})
endfunction()
