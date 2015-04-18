#include "stdafx.h"
#include "MainWnd.h"

#include <shlwapi.h>

#include "ExportGSASnapshot.h"
#include "FileDlg.h"
#include "GSACodeSelect.h"
#include "RomInfo.h"
#include "Reg.h"
#include "WinResUtil.h"
#include "Logging.h"

#include "../gba/GBA.h"
#include "../gba/Globals.h"
#include "../NLS.h"
#include "../gba/Sound.h"
#include "../gb/GB.h"
#include "../gb/gbCheats.h"
#include "../gb/gbGlobals.h"

#include "../version.h"

extern void remoteCleanUp();
extern void InterframeCleanup();


void MainWnd::OnFileOpenGBA()
{
	if( fileOpenSelect( 0 ) ) {
		FileRun();
	}
}

void MainWnd::OnFileOpenGBC()
{
	if( fileOpenSelect( 1 ) ) {
		FileRun();
	}
}

void MainWnd::OnFileOpenGB()
{
	if( fileOpenSelect( 2 ) ) {
		FileRun();
	}
}

void MainWnd::OnFilePause()
{
  paused = !paused;
  if(emulating) {
    if(paused) {
      wasPaused = true;
      soundPause();
    } else {
      soundResume();
    }
  }
}

void MainWnd::OnUpdateFilePause(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(paused);
}

void MainWnd::OnFileReset()
{

  if(emulating) {
    if(theApp.cartridgeType == IMAGE_GB) {
      gbGetHardwareType();
      if (gbHardware & 5) {
        gbCPUInit(theApp.biosFileNameGB, useBiosFileGB);
      } else if (gbHardware & 2) {
        gbCPUInit(theApp.biosFileNameGBC, useBiosFileGBC);
      }
    }
    theApp.emulator.emuReset();
    systemScreenMessage(winResLoadString(IDS_RESET));
  }
}

void MainWnd::OnUpdateFileReset(CCmdUI* pCmdUI)
{
  pCmdUI->Enable(emulating);
}

void MainWnd::OnUpdateFileRecentFreeze(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(recentFreeze);

  if(pCmdUI->m_pMenu == NULL)
    return;

  CMenu *pMenu = pCmdUI->m_pMenu;

  int i;
  for(i = 0; i < 10; i++) {
    if(!pMenu->RemoveMenu(ID_FILE_MRU_FILE1+i, MF_BYCOMMAND))
      break;
  }

  for(i = 0; i < 10; i++) {
    CString p = theApp.recentFiles[i];
    if(p.GetLength() == 0)
      break;
    int index = p.ReverseFind('\\');

    if(index != -1)
      p = p.Right(p.GetLength()-index-1);

    pMenu->AppendMenu(MF_STRING, ID_FILE_MRU_FILE1+i, p);
  }
  theApp.winAccelMgr.UpdateMenu((HMENU)*pMenu);
}

BOOL MainWnd::OnFileRecentFile(UINT nID)
{
  if(theApp.recentFiles[(nID&0xFFFF)-ID_FILE_MRU_FILE1].GetLength()) {
    theApp.szFile = theApp.recentFiles[(nID&0xFFFF)-ID_FILE_MRU_FILE1];
    if(FileRun())
      emulating = true;
    else {
      emulating = false;
      soundPause();
    }
  }
  return TRUE;
}

void MainWnd::OnFileRecentReset()
{
  int i = 0;
  for(i = 0; i < 10; i++)
    theApp.recentFiles[i] = "";
}

void MainWnd::OnFileRecentFreeze()
{
  recentFreeze = !recentFreeze;
}

void MainWnd::OnFileExit()
{
  SendMessage(WM_CLOSE);
}

void MainWnd::OnFileClose()
{
  // save battery file before we change the filename...
  if(rom != NULL || gbRom != NULL) {
    if(autoSaveLoadCheatList)
      winSaveCheatListDefault();
    writeBatteryFile();
    soundPause();
    theApp.emulator.emuCleanUp();
    remoteCleanUp();
  }
  emulating = 0;
  RedrawWindow(NULL,NULL,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN);
  systemSetTitle(VBA_NAME_AND_SUBVERSION);
}

