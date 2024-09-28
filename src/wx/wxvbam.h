#ifndef VBAM_WX_WXVBAM_H_
#define VBAM_WX_WXVBAM_H_

#include <list>
#include <stdexcept>
#include <typeinfo>
#include <iostream>
#include <stdio.h>
#include <time.h>
#include <wx/log.h>
#include <wx/propdlg.h>
#include <wx/datetime.h>

#include "core/base/system.h"
#include "wx/config/bindings.h"
#include "wx/config/emulated-gamepad.h"
#include "wx/config/option-observer.h"
#include "wx/config/option.h"
#include "wx/dialogs/base-dialog.h"
#include "wx/widgets/dpi-support.h"
#include "wx/widgets/event-handler-provider.h"
#include "wx/widgets/keep-on-top-styler.h"
#include "wx/widgets/sdl-poller.h"
#include "wx/widgets/user-input-event.h"
#include "wx/widgets/wxmisc.h"
#include "wx/wxhead.h"

#ifndef NO_LINK
#include "core/gba/gbaLink.h"
#endif

#ifndef NO_FFMPEG
#include "components/av_recording/av_recording.h"
#endif

#include "wx/wxlogdebug.h"

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

class wxvbamApp final : public wxApp, public widgets::EventHandlerProvider {
public:
    wxvbamApp();

    // wxApp implementation.
    bool OnInit() final;
    int OnRun() final;
    bool OnCmdLineHelp(wxCmdLineParser&) final;
    bool OnCmdLineError(wxCmdLineParser&) final;
    void OnInitCmdLine(wxCmdLineParser&) final;
    bool OnCmdLineParsed(wxCmdLineParser&) final;
    // without this, global accels don't always work
    int FilterEvent(wxEvent&) final;
    // Handle most exceptions
    bool OnExceptionInMainLoop() override {
        try {
            throw;
        } catch (const std::exception& e) {
            std::cerr << "AN ERROR HAS OCCURRED: " << e.what() << std::endl;
            return false;
        }
    }

    wxString GetConfigDir();
    wxString GetDataDir();
    bool UsingWayland() { return using_wayland; }
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
    void MacOpenFile(const wxString& f) override
    {
        pending_load = f;
    };
#endif

    widgets::SdlPoller* sdl_poller() { return &sdl_poller_; }

    // vba-over.ini
    wxFileConfig* overrides = nullptr;

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

    // Accessors for configuration data.
    config::Bindings* bindings() { return &bindings_; }
    config::EmulatedGamepad* emulated_gamepad() { return &emulated_gamepad_; }

    ~wxvbamApp() override;

protected:
    bool using_wayland;
    bool console_mode = false;
    int console_status = 0;

private:
    // EventHandlerProvider implementation.
    wxEvtHandler* event_handler() override;

    config::Bindings bindings_;
    config::EmulatedGamepad emulated_gamepad_;

    wxPathList config_path;
    char* home = nullptr;

    widgets::SdlPoller sdl_poller_;
    widgets::KeyboardInputSender keyboard_input_sender_;

    // Main configuration file.
    wxFileName config_file_;
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

struct checkable_mi_t {
    int cmd;
    wxMenuItem* mi;
    int mask, val;
    bool initialized = false;
};

// wxArray is only for small types
typedef std::vector<checkable_mi_t> checkable_mi_array_t;

typedef std::list<wxDialog*> dialog_list_t;

class GameArea;

class LogDialog;

class JoystickPoller;

// true if pause should happen at next frame
extern bool pause_next;

class MainFrame : public wxFrame {
public:
    MainFrame();
    ~MainFrame() override;

