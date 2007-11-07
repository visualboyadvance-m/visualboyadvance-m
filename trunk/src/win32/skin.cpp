// =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//
// WINDOWS SKINNING TUTORIAL - by Vander Nunes - virtware.net
// This is the source-code that shows what is discussed in the tutorial.
// The code is simplified for the sake of clarity, but all the needed
// features for handling skinned windows is present. Please read
// the article for more information.
//
// skin.cpp   : CSkin class implementation
// 28/02/2002 : initial release.
//
// =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

#include "stdafx.h"
#include "skin.h"
#include <stdio.h>
#include "xImage.h"

#include "../System.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

// ----------------------------------------------------------------------------
// constructor 1 - use it when you have not already created the app window.
// this one will not subclass automatically, you must call Hook() and Enable()
// to subclass the app window and enable the skin respectively.
// will throw an exception if unable to initialize skin from resource.
// ----------------------------------------------------------------------------

CSkin::CSkin()
{
  // default starting values
  m_bHooked = false;
  m_OldWndProc = NULL;

  m_rect.top = 0;
  m_rect.bottom = 0;
  m_rect.right = 0;
  m_rect.left = 0;

  m_dOldStyle = 0;

  m_oldRect = m_rect;
  m_nButtons = 0;
  m_buttons = NULL;
}

// ----------------------------------------------------------------------------
// destructor - just call the destroyer
// ----------------------------------------------------------------------------
CSkin::~CSkin()
{
  Destroy();
}

HBITMAP CSkin::LoadImage(const char *filename)
{
  CxImage image;
  image.Load(filename);
  if(!image.IsValid()) {
    return NULL;
  }
  
  return image.MakeBitmap(NULL);
}

// ----------------------------------------------------------------------------
// Initialize the skin
// ----------------------------------------------------------------------------
bool CSkin::Initialize(const char *skinFile)
{
  // try to retrieve the skin data from resource.
  bool res = GetSkinData(skinFile);
  if(!res) 
    systemMessage(0, m_error);
  return res;
}  

// ----------------------------------------------------------------------------
// destroy skin resources and free allocated resources
// ----------------------------------------------------------------------------
void CSkin::Destroy()
{
  if (m_buttons) {
    delete[] m_buttons;
    m_buttons = NULL;
  }

  // unhook the window
  UnHook();

  // free bitmaps and device context
  if (m_dcSkin) { SelectObject(m_dcSkin, m_hOldBmp); DeleteDC(m_dcSkin); m_dcSkin = NULL; }
  if (m_hBmp) { DeleteObject(m_hBmp); m_hBmp = NULL; }

  // free skin region
  if (m_rgnSkin) { DeleteObject(m_rgnSkin); m_rgnSkin = NULL; }
  
}



// ----------------------------------------------------------------------------
// toggle skin on/off - must be Hooked() before attempting to enable skin.
// ----------------------------------------------------------------------------
bool CSkin::Enable(bool bEnable)
{
  // refuse to enable if there is no window subclassed yet.
  if (!Hooked()) return false;

  // toggle
  m_bEnabled = bEnable;

  // force window repainting
  InvalidateRect(m_hWnd, NULL, TRUE);

  return true;
}



// ----------------------------------------------------------------------------
// tell if the skinning is enabled
// ----------------------------------------------------------------------------
bool CSkin::Enabled()
{
  return m_bEnabled;
}



