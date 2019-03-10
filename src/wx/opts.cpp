#include "../common/ConfigManager.h"
#include "wxvbam.h"
#include <algorithm>
#include <wx/display.h>
#include "strutils.h"

/*
       disableSfx(F) -> cpuDisableSfx
       priority(2) -> threadPriority
       saveMoreCPU(F) -> Sm60FPS

     SDL:
      -p/--profile=hz
*/

/* not sure how well other compilers support field-init syntax */
#define STROPT(c, n, d, v) \
    {                      \
        wxT(c), (n), d, &v \
    }
#define INTOPT(c, n, d, v, min, max)             \
    {                                            \
        wxT(c), (n), d, NULL, &v, wxT(""), min, max \
    }
#define DOUBLEOPT(c, n, d, v, min, max)                      \
    {                                                        \
        wxT(c), (n), d, NULL, NULL, wxT(""), min, max, NULL, &v \
    }
#define UINTOPT(c, n, d, v, min, max)                      \
    {                                                        \
        wxT(c), (n), d, NULL, NULL, wxT(""), min, max, NULL, NULL, &v \
    }
#define BOOLOPT(c, n, d, v)                        \
    {                                              \
        wxT(c), (n), d, NULL, NULL, wxT(""), 0, 0, &v \
    }
#define ENUMOPT(c, n, d, v, e)      \
    {                               \
        wxT(c), (n), d, NULL, &v, e \
    }

opts_t gopts;

// having the standard menu accels here means they will work even without menus
const wxAcceleratorEntry default_accels[] = {
    wxAcceleratorEntry(wxMOD_CMD, wxT('C'), XRCID("CheatsList")),
    wxAcceleratorEntry(wxMOD_CMD, wxT('N'), XRCID("NextFrame")),
    // some ports add ctrl-q anyway, so may as well make it official
    // maybe make alt-f4 universal as well...
    // FIXME: ctrl-Q does not work on wxMSW
    // FIXME: esc does not work on wxMSW

    // this was annoying people A LOT #334
    //wxAcceleratorEntry(wxMOD_NONE, WXK_ESCAPE, wxID_EXIT),

    // this was annoying people #298
    //wxAcceleratorEntry(wxMOD_CMD, wxT('X'), wxID_EXIT),

    wxAcceleratorEntry(wxMOD_CMD, wxT('Q'), wxID_EXIT),
    // FIXME: ctrl-W does not work on wxMSW
    wxAcceleratorEntry(wxMOD_CMD, wxT('W'), wxID_CLOSE),
    // load most recent is more commonly used than load other
    //wxAcceleratorEntry(wxMOD_CMD, wxT('L'), XRCID("Load")),
    wxAcceleratorEntry(wxMOD_CMD, wxT('L'), XRCID("LoadGameRecent")),
    wxAcceleratorEntry(wxMOD_NONE, WXK_F1, XRCID("LoadGame01")),
    wxAcceleratorEntry(wxMOD_NONE, WXK_F2, XRCID("LoadGame02")),
    wxAcceleratorEntry(wxMOD_NONE, WXK_F3, XRCID("LoadGame03")),
    wxAcceleratorEntry(wxMOD_NONE, WXK_F4, XRCID("LoadGame04")),
    wxAcceleratorEntry(wxMOD_NONE, WXK_F5, XRCID("LoadGame05")),
    wxAcceleratorEntry(wxMOD_NONE, WXK_F6, XRCID("LoadGame06")),
    wxAcceleratorEntry(wxMOD_NONE, WXK_F7, XRCID("LoadGame07")),
    wxAcceleratorEntry(wxMOD_NONE, WXK_F8, XRCID("LoadGame08")),
    wxAcceleratorEntry(wxMOD_NONE, WXK_F9, XRCID("LoadGame09")),
    wxAcceleratorEntry(wxMOD_NONE, WXK_F10, XRCID("LoadGame10")),
    wxAcceleratorEntry(wxMOD_NONE, WXK_PAUSE, XRCID("Pause")),
    wxAcceleratorEntry(wxMOD_CMD, wxT('P'), XRCID("Pause")),
    wxAcceleratorEntry(wxMOD_CMD, wxT('R'), XRCID("Reset")),
    // save oldest is more commonly used than save other
    //wxAcceleratorEntry(wxMOD_CMD, wxT('S'), XRCID("Save")),
    wxAcceleratorEntry(wxMOD_CMD, wxT('S'), XRCID("SaveGameOldest")),
    wxAcceleratorEntry(wxMOD_SHIFT, WXK_F1, XRCID("SaveGame01")),
    wxAcceleratorEntry(wxMOD_SHIFT, WXK_F2, XRCID("SaveGame02")),
    wxAcceleratorEntry(wxMOD_SHIFT, WXK_F3, XRCID("SaveGame03")),
    wxAcceleratorEntry(wxMOD_SHIFT, WXK_F4, XRCID("SaveGame04")),
    wxAcceleratorEntry(wxMOD_SHIFT, WXK_F5, XRCID("SaveGame05")),
    wxAcceleratorEntry(wxMOD_SHIFT, WXK_F6, XRCID("SaveGame06")),
    wxAcceleratorEntry(wxMOD_SHIFT, WXK_F7, XRCID("SaveGame07")),
    wxAcceleratorEntry(wxMOD_SHIFT, WXK_F8, XRCID("SaveGame08")),
    wxAcceleratorEntry(wxMOD_SHIFT, WXK_F9, XRCID("SaveGame09")),
    wxAcceleratorEntry(wxMOD_SHIFT, WXK_F10, XRCID("SaveGame10")),
    // I prefer the SDL ESC key binding
    //wxAcceleratorEntry(wxMOD_NONE, WXK_ESCAPE, XRCID("ToggleFullscreen"),
    // alt-enter is more standard anyway
    wxAcceleratorEntry(wxMOD_ALT, WXK_RETURN, XRCID("ToggleFullscreen")),
    wxAcceleratorEntry(wxMOD_ALT, wxT('1'), XRCID("JoypadAutofireA")),
    wxAcceleratorEntry(wxMOD_ALT, wxT('2'), XRCID("JoypadAutofireB")),
    wxAcceleratorEntry(wxMOD_ALT, wxT('3'), XRCID("JoypadAutofireL")),
    wxAcceleratorEntry(wxMOD_ALT, wxT('4'), XRCID("JoypadAutofireR")),
    wxAcceleratorEntry(wxMOD_CMD, wxT('1'), XRCID("VideoLayersBG0")),
    wxAcceleratorEntry(wxMOD_CMD, wxT('2'), XRCID("VideoLayersBG1")),
    wxAcceleratorEntry(wxMOD_CMD, wxT('3'), XRCID("VideoLayersBG2")),
    wxAcceleratorEntry(wxMOD_CMD, wxT('4'), XRCID("VideoLayersBG3")),
    wxAcceleratorEntry(wxMOD_CMD, wxT('5'), XRCID("VideoLayersOBJ")),
    wxAcceleratorEntry(wxMOD_CMD, wxT('6'), XRCID("VideoLayersWIN0")),
    wxAcceleratorEntry(wxMOD_CMD, wxT('7'), XRCID("VideoLayersWIN1")),
    wxAcceleratorEntry(wxMOD_CMD, wxT('8'), XRCID("VideoLayersOBJWIN")),
    wxAcceleratorEntry(wxMOD_CMD, wxT('B'), XRCID("Rewind")),
    // following are not in standard menus
    // FILExx are filled in when recent menu is filled
    wxAcceleratorEntry(wxMOD_CMD, WXK_F1, wxID_FILE1),
    wxAcceleratorEntry(wxMOD_CMD, WXK_F2, wxID_FILE2),
    wxAcceleratorEntry(wxMOD_CMD, WXK_F3, wxID_FILE3),
    wxAcceleratorEntry(wxMOD_CMD, WXK_F4, wxID_FILE4),
    wxAcceleratorEntry(wxMOD_CMD, WXK_F5, wxID_FILE5),
    wxAcceleratorEntry(wxMOD_CMD, WXK_F6, wxID_FILE6),
    wxAcceleratorEntry(wxMOD_CMD, WXK_F7, wxID_FILE7),
    wxAcceleratorEntry(wxMOD_CMD, WXK_F8, wxID_FILE8),
    wxAcceleratorEntry(wxMOD_CMD, WXK_F9, wxID_FILE9),
    wxAcceleratorEntry(wxMOD_CMD, WXK_F10, wxID_FILE10),
    wxAcceleratorEntry(wxMOD_CMD, wxT('0'), XRCID("VideoLayersReset")),
    wxAcceleratorEntry(wxMOD_CMD, wxT('G'), XRCID("ChangeFilter")),
    wxAcceleratorEntry(wxMOD_NONE, WXK_NUMPAD_ADD, XRCID("IncreaseVolume")),
    wxAcceleratorEntry(wxMOD_NONE, WXK_NUMPAD_SUBTRACT, XRCID("DecreaseVolume")),
    wxAcceleratorEntry(wxMOD_NONE, WXK_NUMPAD_ENTER, XRCID("ToggleSound"))
};
const int num_def_accels = sizeof(default_accels) / sizeof(default_accels[0]);

