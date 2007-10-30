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

// MemoryViewerAddressSize.cpp : implementation file
//

#include "stdafx.h"
#include "vba.h"
#include "MemoryViewerAddressSize.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// MemoryViewerAddressSize dialog


MemoryViewerAddressSize::MemoryViewerAddressSize(u32 a, int s, CWnd* pParent /*=NULL*/)
  : CDialog(MemoryViewerAddressSize::IDD, pParent)
{
  //{{AFX_DATA_INIT(MemoryViewerAddressSize)
  // NOTE: the ClassWizard will add member initialization here
  //}}AFX_DATA_INIT
  address = a;
  size = s;
}


void MemoryViewerAddressSize::DoDataExchange(CDataExchange* pDX)
{
  CDialog::DoDataExchange(pDX);
  //{{AFX_DATA_MAP(MemoryViewerAddressSize)
  DDX_Control(pDX, IDC_SIZE_CONTROL, m_size);
  DDX_Control(pDX, IDC_ADDRESS, m_address);
  //}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(MemoryViewerAddressSize, CDialog)
  //{{AFX_MSG_MAP(MemoryViewerAddressSize)
  ON_BN_CLICKED(ID_OK, OnOk)
  ON_BN_CLICKED(ID_CANCEL, OnCancel)
  //}}AFX_MSG_MAP
  END_MESSAGE_MAP()

  /////////////////////////////////////////////////////////////////////////////
// MemoryViewerAddressSize message handlers

BOOL MemoryViewerAddressSize::OnInitDialog() 
{
  CDialog::OnInitDialog();
  
  CString buffer;
  if(address != 0xFFFFFFFF) {
    buffer.Format("%08X", address);
    m_address.SetWindowText(buffer);
  }
  if(size != -1) {
    buffer.Format("%08X", size);
    m_size.SetWindowText(buffer);
    m_size.EnableWindow(FALSE);
  }

  if(size == -1 && address != 0xFFFFFFFF)
    m_size.SetFocus();
  
  m_address.LimitText(9);
  m_size.LimitText(9);
  
  return TRUE;  // return TRUE unless you set the focus to a control
                // EXCEPTION: OCX Property Pages should return FALSE
}

void MemoryViewerAddressSize::OnOk() 
{
  CString buffer;

  m_address.GetWindowText(buffer);
  if(buffer.IsEmpty()) {
    m_address.SetFocus();
    return;
  }
  sscanf(buffer, "%x", &address);
  
  m_size.GetWindowText(buffer);
  if(buffer.IsEmpty()) {
    m_size.SetFocus();
    return;
  }
  sscanf(buffer, "%x", &size);
  EndDialog(TRUE);
}

void MemoryViewerAddressSize::OnCancel() 
{
  EndDialog(FALSE);
}

void MemoryViewerAddressSize::setAddress(u32 a)
{
  address = a;
}

void MemoryViewerAddressSize::setSize(int s)
{
  size = s;
}


u32 MemoryViewerAddressSize::getAddress()
{
  return address;
}
\

int MemoryViewerAddressSize::getSize()
{
  return size;
}
