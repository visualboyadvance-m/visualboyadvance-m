if(NOT ENABLE_LIBRETRO)
    return()
endif()

# The libretro core manages its own CRT linking (msvcrt.dll, not ucrt).
# Clear inherited linker flags that reference libstdc++/libgcc
# (they conflict with -nodefaultlibs and don't exist on CLANG64).
# Both the CMAKE variable and the directory LINK_OPTIONS property must
# be cleared before add_library() since CMake captures them at target
# creation time.
if(MINGW)
    set(CMAKE_SHARED_LINKER_FLAGS "")
    set_directory_properties(PROPERTIES LINK_OPTIONS "")
endif()

set(VBAM_SRC_DIR ${CMAKE_SOURCE_DIR}/src)

# The libretro core compiles its own copy of the emulator sources with
# libretro-specific defines, so it does NOT link against vbam-core.
add_library(vbam_libretro SHARED
    # Libretro frontend
    libretro.cpp
    SoundRetro.cpp

    # Core base (libretro variant)
    ${VBAM_SRC_DIR}/core/base/internal/file_util_internal.cpp
    ${VBAM_SRC_DIR}/core/base/file_util_common.cpp
    ${VBAM_SRC_DIR}/core/base/file_util_libretro.cpp

    # APU
    ${VBAM_SRC_DIR}/core/apu/Gb_Oscs.cpp
    ${VBAM_SRC_DIR}/core/apu/Gb_Apu_State.cpp
    ${VBAM_SRC_DIR}/core/apu/Blip_Buffer.cpp
    ${VBAM_SRC_DIR}/core/apu/Multi_Buffer.cpp
    ${VBAM_SRC_DIR}/core/apu/Effects_Buffer.cpp
    ${VBAM_SRC_DIR}/core/apu/Gb_Apu.cpp

    # GBA
    ${VBAM_SRC_DIR}/core/gba/gba.cpp
    ${VBAM_SRC_DIR}/core/gba/gbaCheats.cpp
    ${VBAM_SRC_DIR}/core/gba/gbaCheatSearch.cpp
    ${VBAM_SRC_DIR}/core/gba/gbaCpuArm.cpp
    ${VBAM_SRC_DIR}/core/gba/gbaCpuThumb.cpp
    ${VBAM_SRC_DIR}/core/gba/gbaEeprom.cpp
    ${VBAM_SRC_DIR}/core/gba/gbaFlash.cpp
    ${VBAM_SRC_DIR}/core/gba/gbaGfx.cpp
    ${VBAM_SRC_DIR}/core/gba/gbaGlobals.cpp
    ${VBAM_SRC_DIR}/core/gba/gbaMode0.cpp
    ${VBAM_SRC_DIR}/core/gba/gbaMode1.cpp
    ${VBAM_SRC_DIR}/core/gba/gbaMode2.cpp
    ${VBAM_SRC_DIR}/core/gba/gbaMode3.cpp
    ${VBAM_SRC_DIR}/core/gba/gbaMode4.cpp
    ${VBAM_SRC_DIR}/core/gba/gbaMode5.cpp
    ${VBAM_SRC_DIR}/core/gba/gbaPrint.cpp
    ${VBAM_SRC_DIR}/core/gba/gbaRtc.cpp
    ${VBAM_SRC_DIR}/core/gba/gbaSound.cpp
    ${VBAM_SRC_DIR}/core/gba/internal/gbaBios.cpp
    ${VBAM_SRC_DIR}/core/gba/internal/gbaEreader.cpp
    ${VBAM_SRC_DIR}/core/gba/internal/gbaSram.cpp

    # GB
    ${VBAM_SRC_DIR}/core/gb/gb.cpp
    ${VBAM_SRC_DIR}/core/gb/gbCartData.cpp
    ${VBAM_SRC_DIR}/core/gb/gbCheats.cpp
    ${VBAM_SRC_DIR}/core/gb/gbGfx.cpp
    ${VBAM_SRC_DIR}/core/gb/gbGlobals.cpp
    ${VBAM_SRC_DIR}/core/gb/gbMemory.cpp
    ${VBAM_SRC_DIR}/core/gb/gbSGB.cpp
    ${VBAM_SRC_DIR}/core/gb/gbSound.cpp

    # Filters
    ${VBAM_SRC_DIR}/components/filters_agb/filters_agb.cpp
    ${VBAM_SRC_DIR}/components/filters_cgb/filters_cgb.cpp
    ${VBAM_SRC_DIR}/components/filters_interframe/interframe.cpp
)

