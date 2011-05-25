
// mainline:
//   parse cmd line
//   load xrc file (guiinit.cpp does most of instantiation)
//   create & display main frame

#include "wxvbam.h"
#include <wx/filesys.h>
#include <wx/fs_mem.h>
#include <wx/fs_arc.h>
#include <wx/file.h>
#include <wx/wfstream.h>
#include <wx/mstream.h>
#include <wx/sstream.h>
#include <wx/cmdline.h>
#include <wx/regex.h>

// The built-in xrc file
#include "builtin-xrc.h"

// The built-in vba-over.ini
#include "builtin-over.h"

IMPLEMENT_APP(wxvbamApp)
IMPLEMENT_DYNAMIC_CLASS(MainFrame, wxFrame)

// generate config file path
static void get_config_path(wxPathList &path, bool exists = true)
{
    //   local config dir first, then global
    //   locale-specific res first, then main
    wxStandardPathsBase& stdp = wxStandardPaths::Get();
#define add_path(p) do { \
    const wxString& s = stdp.p; \
    if(!exists || wxDirExists(s)) \
	path.Add(s); \
} while(0)
    // NOTE: this does not support XDG (freedesktop.org) paths
    add_path(GetUserLocalDataDir());
    add_path(GetUserDataDir());
    add_path(GetLocalizedResourcesDir(wxGetApp().locale.GetCanonicalName()));
    add_path(GetResourcesDir());
    add_path(GetDataDir());
    add_path(GetLocalDataDir());
}

static void tack_full_path(wxString &s, const wxString &app = wxEmptyString)
{
    // regenerate path, including nonexistent dirs this time
    wxPathList full_config_path;
    get_config_path(full_config_path, false);
    for(int i = 0; i < full_config_path.size(); i++)
	s += wxT("\n\t") + full_config_path[i] + app;
}

