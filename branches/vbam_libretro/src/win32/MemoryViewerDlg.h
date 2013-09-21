#if !defined(AFX_MEMORYVIEWERDLG_H__15046D5B_D5A2_4C49_A969_2A77F803F2F1__INCLUDED_)
#define AFX_MEMORYVIEWERDLG_H__15046D5B_D5A2_4C49_A969_2A77F803F2F1__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// MemoryViewerDlg.h : header file
//

#include "MemoryViewer.h"
#include "ResizeDlg.h"
#include "IUpdate.h"

class GBAMemoryViewer : public MemoryViewer {
 public:
  GBAMemoryViewer();
  virtual void readData(u32, int, u8 *);
  virtual void editData(u32,int,int,u32);
};

/////////////////////////////////////////////////////////////////////////////
// MemoryViewerDlg dialog

class MemoryViewerDlg : public ResizeDlg, IUpdateListener, IMemoryViewerDlg
{
  GBAMemoryViewer m_viewer;
  // Construction
 public:
  void setCurrentAddress(u32 address);
  bool autoUpdate;
  void update();
  MemoryViewerDlg(CWnd* pParent = NULL);   // standard constructor

  // Dialog Data
  //{{AFX_DATA(MemoryViewerDlg)
  enum { IDD = IDD_MEM_VIEWER };
  CEdit  m_current;
  CEdit  m_address;
  CComboBox  m_addresses;
  int    m_size;
  //}}AFX_DATA


  // Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(MemoryViewerDlg)
 protected:
  virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
  virtual void PostNcDestroy();
  //}}AFX_VIRTUAL

  // Implementation
 protected:

  // Generated message map functions
  //{{AFX_MSG(MemoryViewerDlg)
  virtual BOOL OnInitDialog();
  afx_msg void OnClose();
  afx_msg void OnRefresh();
  afx_msg void On8Bit();
  afx_msg void On16Bit();
  afx_msg void On32Bit();
  afx_msg void OnAutoUpdate();
  afx_msg void OnGo();
  afx_msg void OnSelchangeAddresses();
  afx_msg void OnSave();
  afx_msg void OnLoad();
  //}}AFX_MSG
  DECLARE_MESSAGE_MAP()
    };

    //{{AFX_INSERT_LOCATION}}
    // Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_MEMORYVIEWERDLG_H__15046D5B_D5A2_4C49_A969_2A77F803F2F1__INCLUDED_)
