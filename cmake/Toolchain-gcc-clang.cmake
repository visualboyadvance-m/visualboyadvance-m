add_compile_options(
    -pipe
    $<$<COMPILE_LANGUAGE:CXX>:-Wno-deprecated-copy>
    -Wformat
    -Wformat-security
    -fdiagnostics-color=always
)

# Treat warnings as errors in CI
if(ENABLE_WERROR)
    add_compile_options(-Werror)
endif()

if(APPLE)
    add_compile_options(-Wno-deprecated-declarations)
endif()

# check if ssp flags are supported.
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    check_cxx_compiler_flag(-fstack-protector-strong STACK_PROTECTOR_SUPPORTED)

    if(STACK_PROTECTOR_SUPPORTED)
        add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fstack-protector-strong> $<$<COMPILE_LANGUAGE:C>:-fstack-protector-strong>)

        check_cxx_compiler_flag("--param ssp-buffer-size=4" SSP_BUFFER_SIZE_SUPPORTED)
        if(SSP_BUFFER_SIZE_SUPPORTED)
            add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:--param;ssp-buffer-size=4>" "$<$<COMPILE_LANGUAGE:C>:--param;ssp-buffer-size=4>")
        endif()
    endif()
endif()

if(NOT ENABLE_ASM) # inline asm is not allowed with -fPIC
    add_compile_options(-fPIC)
endif()

if(UPSTREAM_RELEASE)
    if(X86_64)
        if(APPLE)
            add_compile_options(-march=core2 -mtune=skylake)
        elseif(WIN32)
            add_compile_options(-march=core2 -mtune=generic)
        endif()
        add_compile_options(-msse3)
    elseif(WIN32 AND X86_32)
        set(WINXP TRUE)
        add_compile_definitions(-DWINXP)
        add_compile_options(-march=pentium3 -mmmx -msse -mfpmath=sse)
    endif()
elseif(X86_32)
    add_compile_options(-msse2)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-ggdb3 -fno-omit-frame-pointer -Wall -Wextra)
else()
    if(NOT WINXP)
        add_compile_options(-O3 -ffast-math -fomit-frame-pointer)
    else()
        add_compile_options(-O3 -fomit-frame-pointer)
    endif()

    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        add_compile_options(-fexpensive-optimizations)
    endif()
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
    add_compile_options(-Wno-unused-command-line-argument -Wno-unknown-pragmas)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(-feliminate-unused-debug-types)
endif()

# for some reason this is necessary
if(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    include_directories(/usr/local/include)
endif()

if(VBAM_STATIC)
    if(APPLE)
        add_link_options($<$<COMPILE_LANGUAGE:CXX>:-static-libstdc++>)
    else()
        add_link_options(-static-libgcc $<$<COMPILE_LANGUAGE:CXX>:-static-libstdc++> -Wl,-Bstatic -lstdc++ -lpthread)
    endif()
endif()

# To support LTO, this must always fail.
add_compile_options(-Werror=odr -Werror=strict-aliasing)
add_link_options(   -Werror=odr -Werror=strict-aliasing)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(-Werror=lto-type-mismatch)
    add_link_options(   -Werror=lto-type-mismatch)
endif()
