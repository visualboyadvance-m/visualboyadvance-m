# Clone web-data.
# Update version in appcast.xml to latest tag.
# Commit web-data.

find_package(Git)

if(NOT GIT_FOUND)
    message(FATAL_ERROR "git is required to update the appcast")
endif()

function(update_appcast)
    if(UPDATE_APPCAST STREQUAL UNDO)
	file(REMOVE_RECURSE ${CMAKE_BINARY_DIR}/web-data)

        message(FATAL_ERROR [=[
**** APPCAST.XML UPDATE HAS BEEN UNDONE ****"

Ignore the following cmake error.
]=])
    endif()

    # Get last tag.

    execute_process(
        COMMAND ${GIT_EXECUTABLE} tag --sort=-v:refname
        OUTPUT_VARIABLE git_tags
        OUTPUT_STRIP_TRAILING_WHITESPACE
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )

    string(REGEX REPLACE ";"     "\\\\;" git_tags_lines "${git_tags}")
    string(REGEX REPLACE "\r?\n" ";"     git_tags_lines "${git_tags_lines}")

    foreach(line ${git_tags_lines})
        if(line MATCHES "^v[0-9]")
            set(new_tag ${line})
            break()
        endif()
    endforeach()

    if(NOT DEFINED new_tag)
        message(FATAL_ERROR "cannot find last release tag")
    endif()

    # Remove the "v" for the version string.

    string(REGEX REPLACE "^v" "" new_version ${new_tag})

    # Clone repo.

    execute_process(
        COMMAND ${GIT_EXECUTABLE} clone git@github.com:visualboyadvance-m/visualboyadvance-m.github.io web-data
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )

    # Rewrite appcast.xml.

    # First make a copy for backing out.
    configure_file(
	${CMAKE_BINARY_DIR}/web-data/appcast.xml
        ${CMAKE_BINARY_DIR}/appcast.xml.orig
        COPYONLY
    )

    # Now read it and replace the versions.

    include(FileIterator)

    fi_open_file(${CMAKE_BINARY_DIR}/web-data/appcast.xml)

    set(work_file ${CMAKE_BINARY_DIR}/appcast.xml.work)

    file(REMOVE ${work_file})

    while(NOT fi_done)
        fi_get_next_line()

	string(REGEX REPLACE [=[(:version="|v)([0-9.]+)([/"])]=] "\\1${new_version}\\3" fi_line "${fi_line}")

        file(APPEND ${work_file} "${fi_line}" "\n")
    endwhile()

    # Convert to UNIX line endings on Windows, just copy the file otherwise.

    if(CMAKE_HOST_SYSTEM MATCHES Windows OR ((NOT DEFINED CMAKE_HOST_SYSTEM) AND WIN32))
        execute_process(
            COMMAND powershell -NoLogo -NoProfile -ExecutionPolicy Bypass -Command [=[
                $text = [IO.File]::ReadAllText("appcast.xml.work") -replace "`r`n", "`n"
                [IO.File]::WriteAllText("appcast.xml", $text)
            ]=]
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        )
    else()
        configure_file(
            ${CMAKE_BINARY_DIR}/appcast.xml.work
            ${CMAKE_BINARY_DIR}/appcast.xml
            COPYONLY
        )
    endif()

    # Copy the new file and add it to the commit.

    configure_file(
        ${CMAKE_BINARY_DIR}/appcast.xml
        ${CMAKE_BINARY_DIR}/web-data/appcast.xml
        COPYONLY
    )

    execute_process(
        COMMAND ${GIT_EXECUTABLE} add appcast.xml
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/web-data
    )

    # Commit the change.

    execute_process(
        COMMAND ${GIT_EXECUTABLE} commit -m "release ${new_tag}" --signoff -S
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/web-data
    )

    # Make release tag.

    execute_process(
        COMMAND ${GIT_EXECUTABLE} tag -s -m${new_tag} ${new_tag}
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/web-data
    )

    message(FATAL_ERROR [=[

appcast.xml updated.

**** IF YOU ARE SURE YOU WANT TO RELEASE ****

Run the following commands to push the release commit and tag:

    cd web-data
    git push
    git push --tags

**** TO UNDO THE RELEASE ****

To rollback these changes, run this command:

    cmake .. -DUPDATE_APPCAST=UNDO

Ignore the "configuration incomplete" message following, this mode does not
build anything.

]=])
endfunction()

update_appcast()
