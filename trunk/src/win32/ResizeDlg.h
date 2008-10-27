#ifndef VBA_WIN32_RESIZEDLG_H
#define VBA_WIN32_RESIZEDLG_H

#ifndef _INC_TCHAR
#include <tchar.h>
#endif  //      _INC_TCHAR

//
//      Predefined sizing information
#define DS_MoveX                1
#define DS_MoveY                2
#define DS_SizeX                4
#define DS_SizeY                8

typedef struct DialogSizerSizingItem    //      sdi
{
  UINT uControlID;
  UINT uSizeInfo;
} DialogSizerSizingItem;

#define DIALOG_SIZER_START( name )      DialogSizerSizingItem name[] = {
#define DIALOG_SIZER_ENTRY( controlID, flags )  { controlID, flags },
#define DIALOG_SIZER_END()      { 0xFFFFFFFF, 0xFFFFFFFF } };

class ResizeDlg : public CDialog {
  void *dd;
 public:
  ResizeDlg(UINT id, CWnd *parent = NULL);

  void *AddDialogData();
  BOOL SetData(const DialogSizerSizingItem *psd,
               BOOL bShowSizingGrip,
               HKEY hkRootSave,
               LPCTSTR pcszName,
               SIZE *psizeMax);
  void UpdateWindowSize(const int cx, const int cy, HWND);

  virtual BOOL OnWndMsg(UINT, WPARAM, LPARAM, LRESULT *);
};
#endif
