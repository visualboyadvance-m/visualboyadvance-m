extern HMENU winResLoadMenu(LPCTSTR menuName);

extern int winResDialogBox(LPCTSTR boxName,
                           HWND parent,
                           DLGPROC dlgProc);

extern int winResDialogBox(LPCTSTR boxName,
                           HWND parent,
                           DLGPROC dlgProc,
                           LPARAM lParam);

extern CString winResLoadString(UINT id);
