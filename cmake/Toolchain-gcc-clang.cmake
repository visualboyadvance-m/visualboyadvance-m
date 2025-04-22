if(X86_32 OR X86_64)
    add_compile_options(-mfpmath=sse -msse2)
endif()

if(UPSTREAM_RELEASE)
    if(X86_64)
        # Require and optimize for Core2 level support, tune for generic.
        if(APPLE)
            add_compile_options(-march=core2 -mtune=skylake)
        else()
            add_compile_options(-march=core2 -mtune=generic)
        endif()
    elseif(X86_32)
        # Optimize for pentiumi3 and tune for generic for Windows XP builds.
        set(WINXP TRUE)
        add_compile_options(-march=pentium3 -mtune=generic)
        add_compile_definitions(-DWINXP)
    endif()
endif()

# Common flags.
add_compile_options(
    -pipe
    $<$<COMPILE_LANGUAGE:CXX>:-Wno-deprecated-copy>
    -Wformat
    -Wformat-security
    -fdiagnostics-color=always
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_options(-Wno-unused-command-line-argument)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(-feliminate-unused-debug-types)
endif()

# check if ssp flags are supported.
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    check_cxx_compiler_flag(-fstack-protector-strong STACK_PROTECTOR_SUPPORTED)

    if(STACK_PROTECTOR_SUPPORTED)
        add_compile_options(-fstack-protector-strong)

        check_cxx_compiler_flag("--param ssp-buffer-size=4" SSP_BUFFER_SIZE_SUPPORTED)
        if(SSP_BUFFER_SIZE_SUPPORTED)
            add_compile_options(--param ssp-buffer-size=4)
        endif()
    endif()
endif()

if(NOT ENABLE_ASM) # inline asm is not allowed with -fPIC
    add_compile_options(-fPIC)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-ggdb3 -fno-omit-frame-pointer -Wall -Wextra)
else()
    add_compile_options(-O3 -ffast-math -fomit-frame-pointer)
endif()

# for some reason this is necessary
if(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    include_directories(/usr/local/include)
endif()

if(VBAM_STATIC)
    if(APPLE)
        add_link_options(-static-libstdc++)
    else()
        add_link_options(-static-libgcc -static-libstdc++ -Wl,-Bstatic -lstdc++ -lpthread)
    endif()
endif()

# To support LTO, this must always fail.
add_compile_options(-Werror=odr -Werror=strict-aliasing)
add_link_options(   -Werror=odr -Werror=strict-aliasing)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(-Werror=lto-type-mismatch)
    add_link_options(   -Werror=lto-type-mismatch)
endif()