void MainWnd::OnUpdateFileClose(CCmdUI* pCmdUI)
{
  pCmdUI->Enable(emulating);
}

void MainWnd::OnFileLoad()
{
  CString buffer;
  CString filename;

  int index = theApp.filename.ReverseFind('\\');

  if(index != -1)
    buffer = theApp.filename.Right(theApp.filename.GetLength()-index-1);
  else
    buffer = theApp.filename;

  CString saveDir = regQueryStringValue("saveDir", NULL);
  if( saveDir[0] == '.' ) {
	  // handle as relative path
	  char baseDir[MAX_PATH+1];
	  GetModuleFileName( NULL, baseDir, MAX_PATH );
	  baseDir[MAX_PATH] = '\0'; // for security reasons
	  PathRemoveFileSpec( baseDir ); // removes the trailing file name and backslash
	  strcat( baseDir, "\\" );
	  strcat( baseDir, saveDir );
	  saveDir = baseDir;
	}

  if(saveDir.IsEmpty())
    saveDir = getDirFromFile(theApp.filename);

  if(isDriveRoot(saveDir))
    filename.Format("%s%s.sgm", saveDir, buffer);
  else
    filename.Format("%s\\%s.sgm", saveDir, buffer);

  LPCTSTR exts[] = { ".sgm" };
  CString filter = winLoadFilter(IDS_FILTER_SGM);
  CString title = winResLoadString(IDS_SELECT_SAVE_GAME_NAME);

  FileDlg dlg(this, filename, filter, 0, "", exts, saveDir, title, false);

  if(dlg.DoModal() == IDOK) {
    bool res = loadSaveGame(dlg.GetPathName());

    rewindCount = 0;
    rewindCounter = 0;
    rewindSaveNeeded = false;

    if(res)
      systemScreenMessage(winResLoadString(IDS_LOADED_STATE));
  }
}

void MainWnd::OnUpdateFileLoad(CCmdUI* pCmdUI)
{
  pCmdUI->Enable(emulating);
}

BOOL MainWnd::OnFileLoadSlot(UINT nID)
{
  nID = nID + 1 - ID_FILE_LOADGAME_SLOT1;

  CString buffer;
  CString filename;

  int index = theApp.filename.ReverseFind('\\');

  if(index != -1)
    buffer = theApp.filename.Right(theApp.filename.GetLength()-index-1);
  else
    buffer = theApp.filename;

  CString saveDir = regQueryStringValue("saveDir", NULL);
  if( saveDir[0] == '.' ) {
	  // handle as relative path
	  char baseDir[MAX_PATH+1];
	  GetModuleFileName( NULL, baseDir, MAX_PATH );
	  baseDir[MAX_PATH] = '\0'; // for security reasons
	  PathRemoveFileSpec( baseDir ); // removes the trailing file name and backslash
	  strcat( baseDir, "\\" );
	  strcat( baseDir, saveDir );
	  saveDir = baseDir;
	}

  if(saveDir.IsEmpty())
    saveDir = getDirFromFile(theApp.filename);

  if(isDriveRoot(saveDir))
    filename.Format("%s%s%d.sgm", saveDir, buffer, nID);
  else
    filename.Format("%s\\%s%d.sgm", saveDir, buffer, nID);

  CString format = winResLoadString(IDS_LOADED_STATE_N);
  buffer.Format(format, nID);

  bool res = loadSaveGame(filename);

  if (paused)
    InterframeCleanup();

  systemScreenMessage(buffer);

  systemDrawScreen();


  return res;
}

