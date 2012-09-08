#ifndef NO_LINK

#include "stdafx.h"
#include "vba.h"
#include "LinkOptions.h"
#include "../gba/GBALink.h"

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
	//}}AFX_DATA_MAP
}

BOOL LinkOptions::OnInitDialog(){
	TCITEM tabitem;
	char tabtext[3][8] = {"General", "Server", "Client"};
	int i;

	CDialog::OnInitDialog();

	m_tabctrl.SubclassDlgItem(IDC_TAB1, this);

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


 BOOL LinkOptions::PreTranslateMessage(MSG* pMsg)
 {
   return m_tabctrl.TranslatePropSheetMsg(pMsg) ? TRUE :
       CDialog::PreTranslateMessage(pMsg);
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

	m_numplayers = lanlink.numslaves;
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

IMPLEMENT_DYNAMIC(CMyTabCtrl, CTabCtrl)
BEGIN_MESSAGE_MAP(CMyTabCtrl, CTabCtrl)
	ON_NOTIFY_REFLECT(TCN_SELCHANGING, OnSelChanging)
END_MESSAGE_MAP()

BOOL CMyTabCtrl::SubclassDlgItem(UINT nID, CWnd* pParent)
{
	if (!CTabCtrl::SubclassDlgItem(nID, pParent))
		return FALSE;

	ModifyStyle(0, TCS_OWNERDRAWFIXED);

	// If first tab is disabled, go to next enabled tab
	if (!IsTabEnabled(0)) {
		int iTab = NextEnabledTab(0, TRUE);
		SetActiveTab(iTab);
	}
	return TRUE;
}

BOOL CMyTabCtrl::IsTabEnabled(int iTab)
{
	if (!lanlink.active && iTab > 0)
		return false;
	return true;
}

//////////////////
// Draw the tab: mimic SysTabControl32, except use gray if tab is disabled
//
void CMyTabCtrl::DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct)
{
	DRAWITEMSTRUCT& ds = *lpDrawItemStruct;

	int iItem = ds.itemID;

	// Get tab item info
	char text[128];
	TCITEM tci;
	tci.mask = TCIF_TEXT;
	tci.pszText = text;
	tci.cchTextMax = sizeof(text);
	GetItem(iItem, &tci);

	// use draw item DC
	CDC dc;
	dc.Attach(ds.hDC);

	// calculate text rectangle and color
	CRect rc = ds.rcItem;
	rc += CPoint(1,4);						 // ?? by trial and error

	// draw the text
	OnDrawText(dc, rc, text, !IsTabEnabled(iItem));

	dc.Detach();
}

//////////////////
// Draw tab text. You can override to use different color/font.
//
void CMyTabCtrl::OnDrawText(CDC& dc, CRect rc,
	CString sText, BOOL bDisabled)
{
	dc.SetTextColor(GetSysColor(bDisabled ? COLOR_3DHILIGHT : COLOR_BTNTEXT));
	dc.DrawText(sText, &rc, DT_CENTER|DT_VCENTER);

	if (bDisabled) {
		// disabled: draw again shifted northwest for shadow effect
		rc += CPoint(-1,-1);
		dc.SetTextColor(GetSysColor(COLOR_GRAYTEXT));
		dc.DrawText(sText, &rc, DT_CENTER|DT_VCENTER);
	}
}

//////////////////
// Selection is changing: disallow if tab is disabled
//
void CMyTabCtrl::OnSelChanging(NMHDR* pnmh, LRESULT* pRes)
{
	TRACE("CMyTabCtrl::OnSelChanging\n");

	// Figure out index of new tab we are about to go to, as opposed
	// to the current one we're at. Believe it or not, Windows doesn't
	// pass this info
	//
	TC_HITTESTINFO htinfo;
	GetCursorPos(&htinfo.pt);
	ScreenToClient(&htinfo.pt);
	int iNewTab = HitTest(&htinfo);

	if (iNewTab >= 0 && !IsTabEnabled(iNewTab))
		*pRes = TRUE; // tab disabled: prevent selection
}

//////////////////
// Trap arrow-left key to skip disabled tabs.
// This is the only way to know where we're coming from--ie from
// arrow-left (prev) or arrow-right (next).
//
BOOL CMyTabCtrl::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg->message == WM_KEYDOWN &&
		(pMsg->wParam == VK_LEFT || pMsg->wParam == VK_RIGHT)) {

		int iNewTab = (pMsg->wParam == VK_LEFT) ?
			PrevEnabledTab(GetCurSel(), FALSE) :
			NextEnabledTab(GetCurSel(), FALSE);
		if (iNewTab >= 0)
			SetActiveTab(iNewTab);
		return TRUE;
	}
	return CTabCtrl::PreTranslateMessage(pMsg);
}

