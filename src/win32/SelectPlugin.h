#if !defined(AFX_SELECTPLUGIN_H__A097B9D0_7C23_4C1A_A2D3_1AEC07501BBC__INCLUDED_)
#define AFX_SELECTPLUGIN_H__A097B9D0_7C23_4C1A_A2D3_1AEC07501BBC__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// SelectPlugin.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// SelectPlugin dialog
struct PluginDesc
{
	char sFile[MAX_PATH];
	char sDesc[60];
};

class SelectPlugin : public CDialog
{
// Construction
public:
	SelectPlugin(CWnd* pParent = NULL);   // standard constructor
	size_t EnumPlugins();
	bool GetPluginDesc(const char *sRpi, PluginDesc *pDesc);

// Dialog Data
	//{{AFX_DATA(SelectPlugin)
	enum { IDD = IDD_SELECT_PLUGIN };
	CComboBox	m_comboPlugin;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(SelectPlugin)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(SelectPlugin)
	virtual void OnOK();
	virtual void OnCancel();
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_SELECTPLUGIN_H__A097B9D0_7C23_4C1A_A2D3_1AEC07501BBC__INCLUDED_)
