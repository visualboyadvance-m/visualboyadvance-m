// VisualBoyAdvance - Nintendo Gameboy/GameboyAdvance (TM) emulator.
// Copyright (C) 1999-2003 Forgotten
// Copyright (C) 2004 Forgotten and the VBA development team

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
#include "MainWnd.h"

#include "AccelEditor.h"
#include "AVIWrite.h"
#include "Disassemble.h"
#include "FileDlg.h"
#include "GBDisassemble.h"
#include "GBMapView.h"
#include "GBMemoryViewerDlg.h"
#include "GBOamView.h"
#include "GBPaletteView.h"
#include "GBTileView.h"
#include "GDBConnection.h"
#include "IOViewer.h"
#include "MapView.h"
#include "MemoryViewerDlg.h"
#include "OamView.h"
#include "PaletteView.h"
#include "Reg.h"
#include "TileView.h"
#include "WavWriter.h"
#include "WinResUtil.h"

#include "../GBA.h"
#include "../Globals.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

extern bool debugger;
extern int emulating;
extern SOCKET remoteSocket;

extern void remoteCleanUp();
extern void remoteSetSockets(SOCKET, SOCKET);
extern void toolsLogging();

void MainWnd::OnToolsDisassemble() 
{
  if(theApp.cartridgeType == 0) {
    Disassemble *dlg = new Disassemble();
    dlg->Create(IDD_DISASSEMBLE, this);
    dlg->ShowWindow(SW_SHOW);
  } else {
    GBDisassemble *dlg = new GBDisassemble();
    dlg->Create(IDD_GB_DISASSEMBLE, this);
    dlg->ShowWindow(SW_SHOW);
  }
}

void MainWnd::OnUpdateToolsDisassemble(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(theApp.videoOption <= VIDEO_4X);
}

void MainWnd::OnToolsLogging() 
{
  toolsLogging();
}

void MainWnd::OnUpdateToolsLogging(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(theApp.videoOption <= VIDEO_4X);
}

void MainWnd::OnToolsIoviewer() 
{
  IOViewer *dlg = new IOViewer;
  dlg->Create(IDD_IO_VIEWER,this);
  dlg->ShowWindow(SW_SHOW);
}

void MainWnd::OnUpdateToolsIoviewer(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(theApp.videoOption <= VIDEO_4X && theApp.cartridgeType == 0);
}

void MainWnd::OnToolsMapview() 
{
  if(theApp.cartridgeType == 0) {
    MapView *dlg = new MapView;
    dlg->Create(IDD_MAP_VIEW, this);
    dlg->ShowWindow(SW_SHOW);
  } else {
    GBMapView *dlg = new GBMapView;
    dlg->Create(IDD_GB_MAP_VIEW, this);
    dlg->ShowWindow(SW_SHOW);
  }
}

void MainWnd::OnUpdateToolsMapview(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(theApp.videoOption <= VIDEO_4X);
}

void MainWnd::OnToolsMemoryviewer() 
{
  if(theApp.cartridgeType == 0) {
    MemoryViewerDlg *dlg = new MemoryViewerDlg;
    dlg->Create(IDD_MEM_VIEWER, this);
    dlg->ShowWindow(SW_SHOW);
  } else {
    GBMemoryViewerDlg *dlg = new GBMemoryViewerDlg;
    dlg->Create(IDD_MEM_VIEWER, this);
    dlg->ShowWindow(SW_SHOW);
  }
}

void MainWnd::OnUpdateToolsMemoryviewer(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(theApp.videoOption <= VIDEO_4X);
}

void MainWnd::OnToolsOamviewer() 
{
  if(theApp.cartridgeType == 0) {
    OamView *dlg = new OamView;
    dlg->Create(IDD_OAM_VIEW, this);
    dlg->ShowWindow(SW_SHOW);
  } else {
    GBOamView *dlg = new GBOamView;
    dlg->Create(IDD_GB_OAM_VIEW, this);
    dlg->ShowWindow(SW_SHOW);
  }
}

void MainWnd::OnUpdateToolsOamviewer(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(theApp.videoOption <= VIDEO_4X);  
}

void MainWnd::OnToolsPaletteview() 
{
  if(theApp.cartridgeType == 0) {
    PaletteView *dlg = new PaletteView;
    dlg->Create(IDD_PALETTE_VIEW, this);
    dlg->ShowWindow(SW_SHOW);
  } else {
    GBPaletteView *dlg = new GBPaletteView;
    dlg->Create(IDD_GB_PALETTE_VIEW, this);
    dlg->ShowWindow(SW_SHOW);
  }
}