set_target_properties(vbam_libretro PROPERTIES
    PREFIX ""
    OUTPUT_NAME vbam_libretro
)

target_include_directories(vbam_libretro PRIVATE
    ${VBAM_SRC_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/include
)

target_compile_definitions(vbam_libretro PRIVATE
    __LIBRETRO__
    FINAL_VERSION
    C_CORE
    NO_DEBUGGER
    NO_PNG
    TILED_RENDERING
    NO_LINK
    AFFINE_BG_UNIFIED
    FRONTEND_SUPPORTS_RGB565
    VBAM_VERSION="${VBAM_VERSION}"
)

if(COMMITHASH)
    target_compile_definitions(vbam_libretro PRIVATE
        GIT_COMMIT="${COMMITHASH}"
    )
endif()


# Place the DLL at the top of the build directory (next to executables)
# and disable LTO since this is an independent plugin.
set_target_properties(vbam_libretro PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}
    INTERPROCEDURAL_OPTIMIZATION OFF
)

# --- Platform-specific linker settings ---

if(MSVC)
    # Link against the legacy msvcrt.dll instead of the Universal CRT.
    #
    # Strategy: let MSVC compile normally with /MD (which pulls in the VC
    # toolchain's msvcrt.lib for CRT startup, vcruntime, and msvcp).  Then
    # at link time, exclude ucrt.lib so that C library functions come from
    # our custom import library for the system msvcrt.dll instead of from
    # ucrtbase.dll.  A small shim satisfies UCRT-only symbols that have no
    # msvcrt.dll equivalent.
    #
    # Runtime DLL dependencies:
    #   msvcrt.dll        (system - C library)
    #   vcruntime140.dll  (C++ compiler support: RTTI, exceptions, etc.)
    #   msvcp140.dll      (C++ STL: mutex, string exceptions, etc.)
    # NOT:
    #   ucrtbase.dll

    # Always use the release runtime (/MD), even in Debug builds.  The
    # debug runtime (/MDd) pulls in debug CRT libs (libcpmtd, MSVCRTd)
    # that have additional UCRT dependencies we cannot shim.  Debug info
    # (/Zi) still works — only the CRT assertions are lost.
    set_target_properties(vbam_libretro PROPERTIES
        MSVC_RUNTIME_LIBRARY "MultiThreadedDLL"
    )

    # Prevent the MSVC headers from inlining stdio functions.  The inline
    # versions call __stdio_common_* which only exist in ucrtbase.dll.
    # With this define, printf/sscanf/etc. are extern and resolve from our
    # msvcrt.dll import library instead.
    target_compile_definitions(vbam_libretro PRIVATE _NO_CRT_STDIO_INLINE)

    # Determine machine type for lib.exe.
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(_lr_machine X64)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64|aarch64")
        set(_lr_machine ARM64)
    else()
        set(_lr_machine X86)
    endif()

    # --- Generate import library for system msvcrt.dll ---
    set(_lr_msvcrt_def "${CMAKE_CURRENT_BINARY_DIR}/msvcrt_system.def")
    set(_lr_msvcrt_lib "${CMAKE_CURRENT_BINARY_DIR}/msvcrt_system.lib")

    if(NOT EXISTS "${_lr_msvcrt_lib}")
        message(STATUS "Libretro: generating import library for system msvcrt.dll ...")

        execute_process(
            COMMAND dumpbin /EXPORTS "C:/Windows/System32/msvcrt.dll"
            OUTPUT_VARIABLE _lr_exports_output
            RESULT_VARIABLE _lr_exports_result
            ERROR_QUIET
        )

        if(NOT _lr_exports_result EQUAL 0)
            message(FATAL_ERROR "Libretro: dumpbin failed on msvcrt.dll")
        endif()

        set(_lr_def_content "LIBRARY msvcrt.dll\nEXPORTS\n")

        string(REGEX REPLACE "\r" "" _lr_exports_output "${_lr_exports_output}")
        string(REPLACE "\n" ";" _lr_exports_lines "${_lr_exports_output}")

        foreach(_lr_line IN LISTS _lr_exports_lines)
            if(_lr_line MATCHES "^ +[0-9]+ +[0-9A-Fa-f]+ [0-9A-Fa-f]+ +([^ ]+)")
                string(APPEND _lr_def_content "    ${CMAKE_MATCH_1}\n")
            elseif(_lr_line MATCHES "^ +[0-9]+ +[0-9A-Fa-f]+ +([^ ]+) \\(forwarded to")
                string(APPEND _lr_def_content "    ${CMAKE_MATCH_1}\n")
            endif()
        endforeach()

        file(WRITE "${_lr_msvcrt_def}" "${_lr_def_content}")

        execute_process(
            COMMAND lib /DEF:${_lr_msvcrt_def} /OUT:${_lr_msvcrt_lib} /MACHINE:${_lr_machine}
            WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
            RESULT_VARIABLE _lr_lib_result
            OUTPUT_QUIET
            ERROR_QUIET
        )

        if(NOT _lr_lib_result EQUAL 0)
            message(FATAL_ERROR "Libretro: lib.exe failed to create msvcrt import library")
        endif()

        message(STATUS "Libretro: generated ${_lr_msvcrt_lib}")
    endif()

    # --- UCRT shim: provide functions that only exist in ucrtbase.dll ---
    set(_lr_shim_src "${CMAKE_CURRENT_BINARY_DIR}/msvcrt_shim.c")

    file(WRITE "${_lr_shim_src}" [=[
/*
 * Shim for UCRT-only symbols when linking against msvcrt.dll.
 *
 * MSVC's STL and CRT startup code reference functions that only exist in
 * ucrtbase.dll.  We provide minimal implementations here so the DLL loads
 * with just msvcrt.dll + vcruntime140.dll + msvcp140.dll.
 */

/* Override the dllimport marker for __acrt_iob_func so we can define it
 * ourselves without a linker conflict. */
/* Suppress dllimport on functions we define ourselves. */
#define _ACRTIMP_ALT
#define _ACRTIMP
#define _NO_CRT_STDIO_INLINE
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

/* --- stdio bridge --------------------------------------------------- */

/* msvcrt.dll exports __iob_func; UCRT renamed it to __acrt_iob_func. */
extern __declspec(dllimport) FILE* __iob_func(void);

FILE* __cdecl __acrt_iob_func(unsigned index)
{
    return __iob_func() + index;
}

/* UCRT's __stdio_common_vsprintf_s: used by the CRT startup error path.
 * Forward to msvcrt.dll's _vsnprintf + manual truncation.
 * We use _vsnprintf from <stdio.h> (which resolves from msvcrt.dll). */
int __cdecl __stdio_common_vsprintf_s(
    unsigned __int64 options,
    char *buffer,
    size_t buffer_count,
    const char *format,
    _locale_t locale_arg,
    va_list arglist)
{
    (void)options; (void)locale_arg;
    if (!buffer || !buffer_count) return -1;
    int ret = _vsnprintf(buffer, buffer_count, format, arglist);
    buffer[buffer_count - 1] = '\0';
    return ret;
}

/* --- C99 math (not in msvcrt.dll) ----------------------------------- */

extern __declspec(dllimport) double _copysign(double, double);

float __cdecl copysignf(float x, float y)
{
    return (float)_copysign((double)x, (double)y);
}

float __cdecl fmaxf(float a, float b)
{
    if (a != a) return b;  /* NaN */
    if (b != b) return a;
    return a > b ? a : b;
}

float __cdecl fminf(float a, float b)
{
    if (a != a) return b;
    if (b != b) return a;
    return a < b ? a : b;
}

/* --- CRT startup stubs ---------------------------------------------- */
/* The VC toolchain's MSVCRT.lib CRT startup code (utility.obj) calls
 * these UCRT functions during DLL initialization.  For a libretro core
 * loaded via LoadLibrary these can be no-ops or simple redirects to the
 * msvcrt.dll equivalents. */

/* SEH filter for DLL; return EXCEPTION_EXECUTE_HANDLER. */
long __cdecl _seh_filter_dll(unsigned long code, void *ep)
{
    (void)code; (void)ep;
    return 1; /* EXCEPTION_EXECUTE_HANDLER */
}

int __cdecl _configure_narrow_argv(int mode)
{
    (void)mode;
    return 0;
}

int __cdecl _initialize_narrow_environment(void)
{
    return 0;
}

/* onexit table: redirect to msvcrt.dll's atexit/onexit. */
typedef int (__cdecl *_PVFV)(void);

typedef struct {
    _PVFV *_first;
    _PVFV *_last;
    _PVFV *_end;
} _onexit_table_t;

int __cdecl _initialize_onexit_table(_onexit_table_t *table)
{
    if (table) {
        table->_first = NULL;
        table->_last  = NULL;
        table->_end   = NULL;
    }
    return 0;
}

int __cdecl _register_onexit_function(_onexit_table_t *table, _PVFV func)
{
    (void)table;
    /* Delegate to msvcrt.dll's atexit. */
    return atexit((void (__cdecl *)(void))func);
}

int __cdecl _execute_onexit_table(_onexit_table_t *table)
{
    (void)table;
    /* atexit handlers run automatically at DLL unload; nothing to do. */
    return 0;
}

int __cdecl _crt_atexit(_PVFV func)
{
    return atexit((void (__cdecl *)(void))func);
}

int __cdecl _crt_at_quick_exit(_PVFV func)
{
    (void)func;
    return 0;
}

/* C-linkage terminate(); forward to abort(). */
void __cdecl terminate(void)
{
    abort();
}
]=])

    add_library(vbam_libretro_msvcrt_shim OBJECT "${_lr_shim_src}")
    set_target_properties(vbam_libretro_msvcrt_shim PROPERTIES
        MSVC_RUNTIME_LIBRARY "MultiThreadedDLL"
        INTERPROCEDURAL_OPTIMIZATION OFF
    )

    target_link_libraries(vbam_libretro PRIVATE vbam_libretro_msvcrt_shim)

    # Exclude all UCRT variants and debug CRT libs; our msvcrt_system.lib
    # provides C functions.  /FORCE:MULTIPLE resolves the duplicate atexit
    # between the VC toolchain's MSVCRT.lib startup code and our lib.
    target_link_options(vbam_libretro PRIVATE
        /NODEFAULTLIB:ucrt.lib
        /NODEFAULTLIB:libucrt.lib
        /NODEFAULTLIB:ucrtd.lib
        /NODEFAULTLIB:libucrtd.lib
        /NODEFAULTLIB:LIBCMT
        /NODEFAULTLIB:LIBCMTd
        /NODEFAULTLIB:MSVCRTd.lib
        /NODEFAULTLIB:libcpmtd.lib
        /NODEFAULTLIB:vcruntimed.lib
        /NODEFAULTLIB:msvcprtd.lib
        /FORCE:MULTIPLE
    )
    # Explicitly link the release C++ STL and vcruntime import libraries.
    # In Debug builds the auto-linked variants may get excluded above.
    target_link_libraries(vbam_libretro PRIVATE
        "${_lr_msvcrt_lib}"
        msvcprt.lib
        vcruntime.lib
    )

    unset(_lr_machine)
    unset(_lr_msvcrt_def)
    unset(_lr_msvcrt_lib)
    unset(_lr_shim_src)

