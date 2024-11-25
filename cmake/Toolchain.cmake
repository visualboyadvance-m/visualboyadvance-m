if(TRANSLATIONS_ONLY)
    return()
endif()

# Compiler stuff
include(CheckCXXCompilerFlag)

include(ProcessorCount)
ProcessorCount(num_cpus)

if(ENABLE_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT LTO_SUPPORTED)

    # MINGW64 does not support LTO
    if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set(LTO_SUPPORTED FALSE)
    endif()

    if(LTO_SUPPORTED)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
    else()
        message(WARNING "LTO is not supported by the compiler, diasabling LTO")
        set(ENABLE_LTO OFF)
    endif()
endif()

# Output all binaries at top level
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR})

if(NOT HTTPS)
    list(APPEND VBAM_COMPILE_DEFS NO_HTTPS)
endif()

if(ENABLE_GBA_LOGGING)
    list(APPEND VBAM_COMPILE_DEFS GBA_LOGGING )
endif()

if(ENABLE_MMX)
    list(APPEND VBAM_COMPILE_DEFS MMX)
endif()


if(NOT ENABLE_ONLINEUPDATES)
  list(APPEND VBAM_COMPILE_DEFS NO_ONLINEUPDATES)
endif()

# The debugger is enabled by default
if(ENABLE_DEBUGGER)
    list(APPEND VBAM_COMPILE_DEFS VBAM_ENABLE_DEBUGGER)
endif()

# The ASM core is disabled by default because we don't know on which platform we are
if(NOT ENABLE_ASM_CORE)
    list(APPEND VBAM_COMPILE_DEFS C_CORE)
endif()

# Set up "src" and generated directory as a global include directory.
set(VBAM_GENERATED_DIR ${CMAKE_BINARY_DIR}/generated)
include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${VBAM_GENERATED_DIR}
)

# C defines
list(APPEND VBAM_COMPILE_DEFS HAVE_NETINET_IN_H HAVE_ARPA_INET_H HAVE_ZLIB_H FINAL_VERSION SDL USE_OPENGL SYSCONF_INSTALL_DIR="${CMAKE_INSTALL_FULL_SYSCONFDIR}")
list(APPEND VBAM_COMPILE_DEFS PKGDATADIR="${CMAKE_INSTALL_FULL_DATADIR}/vbam")
list(APPEND VBAM_COMPILE_DEFS __STDC_FORMAT_MACROS)
list(APPEND VBAM_COMPILE_DEFS LOCALEDIR="${LOCALEDIR}")

if(APPLE)
    list(APPEND VBAM_COMPILE_DEFS MACHO)
elseif("${CMAKE_SYSTEM}" MATCHES "Linux")
    list(APPEND VBAM_COMPILE_DEFS ELF)
endif()

if(X86_64)
    list(APPEND VBAM_COMPILE_DEFS __AMD64__ __X86_64__)
endif()

# Enable ASAN if requested and supported.
include(Toolchain-asan)

# MINGW/MSYS-specific settings.
include(Toolchain-mingw)

# Toolchain-specific settings.
if(MSVC)
    # This also includes clang-cl.
    include(Toolchain-msvc)
else()
    include(Toolchain-gcc-clang)
endif()

# Assembler flags.
if(ENABLE_ASM_CORE OR ENABLE_ASM_SCALERS)
    if(MSVC)
        if(NOT EXISTS ${CMAKE_BINARY_DIR}/nuget.exe)
            file(DOWNLOAD "https://dist.nuget.org/win-x86-commandline/latest/nuget.exe" ${CMAKE_BINARY_DIR}/nuget.exe)
        endif()

        execute_process(
            COMMAND nuget.exe install nasm2 -OutputDirectory ${CMAKE_BINARY_DIR}/nuget
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        )

        file(GLOB pkg ${CMAKE_BINARY_DIR}/nuget/nasm2*)

        list(APPEND CMAKE_PROGRAM_PATH ${pkg}/tools)
    endif()

    enable_language(ASM_NASM)

    set(ASM_ENABLED ON)
endif()

if(ASM_ENABLED)
    string(REGEX REPLACE "<FLAGS>" "-I${CMAKE_SOURCE_DIR}/src/filters/hq/asm/ -O1 -w-orphan-labels" CMAKE_ASM_NASM_COMPILE_OBJECT ${CMAKE_ASM_NASM_COMPILE_OBJECT})
endif()
