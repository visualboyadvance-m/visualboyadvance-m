if(TRANSLATIONS_ONLY)
    return()
endif()

# Compiler stuff
include(CheckCXXCompilerFlag)

include(ProcessorCount)
ProcessorCount(num_cpus)

if(ENABLE_LTO AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(ENABLE_LTO OFF)
endif()

if(ENABLE_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT LTO_SUPPORTED)

    # MSYS2 toolchains and MinGW do not currently support LTO.
    if(MSYS2 OR CMAKE_CXX_COMPILER MATCHES "[Mm][Ss][Yy][Ss]"
            OR (WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU"))
        set(LTO_SUPPORTED FALSE)
    endif()

    if(LTO_SUPPORTED)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
    else()
        set(ENABLE_LTO OFF)
    endif()
endif()

# Output all binaries at top level
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR})

if(NOT HTTPS)
    add_compile_definitions(NO_HTTPS)
endif()

if(ENABLE_GBA_LOGGING)
    list(APPEND VBAM_FRONTEND_DEFINITIONS GBA_LOGGING)
endif()

if(ENABLE_MMX)
    add_compile_definitions(MMX)
endif()


if(NOT ENABLE_ONLINEUPDATES)
  add_compile_definitions(NO_ONLINEUPDATES)
endif()

# The debugger is enabled by default
if(ENABLE_DEBUGGER)
    list(APPEND VBAM_FRONTEND_DEFINITIONS VBAM_ENABLE_DEBUGGER)
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
add_compile_definitions(HAVE_NETINET_IN_H HAVE_ARPA_INET_H HAVE_ZLIB_H FINAL_VERSION SYSCONF_INSTALL_DIR="${CMAKE_INSTALL_FULL_SYSCONFDIR}")
list(APPEND VBAM_FRONTEND_DEFINITIONS SDL USE_OPENGL)
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
    if(WIN32 AND X86)
        # See the longer note in src/wx/CMakeLists.txt: the download is checked
        # and removed on failure so a truncated file is not mistaken for a good
        # one, and nuget is named by full path because execute_process()
        # resolves a command against PATH rather than WORKING_DIRECTORY.
        if(NOT EXISTS ${CMAKE_BINARY_DIR}/nuget.exe)
            file(DOWNLOAD "https://dist.nuget.org/win-x86-commandline/latest/nuget.exe"
                ${CMAKE_BINARY_DIR}/nuget.exe
                STATUS nuget_download
            )

            list(GET nuget_download 0 nuget_download_code)

            if(NOT nuget_download_code EQUAL 0)
                list(GET nuget_download 1 nuget_download_message)
                file(REMOVE ${CMAKE_BINARY_DIR}/nuget.exe)
                message(FATAL_ERROR "downloading nuget.exe failed: ${nuget_download_message}")
            endif()
        endif()

        # Add the nuget package source, as the Gettext.Tools install in
        # src/wx/CMakeLists.txt does. It cannot be left to that one: this runs
        # first, from the include() in the top-level CMakeLists.txt, and that
        # block only runs at all when xgettext or msginit is missing, so on a
        # machine that has gettext the source would never be added. nuget then
        # has no feed to search and the install fails with "Unable to find
        # package 'nasm2'" and an empty "Feeds used:" list.
        #
        # Left unchecked on purpose: every configure after the first fails it
        # with "the source specified has already been added to the list of
        # available package sources", which is not a problem.
        execute_process(
            COMMAND ${CMAKE_BINARY_DIR}/nuget.exe sources add
                    -Name "NuGet official package source"
                    -Source "https://api.nuget.org/v3/index.json"
            OUTPUT_QUIET
            ERROR_QUIET
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        )

        execute_process(
            COMMAND ${CMAKE_BINARY_DIR}/nuget.exe install nasm2
                    -OutputDirectory ${CMAKE_BINARY_DIR}/nuget
            RESULT_VARIABLE nuget_result
            OUTPUT_VARIABLE nuget_output
            ERROR_VARIABLE  nuget_output
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        )

        if(NOT nuget_result EQUAL 0)
            message(FATAL_ERROR
                "installing the nasm2 nuget package failed (${nuget_result}):
${nuget_output}")
        endif()

        file(GLOB pkg ${CMAKE_BINARY_DIR}/nuget/nasm2*)

        list(APPEND CMAKE_PROGRAM_PATH ${pkg}/tools)
    endif()

    enable_language(ASM_NASM)

    set(ASM_ENABLED ON)
endif()

if(ASM_ENABLED)
    string(REGEX REPLACE "<FLAGS>" "-I${CMAKE_SOURCE_DIR}/src/components/filters/internal/hq/asm/ -O1 -w-orphan-labels" CMAKE_ASM_NASM_COMPILE_OBJECT ${CMAKE_ASM_NASM_COMPILE_OBJECT})
endif()