void MainWnd::OnFileSave()
{
  CString buffer;
  CString filename;

  int index = theApp.filename.ReverseFind('\\');

  if(index != -1)
    buffer = theApp.filename.Right(theApp.filename.GetLength()-index-1);
  else
    buffer = theApp.filename;

  CString saveDir = regQueryStringValue("saveDir", NULL);
  if( saveDir[0] == '.' ) {
	  // handle as relative path
	  char baseDir[MAX_PATH+1];
	  GetModuleFileName( NULL, baseDir, MAX_PATH );
	  baseDir[MAX_PATH] = '\0'; // for security reasons
	  PathRemoveFileSpec( baseDir ); // removes the trailing file name and backslash
	  strcat( baseDir, "\\" );
	  strcat( baseDir, saveDir );
	  saveDir = baseDir;
	}

  if(saveDir.IsEmpty())
    saveDir = getDirFromFile(theApp.filename);

  if(isDriveRoot(saveDir))
    filename.Format("%s%s.sgm", saveDir, buffer);
  else
    filename.Format("%s\\%s.sgm", saveDir, buffer);

  LPCTSTR exts[] = { ".sgm" };
  CString filter = winLoadFilter(IDS_FILTER_SGM);
  CString title = winResLoadString(IDS_SELECT_SAVE_GAME_NAME);

  FileDlg dlg(this, filename, filter, 0, "", exts, saveDir, title, true);

  if(dlg.DoModal() == IDOK) {
    bool res = writeSaveGame(dlg.GetPathName());
    if(res)
      systemScreenMessage(winResLoadString(IDS_WROTE_STATE));
  }
}

void MainWnd::OnUpdateFileSave(CCmdUI* pCmdUI)
{
  pCmdUI->Enable(emulating);
}

BOOL MainWnd::OnFileSaveSlot(UINT nID)
{
  nID = nID + 1 - ID_FILE_SAVEGAME_SLOT1;

  CString buffer;
  CString filename;

  int index = theApp.filename.ReverseFind('\\');

  if(index != -1)
    buffer = theApp.filename.Right(theApp.filename.GetLength()-index-1);
  else
    buffer = theApp.filename;

  CString saveDir = regQueryStringValue("saveDir", NULL);
  if( saveDir[0] == '.' ) {
	  // handle as relative path
	  char baseDir[MAX_PATH+1];
	  GetModuleFileName( NULL, baseDir, MAX_PATH );
	  baseDir[MAX_PATH] = '\0'; // for security reasons
	  PathRemoveFileSpec( baseDir ); // removes the trailing file name and backslash
	  strcat( baseDir, "\\" );
	  strcat( baseDir, saveDir );
	  saveDir = baseDir;
	}

  if(saveDir.IsEmpty())
    saveDir = getDirFromFile(theApp.filename);

  if(isDriveRoot(saveDir))
    filename.Format("%s%s%d.sgm", saveDir, buffer, nID);
  else
    filename.Format("%s\\%s%d.sgm", saveDir, buffer, nID);

  bool res = writeSaveGame(filename);

  CString format = winResLoadString(IDS_WROTE_STATE_N);
  buffer.Format(format, nID);

  systemScreenMessage(buffer);

  systemDrawScreen();

  return res;
}

void MainWnd::OnFileImportBatteryfile()
{
  LPCTSTR exts[] = { ".sav", ".dat" };
  CString filter = winLoadFilter(IDS_FILTER_SAV);
  CString title = winResLoadString(IDS_SELECT_BATTERY_FILE);

  CString saveDir = regQueryStringValue("batteryDir", NULL);
  if( saveDir[0] == '.' ) {
	  // handle as relative path
	  char baseDir[MAX_PATH+1];
	  GetModuleFileName( NULL, baseDir, MAX_PATH );
	  baseDir[MAX_PATH] = '\0'; // for security reasons
	  PathRemoveFileSpec( baseDir ); // removes the trailing file name and backslash
	  strcat( baseDir, "\\" );
	  strcat( baseDir, saveDir );
	  saveDir = baseDir;
	}

  if(saveDir.IsEmpty())
    saveDir = getDirFromFile(theApp.filename);

  FileDlg dlg(this, "", filter, 0, "", exts, saveDir, title, false);

  if(dlg.DoModal() == IDCANCEL)
    return;

  CString str1 = winResLoadString(IDS_SAVE_WILL_BE_LOST);
  CString str2 = winResLoadString(IDS_CONFIRM_ACTION);

  if(MessageBox(str1,
                str2,
                MB_OKCANCEL) == IDCANCEL)
    return;

  bool res = false;

  res = theApp.emulator.emuReadBattery(dlg.GetPathName());

  if(!res)
    systemMessage(MSG_CANNOT_OPEN_FILE, "Cannot open file %s", dlg.GetPathName());
  else {
    //Removed the reset to allow loading a battery file 'within' a save state.
    //theApp.emulator.emuReset();
  }
}

