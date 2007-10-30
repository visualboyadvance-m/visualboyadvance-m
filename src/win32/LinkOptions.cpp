// LinkOptions.cpp : implementation file
//

#include "stdafx.h"
#include "vba.h"
#include "LinkOptions.h"
#include "../Link.h"

extern int lspeed;
extern lserver ls;

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// LinkOptions dialog

CMyTabCtrl::CMyTabCtrl(){
	m_tabdialog[0] = new LinkGeneral;
	m_tabdialog[1] = new LinkServer;
	m_tabdialog[2] = new LinkClient;
}

CMyTabCtrl::~CMyTabCtrl()
{
	m_tabdialog[0]->DestroyWindow();
	m_tabdialog[1]->DestroyWindow();
	m_tabdialog[2]->DestroyWindow();
	
	delete m_tabdialog[0];
	delete m_tabdialog[1];
	delete m_tabdialog[2];
}

LinkOptions::LinkOptions(CWnd* pParent /*=NULL*/)
	: CDialog(LinkOptions::IDD, pParent)
{
	//{{AFX_DATA_INIT(LinkOptions)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}


void LinkOptions::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(LinkOptions)
	DDX_Control(pDX, IDC_TAB1, m_tabctrl);
	//}}AFX_DATA_MAP
}

BOOL LinkOptions::OnInitDialog(){
	TCITEM tabitem;
	char tabtext[3][8] = {"General", "Server", "Client"};
	int i;
		
	CDialog::OnInitDialog();

	tabitem.mask = TCIF_TEXT;
		
	for(i=0;i<3;i++){
		tabitem.pszText = tabtext[i];
		m_tabctrl.InsertItem(i, &tabitem);
	}	

	m_tabctrl.m_tabdialog[0]->Create(IDD_LINKTAB1, this);
	m_tabctrl.m_tabdialog[1]->Create(IDD_LINKTAB2, this);
	m_tabctrl.m_tabdialog[2]->Create(IDD_LINKTAB3, this);

	m_tabctrl.m_tabdialog[0]->ShowWindow(SW_SHOW);
	m_tabctrl.m_tabdialog[1]->ShowWindow(SW_HIDE);
	m_tabctrl.m_tabdialog[2]->ShowWindow(SW_HIDE);

	m_tabctrl.SetCurSel(0);
	m_tabctrl.OnSwitchTabs();

	return TRUE;
}


BEGIN_MESSAGE_MAP(LinkOptions, CDialog)
	//{{AFX_MSG_MAP(LinkOptions)
	ON_NOTIFY(TCN_SELCHANGE, IDC_TAB1, OnSelchangeTab1)
	ON_BN_CLICKED(ID_OK, OnOk)
	ON_BN_CLICKED(ID_CANCEL, OnCancel)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// LinkOptions message handlers
/////////////////////////////////////////////////////////////////////////////
// LinkGeneral dialog


LinkGeneral::LinkGeneral(CWnd* pParent /*=NULL*/)
	: CDialog(LinkGeneral::IDD, pParent)
{
	//{{AFX_DATA_INIT(LinkGeneral)
	//}}AFX_DATA_INIT
}

void LinkGeneral::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(LinkGeneral)
	DDX_Radio(pDX, IDC_LINK_SINGLE, m_type);
	DDX_Control(pDX, IDC_LINKTIMEOUT, m_timeout);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(LinkGeneral, CDialog)
	//{{AFX_MSG_MAP(LinkGeneral)
	ON_BN_CLICKED(IDC_LINK_SINGLE, OnRadio1)
	ON_BN_CLICKED(IDC_LINK_LAN, OnRadio2)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// LinkGeneral message handlers
/////////////////////////////////////////////////////////////////////////////
// LinkServer dialog


LinkServer::LinkServer(CWnd* pParent /*=NULL*/)
	: CDialog(LinkServer::IDD, pParent)
{
	//{{AFX_DATA_INIT(LinkServer)
	m_numplayers = -1;
	m_prottype = -1;
	m_speed = FALSE;
	//}}AFX_DATA_INIT
}


void LinkServer::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(LinkServer)
	DDX_Radio(pDX, IDC_LINK2P, m_numplayers);
	DDX_Radio(pDX, IDC_LINKTCP, m_prottype);
	DDX_Check(pDX, IDC_SSPEED, m_speed);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(LinkServer, CDialog)
	//{{AFX_MSG_MAP(LinkServer)
	ON_BN_CLICKED(IDC_SERVERSTART, OnServerStart)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// LinkServer message handlers

LinkClient::LinkClient(CWnd* pParent /*=NULL*/)
	: CDialog(LinkClient::IDD, pParent)
{
	//{{AFX_DATA_INIT(LinkClient)
	m_prottype = -1;
	m_hacks = -1;
	//}}AFX_DATA_INIT
}


void LinkClient::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(LinkClient)
	DDX_Control(pDX, IDC_SERVERIP, m_serverip);
	DDX_Radio(pDX, IDC_CLINKTCP, m_prottype);
	DDX_Radio(pDX, IDC_SPEEDOFF, m_hacks);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(LinkClient, CDialog)
	//{{AFX_MSG_MAP(LinkClient)
		ON_BN_CLICKED(IDC_LINKCONNECT, OnLinkConnect)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// LinkClient message handlers

BOOL LinkServer::OnInitDialog()
{
	CDialog::OnInitDialog();
	
	m_numplayers = lanlink.numgbas;
	m_prottype = lanlink.type;
	m_speed = lanlink.speed;
	
	UpdateData(FALSE);

	return TRUE;
}

void LinkOptions::OnSelchangeTab1(NMHDR* pNMHDR, LRESULT* pResult) 
{
	m_tabctrl.OnSwitchTabs();
	*pResult = 0;
}

