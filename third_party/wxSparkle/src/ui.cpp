/*
 *  This file is part of wxSparkle, a wxWidgets port of WinSparkle
 *  (https://winsparkle.org)
 *
 *  Copyright (C) 2009-2026 Vaclav Slavik
 *  SPDX-License-Identifier: MIT (see COPYING)
 */

#include "ui.h"
#include "settings.h"
#include "error.h"
#include "platform.h"
#include "updatechecker.h"
#include "updatedownloader.h"
#include "appcontroller.h"
#include "utils.h"

#include <wx/app.h>
#include <wx/bmpbndl.h>
#include <wx/button.h>
#include <wx/cmdline.h>
#include <wx/dcclient.h>
#include <wx/dialog.h>
#include <wx/display.h>
#include <wx/evtloop.h>
#include <wx/filename.h>
#include <wx/gauge.h>
#if wxUSE_HYPERLINKCTRL
    #include <wx/hyperlink.h>
#endif
#include <wx/iconbndl.h>
#include <wx/image.h>
#include <wx/imagpng.h>
#include <wx/intl.h>
#include <wx/msgdlg.h>
#include <wx/mstream.h>
#include <wx/panel.h>
#include <wx/settings.h>
#include <wx/sizer.h>
#include <wx/statbmp.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/timer.h>
#include <wx/translation.h>
#include <wx/uilocale.h>
#include <wx/utils.h>

#include <algorithm>
#include <atomic>
#include <functional>
#include <initializer_list>
#include <vector>

#if WXSPARKLE_USE_WEBVIEW && wxUSE_WEBVIEW
    #include <wx/webview.h>
    #if defined(__WXMSW__) && wxUSE_WEBVIEW_IE
        #include <wx/msw/webview_ie.h>
    #endif
    #define HAVE_WEBVIEW 1
#else
    #define HAVE_WEBVIEW 0
#endif

#if wxUSE_HTML
    #include <wx/html/htmlwin.h>
#endif

// On a phone or tablet there is no room for a window beside the app's: the
// dialogs take the app's whole part of the screen, as dialogs do there, and fit
// their text and buttons to its width instead of to fixed widths. (Can be set
// from the command line to try that layout out on a computer.)
#ifndef WXSPARKLE_FILL_SCREEN
    #if defined(__ANDROID__)
        #define WXSPARKLE_FILL_SCREEN 1
    #else
        #define WXSPARKLE_FILL_SCREEN 0
    #endif
#endif

// Generated at build time by cmake/bin2c.cmake:
#include "wxsparkle_icons.h"
#include "wxsparkle_translations.h"

#ifdef __WXMSW__
    #include <wx/msw/private.h> // wxFindWinFromHandle()
    #include <windows.h>
    #include <commctrl.h>
    #include <shellapi.h>
    #include <tchar.h>
#else
    #include <sys/stat.h>
#endif

#include <cmath>
#include <climits>
#include <functional>
#include <memory>
#include <string>
#include <vector>


#if !wxCHECK_VERSION(3,2,0)
#error "wxWidgets >= 3.2 is required to compile this code"
#endif

// Hack to build with Fedora 20 MinGW
#if defined(__WXMSW__) && !defined(ERROR_RESOURCE_ENUM_USER_STOP)
#define ERROR_RESOURCE_ENUM_USER_STOP                      15106
#endif

#ifdef __WXOSX__
// implemented in platform_mac.mm
wxImage wxsparkle_GetMacApplicationIcon(int sizeInPixels);
#endif

