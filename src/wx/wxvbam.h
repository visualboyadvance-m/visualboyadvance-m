#ifndef VBAM_WX_WXVBAM_H_
#define VBAM_WX_WXVBAM_H_

#include <chrono>
#include <cstdio>
#include <ctime>
#include <list>
#include <memory>
#include <set>
#include <stdexcept>
#include <vector>
#include <iostream>

#include <wx/arrstr.h>

#include <wx/log.h>
#include <wx/propdlg.h>
#include <wx/datetime.h>

#include "core/base/system.h"
#include "wx/config/bindings.h"
#include "wx/hdr.h"
#include "wx/config/emulated-gamepad.h"
#include "wx/config/option-observer.h"
#include "wx/config/option.h"
#include "wx/dialogs/base-dialog.h"
#include "wx/widgets/dpi-support.h"
#include "wx/widgets/event-handler-provider.h"
#include "wx/widgets/keep-on-top-styler.h"
#include "wx/widgets/keyboard-input-handler.h"
#include "wx/widgets/on-screen-controller.h"
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
#include "wx/compat_generic_file_dialog.h"

extern wxLocale *wxvbam_locale;

// True if Vulkan is built in and a runtime instance with the Wayland surface
// extension can be created with at least one physical device -- i.e. the native
// Wayland Vulkan renderer can actually run. Probed at startup (before the GUI is
// up) to decide whether an SDL2 + no-EGL-canvas build can stay a native Wayland
// client or must fall back to XWayland. Defined in panel.cpp.
bool VbamVulkanRuntimeUsable();

// True if the Vulkan loader (vulkan-1.dll) can be loaded at run time. On MSVC the
// loader is delay-loaded -- not bundled and not a load-time dependency -- so this
// probes for it once before any Vulkan code runs; a missing loader must never
// reach a delay-loaded vk* call. On other toolchains Vulkan is linked normally,
// so availability follows the compile-time option. Defined in panel.cpp.
bool VbamVulkanRuntimeAvailable();

#if defined(__WXMSW__)
// True if the machine has a usable hardware graphics adapter. When false (a
// headless server or a VM/RDP session exposing only the Microsoft Basic Render
// Driver), every GPU render method fails to initialize and only the Simple
// renderer's software fallback works. Fails open (returns true) when it cannot
// probe. Result is cached. Defined in panel.cpp.
bool VbamWindowsHasHardwareGpu();

// True if running on Windows 10 or newer, where Direct3D 12 is available. On
// older Windows the D3D12 entry points are absent. Defined in panel.cpp.
bool VbamWindowsIsWin10OrGreater();
#endif  // defined(__WXMSW__)

// Set when the app fell back to XWayland (GDK_BACKEND=x11) because no native
// Wayland renderer was available (SDL2 + no wx EGL canvas + no usable Vulkan).
// In that state neither HDR nor 10-bit SDR can be presented. Defined in
// wxvbam.cpp.
void VbamSetWaylandNoNativeRenderer(bool value);
bool VbamWaylandNoNativeRenderer();

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
    int OnExit() final;
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
#ifndef VBAM_WX_MAC_PATCHED_FOR_ALERT_SOUND
    bool ProcessEvent(wxEvent& event) final;
#endif

    wxString GetConfigDir();
    wxString GetDataDir();
    bool UsingWayland() { return using_wayland; }
    wxString GetConfigurationPath();
    const wxString GetPluginsDir();
    wxString GetAbsolutePath(wxString path);

    // Plugin enumeration cache - pre-populated on startup for fast hotkey cycling
    void EnumeratePlugins();
    const wxArrayString& GetValidPlugins() const { return valid_plugins_; }
    bool ArePluginsEnumerated() const { return plugins_enumerated_; }
    void InvalidatePluginCache() { plugins_enumerated_ = false; valid_plugins_.clear(); }
    // name of a file to load at earliest opportunity
    wxString pending_load;
    // list of options to set after config file loaded
    wxArrayString pending_optset;
    // set fullscreen mode after init
    bool pending_fullscreen;
    // mute audio for this session without persisting to the config
    bool mute;
