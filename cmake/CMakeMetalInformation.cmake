# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENCE.txt or https://cmake.org/licensing for details.

#  CMake(LANG)Information.cmake  -> set up rule variables for LANG :
#    CMAKE_(LANG)_CREATE_SHARED_LIBRARY
#    CMAKE_(LANG)_CREATE_SHARED_MODULE
#    CMAKE_(LANG)_CREATE_STATIC_LIBRARY
#    CMAKE_(LANG)_COMPILE_OBJECT
#    CMAKE_(LANG)_LINK_EXECUTABLE

include(CMakeCommonLanguageInclude)

set(CMAKE_Metal_FLAGS_INIT "-ffast-math")
set(CMAKE_Metal_FLAGS_DEBUG_INIT "-gline-tables-only -frecord-sources")
set(CMAKE_Metal_FLAGS_RELWITHDEBINFO_INIT "-gline-tables-only -frecord-sources")

cmake_initialize_per_config_variable(CMAKE_Metal_FLAGS "Flags used by the Metal compiler")

set(CMAKE_INCLUDE_FLAG_Metal "-I ")
set(CMAKE_Metal_COMPILER_ARG1 "")
set(CMAKE_Metal_DEFINE_FLAG -D)
set(CMAKE_Metal_FRAMEWORK_SEARCH_FLAG "-F ")
set(CMAKE_Metal_LIBRARY_PATH_FLAG "-L ")
set(CMAKE_Metal_SYSROOT_FLAG "-isysroot")
set(CMAKE_Metal_COMPILE_OPTIONS_TARGET "-target ")
set(CMAKE_DEPFILE_FLAGS_Metal "-MMD -MT dependencies -MF <DEP_FILE>")

if(CMAKE_GENERATOR MATCHES "Makefiles")
    set(CMAKE_Metal_DEPFILE_FORMAT gcc)
    set(CMAKE_Metal_DEPENDS_USE_COMPILER TRUE)
endif()

set(CMAKE_Metal_COMPILER_PREDEFINES_COMMAND "${CMAKE_Metal_COMPILER}")
if(CMAKE_Metal_COMPILER_TARGET)
    list(APPEND CMAKE_Metal_COMPILER_PREDEFINES_COMMAND "-target" "${CMAKE_Metal_COMPILER_TARGET}")
endif()

# now define the following rule variables

# CMAKE_Metal_CREATE_SHARED_LIBRARY
# CMAKE_Metal_CREATE_SHARED_MODULE
# CMAKE_Metal_COMPILE_OBJECT
# CMAKE_Metal_LINK_EXECUTABLE

# variables supplied by the generator at use time
# <TARGET>
# <TARGET_BASE> the target without the suffix
# <OBJECTS>
# <OBJECT>
# <LINK_LIBRARIES>
# <FLAGS>
# <LINK_FLAGS>

# Metal compiler information
# <CMAKE_Metal_COMPILER>
# <CMAKE_SHARED_LIBRARY_CREATE_Metal_FLAGS>
# <CMAKE_SHARED_MODULE_CREATE_Metal_FLAGS>
# <CMAKE_Metal_LINK_FLAGS>

if(NOT CMAKE_Metal_COMPILE_OBJECT)
  set(CMAKE_Metal_COMPILE_OBJECT
      "<CMAKE_Metal_COMPILER> -c <DEFINES> <INCLUDES> <FLAGS> -o <OBJECT> <SOURCE>"
  )
endif()

if(NOT CMAKE_Metal_CREATE_SHARED_LIBRARY)
    set(CMAKE_Metal_CREATE_SHARED_LIBRARY
        "<CMAKE_Metal_COMPILER> <CMAKE_SHARED_LIBRARY_Metal_FLAGS> <LANGUAGE_COMPILE_FLAGS> <CMAKE_SHARED_LIBRARY_CREATE_Metal_FLAGS> -o <TARGET> <OBJECTS> <LINK_LIBRARIES>"
    )
endif()

if(NOT CMAKE_Metal_CREATE_SHARED_MODULE)
    set(CMAKE_Metal_CREATE_SHARED_MODULE
        "${CMAKE_Metal_CREATE_SHARED_LIBRARY}"
    )
endif()

if(NOT CMAKE_Metal_LINK_EXECUTABLE)
    # Metal shaders don't really have "executables", but we need this for the try_compile to work properly, so we'll just have it output a metallib file
    set(CMAKE_Metal_LINK_EXECUTABLE
        "<CMAKE_Metal_COMPILER> <FLAGS> <CMAKE_Metal_LINK_FLAGS> -o <TARGET> <OBJECTS> <LINK_LIBRARIES>"
    )
endif()

set(CMAKE_Metal_INFORMATION_LOADED 1)