void MainWnd::OnUpdateFileImportBatteryfile(CCmdUI* pCmdUI)
{
  pCmdUI->Enable(emulating);
}

void MainWnd::OnFileImportGamesharkcodefile()
{
  LPCTSTR exts[] = { "" };
  CString filter = theApp.cartridgeType == 0 ? winLoadFilter(IDS_FILTER_SPC) : winLoadFilter(IDS_FILTER_GCF);
  CString title = winResLoadString(IDS_SELECT_CODE_FILE);

  FileDlg dlg(this, "", filter, 0, "", exts, "", title, false);

  if(dlg.DoModal() == IDCANCEL)
    return;

  CString str1 = winResLoadString(IDS_CODES_WILL_BE_LOST);
  CString str2 = winResLoadString(IDS_CONFIRM_ACTION);

  if(MessageBox(str1,
                str2,
                MB_OKCANCEL) == IDCANCEL)
    return;

  bool res = false;
  CString file = dlg.GetPathName();
  if(theApp.cartridgeType == 1)
    res = gbCheatReadGSCodeFile(file);
  else {
    res = fileImportGSACodeFile(file);
  }
}

void MainWnd::OnUpdateFileImportGamesharkcodefile(CCmdUI* pCmdUI)
{
  pCmdUI->Enable(emulating);
}

void MainWnd::OnFileImportGamesharksnapshot()
{
  LPCTSTR exts[] = { ".?ps", ".gsv" };
  CString filter = theApp.cartridgeType == 1 ? winLoadFilter(IDS_FILTER_GBS) : winLoadFilter(IDS_FILTER_GSVSPS);
  CString title = winResLoadString(IDS_SELECT_SNAPSHOT_FILE);

  FileDlg dlg(this, "", filter, 0, "", exts, "", title, false);

  if(dlg.DoModal() == IDCANCEL)
    return;

  CString str1 = winResLoadString(IDS_SAVE_WILL_BE_LOST);
  CString str2 = winResLoadString(IDS_CONFIRM_ACTION);

  if(MessageBox(str1,
                str2,
                MB_OKCANCEL) == IDCANCEL)
    return;

  if(theApp.cartridgeType == IMAGE_GB && dlg.getFilterIndex() == 1)
    gbReadGSASnapshot(dlg.GetPathName());
  else if(theApp.cartridgeType == IMAGE_GB && dlg.getFilterIndex() == 2) {
	/* gameboy .gsv saves? */ }
  else if(theApp.cartridgeType == IMAGE_GBA && dlg.getFilterIndex() == 2)
	CPUReadGSASPSnapshot(dlg.GetPathName());
  else
    CPUReadGSASnapshot(dlg.GetPathName());
}

void MainWnd::OnUpdateFileImportGamesharksnapshot(CCmdUI* pCmdUI)
{
  pCmdUI->Enable(emulating);
}

