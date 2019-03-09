#ifndef WX_WXVBAM_H
#define WX_WXVBAM_H

#include <list>
#include <stdexcept>
#include <typeinfo>
#include <iostream>
#include <stdio.h>
#include <time.h>
#include <wx/log.h>
#include <wx/propdlg.h>
#include <wx/datetime.h>

#include "wx/joyedit.h"
#include "wx/keyedit.h"
#include "wx/sdljoy.h"
#include "wx/wxmisc.h"
#include "wxhead.h"
#ifndef NO_FFMPEG
#include "../common/ffmpeg.h"
#endif

/* yeah, they aren't needed globally, but I'm too lazy to limit where needed */
#include "../System.h"
#include "../Util.h"
#include "../gb/gb.h"
#include "../gb/gbCheats.h"
#include "../gb/gbGlobals.h"
#include "../gb/gbSound.h"
#include "../gba/Cheats.h"
#include "../gba/GBALink.h"
#include "../gba/Globals.h"
#include "../gba/Sound.h"

// make wxLogDebug work on non-debug builds of Wx, and make it use the console
// on Windows
// (this works on 2.8 too!)
#if !defined(NDEBUG) && (!wxDEBUG_LEVEL || defined(__WXMSW__))
    #ifdef __WXMSW__
        #define VBAM_DEBUG_STREAM stdout
    #else
        #define VBAM_DEBUG_STREAM stderr
    #endif
    #undef  wxLogDebug
    #define wxLogDebug(...)                                                                                                           \
    do {                                                                                                                              \
        fputs(wxString::Format(wxDateTime::UNow().Format(wxT("%X")) + wxT(": Debug: ") + __VA_ARGS__).mb_str(), VBAM_DEBUG_STREAM); \
        fputc('\n', VBAM_DEBUG_STREAM);                                                                                               \
    } while(0)
#endif

template <typename T>
void CheckPointer(T pointer)
{
    if (pointer == NULL) {
        std::string errormessage = "Pointer of type \"";
        errormessage += typeid(pointer).name();
        errormessage += "\" was not correctly created.";
        throw std::runtime_error(errormessage);
    }
}

/// Helper functions to convert WX's crazy string types to std::string

inline std::string ToString(wxCharBuffer aString)
{
    return std::string(aString);
}

inline std::string ToString(const wxChar* aString)
{
    return std::string(wxString(aString).mb_str(wxConvUTF8));
}

class MainFrame;

class wxvbamApp : public wxApp {
public:
    wxvbamApp()
        : wxApp()
        , pending_fullscreen(false)
        , using_wayland(false)
    {
    }
    virtual bool OnInit();
    virtual bool UsingWayland() { return using_wayland; }
    virtual void OnInitCmdLine(wxCmdLineParser&);
    virtual bool OnCmdLineParsed(wxCmdLineParser&);
    wxString GetConfigurationPath();
    const wxString GetPluginsDir();
    wxString GetAbsolutePath(wxString path);
    // name of a file to load at earliest opportunity
    wxString pending_load;
    // list of options to set after config file loaded
    wxArrayString pending_optset;
    // set fullscreen mode after init
    bool pending_fullscreen;
#if __WXMAC__
    // I suppose making this work will require tweaking the bundle
    void MacOpenFile(const wxString& f)
    {
        pending_load = f;
    };
#endif
    // without this, global accels don't always work
    //int FilterEvent(wxEvent& event);
    wxAcceleratorEntry_v accels;

    // the main configuration
    wxFileConfig* cfg;
    // vba-over.ini
    wxFileConfig* overrides;

    wxFileName rom_database;
    wxFileName rom_database_scene;
    wxFileName rom_database_nointro;

    wxString data_path;

    MainFrame* frame;
    // use this to get ms since program lauch
    wxStopWatch timer;
    // append log messages to this for the log viewer
    wxString log;
    // there's no way to retrieve "current" locale, so this is public
    wxLocale locale;

    // Handle most exceptions
    virtual bool OnExceptionInMainLoop()
    {
        try {
            throw;
        } catch (const std::exception& e) {
            std::cerr << "AN ERROR HAS OCCURRED: " << e.what() << std::endl;
            return false;
        }
    }

