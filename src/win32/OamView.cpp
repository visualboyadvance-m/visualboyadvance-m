// OamView.cpp : implementation file
//

#include "stdafx.h"
#include "OamView.h"
#include "afxdialogex.h"
#include "Util.h"
#include "vba.h"
#include "WinResUtil.h"
#include "FileDlg.h"
#include "Reg.h"

#include "../System.h"
#include "../gba/GBA.h"
#include "../gba/Globals.h"
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
// OAM_VIEW dialog

IMPLEMENT_DYNAMIC(OamView, CDialog)
OamViewable::OamViewable(int index, CDialog* parent)
{
	RECT position;
	int x = index % 16;
	int y = index / 16;
	position.left = 16 + 20 * x;
	position.right = 16 + 20 * x + 19;
	position.top = 16 + 20 * y;
	position.bottom = 16 + 20 * y + 19;
	oamView.Create("VbaBitmapControl", "OAM", WS_BORDER | WS_CHILD | WS_VISIBLE | WS_GROUP | WS_TABSTOP, position, parent, IDC_OAM1 + index);
	memset(&bmpInfo.bmiHeader, 0, sizeof(bmpInfo.bmiHeader));
	bmpInfo.bmiHeader.biSize = sizeof(bmpInfo.bmiHeader);
	bmpInfo.bmiHeader.biWidth = 64;
	bmpInfo.bmiHeader.biHeight = 64;
	bmpInfo.bmiHeader.biPlanes = 1;
	bmpInfo.bmiHeader.biBitCount = 24;
	bmpInfo.bmiHeader.biCompression = BI_RGB;
	w = 64;
	h = 64;
	data = (u8*)calloc(1, 3 * 64 * 64);
	oamView.setSize(64, 64);
	oamView.setData(data);
	oamView.setBmpInfo(&bmpInfo);
}
OamViewable::~OamViewable()
{
	free(data);
	data = NULL;
}

OamView::OamView(CWnd* pParent /*=NULL*/)
	: ResizeDlg(OamView::IDD, pParent)
{
	autoUpdate = false;
	selectednumber = 0;

	memset(&bmpInfo.bmiHeader, 0, sizeof(bmpInfo.bmiHeader));
	bmpInfo.bmiHeader.biSize = sizeof(bmpInfo.bmiHeader);
	bmpInfo.bmiHeader.biWidth = 240;
	bmpInfo.bmiHeader.biHeight = 160;
	bmpInfo.bmiHeader.biPlanes = 1;
	bmpInfo.bmiHeader.biBitCount = 24;
	bmpInfo.bmiHeader.biCompression = BI_RGB;
	data_screen = (u8 *)calloc(1, 3 * 240 * 160);
	oamScreen.setSize(240, 160);
	oamScreen.setData(data_screen);
	oamScreen.setBmpInfo(&bmpInfo);
	oamScreen.setStretch(false);
}

void OamView::update()
{
	paint();
}

