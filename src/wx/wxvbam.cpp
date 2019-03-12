// mainline:
//   parse cmd line
//   load xrc file (guiinit.cpp does most of instantiation)
//   create & display main frame

#include "wxvbam.h"
#include <stdio.h>
#include <wx/cmdline.h>
#include <wx/file.h>
#include <wx/filesys.h>
#include <wx/fs_arc.h>
#include <wx/fs_mem.h>
#include <wx/mstream.h>
#include <wx/progdlg.h>
#include <wx/protocol/http.h>
#include <wx/regex.h>
#include <wx/sstream.h>
#include <wx/txtstrm.h>
#include <wx/url.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>
#include "wayland.h"
#include "strutils.h"

// The built-in xrc file
#include "builtin-xrc.h"

// The built-in vba-over.ini
#include "../common/ConfigManager.h"
#include "builtin-over.h"

IMPLEMENT_APP(wxvbamApp)
IMPLEMENT_DYNAMIC_CLASS(MainFrame, wxFrame)

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
#define add_nonstandard_path(p)                                                                                          \
    do {                                                                                                     \
        const wxString& s = p;                                                                          \
        wxFileName parent = wxFileName::DirName(s + wxT("//.."));                                            \
        parent.MakeAbsolute();                                                                               \
        if ((wxDirExists(s) && wxIsWritable(s)) || ((!exists || !wxDirExists(s)) && parent.IsDirWritable())) \
            path.Add(s);                                                                                     \
    } while (0)

    static bool debug_dumped = false;

    if (!debug_dumped) {
        wxLogDebug(wxT("GetUserLocalDataDir(): %s"), stdp.GetUserLocalDataDir().mb_str());
        wxLogDebug(wxT("GetUserDataDir(): %s"), stdp.GetUserDataDir().mb_str());
        wxLogDebug(wxT("GetLocalizedResourcesDir(wxGetApp().locale.GetCanonicalName()): %s"), stdp.GetLocalizedResourcesDir(wxGetApp().locale.GetCanonicalName()).mb_str());
        wxLogDebug(wxT("GetResourcesDir(): %s"), stdp.GetResourcesDir().mb_str());
        wxLogDebug(wxT("GetDataDir(): %s"), stdp.GetDataDir().mb_str());
        wxLogDebug(wxT("GetLocalDataDir(): %s"), stdp.GetLocalDataDir().mb_str());
        wxLogDebug(wxT("plugins_dir: %s"), wxGetApp().GetPluginsDir().mb_str());
        wxLogDebug(wxT("XdgConfigDir: %s"), get_xdg_user_config_home() + current_app_name);
        debug_dumped = true;
    }

// When native support for XDG dirs is available (wxWidgets >= 3.1),
// this will be no longer necessary
#if defined(__WXGTK__)
    // XDG spec manual support
    // ${XDG_CONFIG_HOME:-$HOME/.config}/`appname`
    wxString old_config = wxString(getenv("HOME")) + "/.vbam";
    wxString new_config(get_xdg_user_config_home());
    if (!wxDirExists(old_config) && wxIsWritable(new_config))
    {
        wxFileName new_path(new_config, wxEmptyString);
        new_path.AppendDir(current_app_name);
        new_path.MakeAbsolute();

        add_nonstandard_path(new_path.GetFullPath());
    }
    else
    {
	// config is in $HOME/.vbam/vbam.conf
	add_nonstandard_path(old_config);
    }
#endif

    // NOTE: this does not support XDG (freedesktop.org) paths
    add_path(GetUserLocalDataDir());
    add_path(GetUserDataDir());
    add_path(GetLocalizedResourcesDir(wxGetApp().locale.GetCanonicalName()));
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

    for (int i = 0; i < full_config_path.size(); i++)
        s += wxT("\n\t") + full_config_path[i] + app;
}

const wxString wxvbamApp::GetPluginsDir()
{
    return wxStandardPaths::Get().GetPluginsDir();
}

