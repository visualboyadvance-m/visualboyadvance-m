#if defined(__APPLE__) && defined(__MACH__)
#define GL_SILENCE_DEPRECATION 1
#endif

#include "wx/wxvbam.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <thread>

#ifdef __WXGTK__
    #include <X11/Xlib.h>
    #define Status int
    #include <gdk/gdkx.h>
    #include <gdk/gdkwayland.h>
    #include <gtk/gtk.h>
    // For Wayland EGL.
    #ifdef HAVE_EGL
        #include <EGL/egl.h>
    #endif
#ifdef HAVE_XSS
    #include <X11/extensions/scrnsaver.h>
#endif
#endif

#ifdef ENABLE_SDL3
#include <SDL3/SDL.h>
#else
#include <SDL.h>
#endif

#include <wx/dcbuffer.h>
#include <wx/menu.h>

#include "components/draw_text/draw_text.h"
#include "components/filters/filters.h"
#include "components/filters_agb/filters_agb.h"
#include "components/filters_cgb/filters_cgb.h"
#include "components/filters_interframe/interframe.h"
#include "core/base/check.h"
#include "core/base/file_util.h"
#include "core/base/image_util.h"
#include "core/base/patch.h"
#include "core/base/system.h"
#include "core/base/version.h"
#include "core/gb/gb.h"
#include "core/gb/gbCheats.h"
#include "core/gb/gbGlobals.h"
#include "core/gb/gbPrinter.h"
#include "core/gb/gbSound.h"
#include "core/gba/gbaCheats.h"
#include "core/gba/gbaEeprom.h"
#include "core/gba/gbaFlash.h"
#include "core/gba/gbaGlobals.h"
#include "core/gba/gbaPrint.h"
#include "core/gba/gbaRtc.h"
#include "core/gba/gbaSound.h"
#include "wx/background-input.h"
#include "wx/config/cmdtab.h"
#include "wx/config/emulated-gamepad.h"
#include "wx/config/option-id.h"
#include "wx/config/option-proxy.h"
#include "wx/config/option.h"
#include "wx/drawing.h"
#include "wx/wayland.h"
#include "wx/widgets/render-plugin.h"
#include "wx/widgets/user-input-event.h"

#ifdef __WXMSW__
#include <windows.h>
#endif

#ifdef _MSC_VER
#define strdup _strdup
#endif

namespace {

double GetFilterScale() {
    switch (OPTION(kDispFilter)) {
        case config::Filter::kNone:
            return 1.0;
        case config::Filter::k2xsai:
        case config::Filter::kSuper2xsai:
        case config::Filter::kSupereagle:
        case config::Filter::kPixelate:
        case config::Filter::kAdvmame:
        case config::Filter::kBilinear:
        case config::Filter::kBilinearplus:
        case config::Filter::kScanlines:
        case config::Filter::kTvmode:
        case config::Filter::kSimple2x:
        case config::Filter::kLQ2x:
        case config::Filter::kHQ2x:
        case config::Filter::kXbrz2x:
            return 2.0;
        case config::Filter::kXbrz3x:
        case config::Filter::kSimple3x:
        case config::Filter::kHQ3x:
            return 3.0;
        case config::Filter::kSimple4x:
        case config::Filter::kHQ4x:
        case config::Filter::kXbrz4x:
            return 4.0;
        case config::Filter::kXbrz5x:
            return 5.0;
        case config::Filter::kXbrz6x:
            return 6.0;
        case config::Filter::kPlugin:
        case config::Filter::kLast:
            VBAM_NOTREACHED_RETURN(1.0);
    }
    VBAM_NOTREACHED_RETURN(1.0);
}

// Maximum number of filter threads, capped at 6 to avoid diminishing returns
// and potential issues with filter state sharing.
int GetMaxFilterThreads() {
    unsigned int hw_threads = std::thread::hardware_concurrency();
    if (hw_threads == 0)
        hw_threads = 1;  // Fallback if hardware_concurrency fails
    return static_cast<int>(std::min(hw_threads, 8u));
}

// Apply the currently selected 32-bit filter to the image region.
// This is the core filter dispatch function used by both threaded rendering
// and seam smoothing. Plugin filters are not supported here.
void ApplyFilter32(uint8_t* src, int instride, uint8_t* delta, uint8_t* dst,
                   int outstride, int width, int height) {
    switch (OPTION(kDispFilter)) {
        case config::Filter::k2xsai:
            _2xSaI32(src, instride, delta, dst, outstride, width, height);
            break;
        case config::Filter::kSuper2xsai:
            Super2xSaI32(src, instride, delta, dst, outstride, width, height);
            break;
        case config::Filter::kSupereagle:
            SuperEagle32(src, instride, delta, dst, outstride, width, height);
            break;
        case config::Filter::kPixelate:
            Pixelate32(src, instride, delta, dst, outstride, width, height);
            break;
        case config::Filter::kAdvmame:
            AdMame2x32(src, instride, delta, dst, outstride, width, height);
            break;
        case config::Filter::kBilinear:
            Bilinear32(src, instride, delta, dst, outstride, width, height);
            break;
        case config::Filter::kBilinearplus:
            BilinearPlus32(src, instride, delta, dst, outstride, width, height);
            break;
        case config::Filter::kScanlines:
            Scanlines32(src, instride, delta, dst, outstride, width, height);
            break;
        case config::Filter::kTvmode:
            ScanlinesTV32(src, instride, delta, dst, outstride, width, height);
            break;
        case config::Filter::kLQ2x:
            lq2x32(src, instride, delta, dst, outstride, width, height);
            break;
        case config::Filter::kSimple2x:
            Simple2x32(src, instride, delta, dst, outstride, width, height);
            break;
        case config::Filter::kSimple3x:
            Simple3x32(src, instride, delta, dst, outstride, width, height);
            break;
        case config::Filter::kSimple4x:
            Simple4x32(src, instride, delta, dst, outstride, width, height);
            break;
        case config::Filter::kHQ2x:
            hq2x32(src, instride, delta, dst, outstride, width, height);
            break;
        case config::Filter::kHQ3x:
            hq3x32_32(src, instride, delta, dst, outstride, width, height);
            break;
        case config::Filter::kHQ4x:
            hq4x32_32(src, instride, delta, dst, outstride, width, height);
            break;
        case config::Filter::kXbrz2x:
            xbrz2x32(src, instride, delta, dst, outstride, width, height);
            break;
        case config::Filter::kXbrz3x:
            xbrz3x32(src, instride, delta, dst, outstride, width, height);
            break;
        case config::Filter::kXbrz4x:
            xbrz4x32(src, instride, delta, dst, outstride, width, height);
            break;
        case config::Filter::kXbrz5x:
            xbrz5x32(src, instride, delta, dst, outstride, width, height);
            break;
        case config::Filter::kXbrz6x:
            xbrz6x32(src, instride, delta, dst, outstride, width, height);
            break;
        case config::Filter::kPlugin:
            // Plugin filters require RENDER_PLUGIN_INFO, not supported here
            break;
        case config::Filter::kNone:
        case config::Filter::kLast:
            break;
    }
}

long GetSampleRate() {
    switch (OPTION(kSoundAudioRate)) {
        case config::AudioRate::k48kHz:
            return 48000;
            break;
        case config::AudioRate::k44kHz:
            return 44100;
            break;
        case config::AudioRate::k22kHz:
            return 22050;
            break;
        case config::AudioRate::k11kHz:
            return 11025;
            break;
        case config::AudioRate::kLast:
            VBAM_NOTREACHED_RETURN(44100);
    }
    VBAM_NOTREACHED_RETURN(44100);
}

#define out_8  (systemColorDepth ==  8)
#define out_16 (systemColorDepth == 16)
#define out_24 (systemColorDepth == 24)

// Draw Unicode text on raw pixel buffer using wxWidgets
void drawTextWx(uint8_t* buffer, int pitch, int x, int y, const wxString& text,
                int buffer_width, int buffer_height, double scale, int color_depth) {
    if (text.empty()) return;

    // Determine bytes per pixel based on color depth
    int bpp = color_depth >> 3;  // 16->2, 24->3, 32->4

    // Scale font size with filter scale
    // Use 11pt base size at native resolution - sweet spot for threshold-based anti-aliasing removal
    // Larger font makes character strokes more consistent, helping threshold preserve details like 'e' bar
    int fontSize = (int)std::ceil(11 * scale);

    // Use system default font with Unicode support
    wxFont font = wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT);
    font.SetPointSize(fontSize);
    font.SetWeight(wxFONTWEIGHT_NORMAL);

    // Create memory DC to measure text and handle line wrapping
    wxBitmap tempBmp(1, 1);
    wxMemoryDC tempDc(tempBmp);
    tempDc.SetFont(font);

    // Calculate maximum width for text (leave small right margin)
    int maxWidth = buffer_width - x - (int)std::ceil(5 * scale);
    if (maxWidth <= 0) return;

    // Character-wrap the text into lines (breaks at any character)
    wxArrayString lines;
    wxString currentLine;

    for (size_t i = 0; i < text.length(); i++) {
        wxChar ch = text[i];

        // Handle newline characters
        if (ch == '\n') {
            lines.Add(currentLine);
            currentLine.clear();
            continue;
        }

        // Try adding this character to the current line
        wxString testLine = currentLine + ch;
        wxSize testSize = tempDc.GetTextExtent(testLine);

        if (testSize.GetWidth() > maxWidth && !currentLine.empty()) {
            // Current line is full, start a new line with this character
            lines.Add(currentLine);
            currentLine = ch;
        } else {
            // Add character to current line
            currentLine += ch;
        }
    }

    // Add the last line if not empty
    if (!currentLine.empty()) {
        lines.Add(currentLine);
    }

    // Measure total size needed
    int maxLineWidth = 0;
    int lineHeight = tempDc.GetCharHeight();
    // Tighter line spacing for compact display at native resolution
    int lineSpacing = lineHeight + 1;
    for (size_t i = 0; i < lines.GetCount(); i++) {
        wxSize lineSize = tempDc.GetTextExtent(lines[i]);
        if (lineSize.GetWidth() > maxLineWidth) {
            maxLineWidth = lineSize.GetWidth();
        }
    }

    int textWidth = maxLineWidth + 4;
    int textHeight = (int)(lines.GetCount() * lineSpacing) + 4;

    // Don't clamp bitmap size - let it be full size and clip during pixel copy
    if (textWidth <= 0 || textHeight <= 0) return;

    wxBitmap textBmp(textWidth, textHeight, 24);
    wxMemoryDC dc(textBmp);

    // Set up the DC for text rendering with bold red text
    dc.SetFont(font);
    dc.SetBackground(*wxBLACK_BRUSH);
    dc.Clear();
    dc.SetTextForeground(*wxRED);

    // Draw each line of text with compact spacing
    int currentY = 1;
    int drawLineSpacing = dc.GetCharHeight() + 1;
    for (size_t i = 0; i < lines.GetCount(); i++) {
        dc.DrawText(lines[i], 1, currentY);
        currentY += drawLineSpacing;
    }
    dc.SelectObject(wxNullBitmap);

    // Convert bitmap to image to access pixel data
    wxImage textImg = textBmp.ConvertToImage();
    const unsigned char* imgData = textImg.GetData();

    // Blend the text image into the buffer
    for (int ty = 0; ty < textHeight; ty++) {
        for (int tx = 0; tx < textWidth; tx++) {
            int bufX = x + tx;
            int bufY = y + ty;

            if (bufX >= buffer_width || bufY >= buffer_height) continue;

            // Get pixel from text image (always 24-bit RGB)
            int imgIdx = (ty * textWidth + tx) * 3;
            uint8_t r = imgData[imgIdx];
            (void)imgData[imgIdx + 1];  // g - unused
            (void)imgData[imgIdx + 2];  // b - unused

            // Calculate buffer position
            uint8_t* bufPtr = buffer + (bufY * pitch) + (bufX * bpp);

            // Threshold anti-aliased text to create crisp pixels
            // Use threshold of 100 (~39%) - balances crispness with capturing thin strokes (like 'e' bar)
            if (r > 100) {
                // Render as pure red for sharp, pixel-perfect text
                if (color_depth == 16) {
                    uint16_t* pixel = (uint16_t*)bufPtr;
                    *pixel = (0x1f << systemRedShift);  // Pure red in 16-bit
                } else if (color_depth == 24) {
                    bufPtr[0] = 0;
                    bufPtr[1] = 0;
                    bufPtr[2] = 255;  // Pure red in 24-bit BGR
                } else if (color_depth == 32) {
                    uint32_t* pixel = (uint32_t*)bufPtr;
                    *pixel = (0xff << systemRedShift);  // Pure red in 32-bit
                }
            }
        }
    }
}

}  // namespace

int emulating;

IMPLEMENT_DYNAMIC_CLASS(GameArea, wxPanel)

GameArea::GameArea()
    : wxPanel(),
      panel(NULL),
      emusys(NULL),
      was_paused(false),
      rewind_time(0),
      do_rewind(false),
      rewind_mem(0),
      num_rewind_states(0),
      loaded(IMAGE_UNKNOWN),
      basic_width(GBAWidth),
      basic_height(GBAHeight),
      fullscreen(false),
      paused(false),
      pointer_blanked(false),
      mouse_active_time(0),
      render_observer_({config::OptionID::kDispBilinear, config::OptionID::kDispFilter,
                        config::OptionID::kDispFilterPlugin, config::OptionID::kDispRenderMethod,
                        config::OptionID::kDispIFB, config::OptionID::kDispStretch,
                        config::OptionID::kPrefVsync, config::OptionID::kSDLRenderer,
                        config::OptionID::kBitDepth},
                       std::bind(&GameArea::ResetPanel, this)),
      scale_observer_(config::OptionID::kDispScale, std::bind(&GameArea::AdjustSize, this, true)),
      gb_border_observer_(config::OptionID::kPrefBorderOn,
                          std::bind(&GameArea::OnGBBorderChanged, this, std::placeholders::_1)),
      gb_palette_observer_({config::OptionID::kGBPalette0, config::OptionID::kGBPalette1,
                            config::OptionID::kGBPalette2, config::OptionID::kPrefGBPaletteOption},
                           std::bind(&gbResetPalette)),
      gb_declick_observer_(
          config::OptionID::kSoundGBDeclicking,
          [&](config::Option* option) { gbSoundSetDeclicking(option->GetBool()); }),
      lcd_filters_observer_({config::OptionID::kGBLCDFilter, config::OptionID::kGBADarken, config::OptionID::kGBLighten, config::OptionID::kDispColorCorrectionProfile, config::OptionID::kGBALCDFilter},
                            std::bind(&GameArea::UpdateLcdFilter, this)),
      audio_rate_observer_(config::OptionID::kSoundAudioRate,
                           std::bind(&GameArea::OnAudioRateChanged, this)),
      audio_volume_observer_(config::OptionID::kSoundVolume,
                             std::bind(&GameArea::OnVolumeChanged, this, std::placeholders::_1)),
      audio_observer_({config::OptionID::kSoundAudioAPI, config::OptionID::kSoundAudioDevice,
                       config::OptionID::kSoundBuffers, config::OptionID::kSoundDSoundHWAccel,
                       config::OptionID::kSoundUpmix},
                      [&](config::Option*) { schedule_audio_restart_ = true; }) {
    SetSizer(new wxBoxSizer(wxVERTICAL));
    // all renderers prefer 32-bit
    // well, "simple" prefers 24-bit, but that's not available for filters
    systemColorDepth = (OPTION(kBitDepth) + 1) << 3;

    hq2x_init(32);
    Init_2xSaI(32);
}

