// VisualBoyAdvance - Nintendo Gameboy/GameboyAdvance (TM) emulator.
// Copyright (C) 1999-2003 Forgotten
// Copyright (C) 2005 Forgotten and the VBA development team

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or(at your option)
// any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

#include "stdafx.h"
#include "AcceleratorManager.h"
#include "resource.h"
#include <afxres.h>

#include <afxtempl.h>  // MFC Templates extension
#ifndef CMapStringToWord
typedef CMap< CString, LPCSTR, WORD, WORD& > CMapStringToWord;
#endif

static CMapStringToWord winAccelStrings;
static bool initialized = false;

struct {
  const char *command;
  WORD id;
} winAccelCommands[] = {
  { "FileOpen", ID_FILE_OPEN },
  { "FileOpenGameboy", ID_FILE_OPENGAMEBOY },
  { "FileLoad", ID_FILE_LOAD },
  { "FileSave", ID_FILE_SAVE },
  { "FileLoadGame01", ID_FILE_LOADGAME_SLOT1 },
  { "FileLoadGame02", ID_FILE_LOADGAME_SLOT2 },
  { "FileLoadGame03", ID_FILE_LOADGAME_SLOT3 },
  { "FileLoadGame04", ID_FILE_LOADGAME_SLOT4 },
  { "FileLoadGame05", ID_FILE_LOADGAME_SLOT5 },
  { "FileLoadGame06", ID_FILE_LOADGAME_SLOT6 },
  { "FileLoadGame07", ID_FILE_LOADGAME_SLOT7 },
  { "FileLoadGame08", ID_FILE_LOADGAME_SLOT8 },
  { "FileLoadGame09", ID_FILE_LOADGAME_SLOT9 },
  { "FileLoadGame10", ID_FILE_LOADGAME_SLOT10 },
  { "FileLoadGameAutoLoad", ID_FILE_LOADGAME_AUTOLOADMOSTRECENT },
  { "FileLoadGameRecent", ID_FILE_LOADGAME_MOSTRECENT },
  { "FileSaveGame01", ID_FILE_SAVEGAME_SLOT1 },
  { "FileSaveGame02", ID_FILE_SAVEGAME_SLOT2 },
  { "FileSaveGame03", ID_FILE_SAVEGAME_SLOT3 },
  { "FileSaveGame04", ID_FILE_SAVEGAME_SLOT4 },
  { "FileSaveGame05", ID_FILE_SAVEGAME_SLOT5 },
  { "FileSaveGame06", ID_FILE_SAVEGAME_SLOT6 },
  { "FileSaveGame07", ID_FILE_SAVEGAME_SLOT7 },
  { "FileSaveGame08", ID_FILE_SAVEGAME_SLOT8 },
  { "FileSaveGame09", ID_FILE_SAVEGAME_SLOT9 },
  { "FileSaveGame10", ID_FILE_SAVEGAME_SLOT10 },
  { "FileSaveGameOldest", ID_FILE_SAVEGAME_OLDESTSLOT },
  { "FileRecentReset", ID_FILE_RECENT_RESET },
  { "FileRecentFreeze", ID_FILE_RECENT_FREEZE },
  { "FileRecent01", ID_FILE_MRU_FILE1 },
  { "FileRecent02", ID_FILE_MRU_FILE2 },
  { "FileRecent03", ID_FILE_MRU_FILE3 },
  { "FileRecent04", ID_FILE_MRU_FILE4 },
  { "FileRecent05", ID_FILE_MRU_FILE5 },
  { "FileRecent06", ID_FILE_MRU_FILE6 },
  { "FileRecent07", ID_FILE_MRU_FILE7 },
  { "FileRecent08", ID_FILE_MRU_FILE8 },
  { "FileRecent09", ID_FILE_MRU_FILE9 },
  { "FileRecent10", ID_FILE_MRU_FILE10 },
  { "FilePause", ID_FILE_PAUSE },
  { "FileReset", ID_FILE_RESET },
  { "FileImportBatteryFile", ID_FILE_IMPORT_BATTERYFILE },
  { "FileImportGamesharkCodeFile", ID_FILE_IMPORT_GAMESHARKCODEFILE },
  { "FileImportGamesharkSnapshot", ID_FILE_IMPORT_GAMESHARKSNAPSHOT },
  { "FileExportBatteryFile", ID_FILE_EXPORT_BATTERYFILE },
  { "FileExportGamesharkSnapshot", ID_FILE_EXPORT_GAMESHARKSNAPSHOT },
  { "FileScreenCapture", ID_FILE_SCREENCAPTURE },
  { "FileRomInformation", ID_FILE_ROMINFORMATION },
  { "FileToggleMenu", ID_FILE_TOGGLEMENU },
  { "FileClose", ID_FILE_CLOSE },
  { "FileExit", ID_FILE_EXIT },
  { "OptionsFrameSkip0", ID_OPTIONS_VIDEO_FRAMESKIP_0 },
  { "OptionsFrameSkip1", ID_OPTIONS_VIDEO_FRAMESKIP_1 },
  { "OptionsFrameSkip2", ID_OPTIONS_VIDEO_FRAMESKIP_2 },
  { "OptionsFrameSkip3", ID_OPTIONS_VIDEO_FRAMESKIP_3 },
  { "OptionsFrameSkip4", ID_OPTIONS_VIDEO_FRAMESKIP_4 },
  { "OptionsFrameSkip5", ID_OPTIONS_VIDEO_FRAMESKIP_5 },
  { "OptionsFrameSkip6", ID_OPTIONS_VIDEO_FRAMESKIP_6 },
  { "OptionsFrameSkip7", ID_OPTIONS_VIDEO_FRAMESKIP_7 },
  { "OptionsFrameSkip8", ID_OPTIONS_VIDEO_FRAMESKIP_8 },
  { "OptionsFrameSkip9", ID_OPTIONS_VIDEO_FRAMESKIP_9 },
  { "OptionsThrottleNone", ID_OPTIONS_FRAMESKIP_THROTTLE_NOTHROTTLE },
  { "OptionsThrottle025%", ID_OPTIONS_FRAMESKIP_THROTTLE_25 },
  { "OptionsThrottle050%", ID_OPTIONS_FRAMESKIP_THROTTLE_50 },
  { "OptionsThrottle100%", ID_OPTIONS_FRAMESKIP_THROTTLE_100 },
  { "OptionsThrottle150%", ID_OPTIONS_FRAMESKIP_THROTTLE_150 },
  { "OptionsThrottle200%", ID_OPTIONS_FRAMESKIP_THROTTLE_200 },
  { "OptionsThrottleOther", ID_OPTIONS_FRAMESKIP_THROTTLE_OTHER },
  { "OptionsVideoRenderGDI", ID_OPTIONS_VIDEO_RENDERMETHOD_GDI },
  { "OptionsVideoRenderDDRAW", ID_OPTIONS_VIDEO_RENDERMETHOD_DIRECTDRAW },
  { "OptionsVideoRenderD3D", ID_OPTIONS_VIDEO_RENDERMETHOD_DIRECT3D },
  { "OptionsVideoRenderOGL", ID_OPTIONS_VIDEO_RENDERMETHOD_OPENGL },
  { "OptionsVideoRenderSelectSkin", ID_OPTIONS_VIDEO_RENDEROPTIONS_SELECTSKIN },
  { "OptionsVideoRenderSkin", ID_OPTIONS_VIDEO_RENDEROPTIONS_SKIN },
  { "OptionsVideoVsync", ID_OPTIONS_VIDEO_VSYNC },
  { "OptionsVideoX1", ID_OPTIONS_VIDEO_X1 },
  { "OptionsVideoX2", ID_OPTIONS_VIDEO_X2 },
  { "OptionsVideoX3", ID_OPTIONS_VIDEO_X3 },
  { "OptionsVideoX4", ID_OPTIONS_VIDEO_X4 },
  { "OptionsVideo320x240", ID_OPTIONS_VIDEO_FULLSCREEN320X240 },
  { "OptionsVideo640x480", ID_OPTIONS_VIDEO_FULLSCREEN640X480 },
  { "OptionsVideo800x600", ID_OPTIONS_VIDEO_FULLSCREEN800X600 },
  { "OptionsVideoFullscreen", ID_OPTIONS_VIDEO_FULLSCREEN },
  { "OptionsVideoFullscreenMaxScale", ID_OPTIONS_VIDEO_FULLSCREENMAXSCALE },
  { "OptionsVideoLayersBg0", ID_OPTIONS_VIDEO_LAYERS_BG0 },
  { "OptionsVideoLayersBg1", ID_OPTIONS_VIDEO_LAYERS_BG1 },
  { "OptionsVideoLayersBg2", ID_OPTIONS_VIDEO_LAYERS_BG2 },
  { "OptionsVideoLayersBg3", ID_OPTIONS_VIDEO_LAYERS_BG3 },
  { "OptionsVideoLayersOBJ", ID_OPTIONS_VIDEO_LAYERS_OBJ },
  { "OptionsVideoLayersWIN0", ID_OPTIONS_VIDEO_LAYERS_WIN0 },
  { "OptionsVideoLayersWIN1", ID_OPTIONS_VIDEO_LAYERS_WIN1 },
  { "OptionsVideoLayersOBJWIN", ID_OPTIONS_VIDEO_LAYERS_OBJWIN },
  { "OptionsEmulatorAssociate", ID_OPTIONS_EMULATOR_ASSOCIATE },
  { "OptionsEmulatorDirectories", ID_OPTIONS_EMULATOR_DIRECTORIES },
  { "OptionsEmulatorSelectBIOS", ID_OPTIONS_EMULATOR_SELECTBIOSFILE },
  { "OptionsEmulatorUseBIOS", ID_OPTIONS_EMULATOR_USEBIOSFILE },
  { "OptionsEmulatorGameOverrides", ID_OPTIONS_EMULATOR_GAMEOVERRIDES },
  { "OptionsEmulatorSkipBIOS", ID_OPTIONS_EMULATOR_SKIPBIOS },
  { "OptionsEmulatorShowSpeedNone", ID_OPTIONS_EMULATOR_SHOWSPEED_NONE },
  { "OptionsEmulatorShowSpeedPercentage", ID_OPTIONS_EMULATOR_SHOWSPEED_PERCENTAGE },
  { "OptionsEmulatorShowSpeedDetailed", ID_OPTIONS_EMULATOR_SHOWSPEED_DETAILED },
  { "OptionsEmulatorShowSpeedTransparent", ID_OPTIONS_EMULATOR_SHOWSPEED_TRANSPARENT },
  { "OptionsEmulatorSpeedupToggle", ID_OPTIONS_EMULATOR_SPEEDUPTOGGLE },
  { "OptionsEmulatorAutoHideMenu", ID_OPTIONS_EMULATOR_AUTOHIDEMENU },
  { "OptionsEmulatorSaveAuto", ID_OPTIONS_EMULATOR_SAVETYPE_AUTOMATIC },
  { "OptionsEmulatorSaveEEPROM", ID_OPTIONS_EMULATOR_SAVETYPE_EEPROM },
  { "OptionsEmulatorSaveSRAM", ID_OPTIONS_EMULATOR_SAVETYPE_SRAM },
  { "OptionsEmulatorSaveFLASH", ID_OPTIONS_EMULATOR_SAVETYPE_FLASH },
  { "OptionsEmulatorSaveEEPROMSensor", ID_OPTIONS_EMULATOR_SAVETYPE_EEPROMSENSOR },
  { "OptionsEmulatorSaveFlash64K", ID_OPTIONS_EMULATOR_SAVETYPE_FLASH512K },
  { "OptionsEmulatorSaveFlash128K", ID_OPTIONS_EMULATOR_SAVETYPE_FLASH1M },
  { "OptionsEmulatorAutoIPSPatch", ID_OPTIONS_EMULATOR_AUTOMATICALLYIPSPATCH },
  { "OptionsEmulatorAGBPrint", ID_OPTIONS_EMULATOR_AGBPRINT },
  { "OptionsEmulatorRTC", ID_OPTIONS_EMULATOR_REALTIMECLOCK },
  { "OptionsEmulatorGenericflashcard", ID_OPTIONS_EMULATOR_GENERICFLASHCARD },
  { "OptionsEmulatorRewindInterval", ID_OPTIONS_EMULATOR_REWINDINTERVAL },
  { "OptionsSoundOff", ID_OPTIONS_SOUND_OFF },
  { "OptionsSoundMute", ID_OPTIONS_SOUND_MUTE },
  { "OptionsSoundOn", ID_OPTIONS_SOUND_ON },
  { "OptionsSoundChannel1", ID_OPTIONS_SOUND_CHANNEL1 },
  { "OptionsSoundChannel2", ID_OPTIONS_SOUND_CHANNEL2 },
  { "OptionsSoundChannel3", ID_OPTIONS_SOUND_CHANNEL3 },
  { "OptionsSoundChannel4", ID_OPTIONS_SOUND_CHANNEL4 },
  { "OptionsSoundDirectSoundA", ID_OPTIONS_SOUND_DIRECTSOUNDA },
  { "OptionsSoundDirectSoundB", ID_OPTIONS_SOUND_DIRECTSOUNDB },
  { "OptionsSound11Khz", ID_OPTIONS_SOUND_11KHZ },
  { "OptionsSound22Khz", ID_OPTIONS_SOUND_22KHZ },
  { "OptionsSound44Khz", ID_OPTIONS_SOUND_44KHZ },
  { "OptionsSoundEcho", ID_OPTIONS_SOUND_ECHO },
  { "OptionsSoundLowPassFilter", ID_OPTIONS_SOUND_LOWPASSFILTER },
  { "OptionsSoundReverseStereo", ID_OPTIONS_SOUND_REVERSESTEREO },
  { "OptionsSoundVolume1x", ID_OPTIONS_SOUND_VOLUME_1X },
  { "OptionsSoundVolume2x", ID_OPTIONS_SOUND_VOLUME_2X },
  { "OptionsSoundVolume3x", ID_OPTIONS_SOUND_VOLUME_3X },
  { "OptionsSoundVolume4x", ID_OPTIONS_SOUND_VOLUME_4X },  
  { "OptionsGameboyBorder", ID_OPTIONS_GAMEBOY_BORDER },
  { "OptionsGameboyBorderAutomatic", ID_OPTIONS_GAMEBOY_BORDERAUTOMATIC },
  { "OptionsGameboyColors", ID_OPTIONS_GAMEBOY_COLORS },
  { "OptionsFilterNormal", ID_OPTIONS_FILTER_NORMAL },
  { "OptionsFilterTVMode", ID_OPTIONS_FILTER_TVMODE },
  { "OptionsFilter2xSaI", ID_OPTIONS_FILTER_2XSAI },
  { "OptionsFilterSuper2xSaI", ID_OPTIONS_FILTER_SUPER2XSAI },
  { "OptionsFilterSuperEagle", ID_OPTIONS_FILTER_SUPEREAGLE },
  { "OptionsFilterPixelate", ID_OPTIONS_FILTER16BIT_PIXELATEEXPERIMENTAL },
  { "OptionsFilterMotionBlur", ID_OPTIONS_FILTER16BIT_MOTIONBLUREXPERIMENTAL },
  { "OptionsFilterAdMameScale2x", ID_OPTIONS_FILTER16BIT_ADVANCEMAMESCALE2X },
  { "OptionsFilterSimple2x", ID_OPTIONS_FILTER16BIT_SIMPLE2X },
  { "OptionsFilterBilinear", ID_OPTIONS_FILTER_BILINEAR },
  { "OptionsFilterBilinearPlus", ID_OPTIONS_FILTER_BILINEARPLUS },
  { "OptionsFilterScanlines", ID_OPTIONS_FILTER_SCANLINES },
  { "OptionsFilterHq2x", ID_OPTIONS_FILTER_HQ2X },
  { "OptionsFilterLq2x", ID_OPTIONS_FILTER_LQ2X },
  { "OptionsFilterIFBNone", ID_OPTIONS_FILTER_INTERFRAMEBLENDING_NONE },
  { "OptionsFilterIFBMotionBlur", ID_OPTIONS_FILTER_INTERFRAMEBLENDING_MOTIONBLUR },
  { "OptionsFilterIFBSmart", ID_OPTIONS_FILTER_INTERFRAMEBLENDING_SMART },
  { "OptionsFilterDisableMMX", ID_OPTIONS_FILTER_DISABLEMMX },
  { "OptionsJoypadConfigure1", ID_OPTIONS_JOYPAD_CONFIGURE_1 },
  { "OptionsJoypadConfigure2", ID_OPTIONS_JOYPAD_CONFIGURE_2 },
  { "OptionsJoypadConfigure3", ID_OPTIONS_JOYPAD_CONFIGURE_3 },
  { "OptionsJoypadConfigure4", ID_OPTIONS_JOYPAD_CONFIGURE_4 },
  { "OptionsJoypadMotionConfigure", ID_OPTIONS_JOYPAD_MOTIONCONFIGURE },
  { "OptionsJoypadAutofireA", ID_OPTIONS_JOYPAD_AUTOFIRE_A },
  { "OptionsJoypadAutofireB", ID_OPTIONS_JOYPAD_AUTOFIRE_B },
  { "OptionsJoypadAutofireL", ID_OPTIONS_JOYPAD_AUTOFIRE_L },
  { "OptionsJoypadAutofireR", ID_OPTIONS_JOYPAD_AUTOFIRE_R },
  { "CheatsSearch", ID_CHEATS_SEARCHFORCHEATS },
  { "CheatsList", ID_CHEATS_CHEATLIST },
  { "CheatsLoad", ID_CHEATS_LOADCHEATLIST },
  { "CheatsSave", ID_CHEATS_SAVECHEATLIST },
  { "CheatsDisable", ID_CHEATS_DISABLECHEATS },
  { "ToolsDebugGDB", ID_TOOLS_DEBUG_GDB },
  { "ToolsDebugGDBLoad", ID_TOOLS_DEBUG_LOADANDWAIT },
  { "ToolsDebugGDBBreak", ID_TOOLS_DEBUG_BREAK },
  { "ToolsDebugGDBDisconnect", ID_TOOLS_DEBUG_DISCONNECT },
  { "ToolsDisassemble", ID_TOOLS_DISASSEMBLE },
  { "ToolsIOViewer", ID_TOOLS_IOVIEWER },
  { "ToolsLogging", ID_TOOLS_LOGGING },
  { "ToolsMapViewer", ID_TOOLS_MAPVIEW },
  { "ToolsMemoryViewer", ID_TOOLS_MEMORYVIEWER },
  { "ToolsOAMViewer", ID_TOOLS_OAMVIEWER },
  { "ToolsPaletteViewer", ID_TOOLS_PALETTEVIEW },
  { "ToolsTileViewer", ID_TOOLS_TILEVIEWER },
  { "ToolsNextFrame", ID_DEBUG_NEXTFRAME },
  { "ToolsRecordSoundStartRecording", ID_OPTIONS_SOUND_STARTRECORDING },
  { "ToolsRecordSoundStopRecording", ID_OPTIONS_SOUND_STOPRECORDING },
  { "ToolsRecordAVIStartRecording", ID_TOOLS_RECORD_STARTAVIRECORDING },
  { "ToolsRecordAVIStopRecording", ID_TOOLS_RECORD_STOPAVIRECORDING },
  { "ToolsRecordMovieStartRecording", ID_TOOLS_RECORD_STARTMOVIERECORDING },
  { "ToolsRecordMovieStopRecording", ID_TOOLS_RECORD_STOPMOVIERECORDING },
  { "ToolsPlayMovieStartPlaying", ID_TOOLS_PLAY_STARTMOVIEPLAYING },
  { "ToolsPlayMovieStopPlaying", ID_TOOLS_PLAY_STOPMOVIEPLAYING },
  { "ToolsRewind", ID_TOOLS_REWIND },
  { "ToolsCustomize", ID_TOOLS_CUSTOMIZE },
  { "HelpBugReport", ID_HELP_BUGREPORT },
  { "HelpFAQ", ID_HELP_FAQ },
  { "HelpAbout", ID_HELP_ABOUT },
  { "SystemMinimize", ID_SYSTEM_MINIMIZE }
};

bool winAccelGetID(const char *command, WORD& id)
{
  if(!initialized) {
    int count = sizeof(winAccelCommands)/sizeof(winAccelCommands[0]);
    
    for(int i = 0; i < count; i++) {
      winAccelStrings.SetAt(winAccelCommands[i].command, winAccelCommands[i].id);      
    }
    initialized = true;
  }
  
  return winAccelStrings.Lookup(command, id) ? true : false;
}

void winAccelAddCommands(CAcceleratorManager& mgr)
{
  int count = sizeof(winAccelCommands)/sizeof(winAccelCommands[0]);

  for(int i = 0; i < count; i++) {
    if(!mgr.AddCommandAccel(winAccelCommands[i].id, winAccelCommands[i].command, false))
      mgr.CreateEntry(winAccelCommands[i].id, winAccelCommands[i].command);
  }

}
