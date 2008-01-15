// VisualBoyAdvance - Nintendo Gameboy/GameboyAdvance (TM) emulator.
// Copyright (C) 2008 VBA-M development team

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or(at your option)
// any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.


#include "stdafx.h"
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

//#ifndef NO_OGL
	//case OPENGL:
		//break;
//#endif

	// TODO:  Add extra initialization here

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
	if( failed ) return;

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

		res = pD3D->EnumAdapterModes( adapter, D3DFMT_A2R10G10B10, 0, &mode );
		if( res == D3D_OK ) {
			int index = combo_color_depth.AddString( _T("32bit+ (A2R10G10B10)") );
			combo_color_depth.SetItemData( index, (DWORD_PTR)D3DFMT_A2R10G10B10 );
		}

		res = pD3D->EnumAdapterModes( adapter, D3DFMT_X8R8G8B8, 0, &mode );
		if( res == D3D_OK ) {
			int index = combo_color_depth.AddString( _T("32bit (X8R8G8B8)") );
			combo_color_depth.SetItemData( index, (DWORD_PTR)D3DFMT_X8R8G8B8 );
		}

		res = pD3D->EnumAdapterModes( adapter, D3DFMT_R5G6B5, 0, &mode );
		if( res == D3D_OK ) {
			int index = combo_color_depth.AddString( _T("16bit+ (R5G6B5)") );
			combo_color_depth.SetItemData( index, (DWORD_PTR)D3DFMT_R5G6B5 );
		}

		res = pD3D->EnumAdapterModes( adapter, D3DFMT_X1R5G5B5, 0, &mode );
		if( res == D3D_OK ) {
			int index = combo_color_depth.AddString( _T("16bit (X1R5G5B5)") );
			combo_color_depth.SetItemData( index, (DWORD_PTR)D3DFMT_X1R5G5B5 );
		}

		if( combo_color_depth.GetCount() == 0 ) {
			failed = true;
			return;
		}

		combo_color_depth.SetCurSel( 0 );
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

		int count = combo_resolution.GetCount();
		combo_resolution.SetCurSel( count - 1 ); // select last item
		OnCbnSelchangeComboResolution();
	}
#endif
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

		int count = combo_refresh_rate.GetCount();
		combo_refresh_rate.SetCurSel( count - 1 ); // select last item
	}
#endif
}

void FullscreenSettings::OnOK()
{
	if( failed ) return;

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
	{	// TODO: use these assignments for VBA as well
	case D3DFMT_A2R10G10B10:
		m_colorDepth = 30;
		break;
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


	CDialog::OnOK();
}
