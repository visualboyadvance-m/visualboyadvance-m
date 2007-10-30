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

// GBPrinter.cpp : implementation file
//

#include "stdafx.h"
#include "vba.h"
#include "FileDlg.h"
#include "GBPrinterDlg.h"
#include "Reg.h"
#include "WinResUtil.h"

#include "../System.h"
#include "../NLS.h"
#include "../Util.h"

extern "C" {
#include <png.h>
}

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// GBPrinter dialog


GBPrinterDlg::GBPrinterDlg(CWnd* pParent /*=NULL*/)
  : CDialog(GBPrinterDlg::IDD, pParent)
{
  //{{AFX_DATA_INIT(GBPrinterDlg)
  m_scale = -1;
  //}}AFX_DATA_INIT
  bitmap = (BITMAPINFO *)bitmapHeader;
  
  bitmap->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bitmap->bmiHeader.biWidth = 160;
  bitmap->bmiHeader.biHeight = -144;
  bitmap->bmiHeader.biPlanes = 1;
  bitmap->bmiHeader.biBitCount = 8;
  bitmap->bmiHeader.biCompression = BI_RGB;
  bitmap->bmiHeader.biSizeImage = 160*144;
  bitmap->bmiHeader.biXPelsPerMeter = 0;
  bitmap->bmiHeader.biYPelsPerMeter = 0;
  bitmap->bmiHeader.biClrUsed = 4;
  bitmap->bmiHeader.biClrImportant = 4;
  bitmap->bmiColors[0].rgbBlue =
    bitmap->bmiColors[0].rgbGreen =
    bitmap->bmiColors[0].rgbRed =
    255;
  bitmap->bmiColors[0].rgbReserved = 0;
  bitmap->bmiColors[1].rgbBlue =
    bitmap->bmiColors[1].rgbGreen =
    bitmap->bmiColors[1].rgbRed =
    168;
  bitmap->bmiColors[1].rgbReserved = 0;
  bitmap->bmiColors[2].rgbBlue =
    bitmap->bmiColors[2].rgbGreen =
    bitmap->bmiColors[2].rgbRed =
    96;
  bitmap->bmiColors[2].rgbReserved = 0;
  bitmap->bmiColors[3].rgbBlue =
    bitmap->bmiColors[3].rgbGreen =
    bitmap->bmiColors[3].rgbRed =
    0;
  bitmap->bmiColors[3].rgbReserved = 0;  
}


