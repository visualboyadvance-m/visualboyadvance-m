# FindSSP.cmake
#
# Find libssp necessary when using gcc with e.g. -fstack-protector=strong
#
# See: http://wiki.osdev.org/Stack_Smashing_Protector
#
# To use:
#
# put a copy into your <project_root>/cmake/
#
# In your main CMakeLists.txt do something like this:
#
# if(WIN32)
#     set(SSP_STATIC ON)
# endif()
#
# find_package(SSP)
#
# if(SSP_LIBRARY)
#     set(CMAKE_CXX_LINK_EXECUTABLE "${CMAKE_CXX_LINK_EXECUTABLE} ${SSP_LIBRARY}")
#     set(CMAKE_C_LINK_EXECUTABLE   "${CMAKE_C_LINK_EXECUTABLE}   ${SSP_LIBRARY}")
# endif()

# only do this when compiling with gcc/g++
if(NOT CMAKE_COMPILER_IS_GNUCXX)
    return()
endif()

function(FindSSP)
    if(NOT CMAKE_CXX_COMPILER AND CMAKE_C_COMPILER)
        set(CMAKE_CXX_COMPILER ${CMAKE_C_COMPILER})
    endif()

    foreach(arg ${CMAKE_CXX_COMPILER_ARG1} ${CMAKE_CXX_COMPILER_ARG2} ${CMAKE_CXX_COMPILER_ARG3} ${CMAKE_CXX_COMPILER_ARG4} ${CMAKE_CXX_COMPILER_ARG5} ${CMAKE_CXX_COMPILER_ARG6} ${CMAKE_CXX_COMPILER_ARG7} ${CMAKE_CXX_COMPILER_ARG8} ${CMAKE_CXX_COMPILER_ARG9})
        string(STRIP ${arg} arg)
        set(gcc_args "${gcc_args};${arg}")
    endforeach()

    execute_process(COMMAND ${CMAKE_CXX_COMPILER} ${gcc_args} --print-prog-name=gcc OUTPUT_VARIABLE GCC_EXECUTABLE OUTPUT_STRIP_TRAILING_WHITESPACE)

    if(WIN32 AND NOT MSYS)
        execute_process(COMMAND where.exe         ${GCC_EXECUTABLE}  OUTPUT_VARIABLE GCC_EXECUTABLE OUTPUT_STRIP_TRAILING_WHITESPACE)
    else()
        execute_process(COMMAND sh -c "command -v ${GCC_EXECUTABLE}" OUTPUT_VARIABLE GCC_EXECUTABLE OUTPUT_STRIP_TRAILING_WHITESPACE)
    endif()

    get_filename_component(GCC_DIRNAME "${GCC_EXECUTABLE}" DIRECTORY)

    execute_process(COMMAND ${CMAKE_CXX_COMPILER} ${gcc_args} --print-libgcc-file-name OUTPUT_VARIABLE LIBGCC_FILE OUTPUT_STRIP_TRAILING_WHITESPACE)

    get_filename_component(LIBGCC_DIRNAME "${LIBGCC_FILE}" DIRECTORY)

    set(SSP_SEARCH_PATHS ${GCC_DIRNAME} ${LIBGCC_DIRNAME})

    if(SSP_STATIC)
        if(WIN32)
            set(CMAKE_FIND_LIBRARY_SUFFIXES .lib .a)
        else()
            set(CMAKE_FIND_LIBRARY_SUFFIXES .a)
        endif()
    endif()

    find_library(SSP_LIBRARY
        NAMES ssp libssp
        HINTS ${SSP_SEARCH_PATHS}
        PATH_SUFFIXES lib64 lib lib/x64 lib/x86
    )

    set(SSP_LIBRARY PARENT_SCOPE)
endfunction()

FindSSP()

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(SSP REQUIRED_VARS SSP_LIBRARY)
