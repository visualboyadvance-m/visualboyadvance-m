file(GLOB po_files "${CMAKE_CURRENT_SOURCE_DIR}/*.po")

GETTEXT_CREATE_TRANSLATIONS(
	wxvbam.pot
        ${po_files}
)

ADD_DEPENDENCIES(visualboyadvance-m translations)
