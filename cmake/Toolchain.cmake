# Compiler stuff
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

if(CMAKE_C_COMPILER_ID STREQUAL Clang AND CMAKE_CXX_COMPILER_ID STREQUAL Clang AND NOT MSVC)
    # TODO: This should also be done for clang-cl.
    include(Toolchain-llvm)
endif()

if(MSVC)
    # This also includes clang-cl.
    include(Toolchain-msvc)
elseif(MINGW)
    include(Toolchain-mingw)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL GNU OR CMAKE_CXX_COMPILER_ID STREQUAL Clang)
    include(Toolchain-gcc-clang)
else()
    message(FATAL_ERROR "Unsupported compiler")
endif()

# Assembler flags
if(ASM_ENABLED)
    string(REGEX REPLACE "<FLAGS>" "-I${CMAKE_SOURCE_DIR}/src/filters/hq/asm/ -O1 -w-orphan-labels" CMAKE_ASM_NASM_COMPILE_OBJECT ${CMAKE_ASM_NASM_COMPILE_OBJECT})
endif()