void GameArea::LoadGame(const wxString& name)
{
    rom_scene_rls = "-";
    rom_scene_rls_name = "-";
    rom_name = "";
    // fex just crashes if file does not exist and it's compressed,
    // so check first
    wxFileName fnfn(name);
    bool badfile = !fnfn.IsFileReadable();

    // if path was relative, look for it before giving up
    if (badfile && !fnfn.IsAbsolute()) {
        wxString rp = fnfn.GetPath();

        // can't really decide which dir to use, so try GBA first, then GB
        const wxString gba_rom_dir = OPTION(kGBAROMDir);
        if (!wxGetApp().GetAbsolutePath(gba_rom_dir).empty()) {
            fnfn.SetPath(wxGetApp().GetAbsolutePath(gba_rom_dir) + '/' + rp);
            badfile = !fnfn.IsFileReadable();
        }

        const wxString gb_rom_dir = OPTION(kGBROMDir);
        if (badfile && !wxGetApp().GetAbsolutePath(gb_rom_dir).empty()) {
            fnfn.SetPath(wxGetApp().GetAbsolutePath(gb_rom_dir) + '/' + rp);
            badfile = !fnfn.IsFileReadable();
        }

        const wxString gbc_rom_dir = OPTION(kGBGBCROMDir);
        if (badfile && !wxGetApp().GetAbsolutePath(gbc_rom_dir).empty()) {
            fnfn.SetPath(wxGetApp().GetAbsolutePath(gbc_rom_dir) + '/' + rp);
            badfile = !fnfn.IsFileReadable();
        }
    }

    // auto-conversion of wxCharBuffer to const char * seems broken
    // so save underlying wxCharBuffer (or create one of none is used)
    wxCharBuffer fnb(UTF8(fnfn.GetFullPath()));
    const char* fn = fnb.data();
    IMAGE_TYPE t = badfile ? IMAGE_UNKNOWN : utilFindType(fn);

    if (t == IMAGE_UNKNOWN) {
        wxString s;
        s.Printf(_("%s is not a valid ROM file"), name.mb_str());
        wxMessageDialog dlg(GetParent(), s, _("Problem loading file"), wxOK | wxICON_ERROR);
        dlg.ShowModal();
        return;
    }

    {
        wxConfigBase* cfg = wxConfigBase::Get();

        if (!OPTION(kGenFreezeRecent)) {
            gopts.recent->AddFileToHistory(name);
            wxGetApp().frame->ResetRecentAccelerators();
            cfg->SetPath("/Recent");
            gopts.recent->Save(*cfg);
            cfg->SetPath("/");
            cfg->Flush();
        }
    }

    UnloadGame();
    // strip extension from actual game file name
    // FIXME: save actual file name of archive so patches & cheats can
    // be loaded from archive
    // FIXME: if archive name does not match game name, prepend archive
    // name to uniquify game name
    loaded_game = fnfn;
    loaded_game.ClearExt();
    loaded_game.MakeAbsolute();
    // load patch, if enabled
    // note that it is difficult to load from archive due to
    // ../common/Patch.cpp depending on opening the file itself and doing
    // lots of seeking & such.  The only way would be to copy the patch
    // out to a temporary file and load it (and can't just use
    // AssignTempFileName because it needs correct extension)
    // too much trouble for now, though
    bool loadpatch = OPTION(kPrefAutoPatch);
    wxFileName pfn = loaded_game;
    int ovSaveType = 0;

    if (loadpatch) {
        // SetExt may strip something off by accident, so append to text instead
        // pfn.SetExt(wxT("ips")
        pfn.SetFullName(pfn.GetFullName() + wxT(".ips"));

        if (!pfn.IsFileReadable()) {
            pfn.SetExt(wxT("ups"));

            if (!pfn.IsFileReadable()) {
                pfn.SetExt(wxT("bps"));

                if (!pfn.IsFileReadable()) {
                    pfn.SetExt(wxT("ppf"));
                    loadpatch = pfn.IsFileReadable();
                }
            }
        }
    }

    if (t == IMAGE_GB) {
        if (!gbLoadRom(fn)) {
            wxString s;
            s.Printf(_("Unable to load Game Boy ROM %s"), name.mb_str());
            wxMessageDialog dlg(GetParent(), s, _("Problem loading file"), wxOK | wxICON_ERROR);
            dlg.ShowModal();
            return;
        }

        if (loadpatch) {
            gbApplyPatch(UTF8(pfn.GetFullPath()));
        }

        // Apply overrides.
        wxFileConfig* cfg = wxGetApp().gb_overrides_.get();
        const std::string& title = g_gbCartData.title();
        if (cfg->HasGroup(title)) {
            cfg->SetPath(title);
            coreOptions.gbPrinterEnabled = cfg->Read("gbPrinter", coreOptions.gbPrinterEnabled);
            cfg->SetPath("/");
        }

        // start sound; this must happen before CPU stuff
        gb_effects_config.enabled = OPTION(kSoundGBEnableEffects);
        gb_effects_config.surround = OPTION(kSoundGBSurround);
        gb_effects_config.echo = (float)OPTION(kSoundGBEcho) / 100.0;
        gb_effects_config.stereo = (float)OPTION(kSoundGBStereo) / 100.0;
        if (!soundInit()) {
            wxLogError(_("Could not initialize the sound driver!"));
        }
        soundSetEnable(gopts.sound_en);
        gbSoundSetSampleRate(GetSampleRate());
        // this **MUST** be called **AFTER** setting sample rate because the core calls soundInit()
        soundSetThrottle(coreOptions.throttle);
        gbGetHardwareType();


        // Disable bios loading when using colorizer hack.
        if (OPTION(kPrefUseBiosGB) && OPTION(kGBColorizerHack)) {
            wxLogError(_("Cannot use Game Boy BIOS file when Colorizer Hack is enabled, disabling Game Boy BIOS file."));
            OPTION(kPrefUseBiosGB) = false;
        }

        // Set up the core for the colorizer hack.
        setColorizerHack(OPTION(kGBColorizerHack));

        // Load BIOS for the actual system being emulated
        // gbHardware: 1=GB, 2=GBC, 4=SGB/SGB2, 8=GBA
        // Only load BIOS for GB and GBC modes
        bool use_bios = false;
        wxString bios_file;

        if (gbHardware == 2) {
            // Game Boy Color
            use_bios = OPTION(kPrefUseBiosGBC).Get();
            bios_file = OPTION(kGBGBCBiosFile).Get();
        } else if (gbHardware == 1) {
            // Game Boy
            use_bios = OPTION(kPrefUseBiosGB).Get();
            bios_file = OPTION(kGBBiosFile).Get();
        }
        // For SGB/SGB2 (4) and GBA (8), use_bios remains false (no BIOS loaded)

        gbCPUInit(bios_file.To8BitData().data(), use_bios);

        if (use_bios && !coreOptions.useBios) {
            wxLogError(_("Could not load BIOS %s"), bios_file);
            // could clear use flag & file name now, but better to force
            // user to do it
        }

        gbReset();

        if (OPTION(kPrefBorderOn)) {
            basic_width = gbBorderLineSkip = SGBWidth;
            basic_height = SGBHeight;
            gbBorderColumnSkip = (SGBWidth - GBWidth) / 2;
            gbBorderRowSkip = (SGBHeight - GBHeight) / 2;
        } else {
            basic_width = gbBorderLineSkip = GBWidth;
            basic_height = GBHeight;
            gbBorderColumnSkip = gbBorderRowSkip = 0;
        }

        emusys = &GBSystem;
    } else /* if(t == IMAGE_GBA) */
    {
        if (!(rom_size = CPULoadRom(fn))) {
            wxString s;
            s.Printf(_("Unable to load Game Boy Advance ROM %s"), name.mb_str());
            wxMessageDialog dlg(GetParent(), s, _("Problem loading file"), wxOK | wxICON_ERROR);
            dlg.ShowModal();
            return;
        }

        rom_crc32 = crc32(0L, g_rom, rom_size);

        if (loadpatch) {
            // don't use real rom size or it might try to resize rom[]
            // instead, use known size of rom[]
            int size = 0x2000000 < rom_size ? 0x2000000 : rom_size;
            applyPatch(UTF8(pfn.GetFullPath()), &g_rom, &size);
            // that means we no longer really know rom_size either <sigh>

            gbaUpdateRomSize(size);
        }

        wxFileConfig* cfg = wxGetApp().overrides_.get();
        wxString id = wxString((const char*)&g_rom[0xac], wxConvLibc, 4);

        // Check for game-specific overrides based on ROM ID
        if (cfg->HasGroup(id)) {
            cfg->SetPath(id);
            // Apply RTC override
            bool enable_rtc = cfg->Read(wxT("rtcEnabled"), coreOptions.rtcEnabled);
            rtcEnable(enable_rtc);

            // Set Flash size from config; fallback to global preference
            int fsz = cfg->Read(wxT("flashSize"), (long)0);

            if (fsz != 0x10000 && fsz != 0x20000)
                fsz = 0x10000 << OPTION(kPrefFlashSize);

            flashSetSize(fsz);
            // Set save type from override; fallback to detection if set to Auto (0)
            ovSaveType = cfg->Read(wxT("coreOptions.saveType"), coreOptions.cpuSaveType);

            if (ovSaveType < 0 || ovSaveType > 5)
                ovSaveType = 0;

            if (ovSaveType == 0) {
                flashDetectSaveType(rom_size);
            } else {
                coreOptions.saveType = ovSaveType;
            }

            // Initialize eepromMask to prevent DMA freeze on un-initialized battery
            if (coreOptions.saveType == GBA_SAVE_EEPROM) {
                eepromSetSize(SIZE_EEPROM_512);
            }

            // Apply ROM mirroring and reset path
            coreOptions.mirroringEnable = cfg->Read(wxT("mirroringEnabled"), (long)1);
            cfg->SetPath(wxT("/"));
        } else {
            // Apply global defaults for games without specific overrides
            rtcEnable(coreOptions.rtcEnabled);
            flashSetSize(0x10000 << OPTION(kPrefFlashSize));

            // Determine save type based on global preference or internal detection
            if (coreOptions.cpuSaveType < 0 || coreOptions.cpuSaveType > 5)
                coreOptions.cpuSaveType = 0;

            if (coreOptions.cpuSaveType == 0) {
                flashDetectSaveType(rom_size);
            } else {
                coreOptions.saveType = coreOptions.cpuSaveType;
            }

            // Initialize eepromMask to prevent DMA freeze on un-initialized battery
            if (coreOptions.saveType == GBA_SAVE_EEPROM) {
                eepromSetSize(SIZE_EEPROM_512);
            }

            // Disable mirroring by default
            coreOptions.mirroringEnable = false;
        }

        doMirroring(coreOptions.mirroringEnable);
        // start sound; this must happen before CPU stuff
        if (!soundInit()) {
            wxLogError(_("Could not initialize the sound driver!"));
        }
        soundSetEnable(gopts.sound_en);
        soundSetSampleRate(GetSampleRate());
        // this **MUST** be called **AFTER** setting sample rate because the core calls soundInit()
        soundSetThrottle(coreOptions.throttle);
        soundFiltering = (float)OPTION(kSoundGBAFiltering) / 100.0f;

        rtcEnableRumble(true);

        CPUInit(UTF8(gopts.gba_bios), OPTION(kPrefUseBiosGBA));

        if (OPTION(kPrefUseBiosGBA) && !coreOptions.useBios) {
            wxLogError(_("Could not load BIOS %s"), gopts.gba_bios.mb_str());
            // could clear use flag & file name now, but better to force
            // user to do it
        }

        CPUReset();
        basic_width = GBAWidth;
        basic_height = GBAHeight;
        emusys = &GBASystem;
    }

    // Set sound volume.
    soundSetVolume((float)OPTION(kSoundVolume) / 100.0);

    if (OPTION(kGeomFullScreen)) {
        GameArea::ShowFullScreen(true);
    }

    loaded = t;
    SetFrameTitle();
    SetFocus();
    // Use custom geometry
    AdjustSize(false);
    emulating = true;
    was_paused = true;
    schedule_audio_restart_ = false;
    MainFrame* mf = wxGetApp().frame;
    mf->cmd_enable &= ~(CMDEN_GB | CMDEN_GBA);
    mf->cmd_enable |= ONLOAD_CMDEN;
    mf->cmd_enable |= loaded == IMAGE_GB ? CMDEN_GB : (CMDEN_GBA | CMDEN_NGDB_GBA);
    mf->enable_menus();
#if (defined __WIN32__ || defined _WIN32)
#ifndef NO_LINK
    gbSerialFunction = gbStartLink;
#else
    gbSerialFunction = NULL;
#endif
#endif

    SuspendScreenSaver();

    // probably only need to do this for GB carts
    if (coreOptions.gbPrinterEnabled)
        gbSerialFunction = gbPrinterSend;

    // probably only need to do this for GBA carts
    agbPrintEnable(OPTION(kPrefAgbPrint));

    // set frame skip based on ROM type
    const int frame_skip = OPTION(kPrefFrameSkip);
    if (frame_skip != -1) {
        systemFrameSkip = frame_skip;
    }

    // load battery and/or saved state
    recompute_dirs();
    mf->update_state_ts(true);
    bool did_autoload = OPTION(kGenAutoLoadLastState) ? LoadState() : false;

    if (!did_autoload || coreOptions.skipSaveGameBattery) {
        wxString bname = loaded_game.GetFullName();
#ifndef NO_LINK
        // MakeInstanceFilename doesn't do wxString, so just add slave ID here
        int playerId = GetLinkPlayerId();

        if (playerId >= 0) {
            bname.append(wxT('-'));
            bname.append(wxChar(wxT('1') + playerId));
        }

#endif
        bname.append(wxT(".sav"));
        wxFileName bat(batdir, bname);

        if (emusys->emuReadBattery(UTF8(bat.GetFullPath()))) {
            wxString msg;
            msg.Printf(_("Loaded battery %s"), bat.GetFullPath().wc_str());
            systemScreenMessage(msg);

            if (coreOptions.cpuSaveType == 0 && ovSaveType == 0 && t == IMAGE_GBA) {
                switch (bat.GetSize().GetValue()) {
                case 0x200:
                case 0x2000:
                    coreOptions.saveType = GBA_SAVE_EEPROM;
                    eepromSetSize(bat.GetSize().GetValue());
                    break;

                case 0x8000:
                    coreOptions.saveType = GBA_SAVE_SRAM;
                    break;

                case 0x10000:
                    if (coreOptions.saveType == GBA_SAVE_EEPROM || coreOptions.saveType == GBA_SAVE_SRAM)
                        break;
                    break;

                case 0x20000:
                    coreOptions.saveType = GBA_SAVE_FLASH;
                    flashSetSize(bat.GetSize().GetValue());
                    break;

                default:
                    break;
                }

                SetSaveType(coreOptions.saveType);
            }
        }

        // forget old save writes
        systemSaveUpdateCounter = SYSTEM_SAVE_NOT_UPDATED;
    }

    // do an immediate rewind save
    // even if loaded from state file: not smart enough yet to just
    // do a reset or load from state file when # rewinds == 0
    do_rewind = gopts.rewind_interval > 0;
    // FIXME: backup battery file (useful if game name conflict)
    cheats_dirty = (did_autoload && !coreOptions.skipSaveGameCheats) || (loaded == IMAGE_GB ? gbCheatNumber > 0 : cheatsNumber > 0);

    if (OPTION(kPrefAutoSaveLoadCheatList) && (!did_autoload || coreOptions.skipSaveGameCheats)) {
        wxFileName cfn = loaded_game;
        // SetExt may strip something off by accident, so append to text instead
        cfn.SetFullName(cfn.GetFullName() + wxT(".clt"));

        if (cfn.IsFileReadable()) {
            bool cld;

            if (loaded == IMAGE_GB)
                cld = gbCheatsLoadCheatList(UTF8(cfn.GetFullPath()));
            else
                cld = cheatsLoadCheatList(UTF8(cfn.GetFullPath()));

            if (cld) {
                systemScreenMessage(_("Loaded cheats"));
                cheats_dirty = false;
            }
        }
    }

#ifndef NO_LINK
    if (OPTION(kGBALinkAuto)) {
        BootLink(mf->GetConfiguredLinkMode(), UTF8(gopts.link_host),
                 gopts.link_timeout, OPTION(kGBALinkFast),
                 gopts.link_num_players);
    }
#endif

#if defined(VBAM_ENABLE_DEBUGGER)
    if (OPTION(kPrefGDBBreakOnLoad)) {
        mf->GDBBreak();
    }
#endif  // defined(VBAM_ENABLE_DEBUGGER)
}

void GameArea::SetFrameTitle()
{
    wxString tit = wxT("");

    if (loaded != IMAGE_UNKNOWN) {
        tit.append(loaded_game.GetFullName());
        tit.append(wxT(" - "));
    }

    tit.append(wxT("VisualBoyAdvance-M "));
    tit.append(kVbamVersion);

#ifndef NO_LINK
    int playerId = GetLinkPlayerId();

    if (playerId >= 0) {
        tit.append(_(" player "));
        tit.append(wxChar(wxT('1') + playerId));
    }

#endif
    wxGetApp().frame->SetTitle(tit);
}

