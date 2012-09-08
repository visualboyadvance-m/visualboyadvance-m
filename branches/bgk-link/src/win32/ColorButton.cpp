#include "stdafx.h"
#include "vba.h"
#include "ColorButton.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

bool ColorButton::isRegistered = false;

/////////////////////////////////////////////////////////////////////////////
// ColorButton

ColorButton::ColorButton()
{
  color = 0;
  registerClass();
}

ColorButton::~ColorButton()
{
}


BEGIN_MESSAGE_MAP(ColorButton, CButton)
  //{{AFX_MSG_MAP(ColorButton)
  // NOTE - the ClassWizard will add and remove mapping macros here.
  //}}AFX_MSG_MAP
  END_MESSAGE_MAP()

  /////////////////////////////////////////////////////////////////////////////
// ColorButton message handlers

void ColorButton::PreSubclassWindow()
{
  SetWindowLong(m_hWnd, GWL_STYLE, GetStyle() | BS_OWNERDRAW);
  CWnd::PreSubclassWindow();
}

void ColorButton::DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct)
{
  ASSERT(lpDrawItemStruct);

  int r = (color & 0x1f) << 3;
  int g = (color & 0x3e0) >> 2;
  int b = (color & 0x7c00) >> 7;

  HDC dc = lpDrawItemStruct->hDC;
  UINT state = lpDrawItemStruct->itemState;
  RECT rect = lpDrawItemStruct->rcItem;

  SIZE margins;
  margins.cx = ::GetSystemMetrics(SM_CXEDGE);
  margins.cy = ::GetSystemMetrics(SM_CYEDGE);

  if(GetState() & BST_PUSHED)
    DrawEdge(dc, &rect, EDGE_SUNKEN, BF_RECT);
  else
    DrawEdge(dc, &rect, EDGE_RAISED, BF_RECT);

  InflateRect(&rect, -margins.cx, -margins.cy);

  HBRUSH br = CreateSolidBrush((state & ODS_DISABLED) ?
                               ::GetSysColor(COLOR_3DFACE) : RGB(r,g,b));

  FillRect(dc, &rect, br);

  if(state & ODS_FOCUS) {
    InflateRect(&rect, -1, -1);
    DrawFocusRect(dc, &rect);
  }

  DeleteObject(br);
}

void ColorButton::setColor(u16 c)
{
  color = c;
  Invalidate();
}

void ColorButton::registerClass()
{
  if(!isRegistered) {
    WNDCLASS wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_GLOBALCLASS;
    wc.lpfnWndProc = (WNDPROC)::DefWindowProc;
    wc.hInstance = AfxGetInstanceHandle();
    wc.hIcon = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH )GetStockObject(BLACK_BRUSH);
    wc.lpszMenuName = NULL;
    wc.lpszClassName = "VbaColorButton";
    AfxRegisterClass(&wc);
    isRegistered = true;
  }
}
