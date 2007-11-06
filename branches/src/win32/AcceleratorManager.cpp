////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 1998 by Thierry Maurel
// All rights reserved
//
// Distribute freely, except: don't remove my name from the source or
// documentation (don't take credit for my work), mark your changes (don't
// get me blamed for your possible bugs), don't alter or remove this
// notice.
// No warrantee of any kind, express or implied, is included with this
// software; use at your own risk, responsibility for damages (if any) to
// anyone resulting from the use of this software rests entirely with the
// user.
//
// Send bug reports, bug fixes, enhancements, requests, flames, etc., and
// I'll try to keep a version up to date.  I can be reached as follows:
//    tmaurel@caramail.com   (or tmaurel@hol.fr)
//
////////////////////////////////////////////////////////////////////////////////
// File    : AcceleratorManager.cpp
// Project : AccelsEditor
////////////////////////////////////////////////////////////////////////////////
// Version : 1.0                       * Author : T.Maurel
// Date    : 17.08.98
//
// Remarks : implementation of the CAcceleratorManager class.
//
////////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
//#include "resource.h"
#include "../System.h"

#include "AcceleratorManager.h"
#include "Reg.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif



//////////////////////////////////////////////////////////////////////
// Constructor/Destructor
//////////////////////////////////////////////////////////////////////
//
//
CAcceleratorManager::CAcceleratorManager()
{
  m_hRegKey = HKEY_CURRENT_USER;
  m_szRegKey = "";
  m_bAutoSave = FALSE;
  m_pWndConnected = NULL;

  m_bDefaultTable = false;
}


//////////////////////////////////////////////////////////////////////
//
//
CAcceleratorManager::~CAcceleratorManager()
{
  if ((m_bAutoSave == true) && (m_szRegKey.IsEmpty() != FALSE)) {
    //          bool bRet = Write();
    //          if (!bRet)
    //                  systemMessage(0, "CAcceleratorManager::~CAcceleratorManager\nError in CAcceleratorManager::Write...");
  }

  Reset();
}


//////////////////////////////////////////////////////////////////////
// Internal fcts
//////////////////////////////////////////////////////////////////////
//
//
void CAcceleratorManager::Reset()
{
  CCmdAccelOb* pCmdAccel;
  WORD wKey;
  POSITION pos = m_mapAccelTable.GetStartPosition();
  while (pos != NULL) {
    m_mapAccelTable.GetNextAssoc(pos, wKey, pCmdAccel);
    delete pCmdAccel;
  }
  m_mapAccelTable.RemoveAll();
  m_mapAccelString.RemoveAll();

  pos = m_mapAccelTableSaved.GetStartPosition();
  while (pos != NULL) {
    m_mapAccelTableSaved.GetNextAssoc(pos, wKey, pCmdAccel);
    delete pCmdAccel;
  }
  m_mapAccelTableSaved.RemoveAll();
}


//////////////////////////////////////////////////////////////////////
//
//
bool CAcceleratorManager::AddAccel(BYTE cVirt, WORD wIDCommand, WORD wKey, LPCTSTR szCommand, bool bLocked)
{
  ASSERT(szCommand != NULL);

  WORD wIDCmd;
  if (m_mapAccelString.Lookup(szCommand, wIDCmd) == TRUE) {
    if (wIDCmd != wIDCommand)
      return false;
  }

  CCmdAccelOb* pCmdAccel = NULL;
  if (m_mapAccelTable.Lookup(wIDCommand, pCmdAccel) == TRUE) {
    if (pCmdAccel->m_szCommand != szCommand) {
      return false;
    }
    CAccelsOb* pAccel;
    POSITION pos = pCmdAccel->m_Accels.GetHeadPosition();
    while (pos != NULL) {
      pAccel = pCmdAccel->m_Accels.GetNext(pos);
      if (pAccel->m_cVirt == cVirt &&
          pAccel->m_wKey == wKey)
        return FALSE;
    }
    // Adding the accelerator
    pCmdAccel->Add(cVirt, wKey, bLocked);

  } else {
    pCmdAccel = new CCmdAccelOb(cVirt, wIDCommand, wKey, szCommand, bLocked);
    ASSERT(pCmdAccel != NULL);
    m_mapAccelTable.SetAt(wIDCommand, pCmdAccel);
  }
  // 2nd table
  m_mapAccelString.SetAt(szCommand, wIDCommand);
  return true;
}