bool wxvbamApp::OnInit()
{
    // use consistent names for config
    SetAppName(_T("wxvbam"));

    // load system default locale, if available
    locale.Init();
    locale.AddCatalog(_T("wxvbam"));

    // make built-in xrc file available
    // this has to be done before parent OnInit() so xrc dump works
    wxFileSystem::AddHandler(new wxMemoryFSHandler);
    wxFileSystem::AddHandler(new wxArchiveFSHandler);
    wxMemoryFSHandler::AddFileWithMimeType(wxT("wxvbam.xrs"), builtin_xrs, sizeof(builtin_xrs), wxT("application/zip"));

    if ( !wxApp::OnInit() )
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
    for(int i = 0; i < config_path.size(); i++)
	if(wxSetWorkingDirectory(config_path[i])) {
	    // *.xr[cs] doesn't work (double the number of scans)
	    // 2.9 gives errors for no files found, so manual precheck needed
	    // (yet another double the number of scans)
	    if(!wxFindFirstFile(wxT("*.xrc")).empty())
		xr->Load(wxT("*.xrc"));
	    if(!wxFindFirstFile(wxT("*.xrs")).empty())
		xr->Load(wxT("*.xrs"));
	}
    wxSetWorkingDirectory(cwd);

    // finally, load built-in xrc
    xr->Load(wxT("memory:wxvbam.xrs"));

    // set up config file
    // this needs to be in a subdir to support other config as well
    // but subdir flag behaves differently 2.8 vs. 2.9.  Oh well.
    // NOTE: this does not support XDG (freedesktop.org) paths
    cfg = new wxConfig(wxEmptyString, wxEmptyString, wxEmptyString,
		       wxEmptyString,
		       // style =
		       wxCONFIG_USE_GLOBAL_FILE|wxCONFIG_USE_LOCAL_FILE|
		         wxCONFIG_USE_SUBDIR);
    // set global config for e.g. Windows font mapping
    wxConfig::Set(cfg);
    // yet another bug/deficiency in wxConfig: dirs are not created if needed
    // since a default config is always written, dirs are always needed
    // Can't figure out statically if using wxFileConfig w/o duplicating wx's
    // logic, so do it at run-time
    // wxFileConfig *f = wxDynamicCast(cfg, wxFileConfig);
    // wxConfigBase does not derive from wxObject!!! so no wxDynamicCast
    wxFileConfig *f = dynamic_cast<wxFileConfig *>(cfg);
    if(f) {
	wxFileName s(wxFileConfig::GetLocalFileName(GetAppName()));
	// at least up to 2.8.12, GetLocalFileName returns the dir if
	// SUBDIR is specified instead of actual file name
	// and SUBDIR only affects UNIX
#if defined(__UNIX__) && !wxCHECK_VERSION(2,9,0)
	s.AppendDir(s.GetFullName());
#endif
	// only the path part gets created
	// note that 0777 is default (assumes umask will do og-w)
	s.Mkdir(0777, wxPATH_MKDIR_FULL);
    }
    load_opts();
    // process command-line options
    for(int i = 0; i < pending_optset.size(); i++) {
	wxString p = pending_optset[i];
	size_t eqat = p.find(wxT('='));
	p[eqat] = 0;
	opt_set(p.c_str(), p.c_str() + eqat + 1);
    }
    pending_optset.clear();

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
    wxString bovs((const char *)builtin_over, wxConvUTF8, sizeof(builtin_over));
    bool cont;
    wxString s;
    long grp_idx;
    for(cont = overrides->GetFirstGroup(s, grp_idx); cont;
	cont = overrides->GetNextGroup(s, grp_idx)) {
	// apparently even MacOSX sometimes uses the old \r by itself
#define CMT_RE_START wxT("(^|[\n\r])# ?([^\n\r]*)(\r?\n|\r)\\[")
	wxString cmt(CMT_RE_START);
	cmt += s + wxT("\\]");
	if(cmtre.Compile(cmt) && cmtre.Matches(bovs))
	    cmt = cmtre.GetMatch(bovs, 2);
	else
	    cmt = wxEmptyString;
	overrides->Write(s + wxT("/comment"), cmt);
    }
    for(int i = config_path.size() - 1; i >= 0 ; i--) {
	wxFileName fn(config_path[i], wxT("vba-over.ini"));
	if(!fn.IsFileReadable())
	    continue;
	wxStringOutputStream sos;
	wxFileInputStream fis(fn.GetFullPath());
	// not the most efficient thing to do: read entire file into a string
	// just to parse the comments out
	fis.Read(sos);
	// rather than assuming the file is seekable, use the string we just
	// read as an input stream
	wxStringInputStream sis(sos.GetString());
	wxFileConfig ov(sis);

	for(cont = ov.GetFirstGroup(s, grp_idx); cont;
	    cont = ov.GetNextGroup(s, grp_idx)) {
	    overrides->DeleteGroup(s);
	    overrides->SetPath(s);
	    ov.SetPath(s);
	    overrides->Write(wxT("path"), config_path[i]);
	    // apparently even MacOSX sometimes uses \r by itself
	    wxString cmt(CMT_RE_START);
	    cmt += s + wxT("\\]");
	    if(cmtre.Compile(cmt) && cmtre.Matches(sos.GetString()))
		cmt = cmtre.GetMatch(sos.GetString(), 2);
	    else
		cmt = wxEmptyString;
	    overrides->Write(wxT("comment"), cmt);
	    long ent_idx;
	    for(cont = ov.GetFirstEntry(s, ent_idx); cont;
		cont = ov.GetNextEntry(s, ent_idx))
		overrides->Write(s, ov.Read(s, wxEmptyString));
	    ov.SetPath(wxT("/"));
	    overrides->SetPath(wxT("/"));
	}
    }

    // create the main window
    frame = wxDynamicCast(xr->LoadFrame(NULL, wxT("MainFrame")), MainFrame);
    if(!frame) {
	wxLogError(_("Could not create main window"));
	return false;
    }
    // Create() cannot be overridden easily
    if(!frame->InitMore())
	return false;

    frame->Show(true);

    return true;
}

