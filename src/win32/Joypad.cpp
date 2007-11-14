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

// Joypad.cpp : implementation file
//

#include "stdafx.h"
#include "vba.h"
#include "Joypad.h"
#include "Input.h"


#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

BOOL bAppendMode;

void AssignKey(KeyList &Key, int Out)
{
	if (!bAppendMode)
		Key.RemoveAll();
	Key.AddTail(Out);
}


CString GetKeyListName(KeyList& Keys)
{
	CString txtKeys;

	POSITION p = Keys.GetHeadPosition();
	while(p!=NULL)
	{
		txtKeys+=theApp.input->getKeyName(Keys.GetNext(p));
		if (p!=NULL)
			txtKeys+=", ";
	}
	return txtKeys;
}

void CopyKeys(KeyList &Out, KeyList &In)
{
	Out.RemoveAll();
	POSITION p = In.GetHeadPosition();
	while(p!=NULL)
		Out.AddTail(In.GetNext(p));
}

#define AssignKeys(in, out) CopyKeys(out, in);

/////////////////////////////////////////////////////////////////////////////
// JoypadEditControl

JoypadEditControl::JoypadEditControl()
{
}

JoypadEditControl::~JoypadEditControl()
{
}


BEGIN_MESSAGE_MAP(JoypadEditControl, CEdit)
  //{{AFX_MSG_MAP(JoypadEditControl)
  ON_WM_CHAR()
  //}}AFX_MSG_MAP
  ON_MESSAGE(JOYCONFIG_MESSAGE, OnJoyConfig)
  END_MESSAGE_MAP()

  /////////////////////////////////////////////////////////////////////////////
// JoypadEditControl message handlers


void JoypadEditControl::OnChar(UINT nChar, UINT nRepCnt, UINT nFlags)
{
}

LRESULT JoypadEditControl::OnJoyConfig(WPARAM wParam, LPARAM lParam)
{
	AssignKey(m_Keys, ((wParam<<8)|lParam));
	SetWindowText(GetKeyListName(m_Keys));
	GetParent()->GetNextDlgTabItem(this, FALSE)->SetFocus();
	return TRUE;
}


BOOL JoypadEditControl::PreTranslateMessage(MSG *pMsg)
{
  if(pMsg->message == WM_KEYDOWN && (pMsg->wParam == VK_ESCAPE || pMsg->wParam == VK_RETURN))
    return TRUE;

  return CEdit::PreTranslateMessage(pMsg);
}

/////////////////////////////////////////////////////////////////////////////
// JoypadConfig dialog


JoypadConfig::JoypadConfig(int w, CWnd* pParent /*=NULL*/)
  : CDialog(JoypadConfig::IDD, pParent)
{
  //{{AFX_DATA_INIT(JoypadConfig)
  //}}AFX_DATA_INIT
  timerId = 0;
  which = w;
  if(which < 0 || which > 3)
    which = 0;
}

