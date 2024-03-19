# Compiler stuff
include(CheckCXXCompilerFlag)

include(ProcessorCount)
ProcessorCount(num_cpus)

if (ENABLE_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT LTO_SUPPORTED)
    if (LTO_SUPPORTED)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
    else()
        message(WARNING "LTO is not supported by the compiler, diasabling LTO")
        set(ENABLE_LTO OFF)
    endif()
endif()

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
