#include "stdafx.h"
#include "VBA.h"

#include "FullscreenSettings.h"


// FullscreenSettings dialog

IMPLEMENT_DYNAMIC(FullscreenSettings, CDialog)

FullscreenSettings::FullscreenSettings(CWnd* pParent /*=NULL*/)
	: CDialog(FullscreenSettings::IDD, pParent)
{
	m_device = 0;
	m_colorDepth = 0;
	m_width = 0;
	m_height = 0;
	m_refreshRate = 0;

	failed = false;
#ifndef NO_D3D
	pD3D = NULL;
#endif
}

FullscreenSettings::~FullscreenSettings()
{
#ifndef NO_D3D
	if( pD3D ) {
		pD3D->Release();
		pD3D = NULL;
	}
#endif
}

void FullscreenSettings::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_COMBO_DEVICE, combo_device);
	DDX_Control(pDX, IDC_COMBO_RESOLUTION, combo_resolution);
	DDX_Control(pDX, IDC_COMBO_COLOR_DEPTH, combo_color_depth);
	DDX_Control(pDX, IDC_COMBO_REFRESH_RATE, combo_refresh_rate);
}


BEGIN_MESSAGE_MAP(FullscreenSettings, CDialog)
	ON_CBN_SELCHANGE(IDC_COMBO_DEVICE, &FullscreenSettings::OnCbnSelchangeComboDevice)
	ON_CBN_SELCHANGE(IDC_COMBO_COLOR_DEPTH, &FullscreenSettings::OnCbnSelchangeComboColorDepth)
	ON_CBN_SELCHANGE(IDC_COMBO_RESOLUTION, &FullscreenSettings::OnCbnSelchangeComboResolution)
END_MESSAGE_MAP()


// FullscreenSettings message handlers

BOOL FullscreenSettings::OnInitDialog()
{
	CDialog::OnInitDialog();

#ifndef NO_D3D
	if( api == DIRECT_3D ) {
		pD3D = Direct3DCreate9( D3D_SDK_VERSION );
		if( pD3D == NULL) {
			failed = true;
			return TRUE;
		}

		// enumerate devices
		UINT nAdapters = pD3D->GetAdapterCount();
		if( nAdapters < 1 ) {
			failed = true;
			pD3D->Release();
			pD3D = NULL;
			return TRUE;
		}
		combo_device.ResetContent();
		for( UINT i = 0 ; i < nAdapters ; i++ ) {
			D3DADAPTER_IDENTIFIER9 id;
			pD3D->GetAdapterIdentifier( i, 0, &id );
			int index = combo_device.AddString( id.Description );
			combo_device.SetItemData( index, (DWORD_PTR)i );
		}
		combo_device.SetCurSel( 0 );
		OnCbnSelchangeComboDevice();
	}
#endif

#ifndef NO_OGL
	if( api == OPENGL ) {
		// enumerate devices
		DWORD iDevice = 0;
		DISPLAY_DEVICE devInfo;
		ZeroMemory( &devInfo, sizeof(devInfo) );
		devInfo.cb = sizeof(devInfo);
		combo_device.ResetContent();
		while( TRUE == EnumDisplayDevices( NULL, iDevice, &devInfo, 0 ) ) {
			int index = combo_device.AddString( devInfo.DeviceString );
			combo_device.SetItemData( index, (DWORD_PTR)iDevice );
			iDevice++;
		}
		combo_device.SetCurSel( 0 );
		OnCbnSelchangeComboDevice();
	}
#endif

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}


// Call this function to select the API for the display mode enumeration.
void FullscreenSettings::setAPI( DISPLAY_TYPE type )
{
	api = type;
}


