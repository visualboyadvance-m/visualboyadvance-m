function(changelog_version version revision version_release)
    set(${version}         "" CACHE STRING "Latest ChangeLog Version"  FORCE)
    set(${revision}        "" CACHE STRING "Latest ChangeLog Revision" FORCE)
    set(${version_release} 0  CACHE STRING "Is this a versioned release without revision" FORCE)

    file(READ CHANGELOG.md changelog_file)

    if(NOT changelog_file)
        return()
    endif()

    string(REGEX MATCH "\n## +\\[([0-9.]+)(-([^] ]+))?\\] +.* +[0-9][0-9]?/" match_out "${changelog_file}")

    set(changelog_version "${CMAKE_MATCH_1}")

    set(is_version_release 0)
    set(changelog_revision "${CMAKE_MATCH_3}")
    if(NOT changelog_revision)
        set(is_version_release 1)
    endif()

    set(${version}         "${changelog_version}"  CACHE STRING "Latest ChangeLog Version"  FORCE)
    set(${revision}        "${changelog_revision}" CACHE STRING "Latest ChangeLog Revision" FORCE)
    set(${version_release} "${is_version_release}" CACHE STRING "Is this a versioned release without revision" FORCE)
endfunction()
