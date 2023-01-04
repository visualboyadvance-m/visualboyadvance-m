
# Compiler stuff
include(SetCompilerLinkerFlags)

include(ProcessorCount)
ProcessorCount(num_cpus)

if(CMAKE_C_COMPILER_ID STREQUAL Clang AND CMAKE_CXX_COMPILER_ID STREQUAL Clang AND NOT MSVC)
    # TODO: This should also be done for clang-cl.
    include(LLVMToolchain)
endif()

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID STREQUAL Clang AND NOT MSVC)
    unset(LTO_FLAGS)
    if(ENABLE_LTO)
        if(CMAKE_COMPILER_IS_GNUCXX)
            if(num_cpus GREATER 1)
                set(LTO_FLAGS -flto=${num_cpus} -ffat-lto-objects)
            else()
                set(LTO_FLAGS -flto -ffat-lto-objects)
            endif()
        else()
            set(LTO_FLAGS -flto)
        endif()

        # check that LTO is not broken before enabling it
        set(ORIG_CMAKE_REQUIRED_FLAGS ${CMAKE_REQUIRED_FLAGS})
        set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} ${LTO_FLAGS}")

        include(CheckCXXSourceCompiles)
        check_cxx_source_compiles("int main(int argc, char** argv) { return 0; }" LTO_WORKS)

        set(CMAKE_REQUIRED_FLAGS ${ORIG_CMAKE_REQUIRED_FLAGS})

        if(NOT LTO_WORKS)
            message(WARNING "LTO does not seem to be working on your system, if using clang make sure LLVMGold is installed")
            unset(LTO_FLAGS)
            set(ENABLE_LTO OFF)
        endif()
    endif()

    unset(MY_C_OPT_FLAGS)

    if(X86_32 OR X86_64)
        set(MY_C_OPT_FLAGS -mfpmath=sse -msse2)
    endif()

    # common optimization flags
    if(NOT (APPLE AND CMAKE_CXX_COMPILER_ID STREQUAL Clang AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 4.3))
        set(MY_C_OPT_FLAGS ${MY_C_OPT_FLAGS} -Ofast -fomit-frame-pointer ${LTO_FLAGS})
    else()
        # LTO and -fomit-frame-pointer generate broken binaries on Lion with XCode 4.2 tools
        set(MY_C_OPT_FLAGS ${MY_C_OPT_FLAGS} -Ofast)
    endif()

    # Common flags.
    set(MY_C_FLAGS -pipe -Wno-unused-command-line-argument -Wformat -Wformat-security -feliminate-unused-debug-types)

    include(CheckCXXCompilerFlag)

    # Require and optimize for Core2 level support, tune for generic.
    if(X86_64)
        set(MY_C_FLAGS ${MY_C_FLAGS} -march=core2 -mtune=generic)
    # Optimize for pentium-mmx and tune for generic for older XP builds.
    elseif(X86_32)
        set(MY_C_FLAGS ${MY_C_FLAGS} -march=pentium-mmx -mtune=generic)
    endif()

    # Check for -fopenmp=libomp on clang.
    #    if(CMAKE_CXX_COMPILER_ID STREQUAL Clang)
    #        check_cxx_compiler_flag("-fopenmp=libomp" FOPENMP_LIBOMP_FLAG)
    #        if(FOPENMP_LIBOMP_FLAG)
    #            set(MY_C_FLAGS ${MY_C_FLAGS} -fopenmp=libomp)
    #        endif()
    #    endif()

    # common debug flags
    if(CMAKE_COMPILER_IS_GNUCXX)
        set(MY_C_DBG_FLAGS -ggdb3 -Og -fno-omit-frame-pointer)
    else()
        set(MY_C_DBG_FLAGS -g -fno-omit-frame-pointer)
    endif()

    if(ENABLE_ASAN)
        string(TOLOWER ${CMAKE_BUILD_TYPE} build)
        if(NOT build STREQUAL "debug")
            message(FATAL_ERROR "asan requires debug build, set -DCMAKE_BUILD_TYPE=Debug")
        endif()

        string(TOLOWER ${ENABLE_ASAN} SANITIZER)
        if(SANITIZER STREQUAL "on" OR SANITIZER STREQUAL "true")
            set(SANITIZER address)
        endif()
        list(PREPEND CMAKE_REQUIRED_LIBRARIES -fsanitize=${SANITIZER})
        check_cxx_compiler_flag("-fsanitize=${SANITIZER}" ASAN_SUPPORT_FLAG)
        if(${ASAN_SUPPORT_FLAG})
            list(PREPEND MY_C_DBG_FLAGS -fsanitize=${SANITIZER})
        else()
            message(FATAL_ERROR "asan not available to compiler.")
        endif()
    endif()

    if(ENABLE_SSP AND CMAKE_BUILD_TYPE STREQUAL Debug)
        if(WIN32)
            set(SSP_STATIC ON)
        endif()

        find_package(SSP)

        if(SSP_LIBRARY)
            list(APPEND MY_C_FLAGS        -D_FORTIFY_SOURCE=2)
            list(APPEND MY_C_LINKER_FLAGS ${SSP_LIBRARY})
        endif()
    endif()

    if(NOT (WIN32 OR X86_32)) # inline asm is not allowed with -fPIC
        set(MY_C_FLAGS ${MY_C_FLAGS} -fPIC)
    endif()

    # check if ssp flags are supported for this version of gcc
    if(CMAKE_COMPILER_IS_GNUCXX)
        if(ENABLE_SSP)
            check_cxx_compiler_flag(-fstack-protector-strong  F_STACK_PROTECTOR_STRONG_FLAG)

            if(F_STACK_PROTECTOR_STRONG_FLAG)
                set(MY_C_FLAGS ${MY_C_FLAGS} -fstack-protector-strong)

                check_cxx_compiler_flag("--param ssp-buffer-size=4" SSP_BUFFER_SIZE_FLAG)

                if(SSP_BUFFER_SIZE_FLAG)
                    # we do not add it to MY_C_FLAGS because this breaks things like CMAKE_REQUIRED_LIBRARIES
                    # which misinterpret compiler flags without leading dashes
                    add_compile_options(--param ssp-buffer-size=4)
                endif()
            endif()
        endif()

        set(MY_C_FLAGS ${MY_C_FLAGS} -fopenmp)
    endif()

    if(CMAKE_CXX_COMPILER_ID STREQUAL GNU)
        unset(COMPILER_COLOR_DIAGNOSTICS)
        check_cxx_compiler_flag(-fdiagnostics-color=always COMPILER_COLOR_DIAGNOSTICS)
        if(COMPILER_COLOR_DIAGNOSTICS)
            add_compile_options(-fdiagnostics-color=always)
        else()
            unset(COMPILER_COLOR_DIAGNOSTICS)
            check_cxx_compiler_flag(-fdiagnostics-color COMPILER_COLOR_DIAGNOSTICS)
            if(COMPILER_COLOR_DIAGNOSTICS)
                add_compile_options(-fdiagnostics-color)
            endif()
        endif()
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL Clang)
        unset(COMPILER_COLOR_DIAGNOSTICS)
        check_cxx_compiler_flag(-fcolor-diagnostics COMPILER_COLOR_DIAGNOSTICS)
        if(COMPILER_COLOR_DIAGNOSTICS)
            add_compile_options(-fcolor-diagnostics)
        endif()
    endif()

    if(MINGW)
        set(MY_C_FLAGS ${MY_C_FLAGS} -static-libgcc -static-libstdc++)
    endif()

    if(CMAKE_BUILD_TYPE STREQUAL Debug)
        set(MY_C_FLAGS   ${MY_C_FLAGS}   ${MY_C_DBG_FLAGS} -Wall -Wextra)
    else()
        set(MY_C_FLAGS   ${MY_C_FLAGS}   ${MY_C_OPT_FLAGS} -Wno-error)
    endif()

    # for some reason this is necessary
    if(CMAKE_SYSTEM_NAME STREQUAL FreeBSD)
        set(MY_C_FLAGS -I/usr/local/include ${MY_C_FLAGS})
    endif()

    foreach(C_COMPILE_FLAG ${MY_C_FLAGS})
        add_compile_options(${C_COMPILE_FLAG})
    endforeach()

    include(CheckCXXCompilerFlag)

    check_cxx_compiler_flag(-std=gnu++17 GNUPP17_FLAG)

    if(NOT GNUPP17_FLAG)
        message(FATAL_ERROR "Your compiler does not support -std=gnu++17.")
    endif()

    set(MY_CXX_FLAGS -std=gnu++17 -fexceptions)

    foreach(ARG ${MY_CXX_FLAGS})
        set(MY_CXX_FLAGS_STR "${MY_CXX_FLAGS_STR} ${ARG}")
    endforeach()

    # These must be set for C++ only, and we can't use generator expressions in
    # ADD_COMPILE_OPTIONS because that's a cmake 3.3 feature and we need 2.8.12
    # compat for Ubuntu 14.
    string(REGEX REPLACE "<FLAGS>" "<FLAGS> ${MY_CXX_FLAGS_STR} " CMAKE_CXX_COMPILE_OBJECT ${CMAKE_CXX_COMPILE_OBJECT})

    foreach(ARG ${MY_C_FLAGS})
        set(MY_C_FLAGS_STR "${MY_C_FLAGS_STR} ${ARG}")
    endforeach()

    # need all flags for linking, because of -flto etc.
    set(CMAKE_C_LINK_EXECUTABLE   "${CMAKE_C_LINK_EXECUTABLE}   ${MY_C_FLAGS_STR}")
    set(CMAKE_CXX_LINK_EXECUTABLE "${CMAKE_CXX_LINK_EXECUTABLE} ${MY_C_FLAGS_STR}")

    # for the gcc -fstack-protector* flags we need libssp
    # we also have to use the gcc- binutils for LTO to work
    if(CMAKE_COMPILER_IS_GNUCXX)
        if(ENABLE_LTO)
            include(UseGCCBinUtilsWrappers)
        endif()

        set(MY_C_LINKER_FLAGS ${MY_C_LINKER_FLAGS} -Wl,-allow-multiple-definition)

        if(CMAKE_PREFIX_PATH)
            list(GET CMAKE_PREFIX_PATH 0 prefix_path_first)
            set(MY_C_LINKER_FLAGS ${MY_C_LINKER_FLAGS} "-Wl,-rpath-link=${prefix_path_first}/lib")
        endif()
    endif()

    # set linker flags
    foreach(ARG ${MY_C_LINKER_FLAGS})
        set(MY_C_LINKER_FLAGS_STR "${MY_C_LINKER_FLAGS_STR} ${ARG}")
    endforeach()

    set(CMAKE_C_LINK_EXECUTABLE   "${CMAKE_C_LINK_EXECUTABLE}   ${MY_C_LINKER_FLAGS_STR}")
    set(CMAKE_CXX_LINK_EXECUTABLE "${CMAKE_CXX_LINK_EXECUTABLE} ${MY_C_LINKER_FLAGS_STR}")
