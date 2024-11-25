# Set the runtime library properly.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:DEBUG>:Debug>" CACHE INTERNAL "")

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    # MSVC-specific flags (not supported by clang-cl).
    list(APPEND VBAM_COMPILE_OPTS /nologo)
    if (NOT CMAKE_GENERATOR MATCHES "Ninja")
        # Multi-processor compilation does not work well with Ninja.
        list(APPEND VBAM_COMPILE_OPTS /MP)
    endif()
endif()

include_directories("${CMAKE_SOURCE_DIR}/win32-deps/msvc")

add_compile_definitions(
    __STDC_LIMIT_MACROS
    __STDC_CONSTANT_MACROS
    _UNICODE
    UNICODE
    WINVER=0x0A00
    NTDDI_VERSION=0x0A000007
)

list(APPEND VBAM_COMPILE_DEFS
    WIN32_LEAN_AND_MEAN
    WIN32
    _CRT_SECURE_NO_WARNINGS
    NOMINMAX
)
list(APPEND VBAM_COMPILE_OPTS
    /W4
    /GR
    /EHsc
)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" AND NOT ENABLE_ASAN)
        # Use Edit and Continue with MSVC.
        set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "EditAndContinue" CACHE STRING "" FORCE)
    else()
        set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "ProgramDatabase" CACHE STRING "" FORCE)
    endif()
else()
    add_compile_options(/Oi /Gy)
    add_link_options(/OPT:REF /OPT:ICF)
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "ProgramDatabase" CACHE STRING "" FORCE)
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