wxString wxvbamApp::GetConfigurationPath()
{
#if defined(__WXMSW__) || defined(__APPLE__)
    wxString config("vbam.ini");
#else
    wxString config("vbam.conf");
#endif
    // first check if config files exists in reverse order
    // (from system paths to more local paths.)
    if (data_path.empty()) {
        get_config_path(config_path);

        for (int i = config_path.size() - 1; i >= 0; i--) {
            wxFileName fn(config_path[i], config);

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
        for (int i = 0; i < config_path.size(); i++) {
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
        fn.Normalize();
        return fn.GetFullPath();
    }

    return path;
}

bool wxvbamApp::OnInit()
{
    // set up logging
#ifndef NDEBUG
    wxLog::SetLogLevel(wxLOG_Trace);
#endif
    // turn off output buffering on Windows to support mintty
#ifdef __WXMSW__
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    dup2(1, 2); // redirect stderr to stdout
#endif
    using_wayland = IsItWayland();

    // use consistent names for config
    SetAppName(_("visualboyadvance-m"));
#if (wxMAJOR_VERSION >= 3)
    SetAppDisplayName(_T("VisualBoyAdvance-M"));
#endif
    // load system default locale, if available
    locale.Init();
    locale.AddCatalog(_T("wxvbam"));
    // make built-in xrc file available
    // this has to be done before parent OnInit() so xrc dump works
    wxFileSystem::AddHandler(new wxMemoryFSHandler);
    wxFileSystem::AddHandler(new wxArchiveFSHandler);
    wxMemoryFSHandler::AddFileWithMimeType(wxT("wxvbam.xrs"), builtin_xrs, sizeof(builtin_xrs), wxT("application/zip"));

    if (!wxApp::OnInit())
        return false;

    // prepare for loading xrc files
    wxXmlResource* xr = wxXmlResource::Get();
    // note: if linking statically, next 2 pull in lot of unused code
    // maybe in future if not wxSHARED, load only builtin-needed handlers
    xr->InitAllHandlers();
    wxInitAllImageHandlers();
    get_config_path(config_path);
    // first, load override xrcs
    // this can only override entire root nodes
    // 2.9 has LoadAllFiles(), but this is 2.8, so we'll do it manually
    wxString cwd = wxGetCwd();

    for (int i = 0; i < config_path.size(); i++)
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
// set up config file
// this needs to be in a subdir to support other config as well
// but subdir flag behaves differently 2.8 vs. 2.9.  Oh well.
// NOTE: this does not support XDG (freedesktop.org) paths
    wxString confname("vbam.ini");
    wxFileName vbamconf(GetConfigurationPath(), confname);
// /MIGRATION
// migrate from 'vbam.conf' to 'vbam.ini' to manage a single config
// file for all platforms.
#if !defined(__WXMSW__) && !defined(__APPLE__)
    wxString oldConf(GetConfigurationPath() + "/vbam.conf");
    wxString newConf(GetConfigurationPath() + "/vbam.ini");
    if (wxFileExists(oldConf))
    {
	wxRenameFile(oldConf, newConf, false);
    }
#endif
// /END_MIGRATION
    cfg = new wxFileConfig(wxT("vbam"), wxEmptyString,
        vbamconf.GetFullPath(),
        wxEmptyString, wxCONFIG_USE_LOCAL_FILE);
    // set global config for e.g. Windows font mapping
    wxFileConfig::Set(cfg);
    // yet another bug/deficiency in wxConfig: dirs are not created if needed
    // since a default config is always written, dirs are always needed
    // Can't figure out statically if using wxFileConfig w/o duplicating wx's
    // logic, so do it at run-time
    // wxFileConfig *f = wxDynamicCast(cfg, wxFileConfig);
    // wxConfigBase does not derive from wxObject!!! so no wxDynamicCast
    wxFileConfig* fc = dynamic_cast<wxFileConfig*>(cfg);

    if (fc) {
        wxFileName s(wxFileConfig::GetLocalFileName(GetAppName()));
// at least up to 2.8.12, GetLocalFileName returns the dir if
// SUBDIR is specified instead of actual file name
// and SUBDIR only affects UNIX
#if defined(__UNIX__) && !wxCHECK_VERSION(2, 9, 0)
        s.AppendDir(s.GetFullName());
#endif
        // only the path part gets created
        // note that 0777 is default (assumes umask will do og-w)
        s.Mkdir(0777, wxPATH_MKDIR_FULL);
        s = wxFileName::DirName(GetConfigurationPath());
        s.Mkdir(0777, wxPATH_MKDIR_FULL);
    }

    load_opts();

    // wxGLCanvas segfaults under wayland
    if (UsingWayland() && gopts.render_method == RND_OPENGL) {
        gopts.render_method = RND_SIMPLE;
    }

    // process command-line options
    for (int i = 0; i < pending_optset.size(); i++) {
        auto parts = str_split(pending_optset[i], wxT('='));
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

    // load vba-over.ini
    // rather than dealing with wxConfig's broken search path, just use
    // the same one that the xrc overrides use
    // this also allows us to override a group at a time, add commments, and
    // add the file from which the group came
    wxMemoryInputStream mis(builtin_over, sizeof(builtin_over));
    overrides = new wxFileConfig(mis);
    wxRegEx cmtre;
    // not the most efficient thing to do: read entire file into a string
    // just to parse the comments out
    wxString bovs((const char*)builtin_over, wxConvUTF8, sizeof(builtin_over));
    bool cont;
    wxString s;
    long grp_idx;
#define CMT_RE_START wxT("(^|[\n\r])# ?([^\n\r]*)(\r?\n|\r)\\[")

    for (cont = overrides->GetFirstGroup(s, grp_idx); cont;
         cont = overrides->GetNextGroup(s, grp_idx)) {
        // apparently even MacOSX sometimes uses the old \r by itself
        wxString cmt(CMT_RE_START);
        cmt += s + wxT("\\]");

        if (cmtre.Compile(cmt) && cmtre.Matches(bovs))
            cmt = cmtre.GetMatch(bovs, 2);
        else
            cmt = wxEmptyString;

        overrides->Write(s + wxT("/comment"), cmt);
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
            overrides->DeleteGroup(s);
            overrides->SetPath(s);
            ov.SetPath(s);
            overrides->Write(wxT("path"), GetConfigurationPath());
            // apparently even MacOSX sometimes uses \r by itself
            wxString cmt(CMT_RE_START);
            cmt += s + wxT("\\]");

            if (cmtre.Compile(cmt) && cmtre.Matches(sos.GetString()))
                cmt = cmtre.GetMatch(sos.GetString(), 2);
            else
                cmt = wxEmptyString;

            overrides->Write(wxT("comment"), cmt);
            long ent_idx;

            for (cont = ov.GetFirstEntry(s, ent_idx); cont;
                 cont = ov.GetNextEntry(s, ent_idx))
                overrides->Write(s, ov.Read(s, wxEmptyString));

            ov.SetPath(wxT("/"));
            overrides->SetPath(wxT("/"));
        }
    }

    // create the main window
    int x = windowPositionX;
    int y = windowPositionY;
    int width = windowWidth;
    int height = windowHeight;
    int isFullscreen = fullScreen;
    frame = wxDynamicCast(xr->LoadFrame(NULL, wxT("MainFrame")), MainFrame);

    if (!frame) {
        wxLogError(_("Could not create main window"));
        return false;
    }

    // Create() cannot be overridden easily
    if (!frame->BindControls())
        return false;

    if (x >= 0 && y >= 0 && width > 0 && height > 0)
	frame->SetSize(x, y, width, height);

    if (isFullscreen && wxGetApp().pending_load != wxEmptyString)
	frame->ShowFullScreen(isFullscreen);
    frame->Show(true);
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
            N_("Save built-in XRC file and exit") },
        { wxCMD_LINE_OPTION, NULL, t("save-over"),
            N_("Save built-in vba-over.ini and exit") },
        { wxCMD_LINE_SWITCH, NULL, t("print-cfg-path"),
            N_("Print configuration path and exit") },
        { wxCMD_LINE_SWITCH, t("f"), t("fullscreen"),
            N_("Start in full-screen mode") },
#if !defined(NO_LINK) && !defined(__WXMSW__)
        { wxCMD_LINE_SWITCH, t("s"), t("delete-shared-state"),
            N_("Delete shared link state first, if it exists") },
#endif
        // stupid wx cmd line parser doesn't support duplicate options
        //	{ wxCMD_LINE_OPTION, t("o"),  t("option"),
        //		_("Set configuration option; <opt>=<value> or help for list"),
        {
            wxCMD_LINE_SWITCH, t("o"), t("list-options"),
            N_("List all settable options and exit") },
        { wxCMD_LINE_PARAM, NULL, NULL,
            N_("ROM file"), wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL },
        { wxCMD_LINE_PARAM, NULL, NULL,
            N_("<config>=<value>"), wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_MULTIPLE | wxCMD_LINE_PARAM_OPTIONAL },
        { wxCMD_LINE_NONE }
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
            wxLogError(_("Configuration/build error: can't find built-in xrc"));
            return false;
        }

        wxFileOutputStream os(s);
        os.Write(*f->GetStream());
        delete f;
        wxString lm;
        lm.Printf(_("Wrote built-in configuration to %s.\n"
                    "To override, remove all but changed root node(s).  "
                    "First found root node of correct name in any .xrc or "
                    ".xrs files in following search path overrides built-in:"),
            s.mb_str());
        tack_full_path(lm);
        wxLogMessage(lm);
        return false;
    }

    if (cl.Found(wxT("print-cfg-path"))) {
        // This was most likely done on a command line, so use
        // stderr instead of gui for messages
        wxLog::SetActiveTarget(new wxLogStderr);
        wxString lm(_("Configuration is read from, in order:"));
        tack_full_path(lm);
        wxLogMessage(lm);
        return false;
    }

    if (cl.Found(wxT("save-over"), &s)) {
        // This was most likely done on a command line, so use
        // stderr instead of gui for messages
        wxLog::SetActiveTarget(new wxLogStderr);
        wxFileOutputStream os(s);
        os.Write(builtin_over, sizeof(builtin_over));
        wxString lm;
        lm.Printf(_("Wrote built-in override file to %s\n"
                    "To override, delete all but changed section.  First found section is used from search path:"),
            s.mb_str());
        wxString oi = wxFileName::GetPathSeparator();
        oi += wxT("vba-over.ini");
        tack_full_path(lm, oi);
        lm.append(_("\n\tbuilt-in"));
        wxLogMessage(lm);
        return false;
    }

    if (cl.Found(wxT("f"))) {
        pending_fullscreen = true;
    }

    if (cl.Found(wxT("o"))) {
        wxPrintf(_("Options set from the command line are saved if any"
                   " configuration changes are made in the user interface.\n\n"
                   "For flag options, true and false are specified as 1 and 0, respectively.\n\n"));

        for (int i = 0; i < num_opts; i++) {
            wxPrintf(wxT("%s (%s"), opts[i].opt,
                opts[i].boolopt ? wxT("flag") : opts[i].stropt ? wxT("string") : !opts[i].enumvals.empty() ? opts[i].enumvals : (wxString)(opts[i].intopt ? wxT("int") : opts[i].doubleopt ? wxT("decimal") : wxT("string")));

            if (!opts[i].enumvals.empty()) {
                const wxString evx = wxGetTranslation(opts[i].enumvals);

                if (wxStrcmp(evx, opts[i].enumvals))
                    wxPrintf(wxT(" = %s"), evx);
            }

            wxPrintf(wxT(")\n\t%s\n\n"), opts[i].desc);

            if (!opts[i].enumvals.empty())
                opts[i].enumvals = wxGetTranslation(opts[i].enumvals);
        }

        wxPrintf(_("The commands available for the Keyboard/* option are:\n\n"));

        for (int i = 0; i < ncmds; i++)
            wxPrintf(wxT("%s (%s)\n"), cmdtab[i].cmd, cmdtab[i].name);

        return false;
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
        auto parts = str_split(p, wxT('='));

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
                    wxFprintf(stderr, wxT("%s\n"), pending_load.mb_str());
                    complained = true;
                }

                wxFprintf(stderr, wxT("%s\n"), p);
            }
        }
    }

    home = strdup(wxString(wxApp::argv[0]).char_str());
    SetHome(home);
    LoadConfig(); // Parse command line arguments (overrides ini)
    ReadOpts(argc, (char**)argv);
    return true;
}

