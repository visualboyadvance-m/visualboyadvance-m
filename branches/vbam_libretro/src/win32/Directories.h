#pragma once


class Directories : public CDialog
{
public:
	Directories(CWnd* pParent = NULL);
	enum { IDD = IDD_DIRECTORIES };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual BOOL OnInitDialog();
	afx_msg void OnBatteryDir();
	afx_msg void OnCaptureDir();
	afx_msg void OnGbromDir();
	afx_msg void OnGbcromDir();
	afx_msg void OnRomDir();
	afx_msg void OnSaveDir();
	virtual void OnCancel();
	virtual void OnOK();
	DECLARE_MESSAGE_MAP()

private:
	CEdit  m_savePath;
	CEdit  m_romPath;
	CEdit  m_gbcromPath;
	CEdit  m_gbromPath;
	CEdit  m_capturePath;
	CEdit  m_batteryPath;
	CString initialFolderDir;

	CString browseForDir(CString title);
	bool directoryDoesExist(const char *directory);
};