// ----------------------------------------------------------------------------
// hook a window
// ----------------------------------------------------------------------------
bool CSkin::Hook(CWnd *pWnd)
{
  // unsubclass any other window
  if (Hooked()) UnHook();

  // this will be our new subclassed window
  m_hWnd = (HWND)*pWnd;

  // --------------------------------------------------
  // change window style (get rid of the caption bar)
  // --------------------------------------------------
  LONG_PTR dwStyle = GetWindowLongPtr(m_hWnd, GWL_STYLE);
  m_dOldStyle = dwStyle;
  dwStyle &= ~(WS_CAPTION|WS_SIZEBOX);
  SetWindowLongPtr(m_hWnd, GWL_STYLE, dwStyle);

  RECT r;
  pWnd->GetWindowRect(&r);
  m_oldRect = r;
  pWnd->MoveWindow(r.left,
                   r.top,
                   m_iWidth,
                   m_iHeight,
                   FALSE);
  
  pWnd->SetMenu(NULL);
  
  if(m_rgnSkin != NULL)
    // set the skin region to the window
    pWnd->SetWindowRgn(m_rgnSkin, true);    

  // subclass the window procedure
  m_OldWndProc = (WNDPROC)SetWindowLongPtr( m_hWnd, GWLP_WNDPROC, (LONG_PTR)SkinWndProc );

  // store a pointer to our class instance inside the window procedure.
  if (!SetProp(m_hWnd, "skin", (void*)this))
    {
      // if we fail to do so, we just can't activate the skin.
      UnHook();
      return false;
    }

  
  // update flag
  m_bHooked = ( m_OldWndProc ? true : false );

  for(int i = 0; i < m_nButtons; i++) {
    RECT r;
    m_buttons[i].GetRect(r);
    m_buttons[i].CreateButton("", WS_VISIBLE, r, pWnd, 0);
  }
  
  // force window repainting
  RedrawWindow(NULL,NULL,NULL,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN);  

  // successful return if we're hooked.
  return m_bHooked;
}



// ----------------------------------------------------------------------------
// unhook the window
// ----------------------------------------------------------------------------
bool CSkin::UnHook()
{
  // just to be safe we'll check this
  WNDPROC OurWnd;

  // cannot unsubclass if there is no window subclassed
  // returns true anyways.
  if (!Hooked()) return true;
  
  if(m_rgnSkin != NULL)
    // remove the skin region from the window
    SetWindowRgn(m_hWnd, NULL, true);

  // unsubclass the window procedure
  OurWnd = (WNDPROC)SetWindowLongPtr( m_hWnd, GWLP_WNDPROC, (LONG_PTR)m_OldWndProc );

  // remove the pointer to our class instance, but if we fail we don't care.
  RemoveProp(m_hWnd, "skin");

  // update flag - if we can't get our window procedure address again,
  // we failed to unhook the window.
  m_bHooked = ( OurWnd ? false : true );

  SetWindowLongPtr(m_hWnd, GWL_STYLE, m_dOldStyle);

  RECT r;

  GetWindowRect(m_hWnd, &r);
  
  MoveWindow(m_hWnd,
             r.left,
             r.top,
             m_oldRect.right - m_oldRect.left,
             m_oldRect.bottom - m_oldRect.top,
             FALSE);
  
  // force window repainting
  RedrawWindow(NULL,NULL,NULL,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN);  

  // successful return if we're unhooked.
  return !m_bHooked;
}



// ----------------------------------------------------------------------------
// tell us if there is a window subclassed
// ----------------------------------------------------------------------------
bool CSkin::Hooked()
{
  return m_bHooked;
}



// ----------------------------------------------------------------------------
// return the skin bitmap width
// ----------------------------------------------------------------------------
int CSkin::Width()
{
  return m_iWidth;
}



// ----------------------------------------------------------------------------
// return the skin bitmap height
// ----------------------------------------------------------------------------
int CSkin::Height()
{
  return m_iHeight;
}



// ----------------------------------------------------------------------------
// return the skin device context
// ----------------------------------------------------------------------------
HDC CSkin::HDC()
{
  return m_dcSkin;
}

bool CSkin::ParseRect(char *buffer, RECT& rect)
{
  char *token = strtok(buffer, ",");

  if(token == NULL)
    return false;
  rect.left = atoi(token);

  token = strtok(NULL, ",");
  if(token == NULL)
    return false;
  rect.top = atoi(token);

  token = strtok(NULL, ",");
  if(token == NULL)
    return false;
  rect.right = rect.left + atoi(token);

  token = strtok(NULL, ",");
  if(token == NULL)
    return false;
  rect.bottom = rect.top + atoi(token);

  token = strtok(NULL, ",");
  if(token != NULL)
    return false;

  return true;
}