// Get system name string for the currently loaded ROM
// Returns expanded system name based on the ROM type
static wxString GetCurrentSystemName(IMAGE_TYPE game_type) {
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

// Expand %s in path to system name
static wxString ExpandSystemPath(const wxString& path, IMAGE_TYPE game_type) {
    wxString result = path;
    if (result.Contains(wxT("%s"))) {
        wxString system_name = GetCurrentSystemName(game_type);
        result.Replace(wxT("%s"), system_name);
    }
    return result;
}

void GameArea::recompute_dirs()
{
    batdir = ExpandSystemPath(OPTION(kGenBatteryDir), loaded);

    if (batdir.empty()) {
        batdir = loaded_game.GetPathWithSep();
    } else {
        batdir = wxGetApp().GetAbsolutePath(batdir);
        // Try to create the directory if it doesn't exist
        if (!wxFileName::DirExists(batdir)) {
            if (!wxFileName::Mkdir(batdir, 0777, wxPATH_MKDIR_FULL)) {
                wxMessageBox(
                    wxString::Format(_("Could not create Native Saves directory:\n%s\n\nPlease check your configured path in Options > Directories."), batdir),
                    _("Directory Error"),
                    wxOK | wxICON_ERROR);
            }
        }
    }

    if (!wxIsWritable(batdir)) {
        batdir = wxGetApp().GetDataDir();
    }

    statedir = ExpandSystemPath(OPTION(kGenStateDir), loaded);

    if (statedir.empty()) {
        statedir = loaded_game.GetPathWithSep();
    } else {
        statedir = wxGetApp().GetAbsolutePath(statedir);
        // Try to create the directory if it doesn't exist
        if (!wxFileName::DirExists(statedir)) {
            if (!wxFileName::Mkdir(statedir, 0777, wxPATH_MKDIR_FULL)) {
                wxMessageBox(
                    wxString::Format(_("Could not create Emulator Saves directory:\n%s\n\nPlease check your configured path in Options > Directories."), statedir),
                    _("Directory Error"),
                    wxOK | wxICON_ERROR);
            }
        }
    }

    if (!wxIsWritable(statedir)) {
        statedir = wxGetApp().GetDataDir();
    }
}

void GameArea::UnloadGame(bool destruct)
{
    if (!emulating)
        return;

    // last opportunity to autosave cheats
    if (OPTION(kPrefAutoSaveLoadCheatList) && cheats_dirty) {
        wxFileName cfn = loaded_game;
        // SetExt may strip something off by accident, so append to text instead
        cfn.SetFullName(cfn.GetFullName() + wxT(".clt"));

        if (loaded == IMAGE_GB) {
            if (!gbCheatNumber)
                wxRemoveFile(cfn.GetFullPath());
            else
                gbCheatsSaveCheatList(UTF8(cfn.GetFullPath()));
        } else {
            if (!cheatsNumber)
                wxRemoveFile(cfn.GetFullPath());
            else
                cheatsSaveCheatList(UTF8(cfn.GetFullPath()));
        }
    }

    // if timer was counting down for save, go ahead and save
    // this might not be safe, though..
    if (systemSaveUpdateCounter > SYSTEM_SAVE_NOT_UPDATED) {
        SaveBattery();
    }

    MainFrame* mf = wxGetApp().frame;
#ifndef NO_FFMPEG
    snd_rec.Stop();
    vid_rec.Stop();
#endif
    systemStopGameRecording();
    systemStopGamePlayback();

#if defined(VBAM_ENABLE_DEBUGGER)
    debugger = false;
    remoteCleanUp();
    mf->cmd_enable |= CMDEN_NGDB_ANY;
#endif  // VBAM_ENABLE_DEBUGGER

    if (loaded == IMAGE_GB) {
        gbCleanUp();
        gbCheatRemoveAll();

        // Reset overrides.
        coreOptions.gbPrinterEnabled = OPTION(kPrefGBPrinter);
    } else if (loaded == IMAGE_GBA) {
        CPUCleanUp();
        cheatsDeleteAll(false);
    }

    UnsuspendScreenSaver();
    emulating = false;
    loaded = IMAGE_UNKNOWN;
    emusys = NULL;
    soundShutdown();

    disableKeyboardBackgroundInput();

    if (destruct)
        return;

    // in destructor, panel should be auto-deleted by wx since all panels
    // are derived from a window attached as child to GameArea
    ResetPanel();

    // close any game-related viewer windows
    // in destructor, viewer windows are in process of being deleted anyway
    while (!mf->popups.empty())
        mf->popups.front()->Close(true);

    // remaining items are GUI updates that should not be needed in destructor
    SetFrameTitle();
    mf->cmd_enable &= UNLOAD_CMDEN_KEEP;
    mf->update_state_ts(true);
    mf->enable_menus();
    mf->ResetCheatSearch();

    if (rewind_mem)
        num_rewind_states = 0;
}

bool GameArea::LoadState()
{
    int slot = wxGetApp().frame->newest_state_slot();

    if (slot < 1)
        return false;

    return LoadState(slot);
}

bool GameArea::LoadState(int slot)
{
    wxString fname;
    fname.Printf(SAVESLOT_FMT, game_name().wc_str(), slot);
    return LoadState(wxFileName(statedir, fname));
}

bool GameArea::LoadState(const wxFileName& fname)
{
    // FIXME: first save to backup state if not backup state
    bool ret = emusys->emuReadState(UTF8(fname.GetFullPath()));

    if (ret && num_rewind_states) {
        MainFrame* mf = wxGetApp().frame;
        mf->cmd_enable &= ~CMDEN_REWIND;
        mf->enable_menus();
        num_rewind_states = 0;
        // do an immediate rewind save
        // even if loaded from state file: not smart enough yet to just
        // do a reset or load from state file when # rewinds == 0
        do_rewind = true;
        rewind_time = gopts.rewind_interval * 6;
    }

    if (ret) {
        // forget old save writes
        systemSaveUpdateCounter = SYSTEM_SAVE_NOT_UPDATED;
        // no point in blending after abrupt change
        InterframeCleanup();
        // frame rate calc should probably reset as well
        was_paused = true;
        // save state had a screen frame, so draw it
        systemDrawScreen();
    }

    wxString msg;
    msg.Printf(ret ? _("Loaded state %s") : _("Error loading state %s"),
        fname.GetFullPath().wc_str());
    systemScreenMessage(msg);
    return ret;
}

bool GameArea::SaveState()
{
    return SaveState(wxGetApp().frame->oldest_state_slot());
}

bool GameArea::SaveState(int slot)
{
    wxString fname;
    fname.Printf(SAVESLOT_FMT, game_name().wc_str(), slot);
    return SaveState(wxFileName(statedir, fname));
}

bool GameArea::SaveState(const wxFileName& fname)
{
    // FIXME: first copy to backup state if not backup state
    bool ret = emusys->emuWriteState(UTF8(fname.GetFullPath()));
    wxGetApp().frame->update_state_ts(true);
    wxString msg;
    msg.Printf(ret ? _("Saved state %s") : _("Error saving state %s"),
        fname.GetFullPath().wc_str());
    systemScreenMessage(msg);
    return ret;
}

void GameArea::SaveBattery()
{
    // MakeInstanceFilename doesn't do wxString, so just add slave ID here
    wxString bname = game_name();
#ifndef NO_LINK
    int playerId = GetLinkPlayerId();

    if (playerId >= 0) {
        bname.append(wxT('-'));
        bname.append(wxChar(wxT('1') + playerId));
    }

#endif
    bname.append(wxT(".sav"));
    wxFileName bat(batdir, bname);
    bat.Mkdir(0777, wxPATH_MKDIR_FULL);
    wxString fn = bat.GetFullPath();

    // FIXME: add option to support ring of backups
    // of course some games just write battery way too often for such
    // a thing to be useful
    if (!emusys->emuWriteBattery(UTF8(fn)))
        wxLogError(_("Error writing battery %s"), fn.mb_str());

    systemSaveUpdateCounter = SYSTEM_SAVE_NOT_UPDATED;
}

void GameArea::AddBorder()
{
    if (basic_width != GBWidth)
        return;

    basic_width = SGBWidth;
    basic_height = SGBHeight;
    gbBorderLineSkip = SGBWidth;
    gbBorderColumnSkip = (SGBWidth - GBWidth) / 2;
    gbBorderRowSkip = (SGBHeight - GBHeight) / 2;
    AdjustSize(false);
    wxGetApp().frame->Fit();
    GetSizer()->Detach(panel->GetWindow());
    ResetPanel();
}

void GameArea::DelBorder()
{
    if (basic_width != SGBWidth)
        return;

    basic_width = GBWidth;
    basic_height = GBHeight;
    gbBorderLineSkip = GBWidth;
    gbBorderColumnSkip = gbBorderRowSkip = 0;
    AdjustSize(false);
    wxGetApp().frame->Fit();
    GetSizer()->Detach(panel->GetWindow());
    ResetPanel();
}

void GameArea::AdjustMinSize()
{
    wxWindow* frame           = wxGetApp().frame;
    double dpi_scale_factor = widgets::DPIScaleFactorForWindow(this);
    const double display_scale = OPTION(kDispScale);

    // note: could safely set min size to 1x or less regardless of video_scale
    // but setting it to scaled size makes resizing to default easier
    wxSize sz((std::ceil(basic_width * display_scale) * dpi_scale_factor),
              (std::ceil(basic_height * display_scale) * dpi_scale_factor));
    SetMinSize(sz);
#if wxCHECK_VERSION(2, 8, 8)
    sz = frame->ClientToWindowSize(sz);
#else
    sz += frame->GetSize() - frame->GetClientSize();
#endif
    frame->SetMinSize(sz);
}

void GameArea::LowerMinSize()
{
    wxWindow* frame           = wxGetApp().frame;
    double dpi_scale_factor = widgets::DPIScaleFactorForWindow(this);

    wxSize sz(std::ceil(basic_width * dpi_scale_factor),
              std::ceil(basic_height * dpi_scale_factor));

    SetMinSize(sz);
    // do not take decorations into account
    frame->SetMinSize(sz);
}

void GameArea::AdjustSize(bool force)
{
    AdjustMinSize();

    if (fullscreen)
        return;

    double dpi_scale_factor = widgets::DPIScaleFactorForWindow(this);
    const double display_scale = OPTION(kDispScale);

    const wxSize newsz(
        (std::ceil(basic_width * display_scale) * dpi_scale_factor),
        (std::ceil(basic_height * display_scale) * dpi_scale_factor));

    if (!force) {
        wxSize sz = GetClientSize();

        if (sz.GetWidth() >= newsz.GetWidth() && sz.GetHeight() >= newsz.GetHeight())
            return;
    }

    SetSize(newsz);
    GetParent()->SetClientSize(newsz);
    wxGetApp().frame->Fit();
}

void GameArea::ResetPanel() {
    if (panel) {
        panel->Destroy();
        panel = NULL;
    }
}

void GameArea::ShowFullScreen(bool full)
{
    if (full == fullscreen) {
        // in case the tlw somehow lost its mind, force it to proper mode
        if (wxGetApp().frame->IsFullScreen() != fullscreen)
            wxGetApp().frame->ShowFullScreen(full);

        return;
    }

// on Mac maximize is native fullscreen, so ignore fullscreen requests
#ifdef __WXMAC__
    if (full && main_frame->IsMaximized()) return;
#endif

    fullscreen = full;

    // just in case screen mode is going to change, go ahead and preemptively
    // delete panel to be recreated immediately after resize
    ResetPanel();

    // Windows does not restore old window size/pos
    // at least under Wine
    // so store them before entering fullscreen
    static bool cursz_valid = false;
    static wxSize cursz;
    static wxPoint curpos;
    MainFrame* tlw = wxGetApp().frame;
    int dno = wxDisplay::GetFromWindow(tlw);

    if (!full) {
        if (gopts.fs_mode.w && gopts.fs_mode.h) {
            wxDisplay d(dno);
            d.ChangeMode(wxDefaultVideoMode);
        }

        tlw->ShowFullScreen(false);

        if (!cursz_valid) {
            curpos = wxDefaultPosition;
            cursz = tlw->GetMinSize();
        }

        tlw->SetSize(cursz);
        tlw->SetPosition(curpos);
        AdjustMinSize();
    } else {
        // close all non-modal dialogs
        while (!tlw->popups.empty())
            tlw->popups.front()->Close();

        // mouse stays blank whenever full-screen
        HidePointer();
        cursz_valid = true;
        cursz = tlw->GetSize();
        curpos = tlw->GetPosition();
        LowerMinSize();

        if (gopts.fs_mode.w && gopts.fs_mode.h) {
            // grglflargm
            // stupid wx does not do fullscreen properly when video mode is
            // changed under X11.   Since it does not use xrandr to switch, it
            // just changes the video mode without resizing the desktop.  It
            // still uses the original desktop size for fullscreen, though.
            // It also seems to stick the top-left corner wherever it pleases,
            // so I can't just resize the panel and expect it to show up.
            // ^(*&^^&!!!!  maybe just disable this code altogether on UNIX
            // most 3d cards are fine with full screen size, anyway.
            wxDisplay d(dno);

            if (!d.ChangeMode(gopts.fs_mode)) {
                wxLogInfo(_("Fullscreen mode %d x %d - %d @ %d not supported; looking for another"),
                    gopts.fs_mode.w, gopts.fs_mode.h, gopts.fs_mode.bpp, gopts.fs_mode.refresh);
                // specifying a mode may not work with bpp/rate of 0
                // in particular, unix does Matches() in wrong direction
                wxArrayVideoModes vm = d.GetModes();
                int best_mode = -1;
                size_t i;

                for (i = 0; i < vm.size(); i++) {
                    if (vm[i].w != gopts.fs_mode.w || vm[i].h != gopts.fs_mode.h)
                        continue;

                    int bpp = vm[i].bpp;

                    if (gopts.fs_mode.bpp && bpp == gopts.fs_mode.bpp)
                        break;

                    if (!gopts.fs_mode.bpp && bpp == 32) {
                        gopts.fs_mode.bpp = 32;
                        break;
                    }

                    int bm_bpp = best_mode < 0 ? 0 : vm[i].bpp;

                    if (bpp == 32 && bm_bpp != 32)
                        best_mode = i;
                    else if (bpp == 24 && bm_bpp < 24)
                        best_mode = i;
                    else if (bpp == 16 && bm_bpp < 24 && bm_bpp != 16)
                        best_mode = i;
                    else if (!best_mode)
                        best_mode = i;
                }

                if (i == vm.size() && best_mode >= 0)
                    i = best_mode;

                if (i == vm.size()) {
                    wxLogWarning(_("Fullscreen mode %d x %d - %d @ %d not supported"),
                        gopts.fs_mode.w, gopts.fs_mode.h, gopts.fs_mode.bpp, gopts.fs_mode.refresh);
                    gopts.fs_mode = wxVideoMode();

                    for (i = 0; i < vm.size(); i++)
                        wxLogInfo(_("Valid mode: %d x %d - %d @ %d"), vm[i].w,
                            vm[i].h, vm[i].bpp,
                            vm[i].refresh);
                } else {
                    gopts.fs_mode.bpp = vm[i].bpp;
                    gopts.fs_mode.refresh = vm[i].refresh;
                }

                wxLogInfo(_("Chose mode %d x %d - %d @ %d"),
                    gopts.fs_mode.w, gopts.fs_mode.h, gopts.fs_mode.bpp, gopts.fs_mode.refresh);

                if (!d.ChangeMode(gopts.fs_mode)) {
                    wxLogWarning(_("Failed to change mode to %d x %d - %d @ %d"),
                        gopts.fs_mode.w, gopts.fs_mode.h, gopts.fs_mode.bpp, gopts.fs_mode.refresh);
                    gopts.fs_mode.w = 0;
                }
            }
        }

        tlw->ShowFullScreen(true);
    }
}

GameArea::~GameArea()
{
    UnloadGame(true);

    if (rewind_mem)
        free(rewind_mem);

    if (gopts.fs_mode.w && gopts.fs_mode.h && fullscreen) {
        MainFrame* tlw = wxGetApp().frame;
        int dno = wxDisplay::GetFromWindow(tlw);
        wxDisplay d(dno);
        d.ChangeMode(wxDefaultVideoMode);
    }
}

void GameArea::OnKillFocus(wxFocusEvent& ev)
{
    wxGetApp().emulated_gamepad()->Reset();
    ev.Skip();
}

void GameArea::Pause()
{
    if (paused)
        return;

    // don't pause when linked
#ifndef NO_LINK
    if (GetLinkMode() != LINK_DISCONNECTED)
        return;
#endif

    paused = was_paused = true;
    UnsuspendScreenSaver();

    // when the game is paused like this, we should not allow any
    // input to remain pressed, because they could be released
    // outside of the game zone and we would not know about it.
    wxGetApp().emulated_gamepad()->Reset();

    if (loaded != IMAGE_UNKNOWN)
        soundPause();
}

void GameArea::Resume()
{
    if (!paused)
        return;

    paused = false;
    SuspendScreenSaver();
    SetExtraStyle(GetExtraStyle() | wxWS_EX_PROCESS_IDLE);

    if (loaded != IMAGE_UNKNOWN)
        soundResume();

    SetFocus();
}

void GameArea::OnIdle(wxIdleEvent& event)
{
    wxString pl = wxGetApp().pending_load;
    MainFrame* mf = wxGetApp().frame;

    if (pl.size()) {
        // sometimes this gets into a loop if LoadGame() called before
        // clearing pending_load.  weird.
        wxGetApp().pending_load = wxEmptyString;
        LoadGame(pl);

#if defined(VBAM_ENABLE_DEBUGGER)
        if (OPTION(kPrefGDBBreakOnLoad)) {
            mf->GDBBreak();
        }

        if (debugger && loaded != IMAGE_GBA) {
            wxLogError(_("Not a valid Game Boy Advance cartridge"));
            UnloadGame();
        }
#endif  // defined(VBAM_ENABLE_DEBUGGER)
    }

    // stupid wx doesn't resize to screen size
    // forcing it this way just puts it in an infinite loop, though
    // with wx trying to resize/reposition to what it thinks is full screen
    // every time it detects a manual resize like this
    if (gopts.fs_mode.w && gopts.fs_mode.h && fullscreen) {
        wxSize sz = GetSize();

        if (sz.GetWidth() != gopts.fs_mode.w || sz.GetHeight() != gopts.fs_mode.h) {
            wxGetApp().frame->Move(0, 84);
            SetSize(gopts.fs_mode.w, gopts.fs_mode.h);
        }
    }

    if (!emusys) {
        // No game loaded, can't create panel
        return;
    }

    if (schedule_audio_restart_) {
        soundShutdown();
        if (!soundInit()) {
            wxLogError(_("Could not initialize the sound driver!"));
        }
        schedule_audio_restart_ = false;
    }

    if (!panel) {
        switch (OPTION(kDispRenderMethod)) {
            case config::RenderMethod::kSimple:
                panel = new BasicDrawingPanel(this, basic_width, basic_height);
                break;
            case config::RenderMethod::kSDL:
                panel = new SDLDrawingPanel(this, basic_width, basic_height);
                break;
#ifdef __WXMAC__
#ifndef NO_METAL
            case config::RenderMethod::kMetal:
                if (is_macosx_1012_or_newer()) {
                    panel =
                    new MetalDrawingPanel(this, basic_width, basic_height);
                } else {
                    panel = new GLDrawingPanel(this, basic_width, basic_height);
                    wxLogInfo(_("Metal is unavailable, defaulting to OpenGL"));
                }
                break;
#endif
            case config::RenderMethod::kQuartz2d:
                panel =
                    new Quartz2DDrawingPanel(this, basic_width, basic_height);
                break;
#endif
#ifndef NO_OGL
            case config::RenderMethod::kOpenGL:
                panel = new GLDrawingPanel(this, basic_width, basic_height);
                break;
#endif
#if defined(__WXMSW__) && !defined(NO_D3D)
            case config::RenderMethod::kDirect3d:
                panel = new DXDrawingPanel(this, basic_width, basic_height);
                break;
#endif
            case config::RenderMethod::kLast:
                VBAM_NOTREACHED();
                return;
        }

        wxWindow* w = NULL;

        w = panel->GetWindow();

        // set up event handlers
        w->Bind(VBAM_EVT_USER_INPUT, &GameArea::OnUserInput, this);
        w->Bind(wxEVT_PAINT, &GameArea::PaintEv, this);
        w->Bind(wxEVT_ERASE_BACKGROUND, &GameArea::EraseBackground, this);

        // set userdata so we know it's the panel and not the frame being resized
        // the userdata is freed on disconnect/destruction
        this->Bind(wxEVT_SIZE, &GameArea::OnSize, this);

        // We need to check if the buttons stayed pressed when focus the panel.
        w->Bind(wxEVT_KILL_FOCUS, &GameArea::OnKillFocus, this);

        // Update mouse last-used timers on mouse events etc..
        w->Bind(wxEVT_MOTION, &GameArea::MouseEvent, this);
        w->Bind(wxEVT_LEFT_DOWN, &GameArea::MouseEvent, this);
        w->Bind(wxEVT_RIGHT_DOWN, &GameArea::MouseEvent, this);
        w->Bind(wxEVT_MIDDLE_DOWN, &GameArea::MouseEvent, this);
        w->Bind(wxEVT_MOUSEWHEEL, &GameArea::MouseEvent, this);

        w->SetBackgroundStyle(wxBG_STYLE_CUSTOM);
        w->SetSize(wxSize(basic_width, basic_height));

        // Allow input while on background
        if (OPTION(kUIAllowKeyboardBackgroundInput)) {
            enableKeyboardBackgroundInput(w->GetEventHandler());
        }

        if (gopts.max_scale)
            w->SetMaxSize(wxSize(basic_width * gopts.max_scale,
                                 basic_height * gopts.max_scale));

        // if user changed Display/Scale config, this needs to run
        AdjustMinSize();
        AdjustSize(false);

        const bool retain_aspect = OPTION(kDispStretch);
        const unsigned frame_priority = retain_aspect ? 0 : 1;

        GetSizer()->Clear();

        // add spacers on top and bottom to center panel vertically
        // but not on 2.8 which does not handle this correctly
        if (retain_aspect) {
            GetSizer()->Add(0, 0, wxEXPAND);
        }

        // this triggers an assertion dialog in <= 3.1.2 in debug mode
        GetSizer()->Add(
            w, frame_priority,
            retain_aspect ? (wxSHAPED | wxALIGN_CENTER | wxEXPAND) : wxEXPAND);

        if (retain_aspect) {
            GetSizer()->Add(0, 0, wxEXPAND);
        }

        Layout();
        SendSizeEvent();

        if (pointer_blanked)
            w->SetCursor(wxCursor(wxCURSOR_BLANK));

        // set focus to panel
        w->SetFocus();

        // generate system color maps (after output module init)
        UpdateLcdFilter();
    }

    if (!paused) {
        HidePointer();
        HideMenuBar();
        event.RequestMore();

#if defined(VBAM_ENABLE_DEBUGGER)
        if (debugger) {
            was_paused = true;
            dbgMain();

            if (!emulating) {
                emulating = true;
                SuspendScreenSaver();
                UnloadGame();
            }

            return;
        }
#endif  // defined(VBAM_ENABLE_DEBUGGER)

        emusys->emuMain(emusys->emuCount);
#ifndef NO_LINK

        if (loaded == IMAGE_GBA && GetLinkMode() != LINK_DISCONNECTED)
            CheckLinkConnection();

#endif
    } else {
        was_paused = true;

        if (paused)
            SetExtraStyle(GetExtraStyle() & ~wxWS_EX_PROCESS_IDLE);

        ShowMenuBar();
    }

    if (do_rewind && emusys->emuWriteMemState) {
        if (!rewind_mem) {
            rewind_mem = (char*)malloc(NUM_REWINDS * REWIND_SIZE);
            num_rewind_states = next_rewind_state = 0;
        }

        if (!rewind_mem) {
            wxLogError(_("No memory for rewinding"));
            wxGetApp().frame->Close(true);
            return;
        }

        long resize;

        if (!emusys->emuWriteMemState(&rewind_mem[REWIND_SIZE * next_rewind_state],
                REWIND_SIZE, resize /* actual size */))
            // if you see a lot of these, maybe increase REWIND_SIZE
            wxLogInfo(_("Error writing rewind state"));
        else {
            if (!num_rewind_states) {
                mf->cmd_enable |= CMDEN_REWIND;
                mf->enable_menus();
            }

            if (num_rewind_states < NUM_REWINDS)
                ++num_rewind_states;

            next_rewind_state = (next_rewind_state + 1) % NUM_REWINDS;
        }

        do_rewind = false;
    }
}

static void draw_black_background(wxWindow* win) {
    wxClientDC dc(win);
    wxCoord w, h;
    dc.GetSize(&w, &h);
    dc.SetPen(*wxBLACK_PEN);
    dc.SetBrush(*wxBLACK_BRUSH);
    dc.DrawRectangle(0, 0, w, h);
}

#ifdef __WXGTK__
static Display* GetX11Display() {
    return GDK_WINDOW_XDISPLAY(gtk_widget_get_window(wxGetApp().frame->GetHandle()));
}
#endif  // __WXGTK__

void GameArea::OnUserInput(widgets::UserInputEvent& event) {
    bool emulated_key_pressed = false;
    for (const auto& event_data : event.data()) {
        if (event_data.pressed) {
            if (wxGetApp().emulated_gamepad()->OnInputPressed(event_data.input)) {
                emulated_key_pressed = true;
            }
        } else {
            if (wxGetApp().emulated_gamepad()->OnInputReleased(event_data.input)) {
                emulated_key_pressed = true;
            }
        }
    }

    if (emulated_key_pressed) {
        wxWakeUpIdle();

#if defined(__WXGTK__) && defined(HAVE_X11) && !defined(HAVE_XSS)
        // Tell X11 to turn off the screensaver/screen-blank if a button was
        // was pressed. This shouldn't be necessary.
        if (!wxGetApp().UsingWayland()) {
            auto display = GetX11Display();
            XResetScreenSaver(display);
            XFlush(display);
        }
#endif
    }
}

// these three are forwarded to the DrawingPanel instance
void GameArea::PaintEv(wxPaintEvent& ev)
{
    panel->PaintEv(ev);
}

void GameArea::EraseBackground(wxEraseEvent& ev)
{
    panel->EraseBackground(ev);
}

void GameArea::OnSize(wxSizeEvent& ev)
{
    draw_black_background(this);
    Layout();

    // panel may resize
    if (panel)
        panel->OnSize(ev);
    Layout();

    ev.Skip();
}

BEGIN_EVENT_TABLE(GameArea, wxPanel)
EVT_IDLE(GameArea::OnIdle)
// FIXME: wxGTK does not generate motion events in MainFrame (not sure
// what to do about it)
EVT_MOUSE_EVENTS(GameArea::MouseEvent)
END_EVENT_TABLE()

DrawingPanelBase::DrawingPanelBase(int _width, int _height)
    : width(_width),
      height(_height),
      scale(1),
      did_init(false),
      todraw(0),
      pixbuf1(0),
      pixbuf2(0),
      nthreads(0),
      rpi_(NULL) {
    memset(delta, 0xff, sizeof(delta));
    memset(&rpi_info_, 0, sizeof(rpi_info_));

    if (OPTION(kDispFilter) == config::Filter::kPlugin) {
        const wxString& pluginPath = OPTION(kDispFilterPlugin);
        bool loaded = false;
#ifdef VBAM_RPI_PROXY_SUPPORT
        rpi_proxy::RpiProxyClient::Log("DrawingPanelBase: constructor, pluginPath=%ls", pluginPath.wc_str());
#endif

#ifdef VBAM_RPI_PROXY_SUPPORT
        // Check if this is a 32-bit plugin that needs the proxy
        bool needsProxy = widgets::PluginNeedsProxy(pluginPath);
        rpi_proxy::RpiProxyClient::Log("DrawingPanelBase: needsProxy=%d", needsProxy ? 1 : 0);
        if (needsProxy) {
            rpi_proxy_client_ = rpi_proxy::RpiProxyClient::GetSharedInstance();
            rpi_proxy::RpiProxyClient::Log("DrawingPanelBase: Got shared proxy instance: %p", rpi_proxy_client_);
            if (widgets::MaybeLoadFilterPluginViaProxy(pluginPath,
                    rpi_proxy_client_, &rpi_info_)) {
                rpi_ = &rpi_info_;
                using_rpi_proxy_ = true;
                loaded = true;
                rpi_proxy::RpiProxyClient::Log("DrawingPanelBase: Plugin loaded via proxy: %s (Flags=0x%X)", rpi_info_.Name, rpi_info_.Flags);
            } else {
                rpi_proxy::RpiProxyClient::Log("DrawingPanelBase: MaybeLoadFilterPluginViaProxy failed");
                rpi_proxy_client_ = nullptr;
            }
        }
#endif

        // Try direct loading if proxy wasn't used or failed
        if (!loaded) {
            rpi_ = widgets::MaybeLoadFilterPlugin(pluginPath, &filter_plugin_);
            if (rpi_) {
                loaded = true;
            }
        }

        if (loaded && rpi_) {
            // Select the best color format the plugin supports
            // Prefer 888 (32-bit), then 555 (16-bit), then 565 (16-bit)
            // We prefer 555 over 565 because it's more common and simpler.
            bool using_rgb565 = false;
            if (rpi_->Flags & RPI_888_SUPP) {
                rpi_->Flags &= ~(RPI_555_SUPP | RPI_565_SUPP);
                panel_color_depth_ = 32;
                systemColorDepth = 32;  // Core needs this to output correct format
                rpi_bpp_ = 4;
            } else if (rpi_->Flags & RPI_555_SUPP) {
                rpi_->Flags &= ~(RPI_565_SUPP | RPI_888_SUPP);
                panel_color_depth_ = 16;
                systemColorDepth = 16;  // Core needs this to output correct format
                rpi_bpp_ = 2;
            } else if (rpi_->Flags & RPI_565_SUPP) {
                rpi_->Flags &= ~(RPI_555_SUPP | RPI_888_SUPP);
                panel_color_depth_ = 16;
                systemColorDepth = 16;  // Core needs this to output correct format
                rpi_bpp_ = 2;
                using_rgb565 = true;
            } else {
                // No supported color format - this shouldn't happen
                panel_color_depth_ = 32;
                systemColorDepth = 32;  // Core needs this to output correct format
                rpi_bpp_ = 4;
            }
            // Store for color shift configuration later
            rpi_using_rgb565_ = using_rgb565;

#ifdef VBAM_RPI_PROXY_SUPPORT
            // For proxy mode, Output is handled by the proxy client
            if (!using_rpi_proxy_)
#endif
            {
                if (!rpi_->Output) {
                    // note that in actual kega fusion plugins, rpi_->Output is
                    // unused (as is rpi_->Handle)
                    rpi_->Output =
                        (RENDPLUG_Output)filter_plugin_.GetSymbol("RenderPluginOutput");
                }
            }
            uint32_t pluginScale = (rpi_->Flags & RPI_OUT_SCLMSK) >> RPI_OUT_SCLSH;
            // Ensure scale is at least 1 before multiplying
            if (scale < 1.0) {
                scale = 1.0;
            }
            scale *= pluginScale;
        } else {
            // This is going to delete the object. Do nothing more here.
            OPTION(kDispFilterPlugin) = wxEmptyString;
            OPTION(kDispFilter) = config::Filter::kNone;
            return;
        }
    }

    if (OPTION(kDispFilter) != config::Filter::kPlugin) {
        scale *= GetFilterScale();

        panel_color_depth_ = (OPTION(kBitDepth) + 1) << 3;
        systemColorDepth = panel_color_depth_;  // Core needs this to output correct format
    }

    // Intialize color tables based on panel's color depth
    if (panel_color_depth_ == 24) {
#if wxBYTE_ORDER == wxLITTLE_ENDIAN
        systemRedShift = 3;
        systemGreenShift = 11;
        systemBlueShift = 19;
        RGB_LOW_BITS_MASK = 0x00010101;
#else
        systemRedShift = 27;
        systemGreenShift = 19;
        systemBlueShift = 11;
        RGB_LOW_BITS_MASK = 0x01010100;
#endif
    } else if (panel_color_depth_ == 32) {
#if wxBYTE_ORDER == wxLITTLE_ENDIAN
        systemRedShift = 3;
        systemGreenShift = 11;
        systemBlueShift = 19;
        RGB_LOW_BITS_MASK = 0x00010101;
#else
        systemRedShift = 27;
        systemGreenShift = 19;
        systemBlueShift = 11;
        RGB_LOW_BITS_MASK = 0x01010100;
#endif
    } else {
        // 16-bit mode: RGB555 or RGB565
        if (rpi_ && rpi_using_rgb565_) {
            // RGB565: 5 bits red, 6 bits green, 5 bits blue
            systemRedShift = 11;
            systemGreenShift = 5;
            systemBlueShift = 0;
            RGB_LOW_BITS_MASK = 0x0821;  // Low bit of each component (R:bit 11, G:bit 5, B:bit 0)
        } else {
            // RGB555: 5 bits each for red, green, blue
            systemRedShift = 10;
            systemGreenShift = 5;
            systemBlueShift = 0;
            RGB_LOW_BITS_MASK = 0x0421;
        }
    }
}

DrawingPanel::DrawingPanel(wxWindow* parent, int _width, int _height)
    : DrawingPanelBase(_width, _height)
    , wxPanel(parent, wxID_ANY, wxPoint(0, 0), parent->GetClientSize(),
          wxFULL_REPAINT_ON_RESIZE | wxWANTS_CHARS)
{
}

void DrawingPanelBase::DrawingPanelInit()
{
    did_init = true;
}

void DrawingPanelBase::PaintEv(wxPaintEvent& ev)
{
    (void)ev; // unused params
    wxPaintDC dc(GetWindow());

    static int paint_count = 0;
    if (paint_count < 20) {
        wxLogDebug("PaintEv called (count=%d, todraw=%p)", paint_count, todraw);
        paint_count++;
    }

    if (!todraw) {
        // since this is set for custom background, not drawing anything
        // will cause garbage to be displayed, so draw a black area
        wxLogDebug("PaintEv: todraw is NULL, drawing black background");
        draw_black_background(GetWindow());
        return;
    }

    DrawArea(dc);

    // currently we draw the OSD directly on the framebuffer to reduce flickering
    //DrawOSD(dc);
}

void DrawingPanelBase::EraseBackground(wxEraseEvent& ev)
{
    (void)ev; // unused params
    // do nothing, do not allow propagation
}

// In order to run filters in parallel, they have to run from the method of
// a wxThread-derived class
//   threads are only created once, if possible, to avoid thread creation
//   overhead every frame; mutex+cond used to signal start and sem for end

//   when threading, there _will_ be bands for any nontrivial filter
//   usually the top and bottom line(s) of each region will look a little
//   different.  To ease this a tiny bit, 2 extra lines are generated at
//   the top of each region, so hopefully only the bottom of the previous
//   region will look screwed up.  The only correct way to handle this
//   is to draw an increasingly larger band around the seam until the
//   seam cover matches a line in both top & bottom regions, and then apply
//   cover to seam.   Way too much trouble for this, though.

//   another issue to consider is whether or not these filters are thread-
//   safe to begin with.  The built-in ones are verifyable (I didn't verify
//   them, though).  The plugins cannot be verified.  However, unlike the MFC
//   interface, I will allow them to be threaded at user's discretion.
class FilterThread : public wxThread {
public:
    FilterThread() : wxThread(wxTHREAD_JOINABLE), lock_(), sig_(lock_),
                     src2_(nullptr), dst2_(nullptr), src2_size_(0), dst2_size_(0) {}

    ~FilterThread() {
        // Free pre-allocated conversion buffers
        if (src2_) free(src2_);
        if (dst2_) free(dst2_);
    }

    wxMutex lock_;
    wxCondition sig_;
    wxSemaphore* done_;
    wxSemaphore* ready_;  // Posted when thread is ready to receive work signals

    // Set these params before running
    int nthreads_;
    int threadno_;
    int width_;
    int height_;
    double scale_;
    const RENDER_PLUGIN_INFO* rpi_;
    int rpi_bpp_ = 4;  // Bytes per pixel for RPI plugin (default 32-bit = 4)
    int panel_color_depth_ = 32;  // Panel's color depth (independent from global systemColorDepth)
    uint8_t* dst_;
    uint8_t* delta_;
#ifdef VBAM_RPI_PROXY_SUPPORT
    rpi_proxy::RpiProxyClient* rpi_proxy_client_ = nullptr;
    bool proxy_thread_started_ = false;
#endif

    // set this param every round
    // if NULL, end thread
    uint8_t* src_;

    // Pre-allocated conversion buffers (reused across frames)
    uint32_t* src2_;
    uint32_t* dst2_;
    size_t src2_size_;
    size_t dst2_size_;

    ExitCode Entry() override {
#ifdef VBAM_RPI_PROXY_SUPPORT
        // Start the proxy thread for this filter thread
        if (rpi_proxy_client_ && OPTION(kDispFilter) == config::Filter::kPlugin) {
            rpi_proxy_client_->StartThread(static_cast<uint32_t>(threadno_));
            proxy_thread_started_ = true;
        }
#endif

        // Pre-compute all constants once at thread start (not per-frame)
        const int height_real = height_;
        const int procy = height_ * threadno_ / nthreads_;
        height_ = height_ * (threadno_ + 1) / nthreads_ - procy;

        // Cache depth format flags for fast branching
        // Use panel_color_depth_ instead of systemColorDepth to avoid race conditions
        const bool is_8bit = (panel_color_depth_ == 8);
        const bool is_16bit = (panel_color_depth_ == 16);
        const bool is_24bit = (panel_color_depth_ == 24);
        const bool is_32bit = !is_8bit && !is_16bit && !is_24bit;

        // Pre-compute strides using integer math where possible
        // For plugin filters, use the cached rpi_bpp_ to avoid global state issues
        // For non-plugin filters, use panel_color_depth_ (not systemColorDepth) to avoid race conditions
        const bool using_plugin = (rpi_ != nullptr);
        const int inbpp = using_plugin ? rpi_bpp_ : (panel_color_depth_ >> 3);
        // Calculate inrb based on actual bpp for plugins to avoid global state issues
        const int inrb = using_plugin ? ((rpi_bpp_ == 1) ? 4 : (rpi_bpp_ == 2) ? 2 : (rpi_bpp_ == 3) ? 0 : 1)
                                      : (is_8bit ? 4 : is_16bit ? 2 : is_24bit ? 0 : 1);
        const int instride = (width_ + inrb) * inbpp;

        // For 32bpp conversion: filters need left+right borders of 1 each
        const int total_width32 = width_ + 2;  // left border + image + right border
        const int instride32 = total_width32 * 4;

        const int outbpp = using_plugin ? rpi_bpp_ : (panel_color_depth_ >> 3);
        // Use integer scale calculation (scale_ is typically 2, 3, 4, etc.)
        const int scale_int = static_cast<int>(scale_);
        const int outstride = (width_ + inrb) * outbpp * scale_int;
        const int outstride32 = width_ * 4 * scale_int;

        // Pre-compute source offset (procy + 1 rows)
        const int src_offset = instride * (procy + 1);

        // Adjust delta once for this thread's band
        delta_ += instride * procy;

        // Skip scale rows of top border in output buffer
        dst_ += outstride * (procy + 1) * scale_int;

        uint8_t* const dest = dst_;

        // Pre-allocate conversion buffers for non-32bpp modes (reused across frames)
        if (!is_32bit) {
            // Calculate required buffer sizes
            const size_t src2_required = static_cast<size_t>(total_width32) * (height_real + 3);
            const size_t dst2_required = static_cast<size_t>(width_ * scale_int) * (height_real * scale_int + scale_int);

            // Reallocate only if needed (buffers persist across frames)
            if (src2_size_ < src2_required) {
                if (src2_) free(src2_);
                src2_ = static_cast<uint32_t*>(calloc(4, src2_required));
                src2_size_ = src2_required;
            }
            if (dst2_size_ < dst2_required) {
                if (dst2_) free(dst2_);
                dst2_ = static_cast<uint32_t*>(calloc(4, dst2_required));
                dst2_size_ = dst2_required;
            }
        }

        // Pre-compute scaled dimensions for conversion loops
        // Use height_ (this thread's slice) for multi-threaded mode, not height_real
        const int scaled_height = height_ * scale_int;
        const int scaled_width = width_ * scale_int;
        const int scaled_border_dest = inrb * scale_int;

        // Lock mutex before waiting on condition (required for wxCondition::Wait)
        if (nthreads_ > 1) {
            lock_.Lock();
            // Signal that this thread is ready to receive work signals
            ready_->Post();
        }

        while (nthreads_ == 1 || sig_.Wait() == wxCOND_NO_ERROR) {
            if (!src_) {
#ifdef VBAM_RPI_PROXY_SUPPORT
                // Stop the proxy thread when this filter thread is terminating
                if (proxy_thread_started_) {
                    rpi_proxy_client_->StopThread(static_cast<uint32_t>(threadno_));
                    proxy_thread_started_ = false;
                }
#endif
                lock_.Unlock();
                return 0;
            }

            // Advance source pointer to this thread's band
            src_ += src_offset;

            // Cache filter options for this frame (avoid repeated lookups)
            const config::Interframe ifb_option = OPTION(kDispIFB);
            const config::Filter filter_option = OPTION(kDispFilter);

            // Apply interframe blending filter
            ApplyInterframeOptimized(instride, procy, ifb_option, is_8bit, is_16bit, is_24bit);

            if (filter_option == config::Filter::kNone) {
                if (nthreads_ == 1)
                    return 0;

                done_->Post();
                continue;
            }

            // Apply filter - optimized path selection
            if (is_32bit) {
                // Direct 32bpp path - no conversion needed
                ApplyFilterOptimized(instride, outstride, filter_option);
            } else if (filter_option == config::Filter::kPlugin) {
                // Plugin filters handle their own format - pass data directly without conversion
                // Plugins with RPI_OUT_565 flag expect 16-bit data, not 32-bit converted data
                ApplyFilterOptimized(instride, outstride, filter_option);
            } else {
                // Non-32bpp with built-in filter: convert to 32bpp, apply filter, convert back
                // Save current shift values (set for original depth)
                const int saved_red_shift = systemRedShift;
                const int saved_green_shift = systemGreenShift;
                const int saved_blue_shift = systemBlueShift;
                const int saved_rgb_low_bits_mask = RGB_LOW_BITS_MASK;

                // Set shift values for 32bpp format that filters expect
#if wxBYTE_ORDER == wxLITTLE_ENDIAN
                systemRedShift = 19;
                systemGreenShift = 11;
                systemBlueShift = 3;
                RGB_LOW_BITS_MASK = 0x00010101;
#else
                systemRedShift = 27;
                systemGreenShift = 19;
                systemBlueShift = 11;
                RGB_LOW_BITS_MASK = 0x01010100;
#endif

                // Convert input to 32bpp (buffers pre-allocated above)
                int pos = total_width32;  // Start at row 1 (skip row 0 for top border)

                if (is_8bit) {
                    // 8bpp format: RRRGGGBB (3 red, 3 green, 2 blue bits)
                    int src_pos = 0;
                    for (int y = 0; y < height_; y++) {
                        const int left_border_pos = pos++;
                        for (int x = 0; x < width_; x++) {
                            const uint8_t src_val = src_[src_pos++];
                            const uint8_t r3 = (src_val >> 5) & 0x7;
                            const uint8_t g3 = (src_val >> 2) & 0x7;
                            const uint8_t b2 = src_val & 0x3;
                            const uint8_t r8 = (r3 << 5) | (r3 << 2) | (r3 >> 1);
                            const uint8_t g8 = (g3 << 5) | (g3 << 2) | (g3 >> 1);
                            const uint8_t b8 = (b2 << 6) | (b2 << 4) | (b2 << 2) | b2;
#if wxBYTE_ORDER == wxLITTLE_ENDIAN
                            src2_[pos++] = b8 | (g8 << 8) | (r8 << 16);
#else
                            src2_[pos++] = (r8 << 24) | (g8 << 16) | (b8 << 8);
#endif
                        }
                        src2_[left_border_pos] = src2_[left_border_pos + 1];
                        src2_[pos] = src2_[pos - 1];
                        pos++;
                        src_pos += inrb;
                    }
                } else if (is_16bit) {
                    // 16bpp RGB555 format
                    const uint16_t* src16 = reinterpret_cast<const uint16_t*>(src_);
                    int src_pos = 0;
                    for (int y = 0; y < height_; y++) {
                        const int left_border_pos = pos++;
                        for (int x = 0; x < width_; x++) {
                            const uint16_t src_val = src16[src_pos++];
                            const uint8_t r5 = (src_val >> 10) & 0x1f;
                            const uint8_t g5 = (src_val >> 5) & 0x1f;
                            const uint8_t b5 = src_val & 0x1f;
                            const uint8_t r8 = (r5 << 3) | (r5 >> 2);
                            const uint8_t g8 = (g5 << 3) | (g5 >> 2);
                            const uint8_t b8 = (b5 << 3) | (b5 >> 2);
#if wxBYTE_ORDER == wxLITTLE_ENDIAN
                            src2_[pos++] = b8 | (g8 << 8) | (r8 << 16);
#else
                            src2_[pos++] = (r8 << 24) | (g8 << 16) | (b8 << 8);
#endif
                        }
                        src2_[left_border_pos] = src2_[left_border_pos + 1];
                        src2_[pos] = src2_[pos - 1];
                        pos++;
                        src_pos += inrb;
                    }
                } else {  // is_24bit
                    int src_pos = 0;
                    for (int y = 0; y < height_; y++) {
                        const int left_border_pos = pos++;
                        for (int x = 0; x < width_; x++) {
#if wxBYTE_ORDER == wxLITTLE_ENDIAN
                            src2_[pos++] = src_[src_pos] | (src_[src_pos+1] << 8) | (src_[src_pos+2] << 16);
#else
                            src2_[pos++] = (src_[src_pos] << 24) | (src_[src_pos+1] << 16) | (src_[src_pos+2] << 8);
#endif
                            src_pos += 3;
                        }
                        src2_[left_border_pos] = src2_[left_border_pos + 1];
                        src2_[pos] = src2_[pos - 1];
                        pos++;
                    }
                }

                // Fill top border row by duplicating row 1
                memcpy(src2_, src2_ + total_width32, total_width32 * sizeof(uint32_t));

                // Fill bottom 2 extra rows by duplicating the last image row
                const int last_row_offset = total_width32 * height_;
                memcpy(src2_ + last_row_offset + total_width32, src2_ + last_row_offset, total_width32 * sizeof(uint32_t));
                memcpy(src2_ + last_row_offset + total_width32 * 2, src2_ + last_row_offset, total_width32 * sizeof(uint32_t));

                // Point to first image pixel (skip top row + left border)
                uint8_t* filter_src = reinterpret_cast<uint8_t*>(src2_ + total_width32 + 1);
                // Skip top border rows in output
                const int dst_offset = scaled_width * scale_int;
                uint8_t* filter_dst = reinterpret_cast<uint8_t*>(dst2_ + dst_offset);

                if (filter_option == config::Filter::kPlugin) {
                    // Plugin filter - use proxy or direct plugin call
                    // This path uses internal 32-bit buffers (src2_, dst2_) which have
                    // no side borders, only top border rows. The borders are handled
                    // during conversion back to original format.
                    RENDER_PLUGIN_OUTP outdesc;
                    outdesc.Size = sizeof(outdesc);
                    outdesc.Flags = rpi_->Flags;
                    outdesc.SrcPtr = filter_src;
                    outdesc.SrcPitch = instride32;
                    outdesc.SrcW = width_;
                    outdesc.SrcH = height_;
                    outdesc.DstPtr = filter_dst;
                    // dst2_ is tightly packed (no side borders), so DstPitch = data width
                    const int data_bytes_per_row32 = scaled_width * 4;
                    outdesc.DstPitch = data_bytes_per_row32;
                    outdesc.DstW = scaled_width;
                    outdesc.DstH = static_cast<int>(height_ * scale_);
                    outdesc.OutW = outdesc.DstW;
                    outdesc.OutH = outdesc.DstH;

#ifdef VBAM_RPI_PROXY_SUPPORT
                    if (rpi_proxy_client_) {
                        // Pass outstride32 as actualDstPitch for consistency
                        // (they're equal since dst2_ has no side borders)
                        rpi_proxy_client_->ApplyFilter(&outdesc, static_cast<uint32_t>(threadno_), outstride32);
                    } else
#endif
                    {
                        rpi_->Output(&outdesc);
                    }
                } else {
                    // Built-in filter
                    ApplyFilter32(filter_src, instride32, delta_, filter_dst, outstride32, width_, height_);
                }

                // Convert 32bpp output back to original depth
                const uint32_t* dst32 = dst2_ + dst_offset;

                if (is_8bit) {
                    int dst_pos = 0;
                    pos = 0;
                    for (int y = 0; y < scaled_height; y++) {
                        for (int x = 0; x < scaled_width; x++) {
                            const uint32_t color = dst32[dst_pos++];
#if wxBYTE_ORDER == wxLITTLE_ENDIAN
                            const uint8_t r8 = (color >> 16) & 0xff;
                            const uint8_t g8 = (color >> 8) & 0xff;
                            const uint8_t b8 = color & 0xff;
#else
                            const uint8_t r8 = (color >> 24) & 0xff;
                            const uint8_t g8 = (color >> 16) & 0xff;
                            const uint8_t b8 = (color >> 8) & 0xff;
#endif
                            dest[pos++] = ((r8 >> 5) << 5) | ((g8 >> 5) << 2) | (b8 >> 6);
                        }
                        pos += scaled_border_dest;
                    }
                } else if (is_16bit) {
                    uint16_t* dest16 = reinterpret_cast<uint16_t*>(dest);
                    int dst_pos = 0;
                    pos = 0;
                    for (int y = 0; y < scaled_height; y++) {
                        for (int x = 0; x < scaled_width; x++) {
                            const uint32_t color = dst32[dst_pos++];
#if wxBYTE_ORDER == wxLITTLE_ENDIAN
                            const uint8_t r8 = (color >> 16) & 0xff;
                            const uint8_t g8 = (color >> 8) & 0xff;
                            const uint8_t b8 = color & 0xff;
#else
                            const uint8_t r8 = (color >> 24) & 0xff;
                            const uint8_t g8 = (color >> 16) & 0xff;
                            const uint8_t b8 = (color >> 8) & 0xff;
#endif
                            dest16[pos++] = ((r8 >> 3) << 10) | ((g8 >> 3) << 5) | (b8 >> 3);
                        }
                        pos += scaled_border_dest;
                    }
                } else {  // is_24bit
                    int dst_pos = 0;
                    pos = 0;
                    for (int y = 0; y < scaled_height; y++) {
                        for (int x = 0; x < scaled_width; x++) {
                            const uint8_t* src_pixel = reinterpret_cast<const uint8_t*>(&dst32[dst_pos++]);
                            dest[pos] = src_pixel[0];
                            dest[pos+1] = src_pixel[1];
                            dest[pos+2] = src_pixel[2];
                            pos += 3;
                        }
                    }
                }

                // Restore original shift values
                systemRedShift = saved_red_shift;
                systemGreenShift = saved_green_shift;
                systemBlueShift = saved_blue_shift;
                RGB_LOW_BITS_MASK = saved_rgb_low_bits_mask;
            }

            if (nthreads_ == 1) {
                return 0;
            }

            done_->Post();
        }

#ifdef VBAM_RPI_PROXY_SUPPORT
        // Stop the proxy thread when this filter thread is done
        if (proxy_thread_started_) {
            rpi_proxy_client_->StopThread(static_cast<uint32_t>(threadno_));
            proxy_thread_started_ = false;
        }
#endif

        return 0;
    }

private:
    // Optimized interframe blending filter with cached options
    void ApplyInterframeOptimized(int instride, int procy, config::Interframe ifb_option,
                                   bool is_8bit, bool is_16bit, bool is_24bit) {
        switch (ifb_option) {
            case config::Interframe::kNone:
                break;
            case config::Interframe::kSmart:
                if (is_8bit) {
                    SmartIB8(src_, instride, width_, procy, height_);
                } else if (is_16bit) {
                    SmartIB(src_, instride, width_, procy, height_);
                } else if (is_24bit) {
                    SmartIB24(src_, instride, width_, procy, height_);
                } else {
                    SmartIB32(src_, instride, width_, procy, height_);
                }
                break;
            case config::Interframe::kMotionBlur:
                if (is_8bit) {
                    MotionBlurIB8(src_, instride, width_, procy, height_);
                } else if (is_16bit) {
                    MotionBlurIB(src_, instride, width_, procy, height_);
                } else if (is_24bit) {
                    MotionBlurIB24(src_, instride, width_, procy, height_);
                } else {
                    MotionBlurIB32(src_, instride, width_, procy, height_);
                }
                break;
            case config::Interframe::kLast:
                VBAM_NOTREACHED();
        }
    }

    // Optimized filter application with cached option
    void ApplyFilterOptimized(int instride, int outstride, config::Filter filter_option) {
        if (filter_option == config::Filter::kPlugin) {
            // Plugin filters require special handling
            RENDER_PLUGIN_OUTP outdesc;
            outdesc.Size = sizeof(outdesc);
            outdesc.Flags = rpi_->Flags;
            // src_ points to the start of the row (including left border pixel).
            // Skip the left border pixel (1 pixel = rpi_bpp_ bytes) to point to first data pixel.
            // Use the cached rpi_bpp_ instead of systemColorDepth >> 3 to avoid global state issues.
            const int bpp = rpi_bpp_;
            outdesc.SrcPtr = src_ + bpp;  // Skip left border pixel
            // Compute the correct instride for plugin mode based on rpi_bpp_.
            // The precomputed instride may be wrong if systemColorDepth was different when Entry() started.
            // For RPI plugins: inrb = 1 (32-bit) or 2 (16-bit) based on bytes per pixel.
            const int plugin_inrb = (bpp == 4) ? 1 : 2;
            const int plugin_instride = (width_ + plugin_inrb) * bpp;
            outdesc.SrcPitch = plugin_instride;
            outdesc.SrcW = width_;
            outdesc.SrcH = height_;
            // Plugin output data doesn't include borders.
            // The dst_ buffer has borders, so we need to offset by left border size.
            // Left border bytes = (outstride - data_bytes) / 2
            const int scale_int = static_cast<int>(scale_);
            const int data_bytes_per_row = width_ * bpp * scale_int;
            const int left_border_bytes = (outstride - data_bytes_per_row) / 2;
            outdesc.DstPtr = dst_ + left_border_bytes;  // Skip left border in output
            // Use DstW * bpp as pitch (no border padding) because many plugins don't respect
            // DstPitch and write sequentially based on DstW. The proxy handles stride conversion.
            outdesc.DstPitch = data_bytes_per_row;  // DstW * bytes_per_pixel
            outdesc.DstW = static_cast<int>(width_ * scale_);
            outdesc.DstH = static_cast<int>(height_ * scale_);
            outdesc.OutW = outdesc.DstW;
            outdesc.OutH = outdesc.DstH;

#ifdef VBAM_RPI_PROXY_SUPPORT
            if (rpi_proxy_client_) {
                // Use the proxy client to apply the filter
                // Pass outstride as actualDstPitch so proxy can handle stride conversion
                rpi_proxy_client_->ApplyFilter(&outdesc, static_cast<uint32_t>(threadno_), outstride);
            } else
#endif
            {
                rpi_->Output(&outdesc);
            }
        } else {
            ApplyFilter32(src_, instride, delta_, dst_, outstride, width_, height_);
        }
    }
};

void DrawingPanelBase::DrawArea(uint8_t** data)
{
    // double-buffer buffer:
    //   if filtering, this is filter output, retained for redraws
    //   if not filtering, we still retain current image for redraws
    int outbpp = panel_color_depth_ >> 3;
    int inrb = (panel_color_depth_ == 8) ? 4 : (panel_color_depth_ == 16) ? 2 : (panel_color_depth_ == 24) ? 0 : 1;
    int outstride = std::ceil((width + inrb) * outbpp * scale);

    static int drawarea_count = 0;
    if (drawarea_count < 20) {
        wxLogDebug("DrawArea called (count=%d, data=%p, pixbuf2=%p)",
            drawarea_count, data, pixbuf2);
        drawarea_count++;
    }

    if (!pixbuf2) {
        int allocstride = outstride, alloch = height;

        // gb may write borders, so allocate enough for them
        if (width == GameArea::GBWidth && height == GameArea::GBHeight) {
            allocstride = std::ceil((GameArea::SGBWidth + inrb) * outbpp * scale);
            alloch = GameArea::SGBHeight;
        }

        pixbuf2 = (uint8_t*)calloc(allocstride, std::ceil((alloch + 2) * scale));
    }

    if (OPTION(kDispFilter) == config::Filter::kNone) {
        todraw = *data;
        // *data is assigned below, after old buf has been processed
        pixbuf1 = pixbuf2;
        pixbuf2 = todraw;
    } else
        todraw = pixbuf2;

    // Use multiple threads for filter processing (up to 6)
    const int max_threads = GetMaxFilterThreads();

    // First, apply filters, if applicable, in parallel, if enabled
    // FIXME: && (gopts.ifb != FF_MOTION_BLUR || !renderer_can_motion_blur)
    if (OPTION(kDispFilter) != config::Filter::kNone ||
        OPTION(kDispIFB) != config::Interframe::kNone) {
        if (nthreads != max_threads) {
            if (nthreads) {
                if (nthreads > 1)
                    for (int i = 0; i < nthreads; i++) {
                        threads[i].lock_.Lock();
                        threads[i].src_ = NULL;
                        threads[i].sig_.Signal();
                        threads[i].lock_.Unlock();
                        threads[i].Wait();
                    }

                delete[] threads;
            }

            nthreads = max_threads;
            threads = new FilterThread[nthreads];
            // first time around, no threading in order to avoid
            // static initializer conflicts
            threads[0].threadno_ = 0;
            threads[0].nthreads_ = 1;
            threads[0].width_ = width;
            threads[0].height_ = height;
            threads[0].scale_ = scale;
            threads[0].src_ = *data;
            threads[0].dst_ = todraw;
            threads[0].delta_ = delta;
            threads[0].rpi_ = rpi_;
            threads[0].rpi_bpp_ = rpi_bpp_;
            threads[0].panel_color_depth_ = panel_color_depth_;
#ifdef VBAM_RPI_PROXY_SUPPORT
            threads[0].rpi_proxy_client_ = rpi_proxy_client_;
#endif
            threads[0].Entry();

            // go ahead and start the threads up, though
            if (nthreads > 1) {
                for (int i = 0; i < nthreads; i++) {
                    threads[i].threadno_ = i;
                    threads[i].nthreads_ = nthreads;
                    threads[i].width_ = width;
                    threads[i].height_ = height;
                    threads[i].scale_ = scale;
                    threads[i].dst_ = todraw;
                    threads[i].delta_ = delta;
                    threads[i].rpi_ = rpi_;
                    threads[i].rpi_bpp_ = rpi_bpp_;
                    threads[i].panel_color_depth_ = panel_color_depth_;
#ifdef VBAM_RPI_PROXY_SUPPORT
                    threads[i].rpi_proxy_client_ = rpi_proxy_client_;
#endif
                    threads[i].done_ = &filt_done;
                    threads[i].ready_ = &filt_ready;
                    threads[i].Create();
                    threads[i].Run();
                }
                // Wait for all threads to signal they're ready before returning
                // This prevents lost signals on the next frame
                for (int i = 0; i < nthreads; i++) {
                    filt_ready.Wait();
                }
            }
        } else if (nthreads == 1) {
            threads[0].threadno_ = 0;
            threads[0].nthreads_ = 1;
            threads[0].width_ = width;
            threads[0].height_ = height;
            threads[0].scale_ = scale;
            threads[0].src_ = *data;
            threads[0].dst_ = todraw;
            threads[0].delta_ = delta;
            threads[0].rpi_ = rpi_;
            threads[0].rpi_bpp_ = rpi_bpp_;
            threads[0].panel_color_depth_ = panel_color_depth_;
#ifdef VBAM_RPI_PROXY_SUPPORT
            threads[0].rpi_proxy_client_ = rpi_proxy_client_;
#endif
            threads[0].Entry();
        } else {
            for (int i = 0; i < nthreads; i++) {
                threads[i].lock_.Lock();
                threads[i].src_ = *data;
                threads[i].sig_.Signal();
                threads[i].lock_.Unlock();
            }

            for (int i = 0; i < nthreads; i++)
                filt_done.Wait();

            // Smooth seams between thread regions by re-applying the filter
            // to the boundary rows. This eliminates visual artifacts at thread
            // boundaries that occur because filters read neighboring pixels.
            if (nthreads > 1 && OPTION(kDispFilter) != config::Filter::kNone) {
                // outbpp and inrb already declared in outer scope
                int instride = (width + inrb) * (panel_color_depth_ >> 3);
                int outstride_local = std::ceil((width + inrb) * outbpp * scale);

                // For each seam between threads, re-process a few rows
                // The seam rows in the source are at: height * i / nthreads
                for (int i = 1; i < nthreads; i++) {
                    int seam_row = height * i / nthreads;
                    // Process 5 rows above and 5 rows below the seam (10 total)
                    // to give the filter enough context for smooth blending
                    int start_row = std::max(0, seam_row - 5);
                    int end_row = std::min(height, seam_row + 5);
                    int seam_height = end_row - start_row;

                    if (seam_height <= 0) continue;

                    // Calculate source and destination pointers for this seam region
                    // Source needs 1 row of border above, so start from start_row
                    uint8_t* seam_src = *data + instride * start_row;
                    // Destination position accounts for scale and the 1-row top border
                    uint8_t* seam_dst = todraw + (int)std::ceil(outstride_local * (start_row + 1) * scale);
                    uint8_t* seam_delta = delta + instride * start_row;

                    // Apply the filter to this seam region
                    // The filter functions expect the src pointer to already be advanced past border
                    seam_src += instride;  // Skip the top border row

                    ApplyFilter32(seam_src, instride, seam_delta, seam_dst, outstride_local, width, seam_height);
                }
            }
        }
    }

    // swap buffers now that src has been processed
    if (OPTION(kDispFilter) == config::Filter::kNone) {
        *data = pixbuf1;
    }

    // draw OSD text old-style (directly into output buffer), if needed
    // new style flickers too much, so we'll stick to this for now
    if (wxGetApp().frame->IsFullScreen() || !OPTION(kPrefDisableStatus)) {
        GameArea* panel = wxGetApp().frame->GetPanel();

        int scaled_width = (int)std::ceil(width * scale);
        int scaled_height = (int)std::ceil(height * scale);

        if (panel->osdstat.size()) {
            wxString statText = wxString::FromUTF8(panel->osdstat.c_str());
            // Position closer to top-left for better visibility at native resolution
            // At scale=1.0 (native GBA/GB res): (4, 4) instead of (10, 20)
            int x = (int)std::ceil(4 * scale);
            int y = (int)std::ceil(4 * scale);
            drawTextWx(todraw + outstride * (panel_color_depth_ != 24), outstride,
                x, y, statText, scaled_width, scaled_height, scale, panel_color_depth_);
        }

        if (!panel->osdtext.empty()) {
            if (systemGetClock() - panel->osdtime < OSD_TIME) {
                // Position near bottom of screen, scaled with filter
                // At scale=1.0 (native res): positioned to allow 3 lines of text
                int x = (int)std::ceil(4 * scale);
                int y = scaled_height - (int)std::ceil(44 * scale);
                drawTextWx(todraw + outstride * (panel_color_depth_ != 24), outstride,
                    x, y, panel->osdtext, scaled_width, scaled_height, scale, panel_color_depth_);
            } else
                panel->osdtext.clear();
        }
    }

    // next, draw the frame (queue a PaintEv) Refresh must be used under
    // Wayland or nothing is drawn.
    if (wxGetApp().UsingWayland())
        GetWindow()->Refresh();
    else {
        DrawingPanelBase* panel = wxGetApp().frame->GetPanel()->panel;
        if (panel) {
            wxClientDC dc(panel->GetWindow());
            panel->DrawArea(dc);
        }
    }

    // finally, draw on-screen text using wx method, if possible
    // this method flickers too much right now
    //DrawOSD(dc);
}

void DrawingPanelBase::DrawOSD(wxWindowDC& dc)
{
    // draw OSD message, if available
    // doing this here rather than using drawText() directly into the screen
    // image gives us 2 advantages:
    //   - non-ASCII text in messages
    //   - message is always default font size, regardless of screen stretch
    // and one known disadvantage:
    //   - 3d renderers may overwrite text asynchronously
    // Until I go through the trouble of making this render to a bitmap, and
    // then using the bitmap as a texture for the 3d renderers or rendering
    // directly into the output like DrawText, this is only enabled for
    // non-3d renderers.
    GameArea* panel = wxGetApp().frame->GetPanel();
    dc.SetTextForeground(wxColour(255, 0, 0, 255));
    dc.SetTextBackground(wxColour(0, 0, 0, 0));
    dc.SetUserScale(1.0, 1.0);

    if (panel->osdstat.size())
        dc.DrawText(panel->osdstat, 10, 20);

    if (!panel->osdtext.empty()) {
        if (systemGetClock() - panel->osdtime >= OSD_TIME) {
            panel->osdtext.clear();
            wxGetApp().frame->PopStatusText();
        }
    }

    if (!OPTION(kPrefDisableStatus) && !panel->osdtext.empty()) {
        wxSize asz = dc.GetSize();
        wxString msg = panel->osdtext;
        int lw, lh;
        int mlw = asz.GetWidth() - 20;
        dc.GetTextExtent(msg, &lw, &lh);

        if (lw <= mlw)
            dc.DrawText(msg, 10, asz.GetHeight() - 16 - lh);
        else {
            // Since font is likely proportional, the only way to
            // find amt of text that will fit on a line is to search
            wxArrayInt llen; // length of each line, in chars

            for (size_t off = 0; off < msg.size();) {
// One way would be to bsearch on GetTextExtent() looking for
// best fit.
// Another would be to use GetPartialTextExtents and search
// the resulting array.
// GetPartialTextExtents may be inaccurate, but calling it
// once per line may be more efficient than calling
// GetTextExtent log2(remaining_len) times per line.
#if 1
                wxArrayInt clen;
                dc.GetPartialTextExtents(msg.substr(off), clen);
#endif
                int lenl = 0, lenh = msg.size() - off - 1, len;

                do {
                    len = (lenl + lenh) / 2;
#if 1
                    lw = clen[len];
#else
                    dc.GetTextExtent(msg.substr(off, len), &lw, NULL);
#endif

                    if (lw > mlw)
                        lenh = len - 1;
                    else if (lw < mlw)
                        lenl = len + 1;
                    else
                        break;
                } while (lenh >= lenl);

                if (lw <= mlw)
                    len++;

                off += len;
                llen.push_back(len);
            }

            int nlines = llen.size();
            int cury = asz.GetHeight() - 14 - nlines * (lh + 2);

            for (int off = 0, line = 0; line < nlines; off += llen[line], line++, cury += lh + 2)
                dc.DrawText(msg.substr(off, llen[line]), 10, cury);
        }
    }
}

void DrawingPanelBase::OnSize(wxSizeEvent& ev)
{
    ev.Skip();
}

DrawingPanelBase::~DrawingPanelBase()
{
    // pixbuf1 freed by emulator
    if (pixbuf1 != pixbuf2 && pixbuf2)
    {
        free(pixbuf2);
        pixbuf2 = NULL;
    }
    InterframeCleanup();

    if (nthreads) {
        if (nthreads > 1)
            for (int i = 0; i < nthreads; i++) {
                threads[i].lock_.Lock();
                threads[i].src_ = NULL;
                threads[i].sig_.Signal();
                threads[i].lock_.Unlock();
                threads[i].Wait();
            }

        delete[] threads;
    }

#ifdef VBAM_RPI_PROXY_SUPPORT
    // Unload plugin from proxy client if we were using it
    if (using_rpi_proxy_ && rpi_proxy_client_) {
        rpi_proxy::RpiProxyClient::Log("~DrawingPanelBase: Unloading plugin from proxy");
        rpi_proxy_client_->UnloadPlugin();
        using_rpi_proxy_ = false;
        rpi_proxy_client_ = nullptr;
    }
    rpi_proxy::RpiProxyClient::Log("~DrawingPanelBase: Done");
#endif

    disableKeyboardBackgroundInput();
}


BEGIN_EVENT_TABLE(SDLDrawingPanel, wxPanel)
EVT_PAINT(SDLDrawingPanel::PaintEv)
END_EVENT_TABLE()

SDLDrawingPanel::SDLDrawingPanel(wxWindow* parent, int _width, int _height)
    : DrawingPanel(parent, _width, _height)
{
    memset(delta, 0xff, sizeof(delta));

    // wxImage is 24-bit RGB, so 24-bit is preferred.  Filters require
    // 16 or 32, though
    if (OPTION(kDispFilter) == config::Filter::kNone &&
        OPTION(kDispIFB) == config::Interframe::kNone) {
        // changing from 32 to 24 does not require regenerating color tables
        systemColorDepth = (OPTION(kBitDepth) + 1) << 3;
    }

    DrawingPanelInit();
}

SDLDrawingPanel::~SDLDrawingPanel()
{
    if (did_init)
    {
        if (sdlwindow != NULL)
             SDL_DestroyWindow(sdlwindow);

        if (texture != NULL)
             SDL_DestroyTexture(texture);

        if (renderer != NULL)
             SDL_DestroyRenderer(renderer);

        SDL_QuitSubSystem(SDL_INIT_VIDEO);

        did_init = false;
    }
}

void SDLDrawingPanel::EraseBackground(wxEraseEvent& ev)
{
    (void)ev; // unused params
    // do nothing, do not allow propagation
}

void SDLDrawingPanel::PaintEv(wxPaintEvent& ev)
{
    // FIXME: implement
    if (!did_init) {
        DrawingPanelInit();
    }

    (void)ev; // unused params

    if (!todraw) {
        // since this is set for custom background, not drawing anything
        // will cause garbage to be displayed, so draw a black area
        draw_black_background(GetWindow());
        return;
    }

    if (todraw) {
        DrawArea();
    }
}

void SDLDrawingPanel::DrawingPanelInit()
{
    wxString renderer_name = OPTION(kSDLRenderer);

#ifdef ENABLE_SDL3
    SDL_PropertiesID props = SDL_CreateProperties();
#endif
#ifdef __WXGTK__
    GtkWidget *widget = wxGetApp().frame->GetPanel()->GetHandle();
    gtk_widget_realize(widget);
    XID xid = 0;
    struct wl_surface *wayland_surface = NULL;
    struct wl_display *wayland_display = NULL;

#ifdef ENABLE_SDL3
    if (GDK_IS_WAYLAND_WINDOW(gtk_widget_get_window(widget))) {
        wayland_display = gdk_wayland_display_get_wl_display(gtk_widget_get_display(widget));
        wayland_surface = gdk_wayland_window_get_wl_surface(gtk_widget_get_window(widget));

        if (SDL_SetPointerProperty(SDL_GetGlobalProperties(), SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, wayland_display) == false) {
            wxLogError(_("Failed to set wayland display"));
            return;
        }
    } else {
#endif
        xid = GDK_WINDOW_XID(gtk_widget_get_window(widget));
#ifdef ENABLE_SDL3
    }
#endif

#ifdef ENABLE_SDL3
    if (GDK_IS_WAYLAND_WINDOW(gtk_widget_get_window(widget))) {
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "wayland");
    } else {
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "x11");
    }
