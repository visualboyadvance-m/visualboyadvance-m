#pragma once


// BIOSDialog dialog

class BIOSDialog : public CDialog
{
	DECLARE_DYNAMIC(BIOSDialog)

public:
	BIOSDialog(CWnd* pParent = NULL);   // standard constructor
	virtual ~BIOSDialog();

// Dialog Data
	enum { IDD = IDD_BIOS };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
private:
	CEdit m_editGB;
	CEdit m_editGBC;
	CEdit m_editGBA;
	bool fileExists(CString& file);
	afx_msg void OnBnClickedSelectGbBiosPath();
	afx_msg void OnBnClickedSelectGbcBiosPath();
	afx_msg void OnBnClickedSelectGbaBiosPath();
public:
	BOOL m_enableBIOS_GB;
	BOOL m_enableBIOS_GBC;
	BOOL m_enableBIOS_GBA;
	BOOL m_skipLogo;
	CString m_pathGB;
	CString m_pathGBC;
	CString m_pathGBA;
};