void MainWnd::OnFileExportBatteryfile()
{
  CString name;

  int index = theApp.filename.ReverseFind('\\');

  if(index != -1)
    name = theApp.filename.Right(theApp.filename.GetLength()-index-1);
  else
    name = theApp.filename;

  LPCTSTR exts[] = {".sav", ".dat" };

  CString filter = winLoadFilter(IDS_FILTER_SAV);
  CString title = winResLoadString(IDS_SELECT_BATTERY_FILE);

  CString saveDir = regQueryStringValue("batteryDir", NULL);
  if( saveDir[0] == '.' ) {
	  // handle as relative path
	  char baseDir[MAX_PATH+1];
	  GetModuleFileName( NULL, baseDir, MAX_PATH );
	  baseDir[MAX_PATH] = '\0'; // for security reasons
	  PathRemoveFileSpec( baseDir ); // removes the trailing file name and backslash
	  strcat( baseDir, "\\" );
	  strcat( baseDir, saveDir );
	  saveDir = baseDir;
	}

  if(saveDir.IsEmpty())
    saveDir = getDirFromFile(theApp.filename);

  FileDlg dlg(this,
              name,
              filter,
              1,
              "SAV",
              exts,
              saveDir,
              title,
              true);

  if(dlg.DoModal() == IDCANCEL) {
    return;
  }

  bool result = false;

  if(theApp.cartridgeType == 1) {
    result = gbWriteBatteryFile(dlg.GetPathName(), false);
  } else
    result = theApp.emulator.emuWriteBattery(dlg.GetPathName());

  if(!result)
    systemMessage(MSG_ERROR_CREATING_FILE, "Error creating file %s",
                  dlg.GetPathName());
}

void MainWnd::OnUpdateFileExportBatteryfile(CCmdUI* pCmdUI)
{
  pCmdUI->Enable(emulating);
}

void MainWnd::OnFileExportGamesharksnapshot()
{
  if(eepromInUse) {
    systemMessage(IDS_EEPROM_NOT_SUPPORTED, "EEPROM saves cannot be exported");
    return;
  }

  CString name;

  int index = theApp.filename.ReverseFind('\\');

  if(index != -1)
    name = theApp.filename.Right(theApp.filename.GetLength()-index-1);
  else
    name = theApp.filename;

  LPCTSTR exts[] = {".sps" };

  CString filter = winLoadFilter(IDS_FILTER_SPS);
  CString title = winResLoadString(IDS_SELECT_SNAPSHOT_FILE);

  FileDlg dlg(this,
              name,
              filter,
              1,
              "SPS",
              exts,
              "",
              title,
              true);

  if(dlg.DoModal() == IDCANCEL)
    return;

  char buffer[16];
  strncpy(buffer, (const char *)&rom[0xa0], 12);
  buffer[12] = 0;

  ExportGSASnapshot dlg2(dlg.GetPathName(), buffer, this);
  dlg2.DoModal();
}

void MainWnd::OnUpdateFileExportGamesharksnapshot(CCmdUI* pCmdUI)
{
  pCmdUI->Enable(emulating && theApp.cartridgeType == 0);
}

void MainWnd::OnFileScreencapture()
{
  CString name;
  CString filename;

  int index = theApp.filename.ReverseFind('\\');

  if(index != -1)
    name = theApp.filename.Right(theApp.filename.GetLength()-index-1);
  else
    name = theApp.filename;

  CString capdir = regQueryStringValue("captureDir", "");
  if( capdir[0] == '.' ) {
	  // handle as relative path
	  char baseDir[MAX_PATH+1];
	  GetModuleFileName( NULL, baseDir, MAX_PATH );
	  baseDir[MAX_PATH] = '\0'; // for security reasons
	  PathRemoveFileSpec( baseDir ); // removes the trailing file name and backslash
	  strcat( baseDir, "\\" );
	  strcat( baseDir, capdir );
	  capdir = baseDir;
	}

  if(capdir.IsEmpty())
    capdir = getDirFromFile(theApp.filename);

  CString ext = "png";

  if(captureFormat != 0)
    ext = "bmp";

  if(isDriveRoot(capdir))
    filename.Format("%s%s.%s", capdir, name, ext);
  else
    filename.Format("%s\\%s.%s", capdir, name, ext);

  LPCTSTR exts[] = {".png", ".bmp" };

  CString filter = winLoadFilter(IDS_FILTER_PNG);
  CString title = winResLoadString(IDS_SELECT_CAPTURE_NAME);

  FileDlg dlg(this,
              filename,
              filter,
              captureFormat ? 2 : 1,
              captureFormat ? "BMP" : "PNG",
              exts,
              capdir,
              title,
              true);

  if(dlg.DoModal() == IDCANCEL)
    return;

  if(dlg.getFilterIndex() == 2)
    theApp.emulator.emuWriteBMP(dlg.GetPathName());
  else
    theApp.emulator.emuWritePNG(dlg.GetPathName());

  systemScreenMessage(winResLoadString(IDS_SCREEN_CAPTURE));
}