#else
    SDL_SetHint(SDL_HINT_VIDEODRIVER, "x11");
#endif
#endif

    DrawingPanel::DrawingPanelInit();

#ifdef ENABLE_SDL3
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) == false) {
#else
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0) {
#endif
        wxLogError(_("Failed to initialize SDL video subsystem"));
        return;
    }

#ifdef ENABLE_SDL3
    if (SDL_WasInit(SDL_INIT_VIDEO) == false) {
        if (SDL_Init(SDL_INIT_VIDEO) == false) {
#else
    if (SDL_WasInit(SDL_INIT_VIDEO) < 0) {
        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
#endif
            wxLogError(_("Failed to initialize SDL video"));
            return;
        }
    }

#ifdef ENABLE_SDL3
#ifdef __WXGTK__
    if (GDK_IS_WAYLAND_WINDOW(gtk_widget_get_window(widget))) {
        if (SDL_SetPointerProperty(props, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, wayland_surface) == false) {
            wxLogError(_("Failed to set wayland surface"));
            return;
        }
    } else {
        if (SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_X11_WINDOW_NUMBER, xid) == false)
#elif defined(__WXMAC__)
        if (SDL_SetPointerProperty(props, SDL_PROP_WINDOW_CREATE_COCOA_VIEW_POINTER, wxGetApp().frame->GetPanel()->GetHandle()) == false)
#elif defined(__WXMSW__)
        if (SDL_SetPointerProperty(props, SDL_PROP_WINDOW_CREATE_WIN32_HWND_POINTER, GetHandle()) == false)
#else
        if (SDL_SetPointerProperty(props, "sdl2-compat.external_window", GetWindow()->GetHandle()) == false)
#endif
        {
            wxLogError(_("Failed to set parent window"));
            return;
        }

#ifdef __WXGTK__
    }
