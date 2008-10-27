#if !defined(AFX_MEMORYVIEWERADDRESSSIZE_H__04605262_2B1D_4EED_A467_B6C56AC2CACD__INCLUDED_)
#define AFX_MEMORYVIEWERADDRESSSIZE_H__04605262_2B1D_4EED_A467_B6C56AC2CACD__INCLUDED_

#include "../System.h"  // Added by ClassView
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// MemoryViewerAddressSize.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// MemoryViewerAddressSize dialog

class MemoryViewerAddressSize : public CDialog
{
  u32 address;
  int size;
  // Construction
 public:
  int getSize();
  u32 getAddress();
  void setSize(int s);
  void setAddress(u32 a);
  MemoryViewerAddressSize(u32 a=0xffffff, int s=-1, CWnd* pParent = NULL);   // standard constructor

  // Dialog Data
  //{{AFX_DATA(MemoryViewerAddressSize)
  enum { IDD = IDD_ADDR_SIZE };
  CEdit  m_size;
  CEdit  m_address;
  //}}AFX_DATA


  // Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(MemoryViewerAddressSize)
 protected:
  virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
  //}}AFX_VIRTUAL

  // Implementation
 protected:

  // Generated message map functions
  //{{AFX_MSG(MemoryViewerAddressSize)
  virtual BOOL OnInitDialog();
  afx_msg void OnOk();
  afx_msg void OnCancel();
  //}}AFX_MSG
  DECLARE_MESSAGE_MAP()
    };

    //{{AFX_INSERT_LOCATION}}
    // Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_MEMORYVIEWERADDRESSSIZE_H__04605262_2B1D_4EED_A467_B6C56AC2CACD__INCLUDED_)
