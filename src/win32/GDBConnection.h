#if !defined(AFX_GDBCONNECTION_H__DD73B298_E1A7_4A46_B282_E7A2B37FC9D9__INCLUDED_)
#define AFX_GDBCONNECTION_H__DD73B298_E1A7_4A46_B282_E7A2B37FC9D9__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// GDBConnection.h : header file
//

#include <winsock.h>

/////////////////////////////////////////////////////////////////////////////
// GDBPortDlg dialog

class GDBPortDlg : public CDialog
{
  int port;
  SOCKET sock;
  // Construction
 public:
  SOCKET getSocket();
  int getPort();
  GDBPortDlg(CWnd* pParent = NULL);   // standard constructor

  // Dialog Data
  //{{AFX_DATA(GDBPortDlg)
  enum { IDD = IDD_GDB_PORT };
  CEdit  m_port;
  //}}AFX_DATA


  // Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(GDBPortDlg)
 protected:
  virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
  //}}AFX_VIRTUAL

  // Implementation
 protected:

  // Generated message map functions
  //{{AFX_MSG(GDBPortDlg)
  virtual BOOL OnInitDialog();
  afx_msg void OnOk();
  afx_msg void OnCancel();
  afx_msg void OnClose();
  //}}AFX_MSG
  DECLARE_MESSAGE_MAP()
    };

    /////////////////////////////////////////////////////////////////////////////
// GDBWaitingDlg dialog

class GDBWaitingDlg : public CDialog
{
  int port;
  SOCKET listenSocket;
  SOCKET sock;
  // Construction
 public:
  SOCKET getSocket();
  SOCKET getListenSocket();
  afx_msg LRESULT OnSocketAccept(WPARAM wParam, LPARAM lParam);
  GDBWaitingDlg(int p, CWnd* pParent = NULL);   // standard constructor

  // Dialog Data
  //{{AFX_DATA(GDBWaitingDlg)
  enum { IDD = IDD_GDB_WAITING };
  CStatic  m_port;
  //}}AFX_DATA


  // Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(GDBWaitingDlg)
 protected:
  virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
  //}}AFX_VIRTUAL

  // Implementation
 protected:

  // Generated message map functions
  //{{AFX_MSG(GDBWaitingDlg)
  virtual BOOL OnInitDialog();
  afx_msg void OnCancel();
  afx_msg void OnClose();
  //}}AFX_MSG
  DECLARE_MESSAGE_MAP()
    };
    //{{AFX_INSERT_LOCATION}}
    // Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_GDBCONNECTION_H__DD73B298_E1A7_4A46_B282_E7A2B37FC9D9__INCLUDED_)