namespace winsparkle
{

#ifdef _WIN32
void *UI::ms_hInstance = NULL;
#endif

/*--------------------------------------------------------------------------*
                               translations
 *--------------------------------------------------------------------------*/

// WinSparkle's translations are embedded into the library (see
// cmake/bin2c.cmake) and loaded into a private message catalog, so that they
// work regardless of whether and how the host application uses wxTranslations.

namespace
{

std::unique_ptr<wxMsgCatalog> g_catalog;
// the host's idea of the screen area the dialogs may take, if it has one
std::atomic<win_sparkle_screen_area_callback_t> g_screenAreaCallback{nullptr};
wxLayoutDirection             g_layoutDirection = wxLayout_Default;
bool                          g_translationsInitialized = false;

const wxsparkle_blob *FindEmbeddedTranslation(const wxString& name)
{
    for ( const wxsparkle_blob *b = wxsparkle_translations; b->name; ++b )
    {
        if ( name == b->name )
            return b;
    }
    return NULL;
}

// Maps a language tag (e.g. "pt-BR", "de_DE.UTF-8", "zh-Hans-CN", "sr-Latn")
// to the name of an available embedded catalog. Returns empty string if there
// is none; sets isEnglish if the tag is English, which doesn't need any.
wxString MatchTranslation(wxString tag, bool *isEnglish)
{
    *isEnglish = false;

    tag.Replace("-", "_");
    wxString modifier;
    if ( tag.Contains("@") )
    {
        modifier = tag.AfterFirst('@').Lower();
        tag = tag.BeforeFirst('@');
    }
    if ( tag.Contains(".") )
        tag = tag.BeforeFirst('.');

    const wxArrayString parts = wxSplit(tag, '_');
    if ( parts.empty() )
        return wxString();

    const wxString lang = parts[0].Lower();
    if ( lang.empty() || lang == "c" || lang == "posix" )
        return wxString();
    if ( lang == "en" )
    {
        *isEnglish = true;
        return wxString();
    }

    wxString region, script;
    for ( size_t i = 1; i < parts.size(); i++ )
    {
        if ( parts[i].length() == 4 )
            script = parts[i].Lower();
        else if ( parts[i].length() == 2 || parts[i].length() == 3 )
            region = parts[i].Upper();
    }

    wxArrayString candidates;
    if ( lang == "zh" )
    {
        const bool traditional = script == "hant" || region == "TW" || region == "HK" || region == "MO";
        candidates.push_back(traditional ? "zh_TW" : "zh_CN");
    }
    if ( lang == "sr" && (script == "latn" || modifier == "latin") )
        candidates.push_back("sr@latin");
    if ( !region.empty() )
        candidates.push_back(lang + "_" + region);
    candidates.push_back(lang);

    // e.g. "pt" -> "pt_PT", "gl" -> "gl_ES" if there's no generic catalog
    for ( const wxsparkle_blob *b = wxsparkle_translations; b->name; ++b )
    {
        const wxString name(b->name);
        if ( name.BeforeFirst('_').BeforeFirst('@') == lang )
            candidates.push_back(name);
    }

    for ( const auto& c : candidates )
    {
        if ( FindEmbeddedTranslation(c) )
            return c;
    }

    return wxString();
}

void InitTranslations()
{
    if ( g_translationsInitialized )
        return;
    g_translationsInitialized = true;

    std::vector<wxString> preferred;

    const Settings::Lang langset = Settings::GetLanguage();
    if ( !langset.lang.empty() )
    {
        preferred.push_back(wxString::FromUTF8(langset.lang));
    }
#ifdef __WXMSW__
    else if ( langset.langid != 0 )
    {
        auto f_LCIDToLocaleName = LOAD_DYNAMIC_FUNC(LCIDToLocaleName, kernel32);
        if ( f_LCIDToLocaleName )
        {
            WCHAR strNameBuffer[LOCALE_NAME_MAX_LENGTH];
            if ( f_LCIDToLocaleName(MAKELCID(langset.langid, SORT_DEFAULT), strNameBuffer, LOCALE_NAME_MAX_LENGTH, 0) != 0 )
                preferred.push_back(wxString(strNameBuffer));
        }
    }
#endif

    if ( preferred.empty() )
    {
        // UI language of the host application, if it set any...
        const wxString current = wxUILocale::GetCurrent().GetName();
        if ( !current.empty() )
            preferred.push_back(current);

        // ...and user's preferred languages
#if wxCHECK_VERSION(3,3,0)
        for ( const auto& l : wxUILocale::GetPreferredUILanguages() )
            preferred.push_back(l);
#else
        preferred.push_back(wxUILocale::GetLanguageCanonicalName(wxUILocale::GetSystemLanguage()));
#endif
    }

    for ( const auto& p : preferred )
    {
        bool isEnglish;
        const wxString match = MatchTranslation(p, &isEnglish);
        if ( isEnglish )
            break; // English is preferred over available translations

        if ( match.empty() )
            continue;

        const wxsparkle_blob *blob = FindEmbeddedTranslation(match);
        wxScopedCharBuffer data = wxScopedCharBuffer::CreateNonOwned(
                                      reinterpret_cast<const char*>(blob->data), blob->size);
        g_catalog.reset(wxMsgCatalog::CreateFromData(data, "winsparkle"));
        if ( !g_catalog )
            continue;

        const wxLanguageInfo *info = wxUILocale::FindLanguageInfo(match.BeforeFirst('@'));
        if ( info )
            g_layoutDirection = info->LayoutDirection;
        break;
    }
}

// Translates a string using WinSparkle's own catalog.
wxString Tr(const char *s)
{
    const wxString str = wxString::FromUTF8(s);
    if ( g_catalog )
    {
        const wxString *t = g_catalog->GetString(str);
        if ( t && !t->empty() )
            return *t;
    }
    return str;
}

} // anonymous namespace

// Use our catalog for all strings in this file.
#undef _
#define _(s) Tr(s)


/*--------------------------------------------------------------------------*
                                  helpers
 *--------------------------------------------------------------------------*/

namespace
{

// name given to all WinSparkle top level windows, used to tell them apart
// from host application's windows
const char DIALOG_NAME[] = "WinSparkleDialog";

// shows/hides layout element
void DoShowElement(wxWindow *w, bool show)
{
    w->GetContainingSizer()->Show(w, show, true/*recursive*/);
}

void DoShowElement(wxSizer *s, bool show)
{
    s->ShowItems(show);
}

#define SHOW(c)    DoShowElement(c, true)
#define HIDE(c)    DoShowElement(c, false)


void EnsurePNGHandler()
{
#if wxUSE_LIBPNG
    if ( !wxImage::FindHandler(wxBITMAP_TYPE_PNG) )
        wxImage::AddHandler(new wxPNGHandler);
#endif
    // else: embedded icons can't be loaded, see GetLargestEmbeddedIcon()
}

wxImage LoadEmbeddedPNG(const wxsparkle_blob *blob)
{
    EnsurePNGHandler();
    wxMemoryInputStream stream(blob->data, blob->size);
    wxImage img;
    {
        wxLogNull noLog;
        img.LoadFile(stream, wxBITMAP_TYPE_PNG);
    }
    return img;
}

// Returns embedded WinSparkle icon of the exact given size, if available
wxImage GetEmbeddedIcon(int size)
{
    const wxString name = wxString::Format("%d", size);
    for ( const wxsparkle_blob *b = wxsparkle_icons; b->name; ++b )
    {
        if ( name == b->name )
            return LoadEmbeddedPNG(b);
    }
    return wxImage();
}

#ifdef __WXMSW__
HICON LoadNamedHICON(HMODULE module, const wchar_t *iconName, int size);
#endif

// Returns the largest embedded WinSparkle icon
// (unused where the dialogs fill the screen)
[[maybe_unused]] wxImage GetLargestEmbeddedIcon()
{
    wxImage best;
    for ( const wxsparkle_blob *b = wxsparkle_icons; b->name; ++b )
    {
        wxImage img = LoadEmbeddedPNG(b);
        if ( img.IsOk() && (!best.IsOk() || img.GetWidth() > best.GetWidth()) )
            best = img;
    }

#ifdef __WXMSW__
    // wxWidgets built without PNG support: use the DLL's icon resource
    if ( !best.IsOk() && UI::GetDllHINSTANCE() )
    {
        HICON hIcon = LoadNamedHICON(static_cast<HMODULE>(UI::GetDllHINSTANCE()), L"UpdateAvailable", 256);
        if ( hIcon )
        {
            wxIcon icon;
            icon.CreateFromHICON(static_cast<WXHICON>(hIcon));
            wxBitmap bmp;
            bmp.CopyFromIcon(icon);
            best = bmp.ConvertToImage();
        }
    }
#endif

    return best;
}

// Creates a bundle of logical size dipSize x dipSize for all common DPI
// scale factors, using exact-size embedded images where available.
// (unused where the dialogs fill the screen)
[[maybe_unused]] wxBitmapBundle MakeIconBundle(const wxImage& source, int dipSize, bool preferEmbedded)
{
    wxVector<wxBitmap> bitmaps;
    if ( !source.IsOk() )
        return wxBitmapBundle();

    for ( double scale : { 1.0, 1.5, 2.0, 3.0 } )
    {
        const int px = int(std::lround(dipSize * scale));
        wxImage img;
        if ( preferEmbedded )
            img = GetEmbeddedIcon(px);
        if ( !img.IsOk() )
        {
            img = source;
            if ( img.GetWidth() != px || img.GetHeight() != px )
                img.Rescale(px, px, wxIMAGE_QUALITY_HIGH);
        }
        bitmaps.push_back(wxBitmap(img));
    }

    return wxBitmapBundle::FromBitmaps(bitmaps);
}

// Window (title bar) icons
wxIconBundle GetWinSparkleIconBundle()
{
    wxIconBundle icons;
    for ( const wxsparkle_blob *b = wxsparkle_icons; b->name; ++b )
    {
        wxImage img = LoadEmbeddedPNG(b);
        if ( !img.IsOk() || img.GetWidth() > 256 )
            continue;
        wxIcon icon;
        icon.CopyFromBitmap(wxBitmap(img));
        icons.AddIcon(icon);
    }
    return icons;
}


#ifdef __WXMSW__

HICON LoadNamedHICON(HMODULE module, const wchar_t *iconName, int size)
{
    HICON hIcon = NULL;

    auto f_LoadIconWithScaleDown = LOAD_DYNAMIC_FUNC(LoadIconWithScaleDown, comctl32);
    if (f_LoadIconWithScaleDown)
    {
        if (FAILED(f_LoadIconWithScaleDown(module, iconName, size, size, &hIcon)))
            hIcon = NULL;
    }

    if (!hIcon)
    {
        hIcon = static_cast<HICON>(LoadImageW(module, iconName, IMAGE_ICON, size, size, LR_DEFAULTCOLOR));
    }

    return hIcon;
}

BOOL CALLBACK GetFirstIconProc(HMODULE, LPCWSTR, LPWSTR lpszName, LONG_PTR lParam)
{
    if (IS_INTRESOURCE(lpszName))
        *((LPWSTR*)lParam) = lpszName;
    else
        *((LPWSTR*)lParam) = _wcsdup(lpszName);
    return FALSE; // stop on the first icon found
}

// Returns the application's (i.e. main .exe's) first icon
wxImage GetHostApplicationIcon()
{
    HMODULE hParentExe = GetModuleHandle(NULL);
    if ( !hParentExe )
        return wxImage();

    LPWSTR iconName = 0;
    EnumResourceNamesW(hParentExe, (LPCWSTR)RT_GROUP_ICON, GetFirstIconProc, (LONG_PTR)&iconName);

    if ( !iconName ||
         (GetLastError() != ERROR_SUCCESS && GetLastError() != ERROR_RESOURCE_ENUM_USER_STOP) )
        return wxImage();

    HICON hIcon = LoadNamedHICON(hParentExe, iconName, 256);

    if ( !IS_INTRESOURCE(iconName) )
        free(iconName);

    if ( !hIcon )
        return wxImage();

    wxIcon icon;
    icon.CreateFromHICON(static_cast<WXHICON>(hIcon));
    if ( !icon.IsOk() )
        return wxImage();

    wxBitmap bmp;
    bmp.CopyFromIcon(icon);
    return bmp.ConvertToImage();
}

#else // !__WXMSW__

// Returns the biggest visible top level window of the host app.
wxTopLevelWindow *GetHostApplicationMainWindow()
{
    wxTopLevelWindow *best = NULL;
    long bestArea = 0;

    for ( wxWindowList::compatibility_iterator node = wxTopLevelWindows.GetFirst();
          node;
          node = node->GetNext() )
    {
        wxTopLevelWindow *tlw = wxDynamicCast(node->GetData(), wxTopLevelWindow);
        if ( !tlw || tlw->GetName() == DIALOG_NAME || tlw->IsBeingDeleted() )
            continue;
        if ( !tlw->IsShown() || tlw->IsIconized() )
            continue;

        const wxSize sz = tlw->GetSize();
        const long area = long(sz.x) * long(sz.y);
        if ( area > bestArea )
        {
            bestArea = area;
            best = tlw;
        }
    }

    return best;
}

// (unused where the dialogs fill the screen)
[[maybe_unused]] wxImage GetHostApplicationIcon()
{
#ifdef __WXOSX__
    // bundle's icon (only used if the app has one, to avoid generic icon)
    wxImage img = wxsparkle_GetMacApplicationIcon(256);
    if ( img.IsOk() )
        return img;
#endif

    wxTopLevelWindow *win = wxTheApp ? wxDynamicCast(wxTheApp->GetTopWindow(), wxTopLevelWindow) : NULL;
    if ( !win || win->GetName() == DIALOG_NAME )
        win = GetHostApplicationMainWindow();
    if ( !win )
        return wxImage();

    const wxIconBundle& icons = win->GetIcons();
    if ( icons.IsEmpty() )
        return wxImage();

    wxIcon icon = icons.GetIcon(wxSize(256, 256), wxIconBundle::FALLBACK_NEAREST_LARGER);
    if ( !icon.IsOk() )
        return wxImage();

    wxBitmap bmp;
    bmp.CopyFromIcon(icon);
    return bmp.ConvertToImage();
}

#endif // __WXMSW__/!__WXMSW__


#ifdef __WXMSW__

struct EnumProcessWindowsData
{
    DWORD process_id = 0;
    wxRect biggest;
};

BOOL CALLBACK EnumProcessWindowsCallback(HWND handle, LPARAM lParam)
{
    EnumProcessWindowsData& data = *reinterpret_cast<EnumProcessWindowsData*>(lParam);

    if (!IsWindowVisible(handle) || IsIconic(handle))
        return TRUE;

    DWORD process_id = 0;
    GetWindowThreadProcessId(handle, &process_id);
    if (data.process_id != process_id)
        return TRUE; // another process' window

    if (GetWindow(handle, GW_OWNER) != 0)
        return TRUE; // child, not main, window

    // ignore WinSparkle's own windows
    wxWindow *wxwin = wxFindWinFromHandle(handle);
    if (wxwin && wxwin->GetName() == DIALOG_NAME)
        return TRUE;

    RECT rwin;
    GetWindowRect(handle, &rwin);
    if (MonitorFromRect(&rwin, MONITOR_DEFAULTTONULL) == NULL)
        return TRUE; // window is offscreen

    wxRect r(rwin.left, rwin.top, rwin.right - rwin.left, rwin.bottom - rwin.top);
    if (r.width * r.height > data.biggest.width * data.biggest.height)
        data.biggest = r;

    return TRUE;
}

// works for host applications that don't use wxWidgets, too
wxRect GetHostApplicationScreenArea()
{
    EnumProcessWindowsData data;
    data.process_id = GetCurrentProcessId();
    EnumWindows(EnumProcessWindowsCallback, (LPARAM)&data);
    return data.biggest;
}

#else

wxRect GetHostApplicationScreenArea()
{
    wxTopLevelWindow *win = GetHostApplicationMainWindow();
    return win ? win->GetScreenRect() : wxRect();
}

#endif // __WXMSW__


// (unused where the dialogs fill the screen)
[[maybe_unused]] wxPoint GetWindowOriginSoThatItFits(int display, const wxRect& windowRect)
{
    wxPoint pos = windowRect.GetTopLeft();
    wxRect desktop = wxDisplay(display).GetClientArea();
    if (!desktop.Contains(windowRect))
    {
        if (pos.x < desktop.x)
            pos.x = desktop.x;
        if (pos.y < desktop.y)
            pos.y = desktop.y;
       wxPoint bottomRightDiff = windowRect.GetBottomRight() - desktop.GetBottomRight();
       if (bottomRightDiff.x > 0)
           pos.x -= bottomRightDiff.x;
       if (bottomRightDiff.y > 0)
           pos.y -= bottomRightDiff.y;
    }
    return pos;
}

#if WXSPARKLE_FILL_SCREEN
// The area the dialogs take: the host application's main window, which the
// system sizes to the app's part of the screen -- unlike the display's client
// area, that leaves out the status, navigation and action bars -- limited to
// the display.
wxRect GetFillScreenArea(const wxWindow *win)
{
    int display = win ? wxDisplay::GetFromWindow(win) : wxNOT_FOUND;
    if ( display == wxNOT_FOUND )
        display = 0;

    wxRect area = wxDisplay(unsigned(display)).GetClientArea();
    const wxRect host = GetHostApplicationScreenArea();
    if ( !host.IsEmpty() && area.Intersects(host) )
        area.Intersect(host);

    // Neither of those need be what the app really has: Qt for Android counts
    // the action bar above the app in both, and a dialog that tall has its
    // bottom -- the buttons -- off the screen. The host knows better.
    if ( win_sparkle_screen_area_callback_t cb = g_screenAreaCallback )
    {
        int x = 0, y = 0, w = 0, h = 0;
        if ( cb(&x, &y, &w, &h) && w > 0 && h > 0 )
        {
            const wxRect usable(x, y, w, h);
            if ( area.Intersects(usable) )
                area.Intersect(usable);
            else
                area = usable;
        }
    }
    return area;
}
#endif // WXSPARKLE_FILL_SCREEN

void CenterWindowOnHostApplication(wxTopLevelWindow *win)
{
#if WXSPARKLE_FILL_SCREEN
    win->SetSize(GetFillScreenArea(win));
#else
    // find application's biggest window:
    const wxRect host = GetHostApplicationScreenArea();

    if (host.IsEmpty())
    {
        // no parent window to center on, so center on the screen
        win->Center();
        return;
    }

    // and center WinSparkle on it:
    wxSize winsz = win->GetSize();
    wxPoint pos(host.x + (host.width - winsz.x) / 2,
                host.y + (host.height - winsz.y) / 2);

    // make sure the window is fully visible:
    int display = wxDisplay::GetFromPoint(wxPoint(host.x + host.width / 2,
                                                  host.y + host.height / 2));
    if (display != wxNOT_FOUND)
    {
        pos = GetWindowOriginSoThatItFits(display, wxRect(pos, winsz));
    }

    win->Move(pos);
#endif // !WXSPARKLE_FILL_SCREEN
}

void EnsureWindowIsFullyVisible(wxTopLevelWindow *win)
{
#if WXSPARKLE_FILL_SCREEN
    // already sized to the area, it only has to be at its origin
    win->Move(GetFillScreenArea(win).GetTopLeft());
#else
    int display = wxDisplay::GetFromWindow(win);
    if (display == wxNOT_FOUND)
        return;

    wxPoint pos = GetWindowOriginSoThatItFits(display, win->GetRect());
    win->Move(pos);
#endif
}


enum WebBrowserSourceKind
{
    WebBrowserSourceNone,
    WebBrowserSourceRemote,
    WebBrowserSourceInline
};

struct WebBrowserState
{
    WebBrowserSourceKind SourceKind = WebBrowserSourceNone;
    bool PageLoaded = false;

