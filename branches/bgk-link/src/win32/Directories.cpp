#include "stdafx.h"
#include "vba.h"
#include "Directories.h"
#include "Reg.h"
#include "WinResUtil.h"

#include <shlobj.h>
#include <shlwapi.h>
#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// Directories dialog

static int CALLBACK browseCallbackProc(HWND hWnd, UINT msg,
                                       LPARAM l, LPARAM data)
{
  char *buffer = (char *)data;
  switch(msg) {
  case BFFM_INITIALIZED:
    if(buffer[0])
      SendMessage(hWnd, BFFM_SETSELECTION, TRUE, (LPARAM)buffer);
    break;
  default:
    break;
  }
  return 0;
}

Directories::Directories(CWnd* pParent /*=NULL*/)
  : CDialog(Directories::IDD, pParent)
{
}


void Directories::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_SAVE_PATH, m_savePath);
	DDX_Control(pDX, IDC_ROM_PATH, m_romPath);
	DDX_Control(pDX, IDC_GBCROM_PATH, m_gbcromPath);
	DDX_Control(pDX, IDC_GBROM_PATH, m_gbromPath);
	DDX_Control(pDX, IDC_CAPTURE_PATH, m_capturePath);
	DDX_Control(pDX, IDC_BATTERY_PATH, m_batteryPath);
}


BEGIN_MESSAGE_MAP(Directories, CDialog)
  ON_BN_CLICKED(IDC_BATTERY_DIR, OnBatteryDir)
  ON_BN_CLICKED(IDC_CAPTURE_DIR, OnCaptureDir)
  ON_BN_CLICKED(IDC_GBROM_DIR, OnGbromDir)
  ON_BN_CLICKED(IDC_ROM_DIR, OnRomDir)
  ON_BN_CLICKED(IDC_SAVE_DIR, OnSaveDir)
  ON_BN_CLICKED(IDC_GBCROM_DIR, OnGbcromDir)
END_MESSAGE_MAP()

  /////////////////////////////////////////////////////////////////////////////
// Directories message handlers

