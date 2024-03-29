if(NOT ENABLE_ASAN)
    return()
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    check_cxx_compiler_flag(/fsanitize=address MSVC_ASAN_SUPPORTED)
    if(MSVC_ASAN_SUPPORTED)
        add_compile_options(/fsanitize=address)
        add_compile_definitions(_DISABLE_VECTOR_ANNOTATION _DISABLE_STRING_ANNOTATION)
    else()
        message(WARNING "ASAN not available for the compiler, disabling.")
        set(ENABLE_ASAN OFF)
        return()
    endif()
else()
    # ASAN does not work on non-debug builds for GCC and Clang.
    if(NOT CMAKE_BUILD_TYPE STREQUAL Debug)
        message(WARNING "ASAN requires debug build, set -DCMAKE_BUILD_TYPE=Debug, disabling.")
        set(ENABLE_ASAN OFF)
        return()
    endif()

    # It is necessary to modify the linker flagas for the compiler check to work.
    set(BACKUP_LINKER_FLAGS ${CMAKE_EXE_LINKER_FLAGS})
    set(CMAKE_EXE_LINKER_FLAGS "-fsanitize=address")
    check_cxx_compiler_flag(-fsanitize=address ASAN_SUPPORTED)
    set(CMAKE_EXE_LINKER_FLAGS ${BACKUP_LINKER_FLAGS})

    if(ASAN_SUPPORTED)
        add_compile_options(-fsanitize=address)
        add_link_options(-fsanitize=address)
    else()
        message(WARNING "ASAN not available for the compiler, disabling.")
        set(ENABLE_ASAN OFF)
        return()
    endif()
endif()