void MainWnd::OnUpdateFileScreencapture(CCmdUI* pCmdUI)
{
  pCmdUI->Enable(emulating);
}

void MainWnd::OnFileRominformation()
{
  if(theApp.cartridgeType == 0) {
    RomInfoGBA dlg(rom);
    dlg.DoModal();
  } else {
    RomInfoGB dlg(gbRom);
    dlg.DoModal();
  }
}

void MainWnd::OnUpdateFileRominformation(CCmdUI* pCmdUI)
{
  pCmdUI->Enable(emulating);
}

//OnFileToggleFullscreen
void MainWnd::OnFileTogglemenu()
{
	if( videoOption <= VIDEO_6X ) {
		// switch to full screen
		toolsLoggingClose(); // close log dialog
		theApp.updateWindowSize( theApp.lastFullscreen );
	} else {
		// switch to windowed mode
		theApp.updateWindowSize( theApp.lastWindowed );
	}
}

void MainWnd::OnUpdateFileTogglemenu(CCmdUI* pCmdUI)
{
	// HACK: when uncommented, Esc key will not be send to MainWnd
	//pCmdUI->Enable(videoOption > VIDEO_6X);
}

bool MainWnd::fileImportGSACodeFile(CString& fileName)
{
  FILE *f = fopen(fileName, "rb");

  if(f == NULL) {
    systemMessage(MSG_CANNOT_OPEN_FILE, "Cannot open file %s", fileName);
    return false;
  }

  u32 len;
  fread(&len, 1, 4, f);
  if(len != 14) {
    fclose(f);
    systemMessage(MSG_UNSUPPORTED_CODE_FILE, "Unsupported code file %s",
                  fileName);
    return false;
  }
  char buffer[16];
  fread(buffer, 1, 14, f);
  buffer[14] = 0;
  if(memcmp(buffer, "SharkPortCODES", 14)) {
    fclose(f);
    systemMessage(MSG_UNSUPPORTED_CODE_FILE, "Unsupported code file %s",
                  fileName);
    return false;
  }
  fseek(f, 0x1e, SEEK_SET);
  fread(&len, 1, 4, f);
  INT_PTR game = 0;
  if(len > 1) {
    GSACodeSelect dlg(f);
    game = dlg.DoModal();
  }
  fclose(f);

  bool v3 = false;

  int index = fileName.ReverseFind('.');

  if(index != -1) {
    if(fileName.Right(3).CompareNoCase("XPC") == 0)
      v3 = true;
  }

  if(game != -1) {
    return cheatsImportGSACodeFile(fileName, (int)game, v3);
  }

  return true;
}