void MainWnd::OnUpdateToolsPaletteview(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(theApp.videoOption <= VIDEO_4X);  
}

void MainWnd::OnToolsTileviewer() 
{
  if(theApp.cartridgeType == 0) {
    TileView *dlg = new TileView;
    dlg->Create(IDD_TILE_VIEWER, this);
    dlg->ShowWindow(SW_SHOW);
  } else {
    GBTileView *dlg = new GBTileView;
    dlg->Create(IDD_GB_TILE_VIEWER, this);
    dlg->ShowWindow(SW_SHOW);
  }
}

void MainWnd::OnUpdateToolsTileviewer(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(theApp.videoOption <= VIDEO_4X);  
}

void MainWnd::OnDebugNextframe() 
{
  if(theApp.paused)
    theApp.paused = false;
  theApp.winPauseNextFrame = true;
}

void MainWnd::OnToolsDebugGdb() 
{
  theApp.winCheckFullscreen();
  GDBPortDlg dlg;

  if(dlg.DoModal()) {
    GDBWaitingDlg wait(dlg.getSocket(), dlg.getPort());
    if(wait.DoModal()) {
      remoteSetSockets(wait.getListenSocket(), wait.getSocket());
      debugger = true;
      emulating = 1;
      theApp.cartridgeType = IMAGE_GBA;
      theApp.filename = "\\gnu_stub";
      rom = (u8 *)malloc(0x2000000);
      workRAM = (u8 *)calloc(1, 0x40000);
      bios = (u8 *)calloc(1,0x4000);
      internalRAM = (u8 *)calloc(1,0x8000);
      paletteRAM = (u8 *)calloc(1,0x400);
      vram = (u8 *)calloc(1, 0x20000);
      oam = (u8 *)calloc(1, 0x400);
      pix = (u8 *)calloc(1, 4 * 241 * 162);
      ioMem = (u8 *)calloc(1, 0x400);
      
      theApp.emulator = GBASystem;
      
      CPUInit(theApp.biosFileName, theApp.useBiosFile ? true : false);
      CPUReset();    
    }
  }
}

void MainWnd::OnUpdateToolsDebugGdb(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(theApp.videoOption <= VIDEO_4X && remoteSocket == -1);
}

void MainWnd::OnToolsDebugLoadandwait() 
{
  theApp.winCheckFullscreen();
  if(fileOpenSelect()) {
    if(FileRun()) {
      if(theApp.cartridgeType != 0) {
        systemMessage(IDS_ERROR_NOT_GBA_IMAGE, "Error: not a GBA image");
        OnFileClose();
        return;
      }
      GDBPortDlg dlg;

      if(dlg.DoModal()) {
        GDBWaitingDlg wait(dlg.getSocket(), dlg.getPort());
        if(wait.DoModal()) {
          remoteSetSockets(wait.getListenSocket(), wait.getSocket());
          debugger = true;
          emulating = 1;
        }
      }
    }
  }
}

void MainWnd::OnUpdateToolsDebugLoadandwait(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(theApp.videoOption <= VIDEO_4X && remoteSocket == -1);
}

void MainWnd::OnToolsDebugBreak() 
{
  if(armState) {
    armNextPC -= 4;
    reg[15].I -= 4;
  } else {
    armNextPC -= 2;
    reg[15].I -= 2;
  }
  debugger = true;
}

void MainWnd::OnUpdateToolsDebugBreak(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(theApp.videoOption <= VIDEO_4X && remoteSocket != -1);
}

void MainWnd::OnToolsDebugDisconnect() 
{
  remoteCleanUp();
  debugger = false;
}

void MainWnd::OnUpdateToolsDebugDisconnect(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(theApp.videoOption <= VIDEO_4X && remoteSocket != -1);
}

void MainWnd::OnOptionsSoundStartrecording() 
{
  theApp.winCheckFullscreen();
  CString captureBuffer;

  CString capdir = regQueryStringValue("soundRecordDir", NULL);
  
  if(capdir.IsEmpty())
    capdir = getDirFromFile(theApp.filename);

  CString filter = theApp.winLoadFilter(IDS_FILTER_WAV);
  CString title = winResLoadString(IDS_SELECT_WAV_NAME);

  LPCTSTR exts[] = { ".WAV" };
  
  FileDlg dlg(this, "", filter, 1, "WAV", exts, capdir, title, true);
  
  if(dlg.DoModal() == IDCANCEL) {
    return;
  }

  captureBuffer = theApp.soundRecordName =  dlg.GetPathName();
  theApp.soundRecording = true;
  
  if(dlg.m_ofn.nFileOffset > 0) {
    captureBuffer = captureBuffer.Left(dlg.m_ofn.nFileOffset);
  }

  int len = captureBuffer.GetLength();

  if(len > 3 && captureBuffer[len-1] == '\\')
    captureBuffer = captureBuffer.Left(len-1);
  regSetStringValue("soundRecordDir", captureBuffer);
}