// Note: this must match GUI widget names or GUI won't work
// This table's order determines tab order as well
const wxString joynames[NUM_KEYS] = {
    wxT("Up"), wxT("Down"), wxT("Left"), wxT("Right"),
    wxT("A"), wxT("B"), wxT("L"), wxT("R"),
    wxT("Select"), wxT("Start"),
    wxT("MotionUp"), wxT("MotionDown"), wxT("MotionLeft"), wxT("MotionRight"),
    wxT("MotionIn"), wxT("MotionOut"), wxT("AutoA"), wxT("AutoB"),
    wxT("Speed"), wxT("Capture"), wxT("GS")
};

wxJoyKeyBinding defkeys[NUM_KEYS * 2] = {
    { WXK_UP }, { 1, WXJB_AXIS_MINUS, 1 }, { WXK_DOWN }, { 1, WXJB_AXIS_PLUS, 1 },
    { WXK_LEFT }, { 0, WXJB_AXIS_MINUS, 1 }, { WXK_RIGHT }, { 0, WXJB_AXIS_PLUS, 1 },
    { wxT('X') }, { 0, WXJB_BUTTON, 1 }, { wxT('Z') }, { 1, WXJB_BUTTON, 1 },
    { wxT('A') }, { 2, WXJB_BUTTON, 1 }, { wxT('S') }, { 3, WXJB_BUTTON, 1 },
    { WXK_BACK }, { 4, WXJB_BUTTON, 1 }, { WXK_RETURN }, { 5, WXJB_BUTTON, 1 },
    { WXK_NUMPAD_UP }, { 0 }, { WXK_NUMPAD_DOWN }, { 0 },
    { WXK_NUMPAD_LEFT }, { 0 }, { WXK_NUMPAD_RIGHT }, { 0 },
    { WXK_NUMPAD_PAGEUP }, { 0 }, { WXK_NUMPAD_PAGEDOWN }, { 0 },
    { wxT('W') }, { 0 }, { wxT('Q') }, { 0 },
    { WXK_SPACE }, { 0 }, { 0 }, { 0 },
    { 0 }, { 0 }
};

wxAcceleratorEntry_v sys_accels;