    void Reset(WebBrowserSourceKind kind)
    {
        PageLoaded = false;
        SourceKind = kind;
    }
};


#if WXSPARKLE_CLASSIC_MAC
// Launches the downloaded update file on classic Mac OS: there are no
// executable installers that could be downloaded (an application needs its
// resource fork), so it's an archive or a disk image, opened by the Finder.
bool LaunchUpdateFile(const std::wstring& file, const std::string& arguments)
{
    if ( !arguments.empty() )
        LogWarning("Installer arguments are ignored on classic Mac OS.");

    return ClassicMacOpenFile(wxString(file));
}
#elif !defined(__WXMSW__)
// Launches the downloaded update file on non-Windows platforms:
// - executables (no extension, .AppImage, .run, .sh, .bin) are executed with
//   the installer arguments
// - anything else (.dmg, .pkg, .zip, .deb, .rpm, ...) is opened with the
//   default application, e.g. Installer.app or the system package manager
bool LaunchUpdateFile(const std::wstring& file, const std::string& arguments)
{
    const std::string path = ToUtf8(file);
    const wxFileName fn(wxString::FromUTF8(path));
    const wxString ext = fn.GetExt().Lower();

    const bool isExecutable = ext.empty() || ext == "appimage" || ext == "run" ||
                              ext == "sh" || ext == "bin";

    if ( isExecutable )
    {
        if ( chmod(path.c_str(), S_IRWXU) != 0 )
        {
            LogError("Failed to make the update file executable");
            return false;
        }

        std::vector<std::string> args;
        args.push_back(path);
        const wxArrayString extra = wxCmdLineParser::ConvertStringToArgs(
                                        wxString::FromUTF8(arguments), wxCMD_LINE_SPLIT_UNIX);
        for ( const auto& a : extra )
            args.push_back(a.utf8_string());

        std::vector<const char*> argv;
        for ( const auto& a : args )
            argv.push_back(a.c_str());
        argv.push_back(NULL);

        // run in its own process group, so that it survives the host
        // application's shutdown
        const long pid = wxExecute(argv.data(), wxEXEC_ASYNC | wxEXEC_MAKE_GROUP_LEADER);
        return pid > 0;
    }

    if ( !arguments.empty() )
        LogWarning("Installer arguments are ignored for update files that aren't executables.");

    return wxLaunchDefaultApplication(fn.GetFullPath());
}
#endif // !__WXMSW__

} // anonymous namespace


/*--------------------------------------------------------------------------*
                       Base class for WinSparkle dialogs
 *--------------------------------------------------------------------------*/

class WinSparkleDialog : public wxDialog
{
protected:
    WinSparkleDialog();

    void UpdateLayout();
    static void SetBoldFont(wxWindow *win);
    static void SetHeadingFont(wxWindow *win);

    // enable/disable resizing of the dialog
    void MakeResizable(bool resizable = true);

public:
    // to be called after layout changes
    void FinishLayoutChange();

protected:
    // The width, in pixels, for text and controls of the main area that
    // should be `width` DIPs wide: that, or less where the screen is narrower.
    int FitWidth(int width) const;

    // Sets the text of a label wrapped at FitWidth(width), and gives it the
    // size of the wrapped text as its minimum. The text is kept, so that it
    // can be wrapped again when the dialog's width changes.
    void SetWrappedLabel(wxStaticText *label, const wxString& text, int width);

    // Called by FinishLayoutChange() with the width of the main area where
    // the dialog fills the screen, to rearrange what doesn't fit in it.
    virtual void ArrangeForWidth(int WXUNUSED(width)) {}

    // Lays `buttons` out as a row in `sizer` (by calling `addRow`), or, when
    // the row would be wider than `width`, one per line, full width, in the
    // order given. Returns true if they are stacked.
    bool ArrangeButtons(wxBoxSizer *sizer,
                        std::initializer_list<wxButton*> buttons,
                        int rowGaps, int width,
                        const std::function<void()>& addRow);

    // sizer for the main area of the dialog (to the right of the icon)
    wxSizer      *m_mainAreaSizer;

    // whether the user may resize the dialog
    bool          m_resizable;

    // High DPI support:
    #define PX(x) FromDIP(x)

    static const int MESSAGE_AREA_WIDTH = 300;
    // the border around the main area, on either side
    static const int MAIN_AREA_BORDER = 10;

private:
    struct WrappedLabel
    {
        wxStaticText *label;
        wxString      text;
        int           width;
    };

    void WrapLabel(const WrappedLabel& w);

    std::vector<WrappedLabel> m_wrappedLabels;

#if WXSPARKLE_FILL_SCREEN
    // lays the dialog out again when the screen area changes (rotation)
    void OnIdleCheckScreenArea(wxIdleEvent& event);

    // the area the dialog was last laid out for
    wxRect        m_screenArea;
#endif
};


// Locks window updates to reduce flicker. Redoes layout in dtor.
struct LayoutChangesGuard
{
    LayoutChangesGuard(WinSparkleDialog *win) : m_win(win)
    {
        m_win->Freeze();
    }

    ~LayoutChangesGuard()
    {
        m_win->FinishLayoutChange();
        m_win->Refresh();
        EnsureWindowIsFullyVisible(m_win);
        m_win->Thaw();
        m_win->Update();
    }