HRGN CSkin::LoadRegion(const char *rgn)
{
  // -------------------------------------------------
  // then, we retrieve the skin region from resource.
  // -------------------------------------------------
  FILE *f = fopen(rgn, "rb");
  if(!f) return NULL;
  
  fseek(f, 0, SEEK_END);
  int size = ftell(f);
  LPRGNDATA pSkinData = (LPRGNDATA)malloc(size);
  if(!pSkinData) {
    fclose(f);
    return NULL;
  }

  fseek(f, 0, SEEK_SET);
  
  fread(pSkinData, 1, size, f);
  
  fclose(f);
  
  // create the region using the binary data.
  HRGN r = ExtCreateRegion(NULL, size, pSkinData);
  
  // free the allocated resource
  free(pSkinData);
  
  return r;
}

// ----------------------------------------------------------------------------
// skin retrieval helper
// ----------------------------------------------------------------------------
bool CSkin::GetSkinData(const char *skinFile)
{
  // -------------------------------------------------
  // retrieve the skin bitmap from resource.
  // -------------------------------------------------

  char buffer[2048];

  if(!GetPrivateProfileString("skin", "image", "", buffer, 2048, skinFile)) {
    m_error = "Missing skin bitmap";
    return false;
  }
  CString bmpName = buffer;
  CString rgn = "";
  if(GetPrivateProfileString("skin", "region", "", buffer, 2048, skinFile)) {
    rgn = buffer;
  }

  if(!GetPrivateProfileString("skin", "draw", "", buffer, 2048, skinFile)) {
    m_error = "Missing draw rectangle";
    return false;
  }
  
  if(!ParseRect(buffer, m_rect)) {
    m_error = "Invalid draw rectangle";
    return false;
  }

  m_nButtons = GetPrivateProfileInt("skin", "buttons", 0, skinFile);

  if(m_nButtons) {
    m_buttons = new SkinButton[m_nButtons];
    for(int i = 0; i < m_nButtons; i++) {
      if(!ReadButton(skinFile, i))
        return false;
    }
  }
  
  CString path = skinFile;
  int index = path.ReverseFind('\\');
  if(index != -1) {
    path = path.Left(index+1);
  }

  bmpName = path + bmpName;
  if(strcmp(rgn, ""))
    rgn = path + rgn;

  m_hBmp = LoadImage(bmpName);

  if (!m_hBmp) {
    m_error = "Error loading skin bitmap " + bmpName;
    return false;
  }

  // get skin info
  BITMAP bmp;
  GetObject(m_hBmp, sizeof(bmp), &bmp);

  // get skin dimensions
  m_iWidth = bmp.bmWidth;
  m_iHeight = bmp.bmHeight;

  if(strcmp(rgn, "")) {
    m_rgnSkin = LoadRegion(rgn);
    if(m_rgnSkin == NULL) {
      m_error = "Error loading skin region " + rgn;
      return false;
    }
  }

  // -------------------------------------------------
  // well, things are looking good...
  // as a quick providence, just create and keep
  // a device context for our later blittings.
  // -------------------------------------------------

  // create a context compatible with the user desktop
  m_dcSkin = CreateCompatibleDC(0);
  if (!m_dcSkin) return false;

  // select our bitmap
  m_hOldBmp = (HBITMAP)SelectObject(m_dcSkin, m_hBmp);


  // -------------------------------------------------
  // done
  // -------------------------------------------------
  return true;
}

