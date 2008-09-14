// VisualBoyAdvance - Nintendo Gameboy/GameboyAdvance (TM) emulator.
// Copyright (C) 1999-2003 Forgotten
// Copyright (C) 2004 Forgotten and the VBA development team

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or(at your option)
// any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

#include "screenarea-xvideo.h"

#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include <X11/extensions/Xvlib.h>

#include <cstring>

#define FOURCC_YUY2 0x32595559

namespace VBA
{

template<typename T> T min( T x, T y ) { return x < y ? x : y; }
template<typename T> T max( T x, T y ) { return x > y ? x : y; }

ScreenAreaXv::ScreenAreaXv(int _iWidth, int _iHeight, int _iScale) :
  ScreenArea(_iWidth, _iHeight, _iScale),
  m_puiPixels(0),
  m_puiDelta(0),
  m_iAreaTop(0),
  m_iAreaLeft(0)
{
  XvAdaptorInfo *pAdaptors;
  unsigned int iNumAdaptors;
  GdkWindow *pRoot;

  memset(&m_oShm, 0, sizeof(m_oShm));

  // Ugly bit of GTK+ to get the X display
  Glib::RefPtr<Gdk::Window> poWindow = get_root_window();
  GdkWindow *pWindow = poWindow->gobj();

  m_pDisplay = gdk_x11_drawable_get_xdisplay(GDK_DRAWABLE(pWindow));

  Glib::RefPtr<Gdk::Screen> poScreen = get_screen();
  Glib::RefPtr<Gdk::Window> poRoot = poScreen->get_root_window();
  pRoot = poRoot->gobj();

  m_iXvPortId = -1;
  XvQueryAdaptors(m_pDisplay,
                    GDK_WINDOW_XWINDOW(pRoot),
                    &iNumAdaptors,
                    &pAdaptors);

  for (unsigned int i = 0; i < iNumAdaptors; i++)
  {
    if (pAdaptors[i].type & XvInputMask &&
        pAdaptors[i].type & XvImageMask)
    {
      m_iXvPortId = pAdaptors[i].base_id;
    }
  }

  XvFreeAdaptorInfo(pAdaptors);

  if (m_iXvPortId < 0)
  {
    fprintf (stderr, "Could not open Xv output port.\n");
    throw std::exception();
  }

  m_iFormat = FOURCC_YUY2;

  /* FIXME: RGB mode is disabled for now. Someone with a graphic card that allows
            RGB overlays should try to fix it.

  XvImageFormatValues *pFormats;
  int iNumFormats;

  // Try to find an RGB format
  pFormats = XvListImageFormats(m_pDisplay, m_iXvPortId, &iNumFormats);

  for (int i = 0; i < iNumFormats; i++)
  {
    if (pFormats[i].id == 0x3 || pFormats[i].type == XvRGB)
    {
      // Try to find a 32-bit mode
      if (pFormats[i].bits_per_pixel == 32)
      {
        m_iFormat = pFormats[i].id;
      }
    }
  }
  */

  int iNumAttributes;
  XvAttribute *pAttr = XvQueryPortAttributes(m_pDisplay, m_iXvPortId, &iNumAttributes);

  for (int iAttr = 0; iAttr < iNumAttributes; iAttr++)
  {
    if (!strcmp(pAttr[iAttr].name, "XV_AUTOPAINT_COLORKEY"))
    {
      Atom oAtom = XInternAtom(m_pDisplay, "XV_AUTOPAINT_COLORKEY", True);
      if (oAtom != None)
        XvSetPortAttribute(m_pDisplay, m_iXvPortId, oAtom, 1);

      break;
    }
  }

  vUpdateSize();
}

ScreenAreaXv::~ScreenAreaXv()
{
  XShmDetach(m_pDisplay, &m_oShm);

  if (m_puiPixels != NULL)
  {
    delete[] m_puiPixels;
  }

  if (m_puiDelta != NULL)
  {
    delete[] m_puiDelta;
  }
}

void ScreenAreaXv::vDrawPixels(u8 * _puiData)
{
  GtkWidget *pDrawingArea = GTK_WIDGET(this->gobj());
  GdkGC *gc = pDrawingArea->style->bg_gc[GTK_WIDGET_STATE (pDrawingArea)];
  u32 * puiPixels = (u32 *)_puiData;

  const int iSrcPitch = m_iWidth * sizeof(u32) + 4;
  const int iScaledWidth = m_iFilterScale * m_iWidth;
  const int iScaledHeight = m_iFilterScale * m_iHeight;
  const int iScaledPitch = iScaledWidth * sizeof(u32) + 4;
  const int iDstPitch = (iScaledWidth + 4) * sizeof(u16);

  if (m_vFilterIB != NULL)
  {
    m_vFilterIB(_puiData + iSrcPitch,
                iSrcPitch,
                m_iWidth,
                m_iHeight);
  }

  if (m_vFilter2x)
  {
    m_vFilter2x(_puiData + iSrcPitch,
                iSrcPitch,
                m_puiDelta,
                (u8 *)m_puiPixels,
                iScaledPitch,
                m_iWidth,
                m_iHeight);
    puiPixels = m_puiPixels;
  }

  vRGB32toYUY2((unsigned char*)m_pXvImage->data, iScaledWidth, iScaledHeight, iDstPitch,
               (u8 *)puiPixels + iScaledPitch, iScaledWidth + 4, iScaledHeight + 4, iScaledPitch);

  gdk_display_sync(gtk_widget_get_display(pDrawingArea));

  XvShmPutImage(m_pDisplay,
                m_iXvPortId,
                GDK_WINDOW_XWINDOW (pDrawingArea->window),
                GDK_GC_XGC (gc),
                m_pXvImage,
                0, 0,
                iScaledWidth, iScaledHeight,
                m_iAreaLeft, m_iAreaTop,
                m_iAreaWidth + 4, m_iAreaHeight + 4,
                True);

  gdk_display_sync(gtk_widget_get_display(pDrawingArea));
}

void ScreenAreaXv::vDrawBlackScreen()
{
  modify_bg(get_state(), Gdk::Color("black"));
}

void ScreenAreaXv::vUpdateSize()
{
  const int iScaledWidth = m_iFilterScale * m_iWidth;
  const int iScaledHeight = m_iFilterScale * m_iHeight;

  if (m_puiPixels != NULL)
  {
    delete[] m_puiPixels;
  }

  if (m_puiDelta != NULL)
  {
    delete[] m_puiDelta;
  }

  if (m_oShm.shmid)
  {
    XShmDetach(m_pDisplay, &m_oShm);
  }

  vOnWidgetResize();

  m_pXvImage = XvShmCreateImage(m_pDisplay,
                              m_iXvPortId,
                              m_iFormat,
                              0,
                              iScaledWidth + 4,
                              iScaledHeight + 4,
                              &m_oShm);

  m_oShm.shmid = shmget(IPC_PRIVATE, m_pXvImage->data_size, IPC_CREAT | 0777);
  m_oShm.shmaddr = (char *) shmat(m_oShm.shmid, 0, 0);
  m_oShm.readOnly = FALSE;

  m_pXvImage->data = m_oShm.shmaddr;

  XShmAttach(m_pDisplay, &m_oShm);

  m_puiPixels = new u32[iScaledWidth * iScaledHeight];
  m_puiDelta = new u8[(m_iWidth + 2) * (m_iHeight + 2) * 4];
  memset(m_puiPixels, 0, iScaledWidth * iScaledHeight * sizeof(u32));
  memset(m_puiDelta, 255, (m_iWidth + 2) * (m_iHeight + 2) * 4);

  set_size_request(m_iScale * m_iWidth, m_iScale* m_iHeight);
}

void ScreenAreaXv::vRGB32toYUY2 (unsigned char* dest_ptr,
                                 int            dest_width,
                                 int            dest_height,
                                 int            dest_pitch,
                                 unsigned char* src_ptr,
                                 int            src_width,
                                 int            src_height,
                                 int            src_pitch)
{
    unsigned char* pSrc    = NULL;
    unsigned char* pDst    = NULL;
    int            iR      = 0;
    int            iG      = 0;
    int            iB      = 0;
    int            iY      = 0;
    int            iU      = 0;
    int            iV      = 0;


    /* Run through each row */
    for (int y = 0; y < src_height; y++)
    {
        /* Get the src and dst row pointers */
        pSrc = src_ptr  + y * src_pitch;
        pDst = dest_ptr + y * dest_pitch;
        /* Loop along each row */
        for (int x = 0; x < src_width; x++)
        {
            /* Convert RGB to YUV, using the following fixed-point formula:
             *
             * Y = (r *  2104 + g *  4130 + b *  802 + 4096 +  131072) >> 13
             * U = (r * -1214 + g * -2384 + b * 3598 + 4096 + 1048576) >> 13
             * V = (r *  3598 + g * -3013 + b * -585 + 4096 + 1048576) >> 13
             */
#if defined(_WINDOWS)
            iR = pSrc[2];
            iG = pSrc[1];
            iB = pSrc[0];
#else
            iR = pSrc[0];
            iG = pSrc[1];
            iB = pSrc[2];
#endif
            iY = (iR *  2104 + iG * 4130 + iB *  802 + 4096 +  131072) >> 13;
            iU = (iR * -1214 - iG * 2384 + iB * 3598 + 4096 + 1048576) >> 13;
            iV = (iR *  3598 - iG * 3013 - iB *  585 + 4096 + 1048576) >> 13;
            /* Write out the Y */
            pDst[0] = (unsigned char) iY;
            /* If we are even, write out U. If odd, write V */
            pDst[1] = (unsigned char) ((x & 1) ? iV : iU);
            /* Advance the src pointer */
            pSrc += 4;
            /* Advance the dst pointer */
            pDst += 2;
        }
    }
}

void ScreenAreaXv::vOnWidgetResize()
{
  double dAspectRatio = m_iWidth / (double)m_iHeight;

  m_iAreaHeight = min<int>(get_height(), get_width() / dAspectRatio);
  m_iAreaWidth = min<int>(get_width(), get_height() * dAspectRatio);

  m_iAreaTop = (get_height() - m_iAreaHeight) / 2;
  m_iAreaLeft = (get_width() - m_iAreaWidth) / 2;
}

bool ScreenAreaXv::on_configure_event(GdkEventConfigure * event)
{
  vOnWidgetResize();

  return true;
}

} // namespace VBA
