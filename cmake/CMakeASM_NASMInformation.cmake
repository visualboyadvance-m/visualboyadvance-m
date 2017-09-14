
#=============================================================================
# Copyright 2010 Kitware, Inc.
#
# Distributed under the OSI-approved BSD License (the "License");
# see accompanying file Copyright.txt for details.
#
# This software is distributed WITHOUT ANY WARRANTY; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the License for more information.
#=============================================================================
# (To distribute this file outside of CMake, substitute the full
#  License text for the above reference.)

# support for the nasm assembler

set(CMAKE_ASM_NASM_SOURCE_FILE_EXTENSIONS nasm asm)

if(NOT CMAKE_ASM_NASM_OBJECT_FORMAT)
  if(WIN32)
    if(CMAKE_C_SIZEOF_DATA_PTR EQUAL 8)
      SET(CMAKE_ASM_NASM_OBJECT_FORMAT win64)
    else()
      SET(CMAKE_ASM_NASM_OBJECT_FORMAT win32)
    endif()
  elseif(APPLE)
    EXECUTE_PROCESS(COMMAND ${CMAKE_ASM_NASM_COMPILER} -v COMMAND awk "{print \$3}" OUTPUT_VARIABLE NASM_VERSION)
    IF(NASM_VERSION VERSION_LESS 2.0)
        IF(CMAKE_C_SIZEOF_DATA_PTR EQUAL 8)
            MESSAGE(FATAL_ERROR "Your nasm is too old to support AMD64, please install nasm from Homebrew or MacPorts.")
        ENDIF()
        SET(CMAKE_ASM_NAMS_OBJECT_FORMAT macho)
    ELSE()
        if(CMAKE_C_SIZEOF_DATA_PTR EQUAL 8)
          SET(CMAKE_ASM_NASM_OBJECT_FORMAT macho64)
        else()
          SET(CMAKE_ASM_NASM_OBJECT_FORMAT macho32)
        endif()
    ENDIF()
  else()
    if(CMAKE_C_SIZEOF_DATA_PTR EQUAL 8)
      SET(CMAKE_ASM_NASM_OBJECT_FORMAT elf64)
    else()
      SET(CMAKE_ASM_NASM_OBJECT_FORMAT elf32)
    endif()
  endif()
endif()

set(CMAKE_ASM_NASM_COMPILE_OBJECT "<CMAKE_ASM_NASM_COMPILER> <DEFINES> <INCLUDES> <FLAGS> -f ${CMAKE_ASM_NASM_OBJECT_FORMAT} -o <OBJECT> <SOURCE>")

# Load the generic ASMInformation file:
set(ASM_DIALECT "_NASM")
include(CMakeASMInformation)
set(ASM_DIALECT)
