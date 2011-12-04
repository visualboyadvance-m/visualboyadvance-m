cmake_minimum_required(VERSION 2.4.3)
if(UNIX AND NOT DEFINED CMAKE_BUILD_TYPE)
   set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "Choose the type of
   build, options are: None(CMAKE_CXX_FLAGS or CMAKE_C_FLAGS used) Debug
   Release RelWithDebInfo MinSizeRel.")
endif()

project(PNG C)
enable_testing()

# Copyright (C) 2007-2010 Glenn Randers-Pehrson

# This code is released under the libpng license.
# For conditions of distribution and use, see the disclaimer
# and license in png.h

set(PNGLIB_MAJOR 1)
set(PNGLIB_MINOR 4)
set(PNGLIB_RELEASE 0)
set(PNGLIB_NAME libpng${PNGLIB_MAJOR}${PNGLIB_MINOR})
set(PNGLIB_VERSION ${PNGLIB_MAJOR}.${PNGLIB_MINOR}.${PNGLIB_RELEASE})

set(CMAKE_ALLOW_LOOSE_LOOP_CONSTRUCTS true)

# needed packages
find_package(ZLIB REQUIRED)
include_directories(${ZLIB_INCLUDE_DIR})

if(NOT WIN32)
 find_library(M_LIBRARY
     NAMES m
     PATHS /usr/lib /usr/local/lib
 )
 if(NOT M_LIBRARY)
   message(STATUS
     "math library 'libm' not found - floating point support disabled")
 endif()
else()
 # not needed on windows
 set(M_LIBRARY "")
endif()

# COMMAND LINE OPTIONS
if(DEFINED PNG_SHARED)
  option(PNG_SHARED "Build shared lib" ${PNG_SHARED})
else()
  option(PNG_SHARED "Build shared lib" ON)
endif()
if(DEFINED PNG_STATIC)
  option(PNG_STATIC "Build static lib" ${PNG_STATIC})
else()
  option(PNG_STATIC "Build static lib" ON)
endif()

if(MINGW)
  option(PNG_TESTS  "Build pngtest" NO)
else(MINGW)
  option(PNG_TESTS  "Build pngtest" YES)
endif(MINGW)

option(PNG_NO_CONSOLE_IO "FIXME" YES)
option(PNG_NO_STDIO      "FIXME" YES)
option(PNG_DEBUG         "Build with debug output" NO)
option(PNGARG            "FIXME" YES)
#TODO:
# PNG_CONSOLE_IO_SUPPORTED

# maybe needs improving, but currently I don't know when we can enable what :)
set(png_asm_tmp "OFF")
if(NOT WIN32)
 find_program(uname_executable NAMES uname PATHS /bin /usr/bin /usr/local/bin)
 if(uname_executable)
   EXEC_PROGRAM(${uname_executable} ARGS --machine OUTPUT_VARIABLE uname_output)
   if("uname_output" MATCHES "^.*i[1-9]86.*$")
      set(png_asm_tmp "ON")
   else("uname_output" MATCHES "^.*i[1-9]86.*$")
      set(png_asm_tmp "OFF")
   endif("uname_output" MATCHES "^.*i[1-9]86.*$")
 endif(uname_executable)
else()
 # this env var is normally only set on win64
 SET(TEXT "ProgramFiles(x86)")
 if("$ENV{${TEXT}}" STREQUAL "")
  set(png_asm_tmp "ON")
 endif("$ENV{${TEXT}}" STREQUAL "")
endif()

# SET LIBNAME
set(PNG_LIB_NAME png${PNGLIB_MAJOR}${PNGLIB_MINOR})

# to distinguish between debug and release lib
set(CMAKE_DEBUG_POSTFIX "d")


# OUR SOURCES
set(libpng_sources
 png.h
 pngconf.h
 pngpriv.h
 png.c
 pngerror.c
 pngget.c
 pngmem.c
 pngpread.c
 pngread.c
 pngrio.c
 pngrtran.c
 pngrutil.c
 pngset.c
 pngtrans.c
 pngwio.c
 pngwrite.c
 pngwtran.c
 pngwutil.c
)
set(pngtest_sources
       pngtest.c
)
# SOME NEEDED DEFINITIONS

add_definitions(-DPNG_CONFIGURE_LIBPNG)

if(MSVC)
  add_definitions(-DPNG_NO_MODULEDEF -D_CRT_SECURE_NO_DEPRECATE)
endif(MSVC)

if(PNG_SHARED OR  NOT MSVC)
	#if building msvc static this has NOT to be defined
	add_definitions(-DZLIB_DLL)
endif()

if(PNG_CONSOLE_IO_SUPPORTED)
 add_definitions(-DPNG_CONSOLE_IO_SUPPORTED)
endif()

if(PNG_NO_CONSOLE_IO)
 add_definitions(-DPNG_NO_CONSOLE_IO)
endif()

if(PNG_NO_STDIO)
 add_definitions(-DPNG_NO_STDIO)
endif()

if(PNG_DEBUG)
 add_definitions(-DPNG_DEBUG)
endif()

if(NOT M_LIBRARY AND NOT WIN32)
 add_definitions(-DPNG_NO_FLOATING_POINT_SUPPORTED)
endif()

# NOW BUILD OUR TARGET
include_directories(${PNG_SOURCE_DIR} ${ZLIB_INCLUDE_DIR})

