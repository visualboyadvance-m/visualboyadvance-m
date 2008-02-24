TEMPLATE = app
CONFIG += qt release
QT += opengl
TARGET = VisualBoyAdvance

RESOURCES += ../../src/qt/vba-m.qrc

win32 {
	RC_FILE = ../../src/qt/vba-m.rc
}

TRANSLATIONS += ../../lang/german.ts
TRANSLATIONS += ../../lang/spanish.ts

PRECOMPILED_HEADER = ../../src/qt/precompile.h

HEADERS += ../../src/qt/version.h

HEADERS += ../../src/qt/main.h
SOURCES += ../../src/qt/main.cpp

HEADERS += ../../src/qt/MainWnd.h
SOURCES += ../../src/qt/MainWnd.cpp

HEADERS += ../../src/qt/glwidget.h
SOURCES += ../../src/qt/glwidget.cpp

FORMS += ../../src/qt/sidewidget_cheats.ui
HEADERS += ../../src/qt/sidewidget_cheats.h
SOURCES += ../../src/qt/sidewidget_cheats.cpp

HEADERS += ../../src/qt/MainOptions.h
SOURCES += ../../src/qt/MainOptions.cpp

HEADERS += ../../src/qt/configdialog.h
SOURCES += ../../src/qt/configdialog.cpp

HEADERS += ../../src/qt/EmuManager.h
SOURCES += ../../src/qt/EmuManager.cpp

HEADERS += ../../src/qt/GraphicsOutput.h
SOURCES += ../../src/qt/GraphicsOutput.cpp