// Note: this table must be sorted in option name order
// Both for better user display and for (fast) searching by name
opt_desc opts[] = {
    /// Display
    BOOLOPT("Display/Bilinear", "Bilinear", wxTRANSLATE("Use bilinear filter with 3d renderer"), gopts.bilinear),
    ENUMOPT("Display/Filter", "", wxTRANSLATE("Full-screen filter to apply"), gopts.filter,
        wxTRANSLATE("none|2xsai|super2xsai|supereagle|pixelate|advmame|"
                    L"bilinear|bilinearplus|scanlines|tvmode|hq2x|lq2x|"
                    L"simple2x|simple3x|hq3x|simple4x|hq4x|xbrz2x|xbrz3x|xbrz4x|xbrz5x|xbrz6x|plugin")),
    STROPT("Display/FilterPlugin", "", wxTRANSLATE("Filter plugin library"), gopts.filter_plugin),
    ENUMOPT("Display/IFB", "", wxTRANSLATE("Interframe blending function"), gopts.ifb, wxTRANSLATE("none|smart|motionblur")),
    BOOLOPT("Display/KeepOnTop", "KeepOnTop", wxTRANSLATE("Keep window on top"), gopts.keep_on_top),
    INTOPT("Display/MaxThreads", "Multithread", wxTRANSLATE("Maximum number of threads to run filters in"), gopts.max_threads, 1, 256),
#ifdef __WXMSW__
    ENUMOPT("Display/RenderMethod", "", wxTRANSLATE("Render method; if unsupported, simple method will be used"), gopts.render_method, wxTRANSLATE("simple|opengl|direct3d")),
#elif defined(__WXMAC__)
    ENUMOPT("Display/RenderMethod", "", wxTRANSLATE("Render method; if unsupported, simple method will be used"), gopts.render_method, wxTRANSLATE("simple|opengl|quartz2d")),
#else
    ENUMOPT("Display/RenderMethod", "", wxTRANSLATE("Render method; if unsupported, simple method will be used"), gopts.render_method, wxTRANSLATE("simple|opengl")),
#endif
    DOUBLEOPT("Display/Scale", "", wxTRANSLATE("Default scale factor"), gopts.video_scale, 1, 6),
    BOOLOPT("Display/Stretch", "RetainAspect", wxTRANSLATE("Retain aspect ratio when resizing"), gopts.retain_aspect),

    /// GB
    STROPT("GB/BiosFile", "", wxTRANSLATE("BIOS file to use for GB, if enabled"), gopts.gb_bios),
    STROPT("GB/GBCBiosFile", "", wxTRANSLATE("BIOS file to use for GBC, if enabled"), gopts.gbc_bios),
    { wxT("GB/Palette0"), "", wxTRANSLATE("The default palette, as 8 comma-separated 4-digit hex integers (rgb555).") },
    { wxT("GB/Palette1"), "", wxTRANSLATE("The first user palette, as 8 comma-separated 4-digit hex integers (rgb555).") },
    { wxT("GB/Palette2"), "", wxTRANSLATE("The second user palette, as 8 comma-separated 4-digit hex integers (rgb555).") },
    BOOLOPT("GB/PrintAutoPage", "PrintGather", wxTRANSLATE("Automatically gather a full page before printing"), gopts.print_auto_page),
    BOOLOPT("GB/PrintScreenCap", "PrintSnap", wxTRANSLATE("Automatically save printouts as screen captures with -print suffix"), gopts.print_screen_cap),
    STROPT("GB/ROMDir", "", wxTRANSLATE("Directory to look for ROM files"), gopts.gb_rom_dir),
    STROPT("GB/GBCROMDir", "", wxTRANSLATE("Directory to look for GBC ROM files"), gopts.gbc_rom_dir),

    /// GBA
    STROPT("GBA/BiosFile", "", wxTRANSLATE("BIOS file to use, if enabled"), gopts.gba_bios),
#ifndef NO_LINK
    BOOLOPT("GBA/LinkAuto", "LinkAuto", wxTRANSLATE("Enable link at boot"), gopts.link_auto),
    INTOPT("GBA/LinkFast", "SpeedOn", wxTRANSLATE("Enable faster network protocol by default"), linkHacks, 0, 1),
    STROPT("GBA/LinkHost", "", wxTRANSLATE("Default network link client host"), gopts.link_host),
    INTOPT("GBA/LinkProto", "LinkProto", wxTRANSLATE("Default network protocol"), gopts.link_proto, 0, 1),
    INTOPT("GBA/LinkTimeout", "LinkTimeout", wxTRANSLATE("Link timeout (ms)"), linkTimeout, 0, 9999999),
    INTOPT("GBA/LinkType", "LinkType", wxTRANSLATE("Link cable type"), gopts.gba_link_type, 0, 5),
#endif
    STROPT("GBA/ROMDir", "", wxTRANSLATE("Directory to look for ROM files"), gopts.gba_rom_dir),

    /// General
    BOOLOPT("General/AutoLoadLastState", "", wxTRANSLATE("Automatically load last saved state"), gopts.autoload_state),
    STROPT("General/BatteryDir", "", wxTRANSLATE("Directory to store game save files (relative paths are relative to ROM; blank is config dir)"), gopts.battery_dir),
    BOOLOPT("General/FreezeRecent", "", wxTRANSLATE("Freeze recent load list"), gopts.recent_freeze),
    ENUMOPT("General/OnlineUpdates", "", wxTRANSLATE("Automatically check for online updates"), gopts.onlineupdates, wxTRANSLATE("never|daily|weekly")),
    STROPT("General/RecordingDir", "", wxTRANSLATE("Directory to store A/V and game recordings (relative paths are relative to ROM)"), gopts.recording_dir),
    INTOPT("General/RewindInterval", "", wxTRANSLATE("Number of seconds between rewind snapshots (0 to disable)"), gopts.rewind_interval, 0, 600),
    STROPT("General/ScreenshotDir", "", wxTRANSLATE("Directory to store screenshots (relative paths are relative to ROM)"), gopts.scrshot_dir),
    STROPT("General/StateDir", "", wxTRANSLATE("Directory to store saved state files (relative paths are relative to BatteryDir)"), gopts.state_dir),
    INTOPT("General/StatusBar", "StatusBar", wxTRANSLATE("Enable status bar"), gopts.statusbar, 0, 1),

    /// Joypad
    { wxT("Joypad/*/*"), "", wxTRANSLATE("The parameter Joypad/<n>/<button> contains a comma-separated list of key names which map to joypad #<n> button <button>.  Button is one of Up, Down, Left, Right, A, B, L, R, Select, Start, MotionUp, MotionDown, MotionLeft, MotionRight, AutoA, AutoB, Speed, Capture, GS") },
    INTOPT("Joypad/AutofireThrottle", "", wxTRANSLATE("The autofire toggle period, in frames (1/60 s)"), gopts.autofire_rate, 1, 1000),

    /// Keyboard
    INTOPT("Joypad/Default", "", wxTRANSLATE("The number of the stick to use in single-player mode"), gopts.default_stick, 1, 4),

    /// Keyboard
    { wxT("Keyboard/*"), "", wxTRANSLATE("The parameter Keyboard/<cmd> contains a comma-separated list of key names (e.g. Alt-Shift-F1).  When the named key is pressed, the command <cmd> is executed.") },

    // Core
    INTOPT("preferences/agbPrint", "AGBPrinter", wxTRANSLATE("Enable AGB debug print"), agbPrint, 0, 1),
    INTOPT("preferences/autoFrameSkip", "FrameSkipAuto", wxTRANSLATE("Auto skip frames."), autoFrameSkip, 0, 1),
    INTOPT("preferences/autoPatch", "ApplyPatches", wxTRANSLATE("Apply IPS/UPS/IPF patches if found"), autoPatch, 0, 1),
    INTOPT("preferences/autoSaveCheatList", "", wxTRANSLATE("Automatically save and load cheat list"), autoSaveLoadCheatList, 0, 1),
    INTOPT("preferences/borderAutomatic", "", wxTRANSLATE("Automatically enable border for Super GameBoy games"), gbBorderAutomatic, 0, 1),
    INTOPT("preferences/borderOn", "", wxTRANSLATE("Always enable border"), gbBorderOn, 0, 1),
    INTOPT("preferences/captureFormat", "", wxTRANSLATE("Screen capture file format"), captureFormat, 0, 1),
    INTOPT("preferences/cheatsEnabled", "", wxTRANSLATE("Enable cheats"), cheatsEnabled, 0, 1),
#ifdef MMX
    INTOPT("preferences/disableMMX", "MMX", wxTRANSLATE("Enable MMX"), disableMMX, 0, 1),
#endif
    INTOPT("preferences/disableStatus", "NoStatusMsg", wxTRANSLATE("Disable on-screen status messages"), disableStatusMessages, 0, 1),
    INTOPT("preferences/emulatorType", "", wxTRANSLATE("Type of system to emulate"), gbEmulatorType, 0, 5),
    INTOPT("preferences/flashSize", "", wxTRANSLATE("Flash size 0 = 64KB 1 = 128KB"), optFlashSize, 0, 1),
    INTOPT("preferences/frameSkip", "FrameSkip", wxTRANSLATE("Skip frames.  Values are 0-9 or -1 to skip automatically based on time."), frameSkip, -1, 9),
    INTOPT("preferences/fsColorDepth", "", wxTRANSLATE("Fullscreen mode color depth (0 = any)"), fsColorDepth, 0, 999),
    INTOPT("preferences/fsFrequency", "", wxTRANSLATE("Fullscreen mode frequency (0 = any)"), fsFrequency, 0, 999),
    INTOPT("preferences/fsHeight", "", wxTRANSLATE("Fullscreen mode height (0 = desktop)"), fsHeight, 0, 99999),
    INTOPT("preferences/fsWidth", "", wxTRANSLATE("Fullscreen mode width (0 = desktop)"), fsWidth, 0, 99999),
    INTOPT("preferences/gbPaletteOption", "", wxTRANSLATE("The palette to use"), gbPaletteOption, 0, 2),
    INTOPT("preferences/gbPrinter", "Printer", wxTRANSLATE("Enable printer emulation"), winGbPrinterEnabled, 0, 1),
    INTOPT("preferences/gdbBreakOnLoad", "DebugGDBBreakOnLoad", wxTRANSLATE("Break into GDB after loading the game."), gdbBreakOnLoad, 0, 1),
    INTOPT("preferences/gdbPort", "DebugGDBPort", wxTRANSLATE("Port to connect GDB to."), gdbPort, 0, 65535),
#ifndef NO_LINK
    INTOPT("preferences/LinkNumPlayers", "", wxTRANSLATE("Number of players in network"), linkNumPlayers, 2, 4),
#endif
    INTOPT("preferences/maxScale", "", wxTRANSLATE("Maximum scale factor (0 = no limit)"), maxScale, 0, 100),
    INTOPT("preferences/pauseWhenInactive", "PauseWhenInactive", wxTRANSLATE("Pause game when main window loses focus"), pauseWhenInactive, 0, 1),
    INTOPT("preferences/rtcEnabled", "RTC", wxTRANSLATE("Enable RTC (vba-over.ini override is rtcEnabled"), rtcEnabled, 0, 1),
    INTOPT("preferences/saveType", "", wxTRANSLATE("Native save (\"battery\") hardware type"), cpuSaveType, 0, 5),
    INTOPT("preferences/showSpeed", "", wxTRANSLATE("Show speed indicator"), showSpeed, 0, 2),
    INTOPT("preferences/showSpeedTransparent", "Transparent", wxTRANSLATE("Draw on-screen messages transparently"), showSpeedTransparent, 0, 1),
    INTOPT("preferences/skipBios", "SkipIntro", wxTRANSLATE("Skip BIOS initialization"), skipBios, 0, 1),
    INTOPT("preferences/skipSaveGameCheats", "", wxTRANSLATE("Do not overwrite cheat list when loading state"), skipSaveGameCheats, 0, 1),
    INTOPT("preferences/skipSaveGameBattery", "", wxTRANSLATE("Do not overwrite native (battery) save when loading state"), skipSaveGameBattery, 0, 1),
    UINTOPT("preferences/throttle", "", wxTRANSLATE("Throttle game speed, even when accelerated (0-500%, 0 = no throttle)"), throttle, 0, 600),
    UINTOPT("preferences/speedupThrottle", "", wxTRANSLATE("Set throttle for speedup key (0-600%, 0 = no throttle)"), speedup_throttle, 0, 600),
    UINTOPT("preferences/speedupFrameSkip", "", wxTRANSLATE("Set frame skip for speedup key (0-30)"), speedup_frame_skip, 0, 30),
    INTOPT("preferences/useBiosGB", "BootRomGB", wxTRANSLATE("Use the specified BIOS file for GB"), useBiosFileGB, 0, 1),
    INTOPT("preferences/useBiosGBA", "BootRomEn", wxTRANSLATE("Use the specified BIOS file"), useBiosFileGBA, 0, 1),
    INTOPT("preferences/useBiosGBC", "BootRomGBC", wxTRANSLATE("Use the specified BIOS file for GBC"), useBiosFileGBC, 0, 1),
    INTOPT("preferences/vsync", "VSync", wxTRANSLATE("Wait for vertical sync"), vsync, 0, 1),

    /// Geometry
    INTOPT("geometry/fullScreen", "Fullscreen", wxTRANSLATE("Enter fullscreen mode at startup"), fullScreen, 0, 1),
    UINTOPT("geometry/windowHeight", "Height", wxTRANSLATE("Window height at startup"), windowHeight, 0, 99999),
    UINTOPT("geometry/windowWidth", "Width", wxTRANSLATE("Window width at startup"), windowWidth, 0, 99999),
    INTOPT("geometry/windowX", "X", wxTRANSLATE("Window axis X position at startup"), windowPositionX, -1, 99999),
    INTOPT("geometry/windowY", "Y", wxTRANSLATE("Window axis Y position at startup"), windowPositionY, -1, 99999),

    /// Sound
    ENUMOPT("Sound/AudioAPI", "", wxTRANSLATE("Sound API; if unsupported, default API will be used"), gopts.audio_api, wxTRANSLATE("sdl|openal|directsound|xaudio2|faudio")),
    STROPT("Sound/AudioDevice", "", wxTRANSLATE("Device ID of chosen audio device for chosen driver"), gopts.audio_dev),
    INTOPT("Sound/Buffers", "", wxTRANSLATE("Number of sound buffers"), gopts.audio_buffers, 2, 10),
    INTOPT("Sound/Enable", "", wxTRANSLATE("Bit mask of sound channels to enable"), gopts.sound_en, 0, 0x30f),
    INTOPT("Sound/GBAFiltering", "", wxTRANSLATE("GBA sound filtering (%)"), gopts.gba_sound_filter, 0, 100),
    BOOLOPT("Sound/GBAInterpolation", "GBASoundInterpolation", wxTRANSLATE("GBA sound interpolation"), soundInterpolation),
    BOOLOPT("Sound/GBDeclicking", "GBDeclicking", wxTRANSLATE("GB sound declicking"), gopts.gb_declick),
    INTOPT("Sound/GBEcho", "", wxTRANSLATE("GB echo effect (%)"), gopts.gb_echo, 0, 100),
    BOOLOPT("Sound/GBEnableEffects", "GBEnhanceSound", wxTRANSLATE("Enable GB sound effects"), gopts.gb_effects_config_enabled),
    INTOPT("Sound/GBStereo", "", wxTRANSLATE("GB stereo effect (%)"), gopts.gb_stereo, 0, 100),
    BOOLOPT("Sound/GBSurround", "GBSurround", wxTRANSLATE("GB surround sound effect (%)"), gopts.gb_effects_config_surround),
    ENUMOPT("Sound/Quality", "", wxTRANSLATE("Sound sample rate (kHz)"), gopts.sound_qual, wxTRANSLATE("48|44|22|11")),
    INTOPT("Sound/Volume", "", wxTRANSLATE("Sound volume (%)"), gopts.sound_vol, 0, 400)
};
const int num_opts = sizeof(opts) / sizeof(opts[0]);