    virtual ~wxvbamApp();

protected:
    bool using_wayland;

private:
    wxPathList config_path;
    char* home = NULL;
};

DECLARE_APP(wxvbamApp);

// menu event handlers are declared with these macros in mainwind.cpp
// and mainwind-handlers.h, mainwind-cmd.cpp, and mainwind-evt.cpp are
// auto-generated from them
#define EVT_HANDLER(n, x) void MainFrame::On##n(wxCommandEvent& WXUNUSED(event))
// some commands are masked under some conditions
#define EVT_HANDLER_MASK(n, x, m)                          \
    void MainFrame::On##n(wxCommandEvent& WXUNUSED(event)) \
    {                                                      \
        if (!(cmd_enable & (m)))                           \
            return;                                        \
        Do##n();                                           \
    }                                                      \
    void MainFrame::Do##n()

// here are those conditions
enum { CMDEN_GB = (1 << 0), // GB ROM loaded
    CMDEN_GBA = (1 << 1), // GBA ROM loaded
    // the rest imply the above, unless:
    //   _ANY -> does not imply either
    //   _GBA -> only implies GBA
    CMDEN_REWIND = (1 << 2), // rewind states available
    CMDEN_SREC = (1 << 3), // sound recording in progress
    CMDEN_NSREC = (1 << 4), // no sound recording
    CMDEN_VREC = (1 << 5), // video recording
    CMDEN_NVREC = (1 << 6), // no video recording
    CMDEN_GREC = (1 << 7), // game recording
    CMDEN_NGREC = (1 << 8), // no game recording
    CMDEN_GPLAY = (1 << 9), // game playback
    CMDEN_NGPLAY = (1 << 10), // no game playback
    CMDEN_SAVST = (1 << 11), // any save states
    CMDEN_GDB = (1 << 12), // gdb connected
    CMDEN_NGDB_GBA = (1 << 13), // gdb not connected
    CMDEN_NGDB_ANY = (1 << 14), // gdb not connected
    CMDEN_NREC_ANY = (1 << 15), // not a/v recording
    CMDEN_LINK_ANY = (1 << 16), // link enabled

    CMDEN_NEVER = (1 << 31) // never (for NOOP)
};
#define ONLOAD_CMDEN (CMDEN_NSREC | CMDEN_NVREC | CMDEN_NGREC | CMDEN_NGPLAY)
#define UNLOAD_CMDEN_KEEP (CMDEN_NGDB_ANY | CMDEN_NREC_ANY | CMDEN_LINK_ANY)

struct checkable_mi_t {
    int cmd;
    wxMenuItem* mi;
    bool* boolopt;
    int *intopt, mask, val;
};

// wxArray is only for small types
typedef std::vector<checkable_mi_t> checkable_mi_array_t;

typedef std::list<wxDialog*> dialog_list_t;

class GameArea;

class LogDialog;

// true if pause should happen at next frame
extern bool pause_next;

class MainFrame : public wxFrame {
public:
    MainFrame();
    ~MainFrame();

    bool BindControls();
    void MenuOptionIntMask(const char* menuName, int& field, int mask);
    void MenuOptionIntRadioValue(const char* menuName, int& field, int mask);
    void MenuOptionBool(const char* menuName, bool& field);
    void GetMenuOptionInt(const char* menuName, int& field, int mask);
    void GetMenuOptionBool(const char* menuName, bool& field);
    void SetMenuOption(const char* menuName, int value);

    void SetJoystick();

    GameArea* GetPanel()
    {
        return panel;
    }

    wxString GetGamePath(wxString path);

    // wxMSW pauses the game for menu popups and modal dialogs, but wxGTK
    // does not.  It's probably desirable to pause the game.  To do this for
    // dialogs, use this function instead of dlg->ShowModal()
    int ShowModal(wxDialog* dlg);
    // and here are the wrapper functions for use when ShowModal() isn't
    // possible
    void StartModal();
    void StopModal();
    // however, adding a handler for open/close menu to do the same is unsafe.
    // there is no guarantee every show will be matched by a hide.
    void MenuPopped(wxMenuEvent& evt);

    // flags for enabling commands
    int cmd_enable;

    // adjust menus based on current cmd_enable
    void enable_menus();
    void EnableNetworkMenu();

