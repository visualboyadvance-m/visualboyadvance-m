#include "stdafx.h"
#include "vba.h"
#include "SelectPlugin.h"
#include "rpi.h"
#include "reg.h"
#include <vector>
#include <string>

using namespace std;

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

vector<PluginDesc> rpiPool;

/////////////////////////////////////////////////////////////////////////////
// SelectPlugin dialog


SelectPlugin::SelectPlugin(CWnd* pParent /*=NULL*/)
	: CDialog(SelectPlugin::IDD, pParent)
{
	//{{AFX_DATA_INIT(SelectPlugin)
	//}}AFX_DATA_INIT
}


void SelectPlugin::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(SelectPlugin)
	DDX_Control(pDX, IDC_COMBO_PLUGIN, m_comboPlugin);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(SelectPlugin, CDialog)
	//{{AFX_MSG_MAP(SelectPlugin)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// SelectPlugin message handlers

void SelectPlugin::OnOK()
{
	// TODO: Add extra validation here
	if (m_comboPlugin.GetCount() > 0)
	{
		int nSel = m_comboPlugin.GetCurSel();
		if (nSel >= 0 && nSel < (int)rpiPool.size())
			strcpy(theApp.pluginName, rpiPool[nSel].sFile);
	}

	CDialog::OnOK();
}

void SelectPlugin::OnCancel()
{
	// TODO: Add extra cleanup here

	CDialog::OnCancel();
}

BOOL SelectPlugin::OnInitDialog()
{
	CDialog::OnInitDialog();

	m_comboPlugin.ResetContent();

	size_t nPluginCnt = EnumPlugins();
	if (nPluginCnt > 0)
	{
		for (size_t i = 0; i < rpiPool.size(); i++)
			m_comboPlugin.AddString(rpiPool[i].sDesc);

		for (size_t ii = 0; ii < rpiPool.size(); ii++)
		{
			if (_stricmp(theApp.pluginName, rpiPool[ii].sFile) == 0)
			{
				m_comboPlugin.SetCurSel(ii);
				break;
			}
		}
	}


	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

size_t SelectPlugin::EnumPlugins()
{
	rpiPool.clear();

	char sFindFile[MAX_PATH];
	char *ptr;

	GetModuleFileName(NULL, sFindFile, sizeof(sFindFile));
	ptr = strrchr(sFindFile, '\\');
	if (ptr)
		*ptr = '\0';
	strcat(sFindFile, "\\plugins\\*.rpi");

	PluginDesc plugDesc;
	WIN32_FIND_DATA FindFileData;
	HANDLE hFind;

	memset(&FindFileData, 0, sizeof(FindFileData));
	hFind = FindFirstFile(sFindFile, &FindFileData);
	if (hFind != INVALID_HANDLE_VALUE)
	{
		if (GetPluginDesc(FindFileData.cFileName, &plugDesc))
			rpiPool.push_back(plugDesc);

		while (true)
		{
			memset(&FindFileData, 0, sizeof(FindFileData));
			if (!FindNextFile(hFind, &FindFileData))
				break;

			if (GetPluginDesc(FindFileData.cFileName, &plugDesc))
				rpiPool.push_back(plugDesc);
		}
		FindClose(hFind);
	}

	return rpiPool.size();
}

bool SelectPlugin::GetPluginDesc(const char *sRpi, PluginDesc *pDesc)
{
	HINSTANCE rpiDLL = NULL;
	char sFile[MAX_PATH];
	char *ptr;

	GetModuleFileName(NULL, sFile, sizeof(sFile));
	ptr = strrchr(sFile, '\\');
	if (ptr)
		*ptr = '\0';
	strcat(sFile, "\\plugins\\");
	strcat(sFile, sRpi);

  	rpiDLL = LoadLibrary(sFile);
  	if (!rpiDLL)
  		return false;

	RENDPLUG_GetInfo fnGetInfo = (RENDPLUG_GetInfo) GetProcAddress(rpiDLL, "RenderPluginGetInfo");
	RENDPLUG_Output fnOutput = (RENDPLUG_Output) GetProcAddress(rpiDLL, "RenderPluginOutput");
	if (fnGetInfo == NULL || fnOutput == NULL)
	{
		FreeLibrary(rpiDLL);
		rpiDLL = NULL;
		return false;
	}

	RENDER_PLUGIN_INFO *pRPI = fnGetInfo();
	if (pRPI == NULL)
	{
		FreeLibrary(rpiDLL);
		return false;
	}

	memset(pDesc, 0, sizeof(PluginDesc));
	strcpy(pDesc->sFile, sRpi);
	strcpy(pDesc->sDesc, pRPI->Name);
	FreeLibrary(rpiDLL);

	return true;
}