// This constructor only works with globally allocated gopts.  It relies on
// the default value of every non-object to be 0.
opts_t::opts_t()
{
    frameSkip = -1;
    audio_api = AUD_SDL;
#ifndef NO_OGL
    render_method = RND_OPENGL;
#endif

    video_scale = 3;
    retain_aspect = true;
    max_threads = wxThread::GetCPUCount();

    // handle erroneous thread count values appropriately
    if (max_threads > 256)
        max_threads = 256;

    if (max_threads < 1)
        max_threads = 1;

    // 10 fixes stuttering on mac with openal, as opposed to 5
    // also should be better for modern hardware in general
    audio_buffers = 10;

    sound_en = 0x30f;
    sound_vol = 100;
    sound_qual = 1;
    gb_echo = 20;
    gb_stereo = 15;
    gb_declick = true;
    gba_sound_filter = 50;
    bilinear = true;
    default_stick = 1;

    for (int i = 0; i < NUM_KEYS; i++) {
        if (defkeys[i * 2].key)
            joykey_bindings[0][i].push_back(defkeys[i * 2]);

        if (defkeys[i * 2 + 1].joy)
            joykey_bindings[0][i].push_back(defkeys[i * 2 + 1]);
    }

    recent = new wxFileHistory(10);
    autofire_rate = 1;
    print_auto_page = true;
    autoPatch = true;
    onlineupdates = 1;
}

