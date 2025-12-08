#include "wx/wxvbam.h"

#ifdef __WXMSW__
#include <windows.h>
#include <versionhelpers.h>
#endif

#ifdef __WXGTK__
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#endif

#ifdef __WXGTK3__
#include <gdk/gdk.h>
#endif

#include <stdio.h>
#include <wx/cmdline.h>
#include <wx/display.h>
#include <wx/file.h>
#include <wx/filesys.h>
#include <wx/fs_arc.h>
#include <wx/fs_mem.h>
#include <wx/menu.h>
#include <wx/mstream.h>
#include <wx/progdlg.h>
#include <wx/protocol/http.h>
#include <wx/regex.h>
#include <wx/spinctrl.h>
#include <wx/textctrl.h>
#include <wx/combobox.h>
#include <wx/sstream.h>
#include <wx/stdpaths.h>
#include <wx/string.h>
#include <wx/txtstrm.h>
#include <wx/url.h>
#include <wx/uiaction.h>
#include <wx/wfstream.h>
#include <wx/wxcrtvararg.h>
#include <wx/zipstrm.h>

#include "components/user_config/user_config.h"
#include "core/base/system.h"
#include "core/gb/gbGlobals.h"
#include "core/gba/gbaSound.h"
#include "wx/gb-builtin-over.h"
#include "wx/builtin-over.h"
#include "wx/builtin-xrc.h"
#include "wx/config/cmdtab.h"
#include "wx/config/command.h"
#include "wx/config/emulated-gamepad.h"
#include "wx/config/option-proxy.h"
#include "wx/config/option.h"
#include "wx/config/user-input.h"
#include "wx/config/strutils.h"
#include "wx/wayland.h"
#include "wx/widgets/group-check-box.h"
#include "wx/widgets/user-input-ctrl.h"
#include "wx/widgets/utils.h"

#if defined(VBAM_ENABLE_DEBUGGER)
#include "core/gba/gbaRemote.h"
#endif  // defined(VBAM_ENABLE_DEBUGGER)

#ifdef __WXGTK__
// Global pointers for GTK mnemonic handling (defined later, declared here for OnInit)
static GtkWidget* g_main_window = nullptr;
static GtkWidget* g_menubar_widget = nullptr;
static void ForceMenubarMnemonicsVisible(GtkWidget* menubar_widget);

#endif

namespace {

// Resets the accelerator text for `menu_item` to the best keyboard input.
void ResetMenuItemAccelerator(wxMenuItem* menu_item) {
    const wxString old_label = menu_item->GetItemLabel();
    wxString new_label;

    // Remove existing accelerator text (either tab-separated or in parentheses)
    const size_t tab_index = old_label.find('\t');
    if (tab_index != wxString::npos) {
        new_label = old_label.substr(0, tab_index);
    } else {
        // Check for parentheses format " (shortcut)"
        const size_t paren_index = old_label.rfind(" (");
        if (paren_index != wxString::npos && old_label.Last() == ')') {
            new_label = old_label.substr(0, paren_index);
        } else {
            new_label = old_label;
        }
    }
    std::unordered_set<config::UserInput> user_inputs =
        wxGetApp().bindings()->InputsForCommand(
            config::ShortcutCommand(menu_item->GetId()));

    // Find the best keyboard shortcut to display (prefer ones with modifiers)
    const config::UserInput* best_input = nullptr;
    for (const auto& input : user_inputs) {
        if (!input.is_keyboard()) {
            continue;  // Skip joystick inputs
        }
        if (!best_input) {
            best_input = &input;
            continue;
        }
        // Prefer inputs with extended modifiers (L/R distinction)
        const bool current_has_extended = config::HasExtendedModifiers(input.keyboard_input().mod_extended());
        const bool best_has_extended = config::HasExtendedModifiers(best_input->keyboard_input().mod_extended());
        if (current_has_extended && !best_has_extended) {
            best_input = &input;
            continue;
        }
        // Prefer inputs with any modifiers over those without
        if (input.keyboard_input().mod() != wxMOD_NONE && best_input->keyboard_input().mod() == wxMOD_NONE) {
            best_input = &input;
        }
    }

    if (best_input) {
#ifdef __WXMAC__
        // On macOS, use space and parentheses instead of tab separator
        // because wxWidgets may strip unrecognized modifiers from tab-separated accelerators
        new_label.append(" (");
        new_label.append(best_input->ToLocalizedString());
        new_label.append(")");
#else
        new_label.append('\t');
        new_label.append(best_input->ToLocalizedString());
#endif
    }

    if (old_label != new_label) {
        menu_item->SetItemLabel(new_label);
    }
}

static const wxString kOldConfigFileName("vbam.conf");
static const wxString knewConfigFileName("vbam.ini");
static const char kDotDir[] = "visualboyadvance-m";

}  // namespace

#if defined(VBAM_ENABLE_DEBUGGER)
void(*dbgMain)() = remoteStubMain;
void(*dbgSignal)(int, int) = remoteStubSignal;
void(*dbgOutput)(const char *, uint32_t) = debuggerOutput;
#endif  // defined(VBAM_ENABLE_DEBUGGER)

#ifdef __WXMSW__

int __stdcall WinMain(HINSTANCE hInstance,
                      HINSTANCE hPrevInstance,
                      LPSTR lpCmdLine,
                      int nCmdShow) {
    bool console_attached = AttachConsole(ATTACH_PARENT_PROCESS) != FALSE;
#ifdef DEBUG
    // In debug builds, create a console if none is attached.
    if (!console_attached) {
        console_attached = AllocConsole() != FALSE;
    }
#endif  // DEBUG

    // Redirect stdout/stderr to the console if one is attached.
    // This code was taken from Dolphin.
    // https://github.com/dolphin-emu/dolphin/blob/6cf99195c645f54d54c72322ad0312a0e56bc985/Source/Core/DolphinQt/Main.cpp#L112
    HANDLE stdout_handle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (console_attached && stdout_handle) {
#if __STDC_WANT_SECURE_LIB__
        FILE *ostream;
        FILE *estream;
        freopen_s(&ostream, "CONOUT$", "w", stdout);
        freopen_s(&estream, "CONOUT$", "w", stderr);
#else
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
#endif
    }

    // Set up logging.
#ifdef DEBUG
    wxLog::SetLogLevel(wxLOG_Trace);
#else   // DEBUG
    wxLog::SetLogLevel(wxLOG_Info);
#endif  // DEBUG

    // Redirect e.g. --help to stderr.
    wxMessageOutput::Set(new wxMessageOutputStderr());

    // This will be freed on wxEntry exit.
    wxApp::SetInstance(new wxvbamApp());
    return wxEntry(hInstance, hPrevInstance, lpCmdLine, nCmdShow);
}

#else  // __WXMSW__

int main(int argc, char** argv) {
    // Set up logging.
#ifdef DEBUG
    wxLog::SetLogLevel(wxLOG_Trace);
#else   // DEBUG
    wxLog::SetLogLevel(wxLOG_Info);
#endif  // DEBUG

    // Launch under xwayland on Wayland if EGL is not available.
#if defined(__WXGTK3__) && !defined(HAVE_WAYLAND_EGL)
    wxString xdg_session_type = wxGetenv("XDG_SESSION_TYPE");
    wxString wayland_display  = wxGetenv("WAYLAND_DISPLAY");

    if (xdg_session_type == "wayland" || wayland_display.Contains("wayland")) {
        gdk_set_allowed_backends("x11,*");

        if (wxGetenv("GDK_BACKEND") == NULL) {
            wxSetEnv("GDK_BACKEND", "x11");
        }
    }
#else
#ifdef __WXGTK__
    wxString xdg_session_type = wxGetenv("XDG_SESSION_TYPE");
    wxString wayland_display  = wxGetenv("WAYLAND_DISPLAY");

    if (xdg_session_type == "wayland" || wayland_display.Contains("wayland")) {
        if (wxGetenv("GDK_BACKEND") == NULL) {
#ifdef ENABLE_SDL3
            wxSetEnv("GDK_BACKEND", "wayland");
#else
            wxSetEnv("GDK_BACKEND", "x11");
#endif
        }
    } else {
        if (wxGetenv("GDK_BACKEND") == NULL) {
            wxSetEnv("GDK_BACKEND", "x11");
        }
    }
#endif
#endif

    // This will be freed on wxEntry exit.
    wxApp::SetInstance(new wxvbamApp());
    return wxEntry(argc, argv);
}

#endif  // __WXMSW__

wxvbamApp& wxGetApp() {
    return *static_cast<wxvbamApp*>(wxApp::GetInstance());
}

IMPLEMENT_DYNAMIC_CLASS(MainFrame, wxFrame)

#ifndef NO_ONLINEUPDATES
#include "autoupdater/autoupdater.h"
#endif // NO_ONLINEUPDATES

// generate config file path
static void get_config_path(wxPathList& path, bool exists = true)
{
    wxString current_app_name = wxGetApp().GetAppName();

    //   local config dir first, then global
    //   locale-specific res first, then main
    wxStandardPathsBase& stdp = wxStandardPaths::Get();
#define add_path(p)                                                                                          \
    do {                                                                                                     \
        const wxString& s = stdp.p;                                                                          \
        wxFileName parent = wxFileName::DirName(s + wxT("//.."));                                            \
        parent.MakeAbsolute();                                                                               \
        if ((wxDirExists(s) && wxIsWritable(s)) || ((!exists || !wxDirExists(s)) && parent.IsDirWritable())) \
            path.Add(s);                                                                                     \
    } while (0)
#define add_nonstandard_path(p)                                                                              \
    do {                                                                                                     \
        const wxString& s = p;                                                                               \
        wxFileName parent = wxFileName::DirName(s + wxT("//.."));                                            \
        parent.MakeAbsolute();                                                                               \
        if ((wxDirExists(s) && wxIsWritable(s)) || ((!exists || !wxDirExists(s)) && parent.IsDirWritable())) \
            path.Add(s);                                                                                     \
    } while (0)

    static bool debug_dumped = false;

    if (!debug_dumped) {
        wxLogDebug(wxT("GetUserLocalDataDir(): %s"), stdp.GetUserLocalDataDir().c_str());
        wxLogDebug(wxT("GetUserDataDir(): %s"), stdp.GetUserDataDir().c_str());
        wxLogDebug(wxT("GetLocalizedResourcesDir(wxvbam_locale->GetCanonicalName()): %s"), stdp.GetLocalizedResourcesDir(wxvbam_locale->GetCanonicalName()).c_str());
        wxLogDebug(wxT("GetResourcesDir(): %s"), stdp.GetResourcesDir().c_str());
        wxLogDebug(wxT("GetDataDir(): %s"), stdp.GetDataDir().c_str());
        wxLogDebug(wxT("GetLocalDataDir(): %s"), stdp.GetLocalDataDir().c_str());
        wxLogDebug(wxT("plugins_dir: %s"), wxGetApp().GetPluginsDir().c_str());
        wxLogDebug(wxT("XdgConfigDir: %s"), (wxString(get_xdg_user_config_home().c_str(), wxConvLibc) + current_app_name).c_str());
        debug_dumped = true;
    }

// When native support for XDG dirs is available (wxWidgets >= 3.1),
// this will be no longer necessary
#if defined(__WXGTK__)
    // XDG spec manual support
    // ${XDG_CONFIG_HOME:-$HOME/.config}/`appname`
    wxString old_config = wxString(getenv("HOME"), wxConvLibc) + kFileSep + ".vbam";
    wxString new_config(get_xdg_user_config_home().c_str(), wxConvLibc);
    if (!wxDirExists(old_config) && wxIsWritable(new_config))
    {
        wxFileName new_path(new_config, wxEmptyString);
        new_path.AppendDir(current_app_name);
        new_path.MakeAbsolute();

        add_nonstandard_path(new_path.GetFullPath());
    }
    else
    {
        // config is in $HOME/.vbam/
        add_nonstandard_path(old_config);
    }
#endif

    // NOTE: this does not support XDG (freedesktop.org) paths
    add_path(GetUserLocalDataDir());
    add_path(GetUserDataDir());
    add_path(GetLocalizedResourcesDir(wxvbam_locale->GetCanonicalName()));
    add_path(GetResourcesDir());
    add_path(GetDataDir());
    add_path(GetLocalDataDir());
    add_nonstandard_path(wxGetApp().GetPluginsDir());
}