    // adjust menus based on available save game states
    void update_state_ts(bool force = false);

    // retrieve oldest/newest slot
    // returns lowest-numbered slot on ties
    int oldest_state_slot(); // or empty slot if available
    int newest_state_slot(); // or 0 if none

    // system-defined accelerators
    wxAcceleratorEntry_v sys_accels;
    // adjust recent menu with accelerators
    void SetRecentAccels();
    // merge sys accels with user-defined accels (user overrides sys)
    wxAcceleratorEntry_v get_accels(wxAcceleratorEntry_v user_accels);
    // update menu and global accelerator table with sys+user accel defs
    // probably not the quickest way to add/remove individual accels
    void set_global_accels();

    // 2.8 has no HasFocus(), and FindFocus() doesn't work right
    bool HasFocus()
    {
        return focused;
    }

    // Returns the link mode to set according to the options
    LinkMode GetConfiguredLinkMode();

    void IdentifyRom();

    // Start GDB listener
    void GDBBreak();

    // The various viewer popups; these can be popped up as often as
    // desired
    void Disassemble();
    void IOViewer();
    void MapViewer();
    void MemViewer();
    void OAMViewer();
    void PaletteViewer();
    void TileViewer();

    // since they will all have to be destroyed on game unload:
    dialog_list_t popups;

    // this won't actually be destroyed, but it needs to be tracked so only
    // one is ever up and it needs to be pinged when new messages arrive
    LogDialog* logdlg;

    // the cheat search dialog isn't destroyed or tracked, but it needs
    // to be cleared between games
    void ResetCheatSearch();

    // call this to update the viewers once a frame:
    void UpdateViewers();

    virtual bool MenusOpened() { return menus_opened != 0; }

    virtual void SetMenusOpened(bool state);

    virtual bool DialogOpened() { return dialog_opened != 0; }

    // required for building from xrc
    DECLARE_DYNAMIC_CLASS(MainFrame);
    // required for event handling
    DECLARE_EVENT_TABLE();

    bool IsPaused(bool incendental = false)
    {
        return (paused && !pause_next && !incendental) || menus_opened || dialog_opened;
    }
protected:
    virtual void BindAppIcon();

private:
    GameArea* panel;

    // the various reasons the game might be paused
    bool paused;
    int menus_opened, dialog_opened;

    bool autoLoadMostRecent;
    // copy of top-level menu bar as a context menu
    wxMenu* ctx_menu;
    // load/save states menu items
    wxMenuItem* loadst_mi[10];
    wxMenuItem* savest_mi[10];
    wxDateTime state_ts[10];
    // checkable menu items
    checkable_mi_array_t checkable_mi;
    // recent menu item accels
    wxMenu* recent;
    wxAcceleratorEntry recent_accel[10];
    // joystick reader
    wxSDLJoy joy;

    // helper function for adding menu to accel editor
    void add_menu_accels(wxTreeCtrl* tc, wxTreeItemId& parent, wxMenu* menu);

    // for detecting window focus
    void OnActivate(wxActivateEvent&);
    // quicker & more accurate than FindFocus() != NULL
    bool focused;
    // may work, may not...  if so, load dropped file
    void OnDropFile(wxDropFilesEvent&);
    // pop up menu in fullscreen mode
    void OnMenu(wxContextMenuEvent&);
    // window geometry
    void OnMove(wxMoveEvent& event);
    void OnSize(wxSizeEvent& event);
    // Load a named wxDialog from the XRC file
    wxDialog* LoadXRCDialog(const char* name);
    // Load a named wxDialog from the XRC file
    wxDialog* LoadXRCropertySheetDialog(const char* name);

#include "cmdhandlers.h"
};

// helper class to add HiDPI awareness (mostly for Mac OS X)
class HiDPIAware {
public:
    HiDPIAware() { hidpi_scale_factor = 0; }
    virtual double HiDPIScaleFactor();
    virtual void RequestHighResolutionOpenGLSurface();
    virtual void GetRealPixelClientSize(int* x, int* y);
    virtual wxWindow* GetWindow() = 0;
private:
    double hidpi_scale_factor;
};