void JoypadConfig::DoDataExchange(CDataExchange* pDX)
{
  CDialog::DoDataExchange(pDX);
  //{{AFX_DATA_MAP(JoypadConfig)
  DDX_Control(pDX, IDC_EDIT_UP, up);
  DDX_Control(pDX, IDC_EDIT_SPEED, speed);
  DDX_Control(pDX, IDC_EDIT_RIGHT, right);
  DDX_Control(pDX, IDC_EDIT_LEFT, left);
  DDX_Control(pDX, IDC_EDIT_DOWN, down);
  DDX_Control(pDX, IDC_EDIT_CAPTURE, capture);
  DDX_Control(pDX, IDC_EDIT_BUTTON_START, buttonStart);
  DDX_Control(pDX, IDC_EDIT_BUTTON_SELECT, buttonSelect);
  DDX_Control(pDX, IDC_EDIT_BUTTON_R, buttonR);
  DDX_Control(pDX, IDC_EDIT_BUTTON_L, buttonL);
  DDX_Control(pDX, IDC_EDIT_BUTTON_GS, buttonGS);
  DDX_Control(pDX, IDC_EDIT_BUTTON_B, buttonB);
  DDX_Control(pDX, IDC_EDIT_BUTTON_A, buttonA);
  //}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(JoypadConfig, CDialog)
  ON_BN_CLICKED(ID_CANCEL, OnCancel)
  ON_BN_CLICKED(ID_OK, OnOk)
  ON_WM_CHAR()
  ON_WM_DESTROY()
  ON_WM_TIMER()
  ON_WM_KEYDOWN()
  ON_BN_CLICKED(IDC_APPENDMODE, &JoypadConfig::OnBnClickedAppendmode)
END_MESSAGE_MAP()

  /////////////////////////////////////////////////////////////////////////////
// JoypadConfig message handlers

void JoypadConfig::OnCancel()
{
  EndDialog(FALSE);
}

void JoypadConfig::OnOk()
{
  AssignKeys(up.m_Keys,theApp.input->joypaddata[JOYPAD(which,KEY_UP)]);
  AssignKeys(speed.m_Keys,theApp.input->joypaddata[JOYPAD(which,KEY_BUTTON_SPEED)]);
  AssignKeys(right.m_Keys,theApp.input->joypaddata[JOYPAD(which,KEY_RIGHT)]);
  AssignKeys(left.m_Keys,theApp.input->joypaddata[JOYPAD(which,KEY_LEFT)]);
  AssignKeys(down.m_Keys,theApp.input->joypaddata[JOYPAD(which,KEY_DOWN)]);
  AssignKeys(capture.m_Keys,theApp.input->joypaddata[JOYPAD(which,KEY_BUTTON_CAPTURE)]);
  AssignKeys(buttonStart.m_Keys,theApp.input->joypaddata[JOYPAD(which,KEY_BUTTON_START)]);
  AssignKeys(buttonSelect.m_Keys,theApp.input->joypaddata[JOYPAD(which,KEY_BUTTON_SELECT)]);
  AssignKeys(buttonR.m_Keys,theApp.input->joypaddata[JOYPAD(which,KEY_BUTTON_R)]);
  AssignKeys(buttonL.m_Keys,theApp.input->joypaddata[JOYPAD(which,KEY_BUTTON_L)]);
  AssignKeys(buttonGS.m_Keys,theApp.input->joypaddata[JOYPAD(which,KEY_BUTTON_GS)]);
  AssignKeys(buttonB.m_Keys,theApp.input->joypaddata[JOYPAD(which,KEY_BUTTON_B)]);
  AssignKeys(buttonA.m_Keys,theApp.input->joypaddata[JOYPAD(which,KEY_BUTTON_A)]);

  theApp.input->checkKeys();
  EndDialog(TRUE);
}

void JoypadConfig::OnChar(UINT nChar, UINT nRepCnt, UINT nFlags)
{
}

void JoypadConfig::OnDestroy()
{
  CDialog::OnDestroy();

  KillTimer(timerId);
}

void JoypadConfig::OnTimer(UINT_PTR nIDEvent)
{
  theApp.input->checkDevices();

  CDialog::OnTimer(nIDEvent);
}

void JoypadConfig::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
}

BOOL JoypadConfig::OnInitDialog()
{
  CDialog::OnInitDialog();

  bAppendMode = FALSE;

  timerId = SetTimer(0,50,NULL);

  CopyKeys(up.m_Keys,theApp.input->joypaddata[JOYPAD(which,KEY_UP)]);
  CopyKeys(speed.m_Keys,theApp.input->joypaddata[JOYPAD(which,KEY_BUTTON_SPEED)]);
  CopyKeys(right.m_Keys,theApp.input->joypaddata[JOYPAD(which,KEY_RIGHT)]);
  CopyKeys(left.m_Keys,theApp.input->joypaddata[JOYPAD(which,KEY_LEFT)]);
  CopyKeys(down.m_Keys,theApp.input->joypaddata[JOYPAD(which,KEY_DOWN)]);
  CopyKeys(capture.m_Keys,theApp.input->joypaddata[JOYPAD(which,KEY_BUTTON_CAPTURE)]);
  CopyKeys(buttonStart.m_Keys,theApp.input->joypaddata[JOYPAD(which,KEY_BUTTON_START)]);
  CopyKeys(buttonSelect.m_Keys,theApp.input->joypaddata[JOYPAD(which,KEY_BUTTON_SELECT)]);
  CopyKeys(buttonR.m_Keys,theApp.input->joypaddata[JOYPAD(which,KEY_BUTTON_R)]);
  CopyKeys(buttonL.m_Keys,theApp.input->joypaddata[JOYPAD(which,KEY_BUTTON_L)]);
  CopyKeys(buttonGS.m_Keys,theApp.input->joypaddata[JOYPAD(which,KEY_BUTTON_GS)]);
  CopyKeys(buttonB.m_Keys,theApp.input->joypaddata[JOYPAD(which,KEY_BUTTON_B)]);
  CopyKeys(buttonA.m_Keys,theApp.input->joypaddata[JOYPAD(which,KEY_BUTTON_A)]);

  up.SetWindowText(GetKeyListName(theApp.input->joypaddata[JOYPAD(which,KEY_UP)]));
  down.SetWindowText(GetKeyListName(theApp.input->joypaddata[JOYPAD(which,KEY_DOWN)]));
  left.SetWindowText(GetKeyListName(theApp.input->joypaddata[JOYPAD(which,KEY_LEFT)]));
  right.SetWindowText(GetKeyListName(theApp.input->joypaddata[JOYPAD(which,KEY_RIGHT)]));
  buttonA.SetWindowText(GetKeyListName(theApp.input->joypaddata[JOYPAD(which,KEY_BUTTON_A)]));
  buttonB.SetWindowText(GetKeyListName(theApp.input->joypaddata[JOYPAD(which,KEY_BUTTON_B)]));
  buttonL.SetWindowText(GetKeyListName(theApp.input->joypaddata[JOYPAD(which,KEY_BUTTON_L)]));
  buttonR.SetWindowText(GetKeyListName(theApp.input->joypaddata[JOYPAD(which,KEY_BUTTON_R)]));
  buttonSelect.SetWindowText(GetKeyListName(theApp.input->joypaddata[JOYPAD(which,KEY_BUTTON_SELECT)]));
  buttonStart.SetWindowText(GetKeyListName(theApp.input->joypaddata[JOYPAD(which,KEY_BUTTON_START)]));
  speed.SetWindowText(GetKeyListName(theApp.input->joypaddata[JOYPAD(which,KEY_BUTTON_SPEED)]));
  capture.SetWindowText(GetKeyListName(theApp.input->joypaddata[JOYPAD(which,KEY_BUTTON_CAPTURE)]));
  buttonGS.SetWindowText(GetKeyListName(theApp.input->joypaddata[JOYPAD(which,KEY_BUTTON_GS)]));

  CenterWindow();

  return TRUE;  // return TRUE unless you set the focus to a control
                // EXCEPTION: OCX Property Pages should return FALSE
}