    WinSparkleDialog *m_win;
};


WinSparkleDialog::WinSparkleDialog()
    : wxDialog(NULL, wxID_ANY, _("Software Update"),
               wxDefaultPosition, wxDefaultSize,
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER | wxDIALOG_NO_PARENT,
               DIALOG_NAME),
      m_resizable(true)
{
    // use RTL layout for RTL translations, even if the host app doesn't
    if ( g_layoutDirection != wxLayout_Default )
        SetLayoutDirection(g_layoutDirection);

    SetIcons(GetWinSparkleIconBundle());

    wxSizer *topSizer = new wxBoxSizer(wxHORIZONTAL);

#if !WXSPARKLE_FILL_SCREEN
    // (No icon where the dialog fills the screen: the app is the only thing
    // on it already, and the icon's column is wanted for the text.)

    // Load the dialog box icon: the application icon will be used, if
    // available, otherwise, the standard WinSparkle icon will be used.
    wxBitmapBundle bigIcon = MakeIconBundle(GetHostApplicationIcon(), 48, false);
    if ( !bigIcon.IsOk() )
        bigIcon = MakeIconBundle(GetLargestEmbeddedIcon(), 48, true);

    topSizer->Add
              (
                  new wxStaticBitmap(this, wxID_ANY, bigIcon),
                  wxSizerFlags(0).Border(wxALL, PX(10))
              );
#endif // !WXSPARKLE_FILL_SCREEN

    m_mainAreaSizer = new wxBoxSizer(wxVERTICAL);
    topSizer->Add(m_mainAreaSizer, wxSizerFlags(1).Expand().Border(wxALL, PX(MAIN_AREA_BORDER)));

    SetSizer(topSizer);

    MakeResizable(false);

#if WXSPARKLE_FILL_SCREEN
    Bind(wxEVT_IDLE, &WinSparkleDialog::OnIdleCheckScreenArea, this);
#endif
}


int WinSparkleDialog::FitWidth(int width) const
{
    width = PX(width);
#if WXSPARKLE_FILL_SCREEN
    // The dialog is as wide as the screen: text and controls take all of it,
    // rather than leave most of a wide (landscape) screen empty and wrap the
    // text into more lines than the height has room for.
    const int avail = GetFillScreenArea(this).width - 2 * PX(MAIN_AREA_BORDER);
    if ( avail > 0 )
        width = avail;
#endif
    return width;
}


void WinSparkleDialog::SetWrappedLabel(wxStaticText *label, const wxString& text, int width)
{
    for ( auto& w : m_wrappedLabels )
    {
        if ( w.label == label )
        {
            w.text = text;
            w.width = width;
            WrapLabel(w);
            return;
        }
    }

    m_wrappedLabels.push_back({label, text, width});
    WrapLabel(m_wrappedLabels.back());
}


void WinSparkleDialog::WrapLabel(const WrappedLabel& w)
{
    w.label->SetLabel(w.text);
    // Wrap() does nothing if called with the same width as the last time,
    // even though the label changed: reset it first, so that the new label
    // is wrapped too. (This matters for ports that only show the label as
    // given, e.g. wxUniversal used on classic Mac OS.)
    w.label->Wrap(-1);
    w.label->Wrap(FitWidth(w.width));

    // Wrapping at requested width may fail, e.g. for languages that don't use
    // spaces (Chinese, Japanese etc.). In that case, we want the window to grow
    // wider than 'width' and avoid truncating text:
    w.label->InvalidateBestSize();
    w.label->SetMinSize(wxDefaultSize);
    w.label->SetMinSize(w.label->GetBestSize());
}


bool WinSparkleDialog::ArrangeButtons(wxBoxSizer *sizer,
                                      std::initializer_list<wxButton*> buttons,
                                      int rowGaps, int width,
                                      const std::function<void()>& addRow)
{
    int rowWidth = rowGaps;
    for ( wxButton *b : buttons )
    {
        if ( b->IsShown() )
            rowWidth += b->GetBestSize().x;
    }

    const bool stack = rowWidth > width;

    // detach, without destroying, the buttons; spacers go
    sizer->Clear(false);

    if ( stack )
    {
        sizer->SetOrientation(wxVERTICAL);
        bool first = true;
        for ( wxButton *b : buttons )
        {
            sizer->Add(b, wxSizerFlags().Expand().Border(wxTOP, first ? 0 : PX(10)));
            first = false;
        }
    }
    else
    {
        sizer->SetOrientation(wxHORIZONTAL);
        addRow();
    }

    return stack;
}


#if WXSPARKLE_FILL_SCREEN
void WinSparkleDialog::OnIdleCheckScreenArea(wxIdleEvent& event)
{
    event.Skip();

    if ( IsShown() && GetFillScreenArea(this) != m_screenArea )
    {
        LayoutChangesGuard guard(this);
    }
}
#endif


void WinSparkleDialog::MakeResizable(bool resizable)
{
    m_resizable = resizable;

    bool is_resizable = (GetWindowStyleFlag() & wxRESIZE_BORDER) != 0;
    if ( is_resizable == resizable )
        return;

    ToggleWindowStyle(wxRESIZE_BORDER);
    Refresh(); // to paint the gripper
}


void WinSparkleDialog::FinishLayoutChange()
{
    // reset size constraints from the previous state first
    SetMaxSize(wxDefaultSize);
    SetMinSize(wxDefaultSize);

#if WXSPARKLE_FILL_SCREEN
    m_screenArea = GetFillScreenArea(this);

    for ( const auto& w : m_wrappedLabels )
        WrapLabel(w);
    ArrangeForWidth(m_screenArea.width - 2 * PX(MAIN_AREA_BORDER));

    SetSize(m_screenArea);
    Layout();
#else
    Layout();
    GetSizer()->SetSizeHints(this);

    // Not all ports support changing wxRESIZE_BORDER of an existing window,
    // so also prevent resizing using size hints:
    if ( !m_resizable )
        SetMaxSize(GetSize());
#endif
}


void WinSparkleDialog::UpdateLayout()
{
    FinishLayoutChange();
}


void WinSparkleDialog::SetBoldFont(wxWindow *win)
{
    wxFont f = wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT);
    f.SetWeight(wxFONTWEIGHT_BOLD);

    win->SetFont(f);
}


void WinSparkleDialog::SetHeadingFont(wxWindow *win)
{
    wxFont f = wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT);

#ifdef __WXMSW__
    int winver;
    wxGetOsVersion(&winver);
    if ( winver >= 6 ) // Windows Vista, 7 or newer
    {
        // 9pt is base font, 12pt is for "Main instructions". See
        // http://msdn.microsoft.com/en-us/library/aa511282%28v=MSDN.10%29.aspx
        f.SetPointSize(f.GetPointSize() + 3);
        if ( !wxSystemSettings::GetAppearance().IsDark() )
            win->SetForegroundColour(wxColour(0x00, 0x33, 0x99));
    }
    else // Windows XP/2000
    {
        f.SetWeight(wxFONTWEIGHT_BOLD);
        f.SetPointSize(f.GetPointSize() + 1);
    }
#else
    // bold, slightly larger text, as used by macOS Sparkle and GNOME dialogs
    f.SetWeight(wxFONTWEIGHT_BOLD);
    f.SetPointSize(f.GetPointSize() + 1);
#endif

    win->SetFont(f);
}


/*--------------------------------------------------------------------------*
                      Window for communicating with the user
 *--------------------------------------------------------------------------*/

enum
{
    ID_SKIP_VERSION = wxID_HIGHEST + 1,
    ID_REMIND_LATER,
    ID_INSTALL,
    ID_RUN_INSTALLER
};


class UpdateDialog : public WinSparkleDialog
{
public:
    UpdateDialog();

    // changes state into "checking for updates"
    void StateCheckingUpdates();
    // change state into "no updates found"
    void StateNoUpdateFound(bool installAutomatically);
    // change state into "update error"
    void StateUpdateError(ErrorCode err);
    // change state into "a new version is available"
    void StateUpdateAvailable(const Appcast& info, bool installAutomatically);
    // change state into "downloading update"
    void StateDownloading();
    // update download progress
    void DownloadProgress(size_t downloaded, size_t total);
    // change state into "update downloaded"
    void StateUpdateDownloaded(const std::wstring& updateFile, const std::string &installerArguments);

    // closes the window immediately, without notifying the application
    void ShutDown();

private:
    void EnablePulsing(bool enable);
    void OnTimer(wxTimerEvent& event);
    void OnCloseButton(wxCommandEvent& event);
    void OnClose(wxCloseEvent& event);

    void OnSkipVersion(wxCommandEvent&);
    void OnRemindLater(wxCommandEvent&);
    void OnInstall(wxCommandEvent&);

    void OnRunInstaller(wxCommandEvent&);

    bool RunInstaller();

    void StopDownloader();

    void SetMessage(const wxString& text, int width = MESSAGE_AREA_WIDTH);
    void SetHeading(const wxString& text);
    void ShowReleaseNotes(const Appcast& info);
    void ShowReleaseNotesFallback(const Appcast& info);

    // the size of the release notes area, also its minimum size
    wxSize GetReleaseNotesSize() const;

    void ArrangeForWidth(int width) override;
    // lays the update buttons out in a row, as on a computer's screen
    void AddUpdateButtonsRow();

private:
    wxTimer       m_timer;
    wxSizer      *m_buttonSizer;
    wxStaticText *m_heading;
    wxStaticText *m_message;
    wxGauge      *m_progress;
    wxStaticText *m_progressLabel;
    wxButton     *m_skipVersionButton;
    wxButton     *m_remindLaterButton;
    wxButton     *m_closeButton;
    wxSizer      *m_closeButtonSizer;
    wxButton     *m_runInstallerButton;
    wxSizer      *m_runInstallerButtonSizer;
    wxButton     *m_installButton;
    wxBoxSizer   *m_updateButtonsSizer;
    wxSizer      *m_releaseNotesSizer;
    wxPanel      *m_browserParent;
#if HAVE_WEBVIEW
    wxWebView    *m_webBrowser;
#endif