void FullscreenSettings::OnCbnSelchangeComboDevice()
{
	failed = false;

#ifndef NO_D3D
	if( api == DIRECT_3D ) {
		int selection;

		selection = combo_device.GetCurSel();
		if( selection == LB_ERR ) return;
		UINT adapter = (UINT)combo_device.GetItemData( selection );

		// enumerate color depths
		HRESULT res;
		D3DDISPLAYMODE mode;
		combo_color_depth.ResetContent();

		res = pD3D->EnumAdapterModes( adapter, D3DFMT_X8R8G8B8, 0, &mode );
		if( res == D3D_OK ) {
			int index = combo_color_depth.AddString( _T("24bit (X8R8G8B8)") );
			combo_color_depth.SetItemData( index, (DWORD_PTR)D3DFMT_X8R8G8B8 );
		}

		res = pD3D->EnumAdapterModes( adapter, D3DFMT_R5G6B5, 0, &mode );
		if( res == D3D_OK ) {
			int index = combo_color_depth.AddString( _T("16bit (R5G6B5)") );
			combo_color_depth.SetItemData( index, (DWORD_PTR)D3DFMT_R5G6B5 );
		}

		res = pD3D->EnumAdapterModes( adapter, D3DFMT_X1R5G5B5, 0, &mode );
		if( res == D3D_OK ) {
			int index = combo_color_depth.AddString( _T("15bit (X1R5G5B5)") );
			combo_color_depth.SetItemData( index, (DWORD_PTR)D3DFMT_X1R5G5B5 );
		}

		if( combo_color_depth.GetCount() == 0 ) {
			failed = true;
			combo_resolution.ResetContent();
			combo_refresh_rate.ResetContent();
			return;
		}

		combo_color_depth.SetCurSel( combo_color_depth.GetCount() - 1 );
		OnCbnSelchangeComboColorDepth();
	}
#endif

#ifndef NO_OGL
	if( api == OPENGL ) {
		int selection;

		// get selected device's name
		selection = combo_device.GetCurSel();
		if( selection == LB_ERR ) return;
		DWORD iDevice = (UINT)combo_device.GetItemData( selection );
		DISPLAY_DEVICE devInfo;
		ZeroMemory( &devInfo, sizeof(devInfo) );
		devInfo.cb = sizeof(devInfo);
		EnumDisplayDevices( NULL, iDevice, &devInfo, 0 );

		// enumerate color depths
		DWORD iMode = 0;
		DEVMODE mode;
		ZeroMemory( &mode, sizeof(mode) );
		mode.dmSize = sizeof(mode);
		bool alreadyExists;
		combo_color_depth.ResetContent();

		while( TRUE == EnumDisplaySettings( devInfo.DeviceName, iMode, &mode ) ) {
			if( mode.dmDisplayFlags != 0 ) {
				iMode++;
				continue; // don't list special modes
			}
			alreadyExists = false;
			for( int iItem = 0 ; iItem < combo_color_depth.GetCount() ; iItem++ ) {
				if( mode.dmBitsPerPel == (DWORD)combo_color_depth.GetItemData( iItem ) ) {
					alreadyExists = true;
					break;
				}
			}
			if( !alreadyExists ) {
				CString temp;
				temp.Format( _T("%2u bits per pixel"), mode.dmBitsPerPel );
				int index = combo_color_depth.AddString( temp );
				combo_color_depth.SetItemData( index, (DWORD_PTR)mode.dmBitsPerPel );
			}
			iMode++;
		}

		if( combo_color_depth.GetCount() == 0 ) {
			failed = true;
			combo_resolution.ResetContent();
			combo_refresh_rate.ResetContent();
			return;
		}

		combo_color_depth.SetCurSel( combo_color_depth.GetCount() - 1 );
		OnCbnSelchangeComboColorDepth();
	}
#endif
}


