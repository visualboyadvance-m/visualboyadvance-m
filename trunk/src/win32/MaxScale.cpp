#include "stdafx.h"
#include "vba.h"
#include "MaxScale.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// MaxScale dialog


MaxScale::MaxScale(CWnd* pParent /*=NULL*/)
	: CDialog(MaxScale::IDD, pParent)
{
	//{{AFX_DATA_INIT(MaxScale)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}


void MaxScale::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(MaxScale)
	DDX_Control(pDX, IDC_VALUE, m_value);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(MaxScale, CDialog)
	//{{AFX_MSG_MAP(MaxScale)
	ON_BN_CLICKED(ID_OK, OnOk)
	ON_BN_CLICKED(ID_CANCEL, OnCancel)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// MaxScale message handlers

void MaxScale::OnCancel()
{
  EndDialog(FALSE);
}

void MaxScale::OnOk()
{
  CString tmp;
  m_value.GetWindowText(tmp);
  maxScale = atoi(tmp);
  EndDialog(TRUE);
}

BOOL MaxScale::OnInitDialog()
{
	CDialog::OnInitDialog();

	CString temp;

  temp.Format("%d", maxScale);

  m_value.SetWindowText(temp);

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}