void OamView::paint()
{
	if (oam == NULL || paletteRAM == NULL || vram == NULL)
		return;

	render();
	utilReadScreenPixels(data_screen, 240, 160);
	oamScreen.refresh();

	for (int i = 0; i < 128; i++)
	{
		oamViews[i]->oamView.setSize(oamViews[i]->w, oamViews[i]->h);
		oamViews[i]->oamView.refresh();
	}
	oamPreview.refresh();
}
void OamView::UpdateOAM(int index)
{
	u16 *sprites = &((u16 *)oam)[4 * index];
	u16 *spritePalette = &((u16 *)paletteRAM)[0x100];
	u8 *bmp = oamViews[index]->data;

	u16 a0 = *sprites++;
	u16 a1 = *sprites++;
	u16 a2 = *sprites++;

	int sizeY = 8;
	int sizeX = 8;

	switch (((a0 >> 12) & 0x0c) | (a1 >> 14)) {
	case 0:
		break;
	case 1:
		sizeX = sizeY = 16;
		break;
	case 2:
		sizeX = sizeY = 32;
		break;
	case 3:
		sizeX = sizeY = 64;
		break;
	case 4:
		sizeX = 16;
		break;
	case 5:
		sizeX = 32;
		break;
	case 6:
		sizeX = 32;
		sizeY = 16;
		break;
	case 7:
		sizeX = 64;
		sizeY = 32;
		break;
	case 8:
		sizeY = 16;
		break;
	case 9:
		sizeY = 32;
		break;
	case 10:
		sizeX = 16;
		sizeY = 32;
		break;
	case 11:
		sizeX = 32;
		sizeY = 64;
		break;
	default:
		return;
	}
	oamViews[index]->w = sizeX;
	oamViews[index]->h = sizeY;
	int sy = (a0 & 255);

	if (a0 & 0x2000) {
		int c = (a2 & 0x3FF);
		//          if((DISPCNT & 7) > 2 && (c < 512))
		//            return;
		int inc = 32;
		if (DISPCNT & 0x40)
			inc = sizeX >> 2;
		else
			c &= 0x3FE;

		for (int y = 0; y < sizeY; y++) {
			for (int x = 0; x < sizeX; x++) {
				u32 color = vram[0x10000 + (((c + (y >> 3) * inc) *
					32 + (y & 7) * 8 + (x >> 3) * 64 +
					(x & 7)) & 0x7FFF)];
				color = spritePalette[color];
				*bmp++ = ((color >> 10) & 0x1f) << 3;
				*bmp++ = ((color >> 5) & 0x1f) << 3;
				*bmp++ = (color & 0x1f) << 3;
			}
		}
	}
	else {
		int c = (a2 & 0x3FF);
		//      if((DISPCNT & 7) > 2 && (c < 512))
		//          continue;

		int inc = 32;
		if (DISPCNT & 0x40)
			inc = sizeX >> 3;
		int palette = (a2 >> 8) & 0xF0;
		for (int y = 0; y < sizeY; y++) {
			for (int x = 0; x < sizeX; x++) {
				u32 color = vram[0x10000 + (((c + (y >> 3) * inc) *
					32 + (y & 7) * 4 + (x >> 3) * 32 +
					((x & 7) >> 1)) & 0x7FFF)];
				if (x & 1)
					color >>= 4;
				else
					color &= 0x0F;

				color = spritePalette[palette + color];
				*bmp++ = ((color >> 10) & 0x1f) << 3;
				*bmp++ = ((color >> 5) & 0x1f) << 3;
				*bmp++ = (color & 0x1f) << 3;
			}
		}
	}
	if (selectednumber == index)
	{
		oamPreview.setBmpInfo(&oamViews[index]->bmpInfo);
		oamPreview.setData(oamViews[index]->data);
		oamPreview.setSize(sizeX, sizeY);
	}
}
void OamView::render()
{
	if (oam == NULL || paletteRAM == NULL || vram == NULL)
		return;
	for (int i = 0; i < 128; i++)
	{
		UpdateOAM(i);
	}
	u16 *sprites = &((u16 *)oam)[4 * selectednumber];
	u16 a0 = *sprites++;
	u16 a1 = *sprites++;
	u16 a2 = *sprites++;
	setAttributes(a0, a1, a2);
}

void OamView::setAttributes(u16 a0, u16 a1, u16 a2)
{
	CString buffer;

	int y = a0 & 255;
	int rot = a0 & 512;
	int mode = (a0 >> 10) & 3;
	int mosaic = a0 & 4096;
	int color = a0 & 8192;
	int duple = a0 & 1024;
	int shape = (a0 >> 14) & 3;
	int x = a1 & 511;
	int rotParam = (a1 >> 9) & 31;
	int flipH = a1 & 4096;
	int flipV = a1 & 8192;
	int size = (a1 >> 14) & 3;
	int tile = a2 & 1023;
	int prio = (a2 >> 10) & 3;
	int pal = (a2 >> 12) & 15;
	int sizeY = 8;
	int sizeX = 8;

	switch (((a0 >> 12) & 0x0c) | (a1 >> 14)) {
	case 0:
		break;
	case 1:
		sizeX = sizeY = 16;
		break;
	case 2:
		sizeX = sizeY = 32;
		break;
	case 3:
		sizeX = sizeY = 64;
		break;
	case 4:
		sizeX = 16;
		break;
	case 5:
		sizeX = 32;
		break;
	case 6:
		sizeX = 32;
		sizeY = 16;
		break;
	case 7:
		sizeX = 64;
		sizeY = 32;
		break;
	case 8:
		sizeY = 16;
		break;
	case 9:
		sizeY = 32;
		break;
	case 10:
		sizeX = 16;
		sizeY = 32;
		break;
	case 11:
		sizeX = 32;
		sizeY = 64;
		break;
	default:
		return;
	}
	oamScreen.setSelectedRectangle(x, y, sizeX, sizeY);

	CListBox* attributelist = ((CListBox*)GetDlgItem(IDC_OAMATTRIBUTELIST));
	attributelist->ResetContent();

	buffer.Format("OAM No: %d", selectednumber);
	attributelist->AddString(buffer);

	buffer.Format("Position: %d,%d", x, y);
	attributelist->AddString(buffer);

	buffer.Format("Tile No: %d", tile);
	attributelist->AddString(buffer);

	buffer.Format("Size: %dx%d", sizeX, sizeY);
	attributelist->AddString(buffer);

	buffer.Format("OAM Dat: %04X,%04X,%04X", a0, a1, a2);
	attributelist->AddString(buffer);

	buffer.Format("OAM Addr: %08X", 0x07000000 + selectednumber * 8);
	attributelist->AddString(buffer);

	buffer.Format("Tile Addr: %08X", 0x06010000 + tile * (color ? 0x40 : 0x20));
	attributelist->AddString(buffer);

	buffer.Format("Mode: %d", mode);
	attributelist->AddString(buffer);

	buffer.Format("Palette: %d", pal);
	attributelist->AddString(buffer);


	buffer.Format("Prio: %d", prio);
	attributelist->AddString(buffer);


	if (rot) {
		buffer.Format("RotP: %d", rotParam);
		attributelist->AddString(buffer);
	}
	else
		buffer.Empty();


	buffer.Empty();
	buffer += "Flag: ";
	if (rot)
		buffer += 'R';
	else buffer += ' ';
	if (!rot) {
		if (flipH)
			buffer += 'H';
		else
			buffer += ' ';
		if (flipV)
			buffer += 'V';
		else
			buffer += ' ';
	}
	else {
		buffer += ' ';
		buffer += ' ';
	}
	if (mosaic)
		buffer += 'M';
	else
		buffer += ' ';
	if (duple)
		buffer += 'D';
	else
		buffer += ' ';

	attributelist->AddString(buffer);

}

