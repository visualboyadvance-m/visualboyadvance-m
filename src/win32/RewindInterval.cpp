#include "stdafx.h"
#include "vba.h"
#include "RewindInterval.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// RewindInterval dialog


RewindInterval::RewindInterval(int interval, CWnd* pParent /*=NULL*/)
  : CDialog(RewindInterval::IDD, pParent)
{
  //{{AFX_DATA_INIT(RewindInterval)
  // NOTE: the ClassWizard will add member initialization here
  //}}AFX_DATA_INIT
  this->interval = interval;
}


void RewindInterval::DoDataExchange(CDataExchange* pDX)
{
  CDialog::DoDataExchange(pDX);
  //{{AFX_DATA_MAP(RewindInterval)
  DDX_Control(pDX, IDC_INTERVAL, m_interval);
  //}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(RewindInterval, CDialog)
  //{{AFX_MSG_MAP(RewindInterval)
  ON_BN_CLICKED(ID_CANCEL, OnCancel)
  ON_BN_CLICKED(ID_OK, OnOk)
  //}}AFX_MSG_MAP
  END_MESSAGE_MAP()

  /////////////////////////////////////////////////////////////////////////////
// RewindInterval message handlers

void RewindInterval::OnCancel()
{
  EndDialog(-1);
}

void RewindInterval::OnOk()
{
  CString buffer;

  m_interval.GetWindowText(buffer);

  int v = atoi(buffer);

  if(v >= 0 && v <= 600) {
    EndDialog(v);
  } else
    systemMessage(IDS_INVALID_INTERVAL_VALUE,
                  "Invalid rewind interval value. Please enter a number "
                  "between 0 and 600 seconds");
}

BOOL RewindInterval::OnInitDialog()
{
  CDialog::OnInitDialog();

  m_interval.LimitText(3);

  CString buffer;
  buffer.Format("%d", interval);
  m_interval.SetWindowText(buffer);

  CenterWindow();

  return TRUE;  // return TRUE unless you set the focus to a control
                // EXCEPTION: OCX Property Pages should return FALSE
}
