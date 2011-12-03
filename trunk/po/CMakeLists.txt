IF(ENABLE_GTK)
  GETTEXT_CREATE_TRANSLATIONS(
	gvbam.pot
	gvbam/ca_ES.po
	gvbam/cs.po
	gvbam/de.po
	gvbam/en.po
	gvbam/es_ES.po
	gvbam/fr.po
	gvbam/it_IT.po
	gvbam/nb.po
	gvbam/nl.po
	gvbam/pl.po
	gvbam/pt_BR.po
	gvbam/sv.po
	gvbam/zh_CN.po
  )
  ADD_DEPENDENCIES(gvbam translations)
ENDIF(ENABLE_GTK)
IF(ENABLE_WX)
  # Extract message strings from xrc and source files (NLS only for 2nd cmd)
  ADD_CUSTOM_COMMAND(OUTPUT wx-xrc-strings.h
                     COMMAND wxrc -g ../src/wx/wxvbam.xrc -o ${CMAKE_CURRENT_BINARY_DIR}/wx-xrc-strings.h
		     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		     DEPENDS ../src/wx/wxvbam.xrc)
  SET(XGETTEXTCMD ${XGETTEXT} -k_ -kN_ -kwxTRANSLATE -s --copyright-holder=\"VBA-M development team\" --package-name=VBA-M --package-version=${VERSION})
  # note that GLOB wil return absolute paths unless otherwise requested
  # and it runs in CMAKE_CURRENT_SOURCE_DIR
  FILE(GLOB PO_SRC_FILES ../src/wx/*.cpp ../src/wx/*.h ../src/wx/widgets/*.cpp
                         ../src/Util.cpp ../src/gba/*.cpp ../src/gb/*.cpp)
  ADD_CUSTOM_COMMAND(OUTPUT wxvbam.pot
                     COMMAND ${XGETTEXTCMD} -o wxvbam.pot ${PO_SRC_FILES}
		     COMMAND ${XGETTEXTCMD} -o wxvbam.pot ../src/wx/cmdtab.cpp
		     COMMAND ${XGETTEXTCMD} --from-code=utf-8 -j -o wxvbam.pot wx-xrc-strings.h
		     WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
		     DEPENDS wx-xrc-strings.h) # there is no direct cmdtab.cpp rule

  # is there really any point in auto-generating en?
  ADD_CUSTOM_COMMAND(OUTPUT wxvbam/en.po
                     COMMAND ${CMAKE_COMMAND} -E make_directory wxvbam
		     COMMAND ${MSGINIT} -i wxvbam.pot -o wxvbam/en.po --no-translator -l en_US.utf-8
		     WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
		     DEPENDS wxvbam.pot)

  FILE(GLOB POFILES wxvbam/*.po)
  IF(NOT POFILES)
    SET(POFILES ${CMAKE_CURRENT_BINARY_DIR}/wxvbam/en.po)
  ENDIF(NOT POFILES)
  # only one GETTEXT_CREATE_TRANSLATIONS call can be made
  # probably need to simulate effects of call or force spearate cmake
  # invocations
  IF(NOT ENABLE_GTK)
    GETTEXT_CREATE_TRANSLATIONS(${CMAKE_CURRENT_BINARY_DIR}/wxvbam.pot ALL ${POFILES})
    # try to build wxvbam first so cmdtab is generated
    ADD_DEPENDENCIES(translations wxvbam)
  ELSE(NOT ENABLE_GTK)
    MESSAGE(WARNING "wxWidgets translations overridden by GTK.\nBuild again without GTK to correct.")
    ADD_CUSTOM_TARGET(extra_translations ALL DEPENDS ${POFILES})
    ADD_DEPENDENCIES(extra_translations wxvbam)
  ENDIF(NOT ENABLE_GTK)
ENDIF(ENABLE_WX)

