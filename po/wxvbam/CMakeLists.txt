if(NOT ENABLE_WX OR NOT ENABLE_NLS)
    return()
endif()

file(GLOB po_files "${CMAKE_CURRENT_SOURCE_DIR}/*.po")

gettext_create_translations(wxvbam.pot ${po_files})
