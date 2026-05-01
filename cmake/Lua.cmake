# Lua 5.4 — fetched from upstream lua.org and built as a static lib.
#
# Lua's own build system is a bare Makefile; we don't run it. Instead we
# pull the source tarball, compile every `*.c` (except `lua.c` and
# `luac.c` which contain `main()`) into a single static library, and let
# downstream targets link against it.
#
# This file is included from the top-level CMakeLists.txt when
# ENABLE_LUA is ON. It defines the IMPORTED-style target `vbam::lua`
# (alias `vbam-lua`) with the headers exposed as a public include
# directory.

if(NOT ENABLE_LUA)
    return()
endif()

include(FetchContent)

# Pin to a recent 5.4 release. The download URL on lua.org doesn't
# change once published, so this is reproducible without a GitHub
# mirror. SHA256 was taken from https://www.lua.org/ftp/.
set(VBAM_LUA_VERSION 5.4.7)
set(VBAM_LUA_URL  "https://www.lua.org/ftp/lua-${VBAM_LUA_VERSION}.tar.gz")
set(VBAM_LUA_HASH "SHA256=9fbf5e28ef86c69858f6d3d34eccc32e911c1a28b4120ff3e84aaa70cfbf1e30")

FetchContent_Declare(
    vbam_lua
    URL      "${VBAM_LUA_URL}"
    URL_HASH "${VBAM_LUA_HASH}"
)
FetchContent_MakeAvailable(vbam_lua)

# vbam-lua collects every C source under src/ that *isn't* one of the
# stand-alone executables (lua.c → CLI REPL, luac.c → bytecode
# compiler). The remaining ~30 files form the embeddable interpreter.
file(GLOB _vbam_lua_sources CONFIGURE_DEPENDS
    "${vbam_lua_SOURCE_DIR}/src/*.c")
list(REMOVE_ITEM _vbam_lua_sources
    "${vbam_lua_SOURCE_DIR}/src/lua.c"
    "${vbam_lua_SOURCE_DIR}/src/luac.c"
    "${vbam_lua_SOURCE_DIR}/src/onelua.c")

add_library(vbam-lua STATIC ${_vbam_lua_sources})
target_include_directories(vbam-lua PUBLIC "${vbam_lua_SOURCE_DIR}/src")

# Lua expects the host to define LUA_USE_<PLATFORM> so it picks the
# right system facilities (dlopen, popen, signal, etc.). On Linux,
# additionally link against libdl/libm/libreadline as Lua's own
# Makefile does.
if(APPLE)
    target_compile_definitions(vbam-lua PUBLIC LUA_USE_MACOSX)
elseif(UNIX)
    target_compile_definitions(vbam-lua PUBLIC LUA_USE_LINUX)
    target_link_libraries(vbam-lua PUBLIC dl m)
elseif(WIN32)
    target_compile_definitions(vbam-lua PRIVATE LUA_USE_WINDOWS)
endif()

# Lua's source is C89-style and emits a fair bit of "unused parameter"
# / "implicit fallthrough" / "format" output under -Wall — silence
# those for the vendored sources only so the rest of the project
# keeps -Werror clean.
if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(vbam-lua PRIVATE
        -w        # disable all warnings for vendored Lua
        -fPIC)
elseif(MSVC)
    target_compile_options(vbam-lua PRIVATE /W0 /wd4244)
endif()

add_library(vbam::lua ALIAS vbam-lua)