    bool BindControls();
    void MenuOptionIntMask(const wxString& menuName, int field, int mask);
    void MenuOptionIntRadioValue(const wxString& menuName, int field, int mask);
    void MenuOptionBool(const wxString& menuName, bool field);
    void GetMenuOptionConfig(const wxString& menu_name,
                             const config::OptionID& option_id);
    void GetMenuOptionInt(const wxString& menuName, int* field, int mask);
    void GetMenuOptionBool(const wxString& menuName, bool* field);
    void SetMenuOption(const wxString& menuName, bool value);

    GameArea* GetPanel()
    {
        return panel;
    }

    wxString GetGamePath(wxString path);

    bool CanProcessShortcuts() const { return !menus_opened && !dialog_opened; }

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
#ifndef NO_LINK
    void EnableNetworkMenu();
#endif

    // adjust menus based on available save game states
    void update_state_ts(bool force = false);

    // retrieve oldest/newest slot
    // returns lowest-numbered slot on ties
    int oldest_state_slot(); // or empty slot if available
    int newest_state_slot(); // or 0 if none

    // Resets the Recent menu accelerators. Needs to be called every time the
    // Recent menu is updated.
    void ResetRecentAccelerators();
    // Resets all menu accelerators.
    void ResetMenuAccelerators();

    // 2.8 has no HasFocus(), and FindFocus() doesn't work right
    bool HasFocus() const override { return focused; }

#ifndef NO_LINK
    // Returns the link mode to set according to the options
    LinkMode GetConfiguredLinkMode();
#endif

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
    std::unique_ptr<LogDialog> logdlg;

    // the cheat search dialog isn't destroyed or tracked, but it needs
    // to be cleared between games
    void ResetCheatSearch();

    // call this to update the viewers once a frame:
    void UpdateViewers();

    virtual bool MenusOpened() { return menus_opened; }

    virtual void SetMenusOpened(bool state);

    virtual bool DialogOpened() { return dialog_opened != 0; }

    bool IsPaused(bool incendental = false)
    {
        return (paused && !pause_next && !incendental) || dialog_opened;
    }

    // required for building from xrc
    DECLARE_DYNAMIC_CLASS(MainFrame);
    // required for event handling
    DECLARE_EVENT_TABLE();

protected:
    virtual void BindAppIcon();

private:
    GameArea* panel;

    bool paused, menus_opened;
    int dialog_opened;

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
    // quicker & more accurate than FindFocus() != NULL
    bool focused;
    // One-time toggle to indicate that this object is fully initialized. This
    // used to filter events that are sent during initialization.
    bool init_complete_ = false;
#ifndef NO_LINK
    const config::OptionsObserver gba_link_observer_;
#endif
    const widgets::KeepOnTopStyler keep_on_top_styler_;
    const config::OptionsObserver status_bar_observer_;

    // wxFrame override.
    void SetStatusBar(wxStatusBar* menuBar) override;

    // helper function for adding menu to accel editor
    void add_menu_accels(wxTreeCtrl* tc, wxTreeItemId& parent, wxMenu* menu);

    // For enabling / disabling the status bar.
    void OnStatusBarChanged();
    // for detecting window focus
    void OnActivate(wxActivateEvent&);
    // may work, may not...  if so, load dropped file
    void OnDropFile(wxDropFilesEvent&);
    // pop up menu in fullscreen mode
    void OnMenu(wxContextMenuEvent&);
    // window geometry
    void OnMove(wxMoveEvent& event);
    void OnMoveStart(wxMoveEvent& event);
    void OnMoveEnd(wxMoveEvent& event);
    void OnSize(wxSizeEvent& event);
    // Load a named wxDialog from the XRC file
    wxDialog* LoadXRCDialog(const char* name);

#include "wx/cmdhandlers.h"
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

#ifdef __WXMSW__
// For saving menu handle.
#include <windows.h>
#endif

class GameArea : public wxPanel {
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

