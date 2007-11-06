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
// File    : AcceleratorManager.h
// Project : AccelsEditor
////////////////////////////////////////////////////////////////////////////////
// Version : 1.0                       * Author : T.Maurel
// Date    : 17.08.98
//
// Remarks : interface for the CAcceleratorManager class.
//
////////////////////////////////////////////////////////////////////////////////
#if !defined(AFX_ACCELERATORMANAGER_H__A6D76F4B_26C6_11D2_BE72_006097AC8D00__INCLUDED_)
#define AFX_ACCELERATORMANAGER_H__A6D76F4B_26C6_11D2_BE72_006097AC8D00__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000



#include "CmdAccelOb.h"


// Helper map
#include <afxtempl.h>  // MFC Templates extension
#ifndef CMapStringToWord
typedef CMap< CString, LPCSTR, WORD, WORD& > CMapStringToWord;
#endif

#ifndef CMapWordToCCmdAccelOb
typedef CMap< WORD, WORD&, CCmdAccelOb*, CCmdAccelOb*& > CMapWordToCCmdAccelOb;
#endif


//////////////////////////////////////////////////////////////////////
//
//
class CAcceleratorManager : public CObject {
  friend class AccelEditor;
 public:
  CAcceleratorManager();
  virtual ~CAcceleratorManager();

  // Operations
 public:
  void UpdateMenu(HMENU menu);
  void UpdateMenu();
  // Connection to the main application wnd
  void Connect(CWnd *pWnd, bool bAutoSave = true);
  // In/Out with the registry
  bool Load(HKEY hRegKey, LPCTSTR szRegKey);
  bool Load();
  bool Write();
  // Get the initials accels, not the user's
  bool Default(); 
  // Save a copy in the 2 maps called xxxSaved, which are used in case
  // of Default(), to reload the defaults accels.
  bool CreateDefaultTable();
  bool IsDefaultTableAvailable() {return m_bDefaultTable;}
  bool IsMapStringCommandsEmpty() {
    if (m_mapAccelString.IsEmpty())
      return true;
    else
      return false;
  }

  // Registry access configuration
  bool GetRegKey(HKEY& hRegKey, CString &szRegKey);
  bool SetRegKey(HKEY hRegKey, LPCTSTR szRegKey);
  bool IsAutoSave() {return m_bAutoSave;}
  void SetAutoSave(bool bAutoSave) {m_bAutoSave = bAutoSave;}

  // Helper fct, used for new menus strings
  bool GetStringFromACCEL(ACCEL* pACCEL, CString& szAccel);
  bool GetStringFromACCEL(BYTE cVirt, WORD nCode, CString& szAccel);

  // Update the ACCELS table in the application, from the specified
  // datas in the manager.
  bool UpdateWndTable();

  // Modification helper fcts
  bool SetAccel(BYTE cVirt, WORD wIDCommand, WORD wNewCaract,
                LPCTSTR szCommand, bool bLocked = false);
  bool AddCommandAccel(WORD wIDCommand, LPCTSTR szCommand, bool bLocked = true);
  bool CreateEntry(WORD wIDCommand, LPCTSTR szCommand);

  bool DeleteEntry(LPCTSTR szCommand);
  bool DeleteEntry(WORD wIDCommand);
  bool DeleteAccel(BYTE cVirt, WORD wIDCommand, WORD wNewCaract);

  // Affectation operator
  CAcceleratorManager& operator=(const CAcceleratorManager& accelmgr);

 public:
#ifdef _DEBUG
  virtual void AssertValid() const;
  virtual void Dump(CDumpContext& dc) const;
#endif

 protected:
  // Erase all the datas
  void Reset();
  // Internal affect fct.
  bool AddAccel(BYTE cVirt, WORD wIDCommand, WORD wKey,
                LPCTSTR szCommand, bool bLocked);

  // Attributes
 protected:
  CWnd *m_pWndConnected;

  // User datas
  CMapStringToWord m_mapAccelString;
  CMapWordToCCmdAccelOb m_mapAccelTable;
  // Default datas
  CMapWordToCCmdAccelOb m_mapAccelTableSaved;
  bool m_bDefaultTable;

  // Where the users datas will be saved in the registry
  HKEY m_hRegKey;
  CString m_szRegKey;
  // if true, there is an auto-save in the registry, when the destructor is called
  bool m_bAutoSave;

};


#endif // !defined(AFX_ACCELERATORMANAGER_H__A6D76F4B_26C6_11D2_BE72_006097AC8D00__INCLUDED_)