void MainWnd::OnFileSavegameOldestslot()
{
  if(!emulating)
    return;

  CString filename;

  int index = theApp.filename.ReverseFind('\\');

  if(index != -1)
    filename = theApp.filename.Right(theApp.filename.GetLength()-index-1);
  else
    filename = theApp.filename;

  CString saveDir = regQueryStringValue("saveDir", NULL);
  if( saveDir[0] == '.' ) {
	  // handle as relative path
	  char baseDir[MAX_PATH+1];
	  GetModuleFileName( NULL, baseDir, MAX_PATH );
	  baseDir[MAX_PATH] = '\0'; // for security reasons
	  PathRemoveFileSpec( baseDir ); // removes the trailing file name and backslash
	  strcat( baseDir, "\\" );
	  strcat( baseDir, saveDir );
	  saveDir = baseDir;
	}

  if(saveDir.IsEmpty())
    saveDir = getDirFromFile(theApp.filename);

  if(!isDriveRoot(saveDir))
    saveDir += "\\";

  CString name;
  CFileStatus status;
  CString str;
  time_t time = 0;
  int found = 0;

  for(int i = 0; i < 10; i++) {
    name.Format("%s%s%d.sgm", saveDir, filename, i+1);

    if(emulating && CFile::GetStatus(name, status)) {
      if( (status.m_mtime.GetTime() < time) || !time ) {
        time = status.m_mtime.GetTime();
        found = i;
      }
    } else {
      found = i;
      break;
    }
  }
  OnFileSaveSlot(ID_FILE_SAVEGAME_SLOT1+found);
}

void MainWnd::OnUpdateFileSavegameOldestslot(CCmdUI* pCmdUI)
{
  pCmdUI->Enable(emulating);
  if(pCmdUI->m_pSubMenu != NULL) {
    CMenu *pMenu = pCmdUI->m_pSubMenu;
    CString filename;

    int index = theApp.filename.ReverseFind('\\');

    if(index != -1)
      filename = theApp.filename.Right(theApp.filename.GetLength()-index-1);
    else
      filename = theApp.filename;

    CString saveDir = regQueryStringValue("saveDir", NULL);
	if( saveDir[0] == '.' ) {
		// handle as relative path
		char baseDir[MAX_PATH+1];
		GetModuleFileName( NULL, baseDir, MAX_PATH );
		baseDir[MAX_PATH] = '\0'; // for security reasons
		PathRemoveFileSpec( baseDir ); // removes the trailing file name and backslash
		strcat( baseDir, "\\" );
		strcat( baseDir, saveDir );
		saveDir = baseDir;
	}

    if(saveDir.IsEmpty())
      saveDir = getDirFromFile(theApp.filename);

    if(!isDriveRoot(saveDir))
      saveDir += "\\";

    CString name;
    CFileStatus status;
    CString str;

    for(int i = 0; i < 10; i++) {
      name.Format("%s%s%d.sgm", saveDir, filename, i+1);

      if(emulating && CFile::GetStatus(name, status)) {
        CString timestamp = status.m_mtime.Format("%Y/%m/%d %H:%M:%S");
        str.Format("%d %s", i+1, timestamp);
      } else {
        str.Format("%d ----/--/-- --:--:--", i+1);
      }
      pMenu->ModifyMenu(ID_FILE_SAVEGAME_SLOT1+i, MF_STRING|MF_BYCOMMAND, ID_FILE_SAVEGAME_SLOT1+i, str);
    }

    theApp.winAccelMgr.UpdateMenu(pMenu->GetSafeHmenu());
  }
}

void MainWnd::OnFileLoadgameMostrecent()
{
  if(!emulating)
    return;

  CString filename;

  int index = theApp.filename.ReverseFind('\\');

  if(index != -1)
    filename = theApp.filename.Right(theApp.filename.GetLength()-index-1);
  else
    filename = theApp.filename;

  CString saveDir = regQueryStringValue("saveDir", NULL);
  if( saveDir[0] == '.' ) {
	  // handle as relative path
	  char baseDir[MAX_PATH+1];
	  GetModuleFileName( NULL, baseDir, MAX_PATH );
	  baseDir[MAX_PATH] = '\0'; // for security reasons
	  PathRemoveFileSpec( baseDir ); // removes the trailing file name and backslash
	  strcat( baseDir, "\\" );
	  strcat( baseDir, saveDir );
	  saveDir = baseDir;
	}

  if(saveDir.IsEmpty())
    saveDir = getDirFromFile(theApp.filename);

  if(!isDriveRoot(saveDir))
    saveDir += "\\";

  CString name;
  CFileStatus status;
  CString str;
  time_t time = 0;
  int found = -1;

  for(int i = 0; i < 10; i++) {
    name.Format("%s%s%d.sgm", saveDir, filename, i+1);

    if(emulating && CFile::GetStatus(name, status)) {
if(status.m_mtime.GetTime() > time) {
        time = status.m_mtime.GetTime();
        found = i;
      }
    }
  }
  if(found != -1) {
    OnFileLoadSlot(ID_FILE_LOADGAME_SLOT1+found);
  }
}