    // current appcast data (only valid after StateUpdateAvailable())
    Appcast m_appcast;
    // current update file (only valid after StateUpdateDownloaded)
    std::wstring m_updateFile;
    // space separated arguments to update file (only valid after StateUpdateDownloaded)
    std::string m_installerArguments;
    // downloader (only valid between OnInstall and OnUpdateDownloaded)
    UpdateDownloader* m_downloader;
    // whether the update should be installed without prompting the user
    bool m_installAutomatically;
    // whether an error occurred (used to properly call NotifyUpdateCancelled)
    bool m_errorOccurred;
    // whether window closure was updater-initiated (i.e. not caused by user)
    bool m_closeInitiatedByUpdater;
    // browser state in process of loading the release notes
    WebBrowserState m_webBrowserState;
    // scaling of download sizes so that they fit into wxGauge's int range
    unsigned m_progressShift;

    static const int RELNOTES_WIDTH = 460;
    static const int RELNOTES_HEIGHT = 200;
};


UpdateDialog::UpdateDialog()
    : m_timer(this),
      m_downloader(NULL),
      m_progressShift(0)
{
    m_installAutomatically = false;
    m_errorOccurred = false;
    m_closeInitiatedByUpdater = false;

    m_heading = new wxStaticText(this, wxID_ANY, "");
    SetHeadingFont(m_heading);
    m_mainAreaSizer->Add(m_heading, wxSizerFlags(0).Expand().Border(wxBOTTOM, PX(10)));

    m_message = new wxStaticText(this, wxID_ANY, "",
                                 wxDefaultPosition, wxSize(FitWidth(MESSAGE_AREA_WIDTH), -1));
    m_mainAreaSizer->Add(m_message, wxSizerFlags(0).Expand());

    m_progress = new wxGauge(this, wxID_ANY, 100,
                             wxDefaultPosition, wxSize(FitWidth(MESSAGE_AREA_WIDTH), PX(16)));
    m_progressLabel = new wxStaticText(this, wxID_ANY, "");
    m_mainAreaSizer->Add(m_progress, wxSizerFlags(0).Expand().Border(wxTOP|wxBOTTOM, PX(10)));
    m_mainAreaSizer->Add(m_progressLabel, wxSizerFlags(0).Expand());
    m_mainAreaSizer->AddStretchSpacer(1);

    m_releaseNotesSizer = new wxBoxSizer(wxVERTICAL);

    wxStaticText *notesLabel = new wxStaticText(this, wxID_ANY, _("Release notes:"));
    SetBoldFont(notesLabel);
    m_releaseNotesSizer->Add(notesLabel, wxSizerFlags().Border(wxTOP, PX(10)));

    m_browserParent = new wxPanel(this, wxID_ANY,
                                  wxDefaultPosition,
                                  GetReleaseNotesSize());
#if HAVE_WEBVIEW
    m_webBrowser = NULL;
#endif

    m_releaseNotesSizer->Add
                         (
                             m_browserParent,
                             wxSizerFlags(1).Expand().Border(wxTOP, PX(10))
                         );

    m_mainAreaSizer->Add
                 (
                     m_releaseNotesSizer,
                     // proportion=10000 to overcome stretch spacer above
                     wxSizerFlags(10000).Expand()
                 );

    m_buttonSizer = new wxBoxSizer(wxHORIZONTAL);

    m_skipVersionButton = new wxButton(this, ID_SKIP_VERSION, _("Skip this version"));
    m_remindLaterButton = new wxButton(this, ID_REMIND_LATER, _("Remind me later"));
    m_installButton = new wxButton(this, ID_INSTALL, _("Install update"));

    m_updateButtonsSizer = new wxBoxSizer(wxHORIZONTAL);
    AddUpdateButtonsRow();
    m_buttonSizer->Add(m_updateButtonsSizer, wxSizerFlags(1));

    m_closeButtonSizer = new wxBoxSizer(wxHORIZONTAL);
    m_closeButton = new wxButton(this, wxID_CANCEL, _("Cancel"));
    m_closeButtonSizer->AddStretchSpacer(1);
    m_closeButtonSizer->Add(m_closeButton, wxSizerFlags(0).Border(wxLEFT));
    m_buttonSizer->Add(m_closeButtonSizer, wxSizerFlags(1));

    m_runInstallerButtonSizer = new wxBoxSizer(wxHORIZONTAL);
    // TODO: make this "Install and relaunch"
    m_runInstallerButton = new wxButton(this, ID_RUN_INSTALLER, _("Install update"));
    m_runInstallerButtonSizer->AddStretchSpacer(1);
    m_runInstallerButtonSizer->Add(m_runInstallerButton, wxSizerFlags(0).Border(wxLEFT));
    m_buttonSizer->Add(m_runInstallerButtonSizer, wxSizerFlags(1));

    m_mainAreaSizer->Add
                 (
                     m_buttonSizer,
                     wxSizerFlags(0).Expand().Border(wxTOP, PX(10))
                 );

    UpdateLayout();

    Bind(wxEVT_CLOSE_WINDOW, &UpdateDialog::OnClose, this);
    Bind(wxEVT_TIMER, &UpdateDialog::OnTimer, this);
    Bind(wxEVT_BUTTON, &UpdateDialog::OnCloseButton, this, wxID_CANCEL);
    Bind(wxEVT_BUTTON, &UpdateDialog::OnSkipVersion, this, ID_SKIP_VERSION);
    Bind(wxEVT_BUTTON, &UpdateDialog::OnRemindLater, this, ID_REMIND_LATER);
    Bind(wxEVT_BUTTON, &UpdateDialog::OnInstall, this, ID_INSTALL);
    Bind(wxEVT_BUTTON, &UpdateDialog::OnRunInstaller, this, ID_RUN_INSTALLER);
}


void UpdateDialog::AddUpdateButtonsRow()
{
    m_updateButtonsSizer->Add(m_skipVersionButton, wxSizerFlags().Border(wxRIGHT, PX(20)));
    m_updateButtonsSizer->AddStretchSpacer(1);
    m_updateButtonsSizer->Add(m_remindLaterButton, wxSizerFlags().Border(wxRIGHT, PX(10)));
    m_updateButtonsSizer->Add(m_installButton, wxSizerFlags());
}


void UpdateDialog::ArrangeForWidth(int width)
{
    // Stacked, the default action comes first, the way stacked buttons are
    // ordered on phones.
    ArrangeButtons(m_updateButtonsSizer,
                   { m_installButton, m_remindLaterButton, m_skipVersionButton },
                   PX(30), width,
                   [this] { AddUpdateButtonsRow(); });
}


wxSize UpdateDialog::GetReleaseNotesSize() const
{
#if WXSPARKLE_FILL_SCREEN
    // The release notes take what the rest of the dialog leaves of the screen:
    // just keep a few lines of them where that's little (a phone held
    // sideways).
    return wxSize(FitWidth(RELNOTES_WIDTH), PX(RELNOTES_HEIGHT / 2));
#else
    return wxSize(PX(RELNOTES_WIDTH), PX(RELNOTES_HEIGHT));
#endif
}


void UpdateDialog::EnablePulsing(bool enable)
{
    if ( enable && !m_timer.IsRunning() )
        m_timer.Start(100);
    else if ( !enable && m_timer.IsRunning() )
        m_timer.Stop();
}


void UpdateDialog::OnTimer(wxTimerEvent&)
{
    m_progress->Pulse();
}


void UpdateDialog::OnCloseButton(wxCommandEvent&)
{
    m_closeInitiatedByUpdater = false;
    Close();
}


void UpdateDialog::StopDownloader()
{
    if ( m_downloader )
    {
        m_downloader->TerminateAndJoin();
        delete m_downloader;
        m_downloader = NULL;

        UpdateDownloader::CleanLeftovers();
    }
}


void UpdateDialog::OnClose(wxCloseEvent&)
{
    EnablePulsing(false);
    StopDownloader();

    // We need to override this, because by default, wxDialog doesn't
    // destroy itself in Close().
    Destroy();

    // Notify about window being dismissed or update cancelled. We must not notify
    // if window closure was initiated by WinSparkle itself e.g. to run the installer.
    if (!m_closeInitiatedByUpdater)
    {
        if (!m_errorOccurred && m_appcast.IsValid())
            ApplicationController::NotifyUpdateCancelled();

        ApplicationController::NotifyUpdateDismissed();
    }
}


void UpdateDialog::ShutDown()
{
    EnablePulsing(false);
    StopDownloader();
    m_closeInitiatedByUpdater = true;
    Destroy();
}


void UpdateDialog::OnSkipVersion(wxCommandEvent&)
{
    try
    {
        Settings::WriteConfigValue("SkipThisVersion", m_appcast.Version);
    }
    CATCH_ALL_EXCEPTIONS
    ApplicationController::NotifyUpdateSkipped();
    m_closeInitiatedByUpdater = false; // skipping is cancellation
    Close();
}


void UpdateDialog::OnRemindLater(wxCommandEvent&)
{
    // Just abort the update. Next time it's scheduled to run,
    // the user will be prompted.
    ApplicationController::NotifyUpdatePostponed();
    m_closeInitiatedByUpdater = false; // skipping is cancellation
    Close();
}


void UpdateDialog::OnInstall(wxCommandEvent&)
{
    if ( !m_appcast.HasDownload() )
    {
        wxLaunchDefaultBrowser(wxString::FromUTF8(m_appcast.WebBrowserURL), wxBROWSER_NEW_WINDOW);
        m_closeInitiatedByUpdater = true;
        Close();
    }
    else if ( m_downloader == NULL )
    {
        StateDownloading();

        // Run the download in background.
        m_downloader = new UpdateDownloader(m_appcast);
        try
        {
            m_downloader->Start();
        }
        catch ( const std::exception& e )
        {
            delete m_downloader;
            m_downloader = NULL;
            LogError(e.what());
            StateUpdateError(Err_Generic);
        }
    }
}