////////////////
// Translate parent property sheet message. Translates Control-Tab and
// Control-Shift-Tab keys. These are normally handled by the property
// sheet, so you must call this function from your prop sheet's
// PreTranslateMessage function.
//
BOOL CMyTabCtrl::TranslatePropSheetMsg(MSG* pMsg)
{
	WPARAM key = pMsg->wParam;
	if (pMsg->message == WM_KEYDOWN && GetAsyncKeyState(VK_CONTROL) < 0 &&
		(key == VK_TAB || key == VK_PRIOR || key == VK_NEXT)) {

		int iNewTab = (key==VK_PRIOR || GetAsyncKeyState(VK_SHIFT) < 0) ?
			PrevEnabledTab(GetCurSel(), TRUE) :
			NextEnabledTab(GetCurSel(), TRUE);
		if (iNewTab >= 0)
			SetActiveTab(iNewTab);
		return TRUE;
	}
	return FALSE;
}

//////////////////
// Helper to set the active page, when moving backwards (left-arrow and
// Control-Shift-Tab). Must simulate Windows messages to tell parent I
// am changing the tab; SetCurSel does not do this!!
//
// In normal operation, this fn will always succeed, because I don't call it
// unless I already know IsTabEnabled() = TRUE; but if you call SetActiveTab
// with a random value, it could fail.
//
BOOL CMyTabCtrl::SetActiveTab(UINT iNewTab)
{
	TRACE("CMyTabCtrl::SetActiveTab\n");

	// send the parent TCN_SELCHANGING
	NMHDR nmh;
	nmh.hwndFrom = m_hWnd;
	nmh.idFrom = GetDlgCtrlID();
	nmh.code = TCN_SELCHANGING;

	if (GetParent()->SendMessage(WM_NOTIFY, nmh.idFrom, (LPARAM)&nmh) >=0) {
		// OK to change: set the new tab
		SetCurSel(iNewTab);

		// send parent TCN_SELCHANGE
		nmh.code = TCN_SELCHANGE;
		GetParent()->SendMessage(WM_NOTIFY, nmh.idFrom, (LPARAM)&nmh);
		return TRUE;
	}
	return FALSE;
}

/////////////////
// Return the index of the next enabled tab after a given index, or -1 if none
// (0 = first tab).
// If bWrap is TRUE, wrap from beginning to end; otherwise stop at zero.
//
int CMyTabCtrl::NextEnabledTab(int iCurrentTab, BOOL bWrap)
{
	int nTabs = GetItemCount();
	for (int iTab = iCurrentTab+1; iTab != iCurrentTab; iTab++) {
		if (iTab >= nTabs) {
			if (!bWrap)
				return -1;
			iTab = 0;
		}
		if (IsTabEnabled(iTab)) {
			return iTab;
		}
	}
	return -1;
}

/////////////////
// Return the index of the previous enabled tab before a given index, or -1.
// (0 = first tab).
// If bWrap is TRUE, wrap from beginning to end; otherwise stop at zero.
//
int CMyTabCtrl::PrevEnabledTab(int iCurrentTab, BOOL bWrap)
{
	for (int iTab = iCurrentTab-1; iTab != iCurrentTab; iTab--) {
		if (iTab < 0) {
			if (!bWrap)
				return -1;
			iTab = GetItemCount() - 1;
		}
		if (IsTabEnabled(iTab)) {
			return iTab;
		}
	}
	return -1;
}