elseif(MSVC)
    # first remove all warnings flags, otherwise there is a warning about overriding them
    string(REGEX REPLACE "/[Ww][^ ]+" "" CMAKE_C_FLAGS   ${CMAKE_C_FLAGS})
    string(REGEX REPLACE "/[Ww][^ ]+" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})

    add_compiler_flags(/std:c++17 -D__STDC_LIMIT_MACROS /fp:fast /Oi)

    if(VBAM_STATIC_CRT)
        set(runtime "/MT")
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:DEBUG>:Debug>" CACHE INTERNAL "")
    else()
        set(runtime "/MD")
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:DEBUG>:Debug>DLL" CACHE INTERNAL "")
    endif()

    if(CMAKE_BUILD_TYPE STREQUAL Debug)
        set(runtime "${runtime}d")

        add_compiler_flags(${runtime} /ZI /W4 /Ob0 /Od /RTC1 /DDEBUG /EHsc)
    else()
        add_compiler_flags(/w /DNDEBUG /EHsc)

        if(CMAKE_BUILD_TYPE STREQUAL Release)
            if(X86_32)
                add_compiler_flags(${runtime} /O2 /Ob3)
            else()
                add_compiler_flags(${runtime} /O2 /Ob3)
            endif()
        elseif(CMAKE_BUILD_TYPE STREQUAL RelWithDebInfo)
            add_compiler_flags(${runtime} /Zi /Ob1)
        elseif(CMAKE_BUILD_TYPE STREQUAL MinSizeRel)
            add_compiler_flags(${runtime} /O1 /Ob1)
        else()
            message(FATAL_ERROR "Unknown CMAKE_BUILD_TYPE: '${CMAKE_BUILD_TYPE}'")
        endif()

        if(ENABLE_LTO)
            add_compiler_flags(/GL)
            add_linker_flags(/LTCG)
        endif()
    endif()
endif()

# Assembler flags

if(ASM_ENABLED)
    string(REGEX REPLACE "<FLAGS>" "-I${CMAKE_SOURCE_DIR}/src/filters/hq/asm/ -O1 -w-orphan-labels" CMAKE_ASM_NASM_COMPILE_OBJECT ${CMAKE_ASM_NASM_COMPILE_OBJECT})
endif()