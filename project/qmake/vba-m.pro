TEMPLATE = app
CONFIG += qt release
QT += opengl
TARGET = VisualBoyAdvance

PRECOMPILED_HEADER = ../../src/qt/precompile.h

HEADERS += ../../src/qt/main.h
SOURCES += ../../src/qt/main.cpp

HEADERS += ../../src/qt/MainWnd.h
SOURCES += ../../src/qt/MainWnd.cpp

HEADERS += ../../src/qt/glwidget.h
SOURCES += ../../src/qt/glwidget.cpp

TRANSLATIONS += ../../lang/german.ts
TRANSLATIONS += ../../lang/spanish.ts