void MainWnd::OnUpdateOptionsSoundStartrecording(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(!theApp.soundRecording);
}

void MainWnd::OnOptionsSoundStoprecording() 
{
  if(theApp.soundRecorder) {
    delete theApp.soundRecorder;
    theApp.soundRecorder = NULL;
  }
  theApp.soundRecording = false;
}

void MainWnd::OnUpdateOptionsSoundStoprecording(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(theApp.soundRecording);
}

void MainWnd::OnToolsRecordStartavirecording() 
{
  theApp.winCheckFullscreen();
  CString captureBuffer;

  CString capdir = regQueryStringValue("aviRecordDir", NULL);
  
  if(capdir.IsEmpty())
    capdir = getDirFromFile(theApp.filename);

  CString filter = theApp.winLoadFilter(IDS_FILTER_AVI);
  CString title = winResLoadString(IDS_SELECT_AVI_NAME);

  LPCTSTR exts[] = { ".AVI" };
  
  FileDlg dlg(this, "", filter, 1, "AVI", exts, capdir, title, true);
  
  if(dlg.DoModal() == IDCANCEL) {
    return;
  }

  captureBuffer = theApp.soundRecordName =  dlg.GetPathName();
  theApp.aviRecordName = captureBuffer;
  theApp.aviRecording = true;
  
  if(dlg.m_ofn.nFileOffset > 0) {
    captureBuffer = captureBuffer.Left(dlg.m_ofn.nFileOffset);
  }

  int len = captureBuffer.GetLength();

  if(len > 3 && captureBuffer[len-1] == '\\')
    captureBuffer = captureBuffer.Left(len-1);

  regSetStringValue("aviRecordDir", captureBuffer);
}

void MainWnd::OnUpdateToolsRecordStartavirecording(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(!theApp.aviRecording);
}

void MainWnd::OnToolsRecordStopavirecording() 
{
  if(theApp.aviRecorder != NULL) {
    delete theApp.aviRecorder;
    theApp.aviRecorder = NULL;
    theApp.aviFrameNumber = 0;
  }
  theApp.aviRecording = false;
}

void MainWnd::OnUpdateToolsRecordStopavirecording(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(theApp.aviRecording);
}

void MainWnd::OnToolsRecordStartmovierecording() 
{
  theApp.winCheckFullscreen();
  CString captureBuffer;
  CString capdir = regQueryStringValue("movieRecordDir", "");
  
  if(capdir.IsEmpty())
    capdir = getDirFromFile(theApp.filename);

  CString filter = theApp.winLoadFilter(IDS_FILTER_VMV);
  CString title = winResLoadString(IDS_SELECT_MOVIE_NAME);
  
  LPCTSTR exts[] = { ".VMV" };

  FileDlg dlg(this, "", filter, 1, "VMV", exts, capdir, title, true);
  
  if(dlg.DoModal() == IDCANCEL) {
    return;
  }
  
  CString movieName = dlg.GetPathName();
  captureBuffer = movieName;
  
  if(dlg.m_ofn.nFileOffset > 0) {
    captureBuffer = captureBuffer.Left(dlg.m_ofn.nFileOffset);
  }

  int len = captureBuffer.GetLength();

  if(len > 3 && captureBuffer[len-1] == '\\')
    captureBuffer = captureBuffer.Left(len-1);

  regSetStringValue("movieRecordDir", captureBuffer);
  
  theApp.movieFile = fopen(movieName, "wb");

  if(!theApp.movieFile) {
    systemMessage(IDS_CANNOT_OPEN_FILE, "Cannot open file %s", 
                  (const char *)movieName);
    return;
  }

  int version = 1;

  fwrite(&version, 1, sizeof(int), theApp.movieFile);

  movieName = movieName.Left(movieName.GetLength()-3) + "VM0";

  if(writeSaveGame(movieName)) {
    theApp.movieFrame = 0;
    theApp.movieLastJoypad = 0;
    theApp.movieRecording = true;
    theApp.moviePlaying = false;
  } else {
    systemMessage(IDS_CANNOT_OPEN_FILE, "Cannot open file %s", 
                  (const char *)movieName);  
  }
}

void MainWnd::OnUpdateToolsRecordStartmovierecording(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(!theApp.movieRecording);
}

