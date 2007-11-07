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

// VideoMode.cpp : implementation file
//

#include "stdafx.h"
#include "VBA.h"

#define DIRECTDRAW_VERSION 0x0700
#include <ddraw.h>

#include "VideoMode.h"

#include "../System.h"
#include "resource.h"


#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define MAX_DRIVERS         32                  // 32 drivers maximum

//-----------------------------------------------------------------------------
// Local structures
//-----------------------------------------------------------------------------
// Keeps data on the available DDraw drivers
struct
{
  char        szDescription[128];
  char        szName[128];
  GUID        *pGUID;
  GUID        GUIDcopy;
  HMONITOR    hm;
} Drivers[MAX_DRIVERS];

//-----------------------------------------------------------------------------
// Local data
//-----------------------------------------------------------------------------
static int                  gDriverCnt = 0;     // Total number of drivers
static GUID                *gpSelectedDriverGUID;

//-----------------------------------------------------------------------------
// Name: DDEnumCallbackEx()
// Desc: This call back is used to determine the existing available DDraw
//       devices, so the user can pick which one to run on.
//-----------------------------------------------------------------------------
BOOL WINAPI 
DDEnumCallbackEx(GUID *pGUID, LPSTR pDescription, LPSTR pName, LPVOID pContext, HMONITOR hm)
{
  if (pGUID)
    {
      Drivers[gDriverCnt].GUIDcopy = *pGUID;
      Drivers[gDriverCnt].pGUID = &Drivers[gDriverCnt].GUIDcopy;
    }
  else
    Drivers[gDriverCnt].pGUID = NULL;
  Drivers[gDriverCnt].szDescription[127] = '\0';
  Drivers[gDriverCnt].szName[127] = '\0';
  strncpy(Drivers[gDriverCnt].szDescription,pDescription,127);
  strncpy(Drivers[gDriverCnt].szName,pName,127);
  Drivers[gDriverCnt].hm = hm;
  if (gDriverCnt < MAX_DRIVERS)
    gDriverCnt++;
  else
    return DDENUMRET_CANCEL;
  return DDENUMRET_OK;
}




//-----------------------------------------------------------------------------
// Name: DDEnumCallback()
// Desc: This callback is used only with old versions of DDraw.
//-----------------------------------------------------------------------------
BOOL WINAPI 
DDEnumCallback(GUID *pGUID, LPSTR pDescription, LPSTR pName, LPVOID context)
{
  return (DDEnumCallbackEx(pGUID, pDescription, pName, context, NULL));
}

static HRESULT WINAPI addVideoMode(LPDDSURFACEDESC2 surf, LPVOID lpContext)
{
	HWND h = (HWND)lpContext;
	char buffer[50];
	
	switch( surf->ddpfPixelFormat.dwRGBBitCount )
	{
	case 16:
	case 24:
	case 32:
		sprintf(
			buffer,
			_T("%4dx%4dx%2d"),
			surf->dwWidth,
			surf->dwHeight,
			surf->ddpfPixelFormat.dwRGBBitCount
			);
		WPARAM pos = ::SendMessage( h, LB_ADDSTRING, 0, (LPARAM)buffer );
		::SendMessage(
			h,
			LB_SETITEMDATA,
			pos,
			(surf->ddpfPixelFormat.dwRGBBitCount << 24) |
			((surf->dwWidth & 4095) << 12) |
			(surf->dwHeight & 4095)
			);
    break;
  }

  return DDENUMRET_OK;
}

