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
// File    : KeyboardEdit.cpp
// Project : AccelsEditor
////////////////////////////////////////////////////////////////////////////////
// Version : 1.0                       * Authors : A.Lebatard + T.Maurel
// Date    : 17.08.98
//
// Remarks : implementation file
//
////////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "KeyboardEdit.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

extern TCHAR* mapVirtKeysStringFromWORD(WORD wKey);

/////////////////////////////////////////////////////////////////////////////
// CKeyboardEdit

CKeyboardEdit::CKeyboardEdit()
{
  m_bKeyDefined = false;
  ResetKey ();
}

CKeyboardEdit::~CKeyboardEdit()
{
}


BEGIN_MESSAGE_MAP(CKeyboardEdit, CEdit)
  //{{AFX_MSG_MAP(CKeyboardEdit)
  //}}AFX_MSG_MAP
  END_MESSAGE_MAP()

#pragma warning( disable : 4706 )
  /////////////////////////////////////////////////////////////////////////////
// CKeyboardEdit message handlers
BOOL CKeyboardEdit::PreTranslateMessage (MSG* pMsg) 
{
  bool bPressed;
  if ((bPressed = (pMsg->message == WM_KEYDOWN)) || pMsg->message == WM_KEYUP || (bPressed = (pMsg->message == WM_SYSKEYDOWN)) || pMsg->message == WM_SYSKEYUP) {
    if (bPressed && m_bKeyDefined && !((1 << 30) & pMsg->lParam))
      ResetKey ();
    if (pMsg->wParam == VK_SHIFT && !m_bKeyDefined)
      m_bShiftPressed = bPressed;
    else if (pMsg->wParam == VK_CONTROL &&!m_bKeyDefined) {
      m_bCtrlPressed = bPressed;
    }
    else if (pMsg->wParam == VK_MENU && !m_bKeyDefined)
      m_bAltPressed = bPressed;
    else {
      if (!m_bKeyDefined) {
        m_wVirtKey = (WORD)pMsg->wParam;
        if (bPressed)
          m_bKeyDefined = true;
      }
    }
    DisplayKeyboardString ();
    return TRUE;
  }
        
  return CEdit::PreTranslateMessage(pMsg);
}
#pragma warning( default : 4706 )

////////////////////////////////////////////////////////////////////////
//
void CKeyboardEdit::DisplayKeyboardString()
{
  CString strKbd;

  // modifiers
  if (m_bCtrlPressed)
    strKbd = "Ctrl";
  if (m_bAltPressed) {
    if (strKbd.GetLength () > 0)
      strKbd += '+';
    strKbd += "Alt";
  }
  if (m_bShiftPressed) {
    if (strKbd.GetLength () > 0)
      strKbd += '+';
    strKbd += "Shift";
  }
  // virtual key
  LPCTSTR szVirtKey = mapVirtKeysStringFromWORD(m_wVirtKey);
  if (szVirtKey != NULL) {
    if (strKbd.GetLength () > 0)
      strKbd += '+';
    strKbd += szVirtKey;
  }
  SetWindowText (strKbd);
}


////////////////////////////////////////////////////////////////////////
//
void CKeyboardEdit::ResetKey ()
{
  m_wVirtKey = 0;
  m_bCtrlPressed = false;
  m_bAltPressed = false;
  m_bShiftPressed = false;
  
  m_bKeyDefined = false;
  if(m_hWnd != NULL)
    SetWindowText(_T(""));
}


////////////////////////////////////////////////////////////////////////
//
bool CKeyboardEdit::GetAccelKey(WORD& wVirtKey, bool& bCtrl, bool& bAlt, bool& bShift)
{
  if (!m_bKeyDefined)
    return false;
  
  wVirtKey = m_wVirtKey;
  bAlt = m_bAltPressed;
  bCtrl = m_bCtrlPressed;
  bShift = m_bShiftPressed;
  return true;
}