void wxvbamApp::OnInitCmdLine(wxCmdLineParser &cl)
{
    wxApp::OnInitCmdLine(cl);

    cl.SetLogo(wxT("VisualBoyAdvance-M " VERSION "\n"));

    // 2.9 decided to change all of these from wxChar to char for some
    // reason
#if wxCHECK_VERSION(2,9,0)
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
	{ wxCMD_LINE_OPTION, NULL,    t("save-xrc"),
		N_("Save built-in XRC file and exit") },
	{ wxCMD_LINE_OPTION, NULL,    t("save-over"),
		N_("Save built-in vba-over.ini and exit") },
	{ wxCMD_LINE_SWITCH, NULL,    t("print-cfg-path"),
		N_("Print configuration path and exit") },
	{ wxCMD_LINE_SWITCH, t("f"),  t("fullscreen"),
		N_("Start in full-screen mode") },
#if !defined(NO_LINK) && !defined(__WXMSW__)
	{ wxCMD_LINE_SWITCH, t("s"),  t("delete-shared-state"),
		N_("Delete shared link state first, if it exists") },
#endif
	// stupid wx cmd line parser doesn't support duplicate options
//	{ wxCMD_LINE_OPTION, t("o"),  t("option"),
//		_("Set configuration option; <opt>=<value> or help for list"),
	{ wxCMD_LINE_SWITCH, t("o"),  t("list-options"),
		N_("List all settable options and exit") },
	{ wxCMD_LINE_PARAM,  NULL,    NULL,
		N_("ROM file"), wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL },
	{ wxCMD_LINE_PARAM,  NULL,    NULL,
		N_("<config>=<value>"), wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_MULTIPLE|wxCMD_LINE_PARAM_OPTIONAL },
	{ wxCMD_LINE_NONE }
    };
    // 2.9 automatically translates desc, but 2.8 doesn't
#if !wxCHECK_VERSION(2,9,0)
    for(int i = 0; opttab[i].kind != wxCMD_LINE_NONE; i++)
	opttab[i].description = wxGetTranslation(opttab[i].description);
#endif

    // note that "SetDesc" actually Adds rather than replaces
    // so system standard (port-dependent) options are still included:
    //   -h/--help  --verbose --theme --mode
    cl.SetDesc(opttab);
}

