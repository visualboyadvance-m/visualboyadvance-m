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
#include <string.h>

#include <X11/extensions/Xvlib.h>

#define FOURCC_YUY2 0x32595559

namespace VBA
{

ScreenAreaXv::ScreenAreaXv(int _iWidth, int _iHeight, int _iScale) :
  ScreenArea(_iWidth, _iHeight, _iScale)
{
  XvAdaptorInfo *pAdaptors;
  unsigned int iNumAdaptors;
  XvImageFormatValues *pFormats;
  int iNumFormats;
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

  /* Try to find an RGB format */
  pFormats = XvListImageFormats(m_pDisplay, m_iXvPortId, &iNumFormats);
  
  m_iFormat = FOURCC_YUY2;
  
  for (int i = 0; i < iNumFormats; i++)
  {
    if (pFormats[i].id == 0x3 || pFormats[i].type == XvRGB)
    {
      /* Try to find a 32-bit mode */
      if (pFormats[i].bits_per_pixel == 32)
      {
        m_iFormat = pFormats[i].id;
      }
    }
  }

  Atom oAtom = XInternAtom(m_pDisplay, "XV_AUTOPAINT_COLORKEY", True);
  if (oAtom != None) 
    XvSetPortAttribute (m_pDisplay, m_iXvPortId, oAtom, 1);
  
  vUpdateSize();
}

ScreenAreaXv::~ScreenAreaXv()
{
  XShmDetach(m_pDisplay, &m_oShm);
}

void ScreenAreaXv::vDrawPixels(u8 * _puiData)
{
  GtkWidget *pDrawingArea = GTK_WIDGET(this->gobj());
  GdkGC *gc = pDrawingArea->style->bg_gc[GTK_WIDGET_STATE (pDrawingArea)];

  const int iSrcPitch = (m_iWidth + 4) * sizeof(u16);
  const int iDstPitch = m_iWidth * sizeof(u32) + 4;

  vRGB32toYUY2((unsigned char*)m_pXvImage->data, m_iWidth, m_iHeight, iSrcPitch,
               _puiData + iDstPitch, m_iWidth + 4, m_iHeight + 4, iDstPitch);

  gdk_display_sync(gtk_widget_get_display(pDrawingArea));

  XvShmPutImage(m_pDisplay,
                m_iXvPortId,
                GDK_WINDOW_XWINDOW (pDrawingArea->window),
                GDK_GC_XGC (gc),
                m_pXvImage,
                0, 0,
                m_iWidth, m_iHeight,
                0, 0,
                m_iAreaWidth, m_iAreaHeight,
                True);

  gdk_display_sync(gtk_widget_get_display(pDrawingArea));
}

void ScreenAreaXv::vDrawColor(u32 _uiColor)
{
}

void ScreenAreaXv::vUpdateSize()
{

  if (m_oShm.shmid)
  {
    XShmDetach(m_pDisplay, &m_oShm);
  }

  m_iAreaWidth  = m_iScale * m_iWidth;
  m_iAreaHeight = m_iScale * m_iHeight;
  
  m_pXvImage = XvShmCreateImage(m_pDisplay,
                              m_iXvPortId,
                              m_iFormat,  
                              0,
                              m_iWidth + 4,
                              m_iHeight + 4,
                              &m_oShm);
  
  m_oShm.shmid = shmget(IPC_PRIVATE, m_pXvImage->data_size, IPC_CREAT | 0777);
  m_oShm.shmaddr = (char *) shmat(m_oShm.shmid, 0, 0);
  m_oShm.readOnly = FALSE; 
  
  m_pXvImage->data = m_oShm.shmaddr;
  
  XShmAttach(m_pDisplay, &m_oShm);

  set_size_request(m_iAreaWidth, m_iAreaHeight);
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

} // namespace VBA