OamView::~OamView()
{
	for (int i = 0; i < 128; i++)
	{
		delete oamViews[i];
	}
}

void OamView::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);


	DDX_Control(pDX, IDC_OAMSCREEN, oamScreen);
	DDX_Control(pDX, IDC_OAMPREVIEW, oamPreview);

}

BEGIN_MESSAGE_MAP(OamView, CDialog)
	ON_BN_CLICKED(IDC_CLOSE, OnClose)
	ON_BN_CLICKED(IDC_SAVE, OnSave)
	ON_BN_DOUBLECLICKED(IDC_OAMATTRIBUTELIST, OnListDoubleClick)
	ON_BN_CLICKED(IDC_AUTO_UPDATE, OnAutoUpdate)
	ON_MESSAGE(WM_BITMAPCLICK, OnOAMClick)
END_MESSAGE_MAP()


// OamView message handlers
void OamView::saveBMP(const char *name)
{
	OamViewable* ov = oamViews[selectednumber];
	u8 writeBuffer[1024 * 3];

	FILE *fp = fopen(name, "wb");

	if (!fp) {
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

	u32 fsz = sizeof(bmpheader) + ov->w*ov->h * 3;
	utilPutDword(bmpheader.filesize, fsz);
	utilPutDword(bmpheader.dataoffset, 0x38);
	utilPutDword(bmpheader.headersize, 0x28);
	utilPutDword(bmpheader.width, ov->w);
	utilPutDword(bmpheader.height, ov->h);
	utilPutDword(bmpheader.planes, 1);
	utilPutDword(bmpheader.bitsperpixel, 24);
	utilPutDword(bmpheader.datasize, 3 * ov->w*ov->h);

	fwrite(&bmpheader, 1, sizeof(bmpheader), fp);

	u8 *b = writeBuffer;

	int sizeX = ov->w;
	int sizeY = ov->h;

	u8 *pixU8 = (u8 *)ov->data + 3 * ov->w*(ov->h - 1);
	for (int y = 0; y < sizeY; y++) {
		for (int x = 0; x < sizeX; x++) {
			*b++ = *pixU8++; // B
			*b++ = *pixU8++; // G
			*b++ = *pixU8++; // R
		}
		pixU8 -= 2 * 3 * ov->w;
		fwrite(writeBuffer, 1, 3 * ov->w, fp);

		b = writeBuffer;
	}

	fclose(fp);
}

void OamView::savePNG(const char *name)
{
	OamViewable* ov = oamViews[selectednumber];
	u8 writeBuffer[1024 * 3];

	FILE *fp = fopen(name, "wb");

	if (!fp) {
		systemMessage(MSG_ERROR_CREATING_FILE, "Error creating file %s", name);
		return;
	}

	png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,
		NULL,
		NULL,
		NULL);
	if (!png_ptr) {
		fclose(fp);
		return;
	}

	png_infop info_ptr = png_create_info_struct(png_ptr);

	if (!info_ptr) {
		png_destroy_write_struct(&png_ptr, NULL);
		fclose(fp);
		return;
	}

	if (setjmp(png_ptr->jmpbuf)) {
		png_destroy_write_struct(&png_ptr, NULL);
		fclose(fp);
		return;
	}

	png_init_io(png_ptr, fp);

	png_set_IHDR(png_ptr,
		info_ptr,
		ov->w,
		ov->h,
		8,
		PNG_COLOR_TYPE_RGB,
		PNG_INTERLACE_NONE,
		PNG_COMPRESSION_TYPE_DEFAULT,
		PNG_FILTER_TYPE_DEFAULT);

	png_write_info(png_ptr, info_ptr);

	u8 *b = writeBuffer;

	int sizeX = ov->w;
	int sizeY = ov->h;

	u8 *pixU8 = (u8 *)ov->data;
	for (int y = 0; y < sizeY; y++) {
		for (int x = 0; x < sizeX; x++) {
			int blue = *pixU8++;
			int green = *pixU8++;
			int red = *pixU8++;

			*b++ = red;
			*b++ = green;
			*b++ = blue;
		}
		png_write_row(png_ptr, writeBuffer);

		b = writeBuffer;
	}

	png_write_end(png_ptr, info_ptr);

	png_destroy_write_struct(&png_ptr, &info_ptr);

	fclose(fp);
}

