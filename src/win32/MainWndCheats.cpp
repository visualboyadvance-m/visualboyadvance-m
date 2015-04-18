#include "stdafx.h"
#include "MainWnd.h"

#include "FileDlg.h"
#include "GBACheats.h"
#include "GBCheatsDlg.h"
#include "Reg.h"
#include "WinResUtil.h"

#include "../gba/GBA.h"
#include "../gba/Globals.h"
#include "../gb/gbCheats.h"

extern int emulating;

void MainWnd::OnCheatsSearchforcheats()
{
  if(theApp.cartridgeType == 0) {
    GBACheatSearch dlg;
    dlg.DoModal();
  } else {
    GBCheatSearch dlg;
    dlg.DoModal();
  }
}

void MainWnd::OnUpdateCheatsSearchforcheats(CCmdUI* pCmdUI)
{
  pCmdUI->Enable(emulating);
}

void MainWnd::OnCheatsCheatlist()
{
  if(theApp.cartridgeType == 0) {
    GBACheatList dlg;
    dlg.DoModal();
  } else {
    GBCheatList dlg;
    dlg.DoModal();
  }
}

void MainWnd::OnUpdateCheatsCheatlist(CCmdUI* pCmdUI)
{
  pCmdUI->Enable(emulating);
}

void MainWnd::OnCheatsAutomaticsaveloadcheats()
{
  autoSaveLoadCheatList = !autoSaveLoadCheatList;
}

void MainWnd::OnUpdateCheatsAutomaticsaveloadcheats(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(autoSaveLoadCheatList);
}

void MainWnd::OnCheatsLoadcheatlist()
{
  CString buffer;
  CString filename;

  int index = theApp.filename.ReverseFind('\\');

  if(index != -1)
    buffer = theApp.filename.Right(theApp.filename.GetLength()-index-1);
  else
    buffer = theApp.filename;

  CString saveDir = regQueryStringValue("saveDir", NULL);

  if(saveDir.IsEmpty())
    saveDir = getDirFromFile(theApp.filename);

  if(isDriveRoot(saveDir))
    filename.Format("%s%s.clt", saveDir, buffer);
  else
    filename.Format("%s\\%s.clt", saveDir, buffer);

  LPCTSTR exts[] = { ".clt" };
  CString filter = winLoadFilter(IDS_FILTER_CHEAT_LIST);
  CString title = winResLoadString(IDS_SELECT_CHEAT_LIST_NAME);

  FileDlg dlg(this, filename, filter, 0, "CLT", exts, saveDir, title, false);

  if(dlg.DoModal() == IDOK) {
    winLoadCheatList(dlg.GetPathName());
  }
}

void MainWnd::OnUpdateCheatsLoadcheatlist(CCmdUI* pCmdUI)
{
  pCmdUI->Enable(emulating);
}

void MainWnd::OnCheatsSavecheatlist()
{
  CString buffer;
  CString filename;

  int index = theApp.filename.ReverseFind('\\');

  if(index != -1)
    buffer = theApp.filename.Right(theApp.filename.GetLength()-index-1);
  else
    buffer = theApp.filename;

  CString saveDir = regQueryStringValue("saveDir", NULL);

  if(saveDir.IsEmpty())
    saveDir = getDirFromFile(theApp.filename);

  if(isDriveRoot(saveDir))
    filename.Format("%s%s.clt", saveDir, buffer);
  else
    filename.Format("%s\\%s.clt", saveDir, buffer);

  LPCTSTR exts[] = { ".clt" };
  CString filter = winLoadFilter(IDS_FILTER_CHEAT_LIST);
  CString title = winResLoadString(IDS_SELECT_CHEAT_LIST_NAME);

  FileDlg dlg(this, filename, filter, 0, "CLT", exts, saveDir, title, true);

  if(dlg.DoModal() == IDOK) {
    winSaveCheatList(dlg.GetPathName());
  }
}

void MainWnd::OnUpdateCheatsSavecheatlist(CCmdUI* pCmdUI)
{
  pCmdUI->Enable(emulating);
}

void MainWnd::OnCheatsDisablecheats()
{
  cheatsEnabled = !cheatsEnabled;
  systemScreenMessage(winResLoadString(cheatsEnabled ? IDS_CHEATS_ENABLED : IDS_CHEATS_DISABLED));
}

void MainWnd::OnUpdateCheatsDisablecheats(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(!cheatsEnabled);
}