void FullscreenSettings::OnCbnSelchangeComboColorDepth()
{
	if( failed ) return;

#ifndef NO_D3D
	if( api == DIRECT_3D ) {
		int selection;

		selection = combo_device.GetCurSel();
		if( selection == LB_ERR ) return;
		UINT adapter = (UINT)combo_device.GetItemData( selection );

		selection = combo_color_depth.GetCurSel();
		if( selection == LB_ERR ) return;
		D3DFORMAT format = (D3DFORMAT)combo_color_depth.GetItemData( selection );

		// enumerate resolutions
		HRESULT res;
		D3DDISPLAYMODE mode;
		UINT nModes = pD3D->GetAdapterModeCount( adapter, format );
		D3DDISPLAYMODE *allModes = new D3DDISPLAYMODE[nModes];
		ZeroMemory( allModes, sizeof(D3DDISPLAYMODE) * nModes );
		combo_resolution.ResetContent();

		for( UINT i = 0 ; i < nModes ; i++ ) {
			res = pD3D->EnumAdapterModes( adapter, format, i, &mode );
			if( res == D3D_OK ) {
				CString temp;
				temp.Format( _T("%4u x %4u"), mode.Width, mode.Height );
				bool alreadyPresent = false;
				for( UINT surf = 0 ; surf < i ; surf++ ) {
					// ignore same resolution with different refresh rate
					if( ( allModes[surf].Width == mode.Width ) &&
						( allModes[surf].Height == mode.Height ) ) {
							alreadyPresent = true;
					}
				}

				if( !alreadyPresent ) {
					int index = combo_resolution.AddString( temp );
					combo_resolution.SetItemData( index, (DWORD_PTR)i );
				}
				allModes[i] = mode;
			}
		}
		delete [] allModes;
	}
#endif

#ifndef NO_OGL
	if( api == OPENGL ) {
		int selection;

		// get selected device's name
		selection = combo_device.GetCurSel();
		if( selection == LB_ERR ) return;
		DWORD iDevice = (UINT)combo_device.GetItemData( selection );
		DISPLAY_DEVICE devInfo;
		ZeroMemory( &devInfo, sizeof(devInfo) );
		devInfo.cb = sizeof(devInfo);
		EnumDisplayDevices( NULL, iDevice, &devInfo, 0 );

		// get selected bit depth value
		selection = combo_color_depth.GetCurSel();
		if( selection == LB_ERR ) return;
		DWORD selectedBPP = (DWORD)combo_color_depth.GetItemData( selection );


		// enumerate resolutions
		DWORD iMode = 0;
		DEVMODE mode, surfMode;
		ZeroMemory( &mode, sizeof(mode) );
		ZeroMemory( &surfMode, sizeof(surfMode) );
		mode.dmSize = sizeof(mode);
		surfMode.dmSize = sizeof(surfMode);
		bool alreadyExists;
		combo_resolution.ResetContent();

		while( TRUE == EnumDisplaySettings( devInfo.DeviceName, iMode, &mode ) ) {
			if( ( mode.dmBitsPerPel != selectedBPP) ||
				( mode.dmDisplayFlags != 0 ) ) {
					iMode++;
					continue;
			}
			alreadyExists = false;
			for( int iItem = 0 ; iItem < combo_resolution.GetCount() ; iItem++ ) {
				DWORD iSurfMode = (DWORD)combo_resolution.GetItemData( iItem );
				EnumDisplaySettings( devInfo.DeviceName, iSurfMode, &surfMode );
				if( ( mode.dmPelsWidth == surfMode.dmPelsWidth ) &&
					( mode.dmPelsHeight == surfMode.dmPelsHeight ) ) {
						alreadyExists = true;
						break;
				}
			}
			if( !alreadyExists ) {
				CString temp;
				temp.Format( _T("%4u x %4u"), mode.dmPelsWidth, mode.dmPelsHeight );
				int index = combo_resolution.AddString( temp );
				combo_resolution.SetItemData( index, (DWORD_PTR)iMode );
			}
			iMode++;
		}

		if( combo_resolution.GetCount() == 0 ) {
			failed = true;
			return;
		}
	}
#endif

	combo_resolution.SetCurSel( combo_resolution.GetCount() - 1 ); // select last item
	OnCbnSelchangeComboResolution();
}


void FullscreenSettings::OnCbnSelchangeComboResolution()
{
	if( failed ) return;

#ifndef NO_D3D
	if( api == DIRECT_3D ) {
		int selection;

		selection = combo_device.GetCurSel();
		if( selection == LB_ERR ) return;
		UINT adapter = (UINT)combo_device.GetItemData( selection );

		selection = combo_color_depth.GetCurSel();
		if( selection == LB_ERR ) return;
		D3DFORMAT format = (D3DFORMAT)combo_color_depth.GetItemData( selection );

		selection = combo_resolution.GetCurSel();
		if( selection == LB_ERR ) return;
		UINT iMode = (UINT)combo_resolution.GetItemData( selection );

		// enumerate refresh rates
		HRESULT res;
		D3DDISPLAYMODE mode, mode2;
		pD3D->EnumAdapterModes( adapter, format, iMode, &mode );
		UINT nModes = pD3D->GetAdapterModeCount( adapter, format );
		combo_refresh_rate.ResetContent();

		for( UINT i = 0 ; i < nModes ; i++ ) {
			res = pD3D->EnumAdapterModes( adapter, format, i, &mode2 );
			if( ( res == D3D_OK ) &&
				( mode2.Width == mode.Width ) &&
				( mode2.Height == mode.Height ) )
			{
				CString temp;
				temp.Format( _T("%3u Hz"), mode2.RefreshRate );
				int index = combo_refresh_rate.AddString( temp );
				combo_refresh_rate.SetItemData( index, (DWORD_PTR)i );
			}
		}
	}
#endif

#ifndef NO_OGL
	if( api == OPENGL ) {
		int selection;

		// get selected device's name
		selection = combo_device.GetCurSel();
		if( selection == LB_ERR ) return;
		DWORD iDevice = (UINT)combo_device.GetItemData( selection );
		DISPLAY_DEVICE devInfo;
		ZeroMemory( &devInfo, sizeof(devInfo) );
		devInfo.cb = sizeof(devInfo);
		EnumDisplayDevices( NULL, iDevice, &devInfo, 0 );

		// get selected resolution / bpp
		DEVMODE selectedResolution;
		ZeroMemory( &selectedResolution, sizeof(selectedResolution) );
		selectedResolution.dmSize = sizeof(selectedResolution);
		selection = combo_resolution.GetCurSel();
		if( selection == LB_ERR ) return;
		DWORD dwSelectedRes = (DWORD)combo_resolution.GetItemData( selection );
		EnumDisplaySettings( devInfo.DeviceName, dwSelectedRes, &selectedResolution );


		// enumerate refresh rates
		DWORD iMode = 0;
		DEVMODE mode, surfMode;
		ZeroMemory( &mode, sizeof(mode) );
		ZeroMemory( &surfMode, sizeof(surfMode) );
		mode.dmSize = sizeof(mode);
		surfMode.dmSize = sizeof(surfMode);
		bool alreadyExists;
		combo_refresh_rate.ResetContent();

		while( TRUE == EnumDisplaySettings( devInfo.DeviceName, iMode, &mode ) ) {
			if( ( mode.dmBitsPerPel != selectedResolution.dmBitsPerPel ) ||
				( mode.dmDisplayFlags != 0 ) ||
				( mode.dmPelsWidth != selectedResolution.dmPelsWidth ) ||
				( mode.dmPelsHeight != selectedResolution.dmPelsHeight ) ) {
					iMode++;
					continue;
			}
			alreadyExists = false;
			for( int iItem = 0 ; iItem < combo_refresh_rate.GetCount() ; iItem++ ) {
				DWORD iSurfMode = (DWORD)combo_refresh_rate.GetItemData( iItem );
				EnumDisplaySettings( devInfo.DeviceName, iSurfMode, &surfMode );
				if( ( mode.dmDisplayFrequency == surfMode.dmDisplayFrequency ) ) {
						alreadyExists = true;
						break;
				}
			}
			if( !alreadyExists ) {
				CString temp;
				temp.Format( _T("%3u Hz"), mode.dmDisplayFrequency );
				int index = combo_refresh_rate.AddString( temp );
				combo_refresh_rate.SetItemData( index, (DWORD_PTR)iMode );
			}
			iMode++;
		}

	}
#endif

	combo_refresh_rate.SetCurSel( combo_refresh_rate.GetCount() - 1 ); // select last item
}