void OamView::OnSave()
{
	if (rom != NULL)
	{
		CString captureBuffer;

		if (captureFormat == 0)
			captureBuffer = "oam.png";
		else
			captureBuffer = "oam.bmp";

		LPCTSTR exts[] = { ".png", ".bmp" };

		CString filter = theApp.winLoadFilter(IDS_FILTER_PNG);
		CString title = winResLoadString(IDS_SELECT_CAPTURE_NAME);

		FileDlg dlg(this,
			captureBuffer,
			filter,
			captureFormat ? 2 : 1,
			captureFormat ? "BMP" : "PNG",
			exts,
			"",
			title,
			true);

		if (dlg.DoModal() == IDCANCEL) {
			return;
		}
		captureBuffer = dlg.GetPathName();

		if (dlg.getFilterIndex() == 2)
			saveBMP(captureBuffer);
		else
			savePNG(captureBuffer);
	}
}


BOOL OamView::OnInitDialog()
{
	__super::OnInitDialog();
	DIALOG_SIZER_START(sz)
		DIALOG_SIZER_ENTRY(IDC_OAMATTRIBUTELIST, DS_MoveX)
		DIALOG_SIZER_ENTRY(IDC_OAMPREVIEW, DS_SizeX | DS_SizeY)
		DIALOG_SIZER_ENTRY(IDC_AUTO_UPDATE, DS_MoveX)
		DIALOG_SIZER_ENTRY(IDC_SAVE, DS_MoveX | DS_MoveY)
		DIALOG_SIZER_ENTRY(IDC_CLOSE, DS_MoveX | DS_MoveY)
		DIALOG_SIZER_END()
		for (int i = 0; i < 128; i++)
		{
			oamViews[i] = new OamViewable(i, this);
		}
	oamPreview.setBmpInfo(&oamViews[0]->bmpInfo);
	oamPreview.setData(oamViews[0]->data);
	SetData(sz,
		TRUE,
		HKEY_CURRENT_USER,
		"Software\\Emulators\\VisualBoyAdvance\\Viewer\\OamView",
		NULL);
	// TODO:  Add extra initialization here
	UpdateData(FALSE);
	paint();
	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

LRESULT OamView::OnOAMClick(WPARAM wParam, LPARAM lParam)
{
	if ((wParam >= IDC_OAM1) && (wParam <= IDC_OAM128))
	{
		selectednumber = wParam - IDC_OAM1;
		update();
	}
	return TRUE;
}

void OamView::OnAutoUpdate()
{
	autoUpdate = !autoUpdate;
	if (autoUpdate) {
		theApp.winAddUpdateListener(this);
	}
	else {
		theApp.winRemoveUpdateListener(this);
	}
}
void OamView::OnClose()
{
	theApp.winRemoveUpdateListener(this);

	DestroyWindow();
}

void OamView::OnListDoubleClick()
{
	//theApp.winRemoveUpdateListener(this);

	//DestroyWindow();
}