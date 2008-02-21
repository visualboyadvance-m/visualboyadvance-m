TEMPLATE = app
CONFIG += qt release
QT += opengl
TARGET = VisualBoyAdvance

TRANSLATIONS += ../../lang/german.ts
TRANSLATIONS += ../../lang/spanish.ts
RESOURCES += ../../src/qt/vba-m.qrc

PRECOMPILED_HEADER = ../../src/qt/precompile.h

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
HEADERS += ../../src/qt/configdialog.h
SOURCES += ../../src/qt/MainOptions.cpp
SOURCES += ../../src/qt/configdialog.cpp