#endif

    if (SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_OPENGL_BOOLEAN, true) == false) {
        wxLogError(_("Failed to set OpenGL properties"));
    }

    // This is necessary for joysticks to work at all with SDL video.
    // We control this ourselves so it does not affect the GUI option.
#ifdef ENABLE_SDL3
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
#endif

    sdlwindow = SDL_CreateWindowWithProperties(props);
    SDL_DestroyProperties(props);

    if (sdlwindow == NULL) {
        wxLogError(_("Failed to create SDL window"));
        return;
    }

    if (OPTION(kSDLRenderer) == wxString("default")) {
        renderer = SDL_CreateRenderer(sdlwindow, NULL);
    } else {
        renderer = SDL_CreateRenderer(sdlwindow, renderer_name.mb_str());

        if (renderer == NULL) {
            wxLogError(_("Renderer creating failed, using default renderer"));
            wxLogDebug(_("SDL Error: %s"), SDL_GetError());

            renderer = SDL_CreateRenderer(sdlwindow, NULL);
        }
    }

    if (renderer == NULL) {
        wxLogError(_("Failed to create SDL renderer"));
        return;
    }

    // Set vsync
    if (SDL_SetRenderVSync(renderer, OPTION(kPrefVsync) ? 1 : 0) == false) {
        wxLogError(_("Failed to set vsync for SDL renderer"));
    }

    renderername = wxString(SDL_GetRendererName(renderer));
    wxLogDebug(_("SDL renderer: %s"), (const char*)renderername.mb_str());
#else
#ifdef __WXGTK__
    sdlwindow = SDL_CreateWindowFrom((void *)xid);
#elif defined(__WXMAC__)
    sdlwindow = SDL_CreateWindowFrom(wxGetApp().frame->GetPanel()->GetHandle());
#else
    sdlwindow = SDL_CreateWindowFrom(GetWindow()->GetHandle());
#endif

    if (sdlwindow == NULL) {
        wxLogError(_("Failed to create SDL window"));
        return;
    }

    int vsync_flag = OPTION(kPrefVsync) ? SDL_RENDERER_PRESENTVSYNC : 0;

    if (OPTION(kSDLRenderer) == wxString("default")) {
        renderer = SDL_CreateRenderer(sdlwindow, -1, vsync_flag);
        wxLogDebug(_("SDL renderer: default"));
    } else {
        for (int i = 0; i < SDL_GetNumRenderDrivers(); i++) {
            wxString renderer_name = OPTION(kSDLRenderer);
            SDL_RendererInfo render_info;

            SDL_GetRenderDriverInfo(i, &render_info);

            if (!strcmp(renderer_name.mb_str(), render_info.name)) {
                if (!strcmp(render_info.name, "software")) {
                    renderer = SDL_CreateRenderer(sdlwindow, i, SDL_RENDERER_SOFTWARE | vsync_flag);
                } else {
                    renderer = SDL_CreateRenderer(sdlwindow, i, SDL_RENDERER_ACCELERATED | vsync_flag);
                }

                wxLogDebug(_("SDL renderer: %s"), render_info.name);
            }
        }

        if (renderer == NULL) {
            wxLogError(_("Renderer creating failed, using default renderer"));
            renderer = SDL_CreateRenderer(sdlwindow, -1, vsync_flag);
        }
    }

    if (renderer == NULL) {
        wxLogError(_("Failed to create SDL renderer"));
        return;
    }

    renderername = OPTION(kSDLRenderer);
