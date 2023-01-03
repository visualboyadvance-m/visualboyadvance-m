include(VbamFunctions)

function(add_compiler_flags)
    # Set C and CXX flags if not already set.
    foreach(flag ${ARGV})
        foreach(var CMAKE_CXX_FLAGS_${CMAKE_BUILD_TYPE} CMAKE_C_FLAGS_${CMAKE_BUILD_TYPE})
            # Remove any duplicates first.
            remove_dupes("${${var}}" "${var}")

            string(FIND "${${var}}" "${flag}" found)

            if(found EQUAL -1)
                set("${var}" "${${var}} ${flag}" CACHE STRING "Compiler Flags" FORCE)
                set("${var}" "${${var}} ${flag}" PARENT_SCOPE)
            endif()
        endforeach()
    endforeach()
endfunction()

function(add_linker_flags)
    # Set linker flags if not already set.
    foreach(flag ${ARGV})
        foreach(var EXE SHARED)
            set(var "CMAKE_${var}_LINKER_FLAGS")

            # Remove any duplicates first.
            remove_dupes("${${var}}" "${var}")

            string(FIND "${${var}}" "${flag}" found)

            if(found EQUAL -1)
                set("${var}" "${${var}} ${flag}" CACHE STRING "Linker Flags" FORCE)
                set("${var}" "${${var}} ${flag}" PARENT_SCOPE)
            endif()
        endforeach()
    endforeach()
endfunction()
