# Set the runtime library properly.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:DEBUG>:Debug>" CACHE INTERNAL "")

if(CMAKE_CXX_COMPILER_ID STREQUAL MSVC)
    # MSVC-specific flags (not supported by clang-cl).
    add_compile_options(/nologo)
    if (NOT CMAKE_GENERATOR MATCHES "Ninja")
        # Multi-processor compilation does not work well with Ninja.
        add_compile_options(/MP)
    endif()
endif()

include_directories("${CMAKE_SOURCE_DIR}/dependencies/msvc")

add_compile_definitions(
    _FORCENAMELESSUNION
    WIN32_LEAN_AND_MEAN
    WIN32
    _WINDOWS
    __STDC_LIMIT_MACROS
    __STDC_CONSTANT_MACROS
    _CRT_SECURE_NO_WARNINGS
)
add_compile_options(
    /W4
    /GR
    /EHsc
)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_definitions(_DEBUG DEBUG)
    add_compile_options(/Ob0 /Od /RTC1)
    if (CMAKE_CXX_COMPILER_ID STREQUAL MSVC)
        # Use Edit and Continue with MSVC.
        add_compile_options(/ZI)
    else()
        add_compile_options(/Zi)
    endif()
else()
    add_compile_definitions(NDEBUG)
    add_compile_options(/MT /Oi /Gy /Zi)
    add_link_options(/OPT:REF /OPT:ICF)

    if (CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
        add_compile_options(/O1 /Ob1)
    elseif(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
        add_compile_options(/O2 /Ob1)
    else()
        add_compile_options(/O2)
    endif()
endif()

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
