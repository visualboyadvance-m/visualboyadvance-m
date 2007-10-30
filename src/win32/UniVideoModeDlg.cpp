// source\win32\UniVideoModeDlg.cpp : Implementierungsdatei
//

#include ".\univideomodedlg.h"

// UniVideoModeDlg-Dialogfeld

IMPLEMENT_DYNAMIC(UniVideoModeDlg, CDialog)
UniVideoModeDlg::UniVideoModeDlg(CWnd* pParent /*=NULL*/, int *width, int *height, int *BPP, int *freq, int *adapt)
	: CDialog(UniVideoModeDlg::IDD, pParent)
{
	WidthList = 0;
	HeightList = 0;
	BPPList = 0;
	FreqList = 0;
	iDisplayDevice = 0;
	SelectedWidth = width;
	SelectedHeight = height;
	SelectedBPP = BPP;
	SelectedFreq = freq;
	SelectedAdapter = adapt;
	pD3D = NULL;
	d3dDLL = NULL;
	nAdapters = 1;
}

UniVideoModeDlg::~UniVideoModeDlg()
{
	if (WidthList) delete [] WidthList;
	if (HeightList) delete [] HeightList;
	if (BPPList) delete [] BPPList;
	if (FreqList) delete [] FreqList;
}

void UniVideoModeDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(UniVideoModeDlg, CDialog)
	ON_WM_CREATE()
	ON_BN_CLICKED(IDOK, OnBnClickedOk)
	ON_BN_CLICKED(IDCANCEL, OnBnClickedCancel)
//	ON_STN_CLICKED(IDC_DISPLAYDEVICE, OnStnClickedDisplaydevice)
//ON_STN_CLICKED(IDC_DISPLAYDEVICE, OnStnClickedDisplaydevice)
ON_STN_CLICKED(IDC_DISPLAYDEVICE, OnStnClickedDisplaydevice)
ON_BN_CLICKED(IDC_BUTTON_MAXSCALE, OnBnClickedButtonMaxscale)
ON_BN_CLICKED(IDC_CHECK_STRETCHTOFIT, OnBnClickedCheckStretchtofit)
END_MESSAGE_MAP()


// UniVideoModeDlg-Meldungshandler

int UniVideoModeDlg::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CDialog::OnCreate(lpCreateStruct) == -1)
		return -1;

	// TODO:  Fügen Sie Ihren spezialisierten Erstellcode hier ein.

	return 0;
}