// a helper class to avoid forgetting StopModal()
class ModalPause {
public:
    ModalPause()
    {
        wxGetApp().frame->StartModal();
    }
    ~ModalPause()
    {
        wxGetApp().frame->StopModal();
    }
};

enum showspeed {
    // this order must match order of option enum and selector widget
    SS_NONE,
    SS_PERCENT,
    SS_DETAILED
};

enum filtfunc {
    // this order must match order of option enum and selector widget
    FF_NONE,
    FF_2XSAI,
    FF_SUPER2XSAI,
    FF_SUPEREAGLE,
    FF_PIXELATE,
    FF_ADVMAME,
    FF_BILINEAR,
    FF_BILINEARPLUS,
    FF_SCANLINES,
    FF_TV,
    FF_HQ2X,
    FF_LQ2X,
    FF_SIMPLE2X,
    FF_SIMPLE3X,
    FF_HQ3X,
    FF_SIMPLE4X,
    FF_HQ4X,
    FF_XBRZ2X,
    FF_XBRZ3X,
    FF_XBRZ4X,
    FF_XBRZ5X,
    FF_XBRZ6X,
    FF_PLUGIN // plugin must always be last
};
#define builtin_ff_scale(x)                                                \
    ((x == FF_XBRZ6X) ? 6 : (x == FF_XBRZ5X)                               \
                ? 5                                                        \
                : (x == FF_XBRZ4X || x == FF_HQ4X || x == FF_SIMPLE4X)     \
                    ? 4                                                    \
                    : (x == FF_XBRZ3X || x == FF_HQ3X || x == FF_SIMPLE3X) \
                        ? 3                                                \
                        : x == FF_PLUGIN ? 0 : x == FF_NONE ? 1 : 2)

enum ifbfunc {
    // this order must match order of option enum and selector widget
    IFB_NONE,
    IFB_SMART,
    IFB_MOTION_BLUR
};

// make sure and keep this in sync with opts.cpp!
enum renderer {
    RND_SIMPLE,
    RND_OPENGL,
    RND_DIRECT3D,
    RND_QUARTZ2D,
};

// likewise
enum audioapi { AUD_SDL,
    AUD_OPENAL,
    AUD_DIRECTSOUND,
    AUD_XAUDIO2,
    AUD_FAUDIO };

// an unfortunate legacy default; should have a non-digit preceding %d
// the only reason to keep it is that user can set slotdir to old dir
// otoh, we already make some name changes (double ext strip), and
// rename is not that hard (esp. under UNIX), so I'm going ahead with a new
// name.
//#define SAVESLOT_FMT wxT("%s%d.sgm")
#define SAVESLOT_FMT wxT("%s-%02d.sgm")

// display time, in ms
#define OSD_TIME 3000

class DrawingPanelBase;

class GameArea : public wxPanel, public HiDPIAware {
public:
    GameArea();
    virtual ~GameArea();

    virtual void SetMainFrame(MainFrame* parent) { main_frame = parent; }

    // set to game title + link info
    void SetFrameTitle();

    void LoadGame(const wxString& name);
    void UnloadGame(bool destruct = false);

    IMAGE_TYPE game_type()
    {
        return loaded;
    }
    uint32_t game_size()
    {
        return rom_size;
    }
    wxString game_dir()
    {
        return loaded_game.GetPath();
    }
    wxString game_name()
    {
        return loaded_game.GetFullName();
    }
    wxString bat_dir()
    {
        return batdir;
    }
    wxString state_dir()
    {
        return statedir;
    }
    void recompute_dirs();

    bool LoadState();
    bool LoadState(int slot);
    bool LoadState(const wxFileName& fname);

    bool SaveState();
    bool SaveState(int slot);
    bool SaveState(const wxFileName& fname);

    // save to default location
    void SaveBattery();

    // true if file at default location may not match memory
    bool cheats_dirty;

    static const int GBWidth = 160, GBHeight = 144, SGBWidth = 256, SGBHeight = 224,
                     GBAWidth = 240, GBAHeight = 160;
    void AddBorder();
    void DelBorder();
    // Delete() & set to NULL to force reinit
    DrawingPanelBase* panel;
    struct EmulatedSystem* emusys;

    // pause game or signal a long operation, similar to pausing
    void Pause();
    void Resume();

