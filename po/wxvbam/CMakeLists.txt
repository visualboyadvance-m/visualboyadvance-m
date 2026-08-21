if(NOT TRANSLATIONS_ONLY AND NOT ENABLE_WX)
    return()
endif()

file(GLOB po_files CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.po")

gettext_create_translations(wxvbam.pot ${po_files})
