add_compile_options(
    -pipe
    $<$<COMPILE_LANGUAGE:CXX>:-Wno-deprecated-copy>
    -Wformat
    -fdiagnostics-color=always
)

# Treat warnings as errors in CI
if(ENABLE_WERROR AND NOT ANDROID)
    add_compile_options(-Werror)
endif()

if(APPLE)
    add_compile_options(-Wno-deprecated-declarations)
endif()

# wx/wxcrtbase.h carries an unconditional #warning about the Android NDK's
# limited mb/wchar conversion. Nothing in this project can act on it, and
# because wx/string.h reaches nearly every wxWidgets translation unit it
# accounted for 58 of the 62 warnings in an Android build. Marking the wx
# include directories SYSTEM does not help: clang reports #warning from system
# headers too. So turn the diagnostic off, on Android only, where the sole
# instance of it comes from wx.
# The flag has to be quoted: unquoted, CMake reads the # as the start of a
# comment and the add_compile_options() call loses its closing paren.
if(ANDROID)
    add_compile_options("-Wno-#warnings")
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

# Not for Metal shaders: the AGX compiler in macOS 27.2 never finishes the
# hdrEncodeScRGB pipeline from AIR built with -fPIC, and the app hangs on
# startup in newComputePipelineStateWithFunction:.
# Not for DJGPP either: it ignores -fPIC with a notice on the driver's
# stdout, and with -pipe that notice is the first line the assembler reads.
# Not for Retro68: classic Mac code is relocated by Elf2Mac/MakePEF, and on
# 68k -fPIC stops gas relaxing out-of-range branches into absolute jumps.
if(NOT ENABLE_ASM AND NOT DOS AND NOT RETRO68) # inline asm is not allowed with -fPIC
    add_compile_options($<$<NOT:$<COMPILE_LANGUAGE:Metal>>:-fPIC>)
endif()

# DJGPP's pthread.h (GNU Pth) turns read, write, send, recv and friends into
# macros for its thread-aware wrappers, which renames every method called
# write() (SoundDriver::write) in a translation unit that includes <thread>.
# The header only defaults the switch when it is undefined; turn it off.
if(DOS)
    add_compile_definitions(_POSIX_THREAD_SYSCALL_SOFT=0)
endif()

# DJGPP's GCC targets the Pentium Pro by default and so compares floats with
# fcomi. The Pentium is the natural floor for a DOS build, and the emulator
# most people run one in, DOSBox-X, gets fcomi wrong in its dynamic core
# (the flags come out inverted or stale): SDL's resampler setup, a float
# convergence loop, then never ends and the port hangs before the first
# frame. Built for the 586, GCC compares through fnstsw, which every core
# runs correctly. The SDL3 the port links against must be built the same
# way, since the loop in question is SDL's.
if(DOS)
    add_compile_options(-march=i586 -mtune=generic)
endif()

# Classic 68k Macs. The wx and SDL3 libraries in the Retro68 sysroot are
# built for the default 68000 soft-float ABI; a 68020 soft-float build calls
# them the same way (doubles in d0/d1, no 68881 registers), gets 32-bit
# multiply/divide and long branches, and still runs on the FPU-less 68LC040.
# The GBA cannot fit in a 68000 Mac's 4 MB anyway.
if(RETRO68 AND M68K)
    set(RETRO68_M68K_FLAGS "-m68020;-msoft-float" CACHE STRING
        "Code generation flags for the Retro68 68k build")
    add_compile_options(${RETRO68_M68K_FLAGS})
    add_link_options(${RETRO68_M68K_FLAGS})
endif()

# A classic Mac application is one code resource (68k) or fragment (PEF), so
# leave out what nothing calls, as Retro68 and wxRetro68 recommend.
if(RETRO68)
    add_compile_options(-ffunction-sections -fdata-sections)
    add_link_options(-Wl,--gc-sections)
endif()

# Workarounds every Retro68 source file needs, above all single-threaded
# std::mutex and friends for a libstdc++ without gthreads; see the header.
if(RETRO68)
    add_compile_options(
        "$<$<COMPILE_LANGUAGE:C,CXX>:SHELL:-include ${CMAKE_SOURCE_DIR}/src/core/base/retro68/prefix.h>")
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
        # -DWINXP is added unconditionally for 32-bit Windows in the root
        # CMakeLists.txt alongside -D_WIN32_WINNT=0x0501.
        add_compile_options(-march=pentium3 -mmmx -msse -mfpmath=sse)
    endif()
elseif(X86_32)
    add_compile_options(-msse2)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-ggdb3 -fno-omit-frame-pointer -Wall -Wextra)
elseif(RETRO68 AND M68K)
    # A 68k application's code and data are resources, and a resource fork
    # cannot be over 16 MB; at -O3 the 68k build is 18 MB, at -Os 13 MB. The
    # 68020-68040's instruction caches (256 bytes to 4 KB) favour small code
    # anyway. PowerPC code is a PEF in the data fork, which has no such limit.
    set(RETRO68_M68K_OPT "-Os" CACHE STRING "Optimization flags for the Retro68 68k build")
    add_compile_options(${RETRO68_M68K_OPT} -ffast-math -fomit-frame-pointer)
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
    add_compile_options(-Wno-unused-command-line-argument -Wno-unknown-pragmas  -Wno-nan-infinity-disabled -Wno-unknown-warning-option)

    # With Clang + LTO the static archives hold LLVM bitcode rather than native
    # objects, which GNU ar/ld cannot handle ("file format not recognized").
    # Use llvm-ar/llvm-ranlib, which understand bitcode via the LLVM plugin,
    # whenever they are available.
    find_program(LLVM_AR NAMES llvm-ar)
    find_program(LLVM_RANLIB NAMES llvm-ranlib)
    if(LLVM_AR)
        set(CMAKE_AR "${LLVM_AR}" CACHE FILEPATH "Archiver" FORCE)
    endif()
    if(LLVM_RANLIB)
        set(CMAKE_RANLIB "${LLVM_RANLIB}" CACHE FILEPATH "Ranlib" FORCE)
    endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(-feliminate-unused-debug-types)
endif()

# for some reason this is necessary
if(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    include_directories(/usr/local/include)
endif()

# Retro68 has only static libraries and no libpthread.
if(VBAM_STATIC AND NOT RETRO68)
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