void GBPrinterDlg::DoDataExchange(CDataExchange* pDX)
{
  CDialog::DoDataExchange(pDX);
  //{{AFX_DATA_MAP(GBPrinterDlg)
  DDX_Radio(pDX, IDC_1X, m_scale);
  //}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(GBPrinterDlg, CDialog)
  //{{AFX_MSG_MAP(GBPrinterDlg)
  ON_BN_CLICKED(ID_SAVE, OnSave)
  ON_BN_CLICKED(ID_PRINT, OnPrint)
  ON_BN_CLICKED(ID_OK, OnOk)
  ON_BN_CLICKED(IDC_1X, On1x)
  ON_BN_CLICKED(IDC_2X, On2x)
  ON_BN_CLICKED(IDC_3X, On3x)
  ON_BN_CLICKED(IDC_4X, On4x)
  ON_WM_PAINT()
  //}}AFX_MSG_MAP
  END_MESSAGE_MAP()

  /////////////////////////////////////////////////////////////////////////////
// GBPrinter message handlers

void GBPrinterDlg::saveAsBMP(const char *name)
{
  u8 writeBuffer[512 * 3];
  
  FILE *fp = fopen(name,"wb");

  if(!fp) {
    systemMessage(MSG_ERROR_CREATING_FILE, "Error creating file %s", name);
    return;
  }

  struct {
    u8 ident[2];
    u8 filesize[4];
    u8 reserved[4];
    u8 dataoffset[4];
    u8 headersize[4];
    u8 width[4];
    u8 height[4];
    u8 planes[2];
    u8 bitsperpixel[2];
    u8 compression[4];
    u8 datasize[4];
    u8 hres[4];
    u8 vres[4];
    u8 colors[4];
    u8 importantcolors[4];
    u8 pad[2];
  } bmpheader;
  memset(&bmpheader, 0, sizeof(bmpheader));

  bmpheader.ident[0] = 'B';
  bmpheader.ident[1] = 'M';

  u32 fsz = sizeof(bmpheader) + 160*144*3;
  utilPutDword(bmpheader.filesize, fsz);
  utilPutDword(bmpheader.dataoffset, 0x38);
  utilPutDword(bmpheader.headersize, 0x28);
  utilPutDword(bmpheader.width, 160);
  utilPutDword(bmpheader.height, 144);
  utilPutDword(bmpheader.planes, 1);
  utilPutDword(bmpheader.bitsperpixel, 24);
  utilPutDword(bmpheader.datasize, 160*144);

  fwrite(&bmpheader, 1, sizeof(bmpheader), fp);

  u8 *b = writeBuffer;
  u8 *data = (u8 *)bitmapData;
  u8 *p = data + (160*143);
  for(int y = 0; y < 144; y++) {
    for(int x = 0; x < 160; x++) {
      u8 c = *p++;
      
      *b++ = bitmap->bmiColors[c].rgbBlue;
      *b++ = bitmap->bmiColors[c].rgbGreen;
      *b++ = bitmap->bmiColors[c].rgbRed;
    }
    p -= 2*(160);
    fwrite(writeBuffer, 1, 3*160, fp);
    
    b = writeBuffer;
  }
  
  fclose(fp);
}


void GBPrinterDlg::saveAsPNG(const char *name)
{
  u8 writeBuffer[160 * 3];
  
  FILE *fp = fopen(name,"wb");

  if(!fp) {
    systemMessage(MSG_ERROR_CREATING_FILE, "Error creating file %s",
                  name);
    return;
  }
  
  png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,
                                                NULL,
                                                NULL,
                                                NULL);
  if(!png_ptr) {
    fclose(fp);
    return;
  }

  png_infop info_ptr = png_create_info_struct(png_ptr);
  
  if(!info_ptr) {
    png_destroy_write_struct(&png_ptr,NULL);
    fclose(fp);
    return;
  }
  
  if(setjmp(png_ptr->jmpbuf)) {
    png_destroy_write_struct(&png_ptr,NULL);
    fclose(fp);
    return;
  }

  png_init_io(png_ptr,fp);

  png_set_IHDR(png_ptr,
               info_ptr,
               160,
               144,
               8,
               PNG_COLOR_TYPE_RGB,
               PNG_INTERLACE_NONE,
               PNG_COMPRESSION_TYPE_DEFAULT,
               PNG_FILTER_TYPE_DEFAULT);

  png_write_info(png_ptr,info_ptr);

  u8 *b = writeBuffer;

  int sizeX = 160;
  int sizeY = 144;

  u8 *pixU8 = (u8 *)bitmapData;
  for(int y = 0; y < sizeY; y++) {
    for(int x = 0; x < sizeX; x++) {
      u8 c = *pixU8++;
      *b++ = bitmap->bmiColors[c].rgbRed;
      *b++ = bitmap->bmiColors[c].rgbGreen;
      *b++ = bitmap->bmiColors[c].rgbBlue;
    }
    png_write_row(png_ptr,writeBuffer);
        
    b = writeBuffer;
  }
  
  png_write_end(png_ptr, info_ptr);

  png_destroy_write_struct(&png_ptr, &info_ptr);

  fclose(fp);  
}


void GBPrinterDlg::OnSave() 
{
  CString captureBuffer;

  if(theApp.captureFormat == 0)
    captureBuffer = "printer.png";
  else
    captureBuffer = "printer.bmp";

  LPCTSTR exts[] = {".png", ".bmp" };

  CString filter = theApp.winLoadFilter(IDS_FILTER_PNG);
  CString title = winResLoadString(IDS_SELECT_CAPTURE_NAME);

  FileDlg dlg(this,
              captureBuffer,
              filter,
              theApp.captureFormat ? 2 : 1,
              theApp.captureFormat ? "BMP" : "PNG",
              exts,
              "",
              title,
              true);

  if(dlg.DoModal() == IDCANCEL) {
    return;
  }

  captureBuffer = dlg.GetPathName();

  if(dlg.getFilterIndex() == 2)
    saveAsBMP(captureBuffer);
  else
    saveAsPNG(captureBuffer);
}