if(PNG_SHARED)
 add_library(${PNG_LIB_NAME} SHARED ${libpng_sources})
 if(MSVC)
   # msvc does not append 'lib' - do it here to have consistent name
   set_target_properties(${PNG_LIB_NAME} PROPERTIES PREFIX "lib")
 endif()
 target_link_libraries(${PNG_LIB_NAME} ${ZLIB_LIBRARY} ${M_LIBRARY})
endif()

if(PNG_STATIC)
# does not work without changing name
 set(PNG_LIB_NAME_STATIC ${PNG_LIB_NAME}_static)
 add_library(${PNG_LIB_NAME_STATIC} STATIC ${libpng_sources})
 if(MSVC)
   # msvc does not append 'lib' - do it here to have consistent name
   set_target_properties(${PNG_LIB_NAME_STATIC} PROPERTIES PREFIX "lib")
 endif()
endif()


if(PNG_SHARED AND WIN32)
 set_target_properties(${PNG_LIB_NAME} PROPERTIES DEFINE_SYMBOL PNG_BUILD_DLL)
endif()

if(PNG_TESTS AND PNG_SHARED)
# does not work with msvc due to png_lib_ver issue
 add_executable(pngtest ${pngtest_sources})
 target_link_libraries(pngtest ${PNG_LIB_NAME})
 add_test(pngtest pngtest ${PNG_SOURCE_DIR}/pngtest.png)
endif()


# CREATE PKGCONFIG FILES
# we use the same files like ./configure, so we have to set its vars
set(prefix      ${CMAKE_INSTALL_PREFIX})
set(exec_prefix ${CMAKE_INSTALL_PREFIX})
set(libdir      ${CMAKE_INSTALL_PREFIX}/lib)
set(includedir  ${CMAKE_INSTALL_PREFIX}/include)

configure_file(${PNG_SOURCE_DIR}/libpng.pc.in
  ${PNG_BINARY_DIR}/libpng.pc)
configure_file(${PNG_SOURCE_DIR}/libpng-config.in
  ${PNG_BINARY_DIR}/libpng-config)
configure_file(${PNG_SOURCE_DIR}/libpng.pc.in
  ${PNG_BINARY_DIR}/${PNGLIB_NAME}.pc)
configure_file(${PNG_SOURCE_DIR}/libpng-config.in
  ${PNG_BINARY_DIR}/${PNGLIB_NAME}-config)

# SET UP LINKS
if(PNG_SHARED)
  set_target_properties(${PNG_LIB_NAME} PROPERTIES
#    VERSION 14.${PNGLIB_RELEASE}.1.4.0
     VERSION 14.${PNGLIB_RELEASE}.0
     SOVERSION 14
     CLEAN_DIRECT_OUTPUT 1)
endif()
if(PNG_STATIC)
  if(NOT WIN32)
    # that's uncool on win32 - it overwrites our static import lib...
    set_target_properties(${PNG_LIB_NAME_STATIC} PROPERTIES
       OUTPUT_NAME ${PNG_LIB_NAME}
       CLEAN_DIRECT_OUTPUT 1)
  endif()
endif()

# INSTALL
if(NOT SKIP_INSTALL_LIBRARIES AND NOT SKIP_INSTALL_ALL )
    if(PNG_SHARED)
        install(TARGETS ${PNG_LIB_NAME}
            RUNTIME DESTINATION bin
            LIBRARY DESTINATION lib
            ARCHIVE DESTINATION lib)
    endif()
    if(PNG_STATIC)
        install(TARGETS ${PNG_LIB_NAME_STATIC}
            LIBRARY DESTINATION lib
            ARCHIVE DESTINATION lib)
    endif()
endif()

if(NOT SKIP_INSTALL_HEADERS AND NOT SKIP_INSTALL_ALL )
    install(FILES png.h pngconf.h pngpriv.h         DESTINATION include)
    install(FILES png.h pngconf.h pngpriv.h         DESTINATION include/${PNGLIB_NAME})
endif()
if(NOT SKIP_INSTALL_EXECUTABLES AND NOT SKIP_INSTALL_ALL )
    install(PROGRAMS ${PNG_BINARY_DIR}/libpng-config      DESTINATION bin)
    install(PROGRAMS ${PNG_BINARY_DIR}/${PNGLIB_NAME}-config DESTINATION bin)
endif()
if(NOT SKIP_INSTALL_FILES AND NOT SKIP_INSTALL_ALL )
    install(FILES libpng.3 libpngpf.3             DESTINATION man/man3)
    install(FILES png.5                           DESTINATION man/man5)
    install(FILES ${PNG_BINARY_DIR}/libpng.pc     DESTINATION lib/pkgconfig)
    install(FILES ${PNG_BINARY_DIR}/libpng-config      DESTINATION bin)
    install(FILES ${PNG_BINARY_DIR}/${PNGLIB_NAME}.pc  DESTINATION lib/pkgconfig)
    install(FILES ${PNG_BINARY_DIR}/${PNGLIB_NAME}-config DESTINATION bin)
endif()

# what's with libpng.txt and all the extra files?


# UNINSTALL
# do we need this?


# DIST
# do we need this?

# to create msvc import lib for mingw compiled shared lib
# pexports libpng.dll > libpng.def
# lib /def:libpng.def /machine:x86