    // Resets the panel, it will be re-created on the next frame.
    void ResetPanel();

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
    void OnUserInput(widgets::UserInputEvent& event);
    void PaintEv(wxPaintEvent& ev);
    void EraseBackground(wxEraseEvent& ev);
    void OnSize(wxSizeEvent& ev);
    void OnKillFocus(wxFocusEvent& ev);

#ifndef NO_FFMPEG
    recording::MediaRecorder snd_rec, vid_rec;
#endif

public:
    void ShowPointer();
    void HidePointer();
    void HideMenuBar();
    void ShowMenuBar();
    void OnGBBorderChanged(config::Option* option);
    void UpdateLcdFilter();
    void SuspendScreenSaver();
    void UnsuspendScreenSaver();

protected:
    void MouseEvent(wxMouseEvent&);
    bool pointer_blanked, menu_bar_hidden, xscreensaver_suspended = false;
    uint32_t mouse_active_time;
    wxPoint mouse_last_pos;
#ifdef __WXMSW__
    HMENU current_hmenu = nullptr;
#endif

    DECLARE_DYNAMIC_CLASS(GameArea)
    DECLARE_EVENT_TABLE()

private:
    void OnAudioRateChanged();
    void OnVolumeChanged(config::Option* option);

    bool schedule_audio_restart_ = false;

    const config::OptionsObserver render_observer_;
    const config::OptionsObserver scale_observer_;
    const config::OptionsObserver gb_border_observer_;
    const config::OptionsObserver gb_palette_observer_;
    const config::OptionsObserver gb_declick_observer_;
    const config::OptionsObserver lcd_filters_observer_;
    const config::OptionsObserver audio_rate_observer_;
    const config::OptionsObserver audio_volume_observer_;
    const config::OptionsObserver audio_observer_;
};

// wxString version of OSD message
void systemScreenMessage(const wxString& msg);

#include "wx/rpi.h"
#include <wx/dynlib.h>

class FilterThread;

class DrawingPanelBase {
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
    wxDynamicLibrary filter_plugin_;
    RENDER_PLUGIN_INFO* rpi_; // also flag indicating plugin loaded
    // largest buffer required is 32-bit * (max width + 1) * (max height + 2)
    uint8_t delta[257 * 4 * 226];
};

// base class with a wxPanel when a subclass (such as wxGLCanvas) is not being used
class DrawingPanel : public DrawingPanelBase, public wxPanel {
public:
    DrawingPanel(wxWindow* parent, int _width, int _height);
};

class LogDialog : public dialogs::BaseDialog {
public:
    LogDialog();
    void Update();

private:
    wxTextCtrl* log;
    void Save(wxCommandEvent& ev);
    void Clear(wxCommandEvent& ev);

    DECLARE_EVENT_TABLE()
};

#include "wx/opts.h"

#if defined(VBAM_ENABLE_DEBUGGER)
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
#endif  // defined(VBAM_ENABLE_DEBUGGER)

// supported movie format for game recording
enum MVFormatID {
    MV_FORMAT_ID_NONE,

    /* movie formats */
    MV_FORMAT_ID_VMV,
    MV_FORMAT_ID_VMV1,
    MV_FORMAT_ID_VMV2,
};
std::vector<MVFormatID> getSupMovFormatsToRecord();
std::vector<char*> getSupMovNamesToRecord();
std::vector<char*> getSupMovExtsToRecord();
std::vector<MVFormatID> getSupMovFormatsToPlayback();
std::vector<char*> getSupMovNamesToPlayback();
std::vector<char*> getSupMovExtsToPlayback();

// perhaps these functions should not be called systemXXX
// perhaps they should move to panel.cpp/GameArea
// but they must integrate with systemReadJoypad
void systemStartGameRecording(const wxString& fname, MVFormatID format);
void systemStopGameRecording();
void systemStartGamePlayback(const wxString& fname, MVFormatID format);
void systemStopGamePlayback();

// true if turbo mode (like pressing turbo button constantly)
extern bool turbo;

extern int autofire, autohold;

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

#endif // VBAM_WX_WXVBAM_H_