#endif

    if (out_8) {
#ifdef ENABLE_SDL3
        if (renderername == wxString("direct3d")) {
            texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB332, SDL_TEXTUREACCESS_STREAMING, (width * scale), (height * scale));
        } else {
            texture = SDL_CreateTexture(renderer, SDL_GetPixelFormatForMasks(8, 0xE0, 0x1C, 0x03, 0x00), 
SDL_TEXTUREACCESS_STREAMING, (width * scale), (height * scale));
        }
#else
        if (renderername == wxString("direct3d")) {
            texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB332, SDL_TEXTUREACCESS_STREAMING, (width * scale), (height * scale));
        } else {
            texture = SDL_CreateTexture(renderer, SDL_MasksToPixelFormatEnum(8, 0xE0, 0x1C, 0x03, 0x00), SDL_TEXTUREACCESS_STREAMING, (width * scale), (height * scale));
        }
#endif
    } else if (out_16) {
#ifdef ENABLE_SDL3
        if (renderername == wxString("direct3d")) {
            texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, (width * scale), (height * scale));
        } else {
            texture = SDL_CreateTexture(renderer, SDL_GetPixelFormatForMasks(16, 0x7C00, 0x03E0, 0x001F, 0x0000), SDL_TEXTUREACCESS_STREAMING, (width * scale), (height * scale));
        }
#else
        if (renderername == wxString("direct3d")) {
            texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, (width * scale), (height * scale));
        } else {
            texture = SDL_CreateTexture(renderer, SDL_MasksToPixelFormatEnum(16, 0x7C00, 0x03E0, 0x001F, 0x0000), SDL_TEXTUREACCESS_STREAMING, (width * scale), (height * scale));
        }
#endif
    } else if (out_24) {
#ifdef ENABLE_SDL3
        if (renderername == wxString("direct3d")) {
            texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, (width * scale), (height * scale));
        } else {
            texture = SDL_CreateTexture(renderer, SDL_GetPixelFormatForMasks(24, 0x000000FF, 0x0000FF00, 0x00FF0000, 0x00000000), SDL_TEXTUREACCESS_STREAMING, (width * scale), (height * scale));
        }
#else
        if (renderername == wxString("direct3d")) {
            texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, (width * scale), (height * scale));
        } else {
            texture = SDL_CreateTexture(renderer, SDL_MasksToPixelFormatEnum(24, 0x000000FF, 0x0000FF00, 0x00FF0000, 0x00000000), SDL_TEXTUREACCESS_STREAMING, (width * scale), (height * scale));
        }
#endif
    } else {
#ifdef ENABLE_SDL3
        if (renderername == wxString("direct3d")) {
            texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, (width * scale), (height * scale));
        } else {
            texture = SDL_CreateTexture(renderer, SDL_GetPixelFormatForMasks(32, 0x000000FF, 0x0000FF00, 0x00FF0000, 0x00000000), SDL_TEXTUREACCESS_STREAMING, (width * scale), (height * scale));
        }
#else
        if (renderername == wxString("direct3d")) {
            texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, (width * scale), (height * scale));
        } else {
            texture = SDL_CreateTexture(renderer, SDL_MasksToPixelFormatEnum(32, 0x000000FF, 0x0000FF00, 0x00FF0000, 0x00000000), SDL_TEXTUREACCESS_STREAMING, (width * scale), (height * scale));
        }
#endif
    }

// Set bilinear or nearest on the texture.
#ifdef ENABLE_SDL3
#ifdef HAVE_SDL3_PIXELART
    SDL_SetTextureScaleMode(texture, OPTION(kDispSDLPixelArt) ? SDL_SCALEMODE_PIXELART : OPTION(kDispBilinear) ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST);
#else
    SDL_SetTextureScaleMode(texture, OPTION(kDispBilinear) ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST);
#endif
#else
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, OPTION(kDispBilinear) ? "1" : "0");
#endif
            
    did_init = true;
}
        
void SDLDrawingPanel::OnSize(wxSizeEvent& ev)
{
    if (todraw) {
        DrawArea();
    }
            
    ev.Skip();
}
        
void SDLDrawingPanel::DrawArea(wxWindowDC& dc)
{
    (void)dc;
    DrawArea();
}

void SDLDrawingPanel::DrawArea()
{
    uint32_t srcPitch = 0;
    uint32_t *todraw_argb = NULL;
    uint16_t *todraw_rgb565 = NULL;

    if (!did_init)
        DrawingPanelInit();

    int inrb = out_8 ? 4 : out_16 ? 2 : out_24 ? 0 : 1;
    if (out_8) {
        srcPitch = std::ceil((width + inrb) * scale * 1);
    } else if (out_16) {
        srcPitch = std::ceil((width + inrb) * scale * 2);
    } else if (out_24) {
        srcPitch = std::ceil(width * scale * 3);
    } else {
        srcPitch = std::ceil((width + inrb) * scale * 4);
    }

    SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0x00);
    SDL_RenderClear(renderer);

    if ((renderername == wxString("direct3d")) && (panel_color_depth_ == 32)) {
        todraw_argb = (uint32_t *)(todraw + srcPitch);

        for (int i = 0; i < (height * scale); i++) {
            for (int j = 0; j < (width * scale); j++) {
                todraw_argb[j + (i * (srcPitch / 4))] = 0xFF000000 | ((todraw_argb[j + (i * (srcPitch / 4))] & 0xFF) << 16) | (todraw_argb[j + (i * (srcPitch / 4))] & 0xFF00) | ((todraw_argb[j + (i * (srcPitch / 4))] & 0xFF0000) >> 16);
            }
        }

        if (texture != NULL)
            SDL_UpdateTexture(texture, NULL, todraw_argb, srcPitch);
    } else if ((renderername == wxString("direct3d")) && (panel_color_depth_ == 16)) {
        todraw_rgb565 = (uint16_t *)(todraw + srcPitch);

        for (int i = 0; i < (height * scale); i++) {
            for (int j = 0; j < (width * scale); j++) {
                todraw_rgb565[j + (i * (srcPitch / 2))] = ((todraw_rgb565[j + (i * (srcPitch / 2))] & 0x7FE0) << 1) | (todraw_rgb565[j + (i * (srcPitch / 2))] & 0x1F);
            }
        }

        if (texture != NULL)
            SDL_UpdateTexture(texture, NULL, todraw_rgb565, srcPitch);
    } else {
        if (texture != NULL)
            SDL_UpdateTexture(texture, NULL, todraw + srcPitch, srcPitch);
    }

    if (texture != NULL)
#ifdef ENABLE_SDL3
        SDL_RenderTexture(renderer, texture, NULL, NULL);
#else
        SDL_RenderCopy(renderer, texture, NULL, NULL);
#endif
    
    SDL_RenderPresent(renderer);
}
        
void SDLDrawingPanel::DrawArea(uint8_t** data)
{
    // double-buffer buffer:
    //   if filtering, this is filter output, retained for redraws
    //   if not filtering, we still retain current image for redraws
    int outbpp = panel_color_depth_ >> 3;
    int inrb = (panel_color_depth_ == 8) ? 4 : (panel_color_depth_ == 16) ? 2 : (panel_color_depth_ == 24) ? 0 : 1;
    int outstride = std::ceil((width + inrb) * outbpp * scale);

    // Use multiple threads for filter processing (up to 6)
    const int max_threads = GetMaxFilterThreads();

    if (!pixbuf2) {
        int allocstride = outstride, alloch = height;

        // gb may write borders, so allocate enough for them
        if (width == GameArea::GBWidth && height == GameArea::GBHeight) {
            allocstride = std::ceil((GameArea::SGBWidth + inrb) * outbpp * scale);
            alloch = GameArea::SGBHeight;
        }

        pixbuf2 = (uint8_t*)calloc(allocstride, std::ceil((alloch + 2) * scale));
    }

    if (OPTION(kDispFilter) == config::Filter::kNone) {
        todraw = *data;
        // *data is assigned below, after old buf has been processed
        pixbuf1 = pixbuf2;
        pixbuf2 = todraw;
    } else
        todraw = pixbuf2;

    // First, apply filters, if applicable, in parallel, if enabled
    // FIXME: && (gopts.ifb != FF_MOTION_BLUR || !renderer_can_motion_blur)
    if (OPTION(kDispFilter) != config::Filter::kNone ||
        OPTION(kDispIFB) != config::Interframe::kNone) {
        if (nthreads != max_threads) {
            if (nthreads) {
                if (nthreads > 1)
                    for (int i = 0; i < nthreads; i++) {
                        threads[i].lock_.Lock();
                        threads[i].src_ = NULL;
                        threads[i].sig_.Signal();
                        threads[i].lock_.Unlock();
                        threads[i].Wait();
                    }

                delete[] threads;
            }

            nthreads = max_threads;
            threads = new FilterThread[nthreads];
            // first time around, no threading in order to avoid
            // static initializer conflicts
            threads[0].threadno_ = 0;
            threads[0].nthreads_ = 1;
            threads[0].width_ = width;
            threads[0].height_ = height;
            threads[0].scale_ = scale;
            threads[0].src_ = *data;
            threads[0].dst_ = todraw;
            threads[0].delta_ = delta;
            threads[0].rpi_ = rpi_;
            threads[0].rpi_bpp_ = rpi_bpp_;
            threads[0].panel_color_depth_ = panel_color_depth_;
#ifdef VBAM_RPI_PROXY_SUPPORT
            threads[0].rpi_proxy_client_ = rpi_proxy_client_;
#endif
            threads[0].Entry();

            // go ahead and start the threads up, though
            if (nthreads > 1) {
                for (int i = 0; i < nthreads; i++) {
                    threads[i].threadno_ = i;
                    threads[i].nthreads_ = nthreads;
                    threads[i].width_ = width;
                    threads[i].height_ = height;
                    threads[i].scale_ = scale;
                    threads[i].dst_ = todraw;
                    threads[i].delta_ = delta;
                    threads[i].rpi_ = rpi_;
                    threads[i].rpi_bpp_ = rpi_bpp_;
                    threads[i].panel_color_depth_ = panel_color_depth_;
#ifdef VBAM_RPI_PROXY_SUPPORT
                    threads[i].rpi_proxy_client_ = rpi_proxy_client_;
#endif
                    threads[i].done_ = &filt_done;
                    threads[i].ready_ = &filt_ready;
                    threads[i].Create();
                    threads[i].Run();
                }
                // Wait for all threads to signal they're ready before returning
                for (int i = 0; i < nthreads; i++) {
                    filt_ready.Wait();
                }
            }
        } else if (nthreads == 1) {
            threads[0].threadno_ = 0;
            threads[0].nthreads_ = 1;
            threads[0].width_ = width;
            threads[0].height_ = height;
            threads[0].scale_ = scale;
            threads[0].src_ = *data;
            threads[0].dst_ = todraw;
            threads[0].delta_ = delta;
            threads[0].rpi_ = rpi_;
            threads[0].rpi_bpp_ = rpi_bpp_;
            threads[0].panel_color_depth_ = panel_color_depth_;
#ifdef VBAM_RPI_PROXY_SUPPORT
            threads[0].rpi_proxy_client_ = rpi_proxy_client_;
#endif
            threads[0].Entry();
        } else {
            for (int i = 0; i < nthreads; i++) {
                threads[i].lock_.Lock();
                threads[i].src_ = *data;
                threads[i].sig_.Signal();
                threads[i].lock_.Unlock();
            }

            for (int i = 0; i < nthreads; i++)
                filt_done.Wait();

            // Smooth seams between thread regions by re-applying the filter
            // to the boundary rows. This eliminates visual artifacts at thread
            // boundaries that occur because filters read neighboring pixels.
            if (nthreads > 1 && OPTION(kDispFilter) != config::Filter::kNone) {
                int instride = (width + inrb) * (panel_color_depth_ >> 3);
                int outstride_local = std::ceil((width + inrb) * outbpp * scale);

                // For each seam between threads, re-process a few rows
                // The seam rows in the source are at: height * i / nthreads
                for (int i = 1; i < nthreads; i++) {
                    int seam_row = height * i / nthreads;
                    // Process 5 rows above and 5 rows below the seam (10 total)
                    // to give the filter enough context for smooth blending
                    int start_row = std::max(0, seam_row - 5);
                    int end_row = std::min(height, seam_row + 5);
                    int seam_height = end_row - start_row;

                    if (seam_height <= 0) continue;

                    // Calculate source and destination pointers for this seam region
                    uint8_t* seam_src = *data + instride * start_row;
                    uint8_t* seam_dst = todraw + (int)std::ceil(outstride_local * (start_row + 1) * scale);
                    uint8_t* seam_delta = delta + instride * start_row;

                    // Apply the filter to this seam region
                    seam_src += instride;  // Skip the top border row

                    ApplyFilter32(seam_src, instride, seam_delta, seam_dst, outstride_local, width, seam_height);
                }
            }
        }
    }

    // swap buffers now that src has been processed
    if (OPTION(kDispFilter) == config::Filter::kNone) {
        *data = pixbuf1;
    }

    // draw OSD text old-style (directly into output buffer), if needed
    // new style flickers too much, so we'll stick to this for now
    if (wxGetApp().frame->IsFullScreen() || !OPTION(kPrefDisableStatus)) {
        GameArea* panel = wxGetApp().frame->GetPanel();

        int scaled_width = (int)std::ceil(width * scale);
        int scaled_height = (int)std::ceil(height * scale);

        if (panel->osdstat.size()) {
            wxString statText = wxString::FromUTF8(panel->osdstat.c_str());
            // Position closer to top-left for better visibility at native resolution
            // At scale=1.0 (native GBA/GB res): (4, 4) instead of (10, 20)
            int x = (int)std::ceil(4 * scale);
            int y = (int)std::ceil(4 * scale);
            drawTextWx(todraw + outstride * (panel_color_depth_ != 24), outstride,
                x, y, statText, scaled_width, scaled_height, scale, panel_color_depth_);
        }

        if (!panel->osdtext.empty()) {
            if (systemGetClock() - panel->osdtime < OSD_TIME) {
                // Position near bottom of screen, scaled with filter
                // At scale=1.0 (native res): positioned to allow 3 lines of text
                int x = (int)std::ceil(4 * scale);
                int y = scaled_height - (int)std::ceil(44 * scale);
                drawTextWx(todraw + outstride * (panel_color_depth_ != 24), outstride,
                    x, y, panel->osdtext, scaled_width, scaled_height, scale, panel_color_depth_);
            } else
                panel->osdtext.clear();
        }
    }
            
    // Draw the current frame
    DrawArea();
}
        
BasicDrawingPanel::BasicDrawingPanel(wxWindow* parent, int _width, int _height)
        : DrawingPanel(parent, _width, _height)
{
    // wxImage is 24-bit RGB, so 24-bit is preferred.  Filters require
    // 16 or 32, though
    if (OPTION(kDispFilter) == config::Filter::kNone &&
        OPTION(kDispIFB) == config::Interframe::kNone) {
        // changing from 32 to 24 does not require regenerating color tables
        systemColorDepth = (OPTION(kBitDepth) + 1) << 3;
    }
            
    DrawingPanelInit();
}
        
void BasicDrawingPanel::DrawArea(wxWindowDC& dc)
{
    wxImage* im;

    if (out_24 && OPTION(kDispFilter) == config::Filter::kNone) {
        // never scaled, no borders, no transformations needed
        im = new wxImage(width, height, todraw, true);
    } else if (out_24) {
        // 24-bit with filter: scaled by filters, no color transform needed
        int scaled_width = (int)std::ceil(width * scale);
        int scaled_height = (int)std::ceil(height * scale);
        im = new wxImage(scaled_width, scaled_height, false);
        // Skip 1 row of top border
        uint8_t* src = todraw + scaled_width * 3;
        uint8_t* dst = im->GetData();

        for (int y = 0; y < scaled_height; y++) {
            std::memcpy(dst, src, scaled_width * 3);
            dst += scaled_width * 3;
            src += scaled_width * 3;
        }
    } else if (out_8) {
        // scaled by filters, top/right borders, transform to 24-bit
        im = new wxImage(std::ceil(width * scale), std::ceil(height * scale), false);
        int inrb = 4; // 8-bit has 4-pixel filter border
        int scaled_stride = (int)std::ceil((width + inrb) * scale);
        // Skip 1 row of top border
        uint8_t* src = (uint8_t*)todraw + scaled_stride;
        uint8_t* dst = im->GetData();

        for (int y = 0; y < std::ceil(height * scale); y++) {
            for (int x = 0; x < std::ceil(width * scale); x++, src++) {
                // White color fix
                if (*src == 0xff) {
                    *dst++ = 0xff;
                    *dst++ = 0xff;
                    *dst++ = 0xff;
                } else {
                    *dst++ = (((*src >> 5) & 0x7) << 5);
                    *dst++ = (((*src >> 2) & 0x7) << 5);
                    *dst++ = ((*src & 0x3) << 6);
                }
            }

            src += (int)std::ceil(inrb * scale); // skip rhs border (scaled)
        }
    } else if (out_16) {
        // scaled by filters, top/right borders, transform to 24-bit
        im = new wxImage(std::ceil(width * scale), std::ceil(height * scale), false);
        int inrb = 2; // 16-bit has 2-pixel filter border
        int scaled_stride = (int)std::ceil((width + inrb) * scale);
        // Skip 1 row of top border
        uint16_t* src = (uint16_t*)todraw + scaled_stride;
        uint8_t* dst = im->GetData();

        for (int y = 0; y < std::ceil(height * scale); y++) {
            for (int x = 0; x < std::ceil(width * scale); x++, src++) {
                *dst++ = ((*src >> systemRedShift) & 0x1f) << 3;
                *dst++ = ((*src >> systemGreenShift) & 0x1f) << 3;
                *dst++ = ((*src >> systemBlueShift) & 0x1f) << 3;
            }

            src += (int)std::ceil(inrb * scale); // skip rhs border (scaled)
        }
    } else if (OPTION(kDispFilter) != config::Filter::kNone) {
        // scaled by filters, top/right borders, transform to 24-bit
        im = new wxImage(std::ceil(width * scale), std::ceil(height * scale), false);
        int inrb = 1; // 32-bit has 1-pixel filter border
        int scaled_stride = (int)std::ceil((width + inrb) * scale);
        // Skip 1 row of top border
        uint32_t* src = (uint32_t*)todraw + scaled_stride;
        uint8_t* dst = im->GetData();

        for (int y = 0; y < std::ceil(height * scale); y++) {
            for (int x = 0; x < std::ceil(width * scale); x++, src++) {
                *dst++ = *src >> (systemRedShift - 3);
                *dst++ = *src >> (systemGreenShift - 3);
                *dst++ = *src >> (systemBlueShift - 3);
            }

            src += (int)std::ceil(inrb * scale); // skip rhs border (scaled)
        }
    } else {  // 32 bit
        // not scaled by filters, top/right borders, transform to 24-bit
        im = new wxImage(std::ceil(width * scale), std::ceil(height * scale), false);
        uint32_t* src = (uint32_t*)todraw + (int)std::ceil((width + 1) * scale); // skip top border
        uint8_t* dst = im->GetData();

        for (int y = 0; y < std::ceil(height * scale); y++) {
            for (int x = 0; x < std::ceil(width * scale); x++, src++) {
                *dst++ = *src >> (systemRedShift - 3);
                *dst++ = *src >> (systemGreenShift - 3);
                *dst++ = *src >> (systemBlueShift - 3);
            }
                    
            ++src; // skip rhs border
        }
    }
            
    DrawImage(dc, im);
            
    delete im;
}
        
void BasicDrawingPanel::DrawImage(wxWindowDC& dc, wxImage* im)
{
    double sx, sy;
    int w, h;
    GetClientSize(&w, &h);
    sx = w / (width * scale);
    sy = h / (height * scale);
    dc.SetUserScale(sx, sy);
    wxBitmap bm(*im);
    dc.DrawBitmap(bm, 0, 0);
}
        
#ifndef NO_OGL
// following 3 for vsync
#ifdef __WXMAC__
#include <OpenGL/OpenGL.h>
#endif
#ifdef __WXGTK__ // should actually check for X11, but GTK implies X11
#ifndef Status
#define Status int
#endif
#include <GL/glx.h>
#endif
#ifdef __WXMSW__
#include <GL/gl.h>
#include <GL/glext.h>
#endif
        
// This is supposed to be the default, but DOUBLEBUFFER doesn't seem to be
// turned on by default for wxGTK.
static int glopts[] = {
    WX_GL_RGBA, WX_GL_DOUBLEBUFFER, 0
};

bool GLDrawingPanel::SetContext()
{
#ifndef wxGL_IMPLICIT_CONTEXT
    // Check if the current context is valid
    if (!ctx
#if wxCHECK_VERSION(3, 1, 0)
        || !ctx->IsOK()
#endif
        )
    {
        // Delete the old context
        if (ctx) {
            delete ctx;
            ctx = NULL;
        }
                
        // Create a new context
        ctx = new wxGLContext(this);
        DrawingPanelInit();
    }
    return wxGLCanvas::SetCurrent(*ctx);
#else
    return wxGLContext::SetCurrent(*this);
#endif
}
        
GLDrawingPanel::GLDrawingPanel(wxWindow* parent, int _width, int _height)
        : DrawingPanelBase(_width, _height)
        , wxglc(parent, wxID_ANY, glopts, wxPoint(0, 0), parent->GetClientSize(),
                wxFULL_REPAINT_ON_RESIZE | wxWANTS_CHARS)
{
    widgets::RequestHighResolutionOpenGlSurfaceForWindow(this);
    SetContext();
}
        
GLDrawingPanel::~GLDrawingPanel()
{
    // this should be automatically deleted w/ context
    // it's also unsafe if panel no longer displayed
    if (did_init)
    {
        SetContext();
        glDeleteLists(vlist, 1);
        glDeleteTextures(1, &texid);
    }
            
#ifndef wxGL_IMPLICIT_CONTEXT
    delete ctx;
#endif
}
        
