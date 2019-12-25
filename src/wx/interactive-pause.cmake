# Do not pause in automated jobs.

if(DEFINED ENV{CI} OR DEFINED ENV{VBAM_NO_PAUSE})
    return()
endif()

# I have no idea how to do this reliably on Windows yet.

if(NOT WIN32 OR DEFINED ENV{MSYSTEM_PREFIX})
    execute_process(
        COMMAND sh -c [=[
            echo >&2 "********** PRESS ENTER TO CONTINUE **********"
            read -r dummy
        ]=]
    )
endif()