void UpdateDialog::OnRunInstaller(wxCommandEvent&)
{
    if( !ApplicationController::IsReadyToShutdown() )
    {
        wxString appname;
        try
        {
            appname = Settings::GetAppName();
        }
        CATCH_ALL_EXCEPTIONS

        wxMessageDialog dlg(this,
                            wxString::Format(_("%s cannot be restarted."), appname),
                            _("Software Update"),
                            wxOK | wxOK_DEFAULT | wxICON_EXCLAMATION);
        dlg.SetExtendedMessage(_("Make sure that you don't have any unsaved documents and try again."));
        dlg.ShowModal();
        return;
    }

    wxBusyCursor bcur;

    m_message->SetLabel(_("Launching the installer..."));
    m_runInstallerButton->Disable();

    if ( !RunInstaller() )
    {
        wxMessageDialog dlg(this,
            _("Failed to launch the installer."),
            _("Software Update"),
            wxOK | wxOK_DEFAULT | wxICON_EXCLAMATION);
        dlg.ShowModal();
        m_runInstallerButton->Enable();
    }
    else
    {
        m_closeInitiatedByUpdater = true;
        Close();
        ApplicationController::RequestShutdown();
    }
}

bool UpdateDialog::RunInstaller()
{
    switch (ApplicationController::UserRunInstallerCallback(m_updateFile.c_str()))
    {
        case 0:
            // carry on with default handling
            break;
        case 1:
            // user callback handled installation
            return true;
        case WINSPARKLE_RETURN_ERROR:
        default:
            // some error occured (either runtime or unexpected return value)
            return false;
    }

#ifdef __WXMSW__
    std::wstring wArgs;

    SHELLEXECUTEINFOW sei;
    ::ZeroMemory(&sei, sizeof(sei));
    sei.cbSize = sizeof(sei);
    sei.lpFile = m_updateFile.c_str();
    sei.nShow = SW_SHOWDEFAULT;
    sei.fMask = SEE_MASK_FLAG_NO_UI;	// We display our own dialog box on error

    if (! m_installerArguments.empty())
    {
        wArgs = FromUtf8(m_installerArguments);
        sei.lpParameters = wArgs.c_str();
    }

    return ::ShellExecuteExW(&sei) ? true : false;
#else
    return LaunchUpdateFile(m_updateFile, m_installerArguments);
#endif
}

void UpdateDialog::SetMessage(const wxString& text, int width)
{
    SetWrappedLabel(m_message, text, width);
}


void UpdateDialog::SetHeading(const wxString& text)
{
#if WXSPARKLE_FILL_SCREEN
    // the heading is as wide as the screen allows, and no wider
    SetWrappedLabel(m_heading, text, RELNOTES_WIDTH);
#else
    m_heading->SetLabel(text);
#endif
}


void UpdateDialog::StateCheckingUpdates()
{
    LayoutChangesGuard guard(this);

    SetMessage(_("Checking for updates..."));

    m_closeButton->SetLabel(_("Cancel"));
    EnablePulsing(true);

    HIDE(m_heading);
    SHOW(m_progress);
    HIDE(m_progressLabel);
    SHOW(m_closeButtonSizer);
    HIDE(m_runInstallerButtonSizer);
    HIDE(m_releaseNotesSizer);
    HIDE(m_updateButtonsSizer);
    MakeResizable(false);
}


void UpdateDialog::StateNoUpdateFound(bool installAutomatically)
{
    m_installAutomatically = installAutomatically;

    if ( m_installAutomatically )
    {
        m_closeInitiatedByUpdater = true;
        Close();
        return;
    }

    LayoutChangesGuard guard(this);

    SetHeading(_("You're up to date!"));

    wxString msg;
    try
    {
        msg = wxString::Format
              (
                  _("%s %s is currently the newest version available."),
                  Settings::GetAppName(),
                  Settings::GetAppVersion()
              );
    }
    catch ( std::exception& )
    {
        // GetAppVersion() may fail
        msg = "Error: Updates checking not properly configured.";
    }

    SetMessage(msg);

    m_closeButton->SetLabel(_("Close"));
    m_closeButton->SetDefault();
    EnablePulsing(false);

    SHOW(m_heading);
    HIDE(m_progress);
    HIDE(m_progressLabel);
    SHOW(m_closeButtonSizer);
    HIDE(m_runInstallerButtonSizer);
    HIDE(m_releaseNotesSizer);
    HIDE(m_updateButtonsSizer);
    MakeResizable(false);
}


void UpdateDialog::StateUpdateError(ErrorCode err)
{
    m_errorOccurred = true;

    LayoutChangesGuard guard(this);

    SetHeading(_("Update Error!"));

    wxString msg;
    switch (err)
    {
        case Err_Generic:
            msg = _("An error occurred in retrieving update information; are you connected to the internet? Please try again later.");
            break;
        case Err_BadSignature:
            msg = _("The update is improperly signed.");
            break;
    }
    SetMessage(msg);

    m_closeButton->SetLabel(_("Cancel"));
    m_closeButton->SetDefault();
    EnablePulsing(false);

    SHOW(m_heading);
    HIDE(m_progress);
    HIDE(m_progressLabel);
    SHOW(m_closeButtonSizer);
    HIDE(m_runInstallerButtonSizer);
    HIDE(m_releaseNotesSizer);
    HIDE(m_updateButtonsSizer);
    MakeResizable(false);
}



void UpdateDialog::StateUpdateAvailable(const Appcast& info, bool installAutomatically)
{
    m_appcast = info;
    m_installAutomatically = installAutomatically;

    if ( installAutomatically )
    {
        wxCommandEvent nullEvent;
        OnInstall(nullEvent);
        return;
    }

    const bool showRelnotes = !info.ReleaseNotesURL.empty() || !info.Description.empty();

    {
        LayoutChangesGuard guard(this);

        const wxString appname = Settings::GetAppName();

        wxString ver_my = Settings::GetAppVersion();
        wxString ver_new = wxString::FromUTF8(info.ShortVersionString);
        if ( ver_new.empty() )
            ver_new = wxString::FromUTF8(info.Version);
        if ( ver_my == ver_new )
        {
            ver_my = wxString::Format("%s (%s)", ver_my, Settings::GetAppBuildVersion());
            ver_new = wxString::Format("%s (%s)", ver_new, wxString::FromUTF8(info.Version));
        }

        SetHeading(
            wxString::Format(_("A new version of %s is available!"), appname));

        if ( !info.HasDownload() )
            m_installButton->SetLabel(_("Get update"));

        SetMessage
        (
            wxString::Format
            (
                _("%s %s is now available (you have %s). Would you like to download it now?"),
                appname, ver_new, ver_my
            ),
            showRelnotes ? RELNOTES_WIDTH : MESSAGE_AREA_WIDTH
        );

        EnablePulsing(false);

        m_installButton->SetDefault();

        SHOW(m_heading);
        HIDE(m_progress);
        HIDE(m_progressLabel);
        HIDE(m_closeButtonSizer);
        HIDE(m_runInstallerButtonSizer);
        SHOW(m_updateButtonsSizer);
        if (info.CriticalUpdate)
        {
            HIDE(m_skipVersionButton);
            HIDE(m_remindLaterButton);
        }
        else
        {
            SHOW(m_skipVersionButton);
            SHOW(m_remindLaterButton);
        }
        DoShowElement(m_releaseNotesSizer, showRelnotes);
        MakeResizable(showRelnotes);
    }

    // Only show the release notes now that the layout was updated, as it may
    // take some time to load the web view control:
    if ( showRelnotes )
        ShowReleaseNotes(info);
}


void UpdateDialog::StateDownloading()
{
    LayoutChangesGuard guard(this);

    SetMessage(_("Downloading update..."));

    m_closeButton->SetLabel(_("Cancel"));
    EnablePulsing(false);

    HIDE(m_heading);
    SHOW(m_progress);
    SHOW(m_progressLabel);
    SHOW(m_closeButtonSizer);
    HIDE(m_runInstallerButtonSizer);
    HIDE(m_releaseNotesSizer);
    HIDE(m_updateButtonsSizer);
    MakeResizable(false);
}


void UpdateDialog::DownloadProgress(size_t downloaded, size_t total)
{
    wxString label;

    if ( total )
    {
        // wxGauge range is an int, scale huge sizes down
        while ( (total >> m_progressShift) > size_t(INT_MAX) )
            m_progressShift++;

        const int range = int(total >> m_progressShift);
        if ( m_progress->GetRange() != range )
            m_progress->SetRange(range);
        m_progress->SetValue(int(std::min(downloaded, total) >> m_progressShift));
        label = wxString::Format
                (
                    // TRANSLATORS: This is the progress of a download, e.g. "3 MB of 12 MB".
                    _("%s of %s"),
                    wxFileName::GetHumanReadableSize(wxULongLong(downloaded), "", 1, wxSIZE_CONV_SI),
                    wxFileName::GetHumanReadableSize(wxULongLong(total), "", 1, wxSIZE_CONV_SI)
                );
    }
    else
    {
        m_progress->Pulse();
        label = wxFileName::GetHumanReadableSize(wxULongLong(downloaded), "", 1, wxSIZE_CONV_SI);
    }

    if ( label != m_progressLabel->GetLabel() )
      m_progressLabel->SetLabel(label);

    Refresh();
    Update();
}


void UpdateDialog::StateUpdateDownloaded(const std::wstring& updateFile, const std::string& installerArguments)
{
    if ( m_downloader )
    {
        m_downloader->Join();
        delete m_downloader;
        m_downloader = NULL;
    }

    m_updateFile = updateFile;
    m_installerArguments = installerArguments;

    if ( m_installAutomatically )
    {
        wxCommandEvent nullEvent;
        OnRunInstaller(nullEvent);
        return;
    }

    LayoutChangesGuard guard(this);

    SetMessage(_("Ready to install."));

    m_progress->SetRange(1);
    m_progress->SetValue(1);

    m_runInstallerButton->SetDefault();

    HIDE(m_heading);
    SHOW(m_progress);
    HIDE(m_progressLabel);
    HIDE(m_closeButtonSizer);
    SHOW(m_runInstallerButtonSizer);
    HIDE(m_releaseNotesSizer);
    HIDE(m_updateButtonsSizer);
    MakeResizable(false);
}