void GLDrawingPanel::DrawingPanelInit()
{
    SetContext();

    DrawingPanelBase::DrawingPanelInit();
            
    AdjustViewport();
            
    const bool bilinear = OPTION(kDispBilinear);
            
    // taken from GTK front end almost verbatim
    glDisable(GL_CULL_FACE);
    glEnable(GL_TEXTURE_2D);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, 1.0, 1.0, 0.0, 0.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    vlist = glGenLists(1);
    glNewList(vlist, GL_COMPILE);
    glBegin(GL_TRIANGLE_STRIP);
    glTexCoord2f(0.0, 0.0);
    glVertex3i(0, 0, 0);
    glTexCoord2f(1.0, 0.0);
    glVertex3i(1, 0, 0);
    glTexCoord2f(0.0, 1.0);
    glVertex3i(0, 1, 0);
    glTexCoord2f(1.0, 1.0);
    glVertex3i(1, 1, 0);
    glEnd();
    glEndList();
    glGenTextures(1, &texid);
    glBindTexture(GL_TEXTURE_2D, texid);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                    bilinear ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                    bilinear ? GL_LINEAR : GL_NEAREST);
            
#define int_fmt out_16 ? GL_RGB5 : GL_RGB
#define tex_fmt out_8  ? GL_RGB : \
    out_16 ? GL_BGRA : \
    out_24 ? GL_RGB : GL_RGBA, \
    out_8 ? GL_UNSIGNED_BYTE_3_3_2 : \
    out_16 ? GL_UNSIGNED_SHORT_1_5_5_5_REV : GL_UNSIGNED_BYTE
#if 0
    texsize = width > height ? width : height;
    texsize = std::ceil(texsize * scale);
    // texsize = 1 << ffs(texsize);
    texsize = texsize | (texsize >> 1);
    texsize = texsize | (texsize >> 2);
    texsize = texsize | (texsize >> 4);
    texsize = texsize | (texsize >> 8);
    texsize = (texsize >> 1) + 1;
    glTexImage2D(GL_TEXTURE_2D, 0, int_fmt, texsize, texsize, 0, tex_fmt, NULL);
#else
    // but really, most cards support non-p2 and rect
    // if not, use cairo or wx renderer
    glTexImage2D(GL_TEXTURE_2D, 0, int_fmt, std::ceil(width * scale), std::ceil(height * scale), 0, tex_fmt, NULL);
#endif
    glClearColor(0.0, 0.0, 0.0, 1.0);
    // non-portable vsync code
#if defined(__WXGTK__)
    if (IsWayland()) {
#ifdef HAVE_EGL
        if (OPTION(kPrefVsync))
            wxLogDebug(_("Enabling EGL VSync."));
        else
            wxLogDebug(_("Disabling EGL VSync."));
                
        eglSwapInterval(0, OPTION(kPrefVsync));
#endif
    }
    else {
        if (OPTION(kPrefVsync))
            wxLogDebug(_("Enabling GLX VSync."));
        else
            wxLogDebug(_("Disabling GLX VSync."));
                
        static PFNGLXSWAPINTERVALEXTPROC glXSwapIntervalEXT = NULL;
        static PFNGLXSWAPINTERVALSGIPROC glXSwapIntervalSGI = NULL;
        static PFNGLXSWAPINTERVALMESAPROC glXSwapIntervalMESA = NULL;

        auto display        = GetX11Display();
        auto default_screen = DefaultScreen(display);
                
        char* glxQuery = (char*)glXQueryExtensionsString(display, default_screen);
                
        if (strstr(glxQuery, "GLX_EXT_swap_control") != NULL)
        {
            glXSwapIntervalEXT = reinterpret_cast<PFNGLXSWAPINTERVALEXTPROC>(glXGetProcAddress((const GLubyte*)"glXSwapIntervalEXT"));
            if (glXSwapIntervalEXT)
                glXSwapIntervalEXT(glXGetCurrentDisplay(),
                                    glXGetCurrentDrawable(), OPTION(kPrefVsync));
            else
                wxLogError(_("Failed to set glXSwapIntervalEXT"));
        }
        if (strstr(glxQuery, "GLX_SGI_swap_control") != NULL)
        {
            glXSwapIntervalSGI = reinterpret_cast<PFNGLXSWAPINTERVALSGIPROC>(glXGetProcAddress((const GLubyte*)("glXSwapIntervalSGI")));
                    
            if (glXSwapIntervalSGI)
                glXSwapIntervalSGI(OPTION(kPrefVsync));
            else
                wxLogError(_("Failed to set glXSwapIntervalSGI"));
        }
        if (strstr(glxQuery, "GLX_MESA_swap_control") != NULL)
        {
            glXSwapIntervalMESA = reinterpret_cast<PFNGLXSWAPINTERVALMESAPROC>(glXGetProcAddress((const GLubyte*)("glXSwapIntervalMESA")));
                    
            if (glXSwapIntervalMESA)
                glXSwapIntervalMESA(OPTION(kPrefVsync));
            else
                wxLogError(_("Failed to set glXSwapIntervalMESA"));
        }
    }
#elif defined(__WXMSW__)
    typedef const char* (*wglext)();
    wglext wglGetExtensionsStringEXT = reinterpret_cast<wglext>(reinterpret_cast<void*>(wglGetProcAddress("wglGetExtensionsStringEXT")));
    if (wglGetExtensionsStringEXT == NULL) {
        wxLogError(_("No support for wglGetExtensionsStringEXT"));
    }
    else if (strstr(wglGetExtensionsStringEXT(), "WGL_EXT_swap_control") == 0) {
        wxLogError(_("No support for WGL_EXT_swap_control"));
    }
            
    typedef BOOL (__stdcall *PFNWGLSWAPINTERVALEXTPROC)(BOOL);
    static PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT = NULL;
    wglSwapIntervalEXT = reinterpret_cast<PFNWGLSWAPINTERVALEXTPROC>(reinterpret_cast<void*>(wglGetProcAddress("wglSwapIntervalEXT")));
    if (wglSwapIntervalEXT)
        wglSwapIntervalEXT(OPTION(kPrefVsync));
    else
        wxLogError(_("Failed to set wglSwapIntervalEXT"));
#elif defined(__WXMAC__)
    int swap_interval = OPTION(kPrefVsync) ? 1 : 0;
    CGLContextObj cgl_context = CGLGetCurrentContext();
    CGLSetParameter(cgl_context, kCGLCPSwapInterval, &swap_interval);
#else
    systemScreenMessage(_("No VSYNC available on this platform"));
#endif
}
        
void GLDrawingPanel::OnSize(wxSizeEvent& ev)
{
    AdjustViewport();
            
    // Temporary hack to backport 800d6ed69b from wxWidgets until 3.2.2 is released.
    if (IsWayland())
        MoveWaylandSubsurface(this);

    ev.Skip();
}
        
void GLDrawingPanel::AdjustViewport()
{
    SetContext();
            
    int x, y;
    widgets::GetRealPixelClientSize(this, &x, &y);
    glViewport(0, 0, x, y);
}
        
void GLDrawingPanel::RefreshGL()
{
    SetContext();
            
    // Rebind any textures or other OpenGL resources here
            
    glBindTexture(GL_TEXTURE_2D, texid);
}
        
void GLDrawingPanel::DrawArea(wxWindowDC& dc)
{
    (void)dc; // unused params
    SetContext();
    RefreshGL();

    if (!did_init)
        DrawingPanelInit();

    if (todraw) {
        // Calculate inrb based on panel's color depth, not global systemColorDepth
        int inrb = (panel_color_depth_ == 8) ? 4 : (panel_color_depth_ == 16) ? 2 : (panel_color_depth_ == 24) ? 0 : 1;
        int rowlen = std::ceil((width + inrb) * scale);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, rowlen);
#if wxBYTE_ORDER == wxBIG_ENDIAN

                // FIXME: is this necessary?
        if (panel_color_depth_ == 16)
            glPixelStorei(GL_UNPACK_SWAP_BYTES, GL_TRUE);

#endif
        int tex_width = (int)std::ceil(width * scale);
        int tex_height = (int)std::ceil(height * scale);
        int offset = (int)std::ceil(rowlen * (panel_color_depth_ >> 3));
        uint8_t* tex_ptr = todraw + offset;
        glTexImage2D(GL_TEXTURE_2D, 0, int_fmt, tex_width, tex_height,
                     0, tex_fmt, tex_ptr);

        glCallList(vlist);
    } else
        glClear(GL_COLOR_BUFFER_BIT);

    SwapBuffers();
}
        
#endif // GL support
        
#if defined(__WXMSW__) && !defined(NO_D3D)
#define DIRECT3D_VERSION 0x0900
#include <d3d9.h> // main include file
typedef HRESULT (WINAPI *LPDIRECT3DCREATE9EX)(UINT, IDirect3D9Ex**);

DXDrawingPanel::DXDrawingPanel(wxWindow* parent, int _width, int _height)
        : DrawingPanel(parent, _width, _height)
        , d3d(NULL)
        , device(NULL)
        , texture(NULL)
        , texture_width(0)
        , texture_height(0)
        , using_d3d9ex(false)
{
    HRESULT hr = E_FAIL; // Initialize hr to a failure value

    // Try to create Direct3D 9Ex interface (Vista+) via dynamic loading
    // This is required to support Windows XP, which doesn't export Direct3DCreate9Ex
    HMODULE hD3D9 = LoadLibrary(TEXT("d3d9.dll"));

    if (hD3D9) {
        // Attempt to get the function address
        LPDIRECT3DCREATE9EX Direct3DCreate9ExPtr =
            reinterpret_cast<LPDIRECT3DCREATE9EX>(reinterpret_cast<void*>(GetProcAddress(hD3D9, "Direct3DCreate9Ex")));

        if (Direct3DCreate9ExPtr) {
            // Function found: try to create D3D9Ex
            hr = Direct3DCreate9ExPtr(D3D_SDK_VERSION, (IDirect3D9Ex**)&d3d);

            if (SUCCEEDED(hr)) {
                using_d3d9ex = true;
                wxLogDebug(_("Using Direct3D 9Ex (dynamically loaded)"));
            }
        }
    } else {
        wxLogError(_("Failed to load d3d9.dll"));
        return;
    }

    // If XP, or Direct3DCreate9Ex failed
    if (!d3d) {
        // Fall back to regular Direct3D 9
        d3d = Direct3DCreate9(D3D_SDK_VERSION);
        if (!d3d) {
            wxLogError(_("Failed to create Direct3D 9 interface"));
            return;
        }
        wxLogDebug(_("Using Direct3D 9"));
    }

    // Get display mode
    D3DDISPLAYMODE d3ddm;
    hr = d3d->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &d3ddm);
    if (FAILED(hr)) {
        wxLogError(_("Failed to get adapter display mode: 0x%08X"), hr);
        return;
    }

    // Set up present parameters
    D3DPRESENT_PARAMETERS d3dpp;
    ZeroMemory(&d3dpp, sizeof(d3dpp));
    d3dpp.Windowed = TRUE;
    d3dpp.BackBufferFormat = d3ddm.Format;
    d3dpp.BackBufferWidth = 0;  // Use window size
    d3dpp.BackBufferHeight = 0; // Use window size
    d3dpp.PresentationInterval = OPTION(kPrefVsync) ?
                                  D3DPRESENT_INTERVAL_ONE :
                                  D3DPRESENT_INTERVAL_IMMEDIATE;

    const wxChar* final_swap_effect = wxT("UNKNOWN"); // Variable to log the successful swap effect

    // Try up to 2 times: FlipEx first (if D3D9Ex), then Discard
    for (int swap_attempt = 0; swap_attempt < 2; swap_attempt++) {
        // First attempt with D3D9Ex: try FlipEx, otherwise use Discard
        if (swap_attempt == 0 && using_d3d9ex) {
            d3dpp.SwapEffect = D3DSWAPEFFECT_FLIPEX;
            d3dpp.BackBufferCount = 2;
        } else {
            if (swap_attempt == 1) wxLogDebug(_("FlipEx not supported, falling back to Discard swap effect"));
            d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
            d3dpp.BackBufferCount = 0;
        }

        // Create Direct3D device - try hardware vertex processing first
        hr = d3d->CreateDevice(
                D3DADAPTER_DEFAULT,
                D3DDEVTYPE_HAL,
                (HWND)GetHandle(),
                D3DCREATE_HARDWARE_VERTEXPROCESSING,
                &d3dpp,
                &device
            );

        if (FAILED(hr)) {
            wxLogDebug(_("Hardware vertex processing not available, falling back to software"));

            // Fallback to software vertex processing
            hr = d3d->CreateDevice(
                    D3DADAPTER_DEFAULT,
                    D3DDEVTYPE_HAL,
                    (HWND)GetHandle(),
                    D3DCREATE_SOFTWARE_VERTEXPROCESSING,
                    &d3dpp,
                    &device
                );
        }

        // If succeeded or not using D3D9Ex, break out
        if (SUCCEEDED(hr) || !using_d3d9ex) {
            if (SUCCEEDED(hr)) {
                // Record the successful swap effect for logging
                final_swap_effect = (d3dpp.SwapEffect == D3DSWAPEFFECT_FLIPEX) ? wxT("FLIPEX") : wxT("DISCARD");
            }
            break;
        }
    }

    if (FAILED(hr)) {
        wxLogError(_("Failed to create Direct3D device: 0x%08X"), hr);
        return;
    }

    // Log the successful device creation along with the final swap effect
    wxLogDebug(_("Direct3D 9%s device created successfully (SwapEffect: %s)"),
               using_d3d9ex ? wxT("Ex") : wxT(""),
               final_swap_effect);
}

DXDrawingPanel::~DXDrawingPanel()
{
    if (texture) {
        texture->Release();
        texture = NULL;
    }
    if (device) {
        device->Release();
        device = NULL;
    }
    if (d3d) {
        d3d->Release();
        d3d = NULL;
    }
}

void DXDrawingPanel::DrawingPanelInit()
{
    DrawingPanelBase::DrawingPanelInit();

    if (!device) {
        return;
    }

    // Calculate texture size (use actual scaled size)
    texture_width = (int)std::ceil(width * scale);
    texture_height = (int)std::ceil(height * scale);

    wxLogDebug(_("DXDrawingPanel initialized: %dx%d (scale: %f)"),
               texture_width, texture_height, scale);
}

bool DXDrawingPanel::ResetDevice()
{
    if (!device || !d3d) {
        return false;
    }

    // Release texture before reset (required for D3DPOOL_DEFAULT resources)
    if (texture) {
        texture->Release();
        texture = NULL;
        texture_width = 0;
        texture_height = 0;
    }

    // Get current display mode
    D3DDISPLAYMODE d3ddm;
    HRESULT hr = d3d->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &d3ddm);
    if (FAILED(hr)) {
        wxLogError(_("Failed to get adapter display mode for reset: 0x%08X"), hr);
        return false;
    }

    // Get current client size
    wxSize client_size = GetClientSize();

    // Set up present parameters
    D3DPRESENT_PARAMETERS d3dpp;
    ZeroMemory(&d3dpp, sizeof(d3dpp));
    d3dpp.Windowed = TRUE;
    d3dpp.BackBufferFormat = d3ddm.Format;
    d3dpp.BackBufferWidth = client_size.GetWidth();
    d3dpp.BackBufferHeight = client_size.GetHeight();
    d3dpp.PresentationInterval = OPTION(kPrefVsync) ?
                                  D3DPRESENT_INTERVAL_ONE :
                                  D3DPRESENT_INTERVAL_IMMEDIATE;

    const wxChar* attempted_swap_effect = wxT("DISCARD"); // Variable to track the attempted/final swap effect

    // Apply SwapEffect logic based on D3D9Ex usage (which was determined in the constructor)
    if (using_d3d9ex) {
        // Try FlipEx first, as it's the preferred mode for D3D9Ex
        d3dpp.SwapEffect = D3DSWAPEFFECT_FLIPEX;
        d3dpp.BackBufferCount = 2;
        attempted_swap_effect = wxT("FLIPEX");
    } else {
        // Fall back to standard Discard for regular D3D9
        d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
        d3dpp.BackBufferCount = 0;
    }

    wxLogDebug(_("Attempting Direct3D device reset (SwapEffect: %s)"), attempted_swap_effect);

    // Reset the device with current window size
    hr = device->Reset(&d3dpp);

    // If Reset with FlipEx fails on D3D9Ex, try Discard as a fallback for the reset
    if (FAILED(hr) && using_d3d9ex && d3dpp.SwapEffect == D3DSWAPEFFECT_FLIPEX) {
        wxLogDebug(_("Reset with FlipEx failed, retrying with Discard swap effect"));
        d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
        d3dpp.BackBufferCount = 0;
        hr = device->Reset(&d3dpp);

        if (SUCCEEDED(hr)) {
            // Update logging variable to reflect the successful fallback
            attempted_swap_effect = wxT("DISCARD (Fallback)");
        }
    }

    if (FAILED(hr)) {
        wxLogError(_("Failed to reset Direct3D device: 0x%08X"), hr);
        return false;
    }

    // Log the successful reset along with the final swap effect used
    wxLogDebug(_("Direct3D device reset successfully (Final SwapEffect: %s)"), attempted_swap_effect);
    return true;
}

void DXDrawingPanel::OnSize(wxSizeEvent& ev)
{
    if (device) {
        // Reset device to apply new back buffer size
        ResetDevice();
    }

    ev.Skip();
}

void DXDrawingPanel::DrawArea(wxWindowDC& dc)
{
    (void)dc; // unused params

    if (!device) {
        return;
    }

    if (!did_init) {
        DrawingPanelInit();
    }

    if (!todraw) {
        // Clear screen if no data to draw
        device->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
        device->Present(NULL, NULL, NULL, NULL);
        return;
    }

    // Determine texture format based on output format
    D3DFORMAT tex_format;
    if (out_16) {
        tex_format = D3DFMT_R5G6B5;
    } else {
        // out_24 or 32-bit - convert to 32-bit XRGB for D3D
        tex_format = D3DFMT_X8R8G8B8;
    }

    int scaled_width = (int)std::ceil(width * scale);
    int scaled_height = (int)std::ceil(height * scale);

    // Create or recreate texture if size changed
    if (!texture || texture_width != scaled_width || texture_height != scaled_height) {
        if (texture) {
            texture->Release();
            texture = NULL;
        }

        HRESULT hr = device->CreateTexture(
            scaled_width,
            scaled_height,
            1,
            D3DUSAGE_DYNAMIC,
            tex_format,
            D3DPOOL_DEFAULT,
            &texture,
            NULL
        );

        if (FAILED(hr)) {
            wxLogError(_("Failed to create texture: 0x%08X"), hr);
            return;
        }

        texture_width = scaled_width;
        texture_height = scaled_height;
    }

    // Lock texture and copy pixel data
    D3DLOCKED_RECT locked_rect;
    HRESULT hr = texture->LockRect(0, &locked_rect, NULL, D3DLOCK_DISCARD);
    if (FAILED(hr)) {
        wxLogError(_("Failed to lock texture: 0x%08X"), hr);
        return;
    }

    // Calculate source pitch (bytes per row)
    // Border is scaled by the filter
    int inrb = out_8 ? 4 : out_16 ? 2 : out_24 ? 0 : 1; // border in pixels
    int scaled_border = (int)std::ceil(inrb * scale);
    int src_pitch = scaled_width;
    if (out_8) {
        src_pitch = (scaled_width + scaled_border) * 1;
    } else if (out_16) {
        src_pitch = (scaled_width + scaled_border) * 2;
    } else if (out_24) {
        src_pitch = scaled_width * 3;
    } else {
        // 32-bit
        src_pitch = (scaled_width + scaled_border) * 4;
    }

    // Copy pixel data
    uint8_t* src = todraw;
    uint8_t* dst = (uint8_t*)locked_rect.pBits;

    if (out_8) {
        // 8-bit palette mode - convert to 32-bit
        // Skip 1 row of top border
        src += src_pitch;
        for (int y = 0; y < scaled_height; y++) {
            uint8_t* src_row = src;
            uint8_t* dst_row = dst;
            for (int x = 0; x < scaled_width; x++) {
                uint8_t palIndex = *src_row++;
                // Simple palette expansion (this may need adjustment)
                if (palIndex == 0xff) {
                    dst_row[0] = dst_row[1] = dst_row[2] = 0xff;
                } else {
                    dst_row[0] = (palIndex & 0x3) << 6;       // B
                    dst_row[1] = ((palIndex >> 2) & 0x7) << 5; // G
                    dst_row[2] = ((palIndex >> 5) & 0x7) << 5; // R
                }
                dst_row[3] = 0;
                dst_row += 4;
            }
            src += src_pitch;
            dst += locked_rect.Pitch;
        }
    } else if (out_16) {
        // Convert 16-bit data from system format (1-5-5-5) to D3D R5G6B5
        // Skip 1 row of top border (pitch in bytes so divide by 2)
        uint16_t* src16 = (uint16_t*)src + src_pitch / 2;
        for (int y = 0; y < scaled_height; y++) {
            uint16_t* src_row = src16;
            uint16_t* dst_row = (uint16_t*)dst;
            for (int x = 0; x < scaled_width; x++, src_row++) {
                // Extract RGB components using system shifts (5 bits each)
                uint16_t pixel = *src_row;
                uint8_t r5 = ((pixel >> systemRedShift) & 0x1f);
                uint8_t g5 = ((pixel >> systemGreenShift) & 0x1f);
                uint8_t b5 = ((pixel >> systemBlueShift) & 0x1f);
                // Expand 5-bit green to 6-bit by duplicating the MSB
                uint8_t g6 = (g5 << 1) | (g5 >> 4);
                // Pack into D3D R5G6B5 format: RRRRRGGGGGGBBBBB
                *dst_row++ = (r5 << 11) | (g6 << 5) | b5;
            }
            src16 += src_pitch / 2; // Move to next row including border (pitch in bytes, divide by 2 for uint16_t)
            dst += locked_rect.Pitch;
        }
    } else if (out_24) {
        // Convert 24-bit RGB to 32-bit XRGB
        // Skip 1 row of top border
        src += scaled_width * 3;
        for (int y = 0; y < scaled_height; y++) {
            uint8_t* src_row = src;
            uint8_t* dst_row = dst;
            for (int x = 0; x < scaled_width; x++) {
                dst_row[0] = src_row[2]; // B
                dst_row[1] = src_row[1]; // G
                dst_row[2] = src_row[0]; // R
                dst_row[3] = 0;          // X
                src_row += 3;
                dst_row += 4;
            }
            src += src_pitch;
            dst += locked_rect.Pitch;
        }
    } else {
        // 32-bit - convert RGBA to BGRX (swap R and B)
        // Skip 1 row of top border
        src += src_pitch;
        for (int y = 0; y < scaled_height; y++) {
            uint8_t* src_row = src;
            uint8_t* dst_row = dst;
            for (int x = 0; x < scaled_width; x++) {
                dst_row[0] = src_row[2]; // B
                dst_row[1] = src_row[1]; // G
                dst_row[2] = src_row[0]; // R
                dst_row[3] = src_row[3]; // A/X
                src_row += 4;
                dst_row += 4;
            }
            src += src_pitch;
            dst += locked_rect.Pitch;
        }
    }

    texture->UnlockRect(0);

    // Begin rendering
    device->BeginScene();

    // Clear background
    device->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);

    // Set texture
    device->SetTexture(0, texture);

    // Set texture filtering
    D3DTEXTUREFILTERTYPE filter = OPTION(kDispBilinear) ?
                                   D3DTEXF_LINEAR : D3DTEXF_POINT;
    device->SetSamplerState(0, D3DSAMP_MAGFILTER, filter);
    device->SetSamplerState(0, D3DSAMP_MINFILTER, filter);

    // Disable lighting
    device->SetRenderState(D3DRS_LIGHTING, FALSE);

    // Set up orthographic projection for 2D rendering
    device->SetFVF(D3DFVF_XYZRHW | D3DFVF_TEX1);

    // Get window client size
    wxSize client_size = GetClientSize();
    float win_width = (float)client_size.GetWidth();
    float win_height = (float)client_size.GetHeight();

    // Calculate dimensions based on aspect ratio setting
    float render_width, render_height;
    float offset_x = 0.0f, offset_y = 0.0f;

    if (OPTION(kDispStretch)) {
        // Retain aspect ratio - add letterboxing
        float tex_aspect = (float)scaled_width / (float)scaled_height;
        float win_aspect = win_width / win_height;

        if (win_aspect > tex_aspect) {
            // Window is wider than texture
            render_height = win_height;
            render_width = win_height * tex_aspect;
            offset_x = (win_width - render_width) / 2.0f;
        } else {
            // Window is taller than texture
            render_width = win_width;
            render_height = win_width / tex_aspect;
            offset_y = (win_height - render_height) / 2.0f;
        }
    } else {
        // Stretch to fill - ignore aspect ratio
        render_width = win_width;
        render_height = win_height;
    }

    // Define vertices for textured quad
    struct Vertex {
        float x, y, z, rhw;
        float u, v;
    };

    Vertex vertices[4] = {
        { offset_x,              offset_y,               0.0f, 1.0f, 0.0f, 0.0f },
        { offset_x + render_width, offset_y,               0.0f, 1.0f, 1.0f, 0.0f },
        { offset_x + render_width, offset_y + render_height, 0.0f, 1.0f, 1.0f, 1.0f },
        { offset_x,              offset_y + render_height, 0.0f, 1.0f, 0.0f, 1.0f }
    };

    // Draw the quad
    device->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, 2, vertices, sizeof(Vertex));

    device->EndScene();

    // Present the frame
    HRESULT present_hr = device->Present(NULL, NULL, NULL, NULL);
    (void)present_hr;
}
#endif
        