int winVideoModeSelect(CWnd *pWnd, GUID **guid)
{
#ifdef _AFXDLL
  HINSTANCE h = AfxLoadLibrary("ddraw.dll");
#else
  HMODULE h = LoadLibrary( _T("ddraw.dll") );
#endif
 
  // If ddraw.dll doesn't exist in the search path,
  // then DirectX probably isn't installed, so fail.
  if (!h)
    return -1;
  
  gDriverCnt = 0;
  
  // Note that you must know which version of the
  // function to retrieve (see the following text).
  // For this example, we use the ANSI version.
  LPDIRECTDRAWENUMERATEEX lpDDEnumEx;
  lpDDEnumEx = (LPDIRECTDRAWENUMERATEEX)
    GetProcAddress(h,"DirectDrawEnumerateExA");
 
  // If the function is there, call it to enumerate all display 
  // devices attached to the desktop, and any non-display DirectDraw
  // devices.
  if (lpDDEnumEx)
    lpDDEnumEx(DDEnumCallbackEx, NULL, 
               DDENUM_ATTACHEDSECONDARYDEVICES |
               DDENUM_NONDISPLAYDEVICES 
               );
  else {
    /*
     * We must be running on an old version of DirectDraw.
     * Therefore MultiMon isn't supported. Fall back on
     * DirectDrawEnumerate to enumerate standard devices on a 
     * single-monitor system.
     */
    BOOL (WINAPI *lpDDEnum)(LPDDENUMCALLBACK, LPVOID);
    
    lpDDEnum = (BOOL (WINAPI *)(LPDDENUMCALLBACK, LPVOID))
      GetProcAddress(h, "DirectDrawEnumerateA");
    if(lpDDEnum)
      lpDDEnum(DDEnumCallback,NULL);
    
    /* Note that it could be handy to let the OldCallback function
     * be a wrapper for a DDEnumCallbackEx. 
     * 
     * Such a function would look like:
     *    BOOL FAR PASCAL OldCallback(GUID FAR *lpGUID,
     *                                LPSTR pDesc,
     *                                LPSTR pName,
     *                                LPVOID pContext)
     *    {
     *         return Callback(lpGUID,pDesc,pName,pContext,NULL);
     *    }
     */
  }

  int selected = 0;

  if(gDriverCnt > 1) {
    VideoDriverSelect d(pWnd);

    INT_PTR selected = d.DoModal();

    if(selected == -1) {
#ifdef _AFXDLL
      AfxFreeLibrary( h );
#else
      FreeLibrary( h );
#endif
      
      return -1;
    }
  }

  HRESULT (WINAPI *DDrawCreateEx)(GUID *,LPVOID *,REFIID,IUnknown *);  
  DDrawCreateEx = (HRESULT (WINAPI *)(GUID *,LPVOID *,REFIID,IUnknown *))
    GetProcAddress(h, "DirectDrawCreateEx");

  LPDIRECTDRAW7 ddraw = NULL;
  if(DDrawCreateEx) {
    HRESULT hret = DDrawCreateEx(Drivers[selected].pGUID,
                                 (void **)&ddraw,
                                 IID_IDirectDraw7,
                                 NULL);
    if(hret != DD_OK) {
      systemMessage(0, "Error during DirectDrawCreateEx: %08x", hret);
#ifdef _AFXDLL
      AfxFreeLibrary( h );
#else
      FreeLibrary( h );
#endif
      return -1;
    }
  } else {
    // should not happen....
    systemMessage(0, "Error getting DirectDrawCreateEx");
#ifdef _AFXDLL
    AfxFreeLibrary( h );
#else
    FreeLibrary( h );
#endif
    return -1;
  }  
  
  VideoMode dlg(ddraw, pWnd);

  INT_PTR res = dlg.DoModal();

  if(res != -1) {
    *guid = Drivers[selected].pGUID;
  }
  ddraw->Release();
  ddraw = NULL;

  // If the library was loaded by calling LoadLibrary(),
  // then you must use FreeLibrary() to let go of it.
#ifdef _AFXDLL
  AfxFreeLibrary( h );
#else
  FreeLibrary( h );
#endif

  return (int)res;
}

/////////////////////////////////////////////////////////////////////////////
// VideoMode dialog


VideoMode::VideoMode(LPDIRECTDRAW7 pDraw, CWnd* pParent /*=NULL*/)
  : CDialog(VideoMode::IDD, pParent)
{
  //{{AFX_DATA_INIT(VideoMode)
  // NOTE: the ClassWizard will add member initialization here
  //}}AFX_DATA_INIT
  pDirectDraw = pDraw;
}


