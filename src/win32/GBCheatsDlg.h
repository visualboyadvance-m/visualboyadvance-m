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

#if !defined(AFX_GBCHEATSDLG_H__8ECCB04A_AB75_4552_8625_C6FBF30A95D9__INCLUDED_)
#define AFX_GBCHEATSDLG_H__8ECCB04A_AB75_4552_8625_C6FBF30A95D9__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// GBCheats.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// GBCheatSearch dialog

struct WinGbCheatsData {
  int  bank;  
  u16  addr;
  char address[9];
  char oldValue[12];
  char newValue[12];
};

class GBCheatSearch : public CDialog
{
  // Construction
 public:
  afx_msg void OnSizeType(UINT id);
  afx_msg void OnNumberType(UINT id);
  afx_msg void OnSearchType(UINT id);
  afx_msg void OnValueType(UINT id);
  void addChanges(bool showMsg);
  void addChange(int index, int bank, u16 address, int offset, u32 oldValue, u32 newValue);
  int getBank(u16 addr, int j);
  GBCheatSearch(CWnd* pParent = NULL);   // standard constructor
  ~GBCheatSearch();

  // Dialog Data
  //{{AFX_DATA(GBCheatSearch)
  enum { IDD = IDD_CHEATS };
  CEdit  m_value;
  CListCtrl  m_list;
  int    searchType;
  int    numberType;
  int    sizeType;
  BOOL  updateValues;
  int    valueType;
  //}}AFX_DATA


  // Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(GBCheatSearch)
 protected:
  virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
  //}}AFX_VIRTUAL

  // Implementation
 protected:
  WinGbCheatsData *data;

  // Generated message map functions
  //{{AFX_MSG(GBCheatSearch)
  afx_msg void OnOk();
  afx_msg void OnAddCheat();
  afx_msg void OnSearch();
  afx_msg void OnStart();
  afx_msg void OnUpdate();
  virtual BOOL OnInitDialog();
  afx_msg void OnGetdispinfoCheatList(NMHDR* pNMHDR, LRESULT* pResult);
  afx_msg void OnItemchangedCheatList(NMHDR* pNMHDR, LRESULT* pResult);
  //}}AFX_MSG
  DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
// AddGBCheat dialog

class AddGBCheat : public CDialog
{
  // Construction
 public:
  afx_msg void OnSizeType(UINT id);
  afx_msg void OnNumberType(UINT id);
  bool addCheat();
  AddGBCheat(u32 addr, CWnd* pParent = NULL);   // standard constructor

  // Dialog Data
  //{{AFX_DATA(AddGBCheat)
  enum { IDD = IDD_ADD_CHEAT };
  CEdit  m_value;
  CEdit  m_address;
  CEdit  m_desc;
  int    sizeType;
  int    numberType;
  //}}AFX_DATA


  // Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(AddGBCheat)
 protected:
  virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
  //}}AFX_VIRTUAL

  // Implementation
 protected:
  LONG_PTR address;

  // Generated message map functions
  //{{AFX_MSG(AddGBCheat)
  afx_msg void OnCancel();
  afx_msg void OnOk();
  virtual BOOL OnInitDialog();
  //}}AFX_MSG
  DECLARE_MESSAGE_MAP()
    };
    /////////////////////////////////////////////////////////////////////////////
// GBCheatList dialog

class GBCheatList : public CDialog
{
  // Construction
 public:
  void refresh();
  bool duringRefresh;
  GBCheatList(CWnd* pParent = NULL);   // standard constructor

  // Dialog Data
  //{{AFX_DATA(GBCheatList)
  enum { IDD = IDD_GB_CHEAT_LIST };
  CListCtrl  m_list;
  //}}AFX_DATA


  // Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(GBCheatList)
 protected:
  virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
  //}}AFX_VIRTUAL

  // Implementation
 protected:

  // Generated message map functions
  //{{AFX_MSG(GBCheatList)
  afx_msg void OnOk();
  afx_msg void OnAddGgCheat();
  afx_msg void OnAddGsCheat();
  afx_msg void OnEnable();
  afx_msg void OnRemove();
  afx_msg void OnRemoveAll();
  afx_msg void OnItemchangedCheatList(NMHDR* pNMHDR, LRESULT* pResult);
  virtual BOOL OnInitDialog();
  //}}AFX_MSG
  DECLARE_MESSAGE_MAP()
    };

    /////////////////////////////////////////////////////////////////////////////
// AddGBCode dialog

class AddGBCode : public CDialog
{
  // Construction
 public:
  AddGBCode(bool (*verify)(const char *, const char *),int, const char *, CWnd* pParent = NULL);   // standard constructor

  // Dialog Data
  //{{AFX_DATA(AddGBCode)
  enum { IDD = IDD_ADD_CHEAT_DLG };
  CEdit  m_desc;
  CEdit  m_code;
  //}}AFX_DATA

  int addLength;
  CString addTitle;
  bool (*addVerify)(const char *, const char*);

  // Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(AddGBCode)
 protected:
  virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
  //}}AFX_VIRTUAL

  // Implementation
 protected:

  // Generated message map functions
  //{{AFX_MSG(AddGBCode)
  afx_msg void OnOk();
  afx_msg void OnCancel();
  virtual BOOL OnInitDialog();
  //}}AFX_MSG
  DECLARE_MESSAGE_MAP()
    };
    //{{AFX_INSERT_LOCATION}}
    // Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_GBCHEATSDLG_H__8ECCB04A_AB75_4552_8625_C6FBF30A95D9__INCLUDED_)
