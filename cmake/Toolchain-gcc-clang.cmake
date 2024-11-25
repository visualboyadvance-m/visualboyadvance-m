if(X86_32 OR X86_64)
    list(APPEND VBAM_COMPILE_OPTS -mfpmath=sse -msse2)
endif()

if(UPSTREAM_RELEASE)
    if(X86_64)
        # Require and optimize for Core2 level support, tune for generic.
        list(APPEND VBAM_COMPILE_OPTS -march=core2 -mtune=generic)
    elseif(X86_32)
        # Optimize for pentium-mmx and tune for generic for older builds.
        list(APPEND VBAM_COMPILE_OPTS -march=pentium-mmx -mtune=generic)
    endif()
endif()

# Common flags.
list(APPEND VBAM_COMPILE_OPTS
    -pipe
    $<$<COMPILE_LANGUAGE:CXX>:-Wno-deprecated-copy>
    -Wformat
    -Wformat-security
    -fdiagnostics-color=always
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    list(APPEND VBAM_COMPILE_OPTS -Wno-unused-command-line-argument)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    list(APPEND VBAM_COMPILE_OPTS -feliminate-unused-debug-types)
endif()

# check if ssp flags are supported.
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    check_cxx_compiler_flag(-fstack-protector-strong STACK_PROTECTOR_SUPPORTED)

    if(STACK_PROTECTOR_SUPPORTED)
        list(APPEND VBAM_COMPILE_OPTS -fstack-protector-strong)

        check_cxx_compiler_flag("--param ssp-buffer-size=4" SSP_BUFFER_SIZE_SUPPORTED)
        if(SSP_BUFFER_SIZE_SUPPORTED)
            list(APPEND VBAM_COMPILE_OPTS --param ssp-buffer-size=4)
        endif()
    endif()
endif()

if(NOT ENABLE_ASM) # inline asm is not allowed with -fPIC
    list(APPEND VBAM_COMPILE_OPTS -fPIC)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    list(APPEND VBAM_COMPILE_OPTS -ggdb3 -fno-omit-frame-pointer -Wall -Wextra)
else()
    list(APPEND VBAM_COMPILE_OPTS -Ofast -fomit-frame-pointer)
endif()

# for some reason this is necessary
if(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    include_directories(/usr/local/include)
endif()

if(VBAM_STATIC)
    if(APPLE)
        list(APPEND VBAM_LINK_OPTS -static-libstdc++)
    else()
        list(APPEND VBAM_LINK_OPTS -static-libgcc -static-libstdc++ -Wl,-Bstatic -lstdc++ -lpthread)
    endif()
endif()

# To support LTO, this must always fail.
list(APPEND VBAM_COMPILE_OPTS -Werror=odr -Werror=strict-aliasing)
list(APPEND VBAM_LINK_OPTS    -Werror=odr -Werror=strict-aliasing)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    list(APPEND VBAM_COMPILE_OPTS -Werror=lto-type-mismatch)
    list(APPEND VBAM_LINK_OPTS    -Werror=lto-type-mismatch)
endif()