bool CSkin::ReadButton(const char *skinFile, int num)
{
  char buffer[2048];

  CString path = skinFile;
  int index = path.ReverseFind('\\');
  if(index != -1) {
    path = path.Left(index+1);
  }
  sprintf(buffer, "button-%d", num);
  CString name = buffer;
  
  if(!GetPrivateProfileString(name, "normal", "", buffer, 2048, skinFile)) {
    m_error = "Missing button bitmap for " + name;
    return false;
  }
  
  CString normalBmp = path + buffer;

  HBITMAP bmp = LoadImage(normalBmp);
  if(!bmp) {
    m_error = "Error loading button bitmap " + normalBmp;
    return false;
  }
  m_buttons[num].SetNormalBitmap(bmp);

  if(!GetPrivateProfileString(name, "down", "", buffer, 2048, skinFile)) {
    m_error = "Missing button down bitmap " + name;
    return false;
  }
  
  CString downBmp = path + buffer;

  bmp = LoadImage(downBmp);

  if (!bmp) {
    m_error = "Error loading button down bitmap " + downBmp;
    return false;
  }
  m_buttons[num].SetDownBitmap(bmp);

  if(GetPrivateProfileString(name, "over", "", buffer, 2048, skinFile)) {
    CString overBmp = path + buffer;

    bmp = LoadImage(overBmp);

    if (!bmp) {
      m_error = "Error loading button over bitmap " + overBmp;
      return false;
    }
    m_buttons[num].SetOverBitmap(bmp);
  }

  if(GetPrivateProfileString(name, "region", "", buffer, 2048, skinFile)) {
    CString region = path + buffer;
    
    HRGN rgn = LoadRegion(region);
    if(!rgn) {
      m_error = "Error loading button region " + region;
      return false;
    }
    m_buttons[num].SetRegion(rgn);
  }

  if(!GetPrivateProfileString(name, "id", "", buffer, 2048, skinFile)) {
    "Missing button ID for " + name;
    return false;
  }
  m_buttons[num].SetId(buffer);

  if(!GetPrivateProfileString(name, "rect", "", buffer, 2048, skinFile)) {
    m_error = "Missing button rectangle for " + name;
    return false;
  }
  
  RECT r;
  if(!ParseRect(buffer, r)) {
    m_error = "Invalid button rectangle for " + name;
    return false;
  }
  m_buttons[num].SetRect(r);

  return true;
}

// ------------------------------------------------------------------------
// Default skin window procedure.
// Here the class will handle WM_PAINT and WM_LBUTTONDOWN, originally sent
// to the application window, but now subclassed. Any other messages will
// just pass through the procedure and reach the original app procedure.
// ------------------------------------------------------------------------
LRESULT CALLBACK SkinWndProc(HWND hWnd, UINT uMessage, WPARAM wParam, LPARAM lParam)
{
  // we will need a pointer to the associated class instance
  // (it was stored in the window before, remember?)
  CSkin *pSkin = (CSkin*)GetProp(hWnd, _T("skin"));

  // to handle WM_PAINT
  PAINTSTRUCT ps;

  // if we fail to get our class instance, we can't handle anything.
  if (!pSkin) return DefWindowProc(hWnd,uMessage,wParam,lParam);

  switch(uMessage)
    {
    case WM_WINDOWPOSCHANGING:
      {
        LPWINDOWPOS pos = (LPWINDOWPOS)lParam;
        pos->cx = pSkin->Width();
        pos->cy = pSkin->Height();
        return 0L;
      }
      break;

    case WM_PAINT:
      {
        // ---------------------------------------------------------
        // here we just need to blit our skin
        // directly to the device context
        // passed by the painting message.
        // ---------------------------------------------------------
        BeginPaint(hWnd,&ps);

        // blit the skin
        BitBlt(ps.hdc,0,0,pSkin->Width(),pSkin->Height(),pSkin->HDC(),0,0,SRCCOPY);

        EndPaint(hWnd,&ps);
        break;
      }

    case WM_LBUTTONDOWN:
      {
        // ---------------------------------------------------------
        // this is a common trick for easy dragging of the window.
        // this message fools windows telling that the user is
        // actually dragging the application caption bar.
        // ---------------------------------------------------------
        SendMessage(hWnd, WM_NCLBUTTONDOWN, HTCAPTION,NULL);
        break;
      }

    }

  // ---------------------------------------------------------
  // call the default window procedure to keep things going.
  // ---------------------------------------------------------
  return CallWindowProc(pSkin->m_OldWndProc, hWnd, uMessage, wParam, lParam);
}