void JoypadConfig::assignKey(int id, LONG_PTR key)
{
  switch(id) {
  case IDC_EDIT_LEFT:
    AssignKey(theApp.input->joypaddata[JOYPAD(which,KEY_LEFT)],key);
    break;
  case IDC_EDIT_RIGHT:
    AssignKey(theApp.input->joypaddata[JOYPAD(which,KEY_RIGHT)],key);
    break;
  case IDC_EDIT_UP:
    AssignKey(theApp.input->joypaddata[JOYPAD(which,KEY_UP)],key);
    break;
  case IDC_EDIT_SPEED:
    AssignKey(theApp.input->joypaddata[JOYPAD(which,KEY_BUTTON_SPEED)],key);
    break;
  case IDC_EDIT_CAPTURE:
    AssignKey(theApp.input->joypaddata[JOYPAD(which,KEY_BUTTON_CAPTURE)],key);
    break;
  case IDC_EDIT_DOWN:
    AssignKey(theApp.input->joypaddata[JOYPAD(which,KEY_DOWN)],key);
    break;
  case IDC_EDIT_BUTTON_A:
    AssignKey(theApp.input->joypaddata[JOYPAD(which,KEY_BUTTON_A)],key);
    break;
  case IDC_EDIT_BUTTON_B:
    AssignKey(theApp.input->joypaddata[JOYPAD(which,KEY_BUTTON_B)],key);
    break;
  case IDC_EDIT_BUTTON_L:
    AssignKey(theApp.input->joypaddata[JOYPAD(which,KEY_BUTTON_L)],key);
    break;
  case IDC_EDIT_BUTTON_R:
    AssignKey(theApp.input->joypaddata[JOYPAD(which,KEY_BUTTON_R)],key);
    break;
  case IDC_EDIT_BUTTON_START:
    AssignKey(theApp.input->joypaddata[JOYPAD(which,KEY_BUTTON_START)],key);
    break;
  case IDC_EDIT_BUTTON_SELECT:
    AssignKey(theApp.input->joypaddata[JOYPAD(which,KEY_BUTTON_SELECT)],key);
    break;
  case IDC_EDIT_BUTTON_GS:
    AssignKey(theApp.input->joypaddata[JOYPAD(which,KEY_BUTTON_GS)],key);
    break;
  }
}


/////////////////////////////////////////////////////////////////////////////
// MotionConfig dialog


MotionConfig::MotionConfig(CWnd* pParent /*=NULL*/)
  : CDialog(MotionConfig::IDD, pParent)
{
  //{{AFX_DATA_INIT(MotionConfig)
  // NOTE: the ClassWizard will add member initialization here
  //}}AFX_DATA_INIT
  timerId = 0;
  bAppendMode = 0;
}


