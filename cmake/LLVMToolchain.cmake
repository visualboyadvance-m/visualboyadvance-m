function(use_llvm_toolchain)
    if(CMAKE_C_COMPILER_ID STREQUAL Clang)
        set(compiler "${CMAKE_C_COMPILER}")
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL Clang)
        set(compiler "${CMAKE_CXX_COMPILER}")
    else()
        return()
    endif()

    foreach(tool ar ranlib ld nm objdump as)
        execute_process(
            COMMAND "${compiler}" -print-prog-name=llvm-${tool}
            OUTPUT_VARIABLE prog_path
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )

        if(prog_path MATCHES "^/")
            if(tool STREQUAL ld)
                set(tool linker)
            elseif(tool STREQUAL as)
                set(tool asm_compiler)
            endif()

            string(TOUPPER ${tool} utool)

            set(CMAKE_${utool} "${prog_path}" PARENT_SCOPE)
            set(CMAKE_${utool} "${prog_path}" CACHE FILEPATH "${tool}" FORCE)
        endif()
    endforeach()
endfunction()

use_llvm_toolchain()