bool wxvbamApp::OnCmdLineParsed(wxCmdLineParser &cl)
{
    if(!wxApp::OnCmdLineParsed(cl))
	return false;
    wxString s;
    if(cl.Found(_("save-xrc"), &s)) {
	// This was most likely done on a command line, so use
	// stderr instead of gui for messages
	wxLog::SetActiveTarget(new wxLogStderr);

	wxFileSystem fs;
	wxFSFile *f = fs.OpenFile(wxT("memory:wxvbam.xrs#zip:wxvbam.xrs$wxvbam.xrc"));
	if(!f) {
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
		  s.c_str());
	tack_full_path(lm);
	wxLogMessage(lm);
	return false;
    }
    if(cl.Found(_("print-cfg-path"))) {
	// This was most likely done on a command line, so use
	// stderr instead of gui for messages
	wxLog::SetActiveTarget(new wxLogStderr);

	wxString lm(_("Configuration is read from, in order:"));
	tack_full_path(lm);
	wxLogMessage(lm);
	return false;
    }
    if(cl.Found(_("save-over"), &s)) {
	// This was most likely done on a command line, so use
	// stderr instead of gui for messages
	wxLog::SetActiveTarget(new wxLogStderr);

	wxFileOutputStream os(s);
	os.Write(builtin_over, sizeof(builtin_over));
	wxString lm;
	lm.Printf(_("Wrote built-in override file to %s\n"
		    "To override, delete all but changed section.  First found section is used from search path:"), s.c_str());
	wxString oi = wxFileName::GetPathSeparator();
	oi += wxT("vba-over.ini");
	tack_full_path(lm, oi);
	lm.append(_("\n\tbuilt-in"));
	wxLogMessage(lm);
	return false;
    }
    if(cl.Found(wxT("f"))) {
	pending_fullscreen = true;
    }
    if(cl.Found(wxT("o"))) {
	wxPrintf(_("Options set from the command line are saved if any"
		   " configuration changes are made in the user interface.\n\n"
		   "For flag options, true and false are specified as 1 and 0, respectively.\n\n"));
	for(int i = 0; i < num_opts; i++) {
	    wxPrintf(wxT("%s (%s"), opts[i].opt,
		     opts[i].boolopt ? (const wxChar *)_("flag") :
		     opts[i].stropt ? (const wxChar *)_("string") :
		     opts[i].enumvals ? opts[i].enumvals :
		     opts[i].intopt ? (const wxChar *)_("int") :
		     (const wxChar *)_("string"));
	    if(opts[i].enumvals) {
		const wxChar *evx = wxGetTranslation(opts[i].enumvals);
		if(wxStrcmp(evx, opts[i].enumvals))
		    wxPrintf(wxT(" = %s"), evx);
	    }
	    wxPrintf(wxT(")\n\t%s\n\n"), opts[i].desc);
	if(opts[i].enumvals)
	    opts[i].enumvals = wxGetTranslation(opts[i].enumvals);
	}
	wxPrintf(_("The commands available for the Keyboard/* option are:\n\n"));
	for(int i = 0; i < ncmds; i++)
	    wxPrintf(wxT("%s (%s)\n"), cmdtab[i].cmd, cmdtab[i].name);
	return false;
    }
#if !defined(NO_LINK) && !defined(__WXMSW__)
    if(cl.Found(wxT("s"))) {
	CleanLocalLink();
    }
#endif
    int nparm = cl.GetParamCount();
    bool complained = false, gotfile = false;
    for(int i = 0; i < nparm; i++) {
	wxString p = cl.GetParam(i);
	size_t eqat = p.find(wxT('='));
	if(eqat != wxString::npos) {
	    p[eqat] = 0;
	    if(!opt_set(p.c_str(), p.c_str() + eqat + 1)) {
		p[eqat] = wxT('=');
		eqat = wxString::npos;
	    } else
		p[eqat] = wxT('=');
	    pending_optset.push_back(p);
	}
	if(eqat == wxString::npos) {
	    if(!gotfile) {
		pending_load = p;
		gotfile = true;
	    } else {
		if(!complained) {
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

MainFrame::MainFrame() : wxFrame(), did_link_init(false), focused(false),
    paused(false), menus_opened(0), dialog_opened(0) {}

MainFrame::~MainFrame()
{
    if(did_link_init)
	CloseLink();
}

BEGIN_EVENT_TABLE(MainFrame, wxFrame)
    #include "cmd-evtable.h"
    EVT_CONTEXT_MENU(MainFrame::OnMenu)
    // this is the main window focus?  Or EVT_SET_FOCUS/EVT_KILL_FOCUS?
    EVT_ACTIVATE(MainFrame::OnActivate)
    // requires DragAcceptFiles(true); even then may not do anything
    EVT_DROP_FILES(MainFrame::OnDropFile)
    // pause game if menu pops up
    EVT_MENU_OPEN(MainFrame::MenuPopped)
    EVT_MENU_CLOSE(MainFrame::MenuPopped)
END_EVENT_TABLE()

void MainFrame::OnActivate(wxActivateEvent &event)
{
    focused = event.GetActive();
    if(panel && focused)
	panel->SetFocus();
}

void MainFrame::OnDropFile(wxDropFilesEvent &event)
{
    int n = event.GetNumberOfFiles();
    wxString *f = event.GetFiles();
    // ignore all but last
    wxGetApp().pending_load = f[event.GetNumberOfFiles() - 1];
}

void MainFrame::OnMenu(wxContextMenuEvent &event)
{
    if(IsFullScreen() && ctx_menu) {
	wxPoint p(event.GetPosition());
#if 0 // wx actually recommends ignoring the position
	if(p != wxDefaultPosition)
	    p = ScreenToClient(p);
#endif
	PopupMenu(ctx_menu, p);
    }
}

void MainFrame::SetJoystick()
{
    bool anyjoy = false;
    joy.Remove();
    if(!emulating)
	return;
    for(int i = 0; i < 4; i++)
	for(int j = 0; j < NUM_KEYS; j++) {
	    wxJoyKeyBinding_v b = gopts.joykey_bindings[i][j];
	    for(int k = 0; k < b.size(); k++) {
		int jn = b[k].joy;
		if(jn) {
		    if(!anyjoy) {
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
    for(int i = 0; i < ncmds; i++)
	if(cmdtab[i].mask_flags && cmdtab[i].mi)
	    cmdtab[i].mi->Enable((cmdtab[i].mask_flags & cmd_enable) != 0);
    if(cmd_enable & CMDEN_SAVST)
	for(int i = 0; i < 10; i++)
	    if(loadst_mi[i])
		loadst_mi[i]->Enable(state_ts[i].IsValid());
}

void MainFrame::SetRecentAccels()
{
    for(int i = 0; i <= 10; i++) {
	wxMenuItem *mi = recent->FindItem(i + wxID_FILE1);
	if(!mi)
	    break;
	// if command is 0, there is no accel
	DoSetAccel(mi, recent_accel[i].GetCommand() ? &recent_accel[i] : NULL);
    }
}

void MainFrame::update_state_ts(bool force)
{
    bool any_states = false;
    for(int i = 0; i < 10; i++) {
	if(force)
	    state_ts[i] = wxInvalidDateTime;
	if(panel->game_type() != IMAGE_UNKNOWN) {
	    wxString fn;
	    fn.Printf(SAVESLOT_FMT, panel->game_name().c_str(), i + 1);
	    wxFileName fp(panel->state_dir(), fn);
	    wxDateTime ts; // = wxInvalidDateTime
	    if(fp.IsFileReadable()) {
		ts = fp.GetModificationTime();
		any_states = true;
	    }
	    // if(ts != state_ts[i] || (force && !ts.IsValid())) {
	    // to prevent assertions (stupid wx):
	    if(ts.IsValid() != state_ts[i].IsValid() ||
	       (ts.IsValid() && ts != state_ts[i]) ||
	       (force && !ts.IsValid())) {
		// wx has no easy way of doing the -- bit independent
		// of locale
		// so use a real date and substitute all digits
		wxDateTime fts = ts.IsValid() ? ts : wxDateTime::Now();
		wxString df = fts.Format(wxT("0&0 %x %X"));
		if(!ts.IsValid())
		    for(int j = 0; j < df.size(); j++)
			if(wxIsdigit(df[j]))
			    df[j] = wxT('-');
		df[0] = i == 9 ? wxT('1') : wxT(' ');
		df[2] = wxT('0') + (i + 1) % 10;
		if(loadst_mi[i]) {
		    wxString lab = loadst_mi[i]->GetItemLabel();
		    size_t tab = lab.find(wxT('\t')), dflen = df.size();
		    if(tab != wxString::npos)
			df.append(lab.substr(tab));
		    loadst_mi[i]->SetItemLabel(df);
		    loadst_mi[i]->Enable(ts.IsValid());
		    if(tab != wxString::npos)
			df.resize(dflen);
		}
		if(savest_mi[i]) {
		    wxString lab = savest_mi[i]->GetItemLabel();
		    size_t tab = lab.find(wxT('\t'));
		    if(tab != wxString::npos)
			df.append(lab.substr(tab));
		    savest_mi[i]->SetItemLabel(df);
		}
	    }
	    state_ts[i] = ts;
	}
    }
    int cmd_flg = any_states ? CMDEN_SAVST : 0;
    if((cmd_enable & CMDEN_SAVST) != cmd_flg) {
	cmd_enable = (cmd_enable & ~CMDEN_SAVST) | cmd_flg;
	enable_menus();
    }
}

int MainFrame::oldest_state_slot()
{
    wxDateTime ot;
    int os = -1;
    for(int i = 0; i < 10; i++) {
	if(!state_ts[i].IsValid())
	    return i + 1;
	if(os < 0 || state_ts[i] < ot) {
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
    for(int i = 0; i < 10; i++) {
	if(!state_ts[i].IsValid())
	    continue;
	if(ns < 0 || state_ts[i] > nt) {
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
void MainFrame::MenuPopped(wxMenuEvent &evt)
{
    bool popped = evt.GetEventType() == wxEVT_MENU_OPEN;

#if 0
    if(popped)
	++menus_opened;
    else
	--menus_opened;
    if(menus_opened < 0) // how could this ever be???
	menus_opened = 0;
#else
    if(popped)
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
    if(popped)
	panel->ShowPointer();
    if(menus_opened)
	panel->Pause();
    else if(!IsPaused())
	panel->Resume();
}

// ShowModal that also disables emulator loop
// uses dialog_opened as a nesting counter
int MainFrame::ShowModal(wxDialog *dlg)
{
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
    if(!dialog_opened) // technically an error in the caller
	return;
    --dialog_opened;
    if(!IsPaused())
	panel->Resume();
}

// global event filter
// apparently required for win32; just setting accel table still misses
// a few keys (e.g. only ctrl-x works for exit, but not esc & ctrl-q;
// ctrl-w does not work for close).  It's possible another entity is
// grabbing those keys, but I can't track it down.
// FIXME: move this to MainFrame
int wxvbamApp::FilterEvent(wxEvent &event)
{
    if(frame && frame->IsPaused(true))
	return -1;
    if(event.GetEventType() == wxEVT_KEY_DOWN) {
	wxKeyEvent &ke = (wxKeyEvent &)event;

	for(int i = 0; i < accels.size(); i++) {
	    if(accels[i].GetKeyCode() == ke.GetKeyCode() &&
	       accels[i].GetFlags() == ke.GetModifiers()) {
		wxCommandEvent ev(wxEVT_COMMAND_MENU_SELECTED, accels[i].GetCommand());
		ev.SetEventObject(this);
		frame->GetEventHandler()->ProcessEvent(ev);
		return 1;
	    }
	}
    }
    return -1;
}
