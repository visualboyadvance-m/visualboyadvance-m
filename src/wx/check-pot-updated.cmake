if(NOT EXISTS ${BIN_DIR}/wxvbam.pot)
    return()
endif()

macro(read_pot path var)
    file(READ ${path} ${var}_file)

    string(REGEX REPLACE ";"     "\\\\;" ${var}_file ${${var}_file})
    string(REGEX REPLACE "\r?\n" ";"     ${var}_file ${${var}_file})

    # Ignore timestamp.
    foreach(line IN LISTS ${var}_file)
        if(NOT (line MATCHES [=[^"POT-Creation-Date: ]=] OR line MATCHES "^#"))
            list(APPEND ${var} ${line})
        endif()
    endforeach()
endmacro()

read_pot(${SRC_DIR}/wxvbam.pot src_ver)
read_pot(${BIN_DIR}/wxvbam.pot new_ver)

if(NOT src_ver STREQUAL new_ver)
    file(COPY ${BIN_DIR}/wxvbam.pot DESTINATION ${SRC_DIR})

    message([=[
************ ATTENTION!!! ************ 

The gettext source in ]=] ${SRC_DIR}/wxvbam.pot [=[ has been updated.

Please commit the result, it will be pushed to Transifex automatically.

************************************** 
]=])
endif()