// for binary_search() and friends
bool opt_lt(const opt_desc& opt1, const opt_desc& opt2)
{
    return wxStrcmp(opt1.opt, opt2.opt) < 0;
}

// FIXME: simulate MakeInstanceFilename(vbam.ini) using subkeys (Slave%d/*)

void load_opts()
{
    // just for sanity...
    bool did_init = false;

    if (did_init)
        return;

    did_init = true;

    // Translations can't be initialized in static structures (before locale
    // is initialized), so do them now
    for (int i = 0; i < num_opts; i++)
        opts[i].desc = wxGetTranslation(opts[i].desc);

    // enumvals should not be translated, since they would cause config file
    // change after lang change
    // instead, translate when presented to user
    wxFileConfig* cfg = wxGetApp().cfg;
    cfg->SetPath(wxT("/"));
    // enure there are no unknown options present
    // note that items cannot be deleted until after loop or loop will fail
    wxArrayString item_del, grp_del;
    long grp_idx;
    wxString s;
    bool cont;

    for (cont = cfg->GetFirstEntry(s, grp_idx); cont;
         cont = cfg->GetNextEntry(s, grp_idx)) {
        //wxLogWarning(_("Invalid option %s present; removing if possible"), s.mb_str());
        item_del.push_back(s);
    }

    // Date of last online update check;
    gopts.last_update = cfg->Read(wxT("General/LastUpdated"), (long)0);
    cfg->Read(wxT("General/LastUpdatedFileName"), &gopts.last_updated_filename);
    std::sort(&opts[0], &opts[num_opts], opt_lt);

    for (cont = cfg->GetFirstGroup(s, grp_idx); cont;
         cont = cfg->GetNextGroup(s, grp_idx)) {
        // ignore wxWidgets-managed global library settings
        if (s == wxT("wxWindows"))
            continue;

        // ignore file history
        if (s == wxT("Recent"))
            continue;

        cfg->SetPath(s);
        int poff = s.size();
        long entry_idx;
        wxString e;

        for (cont = cfg->GetFirstGroup(e, entry_idx); cont;
             cont = cfg->GetNextGroup(e, entry_idx)) {
            // the only one with subgroups
            if (s == wxT("Joypad") && e.size() == 1 && e[0] >= wxT('1') && e[0] <= wxT('4')) {
                s.append(wxT('/'));
                s.append(e);
                s.append(wxT('/'));
                int poff2 = s.size();
                cfg->SetPath(e);
                long key_idx;

                for (cont = cfg->GetFirstGroup(e, key_idx); cont;
                     cont = cfg->GetNextGroup(e, key_idx)) {
                    s.append(e);
                    //wxLogWarning(_("Invalid option group %s present; removing if possible"), s.mb_str());
                    grp_del.push_back(s);
                    s.resize(poff2);
                }

                for (cont = cfg->GetFirstEntry(e, key_idx); cont;
                     cont = cfg->GetNextEntry(e, key_idx)) {
                    int i;

                    for (i = 0; i < NUM_KEYS; i++)
                        if (e == joynames[i])
                            break;

                    if (i == NUM_KEYS) {
                        s.append(e);
                        //wxLogWarning(_("Invalid option %s present; removing if possible"), s.mb_str());
                        item_del.push_back(s);
                        s.resize(poff2);
                    }
                }

                s.resize(poff);
                cfg->SetPath(wxT("/"));
                cfg->SetPath(s);
            } else {
                s.append(wxT('/'));
                s.append(e);
                //wxLogWarning(_("Invalid option group %s present; removing if possible"), s.mb_str());
                grp_del.push_back(s);
                s.resize(poff);
            }
        }

        for (cont = cfg->GetFirstEntry(e, entry_idx); cont;
             cont = cfg->GetNextEntry(e, entry_idx)) {
            // kb options come from a different list
            if (s == wxT("Keyboard")) {
                const cmditem dummy = { e };

                if (!std::binary_search(&cmdtab[0], &cmdtab[ncmds], dummy, cmditem_lt)) {
                    s.append(wxT('/'));
                    s.append(e);
                    //wxLogWarning(_("Invalid option %s present; removing if possible"), s.mb_str());
                    item_del.push_back(s);
                    s.resize(poff);
                }
            } else {
                s.append(wxT('/'));
                s.append(e);
                const opt_desc dummy = { s };
                wxString opt_name(dummy.opt);

                if (!std::binary_search(&opts[0], &opts[num_opts], dummy, opt_lt) && opt_name != wxT("General/LastUpdated") && opt_name != wxT("General/LastUpdatedFileName")) {
                    //wxLogWarning(_("Invalid option %s present; removing if possible"), s.mb_str());
                    item_del.push_back(s);
                }

                s.resize(poff);
            }
        }

        cfg->SetPath(wxT("/"));
    }

    for (int i = 0; i < item_del.size(); i++)
        cfg->DeleteEntry(item_del[i]);

    for (int i = 0; i < grp_del.size(); i++)
        cfg->DeleteGroup(grp_del[i]);

    // now read actual values and set to default if unset
    // config file will be updated with unset options
    cfg->SetRecordDefaults();

    for (int i = 0; i < num_opts; i++) {
        opt_desc& opt = opts[i];

        if (opt.stropt) {
            //Fix provided by nhdailey
            cfg->Read(opt.opt, opt.stropt, *opt.stropt);
            opt.curstr = *opt.stropt;
        } else if (!opt.enumvals.empty()) {
            auto enum_opts = str_split(opt.enumvals.MakeLower(), wxT("|"));
            opt.curint     = *opt.intopt;
            bool gotit     = cfg->Read(opt.opt, &s); s.MakeLower();

            if (gotit && !s.empty()) {
                const auto found_pos = vec_find(enum_opts, s);
                const bool matched   = found_pos != wxNOT_FOUND;

                if (!matched) {
                    opt.curint = 0;
                    const wxString ev  = opt.enumvals;
                    const wxString evx = wxGetTranslation(ev);
                    bool isx = wxStrcmp(ev, evx) != 0;
                    // technically, the translation for this string could incorproate
                    // the equals sign if necessary instead of doing it this way
                    wxLogWarning(_("Invalid value %s for option %s; valid values are %s%s%s"),
                        s, opt.opt, ev,
                        isx ? wxT(" = ") : wxT(""),
                        isx ? evx : wxT(""));
                    // write first option
                    cfg->Write(opt.opt, enum_opts[0]);
                } else
                    opt.curint = found_pos;

                *opt.intopt = opt.curint;
            } else {
                cfg->Write(opt.opt, enum_opts[opt.curint]);
            }
        } else if (opt.intopt) {
            cfg->Read(opt.opt, &opt.curint, *opt.intopt);

            if (opt.curint < opt.min || opt.curint > opt.max) {
                wxLogWarning(_("Invalid value %d for option %s; valid values are %d - %d"), opt.curint, opt.opt, opt.min, opt.max);
            } else
                *opt.intopt = opt.curint;
        } else if (opt.doubleopt) {
            cfg->Read(opt.opt, &opt.curdouble, *opt.doubleopt);

            if (opt.curdouble < opt.min || opt.curdouble > opt.max) {
                wxLogWarning(_("Invalid value %f for option %s; valid values are %f - %f"), opt.curdouble, opt.opt, opt.min, opt.max);
            } else
                *opt.doubleopt = opt.curdouble;
        } else if (opt.uintopt) {
            int val;
            cfg->Read(opt.opt, &val, *opt.uintopt);
            opt.curuint = val;

            if (opt.curuint < opt.min || opt.curuint > opt.max) {
                wxLogWarning(_("Invalid value %f for option %s; valid values are %f - %f"), opt.curuint, opt.opt, opt.min, opt.max);
            } else
                *opt.uintopt = opt.curuint;
        } else if (opt.boolopt) {
            cfg->Read(opt.opt, opt.boolopt, *opt.boolopt);
            opt.curbool = *opt.boolopt;
        }
    }

    // GB/Palette[0-2] is special
    for (int i = 0; i < 3; i++) {
        wxString optn;
        optn.Printf(wxT("GB/Palette%d"), i);
        wxString val;
        const opt_desc dummy = { optn };
        opt_desc* opt = std::lower_bound(&opts[0], &opts[num_opts], dummy, opt_lt);
        wxString entry;

        for (int j = 0; j < 8; j++) {
            // stupid wxString.Printf doesn't support printing at offset
            entry.Printf(wxT("%04X,"), (int)systemGbPalette[i * 8 + j]);
            val.append(entry);
        }

        val.resize(val.size() - 1);
        cfg->Read(optn, &val, val);
        opt->curstr = val;

        for (int j = 0, cpos = 0; j < 8; j++) {
            int start = cpos;
            cpos = val.find(wxT(','), cpos);

            if (cpos == wxString::npos)
                cpos = val.size();

            long ival;
            // ignoring errors; if the value is bad, palette will be corrupt
            // -- tough.
            // stupid wxString.ToLong doesn't support start @ offset
            entry = val.substr(start, cpos - start);
            entry.ToLong(&ival, 16);
            systemGbPalette[i * 8 + j] = ival;

            if (cpos != val.size())
                cpos++;
        }
    }

    // joypad is special
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < NUM_KEYS; j++) {
            wxString optname;
            optname.Printf(wxT("Joypad/%d/%s"), i + 1, joynames[j]);
            bool gotit = cfg->Read(optname, &s);

            if (gotit) {
                gopts.joykey_bindings[i][j] = wxJoyKeyTextCtrl::FromString(s);

                if (s.size() && !gopts.joykey_bindings[i][j].size())
                    wxLogWarning(_("Invalid key binding %s for %s"), s.mb_str(), optname.mb_str());
            } else {
                s = wxJoyKeyTextCtrl::ToString(gopts.joykey_bindings[i][j]);
                cfg->Write(optname, s);
            }
        }
    }

    // keyboard is special
    // Keyboard does not get written with defaults
    wxString kbopt(wxT("Keyboard/"));
    int kboff = kbopt.size();

    for (int i = 0; i < ncmds; i++) {
        kbopt.resize(kboff);
        kbopt.append(cmdtab[i].cmd);

        if (cfg->Read(kbopt, &s) && s.size()) {
            wxAcceleratorEntry_v val = wxKeyTextCtrl::FromString(s);

            if (!val.size())
                wxLogWarning(_("Invalid key binding %s for %s"), s.mb_str(), kbopt.mb_str());
            else {
                for (int j = 0; j < val.size(); j++)
                    val[j].Set(val[j].GetFlags(), val[j].GetKeyCode(),
                        cmdtab[i].cmd_id);

                gopts.accels.insert(gopts.accels.end(),
                    val.begin(), val.end());
            }
        }
    }

    // recent is special
    // Recent does not get written with defaults
    cfg->SetPath(wxT("/Recent"));
    gopts.recent->Load(*cfg);
    cfg->SetPath(wxT("/"));
    cfg->Flush();
}

