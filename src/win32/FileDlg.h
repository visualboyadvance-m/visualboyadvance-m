#if !defined(AFX_FILEDLG_H__7E4F8B92_1B63_4126_8261_D9334C645940__INCLUDED_)
#define AFX_FILEDLG_H__7E4F8B92_1B63_4126_8261_D9334C645940__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// FileDlg.h : header file
//

struct OPENFILENAMEEX : public OPENFILENAME {
  void *        pvReserved;
  DWORD         dwReserved;
  DWORD         FlagsEx;
};

/////////////////////////////////////////////////////////////////////////////
// FileDlg dialog

class FileDlg
{
 private:
  CString m_file;
  CString m_filter;
 public:
  OPENFILENAMEEX m_ofn;
  int DoModal();
  LPCTSTR GetPathName();
  virtual int getFilterIndex();
  virtual void OnTypeChange(HWND hwnd);
  FileDlg(CWnd *parent, LPCTSTR file, LPCTSTR filter,
          int filterIndex, LPCTSTR ext, LPCTSTR *exts, LPCTSTR initialDir,
          LPCTSTR title, bool save);
  virtual ~FileDlg();

 protected:
  bool isSave;
  LPCTSTR *extensions;

 protected:
    //{{AFX_INSERT_LOCATION}}
    // Microsoft Visual C++ will insert additional declarations immediately before the previous line.
};

#endif // !defined(AFX_FILEDLG_H__7E4F8B92_1B63_4126_8261_D9334C645940__INCLUDED_)