static void tack_full_path(wxString& s, const wxString& app = wxEmptyString)
{
    // regenerate path, including nonexistent dirs this time
    wxPathList full_config_path;
    get_config_path(full_config_path, false);

    for (size_t i = 0; i < full_config_path.size(); i++)
        s += wxT("\n\t") + full_config_path[i] + app;
}

wxvbamApp::wxvbamApp()
    : wxApp(),
      pending_fullscreen(false),
      frame(NULL),
      using_wayland(false),
      emulated_gamepad_(std::bind(&wxvbamApp::bindings, this)),
      sdl_poller_(this),
      keyboard_input_handler_(this) {
    Bind(wxEVT_ACTIVATE_APP, [this](wxActivateEvent& event) {
        if (!event.GetActive()) {
            keyboard_input_handler_.Reset();
        }
#ifdef __WXGTK__
        else {
            // When app is activated (e.g., after dialog closes), ensure menubar mnemonics are visible
            if (frame) {
                wxMenuBar* menubar = frame->GetMenuBar();
                if (menubar) {
                    GtkWidget* menubar_widget = menubar->GetHandle();
                    if (menubar_widget) {
                        GtkWidget* toplevel = gtk_widget_get_toplevel(menubar_widget);
                        if (toplevel && GTK_IS_WINDOW(toplevel))
                            gtk_window_set_mnemonics_visible(GTK_WINDOW(toplevel), TRUE);
                    }
                }
            }
        }
#endif
        event.Skip();
    });
}

const wxString wxvbamApp::GetPluginsDir()
{
    return wxStandardPaths::Get().GetPluginsDir();
}

wxString wxvbamApp::GetConfigurationPath() {
    // first check if config files exists in reverse order
    // (from system paths to more local paths.)
    if (data_path.empty()) {
        get_config_path(config_path);

        for (int i = config_path.size() - 1; i >= 0; i--) {
            wxFileName fn(config_path[i], knewConfigFileName);

            if (fn.FileExists() && fn.IsFileWritable()) {
                data_path = config_path[i];
                break;
            }
        }
    }

    // if no config file was not found, search for writable config
    // dir or parent to create it in in OnInit in normal order
    // (from user paths to system paths.)
    if (data_path.empty()) {
        for (size_t i = 0; i < config_path.size(); i++) {
            // Check if path is writeable
            if (wxIsWritable(config_path[i])) {
                data_path = config_path[i];
                break;
            }

            // check if parent of path is writable, so we can
            // create the path in OnInit
            wxFileName parent_dir = wxFileName::DirName(config_path[i] + wxT("//.."));
            parent_dir.MakeAbsolute();

            if (parent_dir.IsDirWritable()) {
                data_path = config_path[i];
                break;
            }
        }
    }

    return data_path;
}

wxString wxvbamApp::GetAbsolutePath(wxString path)
{
    wxFileName fn(path);

    if (fn.IsRelative()) {
        fn.MakeRelativeTo(GetConfigurationPath());
        fn.Normalize(wxPATH_NORM_ENV_VARS | wxPATH_NORM_DOTS | wxPATH_NORM_TILDE |
                     wxPATH_NORM_CASE | wxPATH_NORM_ABSOLUTE | wxPATH_NORM_LONG |
                     wxPATH_NORM_SHORTCUT);
        return fn.GetFullPath();
    }

    return path;
}

int language = wxLANGUAGE_DEFAULT;
wxLocale *wxvbam_locale = NULL;

