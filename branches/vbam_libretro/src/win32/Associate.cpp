#include "stdafx.h"
#include "vba.h"
#include "Associate.h"
#include "Reg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// Associate dialog


Associate::Associate(CWnd* pParent /*=NULL*/)
  : CDialog(Associate::IDD, pParent)
{
  //{{AFX_DATA_INIT(Associate)
  m_agb = FALSE;
  m_bin = FALSE;
  m_cgb = FALSE;
  m_dmg = FALSE;
  m_gb = FALSE;
  m_gba = FALSE;
  m_gbc = FALSE;
  m_sgb = FALSE;
  //}}AFX_DATA_INIT
}


void Associate::DoDataExchange(CDataExchange* pDX)
{
  CDialog::DoDataExchange(pDX);
  //{{AFX_DATA_MAP(Associate)
  DDX_Check(pDX, IDC_AGB, m_agb);
  DDX_Check(pDX, IDC_BIN, m_bin);
  DDX_Check(pDX, IDC_CGB, m_cgb);
  DDX_Check(pDX, IDC_DMG, m_dmg);
  DDX_Check(pDX, IDC_GB, m_gb);
  DDX_Check(pDX, IDC_GBA, m_gba);
  DDX_Check(pDX, IDC_GBC, m_gbc);
  DDX_Check(pDX, IDC_SGB, m_sgb);
  //}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(Associate, CDialog)
  //{{AFX_MSG_MAP(Associate)
  ON_BN_CLICKED(ID_CANCEL, OnCancel)
  ON_BN_CLICKED(ID_OK, OnOk)
  //}}AFX_MSG_MAP
END_MESSAGE_MAP()

  /////////////////////////////////////////////////////////////////////////////
// Associate message handlers

BOOL Associate::OnInitDialog()
{
  CDialog::OnInitDialog();

  CenterWindow();

  return TRUE;  // return TRUE unless you set the focus to a control
                // EXCEPTION: OCX Property Pages should return FALSE
}

void Associate::OnCancel()
{
  EndDialog(FALSE);
}

void Associate::OnOk()
{
  UpdateData();

  int mask = 0;
  if(m_gb)
    mask |= 1;
  if(m_dmg)
    mask |= 2;
  if(m_sgb)
    mask |= 4;
  if(m_cgb)
    mask |= 8;
  if(m_gbc)
    mask |= 16;
  if(m_gba)
    mask |= 32;
  if(m_agb)
    mask |= 64;
  if(m_bin)
    mask |= 128;
  if(mask) {
    char applicationPath[2048];
    CString commandPath;
    LPCTSTR types[] = { "*.dmg", ".gb", ".sgb", ".cgb", ".gbc", ".gba", ".agb", ".bin" };
    GetModuleFileName(NULL, applicationPath, 2048);
    commandPath.Format("\"%s\" \"%%1\"", applicationPath);
    regAssociateType("VisualBoyAdvance.Binary",
                     "Binary",
                     commandPath,
					 "%SystemRoot%\\system32\\SHELL32.dll,-13");

    for(int i = 0; i < 8; i++) {
      if(mask & (1<<i)) {
        regCreateFileType(types[i],"VisualBoyAdvance.Binary");
      }
    }
  }
  EndDialog(TRUE);
}
