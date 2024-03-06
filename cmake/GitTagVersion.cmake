function(git_version version revision version_release)
    set(${version}         "" CACHE STRING "Latest Git Tag Version"  FORCE)
    set(${revision}        "" CACHE STRING "Latest Git Tag Revision" FORCE)
    set(${version_release} 0  CACHE STRING "Is this a versioned release without revision" FORCE)

    find_package(Git)
    if(GIT_FOUND AND EXISTS "${CMAKE_SOURCE_DIR}/.git")
        # get latest version from tag history
        execute_process(COMMAND "${GIT_EXECUTABLE}" tag "--format=%(align:width=20)%(refname:short)%(end)%(if)%(*objectname)%(then)%(*objectname)%(else)%(objectname)%(end)" --sort=-creatordate OUTPUT_VARIABLE tags OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

        # if no tags (e.g. shallow clone) do nothing
        if(tags STREQUAL "")
            return()
        endif()

        # convert to list of the form [tag0, ref0, tag1, ref1, ...]
        string(REGEX REPLACE "[ \n]+" ";" tags "${tags}")

        execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse HEAD OUTPUT_VARIABLE current_ref OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

        # if cannot get current ref, do nothing
        if(current_ref STREQUAL "")
            return()
        endif()

        list(LENGTH tags cnt)
        set(i 0)
        set(j 1)

        while(i LESS cnt AND "${${version}}" STREQUAL "")
            list(GET tags ${i} tag)
            list(GET tags ${j} ref)

            # tag is a version number with or without a "-revision"
            if(tag MATCHES "^v?(([0-9]+\\.?)*[0-9]*)(-(.*))?$")
                set(${version} "${CMAKE_MATCH_1}" CACHE STRING "Latest Git Tag Version" FORCE)

                if(i EQUAL 0)
                    if(NOT "${CMAKE_MATCH_4}" STREQUAL "")
                        set(${revision} "${CMAKE_MATCH_4}" CACHE STRING "Latest Git Tag Revision" FORCE)
                    elseif(ref STREQUAL current_ref)
                        set(${version_release} 1 CACHE STRING "Is this a versioned release without revision" FORCE)
                    endif()
                endif()
            elseif(i EQUAL 0 AND ref STREQUAL current_ref)
                # revision name tagged
                set(${revision} "${tag}" CACHE STRING "Latest Git Tag Revision" FORCE)
            endif()

            math(EXPR i "${i} + 2")
            math(EXPR j "${j} + 2")
        endwhile()

        if(NOT "${${version_release}}" AND "${${revision}}" STREQUAL "")
            # dev version, use short sha for ref
            execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse --short HEAD OUTPUT_VARIABLE short_sha OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
            set(${revision} "${short_sha}" CACHE STRING "Latest Git Tag Revision" FORCE)
        endif()
    endif()
endfunction()
