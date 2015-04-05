#pragma once
#include "IUpdate.h"
#include "ResizeDlg.h"
#include "resource.h"
#include "BitmapControl.h"
#include "gba/Globals.h"

// OamView dialog
class OamViewable
{
public:
	BITMAPINFO bmpInfo;
	BitmapControl oamView;
	u8* data;
	OamViewable(int index, CDialog* parent);
	int w;
	int h;
	~OamViewable();
};

class OamView : public ResizeDlg, IUpdateListener
{
	DECLARE_DYNAMIC(OamView)
public:
	void savePNG(const char *name);
	void saveBMP(const char *name);
	void render();
	void setAttributes(u16 a0, u16 a1, u16 a2);
	void paint();

	virtual ~OamView();
	OamView(CWnd* pParent = NULL);   // standard constructor

	virtual void update();
	// Dialog Data
	enum { IDD = IDD_OAM_VIEW };
private:
	OamViewable* oamViews[128];
	BITMAPINFO bmpInfo;
	u8* data_screen;
	BitmapControl oamScreen;
	BitmapControl oamPreview;
	void UpdateOAM(int index);
	int selectednumber;
	bool autoUpdate;

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	virtual BOOL OnInitDialog();
	afx_msg void OnAutoUpdate();
	afx_msg void OnClose();
	afx_msg void OnSave();
	afx_msg LRESULT OnOAMClick(WPARAM wParam, LPARAM lParam);
	afx_msg void OnListDoubleClick();

	DECLARE_MESSAGE_MAP()
public:
};