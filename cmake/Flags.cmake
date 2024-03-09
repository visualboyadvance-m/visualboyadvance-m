# Compiler stuff
include(ProcessorCount)
ProcessorCount(num_cpus)

if(CMAKE_C_COMPILER_ID STREQUAL Clang AND CMAKE_CXX_COMPILER_ID STREQUAL Clang AND NOT MSVC)
    # TODO: This should also be done for clang-cl.
    include(LLVMToolchain)
endif()

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID STREQUAL Clang AND NOT MSVC)
    include(Toolchain-gcc-clang)
elseif(MSVC)
    include(Toolchain-msvc)
elseif(MINGW)
    include(Toolchain-mingw)
endif()

# Assembler flags
if(ASM_ENABLED)
    string(REGEX REPLACE "<FLAGS>" "-I${CMAKE_SOURCE_DIR}/src/filters/hq/asm/ -O1 -w-orphan-labels" CMAKE_ASM_NASM_COMPILE_OBJECT ${CMAKE_ASM_NASM_COMPILE_OBJECT})
endif()
