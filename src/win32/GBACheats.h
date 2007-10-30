// -*- C++ -*-
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

#if !defined(AFX_GBACHEATS_H__FC31D47D_52C8_42B2_95C7_7C3FD09316A4__INCLUDED_)
#define AFX_GBACHEATS_H__FC31D47D_52C8_42B2_95C7_7C3FD09316A4__INCLUDED_

#include "..\System.h"  // Added by ClassView
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// GBACheats.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// GBACheatSearch dialog

struct WinCheatsData {
  u32  addr;
  char address[9];
  char oldValue[12];
  char newValue[12];
};

class GBACheatSearch : public CDialog
{
  // Construction
 public:
  afx_msg void OnSizeType(UINT id);
  afx_msg void OnNumberType(UINT id);
  afx_msg void OnSearchType(UINT id);
  afx_msg void OnValueType(UINT id);
  void addChange(int index, u32 address, u32 oldValue, u32 newValue);
  GBACheatSearch(CWnd* pParent = NULL);   // standard constructor
  ~GBACheatSearch();

  // Dialog Data
  //{{AFX_DATA(GBACheatSearch)
  enum { IDD = IDD_CHEATS };
  CEdit  m_value;
  CListCtrl  m_list;
  int    valueType;
  int    sizeType;
  int    searchType;
  int    numberType;
  BOOL  updateValues;
  //}}AFX_DATA


  // Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(GBACheatSearch)
 protected:
  virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
  //}}AFX_VIRTUAL

  // Implementation
 protected:

  // Generated message map functions
  //{{AFX_MSG(GBACheatSearch)
  afx_msg void OnOk();
  afx_msg void OnStart();
  afx_msg void OnSearch();
  afx_msg void OnAddCheat();
  afx_msg void OnUpdate();
  afx_msg void OnGetdispinfoCheatList(NMHDR* pNMHDR, LRESULT* pResult);
  afx_msg void OnItemchangedCheatList(NMHDR* pNMHDR, LRESULT* pResult);
  virtual BOOL OnInitDialog();
  //}}AFX_MSG
  DECLARE_MESSAGE_MAP()
    private:
  void addChanges(bool showMsgs);
  WinCheatsData *data;
};

/////////////////////////////////////////////////////////////////////////////
// AddCheat dialog

class AddCheat : public CDialog
{
  // Construction
 public:
  bool addCheat();
  afx_msg void OnSizeType(UINT id);
  afx_msg void OnNumberType(UINT id);
  u32 address;
  AddCheat(u32 address, CWnd* pParent = NULL);   // standard constructor

  // Dialog Data
  //{{AFX_DATA(AddCheat)
  enum { IDD = IDD_ADD_CHEAT };
  CEdit  m_value;
  CEdit  m_desc;
  CEdit  m_address;
  int    sizeType;
  int    numberType;
  //}}AFX_DATA


  // Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(AddCheat)
 protected:
  virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
  //}}AFX_VIRTUAL

  // Implementation
 protected:

  // Generated message map functions
  //{{AFX_MSG(AddCheat)
  afx_msg void OnOk();
  afx_msg void OnCancel();
  virtual BOOL OnInitDialog();
  //}}AFX_MSG
  DECLARE_MESSAGE_MAP()
    };
    /////////////////////////////////////////////////////////////////////////////
// GBACheatList dialog

class GBACheatList : public CDialog
{
  // Construction
 public:
  void refresh();
  bool duringRefresh;
  bool restoreValues;

  GBACheatList(CWnd* pParent = NULL);   // standard constructor

  // Dialog Data
  //{{AFX_DATA(GBACheatList)
  enum { IDD = IDD_CHEAT_LIST };
  CButton  m_restore;
  CListCtrl  m_list;
  //}}AFX_DATA


  // Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(GBACheatList)
 protected:
  virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
  //}}AFX_VIRTUAL

  // Implementation
 protected:

  // Generated message map functions
  //{{AFX_MSG(GBACheatList)
  afx_msg void OnAddCheat();
  afx_msg void OnAddCode();
  afx_msg void OnAddCodebreaker();
  afx_msg void OnAddGameshark();
  afx_msg void OnEnable();
  afx_msg void OnRemove();
  afx_msg void OnRemoveAll();
  afx_msg void OnRestore();
  afx_msg void OnOk();
  afx_msg void OnItemchangedCheatList(NMHDR* pNMHDR, LRESULT* pResult);
  virtual BOOL OnInitDialog();
  //}}AFX_MSG
  DECLARE_MESSAGE_MAP()
    };
    /////////////////////////////////////////////////////////////////////////////
// AddGSACode dialog

class AddGSACode : public CDialog
{
  // Construction
 public:
  AddGSACode(CWnd* pParent = NULL);   // standard constructor

  // Dialog Data
  //{{AFX_DATA(AddGSACode)
  enum { IDD = IDD_ADD_CHEAT_DLG };
  CEdit  m_desc;
  CEdit  m_code;
  //}}AFX_DATA


  // Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(AddGSACode)
 protected:
  virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
  //}}AFX_VIRTUAL

  // Implementation
 protected:

  // Generated message map functions
  //{{AFX_MSG(AddGSACode)
  afx_msg void OnOk();
  afx_msg void OnCancel();
  virtual BOOL OnInitDialog();
  //}}AFX_MSG
  DECLARE_MESSAGE_MAP()
    };

    /////////////////////////////////////////////////////////////////////////////
// AddCBACode dialog

class AddCBACode : public CDialog
{
  // Construction
 public:
  AddCBACode(CWnd* pParent = NULL);   // standard constructor

  // Dialog Data
  //{{AFX_DATA(AddCBACode)
  enum { IDD = IDD_ADD_CHEAT_DLG };
  CEdit  m_desc;
  CEdit  m_code;
  //}}AFX_DATA


  // Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(AddCBACode)
 protected:
  virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
  //}}AFX_VIRTUAL

  // Implementation
 protected:

  // Generated message map functions
  //{{AFX_MSG(AddCBACode)
  afx_msg void OnOk();
  afx_msg void OnCancel();
  virtual BOOL OnInitDialog();
  //}}AFX_MSG
  DECLARE_MESSAGE_MAP()
    };

    /////////////////////////////////////////////////////////////////////////////
// AddCheatCode dialog

class AddCheatCode : public CDialog
{
  // Construction
 public:
  AddCheatCode(CWnd* pParent = NULL);   // standard constructor

  // Dialog Data
  //{{AFX_DATA(AddCheatCode)
  enum { IDD = IDD_ADD_CHEAT_DLG };
  CEdit  m_desc;
  CEdit  m_code;
  //}}AFX_DATA


  // Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(AddCheatCode)
 protected:
  virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
  //}}AFX_VIRTUAL

  // Implementation
 protected:

  // Generated message map functions
  //{{AFX_MSG(AddCheatCode)
  afx_msg void OnOk();
  afx_msg void OnCancel();
  virtual BOOL OnInitDialog();
  //}}AFX_MSG
  DECLARE_MESSAGE_MAP()
    };
    //{{AFX_INSERT_LOCATION}}
    // Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_GBACHEATS_H__FC31D47D_52C8_42B2_95C7_7C3FD09316A4__INCLUDED_)
