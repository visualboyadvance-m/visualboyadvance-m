if(X86_32 OR X86_64)
    add_compile_options(-mfpmath=sse -msse2)
endif()

if(X86_64)
    # Require and optimize for Core2 level support, tune for generic.
    add_compile_options(-march=core2 -mtune=generic)
elseif(X86_32)
    # Optimize for pentium-mmx and tune for generic for older builds.
    add_compile_options(-march=pentium-mmx -mtune=generic)
endif()

# Common flags.
add_compile_options(
    -pipe
    -Wno-unused-command-line-argument
    -Wno-deprecated-copy
    -Wformat
    -Wformat-security
    -feliminate-unused-debug-types
    -fdiagnostics-color=always
)

# check if ssp flags are supported.
if(CMAKE_BUILD_TYPE STREQUAL Debug)
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

if(CMAKE_BUILD_TYPE STREQUAL Debug)
    add_compile_options(-ggdb3 -Og -fno-omit-frame-pointer -Wall -Wextra)
else()
    add_compile_options(-Ofast -fomit-frame-pointer)
endif()

# for some reason this is necessary
if(CMAKE_SYSTEM_NAME STREQUAL FreeBSD)
    include_directories(/usr/local/include)
endif()

if(VBAM_STATIC)
    add_link_options("-static-libgcc" "-static-libstdc++")
endif()