//////////////////////////////////////////////////////////////////////
// Debug fcts
//////////////////////////////////////////////////////////////////////
//
//
#ifdef _DEBUG
void CAcceleratorManager::AssertValid() const
{
}

//////////////////////////////////////////////////////////////////////
//
//
void CAcceleratorManager::Dump(CDumpContext& dc) const
{
  CCmdAccelOb* pCmdAccel;
  WORD wKey;
  dc << "CAcceleratorManager::Dump :\n";
  dc << "m_mapAccelTable :\n";
  POSITION pos = m_mapAccelTable.GetStartPosition();
  while (pos != NULL) {
    m_mapAccelTable.GetNextAssoc(pos, wKey, pCmdAccel);
    dc << "a CCmdAccelOb at 0x"  << (void*)pCmdAccel << " = {\n";
    dc << pCmdAccel;
    dc << "}\n";
  }
  dc << "\nm_mapAccelTableSaved\n";
  pos = m_mapAccelTableSaved.GetStartPosition();
  while (pos != NULL) {
    m_mapAccelTableSaved.GetNextAssoc(pos, wKey, pCmdAccel);
    dc << "a CCmdAccelOb at 0x" << (void*)pCmdAccel << " = {\n";
    dc << pCmdAccel;
    dc << "}\n";
  }
}
#endif


//////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////
//
//
void CAcceleratorManager::Connect(CWnd* pWnd, bool bAutoSave)
{
  ASSERT(m_pWndConnected == NULL);
  m_pWndConnected = pWnd;
  m_bAutoSave = bAutoSave;
}


//////////////////////////////////////////////////////////////////////
//
//
bool CAcceleratorManager::GetRegKey(HKEY& hRegKey, CString& szRegKey)
{
  if (m_szRegKey.IsEmpty())
    return false;

  hRegKey = m_hRegKey;
  szRegKey = m_szRegKey;
  return true;
}


//////////////////////////////////////////////////////////////////////
//
//
bool CAcceleratorManager::SetRegKey(HKEY hRegKey, LPCTSTR szRegKey)
{
  ASSERT(hRegKey != NULL);
  ASSERT(szRegKey != NULL);

  m_szRegKey = szRegKey;
  m_hRegKey = hRegKey;
  return true;
}


//////////////////////////////////////////////////////////////////////
// Update the application's ACCELs table
//////////////////////////////////////////////////////////////////////
//
//
bool CAcceleratorManager::UpdateWndTable()
{
  int iLoop = 0;
  CTypedPtrArray<CPtrArray, LPACCEL> arrayACCEL;

  CCmdAccelOb* pCmdAccel;
  WORD wKey;
  LPACCEL pACCEL;
  CAccelsOb* pAccelOb;
  POSITION pos = m_mapAccelTable.GetStartPosition();
  while (pos != NULL) {
    m_mapAccelTable.GetNextAssoc(pos, wKey, pCmdAccel);
    POSITION pos = pCmdAccel->m_Accels.GetHeadPosition();
    while (pos != NULL) {
      pAccelOb = pCmdAccel->m_Accels.GetNext(pos);

      pACCEL = new ACCEL;
      ASSERT(pACCEL != NULL);
      pACCEL->fVirt = pAccelOb->m_cVirt;
      pACCEL->key = pAccelOb->m_wKey;
      pACCEL->cmd = pCmdAccel->m_wIDCommand;
      arrayACCEL.Add(pACCEL);
    }
  }
  
  INT_PTR nAccel = arrayACCEL.GetSize();
  LPACCEL lpAccel = (LPACCEL)LocalAlloc(LPTR, nAccel * sizeof(ACCEL));
  if (!lpAccel) {
    for (iLoop = 0; iLoop < nAccel; iLoop++)
      delete arrayACCEL.GetAt(iLoop);
    arrayACCEL.RemoveAll();

    return false;
  }

  for (iLoop = 0; iLoop < nAccel; iLoop++) {
    
    pACCEL = arrayACCEL.GetAt(iLoop);
    lpAccel[iLoop].fVirt = pACCEL->fVirt;
    lpAccel[iLoop].key = pACCEL->key;
    lpAccel[iLoop].cmd = pACCEL->cmd;

    delete pACCEL;
  }
  arrayACCEL.RemoveAll();

  HACCEL hNewTable = CreateAcceleratorTable(lpAccel, (int)nAccel);
  if (!hNewTable) {
    ::LocalFree(lpAccel);
    return false;
  }
  HACCEL hOldTable = theApp.hAccel;
  if (!::DestroyAcceleratorTable(hOldTable)) {
    ::LocalFree(lpAccel);
    return false;
  }
  theApp.hAccel = hNewTable;
  ::LocalFree(lpAccel);

  UpdateMenu(GetMenu(*AfxGetApp()->m_pMainWnd));

  return true;
}