elseif(MINGW)
    # Link the libretro core against msvcrt.dll on all MSYS2 environments.
    # MINGW64 already defaults to msvcrt, but UCRT64 and CLANG64 default
    # to ucrtbase.dll.  Using -nodefaultlibs prevents the toolchain from
    # adding its default CRT; we then explicitly link msvcrt and the
    # other required runtime libraries.
    target_link_options(vbam_libretro PRIVATE -nodefaultlibs)

    # Version script for symbol visibility (GNU ld only).
    # lld (used by CLANG64) doesn't support --version-script on PE/COFF.
    if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        # Copy link.T into the build dir and use a relative path to avoid
        # Windows drive-letter colons which GNU ld misparses.
        configure_file(
            ${CMAKE_CURRENT_SOURCE_DIR}/link.T
            ${CMAKE_CURRENT_BINARY_DIR}/link.T
            COPYONLY
        )
        file(RELATIVE_PATH _lr_link_script
            ${CMAKE_BINARY_DIR} ${CMAKE_CURRENT_BINARY_DIR}/link.T)
        target_link_options(vbam_libretro PRIVATE
            -Wl,--version-script=${_lr_link_script}
            -Wl,--no-undefined
        )
        set_target_properties(vbam_libretro PROPERTIES
            LINK_DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/link.T
        )
        unset(_lr_link_script)
    endif()

    # Static C++ runtime (avoids extra DLL dependencies).
    if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        target_link_libraries(vbam_libretro PRIVATE
            -Wl,-Bstatic -lc++ -lunwind
            -Wl,-Bdynamic
        )
    else()
        target_link_libraries(vbam_libretro PRIVATE
            -Wl,-Bstatic -lstdc++
            -Wl,-Bdynamic
        )
    endif()

    # CRT and system libraries.  --start-group handles circular
    # dependencies between mingw32/mingwex/msvcrt/gcc.
    set(_lr_mingw_crt_libs -lmingw32 -lmingwex -lmsvcrt -lmoldname -lpthread)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        # Clang's compiler-rt builtins (provides ___chkstk_ms, etc.)
        execute_process(
            COMMAND ${CMAKE_C_COMPILER} --print-libgcc-file-name
            OUTPUT_VARIABLE _lr_compiler_rt
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        if(_lr_compiler_rt)
            list(APPEND _lr_mingw_crt_libs "${_lr_compiler_rt}")
        endif()
    else()
        list(APPEND _lr_mingw_crt_libs -lgcc -lgcc_eh)
    endif()
    target_link_libraries(vbam_libretro PRIVATE
        -Wl,--start-group
        ${_lr_mingw_crt_libs}
        -Wl,--end-group
        -lkernel32 -luser32 -ladvapi32
    )
    unset(_lr_mingw_crt_libs)

elseif(UNIX AND NOT APPLE)
    # Export only retro_* symbols via version script.
    target_link_options(vbam_libretro PRIVATE
        -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/link.T
        -Wl,--no-undefined
    )
    set_target_properties(vbam_libretro PROPERTIES
        LINK_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/link.T
    )
    if(NOT BUILD_SHARED_LIBS)
        target_link_options(vbam_libretro PRIVATE
            -static-libgcc -static-libstdc++
        )
    endif()

endif()

# Position-independent code is required for shared libraries on Unix.
if(NOT WIN32)
    set_target_properties(vbam_libretro PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()

# --- Upstream release zip ---
if(UPSTREAM_RELEASE)
    if(WIN32)
        set(_lr_os Win)
        set(_lr_ext .dll)
    elseif(APPLE)
        set(_lr_os macOS)
        set(_lr_ext .dylib)
    else()
        set(_lr_os Linux)
        set(_lr_ext .so)
    endif()

    if(CMAKE_BUILD_TYPE STREQUAL Debug)
        set(_lr_zip vbam-libretro-${_lr_os}-${ARCH_NAME}-debug.zip)
    else()
        set(_lr_zip vbam-libretro-${_lr_os}-${ARCH_NAME}.zip)
    endif()

    add_custom_command(
        TARGET vbam_libretro
        POST_BUILD
        COMMAND ${ZIP_PROGRAM} -9 ${_lr_zip} vbam_libretro${_lr_ext}
        WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
    )

    unset(_lr_os)
    unset(_lr_ext)
    unset(_lr_zip)
endif()
