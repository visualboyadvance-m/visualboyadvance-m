# Checks that the three Language<N> lists agree with each other:
#
#   - the Language<N> menu items in xrc/MainMenu.xrc  (labels the user sees)
#   - the EVT_HANDLER(Language<N>) handlers in cmdevents.cpp  (what gets applied)
#   - the MenuOptionIntRadioValue("Language<N>") calls in guiinit.cpp  (which
#     item is shown as selected)
#
# All three are keyed by the same XRC id, and nothing in the build catches a
# mismatch: the app just applies a different language than the one clicked and
# checks the wrong menu item. Run as:
#
#   cmake -D SRC_DIR=<src/wx> -P check-language-menu.cmake

# Script mode does not inherit the project's policies, so IN_LIST below needs
# CMP0057 set here.
cmake_minimum_required(VERSION 3.19)

if(NOT SRC_DIR)
    message(FATAL_ERROR "SRC_DIR must be set")
endif()

set(errors)

# xrc/MainMenu.xrc: id -> label.
file(READ "${SRC_DIR}/xrc/MainMenu.xrc" xrc)
string(REGEX MATCHALL
       "<object class=\"wxMenuItem\" name=\"Language[0-9]+\">[ \t\r\n]*<label>[^<]*</label>"
       xrc_items "${xrc}")
set(xrc_ids)
foreach(item IN LISTS xrc_items)
    string(REGEX REPLACE ".*name=\"(Language[0-9]+)\".*" "\\1" id "${item}")
    string(REGEX REPLACE ".*<label>([^<]*)</label>.*" "\\1" label "${item}")
    list(APPEND xrc_ids "${id}")
    set(xrc_label_${id} "${label}")
endforeach()

# cmdevents.cpp: id -> wxLANGUAGE_* constant.
file(READ "${SRC_DIR}/cmdevents.cpp" cmdevents)
string(REGEX MATCHALL
       "EVT_HANDLER\\(Language[0-9]+, \"[^\"]*\"\\)[ \t\r\n]*{[ \t\r\n]*SetUiLanguage\\(wxLANGUAGE_[A-Z_0-9]+\\)"
       handlers "${cmdevents}")
set(handler_ids)
foreach(handler IN LISTS handlers)
    string(REGEX REPLACE "EVT_HANDLER\\((Language[0-9]+),.*" "\\1" id "${handler}")
    string(REGEX REPLACE ".*SetUiLanguage\\((wxLANGUAGE_[A-Z_0-9]+)\\)" "\\1" lang "${handler}")
    list(APPEND handler_ids "${id}")
    set(handler_lang_${id} "${lang}")
endforeach()

# Every EVT_HANDLER(Language<N>) must use the shared helper, or it escapes the
# check above entirely.
string(REGEX MATCHALL "EVT_HANDLER\\(Language[0-9]+," all_handlers "${cmdevents}")
list(LENGTH all_handlers all_handler_count)
list(LENGTH handler_ids handler_count)
if(NOT all_handler_count EQUAL handler_count)
    list(APPEND errors
         "cmdevents.cpp: ${all_handler_count} Language handlers but only ${handler_count} call SetUiLanguage()")
endif()

# guiinit.cpp: id -> wxLANGUAGE_* constant.
file(READ "${SRC_DIR}/guiinit.cpp" guiinit)
string(REGEX MATCHALL
       "MenuOptionIntRadioValue\\(\"Language[0-9]+\", OPTION\\(kLocale\\), wxLANGUAGE_[A-Z_0-9]+\\)"
       radios "${guiinit}")
set(radio_ids)
foreach(radio IN LISTS radios)
    string(REGEX REPLACE ".*\"(Language[0-9]+)\".*" "\\1" id "${radio}")
    string(REGEX REPLACE ".*(wxLANGUAGE_[A-Z_0-9]+)\\)" "\\1" lang "${radio}")
    list(APPEND radio_ids "${id}")
    set(radio_lang_${id} "${lang}")
endforeach()

if(NOT xrc_ids)
    list(APPEND errors "no Language<N> menu items found in xrc/MainMenu.xrc")
endif()

# The three id sets must be identical.
foreach(id IN LISTS xrc_ids)
    if(NOT id IN_LIST handler_ids)
        list(APPEND errors "${id} (\"${xrc_label_${id}}\") is in the menu but has no handler in cmdevents.cpp")
    endif()
    if(NOT id IN_LIST radio_ids)
        list(APPEND errors "${id} (\"${xrc_label_${id}}\") is in the menu but has no MenuOptionIntRadioValue() in guiinit.cpp")
    endif()
endforeach()
foreach(id IN LISTS handler_ids)
    if(NOT id IN_LIST xrc_ids)
        list(APPEND errors "${id} has a handler in cmdevents.cpp but no menu item in xrc/MainMenu.xrc")
    endif()
endforeach()
foreach(id IN LISTS radio_ids)
    if(NOT id IN_LIST xrc_ids)
        list(APPEND errors "${id} has a MenuOptionIntRadioValue() in guiinit.cpp but no menu item in xrc/MainMenu.xrc")
    endif()
endforeach()

foreach(id IN LISTS xrc_ids)
    if(NOT DEFINED handler_lang_${id} OR NOT DEFINED radio_lang_${id})
        continue()
    endif()

    # The handler and the radio state must apply/report the same language.
    if(NOT handler_lang_${id} STREQUAL radio_lang_${id})
        list(APPEND errors
             "${id}: cmdevents.cpp applies ${handler_lang_${id}} but guiinit.cpp checks the item for ${radio_lang_${id}}")
    endif()

    # ...and it must be the language the menu label promises. Compare the first
    # word of the label with the constant, so "Chinese [China]" matches
    # wxLANGUAGE_CHINESE_CHINA and "Italian" does not match wxLANGUAGE_FRENCH.
    string(REGEX REPLACE " .*" "" first_word "${xrc_label_${id}}")
    string(TOUPPER "${first_word}" first_word)
    string(REGEX REPLACE "^wxLANGUAGE_" "" lang_name "${handler_lang_${id}}")
    if(xrc_label_${id} STREQUAL "Default Language")
        set(first_word "DEFAULT")
    endif()
    if(NOT lang_name MATCHES "^${first_word}(_|$)")
        list(APPEND errors
             "${id}: menu label \"${xrc_label_${id}}\" does not match ${handler_lang_${id}}")
    endif()
endforeach()

if(errors)
    string(REPLACE ";" "\n  " errors "${errors}")
    message(FATAL_ERROR "Language menu is inconsistent:\n  ${errors}\n")
endif()

list(LENGTH xrc_ids count)
message(STATUS "Language menu is consistent across all three lists (${count} entries)")