void UpdateDialog::ShowReleaseNotes(const Appcast& info)
{
#if HAVE_WEBVIEW
    if ( !m_webBrowser && wxWebView::IsBackendAvailable(wxWebViewBackendDefault) )
    {
        wxBusyCursor busy;

#if defined(__WXMSW__) && wxUSE_WEBVIEW_IE
        if (!wxWebView::IsBackendAvailable(wxWebViewBackendEdge))
            wxWebViewIE::MSWSetEmulationLevel(wxWEBVIEWIE_EMU_IE11);
#endif
        // Note that about:blank is used so that subsequent SetPage() call doesn't fail with
        // MSIE. See https://github.com/vslavik/winsparkle/issues/155 and
        // https://github.com/wxWidgets/wxWidgets/issues/14768
        m_webBrowser = wxWebView::New(m_browserParent, wxID_ANY, "about:blank",
                                      wxDefaultPosition, GetReleaseNotesSize());
        if ( m_webBrowser )
        {
            auto sizer = new wxBoxSizer(wxVERTICAL);
            sizer->Add(m_webBrowser, wxSizerFlags(1).Expand());
            m_browserParent->SetSizer(sizer);

            // The subsequent code's purpose to is to reliably ensure that external links in the release
            // notes are opened in the default browser, and not in the embedded web view. This is surprisingly
            // complicated, because we need to ensure that the release notes page itself won't open
            // externally as well. And the supported backends behave wildly differently.
            //
            // Edge, WebKit:
            //
            //    ReleaseNotesURL:
            //        1. wxEVT_WEBVIEW_NAVIGATING(ReleaseNotesURL)
            //        2. wxEVT_WEBVIEW_LOADED(ReleaseNotesURL)
            //
            //    Description only (inline notes):
            //        1. wxEVT_WEBVIEW_NAVIGATING("data:text/html;..." or "about:blank")
            //        2. wxEVT_WEBVIEW_LOADED("about:blank")
            //
            // MSIE:
            //
            //    ReleaseNotesURL:
            //        1. wxEVT_WEBVIEW_LOADED("about:blank")
            //        2. wxEVT_WEBVIEW_NAVIGATING(ReleaseNotesURL)
            //        3. wxEVT_WEBVIEW_LOADED(ReleaseNotesURL)
            //
            //    Description only (inline notes):
            //        1. wxEVT_WEBVIEW_LOADED("")
            //
            // To further complicate matters, the ReleaseNotesURL page may be a redirect, triggering
            // a chain of navigating events before eventually landing at loaded.

            m_webBrowser->Bind(wxEVT_WEBVIEW_LOADED, [this](wxWebViewEvent& evt)
                {
                    switch (this->m_webBrowserState.SourceKind)
                    {
                        case WebBrowserSourceInline:
                            // for inline release notes, loaded event means the page is ready:
                            this->m_webBrowserState.PageLoaded = true;
                            break;

                        case WebBrowserSourceRemote:
                            // for remote, we need to filter out internal events like about:blank:
                            if (evt.GetURL().StartsWith("http"))
                                this->m_webBrowserState.PageLoaded = true;
                            break;

                        case WebBrowserSourceNone:
                            break;
                    }
                });

            // Open all links in the default browser once the documented is loaded.
            // Note that wxEVT_WEBVIEW_NAVIGATING is emitted for redirects during page loading
            // as well, as we can only "hijack" the event once the release notes page was loaded
            m_webBrowser->Bind(wxEVT_WEBVIEW_NAVIGATING, [this](wxWebViewEvent& evt)
                {
                    const wxString url = evt.GetURL();
                    if (this->m_webBrowserState.PageLoaded && url.StartsWith("http"))
                    {
                        wxLaunchDefaultBrowser(url);
                        evt.Veto();
                    }
                });

            // links targeting new windows are always opened externally
            m_webBrowser->Bind(wxEVT_WEBVIEW_NEWWINDOW, [](wxWebViewEvent& evt)
                {
                    if (evt.GetURL().StartsWith("http"))
                        wxLaunchDefaultBrowser(evt.GetURL());
                });

            Layout();
        }
    }

    if ( m_webBrowser )
    {
        if (!info.ReleaseNotesURL.empty())
        {
            m_webBrowserState.Reset(WebBrowserSourceRemote);
            m_webBrowser->LoadURL(wxString::FromUTF8(info.ReleaseNotesURL));
        }
        else if ( !info.Description.empty() )
        {
            m_webBrowserState.Reset(WebBrowserSourceInline);
            m_webBrowser->SetPage(wxString::FromUTF8(info.Description), "");
        }
    }
    else
#endif // HAVE_WEBVIEW
    {
        ShowReleaseNotesFallback(info);
    }

    SetWindowStyleFlag(GetWindowStyleFlag() | wxRESIZE_BORDER);
}


