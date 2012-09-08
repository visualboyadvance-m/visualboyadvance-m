#if !defined(AFX_EXPORTGSASNAPSHOT_H__ADF8566A_C64D_43CF_9CD2_A290370BA4F1__INCLUDED_)
#define AFX_EXPORTGSASNAPSHOT_H__ADF8566A_C64D_43CF_9CD2_A290370BA4F1__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// ExportGSASnapshot.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// ExportGSASnapshot dialog

class ExportGSASnapshot : public CDialog
{
  // Construction
 public:
  ExportGSASnapshot(CString filename, CString title,CWnd* pParent = NULL);   // standard constructor

  // Dialog Data
  //{{AFX_DATA(ExportGSASnapshot)
  enum { IDD = IDD_EXPORT_SPS };
  CString  m_desc;
  CString  m_notes;
  CString  m_title;
  //}}AFX_DATA
  CString m_filename;


  // Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(ExportGSASnapshot)
 protected:
  virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
  //}}AFX_VIRTUAL

  // Implementation
 protected:

  // Generated message map functions
  //{{AFX_MSG(ExportGSASnapshot)
  virtual BOOL OnInitDialog();
  afx_msg void OnCancel();
  afx_msg void OnOk();
  //}}AFX_MSG
  DECLARE_MESSAGE_MAP()
    };

    //{{AFX_INSERT_LOCATION}}
    // Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_EXPORTGSASNAPSHOT_H__ADF8566A_C64D_43CF_9CD2_A290370BA4F1__INCLUDED_)
