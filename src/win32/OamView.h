#pragma once
#include "BitmapControl.h"
#include "IUpdate.h"
#include "ResizeDlg.h"
#include "gba/Globals.h"
#include "resource.h"

// OamView dialog
class OamViewable {
public:
    BITMAPINFO bmpInfo;
    BitmapControl oamView;
    uint8_t* data;
    OamViewable(int index, CDialog* parent);
    int w;
    int h;
    ~OamViewable();
};

class OamView : public ResizeDlg, IUpdateListener {
    DECLARE_DYNAMIC(OamView)
public:
    void savePNG(const char* name);
    void saveBMP(const char* name);
    void render();
    void setAttributes(uint16_t a0, uint16_t a1, uint16_t a2);
    void paint();

    virtual ~OamView();
    OamView(CWnd* pParent = NULL); // standard constructor

    virtual void update();
    // Dialog Data
    enum { IDD = IDD_OAM_VIEW };

private:
    OamViewable* oamViews[128];
    BITMAPINFO bmpInfo;
    uint8_t* data_screen;
    BitmapControl oamScreen;
    BitmapControl oamPreview;
    void UpdateOAM(int index);
    int selectednumber;
    bool autoUpdate;

protected:
    virtual void DoDataExchange(CDataExchange* pDX); // DDX/DDV support

    virtual BOOL OnInitDialog();
    afx_msg void OnAutoUpdate();
    afx_msg void OnClose();
    afx_msg void OnSave();
    afx_msg LRESULT OnOAMClick(WPARAM wParam, LPARAM lParam);
    afx_msg void OnListDoubleClick();

    DECLARE_MESSAGE_MAP()
public:
};