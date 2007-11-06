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

extern LONG_PTR joypad[4][13];
extern LONG_PTR motion[4];

/////////////////////////////////////////////////////////////////////////////
// JoypadEditControl

JoypadEditControl::JoypadEditControl()
{
}

JoypadEditControl::~JoypadEditControl()
{
}


BEGIN_MESSAGE_MAP(JoypadEditControl, CEdit)
  ON_MESSAGE(JOYCONFIG_MESSAGE, OnJoyConfig)
  END_MESSAGE_MAP()

  /////////////////////////////////////////////////////////////////////////////
// JoypadEditControl message handlers

LRESULT JoypadEditControl::OnJoyConfig(WPARAM wParam, LPARAM lParam)
{
	SetWindowLongPtr( this->GetSafeHwnd(), GWLP_USERDATA, (wParam<<8) | lParam );
	this->SetWindowText( theApp.input->getKeyName( (int)((wParam<<8)|lParam) ) );
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
  ON_WM_DESTROY()
  ON_WM_TIMER()
  ON_WM_KEYDOWN()
  END_MESSAGE_MAP()

  /////////////////////////////////////////////////////////////////////////////
// JoypadConfig message handlers

void JoypadConfig::OnCancel() 
{
  EndDialog(FALSE);
}

void JoypadConfig::OnOk() 
{
  assignKeys();
  theApp.input->checkKeys();
  EndDialog(TRUE);
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
  
  timerId = SetTimer(0,50,NULL);
  
  SetWindowLongPtr(up, GWLP_USERDATA,joypad[which][KEY_UP]);
  up.SetWindowText(theApp.input->getKeyName(joypad[which][KEY_UP]));
  
  SetWindowLongPtr(down, GWLP_USERDATA,joypad[which][KEY_DOWN]);
  down.SetWindowText(theApp.input->getKeyName(joypad[which][KEY_DOWN]));

  SetWindowLongPtr(left, GWLP_USERDATA,joypad[which][KEY_LEFT]);
  left.SetWindowText(theApp.input->getKeyName(joypad[which][KEY_LEFT]));

  SetWindowLongPtr(right, GWLP_USERDATA,joypad[which][KEY_RIGHT]);
  right.SetWindowText(theApp.input->getKeyName(joypad[which][KEY_RIGHT]));

  SetWindowLongPtr(buttonA, GWLP_USERDATA,joypad[which][KEY_BUTTON_A]);
  buttonA.SetWindowText(theApp.input->getKeyName(joypad[which][KEY_BUTTON_A]));

  SetWindowLongPtr(buttonB, GWLP_USERDATA,joypad[which][KEY_BUTTON_B]);
  buttonB.SetWindowText(theApp.input->getKeyName(joypad[which][KEY_BUTTON_B]));
  
  SetWindowLongPtr(buttonL, GWLP_USERDATA,joypad[which][KEY_BUTTON_L]);
  buttonL.SetWindowText(theApp.input->getKeyName(joypad[which][KEY_BUTTON_L]));

  SetWindowLongPtr(buttonR, GWLP_USERDATA,joypad[which][KEY_BUTTON_R]);
  buttonR.SetWindowText(theApp.input->getKeyName(joypad[which][KEY_BUTTON_R]));
  
  SetWindowLongPtr(buttonSelect, GWLP_USERDATA,joypad[which][KEY_BUTTON_SELECT]);
  buttonSelect.SetWindowText(theApp.input->getKeyName(joypad[which][KEY_BUTTON_SELECT]));

  SetWindowLongPtr(buttonStart, GWLP_USERDATA,joypad[which][KEY_BUTTON_START]);
  buttonStart.SetWindowText(theApp.input->getKeyName(joypad[which][KEY_BUTTON_START]));

  SetWindowLongPtr(speed, GWLP_USERDATA,joypad[which][KEY_BUTTON_SPEED]);
  speed.SetWindowText(theApp.input->getKeyName(joypad[which][KEY_BUTTON_SPEED]));
  
  SetWindowLongPtr(capture, GWLP_USERDATA,joypad[which][KEY_BUTTON_CAPTURE]);
  capture.SetWindowText(theApp.input->getKeyName(joypad[which][KEY_BUTTON_CAPTURE]));

  SetWindowLongPtr(buttonGS, GWLP_USERDATA,joypad[which][KEY_BUTTON_GS]);
  buttonGS.SetWindowText(theApp.input->getKeyName(joypad[which][KEY_BUTTON_GS]));
  
  CenterWindow();

  return TRUE;  // return TRUE unless you set the focus to a control
                // EXCEPTION: OCX Property Pages should return FALSE
}

void JoypadConfig::assignKey(int id, LONG_PTR key)
{
  switch(id) {
  case IDC_EDIT_LEFT:
    joypad[which][KEY_LEFT] = key;
    break;
  case IDC_EDIT_RIGHT:
    joypad[which][KEY_RIGHT] = key;
    break;
  case IDC_EDIT_UP:
    joypad[which][KEY_UP] = key;
    break;
  case IDC_EDIT_SPEED:
    joypad[which][KEY_BUTTON_SPEED] = key;
    break;
  case IDC_EDIT_CAPTURE:
    joypad[which][KEY_BUTTON_CAPTURE] = key;
    break;    
  case IDC_EDIT_DOWN:
    joypad[which][KEY_DOWN] = key;
    break;
  case IDC_EDIT_BUTTON_A:
    joypad[which][KEY_BUTTON_A] = key;
    break;
  case IDC_EDIT_BUTTON_B:
    joypad[which][KEY_BUTTON_B] = key;
    break;
  case IDC_EDIT_BUTTON_L:
    joypad[which][KEY_BUTTON_L] = key;
    break;
  case IDC_EDIT_BUTTON_R:
    joypad[which][KEY_BUTTON_R] = key;
    break;
  case IDC_EDIT_BUTTON_START:
    joypad[which][KEY_BUTTON_START] = key;
    break;
  case IDC_EDIT_BUTTON_SELECT:
    joypad[which][KEY_BUTTON_SELECT] = key;
    break;
  case IDC_EDIT_BUTTON_GS:
    joypad[which][KEY_BUTTON_GS] = key;
    break;
  }
}

void JoypadConfig::assignKeys()
{
  int id;

  id = IDC_EDIT_UP;
  assignKey(id, GetWindowLongPtr(up, GWLP_USERDATA));

  id = IDC_EDIT_DOWN;
  assignKey(id, GetWindowLongPtr(down, GWLP_USERDATA));

  id = IDC_EDIT_LEFT;
  assignKey(id, GetWindowLongPtr(left, GWLP_USERDATA));

  id = IDC_EDIT_RIGHT;
  assignKey(id, GetWindowLongPtr(right, GWLP_USERDATA));

  id = IDC_EDIT_BUTTON_A;
  assignKey(id, GetWindowLongPtr(buttonA, GWLP_USERDATA));

  id = IDC_EDIT_BUTTON_B;
  assignKey(id, GetWindowLongPtr(buttonB, GWLP_USERDATA));

  id = IDC_EDIT_BUTTON_L;
  assignKey(id, GetWindowLongPtr(buttonL, GWLP_USERDATA));

  id = IDC_EDIT_BUTTON_R;
  assignKey(id, GetWindowLongPtr(buttonR, GWLP_USERDATA));
  
  id = IDC_EDIT_BUTTON_SELECT;
  assignKey(id, GetWindowLongPtr(buttonSelect, GWLP_USERDATA));

  id = IDC_EDIT_BUTTON_START;
  assignKey(id, GetWindowLongPtr(buttonStart, GWLP_USERDATA));

  id = IDC_EDIT_SPEED;
  assignKey(id, GetWindowLongPtr(speed, GWLP_USERDATA));

  id = IDC_EDIT_CAPTURE;
  assignKey(id, GetWindowLongPtr(capture, GWLP_USERDATA));

  id = IDC_EDIT_BUTTON_GS;
  assignKey(id, GetWindowLongPtr(buttonGS, GWLP_USERDATA));

  //  winSaveKeys();
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
  
  timerId = SetTimer(0,200,NULL);
  
  SetWindowLongPtr(up, GWLP_USERDATA,motion[KEY_UP]);
  up.SetWindowText(theApp.input->getKeyName(motion[KEY_UP]));
  
  SetWindowLongPtr(down, GWLP_USERDATA,motion[KEY_DOWN]);
  down.SetWindowText(theApp.input->getKeyName(motion[KEY_DOWN]));

  SetWindowLongPtr(left, GWLP_USERDATA,motion[KEY_LEFT]);
  left.SetWindowText(theApp.input->getKeyName(motion[KEY_LEFT]));

  SetWindowLongPtr(right, GWLP_USERDATA,motion[KEY_RIGHT]);
  right.SetWindowText(theApp.input->getKeyName(motion[KEY_RIGHT]));

  CenterWindow();

  return TRUE;  // return TRUE unless you set the focus to a control
                // EXCEPTION: OCX Property Pages should return FALSE
}

void MotionConfig::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags) 
{
}

void MotionConfig::OnTimer(UINT_PTR nIDEvent) 
{
  theApp.input->checkDevices();
  
  CDialog::OnTimer(nIDEvent);
}

void MotionConfig::assignKey(int id, LONG_PTR key)
{
  switch(id) {
  case IDC_EDIT_LEFT:
    motion[KEY_LEFT] = key;
    break;
  case IDC_EDIT_RIGHT:
    motion[KEY_RIGHT] = key;
    break;
  case IDC_EDIT_UP:
    motion[KEY_UP] = key;
    break;
  case IDC_EDIT_DOWN:
    motion[KEY_DOWN] = key;
    break;
  }
}

void MotionConfig::assignKeys()
{
  int id;

  id = IDC_EDIT_UP;
  assignKey(id, GetWindowLongPtr(up, GWLP_USERDATA));

  id = IDC_EDIT_DOWN;
  assignKey(id, GetWindowLongPtr(down, GWLP_USERDATA));

  id = IDC_EDIT_LEFT;
  assignKey(id, GetWindowLongPtr(left, GWLP_USERDATA));

  id = IDC_EDIT_RIGHT;
  assignKey(id, GetWindowLongPtr(right, GWLP_USERDATA));
}