wxvbamApp::~wxvbamApp() {
    if (home != NULL)
	free(home);
}

MainFrame::MainFrame()
    : wxFrame()
    , focused(false)
    , paused(false)
    , menus_opened(0)
    , dialog_opened(0)
{
}

MainFrame::~MainFrame()
{
#ifndef NO_LINK
    CloseLink();
#endif
}

BEGIN_EVENT_TABLE(MainFrame, wxFrame)
#include "cmd-evtable.h"
EVT_CONTEXT_MENU(MainFrame::OnMenu)
// this is the main window focus?  Or EVT_SET_FOCUS/EVT_KILL_FOCUS?
EVT_ACTIVATE(MainFrame::OnActivate)
// requires DragAcceptFiles(true); even then may not do anything
EVT_DROP_FILES(MainFrame::OnDropFile)

// for window geometry
EVT_MOVE(MainFrame::OnMove)
EVT_SIZE(MainFrame::OnSize)
// pause game if menu pops up
//
// This is a feature most people don't like, and it causes problems with
// keyboard game keys on mac, so we will disable it for now.
//
// On Winodws, there will still be a pause because of how the windows event
// model works, in addition the audio will loop with SDL, so we still pause on
// Windows, TODO: this needs to be fixed properly
//
#ifdef __WXMSW__
EVT_MENU_OPEN(MainFrame::MenuPopped)
EVT_MENU_CLOSE(MainFrame::MenuPopped)
EVT_MENU_HIGHLIGHT_ALL(MainFrame::MenuPopped)
#endif