void MotionConfig::DoDataExchange(CDataExchange* pDX)
{
  CDialog::DoDataExchange(pDX);
  //{{AFX_DATA_MAP(MotionConfig)
  DDX_Control(pDX, IDC_EDIT_UP, up);
  DDX_Control(pDX, IDC_EDIT_RIGHT, right);
  DDX_Control(pDX, IDC_EDIT_LEFT, left);
  DDX_Control(pDX, IDC_EDIT_DOWN, down);
  //}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(MotionConfig, CDialog)
  ON_BN_CLICKED(ID_CANCEL, OnCancel)
  ON_BN_CLICKED(ID_OK, OnOk)
  ON_WM_DESTROY()
  ON_WM_KEYDOWN()
  ON_WM_TIMER()
  ON_BN_CLICKED(IDC_APPENDMODE, &MotionConfig::OnBnClickedAppendmode)
END_MESSAGE_MAP()

  /////////////////////////////////////////////////////////////////////////////
// MotionConfig message handlers

void MotionConfig::OnCancel()
{
  EndDialog(FALSE);
}

void MotionConfig::OnOk()
{
  assignKeys();
  theApp.input->checkKeys();
  EndDialog( TRUE);
}

void MotionConfig::OnDestroy()
{
  CDialog::OnDestroy();

  KillTimer(timerId);
}

BOOL MotionConfig::OnInitDialog()
{
  CDialog::OnInitDialog();

  timerId = SetTimer(0,50,NULL);

  CopyKeys(up.m_Keys, theApp.input->joypaddata[MOTION(KEY_UP)]);
  up.SetWindowText(GetKeyListName(theApp.input->joypaddata[MOTION(KEY_UP)]));

  CopyKeys(down.m_Keys, theApp.input->joypaddata[MOTION(KEY_DOWN)]);
  down.SetWindowText(GetKeyListName(theApp.input->joypaddata[MOTION(KEY_DOWN)]));

  CopyKeys(left.m_Keys, theApp.input->joypaddata[MOTION(KEY_LEFT)]);
  left.SetWindowText(GetKeyListName(theApp.input->joypaddata[MOTION(KEY_LEFT)]));

  CopyKeys(right.m_Keys, theApp.input->joypaddata[MOTION(KEY_RIGHT)]);
  right.SetWindowText(GetKeyListName(theApp.input->joypaddata[MOTION(KEY_RIGHT)]));

  CenterWindow();

  return TRUE;  // return TRUE unless you set the focus to a control
                // EXCEPTION: OCX Property Pages should return FALSE
}

void MotionConfig::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
}

void MotionConfig::OnTimer(UINT nIDEvent)
{
  theApp.input->checkDevices();

  CDialog::OnTimer(nIDEvent);
}

void MotionConfig::assignKey(int id, int key)
{
  switch(id) {
  case IDC_EDIT_LEFT:
    AssignKey(theApp.input->joypaddata[MOTION(KEY_LEFT)],key);
    break;
  case IDC_EDIT_RIGHT:
    AssignKey(theApp.input->joypaddata[MOTION(KEY_RIGHT)],key);
    break;
  case IDC_EDIT_UP:
    AssignKey(theApp.input->joypaddata[MOTION(KEY_UP)],key);
    break;
  case IDC_EDIT_DOWN:
    AssignKey(theApp.input->joypaddata[MOTION(KEY_DOWN)],key);
    break;
  }
}

void MotionConfig::assignKeys()
{
  int id;

  id = IDC_EDIT_UP;
  assignKey(id, GetWindowLong(up, GWL_USERDATA));

  id = IDC_EDIT_DOWN;
  assignKey(id, GetWindowLong(down, GWL_USERDATA));

  id = IDC_EDIT_LEFT;
  assignKey(id, GetWindowLong(left, GWL_USERDATA));

  id = IDC_EDIT_RIGHT;
  assignKey(id, GetWindowLong(right, GWL_USERDATA));
}

void JoypadConfig::OnBnClickedAppendmode()
{
	bAppendMode = (::SendMessage(GetDlgItem(IDC_APPENDMODE)->GetSafeHwnd(), BM_GETCHECK, 0, 0L) != 0);
}

void MotionConfig::OnBnClickedAppendmode()
{
	bAppendMode = (::SendMessage(GetDlgItem(IDC_APPENDMODE)->GetSafeHwnd(), BM_GETCHECK, 0, 0L) != 0);
}
