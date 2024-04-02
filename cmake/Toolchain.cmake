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

    # MINGW64 on x64 does not support LTO
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
    add_compile_definitions(NO_HTTPS)
endif()

if(ENABLE_GBA_LOGGING)
    add_compile_definitions(GBA_LOGGING )
endif()

if(ENABLE_MMX)
    add_compile_definitions(MMX)
endif()


if(NOT ENABLE_ONLINEUPDATES)
  add_compile_definitions(NO_ONLINEUPDATES)
endif()

# The debugger is enabled by default
if(ENABLE_DEBUGGER)
    add_compile_definitions(VBAM_ENABLE_DEBUGGER)
endif()

# The ASM core is disabled by default because we don't know on which platform we are
if(NOT ENABLE_ASM_CORE)
    add_compile_definitions(C_CORE)
endif()

# Set up "src" and generated directory as a global include directory.
set(VBAM_GENERATED_DIR ${CMAKE_BINARY_DIR}/generated)
include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${VBAM_GENERATED_DIR}
)

# C defines
add_compile_definitions(HAVE_NETINET_IN_H HAVE_ARPA_INET_H HAVE_ZLIB_H FINAL_VERSION SDL USE_OPENGL SYSCONF_INSTALL_DIR="${CMAKE_INSTALL_FULL_SYSCONFDIR}")
add_compile_definitions(PKGDATADIR="${CMAKE_INSTALL_FULL_DATADIR}/vbam")
add_compile_definitions(__STDC_FORMAT_MACROS)
add_compile_definitions(LOCALEDIR="${LOCALEDIR}")

# Common compiler settings.
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_definitions(DEBUG)
else()
    add_compile_definitions(NDEBUG)
endif()

if(APPLE)
    add_compile_definitions(MACHO)
elseif("${CMAKE_SYSTEM}" MATCHES "Linux")
    add_compile_definitions(ELF)
endif()

if(X86_64)
    add_compile_definitions(__AMD64__ __X86_64__)
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