END_EVENT_TABLE()

void MainFrame::OnActivate(wxActivateEvent& event)
{
    focused = event.GetActive();

    if (panel && focused)
        panel->SetFocus();

    if (pauseWhenInactive) {
        if (panel && focused) {
            panel->Resume();
        }
        else if (panel && !focused) {
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

void MainFrame::OnMove(wxMoveEvent& event)
{
    wxRect pos = GetRect();
    int x = pos.GetX(), y = pos.GetY();
    if (x >= 0 && y >= 0 && !IsFullScreen())
    {
	windowPositionX = x;
	windowPositionY = y;
	update_opts();
    }
}

void MainFrame::OnSize(wxSizeEvent& event)
{
    wxFrame::OnSize(event);
    wxRect pos = GetRect();
    int height = pos.GetHeight(), width = pos.GetWidth();
    int x = pos.GetX(), y = pos.GetY();
    bool isFullscreen = IsFullScreen();
    if (height > 0 && width > 0 && !isFullscreen)
    {
	windowHeight = height;
	windowWidth = width;
    }
    if (x >= 0 && y >= 0 && !isFullscreen)
    {
	windowPositionX = x;
	windowPositionY = y;
    }
    fullScreen = isFullscreen;
    update_opts();
}

wxString MainFrame::GetGamePath(wxString path)
{
    wxString game_path = path;

    if (game_path.size()) {
        game_path = wxGetApp().GetAbsolutePath(game_path);
    } else {
        game_path = panel->game_dir();
        wxFileName::Mkdir(game_path, 0777, wxPATH_MKDIR_FULL);
    }

    if (!wxFileName::DirExists(game_path))
        game_path = wxFileName::GetCwd();

    if (!wxIsWritable(game_path))
        game_path = wxGetApp().GetConfigurationPath();

    return game_path;
}

void MainFrame::SetJoystick()
{
    bool anyjoy = false;
    joy.Remove();

    if (!emulating)
        return;

    for (int i = 0; i < 4; i++)
        for (int j = 0; j < NUM_KEYS; j++) {
            wxJoyKeyBinding_v b = gopts.joykey_bindings[i][j];

            for (int k = 0; k < b.size(); k++) {
                int jn = b[k].joy;

                if (jn) {
                    if (!anyjoy) {
                        anyjoy = true;
                        joy.Attach(panel);
                    }

                    joy.Add(jn - 1);
                }
            }
        }
}

void MainFrame::enable_menus()
{
    for (int i = 0; i < ncmds; i++)
        if (cmdtab[i].mask_flags && cmdtab[i].mi)
            cmdtab[i].mi->Enable((cmdtab[i].mask_flags & cmd_enable) != 0);

    if (cmd_enable & CMDEN_SAVST)
        for (int i = 0; i < 10; i++)
            if (loadst_mi[i])
                loadst_mi[i]->Enable(state_ts[i].IsValid());
}

void MainFrame::SetRecentAccels()
{
    for (int i = 0; i < 10; i++) {
        wxMenuItem* mi = recent->FindItem(i + wxID_FILE1);

        if (!mi)
            break;

        // if command is 0, there is no accel
        DoSetAccel(mi, recent_accel[i].GetCommand() ? &recent_accel[i] : NULL);
    }
}

void MainFrame::update_state_ts(bool force)
{
    bool any_states = false;

    for (int i = 0; i < 10; i++) {
        if (force)
            state_ts[i] = wxInvalidDateTime;

        if (panel->game_type() != IMAGE_UNKNOWN) {
            wxString fn;
            fn.Printf(SAVESLOT_FMT, panel->game_name().mb_str(), i + 1);
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
                    for (int j = 0; j < df.size(); j++)
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

// disable emulator loop while menus are popped up
// not sure how to match up w/ down other than counting...
// only msw is guaranteed to only give one up & one down event for entire
// menu browsing
// if there is ever a mismatch, the game will freeze and there is no way
// to detect if it's doing that

// FIXME: this does not work.
// Not all open events are followed by close events.
// Removing the nesting counter may help, but on wxGTK I still get lockups.
void MainFrame::MenuPopped(wxMenuEvent& evt)
{
    bool popped = evt.GetEventType() != wxEVT_MENU_CLOSE;
#if 0

	if (popped)
		++menus_opened;
	else
		--menus_opened;

	if (menus_opened < 0) // how could this ever be???
		menus_opened = 0;

#else

    if (popped)
        menus_opened = 1;
    else
        menus_opened = 0;

#endif

    // workaround for lack of wxGTK mouse motion events: unblank
    // pointer when menu popped up
    // of course this does not help in finding the menus in the first place
    // the user is better off clicking in the window or entering/
    // exiting the window (which does generate a mouse event)
    // it will auto-hide again once game resumes
    if (popped)
        panel->ShowPointer();

    if (menus_opened)
        panel->Pause();
    else if (!IsPaused())
        panel->Resume();
}

void MainFrame::SetMenusOpened(bool state)
{
    if (state) {
        menus_opened = 1;
        paused       = true;
        panel->Pause();
    }
    else {
        menus_opened = 0;
        paused       = false;
        pause_next   = false;
        panel->Resume();
    }
}

// ShowModal that also disables emulator loop
// uses dialog_opened as a nesting counter
int MainFrame::ShowModal(wxDialog* dlg)
{
    dlg->SetWindowStyle(dlg->GetWindowStyle() | wxCAPTION | wxRESIZE_BORDER);

    if (gopts.keep_on_top)
        dlg->SetWindowStyle(dlg->GetWindowStyle() | wxSTAY_ON_TOP);
    else
        dlg->SetWindowStyle(dlg->GetWindowStyle() & ~wxSTAY_ON_TOP);

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
    panel->Pause();
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

LinkMode MainFrame::GetConfiguredLinkMode()
{
    switch (gopts.gba_link_type) {
    case 0:
        return LINK_DISCONNECTED;
        break;

    case 1:
        if (gopts.link_proto)
            return LINK_CABLE_IPC;
        else
            return LINK_CABLE_SOCKET;

        break;

    case 2:
        if (gopts.link_proto)
            return LINK_RFU_IPC;
        else
            return LINK_RFU_SOCKET;

        break;

    case 3:
        return LINK_GAMECUBE_DOLPHIN;
        break;

    case 4:
        if (gopts.link_proto)
            return LINK_GAMEBOY_IPC;
        else
            return LINK_GAMEBOY_SOCKET;

        break;

    default:
        return LINK_DISCONNECTED;
        break;
    }

    return LINK_DISCONNECTED;
}

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

// global event filter
// apparently required for win32; just setting accel table still misses
// a few keys (e.g. only ctrl-x works for exit, but not esc & ctrl-q;
// ctrl-w does not work for close).  It's possible another entity is
// grabbing those keys, but I can't track it down.
// FIXME: move this to MainFrame
//int wxvbamApp::FilterEvent(wxEvent& event)
//{
//    //if(frame && frame->IsPaused(true))
//    return -1;
//
//    if (event.GetEventType() == wxEVT_KEY_DOWN) {
//        wxKeyEvent& ke = (wxKeyEvent&)event;
//
//        for (int i = 0; i < accels.size(); i++) {
//            if (accels[i].GetKeyCode() == ke.GetKeyCode() && accels[i].GetFlags() == ke.GetModifiers()) {
//                wxCommandEvent ev(wxEVT_COMMAND_MENU_SELECTED, accels[i].GetCommand());
//                ev.SetEventObject(this);
//                frame->GetEventHandler()->ProcessEvent(ev);
//                return 1;
//            }
//        }
//    }
//
//    return -1;
//}
