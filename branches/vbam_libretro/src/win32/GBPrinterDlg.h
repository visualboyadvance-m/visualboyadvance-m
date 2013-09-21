#if !defined(AFX_GBPRINTER_H__3180CC5A_1F9D_47E5_B044_407442CB40A4__INCLUDED_)
#define AFX_GBPRINTER_H__3180CC5A_1F9D_47E5_B044_407442CB40A4__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// GBPrinter.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// GBPrinter dialog

class GBPrinterDlg : public CDialog
{
 private:
  u8 bitmapHeader[sizeof(BITMAPINFO)+4*sizeof(RGBQUAD)];
  BITMAPINFO *bitmap;
  u8 *bitmapData;
  int scale;
  // Construction
 public:
  void processData(u8 *data);
  void saveAsPNG(const char *name);
  void saveAsBMP(const char *name);
  GBPrinterDlg(CWnd* pParent = NULL);   // standard constructor
  ~GBPrinterDlg();

  // Dialog Data
  //{{AFX_DATA(GBPrinterDlg)
  enum { IDD = IDD_GB_PRINTER };
  int    m_scale;
  //}}AFX_DATA


  // Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(GBPrinterDlg)
 protected:
  virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
  //}}AFX_VIRTUAL

  // Implementation
 protected:

  // Generated message map functions
  //{{AFX_MSG(GBPrinterDlg)
  afx_msg void OnSave();
  afx_msg void OnPrint();
  virtual BOOL OnInitDialog();
  afx_msg void OnOk();
  afx_msg void On1x();
  afx_msg void On2x();
  afx_msg void On3x();
  afx_msg void On4x();
  afx_msg void OnPaint();
  //}}AFX_MSG
  DECLARE_MESSAGE_MAP()
    };

    //{{AFX_INSERT_LOCATION}}
    // Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_GBPRINTER_H__3180CC5A_1F9D_47E5_B044_407442CB40A4__INCLUDED_)
