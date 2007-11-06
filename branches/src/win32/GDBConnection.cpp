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

// GDBConnection.cpp : implementation file
//

#include "stdafx.h"
#include "vba.h"
#include "GDBConnection.h"

#include <winsock.h>

#define SOCKET_MESSAGE WM_APP+1

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

static bool initialized = false;

/////////////////////////////////////////////////////////////////////////////
// GDBPortDlg dialog


GDBPortDlg::GDBPortDlg(CWnd* pParent /*=NULL*/)
  : CDialog(GDBPortDlg::IDD, pParent)
{
  //{{AFX_DATA_INIT(GDBPortDlg)
  // NOTE: the ClassWizard will add member initialization here
  //}}AFX_DATA_INIT
  port = 55555;
  sock = INVALID_SOCKET;
  
  if(!initialized) {
    WSADATA wsaData;

    int error = WSAStartup(MAKEWORD(1,1), &wsaData);

    initialized = true;
  }
}


void GDBPortDlg::DoDataExchange(CDataExchange* pDX)
{
  CDialog::DoDataExchange(pDX);
  //{{AFX_DATA_MAP(GDBPortDlg)
  DDX_Control(pDX, IDC_PORT, m_port);
  //}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(GDBPortDlg, CDialog)
  //{{AFX_MSG_MAP(GDBPortDlg)
  ON_BN_CLICKED(ID_OK, OnOk)
  ON_BN_CLICKED(ID_CANCEL, OnCancel)
  ON_WM_CLOSE()
  //}}AFX_MSG_MAP
  END_MESSAGE_MAP()

  /////////////////////////////////////////////////////////////////////////////
// GDBPortDlg message handlers

int GDBPortDlg::getPort()
{
  return port;
}


SOCKET GDBPortDlg::getSocket()
{
  return sock;
}


BOOL GDBPortDlg::OnInitDialog() 
{
  CDialog::OnInitDialog();
  
  CString buffer;

  buffer.Format("%d", port);

  m_port.SetWindowText(buffer);

  CenterWindow();
  
  return TRUE;  // return TRUE unless you set the focus to a control
                // EXCEPTION: OCX Property Pages should return FALSE
}

void GDBPortDlg::OnOk() 
{
  CString buffer;

  m_port.GetWindowText(buffer);
  
  sockaddr_in address;
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = inet_addr("0.0.0.0");
  address.sin_port = htons(atoi(buffer));
  port = ntohs(address.sin_port);

  SOCKET s = socket(AF_INET, SOCK_STREAM, 0);

  if(s != INVALID_SOCKET) {
    int error = bind(s, (sockaddr *)&address, sizeof(address));

    if(error) {
      systemMessage(IDS_ERROR_BINDING, "Error binding socket. Port probably in use.");
      error = closesocket(s);
      EndDialog(FALSE);
    } else {
      error = listen(s, 1);
      if(!error) {
        sock = s;
        EndDialog(TRUE);
      } else {
        systemMessage(IDS_ERROR_LISTENING, "Error listening on socket.");
        closesocket(s);
        EndDialog(FALSE);
      }
    }
  } else {
    systemMessage(IDS_ERROR_CREATING_SOCKET, "Error creating socket.");
    EndDialog(FALSE);
  }
}

void GDBPortDlg::OnCancel() 
{
  OnClose();
}

void GDBPortDlg::OnClose() 
{
  EndDialog(FALSE);
}
/////////////////////////////////////////////////////////////////////////////
// GDBWaitingDlg dialog


GDBWaitingDlg::GDBWaitingDlg(SOCKET s, int p, CWnd* pParent /*=NULL*/)
  : CDialog(GDBWaitingDlg::IDD, pParent)
{
  //{{AFX_DATA_INIT(GDBWaitingDlg)
  //}}AFX_DATA_INIT
  port = p & 65535;
  listenSocket = s;
}


void GDBWaitingDlg::DoDataExchange(CDataExchange* pDX)
{
  CDialog::DoDataExchange(pDX);
  //{{AFX_DATA_MAP(GDBWaitingDlg)
  DDX_Control(pDX, IDC_PORT, m_port);
  //}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(GDBWaitingDlg, CDialog)
  //{{AFX_MSG_MAP(GDBWaitingDlg)
  ON_BN_CLICKED(ID_CANCEL, OnCancel)
  ON_WM_CLOSE()
  //}}AFX_MSG_MAP
  END_MESSAGE_MAP()

  /////////////////////////////////////////////////////////////////////////////
// GDBWaitingDlg message handlers

BOOL GDBWaitingDlg::OnInitDialog() 
{
  CDialog::OnInitDialog();
  
  CString buffer;

  buffer.Format("%d", port);

  m_port.SetWindowText(buffer);

  CenterWindow();

  int error = WSAAsyncSelect(listenSocket,
                             (HWND )*this,
                             SOCKET_MESSAGE,
                             FD_ACCEPT);
  
  return TRUE;  // return TRUE unless you set the focus to a control
                // EXCEPTION: OCX Property Pages should return FALSE
}

LRESULT GDBWaitingDlg::OnSocketAccept(WPARAM wParam, LPARAM lParam)
{
  if(LOWORD(lParam) == FD_ACCEPT) {
    WSAAsyncSelect(listenSocket, (HWND)*this, 0, 0);
    
    int flag = 0;    
    ioctlsocket(listenSocket, FIONBIO, (unsigned long *)&flag);
    
    SOCKET s = accept(listenSocket, NULL, NULL);
    if(s != INVALID_SOCKET) {
      char dummy;
      recv(s, &dummy, 1, 0);
      if(dummy != '+') {
        systemMessage(IDS_ACK_NOT_RECEIVED, "ACK not received from GDB.");
        OnClose(); // calls EndDialog
      } else {
        sock = s;
        EndDialog(TRUE);
      }
    }
  }

  return TRUE;
}

void GDBWaitingDlg::OnCancel() 
{
  OnClose();
}

void GDBWaitingDlg::OnClose() 
{
  if(sock != INVALID_SOCKET) {
    closesocket(sock);
    sock = INVALID_SOCKET;
  }
  if(listenSocket != INVALID_SOCKET) {
    closesocket(listenSocket);
    listenSocket = INVALID_SOCKET;
  }
  EndDialog(FALSE);
}

SOCKET GDBWaitingDlg::getListenSocket()
{
  return listenSocket;
}

SOCKET GDBWaitingDlg::getSocket()
{
  return sock;
}
