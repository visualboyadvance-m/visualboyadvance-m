#include "stdafx.h"
#include "vba.h"
#include "ExportGSASnapshot.h"

#include "../gba/GBA.h"
#include "../NLS.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// ExportGSASnapshot dialog


ExportGSASnapshot::ExportGSASnapshot(CString filename, CString title, CWnd* pParent /*=NULL*/)
  : CDialog(ExportGSASnapshot::IDD, pParent)
{
  //{{AFX_DATA_INIT(ExportGSASnapshot)
  m_desc = _T("");
  m_notes = _T("");
  m_title = _T("");
  //}}AFX_DATA_INIT
  m_title = title;
  m_filename = filename;
  char date[100];
  char time[100];

  GetDateFormat(LOCALE_USER_DEFAULT,
                DATE_SHORTDATE,
                NULL,
                NULL,
                date,
                100);
  GetTimeFormat(LOCALE_USER_DEFAULT,
                0,
                NULL,
                NULL,
                time,
                100);
  m_desc.Format("%s %s", date, time);
}


void ExportGSASnapshot::DoDataExchange(CDataExchange* pDX)
{
  CDialog::DoDataExchange(pDX);
  //{{AFX_DATA_MAP(ExportGSASnapshot)
  DDX_Text(pDX, IDC_DESC, m_desc);
  DDV_MaxChars(pDX, m_desc, 100);
  DDX_Text(pDX, IDC_NOTES, m_notes);
  DDV_MaxChars(pDX, m_notes, 512);
  DDX_Text(pDX, IDC_TITLE, m_title);
  DDV_MaxChars(pDX, m_title, 100);
  //}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(ExportGSASnapshot, CDialog)
  //{{AFX_MSG_MAP(ExportGSASnapshot)
  ON_BN_CLICKED(ID_CANCEL, OnCancel)
  ON_BN_CLICKED(ID_OK, OnOk)
  //}}AFX_MSG_MAP
  END_MESSAGE_MAP()

  /////////////////////////////////////////////////////////////////////////////
// ExportGSASnapshot message handlers

BOOL ExportGSASnapshot::OnInitDialog()
{
  CDialog::OnInitDialog();
  CenterWindow();

  return TRUE;  // return TRUE unless you set the focus to a control
                // EXCEPTION: OCX Property Pages should return FALSE
}

void ExportGSASnapshot::OnCancel()
{
  EndDialog(FALSE);
}

void ExportGSASnapshot::OnOk()
{
  UpdateData(TRUE);

  bool result = CPUWriteGSASnapshot(m_filename, m_title, m_desc, m_notes);

  if(!result)
    systemMessage(MSG_ERROR_CREATING_FILE, "Error creating file %s",
                  m_filename);

  EndDialog(TRUE);
}