void MainWnd::OnUpdateFileLoadgameMostrecent(CCmdUI* pCmdUI)
{
  pCmdUI->Enable(emulating);

  if(pCmdUI->m_pSubMenu != NULL) {
    CMenu *pMenu = pCmdUI->m_pSubMenu;
    CString filename;

    int index = theApp.filename.ReverseFind('\\');

    if(index != -1)
      filename = theApp.filename.Right(theApp.filename.GetLength()-index-1);
    else
      filename = theApp.filename;

    CString saveDir = regQueryStringValue("saveDir", NULL);
	if( saveDir[0] == '.' ) {
		// handle as relative path
		char baseDir[MAX_PATH+1];
		GetModuleFileName( NULL, baseDir, MAX_PATH );
		baseDir[MAX_PATH] = '\0'; // for security reasons
		PathRemoveFileSpec( baseDir ); // removes the trailing file name and backslash
		strcat( baseDir, "\\" );
		strcat( baseDir, saveDir );
		saveDir = baseDir;
	}

    if(saveDir.IsEmpty())
      saveDir = getDirFromFile(theApp.filename);

    if(!isDriveRoot(saveDir))
      saveDir += "\\";

    CString name;
    CFileStatus status;
    CString str;

    for(int i = 0; i < 10; i++) {
      name.Format("%s%s%d.sgm", saveDir, filename, i+1);

      if(emulating && CFile::GetStatus(name, status)) {
        CString timestamp = status.m_mtime.Format("%Y/%m/%d %H:%M:%S");
        str.Format("%d %s", i+1, timestamp);
      } else {
        str.Format("%d ----/--/-- --:--:--", i+1);
      }
      pMenu->ModifyMenu(ID_FILE_LOADGAME_SLOT1+i, MF_STRING|MF_BYCOMMAND, ID_FILE_LOADGAME_SLOT1+i, str);
    }

    theApp.winAccelMgr.UpdateMenu(pMenu->GetSafeHmenu());
  }
}

void MainWnd::OnUpdateFileLoadGameSlot(CCmdUI *pCmdUI)
{
  pCmdUI->Enable(emulating);
}

void MainWnd::OnUpdateFileSaveGameSlot(CCmdUI *pCmdUI)
{
  pCmdUI->Enable(emulating);
}

void MainWnd::OnFileLoadgameAutoloadmostrecent()
{
  autoLoadMostRecent = !autoLoadMostRecent;
}

void MainWnd::OnUpdateFileLoadgameAutoloadmostrecent(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(autoLoadMostRecent);
}

void MainWnd::OnLoadgameDonotchangebatterysave()
{
  skipSaveGameBattery = !skipSaveGameBattery;
}

void MainWnd::OnUpdateLoadgameDonotchangebatterysave(CCmdUI *pCmdUI)
{
  pCmdUI->SetCheck(skipSaveGameBattery);
}

void MainWnd::OnLoadgameDonotchangecheatlist()
{
  skipSaveGameCheats = !skipSaveGameCheats;
}

void MainWnd::OnUpdateLoadgameDonotchangecheatlist(CCmdUI *pCmdUI)
{
  pCmdUI->SetCheck(skipSaveGameCheats);
}