// Note: run load_opts() first to guarantee all config opts exist
void update_opts()
{
    wxFileConfig* cfg = wxGetApp().cfg;

    for (int i = 0; i < num_opts; i++) {
        opt_desc& opt = opts[i];

        if (opt.stropt) {
            if (opt.curstr != *opt.stropt) {
                opt.curstr = *opt.stropt;
                cfg->Write(opt.opt, opt.curstr);
            }
        } else if (!opt.enumvals.empty()) {
            if (*opt.intopt != opt.curint) {
                opt.curint = *opt.intopt;
                auto enum_opts = str_split(opt.enumvals.MakeLower(), wxT("|"));

                cfg->Write(opt.opt, enum_opts[opt.curint]);
            }
        } else if (opt.intopt) {
            if (*opt.intopt != opt.curint)
                cfg->Write(opt.opt, (opt.curint = *opt.intopt));
        } else if (opt.doubleopt) {
            if (*opt.doubleopt != opt.curdouble)
                cfg->Write(opt.opt, (opt.curdouble = *opt.doubleopt));
        } else if (opt.uintopt) {
            if (*opt.uintopt != opt.curuint)
                cfg->Write(opt.opt, (opt.curuint = *opt.uintopt));
        } else if (opt.boolopt) {
            if (*opt.boolopt != opt.curbool)
                cfg->Write(opt.opt, (opt.curbool = *opt.boolopt));
        }
    }

    // gbpalette requires doing the conversion to string over
    // it may trigger a write even with no changes if # of digits changes
    for (int i = 0; i < 3; i++) {
        wxString optn;
        optn.Printf(wxT("GB/Palette%d"), i);
        const opt_desc dummy = { optn };
        opt_desc* opt = std::lower_bound(&opts[0], &opts[num_opts], dummy, opt_lt);
        wxString val;
        wxString entry;

        for (int j = 0; j < 8; j++) {
            // stupid wxString.Printf doesn't support printing at offset
            entry.Printf(wxT("%04X,"), (int)systemGbPalette[i * 8 + j]);
            val.append(entry);
        }

        val.resize(val.size() - 1);

        if (val != opt->curstr) {
            opt->curstr = val;
            cfg->Write(optn, val);
        }
    }

    // for joypad, use ToString comparisons.  It may trigger changes
    // even when there are none (e.g. multi-binding ordering changes)
    // not worth worrying about
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < NUM_KEYS; j++) {
            wxString s, o;
            wxString optname;
            optname.Printf(wxT("Joypad/%d/%s"), i + 1, joynames[j]);
            s = wxJoyKeyTextCtrl::ToString(gopts.joykey_bindings[i][j]);
            cfg->Read(optname, &o);

            if (o != s)
                cfg->Write(optname, s);
        }
    }

    // for keyboard, first remove any commands that aren't bound at all
    if (cfg->HasGroup(wxT("/Keyboard"))) {
        cfg->SetPath(wxT("/Keyboard"));
        wxString s;
        long entry_idx;
        wxArrayString item_del;

        for (bool cont = cfg->GetFirstEntry(s, entry_idx); cont;
             cont = cfg->GetNextEntry(s, entry_idx)) {
            const cmditem dummy = { s };
            cmditem* cmd = std::lower_bound(&cmdtab[0], &cmdtab[ncmds], dummy, cmditem_lt);
            int i;

            for (i = 0; i < gopts.accels.size(); i++)
                if (gopts.accels[i].GetCommand() == cmd->cmd_id)
                    break;

            if (i == gopts.accels.size())
                item_del.push_back(s);
        }

        for (int i = 0; i < item_del.size(); i++)
            cfg->DeleteEntry(item_del[i]);
    }

    // then, add/update the commands that are bound
    // even if only ordering changed, a write will be triggered.
    // nothing to worry about...
    if (gopts.accels.size())
        cfg->SetPath(wxT("/Keyboard"));

    for (wxAcceleratorEntry_v::iterator i = gopts.accels.begin();
         i < gopts.accels.end(); ++i) {
        int cmd_id = i->GetCommand();
        int cmd;

        for (cmd = 0; cmd < ncmds; cmd++)
            if (cmdtab[cmd].cmd_id == cmd_id)
                break;

        wxAcceleratorEntry_v::iterator j;

        for (j = i + 1; j < gopts.accels.end(); ++j)
            if (j->GetCommand() != cmd_id)
                break;

        wxAcceleratorEntry_v nv(i, j);
        wxString nvs = wxKeyTextCtrl::ToString(nv);

        if (nvs != cfg->Read(cmdtab[cmd].cmd))
            cfg->Write(cmdtab[cmd].cmd, nvs);
    }

    cfg->SetPath(wxT("/"));
    // recent items are updated separately
    cfg->Flush();
}