void CMyTabCtrl::OnSwitchTabs(void)
{
	CRect tabRect, itemRect;
	int nX, nY, nXc, nYc, i;

	GetClientRect(&tabRect);
	GetItemRect(0, &itemRect);

	nX=itemRect.left+10;
	nY=itemRect.bottom+12;
	nXc=tabRect.right-itemRect.left-2;
	nYc=tabRect.bottom-nY+8;
	
	if(lanlink.active==0) SetCurSel(0);

	for(i=0;i<3;i++){
		if(i==GetCurSel()){
			m_tabdialog[i]->SetWindowPos(&wndTop, nX, nY, nXc, nYc, SWP_SHOWWINDOW);
		} else {
			m_tabdialog[i]->ShowWindow(SW_HIDE);
		}
	}
	return;
}


void LinkOptions::OnOk() 
{
	GetAllData((LinkGeneral*)m_tabctrl.m_tabdialog[0]);
	CDialog::OnOK();
	return;		
}

void LinkGeneral::OnRadio1() 
{
	m_type = 0;
	lanlink.active = 0;
}

void LinkGeneral::OnRadio2() 
{
	m_type = 1;
	lanlink.active = 1;
}

BOOL LinkGeneral::OnInitDialog(){

	char timeout[6];

	CDialog::OnInitDialog();
	
	m_timeout.LimitText(5);
	sprintf(timeout, "%d", linktimeout);
	m_timeout.SetWindowText(timeout);
	
	m_type = lanlink.active;

	UpdateData(FALSE);

	return TRUE;
}


void LinkOptions::OnCancel() 
{
	CDialog::OnCancel();
	return;
}

void LinkServer::OnServerStart() 
{
	int errorcode;
	ServerWait dlg;

	UpdateData(TRUE);
	
	lanlink.numgbas = m_numplayers+1;
	lanlink.type = m_prottype;
	lanlink.server = 1;
	lanlink.speed = m_speed==1 ? true : false;
	lspeed = lanlink.speed;

	if((errorcode=ls.Init(&dlg))!=0){
		char message[50];
		sprintf(message, "Error %d occured.\nPlease try again.", errorcode);
		MessageBox(message, "Error", MB_OK);
		return;
	}
		
	dlg.DoModal();
	
	return;
}

BOOL LinkClient::OnInitDialog()
{
	CDialog::OnInitDialog();
	
	m_prottype = lanlink.type;
	m_hacks = lanlink.speed;
	
	UpdateData(FALSE);

	return TRUE;
}

void LinkClient::OnLinkConnect()
{
	char ipaddress[31];
	int errorcode;
	ServerWait dlg;

	UpdateData(TRUE);

	lanlink.type = m_prottype;
	lanlink.server = 0;
	lanlink.speed = m_hacks==1 ? true : false;
	lspeed = lanlink.speed;

	m_serverip.GetWindowText(ipaddress, 30);

	if((errorcode=lc.Init(gethostbyname(ipaddress), &dlg))!=0){
		char message[50];
		sprintf(message, "Error %d occured.\nPlease try again.", errorcode);
		MessageBox(message, "Error", MB_OK);
		return;
	}
	dlg.DoModal();
	return;
}

void LinkOptions::GetAllData(LinkGeneral *src)
{
	char timeout[6];
	
	src->UpdateData(true);
	
	src->m_timeout.GetWindowText(timeout, 5);
	sscanf(timeout, "%d", &linktimeout);

	if(src->m_type==0){
		lanlink.speed = 0;
		lspeed = 0;
	}

	return;
}
/////////////////////////////////////////////////////////////////////////////
// ServerWait dialog


ServerWait::ServerWait(CWnd* pParent /*=NULL*/)
	: CDialog(ServerWait::IDD, pParent)
{
	//{{AFX_DATA_INIT(ServerWait)
	m_serveraddress = _T("");
	m_plconn[0] = _T("");
	m_plconn[1] = _T("");
	m_plconn[2] = _T("");
	//}}AFX_DATA_INIT
}


void ServerWait::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(ServerWait)
		DDX_Control(pDX, IDC_SERVERWAIT, m_prgctrl);
		DDX_Text(pDX, IDC_STATIC1, m_serveraddress);
		DDX_Text(pDX, IDC_STATIC2, m_plconn[0]);
		DDX_Text(pDX, IDC_STATIC3, m_plconn[1]);
		DDX_Text(pDX, IDC_STATIC4, m_plconn[2]);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(ServerWait, CDialog)
	//{{AFX_MSG_MAP(ServerWait)
		ON_BN_CLICKED(ID_CANCEL, OnCancel)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// ServerWait message handlers

void ServerWait::OnCancel()
{
	lanlink.terminate = true;
	CDialog::OnCancel();
}

BOOL LinkGeneral::PreTranslateMessage(MSG* pMsg) 
{
	if(pMsg->message==WM_KEYDOWN)
		if(pMsg->wParam==VK_RETURN||pMsg->wParam==VK_ESCAPE)
			pMsg->wParam = NULL;
	
	return CDialog::PreTranslateMessage(pMsg);
}

BOOL LinkClient::PreTranslateMessage(MSG* pMsg) 
{
	if(pMsg->message==WM_KEYDOWN)
		if(pMsg->wParam==VK_RETURN||pMsg->wParam==VK_ESCAPE)
			pMsg->wParam = NULL;
	
	return CDialog::PreTranslateMessage(pMsg);
}

BOOL LinkServer::PreTranslateMessage(MSG* pMsg) 
{
	if(pMsg->message==WM_KEYDOWN)
		if(pMsg->wParam==VK_RETURN||pMsg->wParam==VK_ESCAPE)
			pMsg->wParam = NULL;

	return CDialog::PreTranslateMessage(pMsg);
}
