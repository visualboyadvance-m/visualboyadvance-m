function(git_version version revision version_release)
    set(${version}         "" CACHE STRING "Latest Git Tag Version"  FORCE)
    set(${revision}        "" CACHE STRING "Latest Git Tag Revision" FORCE)
    set(${version_release} 0  CACHE STRING "Is this a versioned release without revision" FORCE)

    find_package(Git)
    if(GIT_FOUND AND EXISTS "${CMAKE_SOURCE_DIR}/.git")
        # get latest version from tag history
        execute_process(COMMAND ${GIT_EXECUTABLE} tag --sort=-creatordate OUTPUT_VARIABLE sorted_tags OUTPUT_STRIP_TRAILING_WHITESPACE)

        # if no tags (e.g. shallow clone) do nothing
        if(NOT sorted_tags)
            return()
        endif()

        # convert to list (see: https://public.kitware.com/pipermail/cmake/2007-May/014222.html)
        string(REGEX REPLACE ";"  "\\\\;" sorted_tags "${sorted_tags}")
        string(REGEX REPLACE "\n" ";"     sorted_tags "${sorted_tags}")

        foreach(tag ${sorted_tags})
            if(tag MATCHES "^v?(([0-9]+\\.?)*[0-9]*)(-(.*))?$")
                set(${version} "${CMAKE_MATCH_1}" CACHE STRING "Latest Git Tag Version" FORCE)
                set(revision_str "${CMAKE_MATCH_4}")
                break()
            endif()
        endforeach()

        if(revision_str)
            # don't need to read revision from tags
            set(${revision} "${revision_str}" CACHE STRING "Latest Git Tag Revision" FORCE)
            return()
        endif()

        # get the current revision
        execute_process(COMMAND ${GIT_EXECUTABLE} tag "--format=%(align:width=20)%(refname:short)%(end)%(if)%(*objectname)%(then)%(*objectname)%(else)%(objectname)%(end)" --sort=-creatordate OUTPUT_VARIABLE sorted_tags_refs OUTPUT_STRIP_TRAILING_WHITESPACE)

        # if no tags (e.g. shallow clone) do nothing
        if(NOT sorted_tags_refs)
            return()
        endif()

        # convert to list (see: https://public.kitware.com/pipermail/cmake/2007-May/014222.html)
        string(REGEX REPLACE ";"  "\\\\;" sorted_tags_refs "${sorted_tags_refs}")
        string(REGEX REPLACE "\n" ";"     sorted_tags_refs "${sorted_tags_refs}")

        # get the newest tag
        list(GET sorted_tags_refs 0 tag_ref)

        string(REGEX REPLACE "^([^ ]+) +([^ ]+)$" "\\1" tag "${tag_ref}")
        string(REGEX REPLACE "^([^ ]+) +([^ ]+)$" "\\2" ref "${tag_ref}")

        execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse HEAD OUTPUT_VARIABLE current_ref OUTPUT_STRIP_TRAILING_WHITESPACE)

        if(tag MATCHES "^v?(([0-9]+\\.?)*[0-9]*)(-(.*))?$" AND ref STREQUAL current_ref)
            set(revision_str "${CMAKE_MATCH_4}")

            # version and no revision tagged for current commit
            if(NOT revision_str)
                set(${revision} "" CACHE STRING "Latest Git Tag Revision" FORCE)
                set(${version_release} 1 CACHE STRING "Is this a versioned release without revision" FORCE)
            else()
                set(${revision} "${revision_str}" CACHE STRING "Latest Git Tag Revision" FORCE)
            endif()
        elseif(ref STREQUAL current_ref)
            # revision name tagged
            set(${revision} "${tag}" CACHE STRING "Latest Git Tag Revision" FORCE)
        else()
            # dev version, use short sha for ref
            execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD OUTPUT_VARIABLE short_sha OUTPUT_STRIP_TRAILING_WHITESPACE)
            set(${revision} "${short_sha}" CACHE STRING "Latest Git Tag Revision" FORCE)
        endif()
    endif()
endfunction()