bool opt_set(const wxString& name, const wxString& val)
{
    const opt_desc dummy = { name };
    const opt_desc* opt = std::lower_bound(&opts[0], &opts[num_opts], dummy, opt_lt);

    if (!wxStrcmp(name, opt->opt)) {
        if (opt->stropt)
            *opt->stropt = wxString(val);
        else if (opt->boolopt) {
            if (!(val == wxT('0') || val == wxT('1')))
                wxLogWarning(_("Invalid flag option %s - %s ignored"),
                    name, val);
            else
                *opt->boolopt = val == wxT('1');
        } else if (!opt->enumvals.empty()) {
            wxString s     = val; s.MakeLower();
            wxString ev    = opt->enumvals; ev.MakeLower();
            auto enum_opts = str_split(ev, wxT("|"));

            const std::size_t found_pos = vec_find(enum_opts, s);
            const bool matched          = found_pos != wxNOT_FOUND;

            if (!matched) {
                const wxString evx = wxGetTranslation(opt->enumvals);
                bool isx = wxStrcmp(opt->enumvals, evx) != 0;
                // technically, the translation for this string could incorproate
                // the equals sign if necessary instead of doing it this way
                wxLogWarning(_("Invalid value %s for option %s; valid values are %s%s%s"),
                    s, opt->opt, opt->enumvals,
                    isx ? wxT(" = ") : wxT(""),
                    isx ? evx : wxT(""));
            } else {
                *opt->intopt = found_pos;
            }
        } else if (opt->intopt) {
            const wxString s(val);
            long ival;

            if (!s.ToLong(&ival) || ival < opt->min || ival > opt->max)
                wxLogWarning(_("Invalid value %d for option %s; valid values are %d - %d"), ival, name, opt->min, opt->max);
            else
                *opt->intopt = ival;
        } else if (opt->doubleopt) {
            const wxString s(val);
            double dval;

            if (!s.ToDouble(&dval) || dval < opt->min || dval > opt->max)
                wxLogWarning(_("Invalid value %f for option %s; valid values are %f - %f"), dval, name, opt->min, opt->max);
            else
                *opt->doubleopt = dval;
        } else if (opt->uintopt) {
            const wxString s(val);
            unsigned long uival;

            if (!s.ToULong(&uival) || uival < opt->min || uival > opt->max)
                wxLogWarning(_("Invalid value %f for option %s; valid values are %f - %f"), uival, name, opt->min, opt->max);
            else
                *opt->uintopt = (uint32_t)uival;
        } else {
            // GB/Palette[0-2] is virtual
            for (int i = 0; i < 3; i++) {
                wxString optn;
                optn.Printf(wxT("GB/Palette%d"), i);

                if (optn != name)
                    continue;

                wxString vals(val);

                for (int j = 0, cpos = 0; j < 8; j++) {
                    int start = cpos;
                    cpos = vals.find(wxT(','), cpos);

                    if (cpos == wxString::npos)
                        cpos = vals.size();

                    long ival;
                    // ignoring errors; if the value is bad, palette will be corrupt
                    // -- tough.
                    // stupid wxString.ToLong doesn't support start @ offset
                    wxString entry = vals.substr(start, cpos - start);
                    entry.ToLong(&ival, 16);
                    systemGbPalette[i * 8 + j] = ival;

                    if (cpos != vals.size())
                        cpos++;
                }
            }
        }

        return true;
    } else {
        if (name.Find(wxT('/')) == wxNOT_FOUND)
            return false;

        auto parts = str_split(name, wxT("/"));

        if (parts[0] != wxT("Keyboard")) {
            cmditem* cmd = std::lower_bound(&cmdtab[0], &cmdtab[ncmds], cmditem{parts[1]}, cmditem_lt);

            if (cmd == &cmdtab[ncmds] || wxStrcmp(parts[1], cmd->cmd))
                return false;

            for (auto i = gopts.accels.begin(); i < gopts.accels.end(); ++i)
                if (i->GetCommand() == cmd->cmd_id) {
                    auto j = i;

                    for (; j < gopts.accels.end(); ++j)
                        if (j->GetCommand() != cmd->cmd_id)
                            break;

                    gopts.accels.erase(i, j);

                    break;
                }

            if (!val.empty()) {
                auto aval = wxKeyTextCtrl::FromString(val);

                for (int i = 0; i < aval.size(); i++)
                    aval[i].Set(aval[i].GetFlags(), aval[i].GetKeyCode(),
                        cmd->cmd_id);

                if (!aval.size())
                    wxLogWarning(_("Invalid key binding %s for %s"), val, name);
                else
                    gopts.accels.insert(gopts.accels.end(), aval.begin(), aval.end());
            }

            return true;
        } else if (!wxStrncmp(name, wxT("Joypad"), wxStrlen(wxT("Joypad")))) {
            if (parts[1] < wxT('1') || parts[1] > wxT('4') || parts.size() < 3)
                return false;

            int jno = parts[1][0] - wxT('1');
            int kno;

            for (kno = 0; kno < NUM_KEYS; kno++)
                if (!wxStrcmp(joynames[kno], parts[2]))
                    break;

            if (kno == NUM_KEYS)
                return false;

            if (val.empty())
                gopts.joykey_bindings[jno][kno].clear();
            else {
                auto b = wxJoyKeyTextCtrl::FromString(val);

                if (!b.size())
                    wxLogWarning(_("Invalid key binding %s for %s"), val, name);
                else
                    gopts.joykey_bindings[jno][kno] = b;
            }

            return true;
        } else
            return false;
    }
}
