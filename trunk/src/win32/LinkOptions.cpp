#ifndef NO_LINK

#include "stdafx.h"
#include "vba.h"
#include "LinkOptions.h"
#include "../gba/GBALink.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

template <class T>
void DDX_CBData(CDataExchange* pDX, int nIDC, T& data)
{
    HWND hWndCtrl = pDX->PrepareCtrl(nIDC);
    if (pDX->m_bSaveAndValidate)
    {
        int index = static_cast<int>(::SendMessage(hWndCtrl, CB_GETCURSEL, 0, 0L));
        data = (index == CB_ERR ? NULL : static_cast<T>(::SendMessage(hWndCtrl, CB_GETITEMDATA, index, 0L)));
    }
    else
    {
        int count = static_cast<int>(::SendMessage(hWndCtrl, CB_GETCOUNT, 0, 0L));
        for (int i = 0; i != count; ++i)
        {
            if (static_cast<T>(::SendMessage(hWndCtrl, CB_GETITEMDATA, i, 0L)) == data)
            {
                ::SendMessage(hWndCtrl, CB_SETCURSEL, i, 0L);
                return;
            }
        }
        ::SendMessage(hWndCtrl, CB_SETCURSEL, -1, 0L);
    }
}

/////////////////////////////////////////////////////////////////////////////
// LinkOptions dialog

LinkOptions::LinkOptions(CWnd* pParent /*=NULL*/)
	: CDialog(LinkOptions::IDD, pParent)
{
	//{{AFX_DATA_INIT(LinkOptions)
	m_type = linkMode;
	m_server = FALSE;
	//}}AFX_DATA_INIT
}


void LinkOptions::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(LinkOptions)
	DDX_CBData(pDX, IDC_LINK_MODE, m_type);
	DDX_Control(pDX, IDC_LINKTIMEOUT, m_timeout);
	DDX_Check(pDX, IDC_AUTOLINK, linkAuto);
	DDX_Check(pDX, IDC_SSPEED, linkHacks);
	DDX_Control(pDX, IDC_LINK_MODE, m_mode);
	DDX_Control(pDX, IDC_SERVERIP, m_serverip);
	DDX_Radio(pDX, IDC_LINK2P, linkNumPlayers);
	DDX_Radio(pDX, IDC_LINK_CLIENT, m_server);
	//}}AFX_DATA_MAP
}

BOOL LinkOptions::OnInitDialog(){
	char timeout[6];

	CDialog::OnInitDialog();

	AddMode("Nothing (Disconnect)", LINK_DISCONNECTED);
	AddMode("Cable - Single Computer", LINK_CABLE_IPC);
	AddMode("Cable - Network", LINK_CABLE_SOCKET);
	AddMode("GameCube - Dolphin", LINK_GAMECUBE_DOLPHIN);
	AddMode("Wireless adapter - Single Computer", LINK_RFU_IPC);
	AddMode("Wireless adapter - Network", LINK_RFU_SOCKET);
	AddMode("Game Link (Game Boy) - Single Computer", LINK_GAMEBOY_IPC);
	AddMode("Game Link (Game Boy) - Network", LINK_GAMEBOY_SOCKET);

	sprintf(timeout, "%d", linkTimeout);

	m_timeout.LimitText(5);
	m_timeout.SetWindowText(timeout);

	m_serverip.SetWindowText(theApp.linkHostAddr);

	CheckDlgButton(IDC_AUTOLINK, linkAuto);

	CheckDlgButton(IDC_SSPEED, linkHacks);

	int player_radio = 0;
	switch (linkNumPlayers)
	{
		case 2:
			player_radio = IDC_LINK2P;
		case 3:
			player_radio = IDC_LINK3P;
		case 4:
			player_radio = IDC_LINK4P;
		default:
			player_radio = IDC_LINK2P;
	}

	CButton* pButton = (CButton*)GetDlgItem(player_radio);
	pButton->SetCheck(true);

	UpdateAvailability();

	UpdateData(FALSE);

	return TRUE;
}


 BEGIN_MESSAGE_MAP(LinkOptions, CDialog)
	//{{AFX_MSG_MAP(LinkOptions)
	ON_CBN_SELCHANGE(IDC_LINK_MODE, &LinkOptions::OnCbnSelchangeLinkMode)
	ON_BN_CLICKED(ID_OK, OnOk)
	ON_BN_CLICKED(ID_CANCEL, OnCancel)
	ON_BN_CLICKED(IDC_LINK_SERVER, &LinkOptions::OnBnClickedLinkServer)
	ON_BN_CLICKED(IDC_LINK_CLIENT, &LinkOptions::OnBnClickedLinkClient)
	//}}AFX_MSG_MAP

 END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// LinkOptions message handlers
/////////////////////////////////////////////////////////////////////////////
// LinkGeneral dialog


void LinkOptions::AddMode(LPCTSTR name, int value) {
	m_mode.AddString(name);
	int index = m_mode.FindStringExact(-1, name);
	m_mode.SetItemData(index, value);
}

