# Set TAG_RELEASE to ON/TRUE/1 to increment minor, or to new version, or to
# UNDO to undo release.

# Increment version.
# Update CHANGELOG.md with git log.
# Make release commit.
# Tag release commit.

if(NOT EXISTS "${CMAKE_SOURCE_DIR}/.git")
    message(FATAL_ERROR "releases can only be done from a git clone")
endif()

find_program(GPG_EXECUTABLE gpg)

if(NOT GPG_EXECUTABLE)
    message(FATAL_ERROR "gpg must be installed and set up with your key to make a release")
endif()

# From: https://gist.github.com/alorence/59d18896aaad5188b7b4.
macro(CURRENT_DATE RESULT)
    if(CMAKE_HOST_SYSTEM MATCHES Windows OR ((NOT DEFINED CMAKE_HOST_SYSTEM) AND WIN32))
        execute_process(COMMAND "cmd" " /C date /T" OUTPUT_VARIABLE ${RESULT})
        string(REGEX REPLACE ".*(..)/(..)/(....).*" "\\3-\\1-\\2" ${RESULT} ${${RESULT}})
    else()
        execute_process(COMMAND "date" "+%Y-%m-%d" OUTPUT_VARIABLE ${RESULT})
    endif()
endmacro()

function(make_release_commit_and_tag)

    # First make sure we are on master.

    execute_process(
        COMMAND git rev-parse --short --abbrev-ref=strict HEAD
        OUTPUT_VARIABLE git_branch
        OUTPUT_STRIP_TRAILING_WHITESPACE
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )

    if(NOT git_branch STREQUAL master)
        message(FATAL_ERROR "you must be on the git master branch to release")
    endif()

    execute_process(
        COMMAND git status --porcelain=2
        OUTPUT_VARIABLE git_status
        OUTPUT_STRIP_TRAILING_WHITESPACE
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )

    string(LENGTH "${git_status}" git_status_length)

    if(NOT git_status_length EQUAL 0)
        message(FATAL_ERROR "working tree must be clean to do a release")
    endif()

    execute_process(
        COMMAND git tag --sort=-v:refname
        OUTPUT_VARIABLE git_tags
        OUTPUT_STRIP_TRAILING_WHITESPACE
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )

    string(REGEX REPLACE ";"     "\\\\;" git_tags_lines "${git_tags}")
    string(REGEX REPLACE "\r?\n" ";"     git_tags_lines "${git_tags_lines}")

    foreach(line ${git_tags_lines})
        if(line MATCHES "^v[0-9]")
            set(git_last_tag ${line})
            break()
        endif()
    endforeach()

    if(NOT DEFINED git_last_tag)
        message(FATAL_ERROR "cannot find last release tag")
    endif()

    execute_process(
        COMMAND git log ${git_last_tag}.. "--pretty=format:* %h - %s [%aL]"
        OUTPUT_VARIABLE release_log
        OUTPUT_STRIP_TRAILING_WHITESPACE
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )

    # Calculate new release version, unless it was passed in.

    if(TAG_RELEASE STREQUAL UNDO)
        execute_process(
            COMMAND git tag -d ${git_last_tag}
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )

        execute_process(
            COMMAND git reset HEAD~1
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )

        execute_process(
            COMMAND git checkout HEAD CHANGELOG.md
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )

        message(FATAL_ERROR [=[
**** RELEASE COMMIT AND TAG HAVE BEEN REMOVED ****"

The git and working tree state have been restored to their state before the
TAG_RELEASE operation.

Ignore the following cmake error.
]=])
    elseif(NOT (TAG_RELEASE STREQUAL TRUE OR TAG_RELEASE STREQUAL ON OR TAG_RELEASE STREQUAL 1))
        set(new_tag ${TAG_RELEASE})
    else()
        string(REGEX MATCH   "\\.[0-9]+$"    last_tag_minor_part     ${git_last_tag})
        string(REGEX REPLACE "\\.[0-9]+$" "" last_tag_minor_stripped ${git_last_tag})

        string(SUBSTRING ${last_tag_minor_part} 1 -1 last_tag_minor)

        math(EXPR last_minor_incremented "${last_tag_minor} + 1")

        string(CONCAT new_tag ${last_tag_minor_stripped} . ${last_minor_incremented})
    endif()

    # Make sure tag begins with "v".

    if(NOT new_tag MATCHES "^v")
        set(new_tag "v${new_tag}")
    endif()

    # But remove the "v" for the version string.

    string(REGEX REPLACE "^v" "" new_version ${new_tag})

    current_date(current_date)

    # Rewrite CHANGELOG.md

    # First make a copy for backing out.
    configure_file(
        ${CMAKE_SOURCE_DIR}/CHANGELOG.md
        ${CMAKE_BINARY_DIR}/CHANGELOG.md.orig
        COPYONLY
    )

    # Now read it and add the new release.

    include(FileIterator)

    fi_open_file(${CMAKE_SOURCE_DIR}/CHANGELOG.md)

    set(work_file ${CMAKE_BINARY_DIR}/CHANGELOG.md.work)

    file(REMOVE ${work_file})

    set(last_release_found FALSE)

    while(NOT fi_done)
        fi_get_next_line()

        if(NOT last_release_found AND fi_line MATCHES "^## \\[[0-9]")
            set(last_release_found TRUE)

            set(tag_line "## [${new_version}] - ${current_date}")

            string(LENGTH "${tag_line}" tag_line_length)

            unset(tag_line_under_bar)

            foreach(i RANGE 1 ${tag_line_length})
                set(tag_line_under_bar "=${tag_line_under_bar}")
            endforeach()

            file(APPEND ${work_file} ${tag_line} "\n")
            file(APPEND ${work_file} ${tag_line_under_bar} "\n")
            file(APPEND ${work_file} "${release_log}" "\n")
            file(APPEND ${work_file} "\n")
        endif()

        file(APPEND ${work_file} "${fi_line}" "\n")
    endwhile()

    # Convert to UNIX line endings on Windows, just copy the file otherwise.
    if(CMAKE_HOST_SYSTEM MATCHES Windows OR ((NOT DEFINED CMAKE_HOST_SYSTEM) AND WIN32))
        if(NOT DEFINED POWERSHELL)
            message(FATAL_ERROR "Powershell is required to convert line endings on Windows.")
        endif()
        execute_process(
            COMMAND ${POWERSHELL} -NoLogo -NoProfile -ExecutionPolicy Bypass -Command [=[
                $text = [IO.File]::ReadAllText("CHANGELOG.md.work") -replace "`r`n", "`n"
                [IO.File]::WriteAllText("CHANGELOG.md", $text)
            ]=]
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        )
    else()
        configure_file(
            ${CMAKE_BINARY_DIR}/CHANGELOG.md.work
            ${CMAKE_BINARY_DIR}/CHANGELOG.md
            COPYONLY
        )
    endif()

    # Copy the new file and add it to the commit.

    configure_file(
        ${CMAKE_BINARY_DIR}/CHANGELOG.md
        ${CMAKE_SOURCE_DIR}/CHANGELOG.md
        COPYONLY
    )

    execute_process(
        COMMAND git add CHANGELOG.md
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )

    message(FATAL_ERROR "

Release prepared.

Edit CHANGELOG.md to remove any non-user-facing commits, and optionally add any
release notes.

Run the following commands to commit the change:

    git commit -a -m'release ${new_tag}' --signoff -S
    git tag -s -m'${new_tag}' ${new_tag}

To rollback these changes, run this command:

    cmake .. -DTAG_RELEASE=UNDO

==== IF YOU ARE SURE YOU WANT TO RELEASE, THIS **CANNOT** BE REVERSED ====

Run the following to push the release commit and tag:

    git push
    git push --tags

Ignore the 'configuration incomplete' message following, this mode does not
build anything.

")
endfunction()

make_release_commit_and_tag()