#if __WXMAC__
    // I suppose making this work will require tweaking the bundle
    void MacOpenFile(const wxString& f) override
    {
        pending_load = f;
    };
#endif

    widgets::SdlPoller* sdl_poller() { return &sdl_poller_; }

    // vba-over.ini
    std::unique_ptr<wxFileConfig> overrides_;
    std::unique_ptr<wxFileConfig> gb_overrides_;

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
    char* home = NULL;

    widgets::SdlPoller sdl_poller_;
    widgets::KeyboardInputHandler keyboard_input_handler_;

    // Main configuration file.
    wxFileName config_file_;

    // Plugin enumeration cache
    wxArrayString valid_plugins_;
    bool plugins_enumerated_ = false;
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
    // Lazy dialog loading - each dialog loaded on first access
    wxDialog* LoadDialog(const wxString& name);  // Load and initialize a dialog by name
    std::set<wxString> dialogs_initialized_;

    // Pulls one dialog off the preload queue and loads it. Called from
    // GameArea::OnIdle while no ROM is running so we amortize the XRC parse
    // cost before the user opens Options. Returns true if more work remains.
    bool PreloadOneDialog();
    std::vector<wxString> dialogs_preload_queue_;
    bool dialogs_preload_populated_ = false;
    wxLongLong dialogs_preload_last_ms_ = 0;
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

    // Menu event handler for tracking menu state on all platforms.
    // On Windows, also disables audio loop when menu is open.
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
    LogDialog* GetLogDialog();  // Lazy-load the log dialog

    // the cheat search dialog isn't destroyed or tracked, but it needs
    // to be cleared between games
    void ResetCheatSearch();

    // call this to update the viewers once a frame:
    void UpdateViewers();

    virtual bool MenusOpened() { return menus_opened; }

    void SetMenusOpened(bool state);

    virtual bool DialogOpened() { return dialog_opened != 0; }

    bool IsPaused(bool incendental = false)
    {
        return (paused && !pause_next && !incendental) || dialog_opened;
    }

    // required for building from xrc
    DECLARE_DYNAMIC_CLASS(MainFrame);
    // required for event handling
    DECLARE_EVENT_TABLE();

public:
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
    void OnIconize(wxIconizeEvent& event);

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

