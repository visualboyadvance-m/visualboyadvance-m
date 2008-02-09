TEMPLATE = app
CONFIG += qt
QT += opengl
TARGET = VisualBoyAdvance

PRECOMPILED_HEADER = ../../src/qt/precompile.h

HEADERS += ../../src/qt/main.h
SOURCES += ../../src/qt/main.cpp

HEADERS += ../../src/qt/mainwnd.h
SOURCES += ../../src/qt/mainwnd.cpp

HEADERS += ../../src/qt/glwidget.h
SOURCES += ../../src/qt/glwidget.cpp

TRANSLATIONS += ../../lang/german.ts
