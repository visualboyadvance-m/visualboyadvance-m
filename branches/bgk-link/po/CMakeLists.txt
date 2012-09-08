IF(ENABLE_GTK)
  add_subdirectory(gvbam)
ENDIF(ENABLE_GTK)
IF(ENABLE_WX)
  add_subdirectory(wxvbam)
ENDIF(ENABLE_WX)