void GBPrinterDlg::OnPrint() 
{
  CPrintDialog dlg(FALSE);

  dlg.m_pd.nFromPage = 1;
  dlg.m_pd.nToPage = 1;
  dlg.m_pd.nMinPage = 1;
  dlg.m_pd.nMaxPage = 1;
  dlg.m_pd.nCopies = 1;

  if(dlg.DoModal() == IDOK) {
    DOCINFO di;
    float fLogPelsX1 = 0;
    float fLogPelsX2 = 0;
    float fLogPelsY1 = 0;
    float fLogPelsY2 = 0;
    float fScaleX = 0, fScaleY = 0;    
    CDC *winDC = NULL;
    memset(&di, 0, sizeof(di));
    di.cbSize = sizeof(DOCINFO);
    CString docName = winResLoadString(IDS_POCKET_PRINTER);
    di.lpszDocName = docName;
    CDC dc;
    dc.Attach(dlg.GetPrinterDC());
    int nError = dc.StartDoc(&di);

    if(nError == SP_ERROR) {
      systemMessage(IDS_ERROR_ON_STARTDOC,"Error on StartDoc");
      goto error;
    }
    nError = dc.StartPage();
    if(nError <= 0) {
      systemMessage(IDS_ERROR_ON_STARTPAGE, "Error on StartPage");
      goto error;
    }

    winDC = GetDC();
    fLogPelsX1 = (float)winDC->GetDeviceCaps(LOGPIXELSX);
    fLogPelsY1 = (float)winDC->GetDeviceCaps(LOGPIXELSY);
    ReleaseDC(winDC);

    fLogPelsX2 = (float)dc.GetDeviceCaps(LOGPIXELSX);
    fLogPelsY2 = (float)dc.GetDeviceCaps(LOGPIXELSY);
    
    if(fLogPelsX1 > fLogPelsX2)
      fScaleX = fLogPelsX1 / fLogPelsX2;
    else
      fScaleX = fLogPelsX2 / fLogPelsX1;

    if(fLogPelsY1 > fLogPelsY2)
      fScaleY = fLogPelsY1 / fLogPelsY2;
    else
      fScaleY = fLogPelsY2 / fLogPelsY1;

    fScaleX *= (scale+1);
    fScaleY *= (scale+1);
    
    if(StretchDIBits(dc,
                     0,
                     0,
                     (int)((float)160*fScaleX),
                     (int)((float)144*fScaleY),
                     0,
                     0,
                     160,
                     144,
                     bitmapData,
                     bitmap,
                     DIB_RGB_COLORS,
                     SRCCOPY) == GDI_ERROR) {
      systemMessage(IDS_ERROR_PRINTING_ON_STRETCH,
                    "Error printing on StretchDIBits");
    }

    nError = dc.EndPage();

    if(nError <= 0) {
      systemMessage(IDS_ERROR_ON_ENDPAGE, "Error on EndPage");
      goto error;
    }

    nError = dc.EndDoc();

    if(nError <= 0)
      systemMessage(IDS_ERROR_ON_ENDDOC, "Error on EndDoc");
  error:
    dc.DeleteDC();
  }
}

void GBPrinterDlg::processData(u8 *data)
{
  for(int y = 0; y < 0x12; y++) {
    for(int x = 0; x < 0x14; x++) {
      for(int k = 0; k < 8; k++) {
        int a = *data++;
        int b = *data++;
        for(int j = 0; j < 8; j++) {
          int mask = 1 << (7-j);
          int c = 0;
          if(a & mask)
            c++;
          if(b & mask)
            c+=2;
          bitmapData[x*8+j + 160*(y*8+k)] = c;
        }
      }
    }
  }  
}


BOOL GBPrinterDlg::OnInitDialog() 
{
  CDialog::OnInitDialog();
  
  scale = regQueryDwordValue("printerScale", 0);
  if(scale < 0 || scale > 3)
    scale = 0;
  m_scale = scale;
  UpdateData(FALSE);
  
  CenterWindow();
  
  return TRUE;  // return TRUE unless you set the focus to a control
                // EXCEPTION: OCX Property Pages should return FALSE
}

void GBPrinterDlg::OnOk() 
{
  EndDialog(TRUE);
}

void GBPrinterDlg::On1x() 
{
  regSetDwordValue("printerScale", 0);
  scale = 0;
}

void GBPrinterDlg::On2x() 
{
  regSetDwordValue("printerScale", 1);
  scale = 1;
}

void GBPrinterDlg::On3x() 
{
  regSetDwordValue("printerScale", 2);
  scale = 2;
}

void GBPrinterDlg::On4x() 
{
  regSetDwordValue("printerScale", 3);
  scale = 3;
}

void GBPrinterDlg::OnPaint() 
{
  CPaintDC dc(this); // device context for painting
  
  RECT rect;
  CWnd *h = GetDlgItem(IDC_GB_PRINTER);
  h->GetWindowRect(&rect);
  POINT p;
  p.x = rect.left;
  p.y = rect.top;
  ScreenToClient((POINT *)&p);
  rect.left = p.x+1;
  rect.top = p.y+1;
  p.x = rect.right;
  p.y = rect.bottom;
  ScreenToClient((POINT *)&p);
  rect.right = p.x-1;
  rect.bottom = p.y-1;
  
  StretchDIBits(dc,
                rect.left,
                rect.top,
                rect.right - rect.left,
                rect.bottom - rect.top,
                0,
                0,
                160,
                144,
                bitmapData,
                bitmap,
                DIB_RGB_COLORS,
                SRCCOPY);
}

void systemGbPrint(u8 *data,
                   int pages,
                   int feed,
                   int palette,
                   int contrast)
{
  theApp.winCheckFullscreen();
  GBPrinterDlg printer;
  printer.processData(data);
  printer.DoModal();
}