void VideoMode::DoDataExchange(CDataExchange* pDX)
{
  CDialog::DoDataExchange(pDX);
  //{{AFX_DATA_MAP(VideoMode)
  DDX_Control(pDX, IDC_MODES, m_modes);
  //}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(VideoMode, CDialog)
  //{{AFX_MSG_MAP(VideoMode)
  ON_LBN_SELCHANGE(IDC_MODES, OnSelchangeModes)
  ON_BN_CLICKED(ID_CANCEL, OnCancel)
  ON_BN_CLICKED(ID_OK, OnOk)
  //}}AFX_MSG_MAP
  END_MESSAGE_MAP()

  /////////////////////////////////////////////////////////////////////////////
// VideoMode message handlers

void VideoMode::OnSelchangeModes() 
{
  int item = m_modes.GetCurSel();

  GetDlgItem(ID_OK)->EnableWindow(item != -1);
}

void VideoMode::OnCancel() 
{
  EndDialog(-1);
}

void VideoMode::OnOk() 
{
  DWORD_PTR cur = m_modes.GetCurSel();

  if(cur != -1) {
    cur = m_modes.GetItemData((int)cur);
  }
  EndDialog((int)cur);
}

BOOL VideoMode::OnInitDialog() 
{
  CDialog::OnInitDialog();
  
  // check for available fullscreen modes
  pDirectDraw->EnumDisplayModes(
	  DDEDM_STANDARDVGAMODES,
	  NULL,
	  m_modes.m_hWnd,
	  addVideoMode);
  
  GetDlgItem(ID_OK)->EnableWindow(FALSE);      
  CenterWindow();

  return TRUE;  // return TRUE unless you set the focus to a control
                // EXCEPTION: OCX Property Pages should return FALSE
}

/////////////////////////////////////////////////////////////////////////////
// VideoDriverSelect dialog


VideoDriverSelect::VideoDriverSelect(CWnd* pParent /*=NULL*/)
  : CDialog(VideoDriverSelect::IDD, pParent)
{
  //{{AFX_DATA_INIT(VideoDriverSelect)
  // NOTE: the ClassWizard will add member initialization here
  //}}AFX_DATA_INIT
}


void VideoDriverSelect::DoDataExchange(CDataExchange* pDX)
{
  CDialog::DoDataExchange(pDX);
  //{{AFX_DATA_MAP(VideoDriverSelect)
  DDX_Control(pDX, IDC_DRIVERS, m_drivers);
  //}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(VideoDriverSelect, CDialog)
  //{{AFX_MSG_MAP(VideoDriverSelect)
  ON_BN_CLICKED(ID_OK, OnOk)
  ON_BN_CLICKED(ID_CANCEL, OnCancel)
	ON_LBN_SELCHANGE(IDC_DRIVERS, OnSelchangeDrivers)
	//}}AFX_MSG_MAP
  END_MESSAGE_MAP()

  /////////////////////////////////////////////////////////////////////////////
// VideoDriverSelect message handlers

void VideoDriverSelect::OnCancel() 
{
  EndDialog(-1);
}

void VideoDriverSelect::OnOk() 
{
  EndDialog(m_drivers.GetCurSel());
}

BOOL VideoDriverSelect::OnInitDialog() 
{
  CDialog::OnInitDialog();
  
  for(int i = 0; i < gDriverCnt; i++) {
    m_drivers.AddString(Drivers[i].szDescription);
  }
  
  GetDlgItem(ID_OK)->EnableWindow(FALSE);      
  CenterWindow();
  
  return TRUE;  // return TRUE unless you set the focus to a control
                // EXCEPTION: OCX Property Pages should return FALSE
}

void VideoDriverSelect::OnSelchangeDrivers() 
{
	GetDlgItem(ID_OK)->EnableWindow(m_drivers.GetCurSel() != -1);
}
