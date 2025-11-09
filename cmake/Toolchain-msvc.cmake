# Set the runtime library properly.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:DEBUG>:Debug>" CACHE INTERNAL "")

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    # MSVC-specific flags (not supported by clang-cl).
    add_compile_options(/nologo)
    if (NOT CMAKE_GENERATOR MATCHES "Ninja")
        # Multi-processor compilation does not work well with Ninja.
        add_compile_options(/MP)
    endif()
endif()

include_directories("${CMAKE_SOURCE_DIR}/win32-deps/msvc")

add_compile_definitions(
    _FORCENAMELESSUNION
    WIN32_LEAN_AND_MEAN
    WIN32
    _WINDOWS
    __STDC_LIMIT_MACROS
    __STDC_CONSTANT_MACROS
    _CRT_SECURE_NO_WARNINGS
    _UNICODE
    UNICODE
    WINVER=0x0A00
    NTDDI_VERSION=0x0A000007
    NOMINMAX
)
add_compile_options(
    /W4
    /GR
    /EHsc
)

# Treat warnings as errors in CI
if(ENABLE_WERROR)
    add_compile_options(/WX)
endif()

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_definitions(_DEBUG)
    add_compile_options(/Ob0 /Od /RTC1)
    if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" AND NOT ENABLE_ASAN)
        # Use Edit and Continue with MSVC.
        set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "EditAndContinue" CACHE STRING "" FORCE)
    else()
        set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "ProgramDatabase" CACHE STRING "" FORCE)
    endif()
else()
    add_compile_options(/MT /Oi /Gy)
    add_link_options(/OPT:REF /OPT:ICF)
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "ProgramDatabase" CACHE STRING "" FORCE)

    if (CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
        add_compile_options(/O1 /Ob1)
    elseif(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
        add_compile_options(/O2 /Ob1)
    else()
        add_compile_options(/O2)
    endif()
endif()

if(CMAKE_VERSION VERSION_LESS "3.25")
    # Backwards-compatible way of setting the /Z option.
    if(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT STREQUAL "EditAndContinue")
        add_compile_options(/ZI)
    elseif(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT STREQUAL "ProgramDatabase")
        add_compile_options(/Zi)
    elseif(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT STREQUAL "Embedded")
        add_compile_options(/Z7)
    else()
        message(FATAL_ERROR "Unknown value for CMAKE_MSVC_DEBUG_INFORMATION_FORMAT: ${CMAKE_MSVC_DEBUG_INFORMATION_FORMAT}")
    endif()
endif()

add_compile_options($<$<COMPILE_LANGUAGE:CXX>:/std:c++17>)

set(CMAKE_RC_FLAGS "-c65001 /DWIN32" CACHE STRING "" FORCE)

# We need to explicitly set all of these to override the CMake defaults.
set(CMAKE_CXX_FLAGS "" CACHE STRING "" FORCE)
set(CMAKE_C_FLAGS "" CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS_DEBUG "" CACHE STRING "" FORCE)
set(CMAKE_C_FLAGS_DEBUG "" CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS_RELEASE "" CACHE STRING "" FORCE)
set(CMAKE_C_FLAGS_RELEASE "" CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "" CACHE STRING "" FORCE)
set(CMAKE_C_FLAGS_RELWITHDEBINFO "" CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS_MINSIZEREL "" CACHE STRING "" FORCE)
set(CMAKE_C_FLAGS_MINSIZEREL "" CACHE STRING "" FORCE)
