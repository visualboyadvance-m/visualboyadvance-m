TEMPLATE = app
CONFIG += qt release
QT += opengl
TARGET = VisualBoyAdvance

# Directory Locations
M_DIR_QT = ../../src/qt/
M_DIR_LANG = ../../lang/

RESOURCES += $${M_DIR_QT}vba-m.qrc

win32 {
	RC_FILE = $${M_DIR_QT}vba-m.rc
}

#  Language Files
TRANSLATIONS += $${M_DIR_LANG}german.ts
TRANSLATIONS += $${M_DIR_LANG}spanish.ts

PRECOMPILED_HEADER = $${M_DIR_QT}precompile.h

HEADERS += $${M_DIR_QT}version.h

HEADERS += $${M_DIR_QT}main.h
SOURCES += $${M_DIR_QT}main.cpp

HEADERS += $${M_DIR_QT}MainWnd.h
SOURCES += $${M_DIR_QT}MainWnd.cpp

FORMS += $${M_DIR_QT}sidewidget_cheats.ui
HEADERS += $${M_DIR_QT}sidewidget_cheats.h
SOURCES += $${M_DIR_QT}sidewidget_cheats.cpp

HEADERS += $${M_DIR_QT}MainOptions.h
SOURCES += $${M_DIR_QT}MainOptions.cpp

HEADERS += $${M_DIR_QT}configdialog.h
SOURCES += $${M_DIR_QT}configdialog.cpp

HEADERS += $${M_DIR_QT}EmuManager.h
SOURCES += $${M_DIR_QT}EmuManager.cpp

HEADERS += $${M_DIR_QT}GraphicsOutput.h
SOURCES += $${M_DIR_QT}GraphicsOutput.cpp
