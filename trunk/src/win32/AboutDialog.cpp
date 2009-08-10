#include "stdafx.h"
#include "AboutDialog.h"
#include "../AutoBuild.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// AboutDialog dialog


AboutDialog::AboutDialog(CWnd* pParent /*=NULL*/)
  : CDialog(AboutDialog::IDD, pParent)
{
	m_version = _T(VBA_VERSION_STRING);
	m_date = _T(__DATE__);
}


void AboutDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(AboutDialog)
	DDX_Text(pDX, IDC_VERSION, m_version);
	DDX_Control(pDX, IDC_URL, m_link);
	//}}AFX_DATA_MAP
	DDX_Text(pDX, IDC_DATE, m_date);
}


BEGIN_MESSAGE_MAP(AboutDialog, CDialog)
  //{{AFX_MSG_MAP(AboutDialog)
  //}}AFX_MSG_MAP
  END_MESSAGE_MAP()

  /////////////////////////////////////////////////////////////////////////////
// AboutDialog message handlers

BOOL AboutDialog::OnInitDialog()
{
  CDialog::OnInitDialog();

  CWnd *p = GetDlgItem(IDC_TRANSLATOR_URL);
  if(p) {
    m_translator.SubclassDlgItem(IDC_TRANSLATOR_URL, this);
  }

  m_link.SetWindowText("http://vba-m.com");

  return TRUE;  // return TRUE unless you set the focus to a control
  // EXCEPTION: OCX Property Pages should return FALSE
}