void FullscreenSettings::OnOK()
{
	if( failed ) return;

#ifndef NO_D3D
	if( api == DIRECT_3D ) {
		int selection;

		selection = combo_device.GetCurSel();
		if( selection == LB_ERR ) return;
		UINT adapter = (UINT)combo_device.GetItemData( selection );

		selection = combo_color_depth.GetCurSel();
		if( selection == LB_ERR ) return;
		D3DFORMAT format = (D3DFORMAT)combo_color_depth.GetItemData( selection );

		selection = combo_refresh_rate.GetCurSel();
		if( selection == LB_ERR ) return;
		UINT selectedMode = (UINT)combo_refresh_rate.GetItemData( selection );

		D3DDISPLAYMODE mode;
		pD3D->EnumAdapterModes( adapter, format, selectedMode, &mode );

		m_device = (unsigned int)adapter;
		switch( mode.Format )
		{
        case D3DFMT_X8R8G8B8:
			m_colorDepth = 24;
			break;
		case D3DFMT_R5G6B5:
			m_colorDepth = 16;
			break;
		case D3DFMT_X1R5G5B5:
			m_colorDepth = 15;
			break;
		}
		m_width = (unsigned int)mode.Width;
		m_height = (unsigned int)mode.Height;
		m_refreshRate = (unsigned int)mode.RefreshRate;
	}
#endif

#ifndef NO_OGL
	if( api == OPENGL ) {
		int selection;

		// get selected device's name
		selection = combo_device.GetCurSel();
		if( selection == LB_ERR ) return;
		DWORD iDevice = (UINT)combo_device.GetItemData( selection );
		DISPLAY_DEVICE devInfo;
		ZeroMemory( &devInfo, sizeof(devInfo) );
		devInfo.cb = sizeof(devInfo);
		EnumDisplayDevices( NULL, iDevice, &devInfo, 0 );

		// get selected resolution / bpp
		DEVMODE mode;
		ZeroMemory( &mode, sizeof(mode) );
		mode.dmSize = sizeof(mode);
		selection = combo_refresh_rate.GetCurSel();
		if( selection == LB_ERR ) return;
		DWORD selectedMode = (DWORD)combo_refresh_rate.GetItemData( selection );
		EnumDisplaySettings( devInfo.DeviceName, selectedMode, &mode );


		m_device = (unsigned int)iDevice;
		switch( mode.dmBitsPerPel )
		{ // the policy of this dialog is to return the actually usable amount of bits
		case 4:
			m_colorDepth = 4;
			break;
		case 8:
			m_colorDepth = 8;
			break;
		case 16:
			m_colorDepth = 16;
			break;
		case 24:
		case 32:
			m_colorDepth = 24;
			break;
		}
		m_width = (unsigned int)mode.dmPelsWidth;
		m_height = (unsigned int)mode.dmPelsHeight;
		m_refreshRate = (unsigned int)mode.dmDisplayFrequency;
	}
#endif

	CDialog::OnOK();
}