//////////////////////////////////////////////////////////////////////
// Create/Destroy accelerators
//////////////////////////////////////////////////////////////////////
//
//
bool CAcceleratorManager::DeleteAccel(BYTE cVirt, WORD wIDCommand, WORD wKey)
{
  CCmdAccelOb* pCmdAccel = NULL;
  if (m_mapAccelTable.Lookup(wIDCommand, pCmdAccel) == TRUE) {
    POSITION pos = pCmdAccel->m_Accels.GetHeadPosition();
    POSITION PrevPos;
    CAccelsOb* pAccel = NULL;
    while (pos != NULL) {
      PrevPos = pos;
      pAccel = pCmdAccel->m_Accels.GetNext(pos);
      if (pAccel->m_bLocked == true)
        return false;

      if (pAccel->m_cVirt == cVirt && pAccel->m_wKey == wKey) {
        pCmdAccel->m_Accels.RemoveAt(PrevPos);
        delete pAccel;
        return true;
      }
    }
  }
  return false;
}


//////////////////////////////////////////////////////////////////////
//
//
bool CAcceleratorManager::DeleteEntry(WORD wIDCommand)
{
  CCmdAccelOb* pCmdAccel = NULL;
  VERIFY(m_mapAccelTable.Lookup(wIDCommand, pCmdAccel) == TRUE);

  CAccelsOb* pAccel;
  POSITION pos = pCmdAccel->m_Accels.GetHeadPosition();
  while (pos != NULL) {
    pAccel = pCmdAccel->m_Accels.GetNext(pos);
    if (pAccel->m_bLocked == true)
      return false;
  }
  m_mapAccelString.RemoveKey(pCmdAccel->m_szCommand);
  m_mapAccelTable.RemoveKey(wIDCommand);
  delete pCmdAccel;

  return true;
}


//////////////////////////////////////////////////////////////////////
//
//
bool CAcceleratorManager::DeleteEntry(LPCTSTR szCommand)
{
  ASSERT(szCommand != NULL);

  WORD wIDCommand;
  if (m_mapAccelString.Lookup(szCommand, wIDCommand) == TRUE) {
    return DeleteEntry(wIDCommand);
  }
  return true;
}


//////////////////////////////////////////////////////////////////////
//
//
bool CAcceleratorManager::SetAccel(BYTE cVirt, WORD wIDCommand, WORD wKey, LPCTSTR szCommand, bool bLocked)
{
  ASSERT(szCommand != NULL);

  return AddAccel(cVirt, wIDCommand, wKey, szCommand, bLocked);
}


//////////////////////////////////////////////////////////////////////
//
//
bool CAcceleratorManager::AddCommandAccel(WORD wIDCommand, LPCTSTR szCommand, bool bLocked)
{
  ASSERT(szCommand != NULL);

  ASSERT(m_pWndConnected != NULL);
  HACCEL hOriginalTable = theApp.hAccel;

  int nAccel = ::CopyAcceleratorTable(hOriginalTable, NULL, 0);
  LPACCEL lpAccel = (LPACCEL)LocalAlloc(LPTR, (nAccel) * sizeof(ACCEL));
  if (!lpAccel)
    return false;
  ::CopyAcceleratorTable(hOriginalTable, lpAccel, nAccel);

  bool bRet = false;
  for (int i = 0; i < nAccel; i++) {
    if (lpAccel[i].cmd == wIDCommand)
      bRet = AddAccel(lpAccel[i].fVirt, wIDCommand, lpAccel[i].key, szCommand, bLocked);
  }
  ::LocalFree(lpAccel);
  return bRet;
}