// Set on first launch (no config file yet) to arm the one-time runtime
// display-filter probe; cleared once GameArea has settled on a filter.
extern bool g_default_filter_probe_pending;

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

    // True if HDR is enabled AND actually being presented by the current
    // renderer/display. False when there is no panel yet, or when HDR is on but
    // unsupported (in which case it stays on but has no effect).
    bool HdrEffective() const;

    // While the color-correction profile is in "auto" mode (the user hasn't
    // explicitly picked one), keep it following the output: Rec2020 for HDR,
    // sRGB for SDR.
    void ApplyAutoColorCorrection();

    // HDR is "effectively available" when the display probed HDR-capable AND not
    // every HDR-capable renderer has failed to present it this session.
    bool HdrEffectivelyAvailable() const;

    // Return HDR / 10-bit deep color to "auto": clear the session revert and the
    // failed-renderer set so the feature is reconsidered (and renderers may be
    // switched again) on the next panel init. Used when the user re-checks a
    // toggle that had fallen back to SDR this session. The caller drives the
    // actual panel reset (the live option write does so via the render observer),
    // so this only resets the bookkeeping.
    void ResetHdrRevert() {
        hdr_reverted_to_sdr_ = false;
        hdr_failed_renderers_.clear();
        hdr_retried_renderers_.clear();
    }
    void ResetDeepColorRevert() {
        deep_color_reverted_ = false;
        deep_color_failed_renderers_.clear();
    }

    // True if the render method should be hidden from the display dialog because
    // HDR is on and effective and this renderer either can't do HDR or has
    // failed to present it.
    bool RenderMethodHiddenForHdr(config::RenderMethod method) const;

    // 10-bit deep color (X11): available when a 10-bit visual was probed AND a
    // capable renderer has not failed to present 10-bit this session.
    bool DeepColorEffectivelyAvailable() const;

    // True if the render method can present a 10-bit SDR surface in XOrg
    // (OpenGL via a 10-bit visual, Vulkan via a 10-bit swapchain). SDL's GL
    // renderer only outputs 8-bit sRGB and the simple/wx renderer is software,
    // so neither is deep-color capable.
    bool IsDeepColorCapableRenderer(config::RenderMethod method) const;

    // True if the render method should be hidden from the display dialog because
    // the 10-bit deep color option is on and effective and this renderer can't
    // present 10-bit (or has failed to this session).
    bool RenderMethodHiddenForDeepColor(config::RenderMethod method) const;

    // Definitive HDR / 10-bit availability probe: actually brings each capable
    // renderer up on this (already-shown) panel with the feature forced on,
    // renders one black frame so the renderer creates its real swapchain/visual
    // and reports whether it presents the feature, then tears it down. The first
    // renderer that succeeds wins; the result is published via hdr::SetAvailability
    // so the dialog checkbox is shown only when a renderer definitely can present
    // it -- i.e. probe and runtime use the very same test. Call once, after the
    // main window is shown and before a ROM is loaded. No-op once a panel exists.
    void ProbeOutputCapabilities();

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

    // Re-initialise an existing panel WITHOUT flashing black: keep the outgoing
    // panel (and the frame it is showing) alive while the replacement is built
    // behind it, and retire the old one only once the replacement has drawn.
    // Use this instead of ResetPanel() everywhere an existing panel is being
    // rebuilt (filter/render/border/fullscreen/HDR), i.e. whenever the panel is
    // destroyed to be re-created rather than created for the first time.
    void ReinitPanel();

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
    HMENU current_hmenu = NULL;
#endif

    DECLARE_DYNAMIC_CLASS(GameArea)
    DECLARE_EVENT_TABLE()