void CMyTabCtrl::OnSwitchTabs(void)
{
	CRect clientRect, wndRect;
	int i;

	GetClientRect(clientRect);
	AdjustRect(FALSE, clientRect);
	GetWindowRect(wndRect);
	GetParent()->ScreenToClient(wndRect);
	clientRect.OffsetRect(wndRect.left, wndRect.top);

	if(lanlink.active==0)
		SetCurSel(0);

	for(i=0;i<3;i++){
		if(i==GetCurSel()){
			m_tabdialog[i]->SetWindowPos(&wndTop, clientRect.left, clientRect.top, clientRect.Width(), clientRect.Height(), SWP_SHOWWINDOW);
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
	GetParent()->Invalidate();
}

void LinkGeneral::OnRadio2()
{
	m_type = 1;
	lanlink.active = 1;
	GetParent()->Invalidate();
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

class Win32ServerInfoDisplay : public ServerInfoDisplay
{
public:
    Win32ServerInfoDisplay(ServerWait *_dlg)
	{
		dlg = _dlg;
	}

	~Win32ServerInfoDisplay()
	{
		if (dlg)
		{
			// not connected
			MessageBox(NULL, "Failed to connect.", "Link", MB_OK);
			dlg->SendMessage(WM_CLOSE, 0, 0);
		}

		delete dlg;
		dlg = NULL;
	}

    void ShowServerIP(const sf::IPAddress& addr)
	{
		dlg->m_serveraddress.Format("Server IP address is: %s", addr.ToString());
	}

	void ShowConnect(const int player)
	{
		dlg->m_plconn[0].Format("Player %d connected", player);
		dlg->UpdateData(false);
	}

	void Ping()
	{
		dlg->m_prgctrl.StepIt();
	}

	void Connected()
	{
		MessageBox(NULL, "All players connected", "Link", MB_OK);
		dlg->SendMessage(WM_CLOSE, 0, 0);
		delete dlg;
		dlg = NULL;
    }

private:
    ServerWait *dlg;
};

void LinkServer::OnServerStart()
{
	UpdateData(TRUE);

	lanlink.numslaves = m_numplayers+1;
	lanlink.type = m_prottype;
	lanlink.server = true;
	lanlink.speed = m_speed==1 ? true : false;
	sf::IPAddress addr;

	// These must be created on the heap - referenced from the connection thread
	ServerWait *dlg = new ServerWait();
	dlg->Create(IDD_SERVERWAIT, this);
	dlg->ShowWindow(SW_SHOW);

	// Owns the ServerWait*
	Win32ServerInfoDisplay *dlginfo = new Win32ServerInfoDisplay(dlg);

	// ls thread will own the dlginfo
	if (!ls.Init(dlginfo))
	{
		// Thread didn't get created
		delete dlginfo;
		MessageBox("Error occurred.\nPlease try again.", "Error", MB_OK);
	}

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

class Win32ClientInfoDisplay : public ClientInfoDisplay
{
public:
    Win32ClientInfoDisplay(ServerWait *_dlg)
	{
		dlg = _dlg;
	}

	~Win32ClientInfoDisplay()
	{
		if (dlg)
		{
			// not connected
			MessageBox(NULL, "Failed to connect.", "Link", MB_OK);
			dlg->SendMessage(WM_CLOSE, 0, 0);
		}

		delete dlg;
		dlg = NULL;
	}

    void ConnectStart(const sf::IPAddress& addr)
	{
	    dlg->SetWindowText("Connecting...");
    }

    void ShowConnect(const int player, const int togo)
	{
	    dlg->m_serveraddress.Format("Connected as #%d", player);
	    if (togo)
			dlg->m_plconn[0].Format("Waiting for %d players to join", togo);
	    else
			dlg->m_plconn[0].Format("All players joined.");
    }

    void Ping()
	{
		dlg->m_prgctrl.StepIt();
	}

    void Connected()
	{
		MessageBox(NULL, "Connected.", "Link", MB_OK);
		dlg->SendMessage(WM_CLOSE, 0, 0);
		delete dlg;
		dlg = NULL;
    }

private:
    ServerWait *dlg;
};

void LinkClient::OnLinkConnect()
{
	char ipaddress[31];

	UpdateData(TRUE);

	lanlink.type = m_prottype;
	lanlink.server = false;
	lanlink.speed = m_hacks==1 ? true : false;

	m_serverip.GetWindowText(ipaddress, 30);

	// These must be created on the heap - referenced from the connection thread
	ServerWait *dlg = new ServerWait();
	dlg->Create(IDD_SERVERWAIT, this);
	dlg->ShowWindow(SW_SHOW);

	// Owns the ServerWait*
	Win32ClientInfoDisplay *dlginfo = new Win32ClientInfoDisplay(dlg);

	// lc thread will own the dlginfo
	if (!lc.Init(sf::IPAddress(std::string(ipaddress)), dlginfo))
	{
		// Thread didn't get created
		delete dlginfo;
		MessageBox("Error occurred.\nPlease try again.", "Error", MB_OK);
	}

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

#endif // NO_LINK
