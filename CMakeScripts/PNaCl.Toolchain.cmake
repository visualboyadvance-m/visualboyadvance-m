# How to build for PNaCl on Linux.
#
# Download the Native Client SDK.
# Run naclsdk update pepper_XX. The pepper version must be pepper_37.
# Set NACL_SDK_ROOT in your environment to nacl_sdk/pepper_XX.
#
# Check out the naclports repository. This was built with
# naclports@b0aaa9899316e157883ee4b54f70affbebd2c3e1.
# Set NACL_ARCH to 'pnacl' in your environment and run 'make sdl'
# from the naclports root.
#
# Come back to the root of vba-m and run
# cmake -DCMAKE_TOOLCHAIN_FILE=CMakeScripts/PNaCl.Toolchain.cmake CMakeLists.txt
# make
#
# This will build a non-finalized PNaCl port to ./vbam and a finalized version
# to src/pnacl/app/vbam.pexe.
#
# The src/pnacl/app folder can be loaded as an unpacked extension into Chrome
# which will run vba-m as a packaged app.

include( CMakeForceCompiler )

set( PNACL                   ON )
set( PLATFORM_PREFIX         "$ENV{NACL_SDK_ROOT}/toolchain/linux_pnacl" )
set( FINALIZED_TARGET        "src/pnacl/app/vbam.pexe" )

set( CMAKE_SYSTEM_NAME       "Linux" CACHE STRING "Target system." )
set( CMAKE_SYSTEM_PROCESSOR  "LLVM-IR" CACHE STRING "Target processor." )
set( CMAKE_FIND_ROOT_PATH    "${PLATFORM_PREFIX}/usr" )
set( CMAKE_AR                "${PLATFORM_PREFIX}/bin64/pnacl-ar" CACHE STRING "")
set( CMAKE_RANLIB            "${PLATFORM_PREFIX}/bin64/pnacl-ranlib" CACHE STRING "")
set( CMAKE_C_COMPILER        "${PLATFORM_PREFIX}/bin64/pnacl-clang" )
set( CMAKE_CXX_COMPILER      "${PLATFORM_PREFIX}/bin64/pnacl-clang++" )
set( CMAKE_C_FLAGS           "-Wno-non-literal-null-conversion -Wno-deprecated-writable-strings -U__STRICT_ANSI__" CACHE STRING "" )
set( CMAKE_CXX_FLAGS         "-Wno-non-literal-null-conversion -Wno-deprecated-writable-strings -U__STRICT_ANSI__" CACHE STRING "" )

cmake_force_c_compiler(      ${CMAKE_C_COMPILER} Clang )
cmake_force_cxx_compiler(    ${CMAKE_CXX_COMPILER} Clang )

set( CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER )
set( CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY )
set( CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY )
set( CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY )

macro( build_to_app _target )
    add_custom_command( TARGET ${_target}
        POST_BUILD
        COMMAND "${PLATFORM_PREFIX}/bin64/pnacl-finalize"
        "-o" "${FINALIZED_TARGET}"
        "$<TARGET_FILE:${_target}>" )
endmacro()

set( ENV{SDLDIR} "${PLATFORM_PREFIX}/usr" )
include_directories( SYSTEM $ENV{NACL_SDK_ROOT}/include )
link_directories( $ENV{NACL_SDK_ROOT}/lib/pnacl/Release )