//////////////////////////////////////////////////////////////////////
//
//
bool CAcceleratorManager::CreateEntry(WORD wIDCommand, LPCTSTR szCommand)
{
  ASSERT(szCommand != NULL);

  WORD wIDDummy;
  if (m_mapAccelString.Lookup(szCommand, wIDDummy) == TRUE)
    return false;

  CCmdAccelOb* pCmdAccel = new CCmdAccelOb(wIDCommand, szCommand);
  ASSERT(pCmdAccel != NULL);
  m_mapAccelTable.SetAt(wIDCommand, pCmdAccel);
  m_mapAccelString.SetAt(szCommand, wIDCommand);

  return false;
}


//////////////////////////////////////////////////////////////////////
// Get a string from the ACCEL definition
//////////////////////////////////////////////////////////////////////
//
//
bool CAcceleratorManager::GetStringFromACCEL(ACCEL* pACCEL, CString& szAccel)
{
  ASSERT(pACCEL != NULL);
  
  CAccelsOb accel(pACCEL);
  accel.GetString(szAccel);

  if (szAccel.IsEmpty())
    return false;
  else
    return true;
}


//////////////////////////////////////////////////////////////////////
//
//
bool CAcceleratorManager::GetStringFromACCEL(BYTE cVirt, WORD nCode, CString& szAccel)
{
  CAccelsOb accel(cVirt, nCode);
  accel.GetString(szAccel);

  if (szAccel.IsEmpty())
    return false;
  else
    return true;
}


//////////////////////////////////////////////////////////////////////
// Copy function
//
CAcceleratorManager& CAcceleratorManager::operator=(const CAcceleratorManager& accelmgr)
{
  Reset();

  CCmdAccelOb* pCmdAccel;
  CCmdAccelOb* pNewCmdAccel;
  WORD wKey;
  // Copy the 2 tables : normal accel table...
  POSITION pos = accelmgr.m_mapAccelTable.GetStartPosition();
  while (pos != NULL) {
    accelmgr.m_mapAccelTable.GetNextAssoc(pos, wKey, pCmdAccel);
    pNewCmdAccel = new CCmdAccelOb;
    ASSERT(pNewCmdAccel != NULL);
    *pNewCmdAccel = *pCmdAccel;
    m_mapAccelTable.SetAt(wKey, pNewCmdAccel);
  }
  // ... and saved accel table.
  pos = accelmgr.m_mapAccelTableSaved.GetStartPosition();
  while (pos != NULL) {
    accelmgr.m_mapAccelTableSaved.GetNextAssoc(pos, wKey, pCmdAccel);
    pNewCmdAccel = new CCmdAccelOb;
    ASSERT(pNewCmdAccel != NULL);
    *pNewCmdAccel = *pCmdAccel;
    m_mapAccelTableSaved.SetAt(wKey, pNewCmdAccel);
  }

  // The Strings-ID table
  CString szKey;
  pos = accelmgr.m_mapAccelString.GetStartPosition();
  while (pos != NULL) {
    accelmgr.m_mapAccelString.GetNextAssoc(pos, szKey, wKey);
    m_mapAccelString.SetAt(szKey, wKey);
  }
  m_bDefaultTable = accelmgr.m_bDefaultTable;

  return *this;
}