    // true if paused since last reset of flag
    bool was_paused;

    // osdstat is always displayed at top-left of screen
    wxString osdstat;

    // osdtext is displayed for 3 seconds after osdtime, and then cleared
    wxString osdtext;
    uint32_t osdtime;

    // Rewind: count down to 0 and rewind
    uint32_t rewind_time;
    // Rewind: flag to OnIdle to do a rewind
    bool do_rewind;
    // Rewind: rewind states
    char* rewind_mem; // should be uint8_t, really
    int num_rewind_states;
    int next_rewind_state;

    // Loaded rom information
    IMAGE_TYPE loaded;
    wxFileName loaded_game;
    uint32_t rom_crc32;
    wxString rom_name;
    wxString rom_scene_rls;
    wxString rom_scene_rls_name;
    uint32_t rom_size;

// FIXME: size this properly
#define NUM_REWINDS 8
#define REWIND_SIZE 1024 * 512 * NUM_REWINDS
    // FIXME: make this a config option

    void ShowFullScreen(bool full);
    bool IsFullScreen()
    {
        return fullscreen;
    }
    // set size of frame & panel to scaled screen size
    void AdjustSize(bool force);
#ifndef NO_FFMPEG
    void StartSoundRecording(const wxString& fname);
    void StopSoundRecording();
    void StartVidRecording(const wxString& fname);
    void StopVidRecording();
    void AddFrame(const uint8_t* data); // video
    void AddFrame(const uint16_t* data, int length); // audio
    bool IsRecording()
    {
        return snd_rec.IsRecording() || vid_rec.IsRecording();
    }
#endif
    void StartGameRecording(const wxString& fname);
    void StopGameRecording();
    void StartGamePlayback(const wxString& fname);
    void StopGamePlayback();

    virtual wxWindow* GetWindow() { return this; }

protected:
    MainFrame* main_frame;

    // set minsize of frame & panel to unscaled screen size
    void LowerMinSize();
    // set minsize of frame & panel to scaled screen size
    void AdjustMinSize();

    wxString batdir, statedir;

    int basic_width, basic_height;
    bool fullscreen;

    bool paused;
    void OnIdle(wxIdleEvent&);
    void OnKeyDown(wxKeyEvent& ev);
    void OnKeyUp(wxKeyEvent& ev);
    void OnSDLJoy(wxSDLJoyEvent& ev);
    void PaintEv(wxPaintEvent& ev);
    void EraseBackground(wxEraseEvent& ev);
    void OnSize(wxSizeEvent& ev);

#ifndef NO_FFMPEG
    MediaRecorder snd_rec, vid_rec;
#endif

public:
    void ShowPointer();
    void HidePointer();

protected:
    void MouseEvent(wxMouseEvent&)
    {
        ShowPointer();
    }
    bool pointer_blanked;
    uint32_t mouse_active_time;

    DECLARE_DYNAMIC_CLASS(GameArea)
    DECLARE_EVENT_TABLE()
};

// wxString version of OSD message
void systemScreenMessage(const wxString& msg);

// List of all commands with their descriptions
// sorted by cmd field for binary searching
// filled in by copy-events.cmake
extern struct cmditem {
    const wxString cmd, name;
    int cmd_id;
    int mask_flags; // if non-0, one of the flags must be turned on in win
    // to enable this command
    wxMenuItem* mi; // the menu item to invoke command, if present
} cmdtab[];
extern const int ncmds;
// for binary search
extern bool cmditem_lt(const struct cmditem& cmd1, const struct cmditem& cmd2);

#include "rpi.h"
#include <wx/dynlib.h>

class FilterThread;

class DrawingPanelBase : public HiDPIAware {
public:
    DrawingPanelBase(int _width, int _height);
    ~DrawingPanelBase();
    void DrawArea(uint8_t** pixels);