bool wxvbamApp::OnInit() {
    using_wayland = IsWayland();

#if ((wxMAJOR_VERSION == 3) && (wxMINOR_VERSION >= 3)) || (wxMAJOR_VERSION > 3)
    // SetAppearance is available in wxWidgets 3.3+
    // Skip on Windows XP where it's not supported
#ifdef __WXMSW__
    if (IsWindowsVistaOrGreater())
#endif
    {
        SetAppearance(Appearance::System);
    }
#endif

    // use consistent names for config, DO NOT TRANSLATE
    SetAppName("visualboyadvance-m");
#if (wxMAJOR_VERSION >= 3)
    SetAppDisplayName("VisualBoyAdvance-M");
#endif

    wxvbam_locale = new wxLocale;

    language = OPTION(kLocale);

    if (language != wxLANGUAGE_DEFAULT) {
        wxvbam_locale->Init(language, wxLOCALE_LOAD_DEFAULT);
    } else {
        wxvbam_locale->Init();
    }

    // load selected language

    wxvbam_locale->AddCatalog("wxvbam");

    // make built-in xrc file available
    // this has to be done before parent OnInit() so xrc dump works
    wxFileSystem::AddHandler(new wxMemoryFSHandler);
    wxFileSystem::AddHandler(new wxArchiveFSHandler);
    wxMemoryFSHandler::AddFileWithMimeType(wxT("wxvbam.xrs"), builtin_xrs, sizeof(builtin_xrs), wxT("application/zip"));

    if (!wxApp::OnInit())
        return false;

    if (console_mode)
        return true;

    // prepare for loading xrc files
    wxXmlResource* xr = wxXmlResource::Get();
    // note: if linking statically, next 2 pull in lot of unused code
    // maybe in future if not wxSHARED, load only builtin-needed handlers
    xr->InitAllHandlers();
    xr->AddHandler(new widgets::GroupCheckBoxXmlHandler());
    xr->AddHandler(new widgets::UserInputCtrlXmlHandler());
    wxInitAllImageHandlers();
    get_config_path(config_path);
    // first, load override xrcs
    // this can only override entire root nodes
    // 2.9 has LoadAllFiles(), but this is 2.8, so we'll do it manually
    wxString cwd = wxGetCwd();

    for (size_t i = 0; i < config_path.size(); i++)
        if (wxDirExists(config_path[i]) && wxSetWorkingDirectory(config_path[i])) {
            // *.xr[cs] doesn't work (double the number of scans)
            // 2.9 gives errors for no files found, so manual precheck needed
            // (yet another double the number of scans)
            if (!wxFindFirstFile(wxT("*.xrc")).empty())
                xr->Load(wxT("*.xrc"));

            if (!wxFindFirstFile(wxT("*.xrs")).empty())
                xr->Load(wxT("*.xrs"));
        }

    wxFileName xrcDir(GetConfigurationPath() + wxT("//xrc"), wxEmptyString);

    if (xrcDir.DirExists() && wxSetWorkingDirectory(xrcDir.GetFullPath()) && !wxFindFirstFile(wxT("*.xrc")).empty()) {
        xr->Load(wxT("*.xrc"));
    } else {
        // finally, load built-in xrc
        xr->Load(wxT("memory:wxvbam.xrs"));
    }

    wxSetWorkingDirectory(cwd);

    if (!config_file_.IsOk()) {
        // Set up the default configuration file.
        // This needs to be in a subdir to support other config as well.
        // NOTE: this does not support XDG (freedesktop.org) paths.
        // We rely on wx to build the paths in a cross-platform manner. However,
        // the wxFileName APIs are weird and don't quite work as intended so we
        // use the wxString APIs for files instead.
        const wxString old_conf_file(
            wxFileName(GetConfigurationPath(), kOldConfigFileName)
                .GetFullPath());
        const wxString new_conf_file(
            wxFileName(GetConfigurationPath(), knewConfigFileName)
                .GetFullPath());

        if (wxDirExists(new_conf_file)) {
            wxLogError(_("Invalid configuration file provided: %s"),
                       new_conf_file);
            return false;
        }

        // /MIGRATION
        // Migrate from 'vbam.conf' to 'vbam.ini' to manage a single config
        // file for all platforms.
        if (!wxFileExists(new_conf_file) && wxFileExists(old_conf_file)) {
            wxRenameFile(old_conf_file, new_conf_file, false);
        }
        // /END_MIGRATION

        config_file_ = new_conf_file;
    }

    if (!config_file_.IsOk() || wxDirExists(config_file_.GetFullPath())) {
        wxLogError(_("Invalid configuration file provided: %s"),
                   config_file_.GetFullPath());
        return false;
    }

    // wx takes ownership of the wxFileConfig here. It will be deleted on app
    // destruction.
    wxConfigBase::DontCreateOnDemand();
    wxConfigBase::Set(new wxFileConfig("vbam", wxEmptyString,
                                       config_file_.GetFullPath(),
                                       wxEmptyString, wxCONFIG_USE_LOCAL_FILE));

    // wx does not create the directories by itself so do it here, if needed.
    if (!wxDirExists(config_file_.GetPath())) {
       config_file_.Mkdir(wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
    }

    // Load the default options.
    load_opts(!config_file_.Exists());

    if (wxvbam_locale)
        wxDELETE(wxvbam_locale);

    wxvbam_locale = new wxLocale;

    language = OPTION(kLocale);

    if (language != wxLANGUAGE_DEFAULT) {
        wxvbam_locale->Init(language, wxLOCALE_LOAD_DEFAULT);
    } else {
        wxvbam_locale->Init();
    }

    // load selected language

#ifdef _WIN32
    if (OPTION(kExternalTranslations) == false)
        wxTranslations::Get()->SetLoader(new wxResourceTranslationsLoader);
#endif

    wxvbam_locale->AddCatalog("wxvbam");

    // wxGLCanvas segfaults under wayland before wx 3.2
#if defined(HAVE_WAYLAND_SUPPORT) && !defined(HAVE_WAYLAND_EGL)
    if (UsingWayland()) {
        OPTION(kDispRenderMethod) = config::RenderMethod::kSimple;
    }
#endif

    // process command-line options
    for (size_t i = 0; i < pending_optset.size(); i++) {
        auto parts = config::str_split(pending_optset[i], wxT('='));
        opt_set(parts[0], parts[1]);
    }

    pending_optset.clear();
    wxFileName vba_over(GetConfigurationPath(), wxT("vba-over.ini"));
    wxFileName rdb(GetConfigurationPath(), wxT("Nintendo - Game Boy Advance*.dat"));
    wxFileName scene_rdb(GetConfigurationPath(), wxT("Nintendo - Game Boy Advance (Scene)*.dat"));
    wxFileName nointro_rdb(GetConfigurationPath(), wxT("Official No-Intro Nintendo Gameboy Advance Number (Date).xml"));
    wxString f = wxFindFirstFile(nointro_rdb.GetFullPath(), wxFILE);

    if (!f.empty() && wxFileName(f).IsFileReadable())
        rom_database_nointro = f;

    f = wxFindFirstFile(scene_rdb.GetFullPath(), wxFILE);

    if (!f.empty() && wxFileName(f).IsFileReadable())
        rom_database_scene = f;

    f = wxFindFirstFile(rdb.GetFullPath(), wxFILE);

    while (!f.empty()) {
        if (f == rom_database_scene.GetFullPath()) {
            f = wxFindNextFile();
        } else if (wxFileName(f).IsFileReadable()) {
            rom_database = f;
            break;
        }
    }

    // load gb-over.ini
    wxMemoryInputStream stream(gb_builtin_over, sizeof(gb_builtin_over));
    gb_overrides_ = std::make_unique<wxFileConfig>(stream);

    // load vba-over.ini
    // rather than dealing with wxConfig's broken search path, just use
    // the same one that the xrc overrides use
    // this also allows us to override a group at a time, add commments, and
    // add the file from which the group came
    wxMemoryInputStream mis(builtin_over, sizeof(builtin_over));
    overrides_ = std::make_unique<wxFileConfig>(mis);
    wxRegEx cmtre;
    // not the most efficient thing to do: read entire file into a string
    // just to parse the comments out
    wxString bovs((const char*)builtin_over, wxConvUTF8, sizeof(builtin_over));
    bool cont;
    wxString s;
    long grp_idx;
#define CMT_RE_START wxT("(^|[\n\r])# ?([^\n\r]*)(\r?\n|\r)\\[")

    for (cont = overrides_->GetFirstGroup(s, grp_idx); cont;
         cont = overrides_->GetNextGroup(s, grp_idx)) {
        // apparently even MacOSX sometimes uses the old \r by itself
        wxString cmt(CMT_RE_START);
        cmt += s + wxT("\\]");

        if (cmtre.Compile(cmt) && cmtre.Matches(bovs))
            cmt = cmtre.GetMatch(bovs, 2);
        else
            cmt = wxEmptyString;

        overrides_->Write(s + wxT("/comment"), cmt);
    }

    if (vba_over.FileExists()) {
        wxStringOutputStream sos;
        wxFileInputStream fis(vba_over.GetFullPath());
        // not the most efficient thing to do: read entire file into a string
        // just to parse the comments out
        fis.Read(sos);
        // rather than assuming the file is seekable, use the string we just
        // read as an input stream
        wxStringInputStream sis(sos.GetString());
        wxFileConfig ov(sis);

        for (cont = ov.GetFirstGroup(s, grp_idx); cont;
             cont = ov.GetNextGroup(s, grp_idx)) {
            overrides_->DeleteGroup(s);
            overrides_->SetPath(s);
            ov.SetPath(s);
            overrides_->Write(wxT("path"), GetConfigurationPath());
            // apparently even MacOSX sometimes uses \r by itself
            wxString cmt(CMT_RE_START);
            cmt += s + wxT("\\]");

            if (cmtre.Compile(cmt) && cmtre.Matches(sos.GetString()))
                cmt = cmtre.GetMatch(sos.GetString(), 2);
            else
                cmt = wxEmptyString;

            overrides_->Write(wxT("comment"), cmt);
            long ent_idx;

            for (cont = ov.GetFirstEntry(s, ent_idx); cont;
                 cont = ov.GetNextEntry(s, ent_idx))
                overrides_->Write(s, ov.Read(s, wxEmptyString));

            ov.SetPath(wxT("/"));
            overrides_->SetPath(wxT("/"));
        }
    }

    // We need to gather this information before crating the MainFrame as the
    // OnSize / OnMove event handlers can fire during construction.
    const wxRect client_rect(
        OPTION(kGeomWindowX).Get(),
        OPTION(kGeomWindowY).Get(),
        OPTION(kGeomWindowWidth).Get(),
        OPTION(kGeomWindowHeight).Get());
    const bool is_fullscreen = OPTION(kGeomFullScreen);
    const bool is_maximized = OPTION(kGeomIsMaximized);

    // Create the main window.
    frame = wxDynamicCast(xr->LoadFrame(NULL, "MainFrame"), MainFrame);
    if (!frame) {
        wxLogError(_("Could not create main window"));
        return false;
    }

    // Create() cannot be overridden easily
    if (!frame->BindControls()) {
        return false;
    }

    // Ensure we are not drawing out of bounds.
    if (widgets::GetDisplayRect().Intersects(client_rect)) {
        frame->SetSize(client_rect);
    }

    if (is_maximized) {
        frame->Maximize();
    }
    if (is_fullscreen && wxGetApp().pending_load != wxEmptyString)
        frame->ShowFullScreen(is_fullscreen);

    frame->Show(true);

    // Windows can render the taskbar icon late if this is done in MainFrame
    // It may also not update at all until the Window has been minimized/restored
    // This seems timing related, possibly based on HWND
    // So do this here since it reliably draws the Taskbar icon on Window creation.
    frame->BindAppIcon();

#ifdef __WXGTK__
    // Initialize menubar mnemonic visibility and global pointers
    wxMenuBar* menubar = frame->GetMenuBar();
    if (menubar) {
        GtkWidget* menubar_widget = menubar->GetHandle();
        if (menubar_widget) {
            g_menubar_widget = menubar_widget;
            GtkWidget* toplevel = gtk_widget_get_toplevel(menubar_widget);
            if (toplevel && GTK_IS_WINDOW(toplevel)) {
                g_main_window = toplevel;
                gtk_window_set_mnemonics_visible(GTK_WINDOW(toplevel), TRUE);
            }
            ForceMenubarMnemonicsVisible(menubar_widget);
        }
    }
#endif


#ifndef NO_ONLINEUPDATES
    initAutoupdater();
#endif
    return true;
}

int wxvbamApp::OnRun()
{
    if (console_mode)
    {
        // we could check for our own error codes here...
        return console_status;
    }
    else
    {
        return wxApp::OnRun();
    }
}

// called on --help
bool wxvbamApp::OnCmdLineHelp(wxCmdLineParser& parser)
{
    wxApp::OnCmdLineHelp(parser);
    console_mode = true;
    return true;
}

bool wxvbamApp::OnCmdLineError(wxCmdLineParser& parser)
{
    wxApp::OnCmdLineError(parser);
    console_mode = true;
    console_status = 1;
    return true;
}

void wxvbamApp::OnInitCmdLine(wxCmdLineParser& cl)
{
    wxApp::OnInitCmdLine(cl);
    cl.SetLogo(wxT("VisualBoyAdvance-M\n"));
// 2.9 decided to change all of these from wxChar to char for some
// reason
#if wxCHECK_VERSION(2, 9, 0)
// N_(x) returns x
#define t(x) x
#else
// N_(x) returns wxT(x)
#define t(x) wxT(x)
#endif
    // while I would rather the long options be translated, there is merit
    // to the idea that command-line syntax should not change based on
    // locale
    static wxCmdLineEntryDesc opttab[] = {
        { wxCMD_LINE_OPTION, NULL, t("save-xrc"),
            N_("Save built-in XRC file and exit"),
            wxCMD_LINE_VAL_STRING, 0 },
        { wxCMD_LINE_OPTION, NULL, t("save-over"),
            N_("Save built-in vba-over.ini and exit"),
            wxCMD_LINE_VAL_STRING, 0 },
        { wxCMD_LINE_SWITCH, NULL, t("print-cfg-path"),
            N_("Print configuration path and exit"),
            wxCMD_LINE_VAL_NONE, 0 },
        { wxCMD_LINE_SWITCH, t("f"), t("fullscreen"),
            N_("Start in full-screen mode"),
            wxCMD_LINE_VAL_NONE, 0 },
        { wxCMD_LINE_OPTION, t("c"), t("config"),
            N_("Set a configuration file"),
            wxCMD_LINE_VAL_STRING, 0 },
#if !defined(NO_LINK) && !defined(__WXMSW__)
        { wxCMD_LINE_SWITCH, t("s"), t("delete-shared-state"),
            N_("Delete shared link state first, if it exists"),
            wxCMD_LINE_VAL_NONE, 0 },
#endif
        // stupid wx cmd line parser doesn't support duplicate options
        //    { wxCMD_LINE_OPTION, t("o"),  t("option"),
        //        _("Set configuration option; <opt>=<value> or help for list"),
        { wxCMD_LINE_SWITCH, t("o"), t("list-options"),
            N_("List all settable options and exit"),
            wxCMD_LINE_VAL_NONE, 0 },
        { wxCMD_LINE_PARAM, NULL, NULL,
            N_("ROM file"), wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL },
        { wxCMD_LINE_PARAM, NULL, NULL,
            N_("<config>=<value>"), wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_MULTIPLE | wxCMD_LINE_PARAM_OPTIONAL },
        { wxCMD_LINE_NONE, NULL, NULL, NULL, wxCMD_LINE_VAL_NONE, 0 }
    };
// 2.9 automatically translates desc, but 2.8 doesn't
#if !wxCHECK_VERSION(2, 9, 0)

    for (int i = 0; opttab[i].kind != wxCMD_LINE_NONE; i++)
        opttab[i].description = wxGetTranslation(opttab[i].description);

#endif
    // note that "SetDesc" actually Adds rather than replaces
    // so system standard (port-dependent) options are still included:
    //   -h/--help  --verbose --theme --mode
    cl.SetDesc(opttab);
}

bool wxvbamApp::OnCmdLineParsed(wxCmdLineParser& cl)
{
    if (!wxApp::OnCmdLineParsed(cl))
        return false;

    wxString s;

    if (cl.Found(wxT("save-xrc"), &s)) {
        // This was most likely done on a command line, so use
        // stderr instead of gui for messages
        wxLog::SetActiveTarget(new wxLogStderr);
        wxFileSystem fs;
        wxFSFile* f = fs.OpenFile(wxT("memory:wxvbam.xrs#zip:wxvbam.xrs$wxvbam.xrc"));

        if (!f) {
            wxLogError(_("Configuration / build error: can't find built-in xrc"));
            return false;
        }

        wxFileOutputStream os(s);
        os.Write(*f->GetStream());
        delete f;
        wxString lm;
        lm.Printf(_("Wrote built-in configuration to %s.\n"
                    "To override, remove all but changed root node(s). First found root node of correct name in any ."
                    "xrc or .xrs files in following search path overrides built-in:"),
            s.c_str());
        tack_full_path(lm);
        wxLogMessage(lm);
        console_mode = true;
        return true;
    }

    if (cl.Found(wxT("print-cfg-path"))) {
        // This was most likely done on a command line, so use
        // stderr instead of gui for messages
        wxLog::SetActiveTarget(new wxLogStderr);
        wxString lm(_("Configuration is read from, in order:"));
        tack_full_path(lm);
        wxLogMessage(lm);
        console_mode = true;
        return true;
    }

    if (cl.Found(wxT("save-over"), &s)) {
        // This was most likely done on a command line, so use
        // stderr instead of gui for messages
        wxLog::SetActiveTarget(new wxLogStderr);
        wxFileOutputStream os(s);
        os.Write(builtin_over, sizeof(builtin_over));
        wxString lm;
        lm.Printf(_("Wrote built-in override file to %s\n"
                    "To override, delete all but changed section. First found section is used from search path:"),
            s.c_str());
        wxString oi = wxFileName::GetPathSeparator();
        oi += wxT("vba-over.ini");
        tack_full_path(lm, oi);
        lm.append(_("\n\tbuilt-in"));
        wxLogMessage(lm);
        console_mode = true;
        return true;
    }

    if (cl.Found(wxT("f"))) {
        pending_fullscreen = true;
    }

    if (cl.Found(wxT("o"))) {
        // This was most likely done on a command line, so use
        // stderr instead of gui for messages
        wxLog::SetActiveTarget(new wxLogStderr);

        wxPrintf(_("Options set from the command line are saved if any"
                   " configuration changes are made in the user interface.\n\n"
                   "For flag options, true and false are specified as 1 and 0, respectively.\n\n"));

        for (const config::Option& opt : config::Option::All()) {
            wxPrintf("%s\n", opt.ToHelperString());
        }

        wxPrintf(_("The commands available for the Keyboard/* option are:\n\n"));
        for (const cmditem& cmd_item : cmdtab) {
            wxPrintf("%s (%s)\n", cmd_item.cmd.c_str(), cmd_item.name.c_str());
        }

        console_mode = true;
        return true;
    }

    if (cl.Found(wxT("c"), &s)) {
        wxFileName vbamconf(s);
        if (!vbamconf.FileExists()) {
            wxLogError(_("Configuration file not found."));
            return false;
        }
        config_file_ = s;
    }

#if !defined(NO_LINK) && !defined(__WXMSW__)

    if (cl.Found(wxT("s"))) {
        CleanLocalLink();
    }

#endif
    int nparm = cl.GetParamCount();
    bool complained = false, gotfile = false;

    for (int i = 0; i < nparm; i++) {
        auto p     = cl.GetParam(i);
        auto parts = config::str_split(p, wxT('='));

        if (parts.size() > 1) {
            opt_set(parts[0], parts[1]);

            pending_optset.push_back(p);
        }
        else {
            if (!gotfile) {
                pending_load = p;
                gotfile = true;
            } else {
                if (!complained) {
                    wxFprintf(stderr, _("Bad configuration option or multiple ROM files given:\n"));
                    wxFprintf(stderr, wxT("%s\n"), pending_load.c_str());
                    complained = true;
                }

                wxFprintf(stderr, wxT("%s\n"), p.c_str());
            }
        }
    }

    return true;
}

wxString wxvbamApp::GetConfigDir()
{
    return GetAbsolutePath(wxString((get_xdg_user_config_home() + kDotDir).c_str(), wxConvLibc));
}

wxString wxvbamApp::GetDataDir()
{
    return GetAbsolutePath(wxString((get_xdg_user_data_home() + kDotDir).c_str(), wxConvLibc));
}

wxvbamApp::~wxvbamApp() {
    if (home != NULL)
    {
        free(home);
        home = NULL;
    }

#ifndef NO_ONLINEUPDATES
    shutdownAutoupdater();
#endif
}

wxEvtHandler* wxvbamApp::event_handler() {
    // Use the active window, if any.
    wxWindow* focused_window = wxWindow::FindFocus();
    if (focused_window) {
        return focused_window;
    }

    if (!frame) {
        return NULL;
    }

    auto panel = frame->GetPanel();
    if (!panel || !panel->panel) {
        return NULL;
    }

    if (OPTION(kUIAllowJoystickBackgroundInput) || OPTION(kUIAllowKeyboardBackgroundInput)) {
        // Use the game panel, if the background polling option is enabled.
        return panel->panel->GetWindow()->GetEventHandler();
    }

    return NULL;
}

MainFrame::MainFrame()
    : wxFrame(),
      paused(false),
      menus_opened(0),
      dialog_opened(0),
#ifndef NO_LINK
      gba_link_observer_(config::OptionID::kGBALinkHost,
                         std::bind(&MainFrame::EnableNetworkMenu, this)),
#endif
      keep_on_top_styler_(this),
      status_bar_observer_(config::OptionID::kGenStatusBar,
                           std::bind(&MainFrame::OnStatusBarChanged, this)) {
}

MainFrame::~MainFrame() {
#ifndef NO_LINK
    CloseLink();
#endif
}

void MainFrame::SetStatusBar(wxStatusBar* menu_bar) {
    wxFrame::SetStatusBar(menu_bar);
    // This will take care of hiding the menu bar at startup, if needed.
    menu_bar->Show(OPTION(kGenStatusBar));
}

void MainFrame::OnStatusBarChanged() {
    GetStatusBar()->Show(OPTION(kGenStatusBar));
    SendSizeEvent();
    panel->AdjustSize(false);
    SendSizeEvent();
}

BEGIN_EVENT_TABLE(MainFrame, wxFrame)
#include "wx/cmd-evtable.h"
EVT_CONTEXT_MENU(MainFrame::OnMenu)
// this is the main window focus?  Or EVT_SET_FOCUS/EVT_KILL_FOCUS?
EVT_ACTIVATE(MainFrame::OnActivate)
// requires DragAcceptFiles(true); even then may not do anything
EVT_DROP_FILES(MainFrame::OnDropFile)

// for window geometry
EVT_MOVE(MainFrame::OnMove)
EVT_MOVE_START(MainFrame::OnMoveStart)
EVT_MOVE_END(MainFrame::OnMoveEnd)
EVT_SIZE(MainFrame::OnSize)
EVT_ICONIZE(MainFrame::OnIconize)

// For tracking menubar state on all platforms.
EVT_MENU_OPEN(MainFrame::MenuPopped)
EVT_MENU_CLOSE(MainFrame::MenuPopped)
EVT_MENU_HIGHLIGHT_ALL(MainFrame::MenuPopped)

END_EVENT_TABLE()

void MainFrame::OnActivate(wxActivateEvent& event) {
    const bool focused = event.GetActive();

    if (!panel) {
        // Nothing more to do if no game is active.
        return;
    }

    if (focused) {
        // Set the focus to the game panel.
        panel->SetFocus();
    }

    if (OPTION(kPrefPauseWhenInactive)) {
        // Handle user preferences for pausing the game when the window is inactive.
        if (focused && !paused) {
            panel->Resume();
        } else if (!focused) {
            panel->Pause();
        }
    }
}

void MainFrame::OnDropFile(wxDropFilesEvent& event)
{
    wxString* f = event.GetFiles();
    // ignore all but last
    wxGetApp().pending_load = f[event.GetNumberOfFiles() - 1];
}

void MainFrame::OnMenu(wxContextMenuEvent& event)
{
    if (IsFullScreen() && ctx_menu) {
        wxPoint p(event.GetPosition());
#if 0 // wx actually recommends ignoring the position

        if (p != wxDefaultPosition)
            p = ScreenToClient(p);

#endif
        PopupMenu(ctx_menu, p);
    }
}

void MainFrame::OnMove(wxMoveEvent&) {
    if (!init_complete_) {
        return;
    }
    if (!IsFullScreen() && !IsMaximized()) {
        const wxPoint window_pos = GetScreenPosition();
        OPTION(kGeomWindowX) = window_pos.x;
        OPTION(kGeomWindowY) = window_pos.y;
    }
}

// On Windows pause sound when moving and resizing the window to prevent dsound
// from looping.
void MainFrame::OnMoveStart(wxMoveEvent&) {
#ifdef __WXMSW__
    soundPause();
#endif
}

void MainFrame::OnMoveEnd(wxMoveEvent&) {
#ifdef __WXMSW__
    soundResume();
#endif
}

void MainFrame::OnSize(wxSizeEvent& event)
{
    wxFrame::OnSize(event);
    if (!init_complete_) {
        return;
    }
    const wxRect window_rect = GetRect();
    const wxPoint window_pos = GetScreenPosition();

    if (!IsFullScreen() && !IsMaximized()) {
        if (window_rect.GetHeight() > 0 && window_rect.GetWidth() > 0) {
            OPTION(kGeomWindowHeight) = window_rect.GetHeight();
            OPTION(kGeomWindowWidth) = window_rect.GetWidth();
        }
        OPTION(kGeomWindowX) = window_pos.x;
        OPTION(kGeomWindowY) = window_pos.y;
    }

    OPTION(kGeomIsMaximized) = IsMaximized();
    OPTION(kGeomFullScreen) = IsFullScreen();
}

void MainFrame::OnIconize(wxIconizeEvent& event)
{
    (void)event; // unused
    if (!init_complete_) {
        return;
    }

    if (OPTION(kPrefPauseWhenInactive)) {
        panel->Pause();
    }
}

// Get system name string for the currently loaded ROM
// Returns expanded system name based on the ROM type
static wxString GetSystemName(GameArea* panel) {
    if (!panel)
        return wxT("GameBoy Advance");

    IMAGE_TYPE game_type = panel->game_type();
    if (game_type == IMAGE_GBA)
        return wxT("GameBoy Advance");

    // For Game Boy ROMs, check for GBC and SGB modes
    if (game_type == IMAGE_GB) {
        if (gbCgbMode)
            return wxT("GameBoy Color");
        if (gbSgbMode)
            return wxT("Super GameBoy");
        return wxT("GameBoy");
    }

    return wxT("GameBoy Advance");
}

wxString MainFrame::GetGamePath(wxString path)
{
    wxString game_path = path;

    // Expand %s to system name (GBA, GBC, SGB, or GB)
    if (game_path.Contains(wxT("%s"))) {
        wxString system_name = GetSystemName(panel);
        game_path.Replace(wxT("%s"), system_name);
    }

    if (game_path.size()) {
        game_path = wxGetApp().GetAbsolutePath(game_path);
        // Try to create the directory if it doesn't exist
        if (!wxFileName::DirExists(game_path)) {
            if (!wxFileName::Mkdir(game_path, 0777, wxPATH_MKDIR_FULL)) {
                wxMessageBox(
                    wxString::Format(_("Could not create directory:\n%s\n\nPlease check your configured path in Options > Directories."), game_path),
                    _("Directory Error"),
                    wxOK | wxICON_ERROR);
            }
        }
    } else {
        game_path = panel->game_dir();
        wxFileName::Mkdir(game_path, 0777, wxPATH_MKDIR_FULL);
    }

    if (!wxFileName::DirExists(game_path))
        game_path = wxFileName::GetCwd();

    if (!wxIsWritable(game_path))
    {
        game_path = wxGetApp().GetAbsolutePath(wxString((get_xdg_user_data_home() + kDotDir).c_str(), wxConvLibc));
        wxFileName::Mkdir(game_path, 0777, wxPATH_MKDIR_FULL);
    }

    return game_path;
}

void MainFrame::enable_menus()
{
    for (const cmditem& cmd_item : cmdtab) {
        if (cmd_item.mask_flags && cmd_item.mi) {
            cmd_item.mi->Enable((cmd_item.mask_flags & cmd_enable) != 0);
        }
    }

    if (cmd_enable & CMDEN_SAVST)
        for (int i = 0; i < 10; i++)
            if (loadst_mi[i])
                loadst_mi[i]->Enable(state_ts[i].IsValid());
}

void MainFrame::update_state_ts(bool force)
{
    bool any_states = false;

    for (int i = 0; i < 10; i++) {
        if (force)
            state_ts[i] = wxInvalidDateTime;

        if (panel->game_type() != IMAGE_UNKNOWN) {
            wxString fn;
            fn.Printf(SAVESLOT_FMT, panel->game_name().wc_str(), i + 1);
            wxFileName fp(panel->state_dir(), fn);
            wxDateTime ts; // = wxInvalidDateTime

            if (fp.IsFileReadable()) {
                ts = fp.GetModificationTime();
                any_states = true;
            }

            // if(ts != state_ts[i] || (force && !ts.IsValid())) {
            // to prevent assertions (stupid wx):
            if (ts.IsValid() != state_ts[i].IsValid() || (ts.IsValid() && ts != state_ts[i]) || (force && !ts.IsValid())) {
                // wx has no easy way of doing the -- bit independent
                // of locale
                // so use a real date and substitute all digits
                wxDateTime fts = ts.IsValid() ? ts : wxDateTime::Now();
                wxString df = fts.Format(wxT("0&0 %x %X"));

                if (!ts.IsValid())
                    for (size_t j = 0; j < df.size(); j++)
                        if (wxIsdigit(df[j]))
                            df[j] = wxT('-');

                df[0] = i == 9 ? wxT('1') : wxT(' ');
                df[2] = wxT('0') + (i + 1) % 10;

                if (loadst_mi[i]) {
                    wxString lab = loadst_mi[i]->GetItemLabel();
                    size_t tab = lab.find(wxT('\t')), dflen = df.size();

                    if (tab != wxString::npos)
                        df.append(lab.substr(tab));

                    loadst_mi[i]->SetItemLabel(df);
                    loadst_mi[i]->Enable(ts.IsValid());

                    if (tab != wxString::npos)
                        df.resize(dflen);
                }

                if (savest_mi[i]) {
                    wxString lab = savest_mi[i]->GetItemLabel();
                    size_t tab = lab.find(wxT('\t'));

                    if (tab != wxString::npos)
                        df.append(lab.substr(tab));

                    savest_mi[i]->SetItemLabel(df);
                }
            }

            state_ts[i] = ts;
        }
    }

    int cmd_flg = any_states ? CMDEN_SAVST : 0;

    if ((cmd_enable & CMDEN_SAVST) != cmd_flg) {
        cmd_enable = (cmd_enable & ~CMDEN_SAVST) | cmd_flg;
        enable_menus();
    }
}

int MainFrame::oldest_state_slot()
{
    wxDateTime ot;
    int os = -1;

    for (int i = 0; i < 10; i++) {
        if (!state_ts[i].IsValid())
            return i + 1;

        if (os < 0 || state_ts[i] < ot) {
            os = i;
            ot = state_ts[i];
        }
    }

    return os + 1;
}

int MainFrame::newest_state_slot()
{
    wxDateTime nt;
    int ns = -1;

    for (int i = 0; i < 10; i++) {
        if (!state_ts[i].IsValid())
            continue;

        if (ns < 0 || state_ts[i] > nt) {
            ns = i;
            nt = state_ts[i];
        }
    }

    return ns + 1;
}

void MainFrame::ResetRecentAccelerators() {
    for (int i = wxID_FILE1; i <= wxID_FILE10; i++) {
        wxMenuItem* menu_item = recent->FindItem(i);
        if (!menu_item) {
            break;
        }
        ResetMenuItemAccelerator(menu_item);
    }
}

void MainFrame::ResetMenuAccelerators() {
    for (const cmditem& cmd_item : cmdtab) {
        if (!cmd_item.mi) {
            continue;
        }
        ResetMenuItemAccelerator(cmd_item.mi);
    }
    ResetRecentAccelerators();
}

#ifdef __WXGTK__
// Forward declarations for GTK mnemonic visibility functions
static void EnsureMnemonicsVisible(GtkWidget* widget);
static void ForceMenuMnemonicsVisible(GtkWidget* menu_widget);
static void ScheduleMnemonicTimeouts(GtkWidget* menu_widget);
static void DisableTakeFocusAndShowMnemonics(GtkMenuShell* menu_shell, GtkWidget* menubar_widget);
static gboolean HandleMenuKeyboardNavigation(GtkWidget* widget, GdkEventKey* event, gpointer user_data);
// Note: g_main_window, g_menubar_widget, and ForceMenubarMnemonicsVisible are declared at the top of the file
#endif

void MainFrame::MenuPopped(wxMenuEvent& evt) {
    // Track menu open/close state for audio handling
    // On macOS, the menubar is in the OS menubar and doesn't need this tracking
#if !defined(__WXMAC__)
#if defined(__WXMSW__)
    if (evt.GetEventType() == wxEVT_MENU_CLOSE && (evt.GetMenu() == NULL || evt.GetMenu()->GetMenuBar() == GetMenuBar()))
#else
    if (evt.GetEventType() == wxEVT_MENU_CLOSE && evt.GetMenu() && evt.GetMenu()->GetMenuBar() == GetMenuBar())
#endif
        SetMenusOpened(false);
    else if (evt.GetEventType() == wxEVT_MENU_OPEN)
        SetMenusOpened(true);
#endif

#ifdef __WXGTK__
    // Force mnemonic underlines to always be visible (GTK3 hides them by default)
    EnsureMnemonicsVisible(GetHandle());

    wxMenuBar* menubar = GetMenuBar();
    GtkWidget* menubar_widget = nullptr;
    if (menubar) {
        menubar_widget = menubar->GetHandle();
        if (menubar_widget)
            ForceMenubarMnemonicsVisible(menubar_widget);
    }

    wxMenu* menu = evt.GetMenu();
    if (menu && evt.GetEventType() == wxEVT_MENU_OPEN) {
        GtkWidget* menu_widget = menu->m_menu;
        if (menu_widget && GTK_IS_MENU(menu_widget)) {
            ForceMenuMnemonicsVisible(menu_widget);

            // Connect keyboard handler for menus opened with mouse
            // Use a quark to ensure we only connect once per menu
            static GQuark key_handler_quark = g_quark_from_static_string("vbam-menu-key-handler");
            if (!g_object_get_qdata(G_OBJECT(menu_widget), key_handler_quark)) {
                g_signal_connect(menu_widget, "key-press-event",
                                G_CALLBACK(HandleMenuKeyboardNavigation), this);
                g_object_set_qdata(G_OBJECT(menu_widget), key_handler_quark, GINT_TO_POINTER(1));
            }

            // Disable GTK's default keyboard handling which can interfere
            if (menubar_widget)
                DisableTakeFocusAndShowMnemonics(GTK_MENU_SHELL(menu_widget), menubar_widget);
        }
    }

    ScheduleMnemonicTimeouts(nullptr);
#endif

    evt.Skip();
}

void MainFrame::SetMenusOpened(bool state) {
    menus_opened = state;
#if defined(__WXMSW__)
    // On Windows, opening the menubar will stop the app, but DirectSound will
    // loop, so we pause audio here.
    if (menus_opened)
        soundPause();
    else if (!paused)
        soundResume();
#endif
}

// ShowModal that also disables emulator loop
// uses dialog_opened as a nesting counter
int MainFrame::ShowModal(wxDialog* dlg)
{
    dlg->SetWindowStyle(dlg->GetWindowStyle() | wxCAPTION | wxRESIZE_BORDER);
    CheckPointer(dlg);
    StartModal();
    int ret = dlg->ShowModal();
    StopModal();
    return ret;
}

void MainFrame::StartModal()
{
    // workaround for lack of wxGTK mouse motion events: unblank
    // pointer when dialog popped up
    // it will auto-hide again once game resumes
    panel->ShowPointer();
    //panel->Pause();
    ++dialog_opened;
}

void MainFrame::StopModal()
{
    if (!dialog_opened) // technically an error in the caller
        return;

    --dialog_opened;

    if (!IsPaused())
        panel->Resume();
}

#ifndef NO_LINK

LinkMode MainFrame::GetConfiguredLinkMode()
{
    switch (gopts.gba_link_type) {
    case 0:
        return LINK_DISCONNECTED;
        break;

    case 1:
        if (OPTION(kGBALinkProto))
                return LINK_CABLE_IPC;
        else
            return LINK_CABLE_SOCKET;

        break;

    case 2:
        if (OPTION(kGBALinkProto))
            return LINK_RFU_IPC;
        else
            return LINK_RFU_SOCKET;

        break;

    case 3:
        return LINK_GAMECUBE_DOLPHIN;
        break;

    case 4:
        if (OPTION(kGBALinkProto))
            return LINK_GAMEBOY_IPC;
        else
            return LINK_GAMEBOY_SOCKET;

        break;

    default:
        return LINK_DISCONNECTED;
        break;
    }
}

#endif  // NO_LINK

void MainFrame::IdentifyRom()
{
    if (!panel->rom_name.empty())
        return;

    panel->rom_name = panel->loaded_game.GetFullName();
    wxString name;
    wxString scene_rls;
    wxString scene_name;
    wxString rom_crc32_str;
    rom_crc32_str.Printf(wxT("%08X"), panel->rom_crc32);

    if (wxGetApp().rom_database_nointro.FileExists()) {
        wxFileInputStream input(wxGetApp().rom_database_nointro.GetFullPath());
        wxTextInputStream text(input, wxT("\x09"), wxConvUTF8);

        while (input.IsOk() && !input.Eof()) {
            wxString line = text.ReadLine();

            if (line.Contains(wxT("<releaseNumber>"))) {
                scene_rls = line.AfterFirst('>').BeforeLast('<');
            }

            if (line.Contains(wxT("romCRC ")) && line.Contains(rom_crc32_str)) {
                panel->rom_scene_rls = scene_rls;
                break;
            }
        }
    }

    if (wxGetApp().rom_database_scene.FileExists()) {
        wxFileInputStream input(wxGetApp().rom_database_scene.GetFullPath());
        wxTextInputStream text(input, wxT("\x09"), wxConvUTF8);

        while (input.IsOk() && !input.Eof()) {
            wxString line = text.ReadLine();

            if (line.StartsWith(wxT("\tname"))) {
                scene_name = line.AfterLast(' ').BeforeLast('"');
            }

            if (line.StartsWith(wxT("\trom")) && line.Contains(wxT("crc ") + rom_crc32_str)) {
                panel->rom_scene_rls_name = scene_name;
                panel->rom_name = scene_name;
                break;
            }
        }
    }

    if (wxGetApp().rom_database.FileExists()) {
        wxFileInputStream input(wxGetApp().rom_database.GetFullPath());
        wxTextInputStream text(input, wxT("\x09"), wxConvUTF8);

        while (input.IsOk() && !input.Eof()) {
            wxString line = text.ReadLine();

            if (line.StartsWith(wxT("\tname"))) {
                name = line.AfterFirst('"').BeforeLast('"');
            }

            if (line.StartsWith(wxT("\trom")) && line.Contains(wxT("crc ") + rom_crc32_str)) {
                panel->rom_name = name;
                break;
            }
        }
    }
}

// Forward declarations
#ifndef __WXGTK__
static bool SearchAndActivateMenuItem(wxMenu* menu, wxChar mnemonic, int depth = 0);
#else
static bool ActivateMenuItemByMnemonic(wxMenu* menu, wxChar mnemonic);
#endif

#ifdef __WXGTK__
//==============================================================================
// GTK Menu Keyboard Navigation and Mnemonic Handling
//==============================================================================
// This section implements custom keyboard navigation for menus on GTK/Linux.
// GTK3 has issues with mnemonic (underline) visibility and keyboard navigation
// that require custom handling:
//
// 1. Mnemonic underlines are hidden by default until Alt is pressed
// 2. Keyboard navigation can trigger unwanted focus changes
// 3. First-letter navigation (pressing a letter to activate matching item)
//    needs to work in addition to explicit mnemonics
//
// The code below:
// - Forces mnemonic underlines to always be visible
// - Adds first-letter underlines to items without matching mnemonics
// - Implements custom arrow key navigation
// - Handles letter/number key activation of menu items
//==============================================================================

// Ensure mnemonic underlines are visible on the window
static void EnsureMnemonicsVisible(GtkWidget* widget) {
    if (!widget)
        return;
    GtkWidget* toplevel = gtk_widget_get_toplevel(widget);
    if (toplevel && GTK_IS_WINDOW(toplevel)) {
        gtk_window_set_mnemonics_visible(GTK_WINDOW(toplevel), TRUE);
        g_main_window = toplevel;
    }
}

// Forward declaration
static void ApplyPermanentUnderline(GtkLabel* label);

// Force mnemonics visible on menubar items using Pango attributes
static void ForceMenubarMnemonicsVisible(GtkWidget* menubar_widget) {
    if (!menubar_widget || !GTK_IS_MENU_BAR(menubar_widget))
        return;

    g_menubar_widget = menubar_widget;

    // Track whether we've already applied permanent underlines to menubar labels
    static GQuark menubar_underline_quark = g_quark_from_static_string("vbam-menubar-underline-done");

    if (!g_object_get_qdata(G_OBJECT(menubar_widget), menubar_underline_quark)) {
        g_object_set_qdata(G_OBJECT(menubar_widget), menubar_underline_quark, GINT_TO_POINTER(1));

        GList* children = gtk_container_get_children(GTK_CONTAINER(menubar_widget));
        for (GList* l = children; l != NULL; l = l->next) {
            GtkWidget* item = GTK_WIDGET(l->data);
            if (GTK_IS_MENU_ITEM(item)) {
                GtkWidget* label = gtk_bin_get_child(GTK_BIN(item));
                if (label && GTK_IS_LABEL(label))
                    ApplyPermanentUnderline(GTK_LABEL(label));
            }
        }
        g_list_free(children);
    }
}

// Walk up the menu hierarchy forcing mnemonics on each level
static void ForceMenuHierarchyMnemonicsVisible(GtkWidget* menu_widget) {
    if (!menu_widget || !GTK_IS_MENU(menu_widget))
        return;

    ForceMenuMnemonicsVisible(menu_widget);

    GtkWidget* attach_widget = gtk_menu_get_attach_widget(GTK_MENU(menu_widget));
    while (attach_widget && GTK_IS_MENU_ITEM(attach_widget)) {
        GtkWidget* parent = gtk_widget_get_parent(attach_widget);
        if (parent && GTK_IS_MENU(parent)) {
            ForceMenuMnemonicsVisible(parent);
            attach_widget = gtk_menu_get_attach_widget(GTK_MENU(parent));
        } else if (parent && GTK_IS_MENU_BAR(parent)) {
            ForceMenubarMnemonicsVisible(parent);
            break;
        } else {
            break;
        }
    }
}

// Timeout callback to force mnemonics after GTK finishes processing
static gboolean ForceMenuMnemonicsTimeoutCallback(gpointer user_data) {
    GtkWidget* menu_widget = GTK_WIDGET(user_data);
    if (menu_widget && GTK_IS_MENU(menu_widget))
        ForceMenuHierarchyMnemonicsVisible(menu_widget);

    EnsureMnemonicsVisible(g_main_window);
    return G_SOURCE_REMOVE;
}

// Timeout callback for menubar mnemonics only
static gboolean ForceMenubarMnemonicsTimeoutCallback(gpointer user_data) {
    (void)user_data;
    if (g_menubar_widget)
        ForceMenubarMnemonicsVisible(g_menubar_widget);
    EnsureMnemonicsVisible(g_main_window);
    return G_SOURCE_REMOVE;
}

// Schedule timeout callbacks to force mnemonics after GTK finishes
static void ScheduleMnemonicTimeouts(GtkWidget* menu_widget) {
    if (menu_widget && GTK_IS_MENU(menu_widget)) {
        g_timeout_add(1, ForceMenuMnemonicsTimeoutCallback, menu_widget);
        g_timeout_add(10, ForceMenuMnemonicsTimeoutCallback, menu_widget);
        g_timeout_add(50, ForceMenuMnemonicsTimeoutCallback, menu_widget);
    } else {
        g_timeout_add(1, ForceMenubarMnemonicsTimeoutCallback, NULL);
        g_timeout_add(10, ForceMenubarMnemonicsTimeoutCallback, NULL);
        g_timeout_add(50, ForceMenubarMnemonicsTimeoutCallback, NULL);
        // Additional longer timeouts for when dialogs open asynchronously
        g_timeout_add(100, ForceMenubarMnemonicsTimeoutCallback, NULL);
        g_timeout_add(200, ForceMenubarMnemonicsTimeoutCallback, NULL);
    }
}

// Signal handler called when a menu is shown/mapped
static void OnMenuShow(GtkWidget* menu_widget, gpointer user_data) {
    (void)user_data;
    ForceMenuHierarchyMnemonicsVisible(menu_widget);
    ScheduleMnemonicTimeouts(menu_widget);
}

// Signal handler called when a menu item with submenu is selected
static void OnMenuItemSelect(GtkMenuItem* menu_item, gpointer user_data) {
    (void)user_data;

    GtkWidget* parent_menu = gtk_widget_get_parent(GTK_WIDGET(menu_item));
    if (parent_menu && GTK_IS_MENU(parent_menu)) {
        ForceMenuMnemonicsVisible(parent_menu);
        ScheduleMnemonicTimeouts(parent_menu);
    }

    GtkWidget* submenu = gtk_menu_item_get_submenu(menu_item);
    if (submenu && GTK_IS_MENU(submenu)) {
        ForceMenuMnemonicsVisible(submenu);
        ScheduleMnemonicTimeouts(submenu);
    }
}

// Apply permanent underline to a label using Pango attributes
// This underlines the mnemonic character (after _) or the first letter if no mnemonic
// Unlike GTK's mnemonic system, Pango attributes are always visible
static void ApplyPermanentUnderline(GtkLabel* label) {
    if (!label)
        return;

    const gchar* text = gtk_label_get_label(label);
    if (!text || !*text)
        return;

    // Parse text to find: mnemonic position (char after _) and first letter position
    // Build display text (without mnemonic underscores) and track underline position
    GString* display_text = g_string_new("");
    const gchar* p = text;
    gint display_pos = 0;
    gint display_underline_start = -1;
    gint display_underline_end = -1;
    gint mnemonic_display_pos = -1;  // Position in display text where mnemonic char appears
    gint first_char_display_pos = -1;  // Position in display text of first letter

    while (*p) {
        gunichar c = g_utf8_get_char(p);

        if (c == '_') {
            const gchar* next = g_utf8_next_char(p);
            if (*next) {
                gunichar next_c = g_utf8_get_char(next);
                if (next_c == '_') {
                    // Escaped underscore - output single underscore
                    g_string_append_c(display_text, '_');
                    display_pos++;
                    p = g_utf8_next_char(next);
                    continue;
                } else {
                    // Mnemonic underscore - skip it, next char is the mnemonic
                    mnemonic_display_pos = display_pos;
                    p = next;
                    continue;
                }
            }
        }

        // Track first non-underscore character position
        if (first_char_display_pos < 0 && c != '_') {
            first_char_display_pos = display_pos;
        }

        // Append the current character
        gchar buf[6];
        gint len = g_unichar_to_utf8(c, buf);
        g_string_append_len(display_text, buf, len);
        display_pos += len;

        p = g_utf8_next_char(p);
    }

    // Determine which character to underline in the display text
    if (mnemonic_display_pos >= 0) {
        // Has explicit mnemonic - underline that character
        display_underline_start = mnemonic_display_pos;
        // Find end of the mnemonic character
        const gchar* disp = display_text->str + mnemonic_display_pos;
        display_underline_end = mnemonic_display_pos + (g_utf8_next_char(disp) - disp);
    } else if (first_char_display_pos >= 0) {
        // No mnemonic - underline first letter
        display_underline_start = first_char_display_pos;
        const gchar* disp = display_text->str + first_char_display_pos;
        display_underline_end = first_char_display_pos + (g_utf8_next_char(disp) - disp);
    }

    if (display_underline_start < 0 || display_underline_end <= display_underline_start) {
        g_string_free(display_text, TRUE);
        return;
    }

    // Create Pango attribute list with underline
    PangoAttrList* attrs = pango_attr_list_new();
    PangoAttribute* underline_attr = pango_attr_underline_new(PANGO_UNDERLINE_SINGLE);
    underline_attr->start_index = display_underline_start;
    underline_attr->end_index = display_underline_end;
    pango_attr_list_insert(attrs, underline_attr);

    // Set the label text without mnemonic parsing and apply attributes
    gtk_label_set_text(label, display_text->str);
    gtk_label_set_attributes(label, attrs);

    // Disable mnemonic parsing since we're handling it manually
    gtk_label_set_use_underline(label, FALSE);

    pango_attr_list_unref(attrs);
    g_string_free(display_text, TRUE);
}

// Force mnemonics visible on a menu and all its items (recursive)
// Uses Pango attributes for permanent underlines that don't flash
static void ForceMenuMnemonicsVisible(GtkWidget* menu_widget) {
    if (!menu_widget || !GTK_IS_MENU(menu_widget))
        return;

    // Connect show/map signals (only once per menu)
    static GQuark show_handler_quark = g_quark_from_static_string("vbam-menu-show-handler");
    if (!g_object_get_qdata(G_OBJECT(menu_widget), show_handler_quark)) {
        g_signal_connect(menu_widget, "show", G_CALLBACK(OnMenuShow), NULL);
        g_signal_connect(menu_widget, "map", G_CALLBACK(OnMenuShow), NULL);
        g_object_set_qdata(G_OBJECT(menu_widget), show_handler_quark, GINT_TO_POINTER(1));
    }

    // Connect select signal on menu items with submenus (only once per item)
    static GQuark select_handler_quark = g_quark_from_static_string("vbam-menuitem-select-handler");
    // Track whether we've already applied permanent underlines to this menu
    static GQuark underline_done_quark = g_quark_from_static_string("vbam-menu-underline-done");

    GList* children = gtk_container_get_children(GTK_CONTAINER(menu_widget));

    // Only apply permanent underlines once per menu
    if (!g_object_get_qdata(G_OBJECT(menu_widget), underline_done_quark)) {
        g_object_set_qdata(G_OBJECT(menu_widget), underline_done_quark, GINT_TO_POINTER(1));

        // Apply permanent underlines to all menu item labels
        for (GList* l = children; l != NULL; l = l->next) {
            GtkWidget* item = GTK_WIDGET(l->data);
            if (!GTK_IS_MENU_ITEM(item))
                continue;

            GtkWidget* label = gtk_bin_get_child(GTK_BIN(item));
            if (label && GTK_IS_LABEL(label))
                ApplyPermanentUnderline(GTK_LABEL(label));
        }
    }

    // Recurse into submenus and connect select handlers
    for (GList* l = children; l != NULL; l = l->next) {
        GtkWidget* item = GTK_WIDGET(l->data);
        if (GTK_IS_MENU_ITEM(item)) {
            GtkWidget* submenu = gtk_menu_item_get_submenu(GTK_MENU_ITEM(item));
            if (submenu && GTK_IS_MENU(submenu)) {
                if (!g_object_get_qdata(G_OBJECT(item), select_handler_quark)) {
                    g_signal_connect(item, "select", G_CALLBACK(OnMenuItemSelect), NULL);
                    g_object_set_qdata(G_OBJECT(item), select_handler_quark, GINT_TO_POINTER(1));
                }
                ForceMenuMnemonicsVisible(submenu);
            }
        }
    }
    g_list_free(children);
}

// Disable take_focus and ensure mnemonics stay visible
static void DisableTakeFocusAndShowMnemonics(GtkMenuShell* menu_shell, GtkWidget* menubar_widget) {
    gtk_menu_shell_set_take_focus(menu_shell, FALSE);
    EnsureMnemonicsVisible(menubar_widget);
    ForceMenuMnemonicsVisible(GTK_WIDGET(menu_shell));
}

// Select a menu item without triggering GTK keyboard navigation mode
static void SelectMenuItemWithoutKeyboardMode(GtkMenuShell* menu_shell, GtkWidget* item, GtkMenuShell* menubar_shell) {
    GtkWidget* menubar_widget = GTK_WIDGET(menubar_shell);

    DisableTakeFocusAndShowMnemonics(menu_shell, menubar_widget);
    if (menubar_shell && menubar_shell != menu_shell)
        DisableTakeFocusAndShowMnemonics(menubar_shell, menubar_widget);

    if (GTK_IS_MENU_ITEM(item)) {
        GtkWidget* item_submenu = gtk_menu_item_get_submenu(GTK_MENU_ITEM(item));
        if (item_submenu && GTK_IS_MENU(item_submenu))
            DisableTakeFocusAndShowMnemonics(GTK_MENU_SHELL(item_submenu), menubar_widget);
    }

    gtk_menu_shell_select_item(menu_shell, item);
    ForceMenuMnemonicsVisible(GTK_WIDGET(menu_shell));
}

// Helper to find next/previous selectable menu item
static GtkWidget* FindSelectableMenuItem(GtkMenuShell* menu_shell, GtkWidget* current, bool forward) {
    GList* children = gtk_container_get_children(GTK_CONTAINER(menu_shell));
    gint count = g_list_length(children);
    gint current_idx = -1;

    // Find current item index
    for (gint i = 0; i < count; i++) {
        if (g_list_nth_data(children, i) == current) {
            current_idx = i;
            break;
        }
    }

    // Search for next/previous selectable item
    GtkWidget* result = nullptr;
    if (forward) {
        // Search forward, wrapping to start
        for (gint i = 1; i <= count; i++) {
            gint idx = (current_idx + i) % count;
            GtkWidget* item = GTK_WIDGET(g_list_nth_data(children, idx));
            if (item && GTK_IS_MENU_ITEM(item) && gtk_widget_get_visible(item) &&
                gtk_widget_get_sensitive(item) && !GTK_IS_SEPARATOR_MENU_ITEM(item)) {
                result = item;
                break;
            }
        }
    } else {
        // Search backward, wrapping to end
        for (gint i = 1; i <= count; i++) {
            gint idx = (current_idx - i + count) % count;
            GtkWidget* item = GTK_WIDGET(g_list_nth_data(children, idx));
            if (item && GTK_IS_MENU_ITEM(item) && gtk_widget_get_visible(item) &&
                gtk_widget_get_sensitive(item) && !GTK_IS_SEPARATOR_MENU_ITEM(item)) {
                result = item;
                break;
            }
        }
    }

    g_list_free(children);
    return result;
}

// Helper to find first selectable menu item
static GtkWidget* FindFirstSelectableMenuItem(GtkMenuShell* menu_shell) {
    GList* children = gtk_container_get_children(GTK_CONTAINER(menu_shell));
    GtkWidget* result = nullptr;

    for (GList* l = children; l != NULL; l = l->next) {
        GtkWidget* item = GTK_WIDGET(l->data);
        if (item && GTK_IS_MENU_ITEM(item) && gtk_widget_get_visible(item) &&
            gtk_widget_get_sensitive(item) && !GTK_IS_SEPARATOR_MENU_ITEM(item)) {
            result = item;
            break;
        }
    }

    g_list_free(children);
    return result;
}

// Helper to check if item is the first selectable in menu
static bool IsFirstSelectableMenuItem(GtkMenuShell* menu_shell, GtkWidget* item) {
    return FindFirstSelectableMenuItem(menu_shell) == item;
}

// Signal handler to intercept and handle keyboard navigation in menus
static gboolean HandleMenuKeyboardNavigation(GtkWidget* widget, GdkEventKey* event, gpointer user_data) {
    (void)widget;
    guint keyval = event->keyval;

    MainFrame* frame = (MainFrame*)user_data;
    if (!frame)
        return FALSE;

    wxMenuBar* menubar = frame->GetMenuBar();
    if (!menubar)
        return FALSE;

    GtkWidget* menubar_widget = menubar->GetHandle();
    if (!menubar_widget)
        return FALSE;

    EnsureMnemonicsVisible(menubar_widget);

    // Handle Escape to close menu
    if (keyval == GDK_KEY_Escape) {
        GtkMenuShell* menu_shell = GTK_MENU_SHELL(menubar_widget);
        gtk_grab_remove(menubar_widget);
        gtk_menu_shell_deselect(menu_shell);
        gtk_menu_shell_cancel(menu_shell);
        frame->SetMenusOpened(false);
        return TRUE;
    }

    GtkMenuShell* menubar_shell = GTK_MENU_SHELL(menubar_widget);
    GtkWidget* selected_menubar_item = gtk_menu_shell_get_selected_item(menubar_shell);
    GtkWidget* active_submenu = nullptr;
    GtkWidget* selected_submenu_item = nullptr;
    GtkWidget* active_sub_submenu = nullptr;
    GtkWidget* selected_sub_submenu_item = nullptr;

    DisableTakeFocusAndShowMnemonics(menubar_shell, menubar_widget);

    if (selected_menubar_item && GTK_IS_MENU_ITEM(selected_menubar_item)) {
        active_submenu = gtk_menu_item_get_submenu(GTK_MENU_ITEM(selected_menubar_item));
        if (active_submenu && GTK_IS_MENU(active_submenu)) {
            DisableTakeFocusAndShowMnemonics(GTK_MENU_SHELL(active_submenu), menubar_widget);
            selected_submenu_item = gtk_menu_shell_get_selected_item(GTK_MENU_SHELL(active_submenu));

            if (selected_submenu_item && GTK_IS_MENU_ITEM(selected_submenu_item)) {
                active_sub_submenu = gtk_menu_item_get_submenu(GTK_MENU_ITEM(selected_submenu_item));
                if (active_sub_submenu && GTK_IS_MENU(active_sub_submenu)) {
                    DisableTakeFocusAndShowMnemonics(GTK_MENU_SHELL(active_sub_submenu), menubar_widget);
                    selected_sub_submenu_item = gtk_menu_shell_get_selected_item(GTK_MENU_SHELL(active_sub_submenu));
                }
            }
        }
    }

    // Helper lambda to select item and handle submenu deselection
    auto selectMenuItem = [&](GtkMenuShell* shell, GtkWidget* item) {
        SelectMenuItemWithoutKeyboardMode(shell, item, menubar_shell);
        GtkWidget* item_submenu = gtk_menu_item_get_submenu(GTK_MENU_ITEM(item));
        if (item_submenu) {
            DisableTakeFocusAndShowMnemonics(GTK_MENU_SHELL(item_submenu), menubar_widget);
            gtk_menu_shell_deselect(GTK_MENU_SHELL(item_submenu));
        }
    };

    // Determine target menu for vertical navigation
    GtkWidget* target_menu = nullptr;
    GtkWidget* target_item = nullptr;
    if (active_sub_submenu && selected_sub_submenu_item) {
        target_menu = active_sub_submenu;
        target_item = selected_sub_submenu_item;
    } else if (selected_submenu_item) {
        target_menu = active_submenu;
        target_item = selected_submenu_item;
    } else if (active_submenu) {
        target_menu = active_submenu;
        target_item = nullptr;
    }

    // Handle DOWN arrow
    if (keyval == GDK_KEY_Down && target_menu) {
        GtkMenuShell* shell = GTK_MENU_SHELL(target_menu);
        GtkWidget* next = target_item ? FindSelectableMenuItem(shell, target_item, true)
                                      : FindFirstSelectableMenuItem(shell);
        if (next)
            selectMenuItem(shell, next);

        if (active_submenu) ForceMenuMnemonicsVisible(active_submenu);
        if (active_sub_submenu) ForceMenuMnemonicsVisible(active_sub_submenu);
        return TRUE;
    }

    // Handle UP arrow
    if (keyval == GDK_KEY_Up && target_menu && target_item) {
        GtkMenuShell* shell = GTK_MENU_SHELL(target_menu);
        if (IsFirstSelectableMenuItem(shell, target_item)) {
            // On first item, deselect to return focus to parent
            gtk_menu_shell_deselect(shell);
            DisableTakeFocusAndShowMnemonics(shell, menubar_widget);
        } else {
            GtkWidget* prev = FindSelectableMenuItem(shell, target_item, false);
            if (prev)
                selectMenuItem(shell, prev);
        }

        if (active_submenu) ForceMenuMnemonicsVisible(active_submenu);
        if (active_sub_submenu) ForceMenuMnemonicsVisible(active_sub_submenu);
        return TRUE;
    }

    // Handle RIGHT arrow - enter submenu or move to next top-level menu
    if (keyval == GDK_KEY_Right) {
        // Try to enter submenu of selected item
        if (selected_submenu_item && GTK_IS_MENU_ITEM(selected_submenu_item)) {
            GtkWidget* sub_submenu = gtk_menu_item_get_submenu(GTK_MENU_ITEM(selected_submenu_item));
            if (sub_submenu && GTK_IS_MENU(sub_submenu)) {
                GtkMenuShell* sub_shell = GTK_MENU_SHELL(sub_submenu);
                DisableTakeFocusAndShowMnemonics(menubar_shell, menubar_widget);
                DisableTakeFocusAndShowMnemonics(GTK_MENU_SHELL(active_submenu), menubar_widget);
                DisableTakeFocusAndShowMnemonics(sub_shell, menubar_widget);
                gtk_widget_set_can_focus(sub_submenu, FALSE);

                g_signal_connect(sub_submenu, "key-press-event",
                                G_CALLBACK(HandleMenuKeyboardNavigation), frame);

                SelectMenuItemWithoutKeyboardMode(GTK_MENU_SHELL(active_submenu), selected_submenu_item, menubar_shell);

                GtkWidget* first = FindFirstSelectableMenuItem(sub_shell);
                if (first) {
                    gtk_menu_shell_select_item(sub_shell, first);
                    GtkWidget* first_sub = gtk_menu_item_get_submenu(GTK_MENU_ITEM(first));
                    if (first_sub)
                        DisableTakeFocusAndShowMnemonics(GTK_MENU_SHELL(first_sub), menubar_widget);
                }

                ForceMenuMnemonicsVisible(sub_submenu);
                if (active_submenu) {
                    ForceMenuMnemonicsVisible(active_submenu);
                    ScheduleMnemonicTimeouts(active_submenu);
                }
                ScheduleMnemonicTimeouts(sub_submenu);
                return TRUE;
            }
        }

        // Move to next top-level menu (wrapping)
        // First close/deselect the current submenu and menubar item
        if (active_submenu)
            gtk_menu_shell_deselect(GTK_MENU_SHELL(active_submenu));
        gtk_menu_shell_deselect(menubar_shell);
        if (selected_menubar_item) {
            GtkWidget* next = FindSelectableMenuItem(menubar_shell, selected_menubar_item, true);
            if (next)
                SelectMenuItemWithoutKeyboardMode(menubar_shell, next, menubar_shell);
        }
        return TRUE;
    }

    // Handle LEFT arrow - close submenu or move to previous top-level menu
    if (keyval == GDK_KEY_Left) {
        // If in a sub-submenu, go back to parent submenu
        if (active_sub_submenu && selected_sub_submenu_item) {
            gtk_menu_shell_deselect(GTK_MENU_SHELL(active_sub_submenu));
            if (active_submenu)
                ForceMenuMnemonicsVisible(active_submenu);
            return TRUE;
        }

        // If a submenu item is selected (or dropdown is open), move to previous top-level menu
        // First close/deselect the current submenu and menubar item
        if (active_submenu)
            gtk_menu_shell_deselect(GTK_MENU_SHELL(active_submenu));
        gtk_menu_shell_deselect(menubar_shell);
        if (selected_menubar_item) {
            GtkWidget* prev = FindSelectableMenuItem(menubar_shell, selected_menubar_item, false);
            if (prev)
                SelectMenuItemWithoutKeyboardMode(menubar_shell, prev, menubar_shell);
        }
        return TRUE;
    }

    // Handle ENTER
    if (keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter) {
        GtkWidget* item_to_activate = selected_sub_submenu_item ? selected_sub_submenu_item : selected_submenu_item;
        if (item_to_activate && GTK_IS_MENU_ITEM(item_to_activate)) {
            gtk_menu_item_activate(GTK_MENU_ITEM(item_to_activate));
            frame->SetMenusOpened(false);
            // Ensure globals are set for timeout callbacks
            GtkWidget* toplevel = gtk_widget_get_toplevel(menubar_widget);
            if (toplevel && GTK_IS_WINDOW(toplevel))
                g_main_window = toplevel;
            g_menubar_widget = menubar_widget;
            // Keep menubar mnemonics visible after menu closes
            ScheduleMnemonicTimeouts(nullptr);
        }
        return TRUE;
    }

    // Handle letter/number keys for mnemonics - only in the currently active menu
    gunichar unicode = gdk_keyval_to_unicode(keyval);
    if (g_unichar_isalnum(unicode)) {
        wxChar letter = wxToupper((wxChar)unicode);

        // Find the currently active wxMenu based on GTK selection state
        wxMenu* active_wx_menu = nullptr;

        // Find which top-level menu is selected
        if (selected_menubar_item) {
            GList* menubar_children = gtk_container_get_children(GTK_CONTAINER(menubar_shell));
            gint menubar_idx = 0;
            for (GList* l = menubar_children; l != NULL; l = l->next, menubar_idx++) {
                if (GTK_WIDGET(l->data) == selected_menubar_item) {
                    if ((size_t)menubar_idx < menubar->GetMenuCount())
                        active_wx_menu = menubar->GetMenu(menubar_idx);
                    break;
                }
            }
            g_list_free(menubar_children);
        }

        // Only switch to submenu if an item within it is actually selected
        // (not just when the submenu is open but focus is still on parent menu item)
        if (active_sub_submenu && selected_sub_submenu_item && active_wx_menu) {
            // active_wx_menu is currently the top-level menu (e.g., File)
            // active_sub_submenu is the submenu with a selected item
            // Find which menu item's submenu matches active_sub_submenu
            wxMenuItemList& items = active_wx_menu->GetMenuItems();
            for (wxMenuItemList::iterator it = items.begin(); it != items.end(); ++it) {
                wxMenuItem* item = *it;
                if (item && item->IsSubMenu()) {
                    wxMenu* submenu = item->GetSubMenu();
                    if (submenu && submenu->m_menu == active_sub_submenu) {
                        active_wx_menu = submenu;
                        break;
                    }
                }
            }
        }
        // If submenu is open but no item selected within it, stay with the parent menu

        if (active_wx_menu && ActivateMenuItemByMnemonic(active_wx_menu, letter)) {
            frame->SetMenusOpened(false);
            gtk_grab_remove(menubar_widget);
            gtk_menu_shell_deselect(menubar_shell);
            gtk_menu_shell_cancel(menubar_shell);
            // Ensure globals are set and force mnemonics visible after menu closes
            g_menubar_widget = menubar_widget;
            EnsureMnemonicsVisible(menubar_widget);
            ForceMenubarMnemonicsVisible(menubar_widget);
            ScheduleMnemonicTimeouts(nullptr);
            return TRUE;
        }
        return TRUE;
    }

    // Let other keys through
    return FALSE;
}

#endif  // __WXGTK__

#ifdef __WXGTK__
//==============================================================================
// Menu Item Activation Helpers (GTK only)
//==============================================================================

// Helper to activate a menu item (handles checkable items)
static bool ActivateMenuItem(wxMenu* menu, wxMenuItem* item) {
    if (!item)
        return false;

    // For checkable items, toggle the state first
    if (item->IsCheckable())
        item->Check(!item->IsChecked());

    wxCommandEvent event(wxEVT_MENU, item->GetId());
    event.SetEventObject(menu);
    // For checkable items, set the int value to the new checked state
    if (item->IsCheckable())
        event.SetInt(item->IsChecked() ? 1 : 0);
    wxTheApp->GetTopWindow()->GetEventHandler()->ProcessEvent(event);
    return true;
}
// Search within a single menu (non-recursive) for a menu item with the given mnemonic/first letter and activate it
// Priority: 1) mnemonic match (character after &), 2) first letter match
static bool ActivateMenuItemByMnemonic(wxMenu* menu, wxChar letter) {
    if (!menu)
        return false;

    wxMenuItem* first_letter_match = nullptr;

    wxMenuItemList& items = menu->GetMenuItems();
    for (wxMenuItemList::iterator it = items.begin(); it != items.end(); ++it) {
        wxMenuItem* item = *it;
        if (!item || item->IsSeparator() || item->IsSubMenu())
            continue;

        wxString full_label = item->GetItemLabel();
        int mnemonic_pos = full_label.Find('&');

        // Check for mnemonic match (highest priority)
        if (mnemonic_pos != wxNOT_FOUND && mnemonic_pos + 1 < (int)full_label.length()) {
            wxChar item_mnemonic = wxToupper(full_label[mnemonic_pos + 1]);
            if (item_mnemonic == letter)
                return ActivateMenuItem(menu, item);
        }

        // Track first letter match as fallback (only remember the first one found)
        if (!first_letter_match && !item->GetItemLabelText().IsEmpty()) {
            wxChar first_char = wxToupper(item->GetItemLabelText()[0]);
            if (first_char == letter)
                first_letter_match = item;
        }
    }

    // No mnemonic match found, try first letter match
    if (first_letter_match)
        return ActivateMenuItem(menu, first_letter_match);

    return false;
}
#else  // !__WXGTK__
// Recursively search a menu for a menu item with the given mnemonic and activate it
static bool SearchAndActivateMenuItem(wxMenu* menu, wxChar mnemonic, int depth) {
    if (!menu)
        return false;

    wxMenuItemList& items = menu->GetMenuItems();
    for (wxMenuItemList::iterator it = items.begin(); it != items.end(); ++it) {
        wxMenuItem* item = *it;
        if (!item || item->IsSeparator())
            continue;

        if (item->IsSubMenu()) {
            if (SearchAndActivateMenuItem(item->GetSubMenu(), mnemonic, depth + 1))
                return true;
        } else {
            wxChar item_mnemonic = 0;
            wxString full_label = item->GetItemLabel();
            int mnemonic_pos = full_label.Find('&');
            if (mnemonic_pos != wxNOT_FOUND && mnemonic_pos + 1 < (int)full_label.length())
                item_mnemonic = wxToupper(full_label[mnemonic_pos + 1]);
            else if (!item->GetItemLabelText().IsEmpty())
                item_mnemonic = wxToupper(item->GetItemLabelText()[0]);

            if (item_mnemonic && item_mnemonic == mnemonic) {
                wxCommandEvent event(wxEVT_MENU, item->GetId());
                event.SetEventObject(menu);
                wxTheApp->GetTopWindow()->GetEventHandler()->ProcessEvent(event);
                return true;
            }
        }
    }
    return false;
}
#endif

int wxvbamApp::FilterEvent(wxEvent& event)
{
    if (!frame) {
        // Ignore early events.
        return wxEventFilter::Event_Skip;
    }

    if (event.GetEventType() == wxEVT_KEY_DOWN || event.GetEventType() == wxEVT_KEY_UP) {
        // Skip keyboard processing when focus is on a text entry control
        // to allow typing special characters like %
        // Exception: UserInputCtrl needs keyboard processing to capture key bindings
        wxWindow* focused = wxWindow::FindFocus();
        if (focused && !wxDynamicCast(focused, widgets::UserInputCtrl) &&
            (wxDynamicCast(focused, wxTextCtrl) || wxDynamicCast(focused, wxComboBox))) {
            return wxEventFilter::Event_Skip;
        }
#ifdef __WXGTK__
        // Keep menubar mnemonics visible when Alt is released
        // GTK hides them by default on Alt release
        if (event.GetEventType() == wxEVT_KEY_UP) {
            wxKeyEvent& key_event = static_cast<wxKeyEvent&>(event);
            int key_code = key_event.GetKeyCode();
            if (key_code == WXK_ALT || key_code == WXK_RAW_CONTROL) {
                wxMenuBar* menubar = frame->GetMenuBar();
                if (menubar) {
                    GtkWidget* menubar_widget = menubar->GetHandle();
                    if (menubar_widget)
                        EnsureMnemonicsVisible(menubar_widget);
                }
            }
        }
#endif
        // If menus are open, handle menu item mnemonics recursively
        // Exception: UserInputCtrl needs keyboard processing to capture key bindings
        if (!frame->CanProcessShortcuts() && !wxDynamicCast(focused, widgets::UserInputCtrl)) {
            if (event.GetEventType() == wxEVT_KEY_DOWN) {
                wxKeyEvent& key_event = static_cast<wxKeyEvent&>(event);
                int key_code = key_event.GetKeyCode();

                // Check for Esc to close menu
                if (key_code == WXK_ESCAPE) {
#ifdef __WXGTK__
                    // On GTK, remove the grab and close the menu
                    wxMenuBar* menubar = frame->GetMenuBar();
                    if (menubar) {
                        GtkWidget* menubar_widget = menubar->GetHandle();
                        if (menubar_widget) {
                            gtk_grab_remove(menubar_widget);
                            GtkMenuShell* menu_shell = GTK_MENU_SHELL(menubar_widget);
                            gtk_menu_shell_deselect(menu_shell);
                            gtk_menu_shell_cancel(menu_shell);
                        }
                    }
#endif
                    // Mark menus as closed
                    frame->SetMenusOpened(false);
                    return wxEventFilter::Event_Processed;
                }

                // Arrow keys and Enter are handled by HandleMenuKeyboardNavigation on GTK
                // Just skip them here to let the GTK handler process them
                if (key_code == WXK_UP || key_code == WXK_DOWN || key_code == WXK_LEFT ||
                    key_code == WXK_RIGHT || key_code == WXK_RETURN) {
                    return wxEventFilter::Event_Skip;
                }

                // Letter/number keys are handled by HandleMenuKeyboardNavigation on GTK
#ifdef __WXGTK__
                // Let the GTK handler process letter keys for menu mnemonics
                return wxEventFilter::Event_Skip;
#else
                // Non-GTK fallback: search all menus for matching mnemonic
                if ((key_code >= 'A' && key_code <= 'Z') || (key_code >= 'a' && key_code <= 'z') ||
                    (key_code >= '0' && key_code <= '9')) {
                    wxChar letter = wxToupper(static_cast<wxChar>(key_code));
                    wxMenuBar* menubar = frame->GetMenuBar();
                    if (menubar) {
                        for (size_t i = 0; i < menubar->GetMenuCount(); i++) {
                            wxMenu* menu = menubar->GetMenu(i);
                            if (menu && SearchAndActivateMenuItem(menu, letter))
                                return wxEventFilter::Event_Processed;
                        }
                    }
                }
#endif
            }
            return wxEventFilter::Event_Skip;
        }

#ifndef __WXMAC__
        // On non-macOS platforms, check if this is an Alt+letter menu mnemonic BEFORE calling ProcessKeyEvent.
        if (event.GetEventType() == wxEVT_KEY_DOWN) {
            wxKeyEvent& key_event = static_cast<wxKeyEvent&>(event);
            if (key_event.AltDown() && !key_event.ControlDown() && !key_event.ShiftDown()) {
                int key_code = key_event.GetKeyCode();
                if ((key_code >= 'A' && key_code <= 'Z') || (key_code >= 'a' && key_code <= 'z')) {
                    wxMenuBar* menubar = frame->GetMenuBar();
                    if (menubar && frame->CanProcessShortcuts()) {
                        wxChar letter = wxToupper(static_cast<wxChar>(key_code));
                        for (size_t i = 0; i < menubar->GetMenuCount(); i++) {
                            wxString label = menubar->GetMenuLabel(i);
                            int mnemonic_pos = label.Find('&');
                            if (mnemonic_pos != wxNOT_FOUND && mnemonic_pos + 1 < (int)label.length()) {
                                wxChar mnemonic = wxToupper(label[mnemonic_pos + 1]);
                                if (mnemonic == letter) {
#ifdef __WXGTK__
                                    GtkWidget* menubar_widget = menubar->GetHandle();
                                    if (menubar_widget && GTK_IS_MENU_BAR(menubar_widget)) {
                                        GtkMenuShell* menu_shell = GTK_MENU_SHELL(menubar_widget);

                                        // Force mnemonic underlines to be visible
                                        EnsureMnemonicsVisible(menubar_widget);

                                        // Install keyboard navigation handler and disable GTK's default
                                        g_signal_connect(menubar_widget, "key-press-event",
                                                        G_CALLBACK(HandleMenuKeyboardNavigation), frame);
                                        DisableTakeFocusAndShowMnemonics(menu_shell, menubar_widget);

                                        // Setup submenus
                                        GList* menubar_children = gtk_container_get_children(GTK_CONTAINER(menu_shell));
                                        for (GList* l = menubar_children; l != NULL; l = l->next) {
                                            GtkWidget* menu_item_widget = GTK_WIDGET(l->data);
                                            if (GTK_IS_MENU_ITEM(menu_item_widget)) {
                                                GtkWidget* submenu = gtk_menu_item_get_submenu(GTK_MENU_ITEM(menu_item_widget));
                                                if (submenu && GTK_IS_MENU(submenu)) {
                                                    DisableTakeFocusAndShowMnemonics(GTK_MENU_SHELL(submenu), menubar_widget);
                                                    g_signal_connect(submenu, "key-press-event",
                                                                    G_CALLBACK(HandleMenuKeyboardNavigation), frame);

                                                    // Setup sub-submenus and enable underlines
                                                    GList* menu_children = gtk_container_get_children(GTK_CONTAINER(submenu));
                                                    for (GList* m = menu_children; m != NULL; m = m->next) {
                                                        GtkWidget* item = GTK_WIDGET(m->data);
                                                        if (GTK_IS_MENU_ITEM(item)) {
                                                            GtkWidget* sub_submenu = gtk_menu_item_get_submenu(GTK_MENU_ITEM(item));
                                                            if (sub_submenu && GTK_IS_MENU(sub_submenu))
                                                                DisableTakeFocusAndShowMnemonics(GTK_MENU_SHELL(sub_submenu), menubar_widget);

                                                            GtkWidget* item_label = gtk_bin_get_child(GTK_BIN(item));
                                                            if (item_label && GTK_IS_LABEL(item_label)) {
                                                                gtk_label_set_use_underline(GTK_LABEL(item_label), TRUE);
                                                                gtk_label_set_mnemonic_widget(GTK_LABEL(item_label), item);
                                                            }
                                                        }
                                                    }
                                                    g_list_free(menu_children);
                                                }
                                            }
                                        }
                                        g_list_free(menubar_children);

                                        // Select the target menu
                                        GList* children = gtk_container_get_children(GTK_CONTAINER(menu_shell));
                                        if (children && g_list_length(children) > i) {
                                            GtkWidget* menu_item = GTK_WIDGET(g_list_nth_data(children, i));
                                            if (menu_item && GTK_IS_MENU_ITEM(menu_item)) {
                                                gtk_widget_set_can_focus(menubar_widget, TRUE);
                                                gtk_widget_grab_focus(menubar_widget);
                                                gtk_grab_add(menubar_widget);
                                                gtk_menu_shell_select_first(menu_shell, TRUE);
                                                SelectMenuItemWithoutKeyboardMode(menu_shell, menu_item, menu_shell);
                                            }
                                        }
                                        g_list_free(children);
                                    }
                                    return wxEventFilter::Event_Skip;
#elif defined(__WXMSW__)
                                    HWND hwnd = (HWND)frame->GetHandle();
                                    ::PostMessage(hwnd, WM_SYSCOMMAND, SC_KEYMENU, (LPARAM)letter);
                                    return wxEventFilter::Event_Processed;
#endif
                                }
                            }
                        }
                    }
                }
            }
        }
#endif
        // Handle keyboard input events here to generate user input events.
        keyboard_input_handler_.ProcessKeyEvent(static_cast<wxKeyEvent&>(event));
        return wxEventFilter::Event_Skip;
    }

    if (event.GetEventType() != VBAM_EVT_USER_INPUT) {
        // We only treat "VBAM_EVT_USER_INPUT" events here.
        return wxEventFilter::Event_Skip;
    }

    if (!frame->CanProcessShortcuts()) {
        return wxEventFilter::Event_Skip;
    }

    widgets::UserInputEvent& user_input_event = static_cast<widgets::UserInputEvent&>(event);
    int command_id = wxID_NONE;
    nonstd::optional<config::UserInput> user_input;

    for (const auto& event_data : user_input_event.data()) {
        if (!event_data.pressed) {
            // We only treat key press events here.
            continue;
        }

        const nonstd::optional<config::Command> command =
            bindings_.CommandForInput(event_data.input);
        if (command != nonstd::nullopt && command->is_shortcut()) {
            // Associated shortcut command found.
            command_id = command->shortcut().id();
            user_input.emplace(event_data.input);
            break;
        }
    }

    if (command_id == wxID_NONE) {
        // No associated command found.
        return wxEventFilter::Event_Skip;
    }

    // Find the associated checkable menu item (if any).
    for (const cmditem& cmd_item : cmdtab) {
        if (cmd_item.cmd_id == command_id) {
            if (cmd_item.mi && cmd_item.mi->IsCheckable()) {
                // Toggle the checkable menu item.
                cmd_item.mi->Check(!cmd_item.mi->IsChecked());
            }
            break;
        }
    }

    // Queue the associated shortcut command event.
    wxCommandEvent* command_event = new wxCommandEvent(wxEVT_MENU, command_id);
    command_event->SetEventObject(this);
    frame->GetEventHandler()->QueueEvent(command_event);

    return user_input_event.FilterProcessedInput(user_input.value());
}

#ifndef VBAM_WX_MAC_PATCHED_FOR_ALERT_SOUND
bool wxvbamApp::ProcessEvent(wxEvent& event) {
    if (event.GetEventType() == wxEVT_KEY_DOWN) {
        // First, figure out if the focused window can process the key down event.
        wxWindow* focused_window = wxWindow::FindFocus();
        wxTextCtrl* text_ctrl = wxDynamicCast(focused_window, wxTextCtrl);
        if (text_ctrl) {
            return wxApp::ProcessEvent(event);
        }
        wxSpinCtrl* spin_ctrl = wxDynamicCast(focused_window, wxSpinCtrl);
        if (spin_ctrl) {
            return wxApp::ProcessEvent(event);
        }

        // Mark the event as processed. This prevents wxWidgets from firing alerts on macOS.
        // See https://github.com/wxWidgets/wxWidgets/issues/25262 for details.
        return true;
    }
    return wxApp::ProcessEvent(event);
}
#endif