void CAcceleratorManager::UpdateMenu(HMENU menu)
{
  int count = GetMenuItemCount(menu);

  OSVERSIONINFO info;
  info.dwOSVersionInfoSize = sizeof(info);
  GetVersionEx(&info);
  
  if(info.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS) {
    MENUITEMINFO info;
    char ss[128];
    ZeroMemory(&info, sizeof(info));
    info.cbSize = sizeof(info) - sizeof(HBITMAP);
    info.fMask = MIIM_ID | MIIM_SUBMENU;
    for(int i = 0; i < count; i++) {
      GetMenuItemInfo(menu, i, TRUE, &info);
      
      if(info.hSubMenu != NULL) {
        UpdateMenu(info.hSubMenu);
      } else {
        if(info.wID != (UINT)-1) {
          MENUITEMINFO info2;
          ZeroMemory(&info2, sizeof(info2));
          info2.cbSize = sizeof(info2) - sizeof(HBITMAP);
          info2.fMask = MIIM_STRING;
          info2.dwTypeData = ss;
          info2.cch = 128;
          GetMenuItemInfo(menu, i, MF_BYPOSITION, &info2);
          CString str = ss;
          int index = str.Find('\t');
          if(index != -1)
            str = str.Left(index);
          
          WORD command = info.wID;
          
          CCmdAccelOb *o;
          if(m_mapAccelTable.Lookup(command, o)) {
            if(o->m_Accels.GetCount()) {
              POSITION pos = o->m_Accels.GetHeadPosition();
              CAccelsOb *accel = o->m_Accels.GetNext(pos);
              
              CString s;
              accel->GetString(s);
              str += "\t";
              str += s;
            }
          }
          if(str != ss)
            ModifyMenu(menu, i, MF_BYPOSITION | MF_STRING, info.wID, str);
        }
      }
    }
  } else {
    MENUITEMINFO info;
    wchar_t ss[128];
    wchar_t str[512];
    
    ZeroMemory(&info, sizeof(info));
    info.cbSize = sizeof(info);
    info.fMask = MIIM_ID | MIIM_SUBMENU;
    for(int i = 0; i < count; i++) {
      GetMenuItemInfo(menu, i, TRUE, &info);
      
      if(info.hSubMenu != NULL) {
        UpdateMenu(info.hSubMenu);
      } else {
        if(info.wID != (WORD)-1) {
          MENUITEMINFOW info2;
          ZeroMemory(&info2, sizeof(info2));
          info2.cbSize = sizeof(info2);
          info2.fMask = MIIM_STRING;
          info2.dwTypeData = ss;
          info2.cch = 128;
          GetMenuItemInfoW(menu, i, MF_BYPOSITION, &info2);

          wcscpy(str, ss);
          
          wchar_t *p = wcschr(str, '\t');
          if(p)
            *p = 0;
          
          CCmdAccelOb *o;
          WORD command = info.wID;
          if(m_mapAccelTable.Lookup(command, o)) {
            if(o->m_Accels.GetCount()) {
              POSITION pos = o->m_Accels.GetHeadPosition();
              
              CAccelsOb *accel = o->m_Accels.GetNext(pos);
              
              CString s;
              accel->GetString(s);

              wchar_t temp[128];
              temp[0] = '\t';
              temp[1] = 0;
              wcscat(str, temp);
              p = temp;
              for(const char *sp = s; *sp; sp++)
                *p++ = *sp;
              *p = 0;
              wcscat(str, temp);
            }
          }
          if(wcscmp(str,ss))
            ModifyMenuW(menu, i, MF_BYPOSITION | MF_STRING, info.wID, str);
        }
      }
    }
  }
}

//////////////////////////////////////////////////////////////////////
// In/Out to the registry
//////////////////////////////////////////////////////////////////////
//
//
bool CAcceleratorManager::Load(HKEY hRegKey, LPCTSTR szRegKey)
{
  ASSERT(szRegKey != NULL);

  m_hRegKey = hRegKey;
  m_szRegKey = szRegKey;

  DWORD data[2048/sizeof(DWORD)];

  DWORD len = sizeof(data);
  if(regQueryBinaryValue("keyboard", (char *)data, len)) {
    int count = len/sizeof(DWORD);

    CCmdAccelOb* pCmdAccel;
    CAccelsOb* pAccel;
    DWORD dwIDAccelData, dwAccelData;
    BOOL bExistID;    
    int iIndex = 0;
    if(count) {
      WORD wKey;
      POSITION pos = m_mapAccelTable.GetStartPosition();

      while(pos != NULL) {
        m_mapAccelTable.GetNextAssoc(pos, wKey, pCmdAccel);
        pCmdAccel->DeleteUserAccels();
      }
      
      while(iIndex < count) {
        dwIDAccelData = data[iIndex++];
        
        WORD wIDCommand = LOWORD(dwIDAccelData);
        bExistID = m_mapAccelTable.Lookup(wIDCommand, pCmdAccel);

        if (bExistID) {
          pCmdAccel->DeleteUserAccels();
        }
        for (int j = 0; j < HIWORD(dwIDAccelData) && iIndex < count; j++) {
          dwAccelData = data[iIndex++];
          if (bExistID) {
            pAccel = new CAccelsOb;
            ASSERT(pAccel != NULL);
            pAccel->SetData(dwAccelData);
            pCmdAccel->Add(pAccel);
          }
        }
      }
    }
    UpdateWndTable();
    return true;
  }
  return false;
}