#ifndef NO_FFMPEG
static const wxString media_err(recording::MediaRet ret)
{
    switch (ret) {
        case recording::MRET_OK:
            return wxT("");

        case recording::MRET_ERR_NOMEM:
            return _("Memory allocation error");
                    
        case recording::MRET_ERR_NOCODEC:
            return _("Error initializing codec");
                    
        case recording::MRET_ERR_FERR:
            return _("Error writing to output file");
                    
        case recording::MRET_ERR_FMTGUESS:
            return _("Can't guess output format from file name");

        default:
            //    case MRET_ERR_RECORDING:
            //    case MRET_ERR_BUFSIZE:
            return _("Programming error; aborting!");
    }
}
        
void GameArea::StartVidRecording(const wxString& fname)
{
    recording::MediaRet ret;
            
    vid_rec.SetSampleRate(soundGetSampleRate());
    if ((ret = vid_rec.Record(UTF8(fname), basic_width, basic_height,
                                systemColorDepth))
        != recording::MRET_OK)
        wxLogError(_("Unable to begin recording to %s (%s)"), fname.mb_str(),
                    media_err(ret));
    else {
        MainFrame* mf = wxGetApp().frame;
        mf->cmd_enable &= ~(CMDEN_NVREC | CMDEN_NREC_ANY);
        mf->cmd_enable |= CMDEN_VREC;
        mf->enable_menus();
    }
}
        
void GameArea::StopVidRecording()
{
    vid_rec.Stop();
    MainFrame* mf = wxGetApp().frame;
    mf->cmd_enable &= ~CMDEN_VREC;
    mf->cmd_enable |= CMDEN_NVREC;
            
    if (!(mf->cmd_enable & (CMDEN_VREC | CMDEN_SREC)))
        mf->cmd_enable |= CMDEN_NREC_ANY;
            
    mf->enable_menus();
}
        
void GameArea::StartSoundRecording(const wxString& fname)
{
    recording::MediaRet ret;
            
    snd_rec.SetSampleRate(soundGetSampleRate());
    if ((ret = snd_rec.Record(UTF8(fname))) != recording::MRET_OK)
        wxLogError(_("Unable to begin recording to %s (%s)"), fname.mb_str(),
                    media_err(ret));
    else {
        MainFrame* mf = wxGetApp().frame;
        mf->cmd_enable &= ~(CMDEN_NSREC | CMDEN_NREC_ANY);
        mf->cmd_enable |= CMDEN_SREC;
        mf->enable_menus();
    }
}
        
void GameArea::StopSoundRecording()
{
    snd_rec.Stop();
    MainFrame* mf = wxGetApp().frame;
    mf->cmd_enable &= ~CMDEN_SREC;
    mf->cmd_enable |= CMDEN_NSREC;
            
    if (!(mf->cmd_enable & (CMDEN_VREC | CMDEN_SREC)))
        mf->cmd_enable |= CMDEN_NREC_ANY;
            
    mf->enable_menus();
}
        
void GameArea::AddFrame(const uint16_t* data, int length)
{
    recording::MediaRet ret;
            
    if ((ret = vid_rec.AddFrame(data, length)) != recording::MRET_OK) {
        wxLogError(_("Error in audio / video recording (%s); aborting"),
                    media_err(ret));
        vid_rec.Stop();
    }
            
    if ((ret = snd_rec.AddFrame(data, length)) != recording::MRET_OK) {
        wxLogError(_("Error in audio recording (%s); aborting"), media_err(ret));
        snd_rec.Stop();
    }
}
        
void GameArea::AddFrame(const uint8_t* data)
{
    recording::MediaRet ret;
            
    if ((ret = vid_rec.AddFrame(data)) != recording::MRET_OK) {
        wxLogError(_("Error in video recording (%s); aborting"), media_err(ret));
        vid_rec.Stop();
    }
}
#endif
        
void GameArea::MouseEvent(wxMouseEvent& ev)
{
    mouse_active_time = systemGetClock();
            
    wxPoint cur_pos = wxGetMousePosition();
            
    // Ignore small movements.
    if (!ev.Moving() || (std::abs(cur_pos.x - mouse_last_pos.x) >= 11 && std::abs(cur_pos.y - mouse_last_pos.y) >= 11)) {
        ShowPointer();
        ShowMenuBar();
    }
            
    mouse_last_pos = cur_pos;
            
    ev.Skip();
}
        
void GameArea::ShowPointer()
{
    if (!pointer_blanked || fullscreen) return;
            
    pointer_blanked = false;
            
    SetCursor(wxNullCursor);
            
    if (panel)
        panel->GetWindow()->SetCursor(wxNullCursor);
}
        
void GameArea::HidePointer()
{
    if (pointer_blanked || !main_frame) return;
            
    // FIXME: make time configurable
    if ((fullscreen || (systemGetClock() - mouse_active_time) > 3000) &&
        !(main_frame->MenusOpened() || main_frame->DialogOpened())) {
        pointer_blanked = true;
        SetCursor(wxCursor(wxCURSOR_BLANK));
                
        // wxGTK requires that subwindows get the cursor as well
        if (panel)
            panel->GetWindow()->SetCursor(wxCursor(wxCURSOR_BLANK));
    }
}
        
        // We do not hide the menubar on mac, on mac it is not part of the main frame
        // and the user can adjust hiding behavior herself.
void GameArea::HideMenuBar()
{
#ifndef __WXMAC__
    if (!main_frame || menu_bar_hidden || !gopts.hide_menu_bar) return;
            
    if (((systemGetClock() - mouse_active_time) > 3000) && !main_frame->MenusOpened()) {
#ifdef __WXMSW__
        current_hmenu = static_cast<HMENU>(main_frame->GetMenuBar()->GetHMenu());
        ::SetMenu(main_frame->GetHandle(), NULL);
#else
        main_frame->GetMenuBar()->Hide();
#endif
        SendSizeEvent();
        menu_bar_hidden = true;
    }
#endif
}
        
void GameArea::ShowMenuBar()
{
#ifndef __WXMAC__
    if (!main_frame || !menu_bar_hidden) return;
            
#ifdef __WXMSW__
    if (current_hmenu != NULL) {
        ::SetMenu(main_frame->GetHandle(), current_hmenu);
        current_hmenu = NULL;
    }
#else
    main_frame->GetMenuBar()->Show();
#endif
    SendSizeEvent();
    menu_bar_hidden = false;
#endif
}
        
void GameArea::OnGBBorderChanged(config::Option* option) {
    if (game_type() == IMAGE_GB && gbSgbMode) {
        if (option->GetBool()) {
            AddBorder();
            gbSgbRenderBorder();
        } else {
            DelBorder();
        }
    }
}
        
void GameArea::UpdateLcdFilter() {
    int DCCP = 0;

    switch (OPTION(kDispColorCorrectionProfile)) {
        case config::ColorCorrectionProfile::kSRGB:
            DCCP = 0;
            break;

        case config::ColorCorrectionProfile::kDCI:
            DCCP = 1;
            break;

        case config::ColorCorrectionProfile::kRec2020:
            DCCP = 2;
            break;

        case config::ColorCorrectionProfile::kLast:
            DCCP = 0;
            break;
    }

    if (loaded == IMAGE_GBA) {
        gbafilter_set_params(DCCP, (((float)OPTION(kGBADarken)) / 100));
        gbafilter_update_colors(OPTION(kGBALCDFilter));
    } else if (loaded == IMAGE_GB) {
        if (gbHardware & 4) { // Emulated Hardware is SGB
            // Prevent CGB LCD filter from applying
            gbcfilter_update_colors(false);
        } else if (gbHardware & 8) { // Emulated Hardware is GBA
            // Apply GBA LCD filter instead of the GBC LCD Filter
            gbafilter_set_params(DCCP, (((float)OPTION(kGBADarken)) / 100));
            gbafilter_update_colors(OPTION(kGBLCDFilter));
        } else {
            // Apply GBC LCD filter for GB/GBC modes
            gbcfilter_set_params(DCCP, (((float)OPTION(kGBLighten)) / 100));
            gbcfilter_update_colors(OPTION(kGBLCDFilter));
        }
    } else {
        gbafilter_set_params(DCCP, (((float)OPTION(kGBADarken)) / 100));
        gbcfilter_set_params(DCCP, (((float)OPTION(kGBLighten)) / 100));
        gbafilter_update_colors(false);
        gbcfilter_update_colors(false);
    }
}
        
void GameArea::SuspendScreenSaver() {
#ifdef HAVE_XSS
    if (xscreensaver_suspended || !gopts.suspend_screensaver)
        return;
    // suspend screensaver
    if (emulating && !wxGetApp().UsingWayland()) {
        auto display = GetX11Display();
        XScreenSaverSuspend(display, true);
        xscreensaver_suspended = true;
    }
#endif // HAVE_XSS
}
        
void GameArea::UnsuspendScreenSaver() {
#ifdef HAVE_XSS
            // unsuspend screensaver
    if (xscreensaver_suspended) {
        auto display = GetX11Display();
        XScreenSaverSuspend(display, false);
        xscreensaver_suspended = false;
    }
#endif // HAVE_XSS
}
        
void GameArea::OnAudioRateChanged() {
    if (loaded == IMAGE_UNKNOWN) {
        return;
    }
            
    switch (game_type()) {
        case IMAGE_UNKNOWN:
            break;
                    
        case IMAGE_GB:
            gbSoundSetSampleRate(GetSampleRate());
            break;
                    
        case IMAGE_GBA:
            soundSetSampleRate(GetSampleRate());
            break;
    }
}
        
void GameArea::OnVolumeChanged(config::Option* option) {
    const int volume = option->GetInt();
    soundSetVolume((float)volume / 100.0);
    systemScreenMessage(wxString::Format(_("Volume: %d %%"), volume));
}
        
#ifdef __WXMAC__
#ifndef NO_METAL
MetalDrawingPanel::MetalDrawingPanel(wxWindow* parent, int _width, int _height)
        : DrawingPanel(parent, _width, _height)
{
    memset(delta, 0xff, sizeof(delta));

    // wxImage is 24-bit RGB, so 24-bit is preferred.  Filters require
    // 16 or 32, though
    if (OPTION(kDispFilter) == config::Filter::kNone &&
        OPTION(kDispIFB) == config::Interframe::kNone) {
        // changing from 32 to 24 does not require regenerating color tables
        systemColorDepth = (OPTION(kBitDepth) + 1) << 3;
    }

    DrawingPanelInit();
}
        
void MetalDrawingPanel::DrawArea(uint8_t** data)
{
    // double-buffer buffer:
    //   if filtering, this is filter output, retained for redraws
    //   if not filtering, we still retain current image for redraws
    int outbpp = panel_color_depth_ >> 3;
    int inrb = (panel_color_depth_ == 8) ? 4 : (panel_color_depth_ == 16) ? 2 : (panel_color_depth_ == 24) ? 0 : 1;
    int outstride = std::ceil((width + inrb) * outbpp * scale);

    // Use multiple threads for filter processing (up to 6)
    const int max_threads = GetMaxFilterThreads();

    if (!pixbuf2) {
        int allocstride = outstride, alloch = height;

        // gb may write borders, so allocate enough for them
        if (width == GameArea::GBWidth && height == GameArea::GBHeight) {
            allocstride = std::ceil((GameArea::SGBWidth + inrb) * outbpp * scale);
            alloch = GameArea::SGBHeight;
        }

        pixbuf2 = (uint8_t*)calloc(allocstride, std::ceil((alloch + 2) * scale));
    }

    if (OPTION(kDispFilter) == config::Filter::kNone) {
        todraw = *data;
        // *data is assigned below, after old buf has been processed
        pixbuf1 = pixbuf2;
        pixbuf2 = todraw;
    } else
        todraw = pixbuf2;

    // First, apply filters, if applicable, in parallel, if enabled
    // FIXME: && (gopts.ifb != FF_MOTION_BLUR || !renderer_can_motion_blur)
    if (OPTION(kDispFilter) != config::Filter::kNone ||
        OPTION(kDispIFB) != config::Interframe::kNone) {
        if (nthreads != max_threads) {
            if (nthreads) {
                if (nthreads > 1)
                    for (int i = 0; i < nthreads; i++) {
                        threads[i].lock_.Lock();
                        threads[i].src_ = NULL;
                        threads[i].sig_.Signal();
                        threads[i].lock_.Unlock();
                        threads[i].Wait();
                    }

                delete[] threads;
            }

            nthreads = max_threads;
            threads = new FilterThread[nthreads];
            // first time around, no threading in order to avoid
            // static initializer conflicts
            threads[0].threadno_ = 0;
            threads[0].nthreads_ = 1;
            threads[0].width_ = width;
            threads[0].height_ = height;
            threads[0].scale_ = scale;
            threads[0].src_ = *data;
            threads[0].dst_ = todraw;
            threads[0].delta_ = delta;
            threads[0].rpi_ = rpi_;
            threads[0].rpi_bpp_ = rpi_bpp_;
            threads[0].panel_color_depth_ = panel_color_depth_;
#ifdef VBAM_RPI_PROXY_SUPPORT
            threads[0].rpi_proxy_client_ = rpi_proxy_client_;
#endif
            threads[0].Entry();

            // go ahead and start the threads up, though
            if (nthreads > 1) {
                for (int i = 0; i < nthreads; i++) {
                    threads[i].threadno_ = i;
                    threads[i].nthreads_ = nthreads;
                    threads[i].width_ = width;
                    threads[i].height_ = height;
                    threads[i].scale_ = scale;
                    threads[i].dst_ = todraw;
                    threads[i].delta_ = delta;
                    threads[i].rpi_ = rpi_;
                    threads[i].rpi_bpp_ = rpi_bpp_;
                    threads[i].panel_color_depth_ = panel_color_depth_;
#ifdef VBAM_RPI_PROXY_SUPPORT
                    threads[i].rpi_proxy_client_ = rpi_proxy_client_;
#endif
                    threads[i].done_ = &filt_done;
                    threads[i].ready_ = &filt_ready;
                    threads[i].Create();
                    threads[i].Run();
                }
                // Wait for all threads to signal they're ready before returning
                for (int i = 0; i < nthreads; i++) {
                    filt_ready.Wait();
                }
            }
        } else if (nthreads == 1) {
            threads[0].threadno_ = 0;
            threads[0].nthreads_ = 1;
            threads[0].width_ = width;
            threads[0].height_ = height;
            threads[0].scale_ = scale;
            threads[0].src_ = *data;
            threads[0].dst_ = todraw;
            threads[0].delta_ = delta;
            threads[0].rpi_ = rpi_;
            threads[0].rpi_bpp_ = rpi_bpp_;
            threads[0].panel_color_depth_ = panel_color_depth_;
#ifdef VBAM_RPI_PROXY_SUPPORT
            threads[0].rpi_proxy_client_ = rpi_proxy_client_;
#endif
            threads[0].Entry();
        } else {
            for (int i = 0; i < nthreads; i++) {
                threads[i].lock_.Lock();
                threads[i].src_ = *data;
                threads[i].sig_.Signal();
                threads[i].lock_.Unlock();
            }

            for (int i = 0; i < nthreads; i++)
                filt_done.Wait();

            // Smooth seams between thread regions by re-applying the filter
            // to the boundary rows. This eliminates visual artifacts at thread
            // boundaries that occur because filters read neighboring pixels.
            if (nthreads > 1 && OPTION(kDispFilter) != config::Filter::kNone) {
                int instride = (width + inrb) * (panel_color_depth_ >> 3);
                int outstride_local = std::ceil((width + inrb) * outbpp * scale);

                // For each seam between threads, re-process a few rows
                // The seam rows in the source are at: height * i / nthreads
                for (int i = 1; i < nthreads; i++) {
                    int seam_row = height * i / nthreads;
                    // Process 5 rows above and 5 rows below the seam (10 total)
                    // to give the filter enough context for smooth blending
                    int start_row = std::max(0, seam_row - 5);
                    int end_row = std::min(height, seam_row + 5);
                    int seam_height = end_row - start_row;

                    if (seam_height <= 0) continue;

                    // Calculate source and destination pointers for this seam region
                    uint8_t* seam_src = *data + instride * start_row;
                    uint8_t* seam_dst = todraw + (int)std::ceil(outstride_local * (start_row + 1) * scale);
                    uint8_t* seam_delta = delta + instride * start_row;

                    // Apply the filter to this seam region
                    seam_src += instride;  // Skip the top border row

                    ApplyFilter32(seam_src, instride, seam_delta, seam_dst, outstride_local, width, seam_height);
                }
            }
        }
    }

    // swap buffers now that src has been processed
    if (OPTION(kDispFilter) == config::Filter::kNone) {
        *data = pixbuf1;
    }

    // draw OSD text old-style (directly into output buffer), if needed
    // new style flickers too much, so we'll stick to this for now
    if (wxGetApp().frame->IsFullScreen() || !OPTION(kPrefDisableStatus)) {
        GameArea* panel = wxGetApp().frame->GetPanel();

        int scaled_width = (int)std::ceil(width * scale);
        int scaled_height = (int)std::ceil(height * scale);

        if (panel->osdstat.size()) {
            wxString statText = wxString::FromUTF8(panel->osdstat.c_str());
            // Position closer to top-left for better visibility at native resolution
            // At scale=1.0 (native GBA/GB res): (4, 4) instead of (10, 20)
            int x = (int)std::ceil(4 * scale);
            int y = (int)std::ceil(4 * scale);
            drawTextWx(todraw + outstride * (panel_color_depth_ != 24), outstride,
                x, y, statText, scaled_width, scaled_height, scale, panel_color_depth_);
        }

        if (!panel->osdtext.empty()) {
            if (systemGetClock() - panel->osdtime < OSD_TIME) {
                // Position near bottom of screen, scaled with filter
                // At scale=1.0 (native res): positioned to allow 3 lines of text
                int x = (int)std::ceil(4 * scale);
                int y = scaled_height - (int)std::ceil(44 * scale);
                drawTextWx(todraw + outstride * (panel_color_depth_ != 24), outstride,
                    x, y, panel->osdtext, scaled_width, scaled_height, scale, panel_color_depth_);
            } else
                panel->osdtext.clear();
        }
    }

    // Draw the current frame
    DrawArea();
}
        
void MetalDrawingPanel::PaintEv(wxPaintEvent& ev)
{
    // FIXME: implement
    if (!did_init) {
        DrawingPanelInit();
    }

    (void)ev; // unused params
            
    if (!todraw) {
        // since this is set for custom background, not drawing anything
        // will cause garbage to be displayed, so draw a black area
        draw_black_background(GetWindow());
        return;
    }

    if (todraw) {
        DrawArea();
    }
}
#endif
#endif

