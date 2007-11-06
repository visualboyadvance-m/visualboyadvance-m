// =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//
// WINDOWS SKINNING TUTORIAL - by Vander Nunes - virtware.net
// This is the source-code that shows what is discussed in the tutorial.
// The code is simplified for the sake of clarity, but all the needed
// features for handling skinned windows is present. Please read
// the article for more information.
//
// skin.h     : CSkin class declaration
// 28/02/2002 : initial release.
//
// =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-


#ifndef _SKIN_H_

#define _SKIN_H_

#include "skinButton.h"



// --------------------------------------------------------------------------
// The CSkin class will load the skin from a resource
// and subclass the associated window, so that the
// WM_PAINT message will be redirected to the provided
// window procedure. All the skin handling will be automatized.
// --------------------------------------------------------------------------

class CSkin
{

  // --------------------------------------------------------------------------
  // the skin window procedure, where the class
  // will handle WM_PAINT and WM_LBUTTONDOWN automatically.
  // --------------------------------------------------------------------------
  friend LRESULT CALLBACK SkinWndProc(HWND hWnd, UINT uMessage, WPARAM wParam, LPARAM lParam);

 private:

  // the associated window handle
  HWND      m_hWnd;

  // the old window procedure
  WNDPROC   m_OldWndProc;

  // skin region
  HRGN      m_rgnSkin;

  // the internal skin device context handle
  HDC       m_dcSkin;

  // bitmap and old bitmap from the device context
  HBITMAP   m_hBmp, m_hOldBmp;

  // skin dimensions
  int       m_iWidth, m_iHeight;

  // on|off toggle
  bool      m_bEnabled;

  // tell the class if it has a window subclassed.
  bool      m_bHooked;
      
  // skin retrieval helper
  bool      GetSkinData(const char *skin);

  RECT      m_rect;

  LONG_PTR     m_dOldStyle;
  RECT      m_oldRect;
  int       m_nButtons;
  SkinButton *m_buttons;
  CString m_error;

 public:

  // ----------------------------------------------------------------------------
  // constructor 1 - use it when you have not already created the app window.
  // this one will not subclass automatically, you must call Hook() to subclass.
  // will throw an exception if unable to initialize skin from resource.
  // ----------------------------------------------------------------------------

  CSkin();

  // ----------------------------------------------------------------------------
  // destructor - just call the destroyer
  // ----------------------------------------------------------------------------

  virtual ~CSkin();

  // ----------------------------------------------------------------------------
  // Initialize the skin
  // ----------------------------------------------------------------------------
  bool Initialize(const char *);

      
  // ----------------------------------------------------------------------------
  // destroy skin resources and free allocated resources
  // ----------------------------------------------------------------------------

  void Destroy();

  // ----------------------------------------------------------------------------
  // subclass a window.
  // ----------------------------------------------------------------------------

  bool    Hook(CWnd *pWnd);

  // ----------------------------------------------------------------------------
  // unsubclass the subclassed window.
  // ----------------------------------------------------------------------------

  bool    UnHook();

  // ----------------------------------------------------------------------------
  // tell us if we have a window subclassed.
  // ----------------------------------------------------------------------------

  bool    Hooked();

  // ----------------------------------------------------------------------------
  // toggle skin on/off.
  // ----------------------------------------------------------------------------

  bool    Enable(bool bEnable);

  // ----------------------------------------------------------------------------
  // tell if the skinning is enabled
  // ----------------------------------------------------------------------------

  bool    Enabled();

  // ----------------------------------------------------------------------------
  // return the skin bitmap width.
  // ----------------------------------------------------------------------------

  int     Width();

  // ----------------------------------------------------------------------------
  // return the skin bitmap height.
  // ----------------------------------------------------------------------------

  int     Height();

  // Return blit rect
  RECT    &GetBlitRect() { return m_rect; }

  // ----------------------------------------------------------------------------
  // return the skin device context.
  // ----------------------------------------------------------------------------

  HDC     HDC();

 private:
  HBITMAP LoadImage(const char *);
  bool ReadButton(const char *, int);
  static bool ParseRect(char *, RECT &);
  static HRGN LoadRegion(const char *);
};

#endif
