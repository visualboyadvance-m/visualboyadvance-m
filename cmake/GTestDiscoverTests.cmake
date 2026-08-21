# Helper module for gtest_discover_tests that handles MinGW PATH issues.
#
# When building 32-bit MinGW from a 64-bit shell (or vice versa), the test
# executables may fail to run because they find the wrong architecture DLLs
# in PATH. This module detects the MinGW bin directory from the compiler path
# and creates a wrapper script that sets the correct PATH before running tests.

# Detect MinGW bin directory from compiler path.
if(MINGW AND CMAKE_CXX_COMPILER AND CMAKE_HOST_WIN32)
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
    if(WIN32)
        # Both gtest_discover_tests() -- which runs the binary to enumerate its
        # cases -- and ctest read the test's stdout, and a GUI subsystem
        # executable has none. The wx link flags add -mwindows, so ask for the
        # console subsystem and repeat it as a link option, which lands after
        # them on the command line.
        set_target_properties(${TARGET} PROPERTIES WIN32_EXECUTABLE FALSE)

        if(NOT MSVC)
            # As a link library, not a link option: the wx libraries bring their
            # own -Wl,--subsystem,windows and the last one on the command line
            # wins, so this has to be emitted after them.
            # Appended to the property rather than via target_link_libraries(),
            # whose plain and keyword signatures cannot be mixed per target.
            set_property(TARGET ${TARGET} APPEND PROPERTY
                         LINK_LIBRARIES "-Wl,--subsystem,console")
        endif()
    endif()

    # Otherwise CMAKE_CROSSCOMPILING_EMULATOR from the toolchain file applies.
    if(MINGW AND MINGW_TEST_WRAPPER)
        # Use the wrapper script as CROSSCOMPILING_EMULATOR to ensure the
        # correct PATH is set during both test discovery and execution.
        set_target_properties(${TARGET} PROPERTIES
            CROSSCOMPILING_EMULATOR "${MINGW_TEST_WRAPPER}"
        )
    endif()

    if(CMAKE_CROSSCOMPILING_EMULATOR)
        # Discovery runs the binary; the 5s default is not enough under wine.
        gtest_discover_tests(${TARGET} DISCOVERY_TIMEOUT 120)
    else()
        gtest_discover_tests(${TARGET})
    endif()
endfunction()