//////////////////////////////////////////////////////////////////////
//
//
bool CAcceleratorManager::Load()
{
  BOOL bRet = FALSE;
  if (!m_szRegKey.IsEmpty())
    bRet = Load(m_hRegKey, m_szRegKey);

  if (bRet == TRUE)
    return true;
  else
    return false;
}


//////////////////////////////////////////////////////////////////////
//
//
bool CAcceleratorManager::Write()
{
  CDWordArray AccelsDatasArray;
  CDWordArray CmdDatasArray;
  
  int iCount = 0;
  CCmdAccelOb* pCmdAccel;
  CAccelsOb* pAccel;
  DWORD dwAccelData;
  
  WORD wKey;
  POSITION pos = m_mapAccelTable.GetStartPosition();
  while (pos != NULL) {
    m_mapAccelTable.GetNextAssoc(pos, wKey, pCmdAccel);
    CmdDatasArray.RemoveAll();

    POSITION pos = pCmdAccel->m_Accels.GetHeadPosition();
    while (pos != NULL) {
      pAccel = pCmdAccel->m_Accels.GetNext(pos);
      //      if (!pAccel->m_bLocked) {
      dwAccelData = pAccel->GetData();
      CmdDatasArray.Add(dwAccelData);
      //      }
    }

    if (CmdDatasArray.GetSize() > 0) {
      CmdDatasArray.InsertAt(0, MAKELONG(pCmdAccel->m_wIDCommand, CmdDatasArray.GetSize()));
      
      AccelsDatasArray.Append(CmdDatasArray);
      iCount++;
    }
  }
  //  AccelsDatasArray.InsertAt(0, MAKELONG(65535, iCount));
  
  INT_PTR count = AccelsDatasArray.GetSize();
  DWORD *data = (DWORD *)malloc(count * sizeof(DWORD));
  ASSERT(data != NULL);

  for(int index = 0; index < count; index++)
    data[index] = AccelsDatasArray[index];

  regSetBinaryValue("keyboard", (char *)data, (int)(count*sizeof(DWORD)));

  AccelsDatasArray.RemoveAll();
  CmdDatasArray.RemoveAll();

  free(data);

  return true;
}


//////////////////////////////////////////////////////////////////////
// Defaults values management.
//////////////////////////////////////////////////////////////////////
//
//
bool CAcceleratorManager::CreateDefaultTable()
{
  if (m_bDefaultTable)
    return false;
        
  CCmdAccelOb* pCmdAccel;
  CCmdAccelOb* pNewCmdAccel;

  CAccelsOb* pAccel;
  CAccelsOb* pNewAccel;

  WORD wKey;
  POSITION pos = m_mapAccelTable.GetStartPosition();
  while (pos != NULL) {
    m_mapAccelTable.GetNextAssoc(pos, wKey, pCmdAccel);
    pNewCmdAccel = new CCmdAccelOb;
    ASSERT(pNewCmdAccel != NULL);
    
    POSITION pos = pCmdAccel->m_Accels.GetHeadPosition();
    while (pos != NULL) {
      pAccel = pCmdAccel->m_Accels.GetNext(pos);
      if (!pAccel->m_bLocked) {
        pNewAccel = new CAccelsOb;
        ASSERT(pNewAccel != NULL);
  
        *pNewAccel = *pAccel;
        pNewCmdAccel->m_Accels.AddTail(pNewAccel);
      }
    }
    if (pNewCmdAccel->m_Accels.GetCount() != 0) {
      pNewCmdAccel->m_wIDCommand = pCmdAccel->m_wIDCommand;
      pNewCmdAccel->m_szCommand = pCmdAccel->m_szCommand;
      
      m_mapAccelTableSaved.SetAt(wKey, pNewCmdAccel);
    } else 
      delete pNewCmdAccel;
  }

  m_bDefaultTable = true;
  return true;
}


//////////////////////////////////////////////////////////////////////
//
//
bool CAcceleratorManager::Default()
{
  return true;
}