BOOL UniVideoModeDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
	CButton* check_stretchtofit = (CButton*)GetDlgItem(IDC_CHECK_STRETCHTOFIT);
	check_stretchtofit->SetCheck(theApp.fullScreenStretch ? BST_CHECKED : BST_UNCHECKED);

	CStatic* apiname = (CStatic*)GetDlgItem(IDC_APINAME);
	CStatic* devicename = (CStatic*)GetDlgItem(IDC_DISPLAYDEVICE);
	CListBox* listmodes = (CListBox*)GetDlgItem(IDC_LISTMODES);
	CString temp;
	DWORD w, h, b, f;
	UINT nModes16, nModes32;

	switch (theApp.display->getType())
	{
	case GDI:
		apiname->SetWindowText("Windows GDI");
		DISPLAY_DEVICE dd;
		dd.cb = sizeof(dd);
		EnumDisplayDevices(NULL, iDisplayDevice, &dd, 0);
		devicename->SetWindowText(dd.DeviceString);
		DEVMODE dm;
		dm.dmSize = sizeof(DEVMODE);
		dm.dmDriverExtra = 0;
		DWORD maxMode;
		for (DWORD i=0; 0 != EnumDisplaySettings(dd.DeviceName, i, &dm); i++) {}
		maxMode = i-1;
		listmodes->InitStorage(i, 25);
		if (WidthList!=0) delete [] WidthList;
		if (HeightList!=0) delete [] HeightList;
		if (BPPList!=0) delete [] BPPList;
		if (FreqList!=0) delete [] FreqList;
		WidthList = new DWORD[i];
		HeightList = new DWORD[i];
		BPPList = new DWORD[i];
		FreqList = new DWORD[i];
		listmodes->ResetContent();
		for (i=0; i<maxMode+1; i++)
		{
			EnumDisplaySettings(dd.DeviceName, i, &dm);
			w = dm.dmPelsWidth;
			h = dm.dmPelsHeight;
			b = dm.dmBitsPerPel;
			f = dm.dmDisplayFrequency;

			temp.Format("%dx%dx%d @%dHz", w, h, b, f);
			listmodes->AddString(temp);
			WidthList[i] = w;
			HeightList[i] = h;
			BPPList[i] = b;
			FreqList[i] = f;
		}
		break;


	case DIRECT_DRAW:
		apiname->SetWindowText("DirectDraw 7");
		break;


	case DIRECT_3D:
		apiname->SetWindowText("Direct3D 9");
		// Load DirectX DLL
		d3dDLL = LoadLibrary("D3D9.DLL");
		LPDIRECT3D9 (WINAPI *D3DCreate)(UINT);
		D3DCreate = (LPDIRECT3D9 (WINAPI *)(UINT))
			GetProcAddress(d3dDLL, "Direct3DCreate9");
		pD3D = D3DCreate(D3D_SDK_VERSION);
		nAdapters = pD3D->GetAdapterCount();
		D3DADAPTER_IDENTIFIER9 id;
		pD3D->GetAdapterIdentifier(iDisplayDevice, 0, &id);
		devicename->SetWindowText(id.Description);

		D3DDISPLAYMODE d3ddm;

		nModes16 = pD3D->GetAdapterModeCount(iDisplayDevice, D3DFMT_R5G6B5);
		nModes32 = pD3D->GetAdapterModeCount(iDisplayDevice, D3DFMT_X8R8G8B8);

		listmodes->InitStorage(nModes16+nModes32, 25);
		listmodes->ResetContent();

		if (WidthList!=0) delete [] WidthList;
		if (HeightList!=0) delete [] HeightList;
		if (BPPList!=0) delete [] BPPList;
		if (FreqList!=0) delete [] FreqList;
		WidthList = new DWORD[nModes16+nModes32];
		HeightList = new DWORD[nModes16+nModes32];
		BPPList = new DWORD[nModes16+nModes32];
		FreqList = new DWORD[nModes16+nModes32];

		b = 16;
		for (UINT i = 0; i < nModes16; i++)
		{
			pD3D->EnumAdapterModes(iDisplayDevice, D3DFMT_R5G6B5, i, &d3ddm);
			w = d3ddm.Width;
			h = d3ddm.Height;
			f = d3ddm.RefreshRate;

			temp.Format("%dx%dx%d @%dHz", w, h, b, f);
			listmodes->AddString(temp);

			WidthList[i] = w;
			HeightList[i] = h;
			BPPList[i] = b;
			FreqList[i] = f;
		}
		b = 32;
		for (UINT i = 0; i < nModes32; i++)
		{
			pD3D->EnumAdapterModes(iDisplayDevice, D3DFMT_X8R8G8B8, i, &d3ddm);
			w = d3ddm.Width;
			h = d3ddm.Height;
			f = d3ddm.RefreshRate;

			temp.Format("%dx%dx%d @%dHz", w, h, b, f);
			listmodes->AddString(temp);

			WidthList[i+nModes16] = w;
			HeightList[i+nModes16] = h;
			BPPList[i+nModes16] = b;
			FreqList[i+nModes16] = f;
		}

		// Clean up		
		pD3D->Release();
		pD3D = NULL;
		if(d3dDLL != NULL)
		{
			FreeLibrary(d3dDLL);
			d3dDLL = NULL;
		}
		break;


	case OPENGL:
		apiname->SetWindowText("OpenGL");
		break;
	}

   return TRUE;   // return TRUE unless you set the focus to a control
                  // EXCEPTION: OCX Property Pages should return FALSE
}


void UniVideoModeDlg::OnBnClickedOk()
{
	CListBox* listmodes = (CListBox*)GetDlgItem(IDC_LISTMODES);
	int selection = listmodes->GetCurSel();
	if (selection == LB_ERR)
	{
		MessageBox("No mode selected!", "Error", MB_OK | MB_ICONWARNING);
		return;
	}
	*SelectedWidth = WidthList[selection];
	*SelectedHeight = HeightList[selection];
	*SelectedBPP = BPPList[selection];
	*SelectedFreq = FreqList[selection];
	*SelectedAdapter = iDisplayDevice;

	EndDialog(0);
}

void UniVideoModeDlg::OnBnClickedCancel()
{
	EndDialog(-1);
}
void UniVideoModeDlg::OnStnClickedDisplaydevice()
{
	DWORD old = iDisplayDevice;
	switch (theApp.display->getType())
	{
	case GDI:
		DISPLAY_DEVICE dd;
		dd.cb = sizeof(dd);
		if (0 == EnumDisplayDevices(NULL, ++iDisplayDevice, &dd, 0))
			iDisplayDevice = 0;
		break;


	case DIRECT_DRAW:
		break;
	
	
	case DIRECT_3D:
		iDisplayDevice++;
		if (iDisplayDevice == nAdapters) iDisplayDevice = 0;
		break;


	case OPENGL:
		break;
	}

	if (iDisplayDevice != old)
		OnInitDialog();
}

void UniVideoModeDlg::OnBnClickedButtonMaxscale()
{
	MaxScale dlg;
	theApp.winCheckFullscreen();
	dlg.DoModal();
}

void UniVideoModeDlg::OnBnClickedCheckStretchtofit()
{
	theApp.fullScreenStretch = !theApp.fullScreenStretch;
	theApp.updateWindowSize(theApp.videoOption);
	if(theApp.display)
		theApp.display->clear();
	this->SetFocus();
}
