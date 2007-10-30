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

// Logging.cpp : implementation file
//

#include "stdafx.h"
#include "vba.h"

#include "FileDlg.h"
#include "Logging.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// Logging dialog

Logging *Logging::instance = NULL;
CString Logging::text;

Logging::Logging(CWnd* pParent /*=NULL*/)
  : ResizeDlg(Logging::IDD, pParent)
{
  //{{AFX_DATA_INIT(Logging)
  m_swi = FALSE;
  m_unaligned_access = FALSE;
  m_illegal_write = FALSE;
  m_illegal_read = FALSE;
  m_dma0 = FALSE;
  m_dma1 = FALSE;
  m_dma2 = FALSE;
  m_dma3 = FALSE;
  m_agbprint = FALSE;
  m_undefined = FALSE;
  //}}AFX_DATA_INIT
}


void Logging::DoDataExchange(CDataExchange* pDX)
{
  CDialog::DoDataExchange(pDX);
  //{{AFX_DATA_MAP(Logging)
  DDX_Control(pDX, IDC_LOG, m_log);
  DDX_Check(pDX, IDC_VERBOSE_SWI, m_swi);
  DDX_Check(pDX, IDC_VERBOSE_UNALIGNED_ACCESS, m_unaligned_access);
  DDX_Check(pDX, IDC_VERBOSE_ILLEGAL_WRITE, m_illegal_write);
  DDX_Check(pDX, IDC_VERBOSE_ILLEGAL_READ, m_illegal_read);
  DDX_Check(pDX, IDC_VERBOSE_DMA0, m_dma0);
  DDX_Check(pDX, IDC_VERBOSE_DMA1, m_dma1);
  DDX_Check(pDX, IDC_VERBOSE_DMA2, m_dma2);
  DDX_Check(pDX, IDC_VERBOSE_DMA3, m_dma3);
  DDX_Check(pDX, IDC_VERBOSE_AGBPRINT, m_agbprint);
  DDX_Check(pDX, IDC_VERBOSE_UNDEFINED, m_undefined);
  //}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(Logging, CDialog)
  //{{AFX_MSG_MAP(Logging)
  ON_BN_CLICKED(ID_OK, OnOk)
  ON_BN_CLICKED(IDC_CLEAR, OnClear)
  ON_BN_CLICKED(IDC_VERBOSE_AGBPRINT, OnVerboseAgbprint)
  ON_BN_CLICKED(IDC_VERBOSE_DMA0, OnVerboseDma0)
  ON_BN_CLICKED(IDC_VERBOSE_DMA1, OnVerboseDma1)
  ON_BN_CLICKED(IDC_VERBOSE_DMA2, OnVerboseDma2)
  ON_BN_CLICKED(IDC_VERBOSE_DMA3, OnVerboseDma3)
  ON_BN_CLICKED(IDC_VERBOSE_ILLEGAL_READ, OnVerboseIllegalRead)
  ON_BN_CLICKED(IDC_VERBOSE_ILLEGAL_WRITE, OnVerboseIllegalWrite)
  ON_BN_CLICKED(IDC_VERBOSE_SWI, OnVerboseSwi)
  ON_BN_CLICKED(IDC_VERBOSE_UNALIGNED_ACCESS, OnVerboseUnalignedAccess)
  ON_BN_CLICKED(IDC_VERBOSE_UNDEFINED, OnVerboseUndefined)
  ON_BN_CLICKED(IDC_SAVE, OnSave)
  ON_EN_ERRSPACE(IDC_LOG, OnErrspaceLog)
  ON_EN_MAXTEXT(IDC_LOG, OnMaxtextLog)
  ON_WM_CLOSE()
  //}}AFX_MSG_MAP
  END_MESSAGE_MAP()

  /////////////////////////////////////////////////////////////////////////////
// Logging message handlers

void Logging::OnOk() 
{
  EndDialog(TRUE);

  instance = NULL;
}

void Logging::OnClear() 
{
  text = "";
  m_log.SetWindowText("");
}

void Logging::OnVerboseAgbprint() 
{
  systemVerbose ^= 512;
}

void Logging::OnVerboseDma0() 
{
  systemVerbose ^= 16;
}

void Logging::OnVerboseDma1() 
{
  systemVerbose ^= 32;
}

void Logging::OnVerboseDma2() 
{
  systemVerbose ^= 64;
}

void Logging::OnVerboseDma3() 
{
  systemVerbose ^= 128;
}

void Logging::OnVerboseIllegalRead() 
{
  systemVerbose ^= 8;
}

void Logging::OnVerboseIllegalWrite() 
{
  systemVerbose ^= 4;
}

void Logging::OnVerboseSwi() 
{
  systemVerbose ^= 1;
}

void Logging::OnVerboseUnalignedAccess() 
{
  systemVerbose ^= 2;
}

void Logging::OnVerboseUndefined() 
{
  systemVerbose ^= 256;
}

void Logging::OnSave() 
{
  int len = m_log.GetWindowTextLength();

  char *mem = (char *)malloc(len);

  if(mem) {
    LPCTSTR exts[] = { ".txt" };
    m_log.GetWindowText(mem, len);
    CString filter = "All Files|*.*||";
    FileDlg dlg(this, "", filter, 0,
                NULL, exts, NULL, "Save output", true);

    if(dlg.DoModal() == IDOK) {
      FILE *f = fopen(dlg.GetPathName(), "w");
      if(f) {
        fwrite(mem, 1, len, f);
        fclose(f);
      }
    }
  }

  free(mem);
}

void Logging::OnErrspaceLog() 
{
  systemMessage(0, "Error allocating space");
}

void Logging::OnMaxtextLog() 
{
  systemMessage(0, "Max text length reached %d", m_log.GetLimitText());
}

void Logging::PostNcDestroy() 
{
  delete this;
}

BOOL Logging::OnInitDialog() 
{
  CDialog::OnInitDialog();
  
  DIALOG_SIZER_START( sz )
    DIALOG_SIZER_ENTRY( IDC_LOG, DS_SizeY|DS_SizeX)
    DIALOG_SIZER_ENTRY( ID_OK, DS_MoveY)
    DIALOG_SIZER_ENTRY( IDC_CLEAR, DS_MoveY)
    DIALOG_SIZER_ENTRY( IDC_SAVE, DS_MoveY)
    DIALOG_SIZER_END()
    SetData(sz,
            TRUE,
            HKEY_CURRENT_USER,
            "Software\\Emulators\\VisualBoyAdvance\\Viewer\\LogView",
            NULL);
  m_swi = (systemVerbose & 1) != 0;
  m_unaligned_access = (systemVerbose & 2) != 0;
  m_illegal_write = (systemVerbose & 4) != 0;
  m_illegal_read = (systemVerbose & 8) != 0;
  m_dma0 = (systemVerbose & 16) != 0;
  m_dma1 = (systemVerbose & 32) != 0;
  m_dma2 = (systemVerbose & 64) != 0;
  m_dma3 = (systemVerbose & 128) != 0;
  m_undefined = (systemVerbose & 256) != 0;
  m_agbprint = (systemVerbose & 256) != 0;
  UpdateData(FALSE);

  m_log.LimitText(-1);
  m_log.SetWindowText(text);
  
  return TRUE;  // return TRUE unless you set the focus to a control
                // EXCEPTION: OCX Property Pages should return FALSE
}

void Logging::log(const char *s)
{
  int size = ::SendMessage(m_log, WM_GETTEXTLENGTH, 0, 0);
  m_log.SetSel(size, size);
  m_log.ReplaceSel(s);
}

void Logging::OnClose() 
{
  EndDialog(FALSE);

  instance = NULL;
  
  CDialog::OnClose();
}

void toolsLogging()
{
  if(Logging::instance == NULL) {
    Logging::instance = new Logging();
    Logging::instance->Create(IDD_LOGGING, AfxGetApp()->m_pMainWnd);
    Logging::instance->ShowWindow(SW_SHOW);
  } else {
    Logging::instance->SetForegroundWindow();
  }
}

void toolsLog(const char *s)
{
  CString str;
  int state = 0;
  if(s) {
    char c = *s++;
    while(c) {
      if(c == '\n' && state == 0)
        str += '\r';
      else if(c == '\r')
        state = 1;
      else
        state = 0;
      str += c;
      c = *s++;
    }
  }

  Logging::text += str;
  if(Logging::instance != NULL) {
    Logging::instance->log(str);
  }
}