private:
    void OnAudioRateChanged();
    void OnVolumeChanged(config::Option* option);

    bool schedule_audio_restart_ = false;
    bool pending_resume_after_panel_ = false;
    bool pending_panel_reset_ = false;  // Deferred panel reset to avoid mid-frame crashes

    // Outgoing panel kept alive by ReinitPanel() so its last frame stays on
    // screen while the replacement is built behind it; destroyed by
    // RetireOldPanel() once the replacement has drawn its first real frame.
    DrawingPanelBase* old_panel_ = nullptr;
    void RetireOldPanel();
    void TeardownPanel(DrawingPanelBase* p);
    // kDispFilter observer: adopt the new filter in place when the renderer
    // supports it (no panel rebuild, no black flash), else fall back to a full
    // ReinitPanel().
    void OnDispFilterChanged();
    // Which live panel owns the window an event targets (see PaintEv). Handles
    // the swap window where both old_panel_ and panel exist simultaneously.
    DrawingPanelBase* PanelForWindow(wxObject* obj) const;

    // On-screen touch controller overlay. Created lazily and kept stacked above
    // the render panel; on by default on Android, optional elsewhere.
    widgets::OnScreenController* osc_ = nullptr;
    std::unique_ptr<config::OptionsObserver> osc_observer_;
    // Creates/shows/hides and resizes the overlay to match
    // kUIShowOnScreenController and whether a game is loaded.
    void UpdateOnScreenController();
    // Pops up a compact application menu (invoked by the overlay Menu button).
    void ShowOnScreenMenu();

    // Runtime display-filter auto-probe (first launch only). Once a ROM is
    // running, this cycles candidate filters from highest to lowest quality,
    // measures each one's real frame rate, and settles on the highest that
    // sustains ~60fps, persisting the choice. Armed by g_default_filter_probe_
    // pending; driven from OnIdle via StepFilterProbe(). Implementation and
    // tuning constants live in panel.cpp.
    enum class FilterProbeState { kInactive, kStartupDelay, kStabilize, kMeasure };
    FilterProbeState filter_probe_state_ = FilterProbeState::kInactive;
    int filter_probe_index_ = 0;       // index into the candidate list
    int filter_probe_frames_ = 0;      // frames counted in the current phase
    std::chrono::steady_clock::time_point filter_probe_phase_start_;
    // Per-frame tracking used by the kStabilize phase to wait for steady-state
    // frame pacing (after the panel rebuild's cold-start frames) before
    // measuring, instead of a fixed warm-up window.
    std::chrono::steady_clock::time_point filter_probe_last_frame_;
    double filter_probe_min_interval_ms_ = 0.0;
    int filter_probe_stable_count_ = 0;
    // Per-frame intervals collected during the kMeasure window. The candidate's
    // fps is derived from the fast quarter of these (its highest sustained rate),
    // so the slow-frame tail from transient hitches doesn't drag it below the
    // accept threshold when the filter otherwise holds ~60fps.
    std::vector<double> filter_probe_intervals_;
    void StepFilterProbe();            // called once per emulated frame
    void BeginFilterProbeCandidate();  // switch to candidate and start stabilizing
    void AdvanceFilterProbe(double measured_fps);  // pass/settle or step down
    void FinishFilterProbe();          // stop probing and persist the result

    // HDR renderer fallback state. When HDR is on, the active renderer is
    // checked once its panel is initialized; a renderer that fails to present
    // HDR is recorded here and another HDR-capable renderer is tried. If they
    // all fail, HDR reverts to SDR for the session.
    std::set<config::RenderMethod> hdr_failed_renderers_;
    // An HDR-capable renderer can latch its HDR state at panel construction
    // before the window has been composited on the HDR output (notably the very
    // first panel at startup), reporting SDR even though it can present HDR. Each
    // such renderer is recreated once -- recorded here -- before being treated as
    // a genuine HDR failure, so the user's choice isn't abandoned over a timing
    // artifact. Cleared by ResetHdrRevert() when the user re-enables HDR.
    std::set<config::RenderMethod> hdr_retried_renderers_;
    bool hdr_evaluated_ = false;        // current panel's HDR/deep-color checked
    bool hdr_reverted_to_sdr_ = false;  // every HDR renderer failed this session
    bool deep_color_reverted_ = false;  // no renderer could present 10-bit
    // Deep-color renderer fallback state, mirroring the HDR set above: a capable
    // renderer that fails to actually present 10-bit this session is recorded
    // here so another capable renderer is tried before reverting.
    std::set<config::RenderMethod> deep_color_failed_renderers_;
    void EvaluateHdrRenderer();
    void EvaluateDeepColorRenderer();
    bool IsHdrCapableRenderer(config::RenderMethod method) const;

    // Renderer-init fallback: when a panel reports DrawingInitFailed(), record
    // the method and switch to the next renderer in the platform priority list
    // (Windows: DX12, Vulkan, SDL, OpenGL, Simple; Linux: Vulkan, SDL, OpenGL,
    // Simple; macOS: Metal, Vulkan, SDL, OpenGL, Simple). Mirrors the HDR
    // fallback above. Renderers that failed to init are remembered for the
    // session so the search converges instead of looping.
    std::set<config::RenderMethod> render_init_failed_;
    bool renderer_evaluated_ = false;   // current panel's renderer-init checked
    void EvaluateRenderer();

    // Construct (but do not lay out) the DrawingPanel for `method`, applying the
    // same per-platform fallbacks as panel creation in OnIdle. Returns nullptr
    // for kLast. Shared by OnIdle and ProbeOutputCapabilities().
    DrawingPanelBase* NewPanelForRenderMethod(config::RenderMethod method);

    // Schedule a panel reset to occur at the start of the next OnIdle
    // This is used instead of calling ResetPanel directly from observers
    // to avoid resetting the panel while emulation is in progress
    void SchedulePanelReset();

    const config::OptionsObserver render_observer_;
    const config::OptionsObserver disp_filter_observer_;
    const config::OptionsObserver color_correction_auto_observer_;
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

