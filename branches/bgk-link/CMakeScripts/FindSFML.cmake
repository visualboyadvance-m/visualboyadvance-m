# Locate SFML library 
# This module defines 
# SFML_LIBRARY, the name of the library to link against 
# SFML_FOUND, if false, do not try to link to SFML 
# SFML_INCLUDE_DIR, where to find SFML headers 
# 
# Created by Nils Hasenbanck. Based on the FindSDL_*.cmake modules, 
# created by Eric Wing, which were influenced by the FindSDL.cmake 
# module, but with modifications to recognize OS X frameworks and 
# additional Unix paths (FreeBSD, etc). 

SET(SFML_COMPONENTS 
    System 
    Audio 
    Graphics 
    Network 
    Window 
) 

SET(SFML_INCLUDE_SEARCH_DIR 
    ~/Library/Frameworks 
    /Library/Frameworks 
    /usr/local/include/SFML 
    /usr/include/SFML 
    /usr/local/include 
    /usr/include 
    /sw/include/SFML # Fink 
    /sw/include 
    /opt/local/include/SFML # DarwinPorts 
    /opt/local/include 
    /opt/csw/include/SFML # Blastwave 
    /opt/csw/include 
    /opt/include/SFML 
    /opt/include 
) 

SET(SFML_LIBRARY_SEARCH_DIR 
    ~/Library/Frameworks 
    /Library/Frameworks 
    /usr/local 
    /usr 
    /sw 
    /opt/local 
    /opt/csw 
    /opt 
) 

FOREACH(COMPONENT ${SFML_COMPONENTS}) 
    STRING(TOUPPER ${COMPONENT} UPPERCOMPONENT) 
    STRING(TOLOWER ${COMPONENT} LOWERCOMPONENT) 
    FIND_LIBRARY(SFML_${UPPERCOMPONENT}_LIBRARY 
        NAMES sfml-${LOWERCOMPONENT} 
        HINTS 
        $ENV{SFMLDIR} 
        PATH_SUFFIXES lib64 lib 
        PATHS ${SFML_LIBRARY_SEARCH_DIR} 
    ) 
    FIND_PATH(SFML_${UPPERCOMPONENT}_INCLUDE_DIR ${COMPONENT}.hpp 
        HINTS 
        $ENV{SFMLDIR} 
        PATH_SUFFIXES include 
        PATHS ${SFML_INCLUDE_SEARCH_DIR} 
    ) 
    IF(SFML_${UPPERCOMPONENT}_INCLUDE_DIR AND SFML_${UPPERCOMPONENT}_LIBRARY) 
        LIST(APPEND SFML_LIBRARY ${SFML_${UPPERCOMPONENT}_LIBRARY}) 
        LIST(APPEND SFML_INCLUDE_DIR ${SFML_${UPPERCOMPONENT}_INCLUDE_DIR}) 
        LIST(REMOVE_DUPLICATES SFML_LIBRARY) 
        LIST(REMOVE_DUPLICATES SFML_INCLUDE_DIR) 
    ENDIF(SFML_${UPPERCOMPONENT}_INCLUDE_DIR AND SFML_${UPPERCOMPONENT}_LIBRARY) 
ENDFOREACH(COMPONENT) 

SET(SFML_FOUND "NO") 
IF(SFML_SYSTEM_LIBRARY AND SFML_INCLUDE_DIR) 
    SET(SFML_FOUND "YES") 
ENDIF(SFML_SYSTEM_LIBRARY AND SFML_INCLUDE_DIR) 

INCLUDE(FindPackageHandleStandardArgs) 
FIND_PACKAGE_HANDLE_STANDARD_ARGS(SFML DEFAULT_MSG SFML_LIBRARY SFML_INCLUDE_DIR)