    virtual void PaintEv(wxPaintEvent& ev);
    virtual void EraseBackground(wxEraseEvent& ev);
    virtual void OnSize(wxSizeEvent& ev);
    wxWindow* GetWindow() { return dynamic_cast<wxWindow*>(this); }
    virtual bool Destroy() { return GetWindow()->Destroy(); }
protected:
    virtual void DrawArea(wxWindowDC&) = 0;
    virtual void DrawOSD(wxWindowDC&);
    int width, height;
    double scale;
    virtual void DrawingPanelInit();
    bool did_init;
    uint8_t* todraw;
    uint8_t *pixbuf1, *pixbuf2;
    FilterThread* threads;
    int nthreads;
    wxSemaphore filt_done;
    wxDynamicLibrary filt_plugin;
    const RENDER_PLUGIN_INFO* rpi; // also flag indicating plugin loaded
    // largest buffer required is 32-bit * (max width + 1) * (max height + 2)
    uint8_t delta[257 * 4 * 226];
};

// base class with a wxPanel when a subclass (such as wxGLCanvas) is not being used
class DrawingPanel : public DrawingPanelBase, public wxPanel {
public:
    DrawingPanel(wxWindow* parent, int _width, int _height);
};

class LogDialog : public wxDialog {
public:
    LogDialog();
    void Update();

private:
    wxTextCtrl* log;
    void Save(wxCommandEvent& ev);
    void Clear(wxCommandEvent& ev);

    DECLARE_EVENT_TABLE()
};

#include "opts.h"

// I should add this to SoundDriver, but wxArrayString is wx-specific
// I suppose I could make subclass wxSoundDriver.  maybe later.

#ifndef NO_OAL
class SoundDriver;
extern SoundDriver* newOpenAL();
extern bool GetOALDevices(wxArrayString& names, wxArrayString& ids);
#endif

#ifdef __WXMSW__
extern SoundDriver* newDirectSound();
extern bool GetDSDevices(wxArrayString& names, wxArrayString& ids);

#ifndef NO_XAUDIO2
extern SoundDriver* newXAudio2_Output();
extern bool GetXA2Devices(wxArrayString& names, wxArrayString& ids);
#endif

#ifndef NO_FAUDIO
extern SoundDriver* newFAudio_Output();
extern bool GetFADevices(wxArrayString& names, wxArrayString& ids);
#endif
#endif

extern bool debugger;
extern void (*dbgMain)();
extern void (*dbgSignal)(int, int);
extern void (*dbgOutput)(const char*, uint32_t);
extern void remoteStubMain();
extern void remoteCleanUp();
extern void remoteStubSignal(int, int);
extern void remoteOutput(const char*, uint32_t);

extern bool debugOpenPty();
extern const wxString& debugGetSlavePty();
extern bool debugWaitPty();
extern bool debugStartListen(int port);
extern bool debugWaitSocket();

// perhaps these functions should not be called systemXXX
// perhaps they should move to panel.cpp/GameArea
// but they must integrate with systemReadJoypad
void systemStartGameRecording(const wxString& fname);
void systemStopGameRecording();
void systemStartGamePlayback(const wxString& fname);
void systemStopGamePlayback();

// true if turbo mode (like pressing turbo button constantly)
extern bool turbo;

// mask of key press flags; see below
extern int joypress[4], autofire;

// FIXME: these defines should be global to project and used instead of raw numbers
#define KEYM_A (1 << 0)
#define KEYM_B (1 << 1)
#define KEYM_SELECT (1 << 2)
#define KEYM_START (1 << 3)
#define KEYM_RIGHT (1 << 4)
#define KEYM_LEFT (1 << 5)
#define KEYM_UP (1 << 6)
#define KEYM_DOWN (1 << 7)
#define KEYM_R (1 << 8)
#define KEYM_L (1 << 9)
#define KEYM_SPEED (1 << 10)
#define KEYM_CAPTURE (1 << 11)
#define KEYM_GS (1 << 12)

// actually, the wx port adds the following, which could be local:
#define REALKEY_MASK ((1 << 13) - 1)

#define KEYM_AUTO_A (1 << 13)
#define KEYM_AUTO_B (1 << 14)
#define KEYM_MOTION_UP (1 << 15)
#define KEYM_MOTION_DOWN (1 << 16)
#define KEYM_MOTION_LEFT (1 << 17)
#define KEYM_MOTION_RIGHT (1 << 18)
#define KEYM_MOTION_IN (1 << 19)
#define KEYM_MOTION_OUT (1 << 20)

#include "filters.h"

#endif /* WX_WXVBAM_H */