#include "wx/widgets/render-plugin.h"

class FilterThread;

class DrawingPanelBase {
public:
    DrawingPanelBase(int _width, int _height);
    ~DrawingPanelBase();
    void DrawArea(uint8_t** pixels);
    // Synchronously stop filter threads - call before Destroy() to ensure
    // threads are stopped before InterframeManager is cleaned up
    void StopFilterThreads();

    // True if this renderer can adopt a new (non-plugin) display filter in place
    // -- recomputing scale and rebuilding its source texture on the next
    // DrawArea -- without recreating its window/device/swapchain, so a filter
    // change doesn't flash black. Every renderer re-specifies its source texture
    // from `width * scale` each frame (or, for the wx software panel, rebuilds
    // its wxImage each frame), so this is true by default; a renderer that ever
    // cannot may override it to false to fall back to a full ReinitPanel.
    virtual bool SupportsInPlaceFilterChange() const { return true; }

    // Arm an in-place filter change: the next DrawArea() adopts the current
    // OPTION(kDispFilter) via ApplyPendingFilterChange(). Only valid when
    // SupportsInPlaceFilterChange() and the filter is not a plugin. See
    // GameArea's kDispFilter observer.
    void ApplyInPlaceFilterChange();

    // Apply an armed in-place filter change at the start of a frame (called from
    // every DrawArea(uint8_t**) path): recompute the scale, drop the now
    // wrongly-sized filter output buffer, and reset the filter threads so they
    // rebuild at the new scale. No-op unless a change is pending.
    void ApplyPendingFilterChange();

    // True if this panel was built for a filter plugin (its color format and
    // pipeline are plugin-specific). Switching a plugin in or out needs a full
    // rebuild, so the in-place filter path is skipped for it.
    bool IsUsingFilterPlugin() const { return rpi_ != nullptr; }

    virtual void PaintEv(wxPaintEvent& ev);
    virtual void EraseBackground(wxEraseEvent& ev);
    virtual void OnSize(wxSizeEvent& ev);
    // Present the frame just produced by DrawArea(uint8_t**). The default (used
    // by the wxQt software path) invalidates the window so a paint event
    // re-presents `todraw`. A renderer that can present the frame directly
    // (e.g. the Android GLES panel) overrides this to skip the paint round-trip.
    virtual void PresentFrame();
    wxWindow* GetWindow() { return dynamic_cast<wxWindow*>(this); }
    virtual bool Destroy() { return GetWindow()->Destroy(); }

    // ---- HDR output (renderer-independent, see src/wx/hdr.h) ----------------

    // True if this renderer can present an HDR surface on the current display.
    // Default false; renderers override and may consult the windowing system
    // (e.g. only on Wayland).
    virtual bool SupportsHdr() const { return false; }

    // Surface encoding this renderer wants when HDR is active. PQ10 for the
    // 10-bit platforms (OpenGL/Vulkan/D3D12), scRGB fp16 for macOS EDR.
    virtual hdr::Encoding PreferredHdrEncoding() const { return hdr::Encoding::kPQ10; }

    // For the scRGB encoding, whether the surface is extended-linear Display P3
    // (macOS EDR) rather than extended-linear Rec.709 (SDL3 / generic scRGB).
    virtual bool HdrScRgbUsesP3() const { return false; }

    // For the scRGB encoding, the nits value mapped to 1.0 in the float output,
    // or 0 to use the relative reference-white model (reference -> 1.0). Non-zero
    // on SDL where the renderer applies an SDR white point > 1 (Windows scRGB
    // swapchains): the encoder then emits absolute nits that survive SDL's
    // automatic white-point scaling instead of being over-brightened by it.
    virtual float HdrScRgbWhiteNits() const { return 0.0f; }

