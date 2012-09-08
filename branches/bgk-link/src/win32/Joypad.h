#include "afxwin.h"
#if !defined(AFX_JOYPAD_H__FFFB2470_9EEC_4D2D_A5F0_3BF31579999A__INCLUDED_)
#define AFX_JOYPAD_H__FFFB2470_9EEC_4D2D_A5F0_3BF31579999A__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// Joypad.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// JoypadEditControl window

class JoypadEditControl : public CEdit
{
  // Construction
public:

  JoypadEditControl();

  KeyList m_Keys;

  // Attributes
 public:

  // Operations
 public:

  // Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(JoypadEditControl)
  //}}AFX_VIRTUAL

  // Implementation
 public:
  virtual BOOL PreTranslateMessage(MSG *pMsg);
  afx_msg LRESULT OnJoyConfig(WPARAM wParam, LPARAM lParam);
  virtual ~JoypadEditControl();

  // Generated message map functions
 protected:
  DECLARE_MESSAGE_MAP()
    };

    /////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
// JoypadConfig dialog

class JoypadConfig : public CDialog
{
  // Construction
 public:
  void assignKey(int id, LONG_PTR key);
  JoypadConfig(int w, CWnd* pParent = NULL);   // standard constructor

  // Dialog Data
  //{{AFX_DATA(JoypadConfig)
  enum { IDD = IDD_CONFIG };
  JoypadEditControl  up;
  JoypadEditControl  speed;
  JoypadEditControl  right;
  JoypadEditControl  left;
  JoypadEditControl  down;
  JoypadEditControl  capture;
  JoypadEditControl  buttonStart;
  JoypadEditControl  buttonSelect;
  JoypadEditControl  buttonR;
  JoypadEditControl  buttonL;
  JoypadEditControl  buttonGS;
  JoypadEditControl  buttonB;
  JoypadEditControl  buttonA;

 //}}AFX_DATA


  // Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(JoypadConfig)
 protected:
  virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
  //}}AFX_VIRTUAL

  // Implementation
 protected:
  UINT_PTR timerId;
  int which;

  // Generated message map functions
  //{{AFX_MSG(JoypadConfig)
  afx_msg void OnCancel();
  afx_msg void OnOk();
  afx_msg void OnChar(UINT nChar, UINT nRepCnt, UINT nFlags);
  afx_msg void OnDestroy();
  afx_msg void OnTimer(UINT_PTR nIDEvent);
  virtual BOOL OnInitDialog();
  //}}AFX_MSG
  DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnBnClickedAppendmode();
	afx_msg void OnBnClickedClearAll();
};
    /////////////////////////////////////////////////////////////////////////////
// MotionConfig dialog

class MotionConfig : public CDialog
{
  // Construction
 public:
  void assignKeys();
  void assignKey(int id, int key);
  MotionConfig(CWnd* pParent = NULL);   // standard constructor

  // Dialog Data
  //{{AFX_DATA(MotionConfig)
  enum { IDD = IDD_MOTION_CONFIG };
  JoypadEditControl  up;
  JoypadEditControl  right;
  JoypadEditControl  left;
  JoypadEditControl  down;
  //}}AFX_DATA


  // Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(MotionConfig)
 protected:
  virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
  //}}AFX_VIRTUAL

  // Implementation
 protected:

  // Generated message map functions
  //{{AFX_MSG(MotionConfig)
  afx_msg void OnCancel();
  afx_msg void OnOk();
  afx_msg void OnChar(UINT nChar, UINT nRepCnt, UINT nFlags);
  afx_msg void OnDestroy();
  virtual BOOL OnInitDialog();
  afx_msg void OnTimer(UINT_PTR nIDEvent);
  //}}AFX_MSG
  DECLARE_MESSAGE_MAP()
    private:
  UINT_PTR timerId;
public:
	afx_msg void OnBnClickedAppendmode();
};
//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_JOYPAD_H__FFFB2470_9EEC_4D2D_A5F0_3BF31579999A__INCLUDED_)
