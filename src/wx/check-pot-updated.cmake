if(NOT EXISTS ${BIN_DIR}/wxvbam.pot)
    return()
endif()

file(READ ${SRC_DIR}/wxvbam.pot src_ver)

file(READ ${BIN_DIR}/wxvbam.pot new_ver)

if(NOT src_ver STREQUAL new_ver)
    file(COPY ${BIN_DIR}/wxvbam.pot DESTINATION ${SRC_DIR})

    message([=[
************ ATTENTION!!! ************ 

The gettext source in ]=] ${SRC_DIR}/wxvbam.pot [=[ has been updated.

Please commit the result and push to transifex.

************************************** 
]=])
endif()