void LinkOptions::OnOk()
{
	static const int length = 256; 
	int timeout;
	CString timeoutStr;
	CString host;
	CString title;
	CString addressMessage;
	
	UpdateData(TRUE);

	// Close any previous link
	CloseLink();

	m_serverip.GetWindowText(host);
	m_timeout.GetWindowText(timeoutStr);
	sscanf(timeoutStr, "%d", &timeout);
	SetLinkTimeout(timeout);

	LinkMode newMode = (LinkMode) m_type;

	if (newMode == LINK_DISCONNECTED) {
		linkTimeout = timeout;
		linkMode = LINK_DISCONNECTED;
		theApp.linkHostAddr = host;
		CDialog::OnOK();
		return;
	}

	bool needsServerHost = newMode == LINK_GAMECUBE_DOLPHIN || (newMode == LINK_CABLE_SOCKET && !m_server) || (newMode == LINK_RFU_SOCKET && !m_server);

	if (needsServerHost) {
		bool valid = SetLinkServerHost(host);
		if (!valid) {
			AfxMessageBox("You must enter a valid host name", MB_OK | MB_ICONSTOP);
			return;
		}
	}

	EnableSpeedHacks(linkHacks);
	EnableLinkServer(m_server, linkNumPlayers + 1);

	if (m_server) {
		char localhost[length];
		GetLinkServerHost(localhost, length);

		title = "Waiting for clients...";
		addressMessage.Format("Server IP address is: %s\n", localhost);
	} else {
		title = "Waiting for connection...";
		addressMessage.Format("Connecting to %s\n", host);
	}

	// Init link
	ConnectionState state = InitLink(newMode);

	// Display a progress dialog while the connection is establishing
	if (state == LINK_NEEDS_UPDATE) {
		ServerWait *dlg = new ServerWait();
		dlg->Create(ServerWait::IDD, this);
		dlg->m_plconn[1] = title;
		dlg->m_serveraddress = addressMessage;
		dlg->ShowWindow(SW_SHOW);

		while (state == LINK_NEEDS_UPDATE) {
			// Ask the core for updates
			char message[length];
			state = ConnectLinkUpdate(message, length);

			// Update the wait message
			if (strlen(message)) {
				dlg->m_plconn[1] = message;
			}

			// Step the progress bar and update dialog data
			dlg->m_prgctrl.StepIt();
			dlg->UpdateData(false);

			// Process Windows messages
			MSG msg;
			while (PeekMessage (&msg, 0, 0, 0, PM_NOREMOVE)) {
				AfxGetApp()->PumpMessage();
			}

			// Check whether the user has aborted
			if (dlg->m_userAborted) {
				state = LINK_ABORT;
			}
		}

		delete dlg;
	}

	// The user canceled the connection attempt
	if (state == LINK_ABORT) {
		CloseLink();
		return;
	}

	// Something failed during init
	if (state == LINK_ERROR) {
		AfxMessageBox("Error occurred.\nPlease try again.", MB_OK | MB_ICONSTOP);
		return;
	}

	linkTimeout = timeout;
	linkMode = GetLinkMode();
	theApp.linkHostAddr = host;

	CDialog::OnOK();
	return;
}

void LinkOptions::OnCancel()
{
	CDialog::OnCancel();
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

	m_userAborted = false;
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
	m_userAborted = true;
	ShowWindow(SW_HIDE);
}

void LinkOptions::OnCbnSelchangeLinkMode()
{
	UpdateData(TRUE);
	UpdateAvailability();
}

void LinkOptions::UpdateAvailability()
{
	bool isDisconnected = m_type == LINK_DISCONNECTED;
	bool isNetwork = (m_type == LINK_CABLE_SOCKET) || (m_type == LINK_RFU_SOCKET);
	bool canHaveServer = (m_type == LINK_CABLE_SOCKET && !m_server) || (m_type == LINK_RFU_SOCKET && !m_server) || m_type == LINK_GAMECUBE_DOLPHIN;
	bool hasHacks = m_type == LINK_CABLE_SOCKET;

	GetDlgItem(IDC_LINK_CLIENT)->EnableWindow(isNetwork);
	GetDlgItem(IDC_LINK_SERVER)->EnableWindow(isNetwork);
	GetDlgItem(IDC_SSPEED)->EnableWindow(isNetwork);

	m_serverip.EnableWindow(canHaveServer);
	m_timeout.EnableWindow(!isDisconnected);

	GetDlgItem(IDC_LINK2P)->EnableWindow(isNetwork && m_server);
	GetDlgItem(IDC_LINK3P)->EnableWindow(isNetwork && m_server);
	GetDlgItem(IDC_LINK4P)->EnableWindow(isNetwork && m_server);
}

void LinkOptions::OnBnClickedLinkServer()
{
	UpdateData(TRUE);
	UpdateAvailability();
}


void LinkOptions::OnBnClickedLinkClient()
{
	UpdateData(TRUE);
	UpdateAvailability();
}

#endif // NO_LINK