    // Recompute the active HDR encoding from the options and this renderer's
    // capabilities, and push the brightness settings into the encoder. Call on
    // panel init and whenever the HDR / color-correction options change.
    void UpdateHdrState();

    bool HdrActive() const { return hdr_encoding_ != hdr::Encoding::kNone; }
    hdr::Encoding hdr_encoding() const { return hdr_encoding_; }

    // True if this panel is actually presenting a 10-bit "deep color" SDR
    // surface (only the OpenGL panel can, and only if it got a 10-bit visual).
    virtual bool DeepColorActive() const { return false; }

    // True once DrawingPanelInit() has run, i.e. HdrActive()/DeepColorActive()
    // are final.
    bool DrawingInitialized() const { return did_init; }

    // True if this renderer failed to come up (no GPU device/context/renderer).
    // The panel object still exists but cannot present; GameArea uses this to
    // fall back to the next renderer in the platform priority list. Set by each
    // renderer panel on its creation-failure path. Note did_init is unreliable
    // for this: several panels set it (via the base init) before the renderer is
    // actually created, so it can be true even when the renderer failed.
    bool DrawingInitFailed() const { return init_failed_; }

    // Encode `rows` x `row_pixels` RGBA8 pixels read from `src` (with
    // `src_stride` bytes per scanline) into hdr_buf_, tightly packed. Returns
    // the encoded buffer and sets `out_bpp` to its bytes per pixel. Only valid
    // when HdrActive(); the source must be 32-bit (panel_color_depth_ == 32).
    const uint8_t* EncodeHdr(const uint8_t* src, int src_stride, int row_pixels,
                             int rows, int* out_bpp);

protected:
    virtual void DrawArea(wxWindowDC&) = 0;
    virtual void DrawOSD(wxWindowDC&);
    int width, height;
    double scale;
    virtual void DrawingPanelInit();
    bool did_init;
    bool init_failed_ = false;  // renderer creation failed (see DrawingInitFailed)
    // Set by ApplyInPlaceFilterChange(); the next DrawArea() recomputes scale,
    // rebuilds the filter buffer/threads at that scale, and clears this. Applied
    // there (not when the option changes) so scale/pixbuf2/todraw stay mutually
    // consistent -- an interleaved repaint keeps showing the old frame until the
    // new one is ready instead of flashing black.
    bool pending_filter_change_ = false;
    uint8_t* todraw;
    uint8_t *pixbuf1, *pixbuf2;
    FilterThread* threads;
    int nthreads;
    wxSemaphore filt_done;
    wxSemaphore filt_ready;  // Posted by threads when they're ready to receive work
    wxDynamicLibrary filter_plugin_;
    RENDER_PLUGIN_INFO* rpi_; // also flag indicating plugin loaded
    RENDER_PLUGIN_INFO rpi_info_; // storage for plugin info when using proxy
    bool rpi_using_rgb565_ = false; // true if plugin uses RGB565 format
    bool rpi_is_mt_ = false; // true if plugin is multi-threaded (name contains " MT")
    int rpi_bpp_ = 4; // bytes per pixel for RPI plugin (4 for 32-bit, 2 for 16-bit)
    int panel_color_depth_ = 16; // Color depth for this panel (may differ from global systemColorDepth)
    hdr::Encoding hdr_encoding_ = hdr::Encoding::kNone; // active HDR surface encoding
    std::vector<uint8_t> hdr_buf_;                       // scratch for EncodeHdr()
#ifdef VBAM_RPI_PROXY_SUPPORT
    rpi_proxy::RpiProxyClient* rpi_proxy_client_ = nullptr;  // Points to shared instance
    bool using_rpi_proxy_ = false;
#endif
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
    size_t shown_len_ = 0;
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