BOOL Directories::OnInitDialog()
{
	CDialog::OnInitDialog();
	CString p;

	p = regQueryStringValue("romdir", NULL);
	if(!p.IsEmpty())
		GetDlgItem(IDC_ROM_PATH)->SetWindowText(p);

	p = regQueryStringValue("gbcromdir", NULL);
	if(!p.IsEmpty())
		GetDlgItem(IDC_GBCROM_PATH)->SetWindowText(p);

	p = regQueryStringValue("gbromdir", NULL);
	if(!p.IsEmpty())
		GetDlgItem(IDC_GBROM_PATH)->SetWindowText(p);

	p = regQueryStringValue("batteryDir", NULL);
	if(!p.IsEmpty())
		GetDlgItem(IDC_BATTERY_PATH)->SetWindowText( p);

	p = regQueryStringValue("saveDir", NULL);
	if(!p.IsEmpty())
		GetDlgItem(IDC_SAVE_PATH)->SetWindowText(p);

	p = regQueryStringValue("captureDir", NULL);
	if(!p.IsEmpty())
		GetDlgItem(IDC_CAPTURE_PATH)->SetWindowText(p);

	return TRUE;
	// return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

void Directories::OnBatteryDir()
{
  m_batteryPath.GetWindowText(initialFolderDir);
  CString p = browseForDir(winResLoadString(IDS_SELECT_BATTERY_DIR));
  if(!p.IsEmpty())
    m_batteryPath.SetWindowText(p);
}

void Directories::OnCaptureDir()
{
  m_capturePath.GetWindowText(initialFolderDir);
  CString p = browseForDir(winResLoadString(IDS_SELECT_CAPTURE_DIR));
  if(!p.IsEmpty())
    m_capturePath.SetWindowText(p);
}

void Directories::OnGbromDir()
{
  m_gbromPath.GetWindowText(initialFolderDir);
  CString p = browseForDir(winResLoadString(IDS_SELECT_ROM_DIR));
  if(!p.IsEmpty())
    m_gbromPath.SetWindowText(p);
}

void Directories::OnGbcromDir()
{
  m_gbcromPath.GetWindowText(initialFolderDir);
  CString p = browseForDir(winResLoadString(IDS_SELECT_ROM_DIR));
  if(!p.IsEmpty())
    m_gbcromPath.SetWindowText(p);
}

void Directories::OnRomDir()
{
  m_romPath.GetWindowText(initialFolderDir);
  CString p = browseForDir(winResLoadString(IDS_SELECT_ROM_DIR));
  if(!p.IsEmpty())
    m_romPath.SetWindowText(p);
}

void Directories::OnSaveDir()
{
  m_savePath.GetWindowText(initialFolderDir);
  CString p = browseForDir(winResLoadString(IDS_SELECT_SAVE_DIR));
  if(!p.IsEmpty())
    m_savePath.SetWindowText(p);
}

void Directories::OnCancel()
{
  EndDialog(FALSE);
}

void Directories::OnOK()
{
	CDialog::OnOK();

	char baseDir[MAX_PATH+1];
	char temp[MAX_PATH+1];
	GetModuleFileName( NULL, baseDir, MAX_PATH );
	baseDir[MAX_PATH] = '\0'; // for security reasons
	PathRemoveFileSpec( baseDir ); // removes the trailing file name and backslash


	CString buffer;

	m_romPath.GetWindowText(buffer);
	if( !buffer.IsEmpty() )
		regSetStringValue( "romdir", buffer );
	if( buffer[0] == '.' ) {
		strcpy( temp, baseDir );
		strcat( temp, "\\" );
		strcat( temp, buffer );
		buffer = temp;
	}
	if( !directoryDoesExist( buffer ) )
		SHCreateDirectoryEx( NULL, buffer, NULL );

	m_gbcromPath.GetWindowText(buffer);
	if( !buffer.IsEmpty() )
		regSetStringValue( "gbcromdir", buffer );
	if( buffer[0] == '.' ) {
		strcpy( temp, baseDir );
		strcat( temp, "\\" );
		strcat( temp, buffer );
		buffer = temp;
	}
	if( !directoryDoesExist( buffer ) )
		SHCreateDirectoryEx( NULL, buffer, NULL );

	m_gbromPath.GetWindowText(buffer);
	if( !buffer.IsEmpty() )
		regSetStringValue( "gbromdir", buffer );
	if( buffer[0] == '.' ) {
		strcpy( temp, baseDir );
		strcat( temp, "\\" );
		strcat( temp, buffer );
		buffer = temp;
	}
	if( !directoryDoesExist( buffer ) )
		SHCreateDirectoryEx( NULL, buffer, NULL );

	m_batteryPath.GetWindowText(buffer);
	if( !buffer.IsEmpty() )
		regSetStringValue( "batteryDir", buffer );
	if( buffer[0] == '.' ) {
		strcpy( temp, baseDir );
		strcat( temp, "\\" );
		strcat( temp, buffer );
		buffer = temp;
	}
	if( !directoryDoesExist( buffer ) )
		SHCreateDirectoryEx( NULL, buffer, NULL );

	m_savePath.GetWindowText(buffer);
	if( !buffer.IsEmpty() )
		regSetStringValue( "saveDir", buffer );
	if( buffer[0] == '.' ) {
		strcpy( temp, baseDir );
		strcat( temp, "\\" );
		strcat( temp, buffer );
		buffer = temp;
	}
	if( !directoryDoesExist( buffer ) )
		SHCreateDirectoryEx( NULL, buffer, NULL );

	m_capturePath.GetWindowText(buffer);
	if( !buffer.IsEmpty() )
		regSetStringValue( "captureDir", buffer );
	if( buffer[0] == '.' ) {
		strcpy( temp, baseDir );
		strcat( temp, "\\" );
		strcat( temp, buffer );
		buffer = temp;
	}
	if( !directoryDoesExist( buffer ) )
		SHCreateDirectoryEx( NULL, buffer, NULL );

	EndDialog(TRUE);
}

CString Directories::browseForDir(CString title)
{
  static char buffer[1024];
  LPMALLOC pMalloc;
  LPITEMIDLIST pidl;

  CString res;

  if(SUCCEEDED(SHGetMalloc(&pMalloc))) {
    BROWSEINFO bi;
    ZeroMemory(&bi, sizeof(bi));
    bi.hwndOwner = m_hWnd;
    bi.lpszTitle = title;
    bi.pidlRoot = 0;
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_USENEWUI;
    bi.lpfn = browseCallbackProc;
    bi.lParam = (LPARAM)(LPCTSTR)initialFolderDir;

    pidl = SHBrowseForFolder(&bi);

    if(pidl) {
      if(SHGetPathFromIDList(pidl, buffer)) {
        res = buffer;
      }
      pMalloc->Free(pidl);
      pMalloc->Release();
    }
  }
  return res;
}

// returns true if the directory does exist
bool Directories::directoryDoesExist(const char *directory)
{
	HANDLE hDir;
	hDir = CreateFile(
		directory,
		GENERIC_READ,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		NULL,
		OPEN_EXISTING,
		FILE_FLAG_BACKUP_SEMANTICS,
		NULL );
	bool retval = (hDir == INVALID_HANDLE_VALUE) ? false : true;
	CloseHandle( hDir );
	return retval;
}