// Used if no web view is available: shows inline notes in a simple HTML
// control and offers a link to remote release notes.
void UpdateDialog::ShowReleaseNotesFallback(const Appcast& info)
{
    m_browserParent->DestroyChildren();
    auto sizer = new wxBoxSizer(wxVERTICAL);

    if ( !info.ReleaseNotesURL.empty() )
    {
        const wxString url = wxString::FromUTF8(info.ReleaseNotesURL);
#if wxUSE_HYPERLINKCTRL
        wxWindow *link = new wxHyperlinkCtrl(m_browserParent, wxID_ANY, url, url);
#else
        wxButton *link = new wxButton(m_browserParent, wxID_ANY, url,
                                      wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
        link->Bind(wxEVT_BUTTON, [url](wxCommandEvent&) { wxLaunchDefaultBrowser(url); });
#endif
#if WXSPARKLE_FILL_SCREEN
        // a long URL is cut off rather than make the dialog wider than the screen
        link->SetMinSize(wxSize(std::min(link->GetBestSize().x,
                                         FitWidth(RELNOTES_WIDTH) - PX(10)), -1));
#endif
        sizer->Add(link, wxSizerFlags().Border(wxALL, PX(5)));
    }
    else
    {
        const wxString html = wxString::FromUTF8(info.Description);
#if wxUSE_HTML
        wxHtmlWindow *htmlWin = new wxHtmlWindow(m_browserParent, wxID_ANY);
        htmlWin->SetPage(html);
        htmlWin->Bind(wxEVT_HTML_LINK_CLICKED, [](wxHtmlLinkEvent& evt)
            {
                wxLaunchDefaultBrowser(evt.GetLinkInfo().GetHref());
            });
        sizer->Add(htmlWin, wxSizerFlags(1).Expand());
#else
        sizer->Add(new wxTextCtrl(m_browserParent, wxID_ANY, html,
                                  wxDefaultPosition, wxDefaultSize,
                                  wxTE_MULTILINE | wxTE_READONLY),
                   wxSizerFlags(1).Expand());
#endif
    }

    m_browserParent->SetSizer(sizer);
    m_browserParent->Layout();
}


/*--------------------------------------------------------------------------*
              Dialog that asks for permission to check for updates
 *--------------------------------------------------------------------------*/

class AskPermissionDialog : public WinSparkleDialog
{
public:
    AskPermissionDialog();

private:
    void ArrangeForWidth(int width) override;
    void AddButtonsRow();

    wxBoxSizer *m_buttonSizer;
    wxButton   *m_okButton;
    wxButton   *m_cancelButton;
};


AskPermissionDialog::AskPermissionDialog()
{
    wxStaticText *heading =
            new wxStaticText(this, wxID_ANY,
                             _("Check for updates automatically?"));
    SetHeadingFont(heading);
    m_mainAreaSizer->Add(heading, wxSizerFlags(0).Expand().Border(wxBOTTOM, PX(10)));

    wxString appname;
    try
    {
        appname = Settings::GetAppName();
    }
    CATCH_ALL_EXCEPTIONS

    const wxString text = wxString::Format
                          (
                              _("Should %s automatically check for updates? You can always check for updates manually from the menu."),
                              appname
                          );
#if WXSPARKLE_FILL_SCREEN
    wxStaticText *message = new wxStaticText(this, wxID_ANY, "");
    SetWrappedLabel(message, text, MESSAGE_AREA_WIDTH);
#else
    wxStaticText *message =
            new wxStaticText
                (
                    this, wxID_ANY, text,
                    wxDefaultPosition, wxSize(PX(MESSAGE_AREA_WIDTH), -1)
                );
    message->Wrap(PX(MESSAGE_AREA_WIDTH));
#endif
    m_mainAreaSizer->Add(message, wxSizerFlags(0).Expand());

    m_mainAreaSizer->AddStretchSpacer(1);

    m_okButton = new wxButton(this, wxID_OK, _("Check automatically"));
    m_cancelButton = new wxButton(this, wxID_CANCEL, _("Don't check"));

    m_buttonSizer = new wxBoxSizer(wxHORIZONTAL);
    AddButtonsRow();

    m_mainAreaSizer->Add
                 (
                     m_buttonSizer,
                     wxSizerFlags(0).Expand().Border(wxTOP, PX(10))
                 );

    UpdateLayout();
}


void AskPermissionDialog::AddButtonsRow()
{
    // right-aligned
    m_buttonSizer->AddStretchSpacer(1);
    m_buttonSizer->Add(m_okButton, wxSizerFlags().Border(wxRIGHT));
    m_buttonSizer->Add(m_cancelButton);
}


void AskPermissionDialog::ArrangeForWidth(int width)
{
    ArrangeButtons(m_buttonSizer, { m_okButton, m_cancelButton },
                   wxSizerFlags::GetDefaultBorder(), width,
                   [this] { AddButtonsRow(); });
}


/*--------------------------------------------------------------------------*
                     UI state (only accessed from UI thread)
 *--------------------------------------------------------------------------*/

namespace
{

UpdateDialog *g_win = NULL;

void InitWindow()
{
    if ( !g_win )
    {
        g_win = new UpdateDialog();
        // note: bound after UpdateDialog's own handler, so it's called first
        g_win->Bind(wxEVT_CLOSE_WINDOW, [](wxCloseEvent& event)
            {
                g_win = NULL;
                event.Skip();
            });
    }
}

void ShowWindow()
{
    if ( !g_win )
        return; // the window may have been closed already (e.g. web link)

    g_win->Freeze();
    if (!g_win->IsShown())
        CenterWindowOnHostApplication(g_win);
    g_win->Show();
    g_win->Thaw();
    g_win->Raise();
}

void ShutDownUIInUIThread()
{
    if ( g_win )
    {
        UpdateDialog *win = g_win;
        g_win = NULL;
        win->ShutDown();
    }
}

} // anonymous namespace


/*--------------------------------------------------------------------------*
                 Standalone UI thread (Windows, non-wx hosts)
 *--------------------------------------------------------------------------*/

#ifdef __WXMSW__

namespace
{

class StandaloneApp : public wxApp
{
public:
    StandaloneApp()
    {
        // Keep the wx "main" thread running even without windows. This greatly
        // simplifies threads handling, because we don't have to correctly
        // implement wx-thread restarting.
        //
        // Note that this only works if we don't explicitly call ExitMainLoop(),
        // except in reaction to win_sparkle_cleanup()'s message.
        SetExitOnFrameDelete(false);
    }

    // don't parse the host application's command line
    virtual bool OnInit() { return true; }

    virtual wxLayoutDirection GetLayoutDirection() const { return g_layoutDirection; }
};


class StandaloneUIThread : public Thread
{
public:
    StandaloneUIThread() : Thread("WinSparkle UI thread"), m_ok(false) {}

    bool IsOk() const { return m_ok; }

protected:
    virtual void Run()
    {
        // We need to pass correct HINSTANCE to wxEntry() and the right value is
        // HINSTANCE of this DLL, not of the main .exe.
        HINSTANCE hInstance = static_cast<HINSTANCE>(UI::GetDllHINSTANCE());
        if ( !hInstance )
        {
            // If DllMain() was not called, assume we're statically linked
            // and use the hInstance of the containing program.
            hInstance = GetModuleHandle(NULL);
        }

        // IMPLEMENT_WXWIN_MAIN does this as the first thing
        wxDISABLE_DEBUG_SUPPORT();

        // Create our application object (there's no IMPLEMENT_APP() in the
        // library, so that it doesn't conflict with host applications).
        wxApp::SetInstance(new StandaloneApp);

        // We do this before wxEntry() explicitly, even though wxEntry() would
        // do it too, so that we know when wx is initialized and can signal
        // Start() about it *before* starting the event loop.
        wxInitializer wxinit;
        m_ok = wxinit.IsOk();

        // Signal Start() that it can continue
        SignalReady();

        if ( !m_ok )
            return;

        // Run the app:
        wxEntry(hInstance);
    }

    virtual bool IsJoinable() const { return true; }

private:
    bool m_ok;
};

} // anonymous namespace

#endif // __WXMSW__


/*--------------------------------------------------------------------------*
                     Dispatching calls to the UI thread
 *--------------------------------------------------------------------------*/

namespace
{

class UIDispatcher
{
public:
    // Runs fn in the UI thread asynchronously. If startUI is false and the UI
    // isn't running yet, does nothing and returns false.
    static bool Post(const std::function<void()>& fn, bool startUI = true);

    static void ShutDown();

private:
    enum Mode
    {
        Mode_Undecided,
        Mode_Host,          // using host application's wxApp
        Mode_Standalone,    // using our own wxApp in a separate thread
        Mode_Unavailable    // UI cannot be shown
    };

    static CriticalSection ms_cs;
    static Mode            ms_mode;
    static bool            ms_shutDown;
#ifdef __WXMSW__
    static StandaloneUIThread *ms_thread;
#endif
};

CriticalSection UIDispatcher::ms_cs;
UIDispatcher::Mode UIDispatcher::ms_mode = UIDispatcher::Mode_Undecided;
bool UIDispatcher::ms_shutDown = false;
#ifdef __WXMSW__
StandaloneUIThread *UIDispatcher::ms_thread = NULL;
#endif


bool UIDispatcher::Post(const std::function<void()>& fn, bool startUI)
{
    CriticalSectionLocker lock(ms_cs);

    if ( ms_shutDown )
        return false;

    if ( ms_mode == Mode_Undecided )
    {
        if ( wxTheApp )
        {
            if ( wxTheApp->IsGUI() )
            {
                ms_mode = Mode_Host;
            }
            else
            {
                LogError("The host wxApp is not a GUI application, cannot show any UI.");
                ms_mode = Mode_Unavailable;
            }
        }
        else
        {
#ifdef __WXMSW__
            if ( !startUI )
                return false;

            ms_thread = new StandaloneUIThread();
            ms_thread->Start();
            if ( ms_thread->IsOk() )
            {
                ms_mode = Mode_Standalone;
            }
            else
            {
                LogError("Failed to initialize wxWidgets.");
                ms_thread->Join();
                delete ms_thread;
                ms_thread = NULL;
                ms_mode = Mode_Unavailable;
            }
#else
            (void)startUI;
            LogError("No wxApp instance: the user interface is only available in wxWidgets applications on this platform.");
            ms_mode = Mode_Unavailable;
#endif
        }
    }

    if ( ms_mode == Mode_Unavailable || !wxTheApp )
        return false;

    wxTheApp->CallAfter([fn]
        {
            try
            {
                InitTranslations();
                fn();
            }
            CATCH_ALL_EXCEPTIONS
        });

    return true;
}


void UIDispatcher::ShutDown()
{
    Mode mode;
#ifdef __WXMSW__
    StandaloneUIThread *thread;
#endif

    {
        CriticalSectionLocker lock(ms_cs);
        if ( ms_shutDown )
            return;
        ms_shutDown = true;
        mode = ms_mode;
#ifdef __WXMSW__
        thread = ms_thread;
        ms_thread = NULL;
#endif
    }

    if ( mode == Mode_Host && wxTheApp )
    {
        if ( wxIsMainThread() )
            ShutDownUIInUIThread();
        else
            wxTheApp->CallAfter([] { ShutDownUIInUIThread(); });
    }

#ifdef __WXMSW__
    if ( thread )
    {
        // Note: the lock must not be held while joining the UI thread, as
        // other threads (e.g. the downloader) may need it to post messages.
        wxTheApp->CallAfter([]
            {
                wxEventLoopBase *activeLoop = wxEventLoopBase::GetActive();
                if ( activeLoop && !activeLoop->IsMain() )
                    activeLoop->Exit(wxID_CANCEL);
                if ( wxEventLoopBase *mainLoop = wxTheApp->GetMainLoop() )
                    mainLoop->ScheduleExit(0);
            });
        thread->Join();
        delete thread;
    }
#endif
}

} // anonymous namespace


/*--------------------------------------------------------------------------*
                             winsparkle::UI class
 *--------------------------------------------------------------------------*/

/*static*/
void UI::SetScreenAreaCallback(win_sparkle_screen_area_callback_t cb)
{
    g_screenAreaCallback = cb;
}


/*static*/
void UI::ShutDown()
{
    UIDispatcher::ShutDown();
}


/*static*/
void UI::NotifyNoUpdates(bool installAutomatically)
{
    ApplicationController::NotifyUpdateNotFound();

    UIDispatcher::Post([installAutomatically]
        {
            if ( g_win )
                g_win->StateNoUpdateFound(installAutomatically);
        },
        false);
}


/*static*/
void UI::NotifyUpdateAvailable(const Appcast& info, bool installAutomatically)
{
    ApplicationController::NotifyUpdateFound();

    UIDispatcher::Post([info, installAutomatically]
        {
            InitWindow();
            g_win->StateUpdateAvailable(info, installAutomatically);
            ShowWindow();
        });
}


/*static*/
void UI::NotifyDownloadProgress(size_t downloaded, size_t total)
{
    UIDispatcher::Post([downloaded, total]
        {
            if ( g_win )
                g_win->DownloadProgress(downloaded, total);
        },
        false);
}


/*static*/
void UI::NotifyUpdateDownloaded(const std::wstring& updateFile, const Appcast &appcast)
{
    const std::string args = appcast.enclosure.InstallerArguments;
    UIDispatcher::Post([updateFile, args]
        {
            if ( g_win )
                g_win->StateUpdateDownloaded(updateFile, args);
        },
        false);
}


/*static*/
void UI::NotifyUpdateError(ErrorCode err)
{
    ApplicationController::NotifyUpdateError();

    UIDispatcher::Post([err]
        {
            if ( g_win )
                g_win->StateUpdateError(err);
        },
        false);
}


/*static*/
void UI::ShowCheckingUpdates()
{
    UIDispatcher::Post([]
        {
            InitWindow();
            g_win->StateCheckingUpdates();
            ShowWindow();
        });
}


/*static*/
void UI::AskForPermission()
{
    UIDispatcher::Post([]
        {
            AskPermissionDialog dlg;
            CenterWindowOnHostApplication(&dlg);
            dlg.Raise();
            bool shouldCheck = (dlg.ShowModal() == wxID_OK);

            Settings::WriteConfigValue("CheckForUpdates", shouldCheck);

            if ( shouldCheck )
            {
                // same as in win_sparkle_init()
                PeriodicUpdateChecker::StartGlobal();
            }
        });
}

} // namespace winsparkle
