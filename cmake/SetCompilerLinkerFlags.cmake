include(VbamFunctions)

function(add_compiler_flags)
    foreach(var RELEASE DEBUG RELWITHDEBINFO MINSIZEREL)
        set("CMAKE_CXX_FLAGS_${var}" "" CACHE STRING "MUST BE UNSET" FORCE)
        set("CMAKE_CXX_FLAGS_${var}" "" PARENT_SCOPE)
        set("CMAKE_C_FLAGS_${var}"   "" CACHE STRING "MUST BE UNSET" FORCE)
        set("CMAKE_C_FLAGS_${var}"   "" PARENT_SCOPE)
    endforeach()

    # Set C and CXX flags if not already set.
    foreach(flag ${ARGV})
        foreach(var CMAKE_CXX_FLAGS CMAKE_C_FLAGS)
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
