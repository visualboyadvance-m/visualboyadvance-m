TEMPLATE = app
CONFIG += qt release
QT += opengl
TARGET = VisualBoyAdvance
DEFINES += BKPT_SUPPORT

# Directory Locations
M_DIR_QT = ../../src/qt/
M_DIR_LANG = ../../lang/
M_DIR_SHARED = ../../src/
M_DIR_GBAPU = ../../src/dmg/gb_apu/
M_DIR_AGB = ../../src/agb/
M_DIR_DMG = ../../src/dmg/


# OSs
!win32 {
  DEFINES += C_CORE
}


# Tweaks
win32-msvc2005 {
	DEFINES += _CRT_SECURE_NO_WARNINGS
	DEFINES += NO_PNG
}


# Resource Files
RESOURCES += $${M_DIR_QT}vba-m.qrc

win32 {
	RC_FILE = $${M_DIR_QT}vba-m.rc
}


# Language Files
TRANSLATIONS += $${M_DIR_LANG}german.ts
TRANSLATIONS += $${M_DIR_LANG}spanish.ts


# Qt GUI Files
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

HEADERS += $${M_DIR_QT}emu.h
SOURCES += $${M_DIR_QT}emu.cpp

HEADERS += $${M_DIR_QT}EmuManager.h
SOURCES += $${M_DIR_QT}EmuManager.cpp

HEADERS += $${M_DIR_QT}GraphicsOutput.h
SOURCES += $${M_DIR_QT}GraphicsOutput.cpp


# Shared Core Files
HEADERS += $${M_DIR_SHARED}System.h

HEADERS += $${M_DIR_SHARED}Globals.h
SOURCES += $${M_DIR_SHARED}Globals.cpp

#HEADERS += $${M_DIR_SHARED}armdis.h
#SOURCES += $${M_DIR_SHARED}armdis.cpp

HEADERS += $${M_DIR_SHARED}bios.h
SOURCES += $${M_DIR_SHARED}bios.cpp

HEADERS += $${M_DIR_SHARED}Cheats.h
SOURCES += $${M_DIR_SHARED}Cheats.cpp

#HEADERS += $${M_DIR_SHARED}CheatSearch.h
#SOURCES += $${M_DIR_SHARED}CheatSearch.cpp

HEADERS += $${M_DIR_SHARED}EEprom.h
SOURCES += $${M_DIR_SHARED}EEprom.cpp

HEADERS += $${M_DIR_SHARED}Flash.h
SOURCES += $${M_DIR_SHARED}Flash.cpp

HEADERS += $${M_DIR_SHARED}Sram.h
SOURCES += $${M_DIR_SHARED}Sram.cpp

HEADERS += $${M_DIR_SHARED}elf.h
SOURCES += $${M_DIR_SHARED}elf.cpp

HEADERS += $${M_DIR_SHARED}RTC.h
SOURCES += $${M_DIR_SHARED}RTC.cpp

HEADERS += $${M_DIR_SHARED}Sound.h
SOURCES += $${M_DIR_SHARED}Sound.cpp

HEADERS += $${M_DIR_SHARED}memgzio.h
SOURCES += $${M_DIR_SHARED}memgzio.c

HEADERS += $${M_DIR_SHARED}fex.h
SOURCES += $${M_DIR_SHARED}fex_mini.cpp

HEADERS += $${M_DIR_SHARED}Util.h
SOURCES += $${M_DIR_SHARED}Util.cpp

SOURCES += $${M_DIR_SHARED}Mode0.cpp
SOURCES += $${M_DIR_SHARED}Mode1.cpp
SOURCES += $${M_DIR_SHARED}Mode2.cpp
SOURCES += $${M_DIR_SHARED}Mode3.cpp
SOURCES += $${M_DIR_SHARED}Mode4.cpp
SOURCES += $${M_DIR_SHARED}Mode5.cpp

#HEADERS += $${M_DIR_SHARED}NLS.h

#HEADERS += $${M_DIR_SHARED}Port.h

#SOURCES += $${M_DIR_SHARED}remote.cpp


# GB APU
HEADERS += $${M_DIR_GBAPU}blargg_common.h
HEADERS += $${M_DIR_GBAPU}blargg_config.h
HEADERS += $${M_DIR_GBAPU}blargg_source.h

HEADERS += $${M_DIR_GBAPU}Blip_Buffer.h
SOURCES += $${M_DIR_GBAPU}Blip_Buffer.cpp

HEADERS += $${M_DIR_GBAPU}Effects_Buffer.h
SOURCES += $${M_DIR_GBAPU}Effects_Buffer.cpp

HEADERS += $${M_DIR_GBAPU}Gb_Apu.h
SOURCES += $${M_DIR_GBAPU}Gb_Apu.cpp
SOURCES += $${M_DIR_GBAPU}Gb_Apu_State.cpp

HEADERS += $${M_DIR_GBAPU}Gb_Oscs.h
SOURCES += $${M_DIR_GBAPU}Gb_Oscs.cpp

HEADERS += $${M_DIR_GBAPU}Multi_Buffer.h
SOURCES += $${M_DIR_GBAPU}Multi_Buffer.cpp


# GBA Core Files
HEADERS += $${M_DIR_AGB}agbprint.h
SOURCES += $${M_DIR_AGB}agbprint.cpp

HEADERS += $${M_DIR_AGB}GBA.h
SOURCES += $${M_DIR_AGB}GBA.cpp

#HEADERS += $${M_DIR_AGB}gbafilter.h
#SOURCES += $${M_DIR_AGB}gbafilter.cpp

HEADERS += $${M_DIR_AGB}GBAGfx.h
SOURCES += $${M_DIR_AGB}GBAGfx.cpp

#HEADERS += $${M_DIR_AGB}GBALink.h
#SOURCES += $${M_DIR_AGB}GBALink.cpp

SOURCES += $${M_DIR_AGB}GBA-arm.cpp
SOURCES += $${M_DIR_AGB}GBA-thumb.cpp

HEADERS += $${M_DIR_AGB}GBAcpu.h
HEADERS += $${M_DIR_AGB}GBAinline.h
