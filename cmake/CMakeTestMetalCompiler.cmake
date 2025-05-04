# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENCE.txt or https://cmake.org/licensing for details.

# CMakeTest(LANG)Compiler.cmake -> test the compiler and set:
#     SET(CMAKE_(LANG)_COMPILER_WORKS 1 CACHE INTERNAL "")

if(CMAKE_Metal_COMPILER_FORCED)
    # The compiler configuration was forced by the user.
    # Assume the user has configured all compiler information.
    set(CMAKE_Metal_COMPILER_WORKS TRUE)
    return()
endif()

include(CMakeTestCompilerCommon)

if("${CMAKE_GENERATOR}" STREQUAL "Xcode")
    if(XCODE_VERSION VERSION_GREATER 7.0)
        set(CMAKE_Metal_COMPILER_WORKS 1)
    endif()
endif()

# This file is used by EnableLanguage in cmGlobalGenerator to
# determine that that selected Metal compiler can actually compile
# and link the most basic of programs.   If not, a fatal error
# is set and cmake stops processing commands and will not generate
# any makefiles or projects.
if(NOT CMAKE_Metal_COMPILER_WORKS)
    PrintTestCompilerStatus("Metal")
    __TestCompiler_setTryCompileTargetType()

    string(CONCAT __TestCompiler_testMetalCompilerSource
        "#ifndef __METAL_VERSION__\n"
        "# error \"The CMAKE_Metal_COMPILER is not a Metal compiler\"\n"
        "#endif\n"
        "#import <metal_stdlib>\n"
        "using namespace metal;\n"
    )

    # Clear result from normal variable.
    unset(CMAKE_Metal_COMPILER_WORKS)

    # Puts test result in cache variable.
    try_compile(CMAKE_Metal_COMPILER_WORKS
        SOURCE_FROM_VAR testMetalCompiler.metal __TestCompiler_testMetalCompilerSource
        OUTPUT_VARIABLE __CMAKE_Metal_COMPILER_OUTPUT
    )
    unset(__TestCompiler_testMetalCompilerSource)

    # Move result from cache to normal variable.
    set(CMAKE_Metal_COMPILER_WORKS ${CMAKE_Metal_COMPILER_WORKS})
    unset(CMAKE_Metal_COMPILER_WORKS CACHE)
    __TestCompiler_restoreTryCompileTargetType()
    set(METAL_TEST_WAS_RUN 1)
endif()

if(NOT CMAKE_Metal_COMPILER_WORKS)
    PrintTestCompilerResult(CHECK_FAIL "broken")
    string(REPLACE "\n" "\n  " _output "${__CMAKE_Metal_COMPILER_OUTPUT}")
    message(FATAL_ERROR "The Metal compiler\n  \"${CMAKE_Metal_COMPILER}\"\n"
        "is not able to compile a simple test program.\nIt fails "
        "with the following output:\n  ${_output}\n\n"
        "CMake will not be able to correctly generate this project."
    )
else()
    if(METAL_TEST_WAS_RUN)
        PrintTestCompilerResult(CHECK_PASS "works")
    endif()

    # Re-configure to save learned information.
    configure_file(
        ${CMAKE_CURRENT_LIST_DIR}/CMakeMetalCompiler.cmake.in
        ${CMAKE_PLATFORM_INFO_DIR}/CMakeMetalCompiler.cmake
        @ONLY
    )
    include(${CMAKE_PLATFORM_INFO_DIR}/CMakeMetalCompiler.cmake)
endif()

unset(__CMAKE_Metal_COMPILER_OUTPUT)