void MainWnd::OnToolsRecordStopmovierecording() 
{
  if(theApp.movieRecording) {
    if(theApp.movieFile != NULL) {
      // record the last joypad change so that the correct time can be
      // recorded
      fwrite(&theApp.movieFrame, 1, sizeof(int), theApp.movieFile);
      fwrite(&theApp.movieLastJoypad, 1, sizeof(u32), theApp.movieFile);
      fclose(theApp.movieFile);
      theApp.movieFile = NULL;
    }
    theApp.movieRecording = false;
    theApp.moviePlaying = false;
    theApp.movieLastJoypad = 0;
  }
}

void MainWnd::OnUpdateToolsRecordStopmovierecording(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(theApp.movieRecording);
}

void MainWnd::OnToolsPlayStartmovieplaying() 
{
  static bool moviePlayMessage = false;

  if(!moviePlayMessage) {
    moviePlayMessage = true;
    CString msg = winResLoadString(IDS_MOVIE_PLAY);
    CString title = winResLoadString(IDS_CONFIRM_ACTION);
    if(MessageBox(msg,
                  title,
                  MB_OKCANCEL) == IDCANCEL)
      return;
  }

  CString captureBuffer;
  CString capdir = regQueryStringValue("movieRecordDir", "");
  
  if(capdir.IsEmpty())
    capdir = getDirFromFile(theApp.filename);

  CString filter = theApp.winLoadFilter(IDS_FILTER_VMV);
  CString title = winResLoadString(IDS_SELECT_MOVIE_NAME);

  LPCTSTR exts[] = { ".VMV" };

  FileDlg dlg(this, "", filter, 1, "VMV", exts, capdir, title, false);
  
  if(dlg.DoModal() == IDCANCEL) {
    return;
  }

  CString movieName = dlg.GetPathName();
  captureBuffer = movieName;
  
  theApp.movieFile = fopen(movieName, "rb");

  if(!theApp.movieFile) {
    systemMessage(IDS_CANNOT_OPEN_FILE, "Cannot open file %s", 
                  (const char *)movieName);
    return;
  }
  int version = 0;
  fread(&version, 1, sizeof(int), theApp.movieFile);
  if(version != 1) {
    systemMessage(IDS_UNSUPPORTED_MOVIE_VERSION, 
                  "Unsupported movie version %d.",
                  version);
    fclose(theApp.movieFile);
    theApp.movieFile = NULL;
    return;
  }
  movieName = movieName.Left(movieName.GetLength()-3)+"VM0";
  if(loadSaveGame(movieName)) {
    theApp.moviePlaying = true;
    theApp.movieFrame = 0;
    theApp.moviePlayFrame = 0;
    theApp.movieLastJoypad = 0;
    theApp.movieReadNext();
  } else {
    systemMessage(IDS_CANNOT_OPEN_FILE, "Cannot open file %s", 
                  (const char *)movieName);
  }
}

void MainWnd::OnUpdateToolsPlayStartmovieplaying(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(!theApp.moviePlaying);
}

void MainWnd::OnToolsPlayStopmovieplaying() 
{
  if(theApp.moviePlaying) {
    if(theApp.movieFile != NULL) {
      fclose(theApp.movieFile);
      theApp.movieFile = NULL;
    }
    theApp.moviePlaying = false;
    theApp.movieLastJoypad = 0;
  }
}

void MainWnd::OnUpdateToolsPlayStopmovieplaying(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(theApp.moviePlaying);
}

void MainWnd::OnToolsRewind() 
{
  if(emulating && theApp.emulator.emuReadMemState && theApp.rewindMemory && theApp.rewindCount) {
    theApp.rewindPos = --theApp.rewindPos & 7;
    theApp.emulator.emuReadMemState(&theApp.rewindMemory[REWIND_SIZE*theApp.rewindPos], REWIND_SIZE);
    theApp.rewindCount--;
    theApp.rewindCounter = 0;
  }
}

void MainWnd::OnUpdateToolsRewind(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(theApp.rewindMemory != NULL && emulating && theApp.rewindCount);
}

void MainWnd::OnToolsCustomize() 
{
  AccelEditor dlg;

  if(dlg.DoModal()) {
    theApp.winAccelMgr = dlg.mgr;
    theApp.winAccelMgr.UpdateWndTable();
    theApp.winAccelMgr.Write();
    theApp.winAccelMgr.UpdateMenu(theApp.menu);
  }
}

void MainWnd::OnUpdateToolsCustomize(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(theApp.videoOption != VIDEO_320x240);
}
