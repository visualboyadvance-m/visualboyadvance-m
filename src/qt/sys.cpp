// Core system callbacks (the system* functions the emulator core calls back
// into), game movie recording/playback, the Game Boy printer dialog and the
// GDB stub transports. Ported from src/wx/sys.cpp.

#include "qt/sys.h"

#include "qt/android-compat.h"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include <QComboBox>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QPainter>
#include <QPixmap>
#include <QPrintDialog>
#include <QPrinter>
#include <QPushButton>
#include <QScrollArea>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QtEndian>

#include "core/base/image_util.h"
#include "core/base/sdl_motion.h"
#include "core/gb/gbGlobals.h"
#include "core/gba/gbaGlobals.h"
#include "core/gba/gbaSound.h"
#include "qt/app.h"
#include "qt/audio/audio.h"
#include "qt/config/cmdtab.h"
#include "qt/config/emulated-gamepad.h"
#include "qt/config/option-proxy.h"
#include "qt/drawing-panel.h"
#include "qt/game-area.h"
#include "qt/log.h"
#include "qt/main-window.h"
#include "qt/opts.h"
#include "qt/lua/lua_engine.h"

#define TR(s) QCoreApplication::translate("sys", s)

// These should probably be in vbamcore
int systemVerbose;
int systemFrameSkip;

int systemRedShift;
int systemGreenShift;
int systemBlueShift;
int systemColorDepth;
uint8_t  systemColorMap8[0x10000];
uint16_t systemColorMap16[0x10000];
uint32_t systemColorMap32[0x10000];
#define gs555(x) (x | (x << 5) | (x << 10))
uint16_t systemGbPalette[24] = {
    gs555(0x1f), gs555(0x15), gs555(0x0c), 0,
    gs555(0x1f), gs555(0x15), gs555(0x0c), 0,
    gs555(0x1f), gs555(0x15), gs555(0x0c), 0,
    gs555(0x1f), gs555(0x15), gs555(0x0c), 0,
    gs555(0x1f), gs555(0x15), gs555(0x0c), 0,
    gs555(0x1f), gs555(0x15), gs555(0x0c), 0
};
int RGB_LOW_BITS_MASK;

// these are local, though.
int autofire, autohold;
static int sensorx[4], sensory[4], sensorz[4];
int sunBars = 1;
bool pause_next;
bool turbo;

// and this is from MFC interface
bool soundBufferLow;

namespace {

// True while the GUI is alive: the application object exists and the main
// window has been created and not yet destroyed. Equivalent of the wx port's
// IsMainLoopRunning() checks guarding OSD/status bar access.
bool GuiAlive() {
    return QCoreApplication::instance() != nullptr && vbamApp().frame != nullptr;
}

GameArea* CurrentPanel() {
    if (!GuiAlive())
        return nullptr;
    return vbamApp().frame->GetPanel();
}

}  // namespace

void systemMessage(int id, const char* fmt, ...)
{
    (void)id; // unused params
    static char* buf = nullptr;
    static int buflen = 80;
    va_list args;

    if (!buf) {
        buf = (char*)malloc(buflen);

        if (!buf)
            exit(1);
    }

    while (1) {
        va_start(args, fmt);
        int needsz = vsnprintf(buf, buflen, fmt, args);
        va_end(args);

        if (needsz < buflen)
            break;

        while (buflen <= needsz)
            buflen *= 2;

        free(buf);
        buf = (char*)malloc(buflen);

        if (!buf)
            exit(1);
    }

    vbam::LogError(QString::fromUtf8(buf));
}

void systemSendScreen()
{
#ifndef NO_FFMPEG
    GameArea* ga = CurrentPanel();
    if (ga) ga->AddFrame(g_pix);
#endif
}

static int frames = 0;

void systemDrawScreen()
{
    frames++;
    MainWindow* mf = vbamApp().frame;
    if (!mf)
        return;
    mf->UpdateViewers();
    GameArea* ga = mf->GetPanel();

#ifndef NO_FFMPEG
    if (ga)
        ga->AddFrame(g_pix);
#endif

    // FCEUX-style per-frame Lua hooks (no-ops without VBAM_ENABLE_LUA): run the
    // script's frame step and registerbefore() callbacks, and compose the gui.*
    // overlay into g_pix so every renderer shows it.
    vbam::lua::LuaOnFrameBegin(g_pix, gbRom ? 160 : 240, gbRom ? 144 : 160,
                               (gbRom ? 161 : 241) * (systemColorDepth / 8), systemColorDepth,
                               systemRedShift, systemGreenShift, systemBlueShift);

    if (ga && ga->panel) {
        ga->panel->DrawArea(&g_pix);
    }

    vbam::lua::LuaOnFrameEnd();
}

// record a game "movie"
// actually just game save state combined with a keystroke log
// doesn't work in GB "multiplayer" mode (only records default joypad)
//
//  <name>.vmv = keystroke log; all values little-endian ints:
//     <version>.32 = 1
//     for every joypad change (init to 0) and once at end of movie {
//        <timestamp>.32 = frames since start of movie in version 1 and frames since the previous change in version 2
//        <joypad>.32 = default joypad reading at that time
//     }
//  <name>.vm0 = saved state

struct supportedMovie {
    MVFormatID formatId;
    char const* longName;
    char const* exts;
};

const supportedMovie movieSupportedToRecord[] = {
    { MV_FORMAT_ID_VMV2, "VBA Movie v2, Time Diff Format", "vmv" },
    { MV_FORMAT_ID_VMV1, "VBA Movie v1, Old Version for Compatibility", "vmv" },
};

std::vector<MVFormatID> getSupMovFormatsToRecord()
{
    std::vector<MVFormatID> result;
    for (auto&& fmt: movieSupportedToRecord)
        result.push_back(fmt.formatId);
    return result;
}

std::vector<char*> getSupMovNamesToRecord()
{
    std::vector<char*> result;
    for (auto&& fmt: movieSupportedToRecord)
        result.push_back((char*)fmt.longName);
    return result;
}

std::vector<char*> getSupMovExtsToRecord()
{
    std::vector<char*> result;
    for (auto&& fmt: movieSupportedToRecord)
        result.push_back((char*)fmt.exts);
    return result;
}

const supportedMovie movieSupportedToPlayback[] = {
    { MV_FORMAT_ID_VMV, "VBA Movie", "vmv" },
};

std::vector<MVFormatID> getSupMovFormatsToPlayback()
{
    std::vector<MVFormatID> result;
    for (auto&& fmt: movieSupportedToPlayback)
        result.push_back(fmt.formatId);
    return result;
}

std::vector<char*> getSupMovNamesToPlayback()
{
    std::vector<char*> result;
    for (auto&& fmt: movieSupportedToPlayback)
        result.push_back((char*)fmt.longName);
    return result;
}

std::vector<char*> getSupMovExtsToPlayback()
{
    std::vector<char*> result;
    for (auto&& fmt: movieSupportedToPlayback)
        result.push_back((char*)fmt.exts);
    return result;
}

const MVFormatID VMVFormatVersions[] = {
    MV_FORMAT_ID_VMV,
    MV_FORMAT_ID_VMV1,
    MV_FORMAT_ID_VMV2,
};

enum MVFormatID recording_format;
static QFile game_file;
bool game_recording, game_playback;
uint32_t game_frame;
uint32_t game_joypad;

namespace {

bool WriteU32(QFile& f, uint32_t v) {
    return f.write(reinterpret_cast<const char*>(&v), sizeof(v)) == qint64(sizeof(v));
}

bool ReadU32(QFile& f, uint32_t* v) {
    return f.read(reinterpret_cast<char*>(v), sizeof(*v)) == qint64(sizeof(*v));
}

}  // namespace

void systemStartGameRecording(const QString& fname, MVFormatID format)
{
    GameArea* panel = CurrentPanel();

    if (!panel || panel->game_type() == IMAGE_UNKNOWN || !panel->emusys->emuWriteState) {
        vbam::LogError(TR("No game in progress to record"));
        return;
    }

    systemStopGamePlayback();

    QString fn = fname;
    recording_format = format;

    if (fn.size() < 4 || fn.right(4).compare(QStringLiteral(".vmv"), Qt::CaseInsensitive) != 0)
        fn.append(QStringLiteral(".vmv"));

    uint32_t version = 1;

    if (recording_format == MV_FORMAT_ID_VMV2)
        version = 2;

    game_file.setFileName(fn);
    if (!game_file.open(QIODevice::WriteOnly | QIODevice::Truncate) || !WriteU32(game_file, qToLittleEndian(version))) {
        game_file.close();
        vbam::LogError(TR("Cannot open output file %1").arg(fname));
        return;
    }

    fn[fn.size() - 1] = QLatin1Char('0');

    if (!panel->emusys->emuWriteState(vbam::ToPath(fn).c_str())) {
        vbam::LogError(TR("Error writing game recording"));
        game_file.close();
        return;
    }

    game_frame = 0;
    game_joypad = 0;
    game_recording = true;
    MainWindow* mf = vbamApp().frame;
    mf->cmd_enable &= ~(CMDEN_NGREC | CMDEN_GPLAY | CMDEN_NGPLAY);
    mf->cmd_enable |= CMDEN_GREC;
    mf->enable_menus();
}

QString systemGameRecordingFile()
{
    return game_recording ? game_file.fileName() : QString();
}

void systemStopGameRecording()
{
    if (!game_recording)
        return;

    bool ok = WriteU32(game_file, qToLittleEndian(game_frame)) &&
              WriteU32(game_file, qToLittleEndian(game_joypad));
    game_file.close();
    // Android content:// staging: transfer the finished movie (no-op elsewhere).
    VbamCommitAndroidOutputFile(game_file.fileName());
    if (!ok || game_file.error() != QFile::NoError)
        vbam::LogError(TR("Error writing game recording"));

    game_recording = false;
    MainWindow* mf = vbamApp().frame;
    if (mf) {
        mf->cmd_enable &= ~CMDEN_GREC;
        mf->cmd_enable |= CMDEN_NGREC | CMDEN_NGPLAY;
        mf->enable_menus();
    }
}

uint32_t game_next_frame, game_next_joypad;

void systemStartGamePlayback(const QString& fname, MVFormatID format)
{
    GameArea* panel = CurrentPanel();

    if (!panel || panel->game_type() == IMAGE_UNKNOWN || !panel->emusys->emuReadState) {
        vbam::LogError(TR("No game in progress to record"));
        return;
    }

    if (game_recording) {
        vbam::LogError(TR("Cannot play game recording while recording"));
        return;
    }

    systemStopGamePlayback();

    QString fn = fname;
    recording_format = format;

    if (fn.size() < 4 || fn.right(4).compare(QStringLiteral(".vmv"), Qt::CaseInsensitive) != 0)
        fn.append(QStringLiteral(".vmv"));

    uint32_t version = 0;

    game_file.setFileName(fn);
    if (!game_file.open(QIODevice::ReadOnly) || !ReadU32(game_file, &version) ||
        qFromLittleEndian(version) < 1 || qFromLittleEndian(version) > 2) {
        game_file.close();
        vbam::LogError(TR("Cannot open recording file %1").arg(fname));
        return;
    }
    version = qFromLittleEndian(version);

    recording_format = VMVFormatVersions[version];

    uint32_t gf, jp;

    if (!ReadU32(game_file, &gf) || !ReadU32(game_file, &jp)) {
        vbam::LogError(TR("Error reading game recording"));
        game_file.close();
        return;
    }

    game_next_frame = qFromLittleEndian(gf);
    game_next_joypad = qFromLittleEndian(jp);
    fn[fn.size() - 1] = QLatin1Char('0');

    if (!panel->emusys->emuReadState(vbam::ToPath(fn).c_str())) {
        vbam::LogError(TR("Error reading game recording"));
        game_file.close();
        return;
    }

    game_frame = 0;
    game_joypad = 0;
    game_playback = true;
    MainWindow* mf = vbamApp().frame;
    mf->cmd_enable &= ~(CMDEN_NGREC | CMDEN_GREC | CMDEN_NGPLAY);
    mf->cmd_enable |= CMDEN_GPLAY;
    mf->enable_menus();
}

void systemStopGamePlayback()
{
    if (!game_playback)
        return;

    game_file.close();
    game_playback = false;
    MainWindow* mf = vbamApp().frame;
    if (mf) {
        mf->cmd_enable &= ~CMDEN_GPLAY;
        mf->cmd_enable |= CMDEN_NGREC | CMDEN_NGPLAY;
        mf->enable_menus();
    }
}

// Hidden test hook: VBAM_AUTOINPUT="<frame>:<keys>,<frame>:<keys>,..."
// holds <keys> for the default joypad from emulated frame <frame> until
// the next entry's frame (empty <keys> releases everything). Keys are
// '+'-separated tokens: A B ST SE U D L R TL TR. Frames are counted as
// calls for the default pad, one per emulated frame. Used by automated
// two-instance link tests to navigate game menus; inert unless the env
// var is set.
static uint32_t AutoInputMask(int joy)
{
    struct Step {
        long frame;
        uint32_t keys;
    };
    static std::vector<Step> script;
    static long frame_count = 0;
    static bool parsed = false;

    if (!parsed) {
        parsed = true;
        if (const char* env = getenv("VBAM_AUTOINPUT")) {
            for (const char* p = env; *p;) {
                char* end = nullptr;
                long f = strtol(p, &end, 10);
                if (end == p || *end != ':')
                    break;
                p = end + 1;
                uint32_t keys = 0;
                while (*p && *p != ',') {
                    if (!strncmp(p, "ST", 2)) { keys |= KEYM_START; p += 2; }
                    else if (!strncmp(p, "SE", 2)) { keys |= KEYM_SELECT; p += 2; }
                    else if (!strncmp(p, "TL", 2)) { keys |= KEYM_L; p += 2; }
                    else if (!strncmp(p, "TR", 2)) { keys |= KEYM_R; p += 2; }
                    else if (*p == 'A') { keys |= KEYM_A; p++; }
                    else if (*p == 'B') { keys |= KEYM_B; p++; }
                    else if (*p == 'U') { keys |= KEYM_UP; p++; }
                    else if (*p == 'D') { keys |= KEYM_DOWN; p++; }
                    else if (*p == 'L') { keys |= KEYM_LEFT; p++; }
                    else if (*p == 'R') { keys |= KEYM_RIGHT; p++; }
                    else p++;
                }
                script.push_back({ f, keys });
                if (*p == ',')
                    p++;
            }
        }
    }
    if (script.empty())
        return 0;
    if (joy == (int)(OPTION(kJoyDefault) - 1))
        frame_count++;
    uint32_t mask = 0;
    for (const Step& s : script)
        if (frame_count >= s.frame)
            mask = s.keys;
    return mask;
}

// updates the joystick data (done in background by the SDL poller)
bool systemReadJoypads()
{
    return true;
}

// return information about the given joystick, -1 for default joystick
uint32_t systemReadJoypad(int joy)
{
    if (joy < 0 || joy > 3)
        joy = OPTION(kJoyDefault) - 1;

    uint32_t ret = vbamApp().emulated_gamepad()->GetJoypad(joy);
    ret |= AutoInputMask(joy);

    if (turbo)
        ret |= KEYM_SPEED;

    uint32_t af = autofire;

    if (ret & KEYM_AUTO_A) {
        ret |= KEYM_A;
        af |= KEYM_A;
    }

    if (ret & KEYM_AUTO_B) {
        ret |= KEYM_B;
        af |= KEYM_B;
    }

    uint32_t ah = autohold;
    uint32_t ah_but = ah | ret;
    if (ah_but)
    {
        ret ^= ah;
    }

    static int autofire_trigger = 1;
    static bool autofire_state = true;
    uint32_t af_but = af & ret;

    if (af_but) {
        if (!autofire_state)
            ret &= ~af_but;

        if (!--autofire_trigger) {
            autofire_trigger = gopts.autofire_rate;
            autofire_state = !autofire_state;
        }
    } else {
        autofire_state = true;
        autofire_trigger = gopts.autofire_rate;
    }

    // disallow opposite directionals simultaneously
    ret &= ~((ret & (KEYM_LEFT | KEYM_DOWN | KEYM_MOTION_DOWN | KEYM_MOTION_RIGHT)) >> 1);
    ret &= REALKEY_MASK;

    // joypad.set() override from a Lua script (no-op without VBAM_ENABLE_LUA).
    uint32_t lua_mask = 0;
    if (vbam::lua::LuaJoypadOverride(joy, &lua_mask))
        ret = lua_mask & REALKEY_MASK;

    if (game_recording) {
        uint32_t rret = ret & ~(KEYM_SPEED | KEYM_CAPTURE);

        if (rret != game_joypad) {
            game_joypad = rret;

            if (!WriteU32(game_file, qToLittleEndian(game_frame)) ||
                !WriteU32(game_file, qToLittleEndian(game_joypad))) {
                game_file.close();
                game_recording = false;
                vbam::LogError(TR("Error writing game recording"));
            }

            if (recording_format == MV_FORMAT_ID_VMV2)
                game_frame = 0;
        }
    } else if (game_playback) {
        switch (recording_format) {
        case MV_FORMAT_ID_VMV2:
            if (game_frame >= game_next_frame) {
                game_joypad = game_next_joypad;
                uint32_t gf, jp;

                if (!ReadU32(game_file, &gf) || !ReadU32(game_file, &jp)) {
                    systemStopGamePlayback();
                    systemScreenMessage(TR("Playback ended"));
                    break;
                }

                game_next_frame = qFromLittleEndian(gf);
                game_next_joypad = qFromLittleEndian(jp);

                game_frame = 0;
            }
            break;

        case MV_FORMAT_ID_VMV1:
            while (game_frame >= game_next_frame) {
                game_joypad = game_next_joypad;
                uint32_t gf, jp;

                if (!ReadU32(game_file, &gf) || !ReadU32(game_file, &jp)) {
                    systemStopGamePlayback();
                    systemScreenMessage(TR("Playback ended"));
                    break;
                }

                game_next_frame = qFromLittleEndian(gf);
                game_next_joypad = qFromLittleEndian(jp);
            }
            break;

        default:
            break;
        }

        ret = game_joypad;
    }

    return ret;
}

void systemShowSpeed(int speed)
{
    MainWindow* f = vbamApp().frame;
    if (!f || !f->GetPanel())
        return;
    QString s = TR("%1 % (%2, %3 fps)").arg(speed).arg(systemFrameSkip).arg(frames * speed / 100);

    switch (OPTION(kPrefShowSpeed)) {
    case SS_NONE:
        f->GetPanel()->osdstat.clear();
        break;

    case SS_PERCENT:
        f->GetPanel()->osdstat = QStringLiteral("%1 %").arg(speed);
        break;

    case SS_DETAILED:
        f->GetPanel()->osdstat = s;
        break;
    }

    f->SetStatusText(s, 1);
    frames = 0;
}

int systemSaveUpdateCounter = SYSTEM_SAVE_NOT_UPDATED;

void system10Frames() {
    GameArea* panel = CurrentPanel();
    if (!panel)
        return;

    if (OPTION(kPrefFrameSkip) == -1) {
        // We keep a rolling mean of the last second and use this value to
        // adjust the systemFrameSkip value dynamically.

        // Target time in ms for 10 frames at 60 FPS.
        constexpr int kTarget = 10 * 1000 / 60;

        static uint32_t prevclock = 0;
        static int speedadj = 0;
        static int last_second[6] = {kTarget, kTarget, kTarget,
                                    kTarget, kTarget, kTarget};
        static size_t last_index = 0;

        const uint32_t timestamp = systemGetClock();

        if (!panel->was_paused && prevclock) {
            last_second[last_index] = systemGetClock() - prevclock;
            last_index = (last_index + 1) % 6;

            int average = 0;
            for (size_t i = 0; i < 6; i++) {
                average += last_second[i];
            }
            average /= 6;

            const int speed = (kTarget * 100) / std::max(average, 1);

            // why 98??
            if (speed >= 98)
                speedadj++;
            else if (speed < 80)
                speedadj -= (90 - speed) / 10;
            else
                speedadj--;

            if (speedadj >= 3) {
                speedadj = 0;
                systemFrameSkip = std::max(systemFrameSkip - 1, 0);
            } else if (speedadj <= -2) {
                speedadj += 2;
                systemFrameSkip = std::min(systemFrameSkip + 1, 9);
            }
        }

        prevclock = timestamp;
        panel->was_paused = false;
    }

    if (gopts.rewind_interval) {
        if (!panel->rewind_time)
            panel->rewind_time = gopts.rewind_interval * 6;
        else if (!--panel->rewind_time)
            panel->do_rewind = true;
    }

    if (--systemSaveUpdateCounter == SYSTEM_SAVE_NOT_UPDATED)
        panel->SaveBattery();
    else if (systemSaveUpdateCounter < SYSTEM_SAVE_NOT_UPDATED)
        systemSaveUpdateCounter = SYSTEM_SAVE_NOT_UPDATED;
}

void systemFrame()
{
    if (game_recording || game_playback)
        game_frame++;
}

// technically, num is ignored in favor of finding the first
// available slot
void systemScreenCapture(int num)
{
    GameArea* panel = CurrentPanel();
    if (!panel || !panel->emusys)
        return;
    const QString dir = vbamApp().frame->GetGamePath(OPTION(kGenScreenshotDir));
    const int capture_format = OPTION(kPrefCaptureFormat);
    QString path;

    do {
        QString bfn = QStringLiteral("%1%2").arg(panel->game_base_name())
                          .arg(num++, 2, 10, QLatin1Char('0'));

        if (capture_format == 0)
            bfn.append(QStringLiteral(".png"));
        else
            bfn.append(QStringLiteral(".bmp"));

        path = QDir(dir).filePath(bfn);
    } while (QFileInfo::exists(path));

    QDir().mkpath(dir);

    if (capture_format == 0)
        panel->emusys->emuWritePNG(vbam::ToPath(path).c_str());
    else
        panel->emusys->emuWriteBMP(vbam::ToPath(path).c_str());

    systemScreenMessage(TR("Wrote snapshot %1").arg(path));
}

void systemSaveOldest()
{
    // I need to be implemented
}

void systemLoadRecent()
{
    // I need to be implemented
}

uint32_t systemGetClock()
{
    if (!QCoreApplication::instance())
        return 0;
    return static_cast<uint32_t>(vbamApp().timer.elapsed());
}

void systemCartridgeRumble(bool b)
{
    if (QCoreApplication::instance() && vbamApp().sdl_poller())
        vbamApp().sdl_poller()->SetRumble(b);
}

static uint8_t sensorDarkness = 0xE8; // total darkness (including daylight on rainy days)

uint8_t systemGetSensorDarkness()
{
    return sensorDarkness;
}

void systemUpdateSolarSensor()
{
    uint8_t sun = 0x0; //sun = 0xE8 - 0xE8 (case 0 and default)
    int level = sunBars / 10;

    switch (level) {
    case 1:
        sun = 0xE8 - 0xE0;
        break;

    case 2:
        sun = 0xE8 - 0xDA;
        break;

    case 3:
        sun = 0xE8 - 0xD0;
        break;

    case 4:
        sun = 0xE8 - 0xC8;
        break;

    case 5:
        sun = 0xE8 - 0xC0;
        break;

    case 6:
        sun = 0xE8 - 0xB0;
        break;

    case 7:
        sun = 0xE8 - 0xA0;
        break;

    case 8:
        sun = 0xE8 - 0x88;
        break;

    case 9:
        sun = 0xE8 - 0x70;
        break;

    case 10:
        sun = 0xE8 - 0x50;
        break;

    default:
        break;
    }

    sensorDarkness = 0xE8 - sun;
}

void systemUpdateMotionSensor()
{
    // First poll the SDL gamepad motion sensor (if any). When the user's
    // controller has a usable accelerometer/gyro, its readouts replace the
    // keyboard/button tilt accumulation below.
    auto& motion = vbam::core::SdlMotion::Instance();
    motion.Poll();
    const bool sensor_active = motion.IsActive();

    for (int i = 0; i < 4; i++) {
        const uint32_t joy_value = vbamApp().emulated_gamepad()->GetJoypad(i);

        if (!sensorx[i])
            sensorx[i] = 2047;

        if (!sensory[i])
            sensory[i] = 2047;

        if (sensor_active) {
            sensorx[i] = motion.TiltX();
            sensory[i] = motion.TiltY();
            sensorz[i] = motion.GyroZ() * 10;
            continue;
        }

        if (joy_value & KEYM_MOTION_LEFT) {
            sunBars--;

            if (sunBars < 1)
                sunBars = 1;

            sensorx[i] += 3;

            if (sensorx[i] > 2197)
                sensorx[i] = 2197;

            if (sensorx[i] < 2047)
                sensorx[i] = 2057;
        } else if (joy_value & KEYM_MOTION_RIGHT) {
            sunBars++;

            if (sunBars > 100)
                sunBars = 100;

            sensorx[i] -= 3;

            if (sensorx[i] < 1897)
                sensorx[i] = 1897;

            if (sensorx[i] > 2047)
                sensorx[i] = 2037;
        } else if (sensorx[i] > 2047) {
            sensorx[i] -= 2;

            if (sensorx[i] < 2047)
                sensorx[i] = 2047;
        } else {
            sensorx[i] += 2;

            if (sensorx[i] > 2047)
                sensorx[i] = 2047;
        }

        if (joy_value & KEYM_MOTION_UP) {
            sensory[i] += 3;

            if (sensory[i] > 2197)
                sensory[i] = 2197;

            if (sensory[i] < 2047)
                sensory[i] = 2057;
        } else if (joy_value & KEYM_MOTION_DOWN) {
            sensory[i] -= 3;

            if (sensory[i] < 1897)
                sensory[i] = 1897;

            if (sensory[i] > 2047)
                sensory[i] = 2037;
        } else if (sensory[i] > 2047) {
            sensory[i] -= 2;

            if (sensory[i] < 2047)
                sensory[i] = 2047;
        } else {
            sensory[i] += 2;

            if (sensory[i] > 2047)
                sensory[i] = 2047;
        }

        const int lowZ = -1800;
        const int centerZ = 0;
        const int highZ = 1800;
        const int accelZ = 3;

        if (joy_value & KEYM_MOTION_IN) {
            sensorz[i] += accelZ;

            if (sensorz[i] > highZ)
                sensorz[i] = highZ;

            if (sensorz[i] < centerZ)
                sensorz[i] = centerZ + (accelZ * 300);
        } else if (joy_value & KEYM_MOTION_OUT) {
            sensorz[i] -= accelZ;

            if (sensorz[i] < lowZ)
                sensorz[i] = lowZ;

            if (sensorz[i] > centerZ)
                sensorz[i] = centerZ - (accelZ * 300);
        } else if (sensorz[i] > centerZ) {
            sensorz[i] -= (accelZ * 100);

            if (sensorz[i] < centerZ)
                sensorz[i] = centerZ;
        } else {
            sensorz[i] += (accelZ * 100);

            if (sensorz[i] > centerZ)
                sensorz[i] = centerZ;
        }
    }

    systemUpdateSolarSensor();
}

int systemGetSensorX()
{
    return sensorx[OPTION(kJoyDefault) - 1];
}

int systemGetSensorY()
{
    return sensory[OPTION(kJoyDefault) - 1];
}

int systemGetSensorZ()
{
    return sensorz[OPTION(kJoyDefault) - 1] / 10;
}

// ---------------------------------------------------------------------------
// Game Boy printer output dialog
// ---------------------------------------------------------------------------

namespace {

// Shows the printed page (160 x lines pixels, RGB555 rows of 162 with a 1
// pixel top border and 2 pixel right border) with a magnification choice, and
// offers Save (image file), Print (QPrinter), Continue (OK: keep accumulating)
// and Discard/Close.
class PrintDialog : public QDialog {
public:
    PrintDialog(const uint16_t* data, int lines, bool cont)
        : QDialog(vbamApp().frame), img_(160, lines, QImage::Format_RGB32) {
        setWindowTitle(TR("Game Boy Printer"));
        Qt::WindowFlags flags = Qt::Dialog | Qt::WindowTitleHint | Qt::WindowCloseButtonHint;
        if (OPTION(kDispKeepOnTop))
            flags |= Qt::WindowStaysOnTopHint;
        setWindowFlags(flags);

        data += 162; // top border
        for (int y = 0; y < lines; y++) {
            QRgb* row = reinterpret_cast<QRgb*>(img_.scanLine(y));
            for (int x = 0; x < 160; x++) {
                uint16_t d = *data++;
                row[x] = qRgb(((d >> 10) & 0x1f) << 3, ((d >> 5) & 0x1f) << 3, (d & 0x1f) << 3);
            }
            data += 2; // rhs border
        }

        auto* layout = new QVBoxLayout(this);

        scroll_ = new QScrollArea(this);
        preview_ = new QLabel(scroll_);
        preview_->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        scroll_->setWidget(preview_);
        scroll_->setWidgetResizable(false);
        scroll_->setMinimumSize(320 + 20, std::min(lines, 144) * 2 + 20);
        layout->addWidget(scroll_, 1);

        auto* mag_row = new QHBoxLayout();
        mag_row->addWidget(new QLabel(TR("Magnification:"), this));
        mag_ = new QComboBox(this);
        mag_->addItems({QStringLiteral("1x"), QStringLiteral("2x"), QStringLiteral("3x"),
                        QStringLiteral("4x")});
        mag_->setCurrentIndex(1);
        mag_row->addWidget(mag_);
        mag_row->addStretch(1);
        layout->addLayout(mag_row);

        auto* buttons = new QDialogButtonBox(this);
        save_ = buttons->addButton(QDialogButtonBox::Save);
        print_ = buttons->addButton(TR("&Print..."), QDialogButtonBox::ActionRole);
        ok_ = buttons->addButton(TR("&Continue"), QDialogButtonBox::AcceptRole);
        cancel_ = buttons->addButton(TR("&Discard"), QDialogButtonBox::RejectRole);
        layout->addWidget(buttons);

        connect(mag_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                [this](int) { UpdatePreview(); });
        connect(save_, &QPushButton::clicked, this, [this]() { DoSave(); });
        connect(print_, &QPushButton::clicked, this, [this]() { DoPrint(); });
        connect(ok_, &QPushButton::clicked, this, &QDialog::accept);
        connect(cancel_, &QPushButton::clicked, this, &QDialog::reject);

        UpdatePreview();
        (cont ? static_cast<QWidget*>(ok_) : static_cast<QWidget*>(save_))->setFocus();
    }

private:
    int Magnification() const { return mag_->currentIndex() + 1; }

    void UpdatePreview() {
        const int m = Magnification();
        const QImage scaled = img_.scaled(160 * m, img_.height() * m, Qt::IgnoreAspectRatio,
                                          Qt::FastTransformation);
        preview_->setPixmap(QPixmap::fromImage(scaled));
        preview_->resize(scaled.size());
    }

    void DoSave() {
        static QString prsav_path;
        QString dn = vbamApp().frame->GetPanel()->game_base_name();
        if (OPTION(kPrefCaptureFormat) == 0)
            dn.append(QStringLiteral(".png"));
        else
            dn.append(QStringLiteral(".bmp"));

        const QString filter = TR("Image files (*.bmp *.jpg *.png);;All files (*)");
        const QString of = QFileDialog::getSaveFileName(this, TR("Save printer image to"),
                                                        QDir(prsav_path).filePath(dn), filter);
        if (of.isEmpty())
            return;
        prsav_path = QFileInfo(of).absolutePath();

        const int m = Magnification();
        const QImage scimg = img_.scaled(160 * m, img_.height() * m, Qt::IgnoreAspectRatio,
                                         Qt::FastTransformation);
        if (scimg.save(of)) {
            systemScreenMessage(TR("Wrote printer output to %1").arg(of));
            cancel_->setText(TR("&Close"));
            cancel_->setFocus();
        }
    }

    void DoPrint() {
        static QPrinter* printer = nullptr;
        if (!printer)
            printer = new QPrinter(QPrinter::HighResolution);

        QPrintDialog pd(printer, this);
        pd.setWindowTitle(TR("Print"));
        if (pd.exec() != QDialog::Accepted)
            return;

        const int m = Magnification();
        const QImage scimg = img_.scaled(160 * m, img_.height() * m, Qt::IgnoreAspectRatio,
                                         Qt::FastTransformation);
        QPainter painter(printer);
        const QRect page = printer->pageLayout().paintRectPixels(printer->resolution());
        // Tile the image across as many pages as needed, at 1:1 device pixels.
        const int npw = (scimg.width() + page.width() - 1) / page.width();
        const int nph = (scimg.height() + page.height() - 1) / page.height();
        bool first = true;
        for (int py = 0; py < nph; py++) {
            for (int px = 0; px < npw; px++) {
                if (!first)
                    printer->newPage();
                first = false;
                painter.drawImage(page.x() - px * page.width(), page.y() - py * page.height(),
                                  scimg);
            }
        }
        painter.end();

        systemScreenMessage(TR("Printed"));
        cancel_->setText(TR("&Close"));
        cancel_->setFocus();
    }

    QImage img_;
    QScrollArea* scroll_ = nullptr;
    QLabel* preview_ = nullptr;
    QComboBox* mag_ = nullptr;
    QPushButton* save_ = nullptr;
    QPushButton* print_ = nullptr;
    QPushButton* ok_ = nullptr;
    QPushButton* cancel_ = nullptr;
};

}  // namespace

void systemGbPrint(uint8_t* data, int len, int pages, int feed, int pal, int cont)
{
    (void)pages; // unused params
    (void)cont; // unused params
    ModalPause mp; // this might take a while, so signal a pause
    GameArea* panel = CurrentPanel();
    static uint16_t* accum_prdata;
    static int accum_prdata_len = 0, accum_prdata_size = 0;
    static uint16_t prdata[162 * 145] = { 0 };
    int lines = len / 40;
    uint16_t* out = prdata + 162; // 1-pix top border

    for (int y = 0; y < lines / 8; y++) {
        for (int x = 0; x < 160 / 8; x++) {
            for (int k = 0; k < 8; k++) {
                int a = *data++;
                int b = *data++;

                for (int j = 0, mask = 0x80; j < 8; j++, mask >>= 1) {
                    int c = ((a & mask) | ((b & mask) << 1)) >> (7 - j);

                    // 00 in c means bits 0-1 in pal
                    if (pal != 0xe4) // 11 10 01 00
                        c = (pal & (3 << c * 2)) >> c * 2;

                    out[j + k * 162] = systemGbPalette[c];
                }
            }

            out += 8;
        }

        out += 2 + 7 * 162; // 2-pix rhs border
    }

    // assume no bottom margin means "more coming"
    uint16_t* to_print = prdata;

    if ((OPTION(kGBPrintAutoPage) && !(feed & 15)) || accum_prdata_len) {
        if (!accum_prdata_len)
            accum_prdata_len = 162; // top border

        accum_prdata_len += lines * 162;

        if (accum_prdata_size < accum_prdata_len) {
            if (!accum_prdata_size)
                accum_prdata = (uint16_t*)calloc(accum_prdata_len, 2);
            else
                accum_prdata = (uint16_t*)realloc(accum_prdata, accum_prdata_len * 2);

            accum_prdata_size = accum_prdata_len;
        }

        memcpy(accum_prdata + accum_prdata_len - lines * 162, prdata + 162,
            lines * 162 * 2);

        if (OPTION(kGBPrintAutoPage) && !(feed & 15))
            return;

        to_print = accum_prdata;
        lines = accum_prdata_len / 162 - 1;
        accum_prdata_len = 0;
    }

    if (OPTION(kGBPrintScreenCap) && panel) {
        const QString dir = vbamApp().frame->GetGamePath(OPTION(kGenScreenshotDir));
        int num = 1;
        const int capture_format = OPTION(kPrefCaptureFormat);
        QString path;

        do {
            QString bfn = QStringLiteral("%1-print%2").arg(panel->game_base_name())
                              .arg(num++, 2, 10, QLatin1Char('0'));

            if (capture_format == 0)
                bfn.append(QStringLiteral(".png"));
            else
                bfn.append(QStringLiteral(".bmp"));

            path = QDir(dir).filePath(bfn);
        } while (QFileInfo::exists(path));

        QDir().mkpath(dir);
        int d = systemColorDepth;
        int rs = systemRedShift, bs = systemBlueShift, gs = systemGreenShift;
        systemColorDepth = 16;
        systemRedShift = 10;
        systemGreenShift = 5;
        systemBlueShift = 0;
        const std::string of = vbam::ToPath(path);
        bool ret = capture_format == 0 ? utilWritePNGFile(of.c_str(), 160, lines, (uint8_t*)to_print)
                                       : utilWriteBMPFile(of.c_str(), 160, lines, (uint8_t*)to_print);

        if (ret) {
            systemScreenMessage(TR("Wrote printer output to %1").arg(path));
        }

        systemColorDepth = d;
        systemRedShift = rs;
        systemGreenShift = gs;
        systemBlueShift = bs;
        return;
    }

    if (!GuiAlive())
        return;

    PrintDialog dlg(to_print, lines, !(feed & 15));
    int ret = vbamApp().frame->ShowModal(&dlg);

    if (ret == QDialog::Accepted) {
        accum_prdata_len = (lines + 1) * 162;

        if (to_print != accum_prdata) {
            if (accum_prdata_size < accum_prdata_len) {
                if (!accum_prdata_size)
                    accum_prdata = (uint16_t*)calloc(accum_prdata_len, 2);
                else
                    accum_prdata = (uint16_t*)realloc(accum_prdata, accum_prdata_len * 2);

                accum_prdata_size = accum_prdata_len;
            }

            memcpy(accum_prdata, to_print, accum_prdata_len * 2);
        }
    }
}

void systemClearStatusMessage()
{
    if (!GuiAlive())
        return;

    MainWindow* f = vbamApp().frame;
    if (f->statusBar() && !f->statusBar()->currentMessage().isEmpty())
        f->SetStatusText(QString(), 0);
}

void systemScreenMessage(const QString& msg)
{
    // During teardown the frame goes away before the core is done; accessing
    // the panel or status bar at that point is unsafe -- skip the OSD.
    if (!GuiAlive())
        return;

    MainWindow* f = vbamApp().frame;
    if (f->isVisible()) {
        fputs(msg.toUtf8().constData(), stdout); // show **something** on terminal
        fputc('\n', stdout);
        GameArea* panel = f->GetPanel();

        if (panel)
            f->SetStatusText(msg, 0);

        if (panel) {
            panel->osdtext = msg;
            panel->osdtime = systemGetClock();
        }
    }
}

void systemScreenMessage(const char* msg)
{
    systemScreenMessage(QString::fromUtf8(msg));
}

bool systemCanChangeSoundQuality()
{
#ifndef NO_FFMPEG
    extern int emulating;
    if (emulating) {
        GameArea* panel = CurrentPanel();

        if (panel)
            return !panel->IsRecording();
    }
#endif
    return GuiAlive();
}

bool systemPauseOnFrame()
{
    if (pause_next) {
        pause_next = false;
        if (GameArea* panel = CurrentPanel())
            panel->Pause();
        return true;
    }

    return false;
}

void systemGbBorderOn()
{
    GameArea* panel = CurrentPanel();

    if (panel)
        panel->AddBorder();
}

class SoundDriver;
std::unique_ptr<SoundDriver> systemSoundInit()
{
    soundShutdown();
    return audio::CreateSoundDriver(OPTION(kSoundAudioAPI));
}

void systemOnWriteDataToSoundBuffer(const uint16_t* finalWave, int length)
{
    (void)finalWave; // unused params
    (void)length; // unused params
#ifndef NO_FFMPEG
    GameArea* panel = CurrentPanel();

    if (panel)
        panel->AddFrame(finalWave, length);
#endif
}

void systemOnSoundShutdown()
{
}

// ---------------------------------------------------------------------------
// GDB stub transports
// ---------------------------------------------------------------------------

#if defined(VBAM_ENABLE_DEBUGGER)

#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>

extern int (*remoteSendFnc)(char*, int);
extern int (*remoteRecvFnc)(char*, int);
extern void (*remoteCleanUpFnc)();

#ifndef _WIN32
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/poll.h>
#include <unistd.h>

static QString pty_slave;
static int pty_master = -1;

static int debugReadPty(char* buf, int len)
{
    while (1) {
        struct pollfd fd;
        fd.fd = pty_master;
        fd.events = POLLIN;

        if (poll(&fd, 1, 200) != 0)
            return read(pty_master, buf, len);
        else
            return -2; // try to let repaints & such run
    }
}

static int debugWritePty(/* const */ char* buf, int len)
{
    return write(pty_master, buf, len);
}

static void debugClosePty()
{
    if (pty_master >= 0) {
        close(pty_master);
        pty_master = -1;
    }
}

bool debugOpenPty()
{
    if (pty_master >= 0) // should never happen
        close(pty_master);

    const char* slave_name;

    if ((pty_master = posix_openpt(O_RDWR | O_NOCTTY)) < 0 || grantpt(pty_master) < 0 ||
        unlockpt(pty_master) < 0 || !(slave_name = ptsname(pty_master))) {
        vbam::LogError(TR("Error opening pseudo tty: %1").arg(QString::fromLocal8Bit(strerror(errno))));

        if (pty_master >= 0) {
            close(pty_master);
            pty_master = -1;
        }

        return false;
    }

    pty_slave = QString::fromLocal8Bit(slave_name);
    remoteSendFnc = debugWritePty;
    remoteRecvFnc = debugReadPty;
    remoteCleanUpFnc = debugClosePty;
    return true;
}

const QString& debugGetSlavePty()
{
    return pty_slave;
}

bool debugWaitPty()
{
    if (pty_master < 0)
        return false;

    struct pollfd fd;
    fd.fd = pty_master;
    fd.events = POLLIN;
    return poll(&fd, 1, 100) > 0;
}
#else
static QString pty_slave;

bool debugOpenPty() { return false; }
const QString& debugGetSlavePty() { return pty_slave; }
bool debugWaitPty() { return false; }
#endif

static QTcpServer* debug_server = nullptr;
QTcpSocket* debug_remote = nullptr;

static int debugReadSock(char* buf, int len)
{
    if (!debug_remote)
        return -1;
    MainWindow* f = vbamApp().frame;
    // Block for a short while so repaints and menu handling can run between
    // polls; the modal counter keeps the emulation loop from re-entering.
    if (f) f->StartModal();
    const bool ready = debug_remote->bytesAvailable() > 0 || debug_remote->waitForReadyRead(200);
    if (f) f->StopModal();

    if (debug_remote->state() != QAbstractSocket::ConnectedState &&
        debug_remote->bytesAvailable() <= 0)
        return -1;

    if (!ready)
        return -2;

    const qint64 n = debug_remote->read(buf, len);
    if (n < 0)
        return -1;
    return static_cast<int>(n);
}

static int debugWriteSock(char* buf, int len)
{
    if (!debug_remote)
        return -1;
    qint64 total = 0;
    while (total < len) {
        const qint64 n = debug_remote->write(buf + total, len - total);
        if (n < 0)
            return -1;
        total += n;
        if (!debug_remote->waitForBytesWritten(1000))
            return -1;
    }
    return static_cast<int>(total);
}

static void debugCloseSock()
{
    delete debug_remote;
    debug_remote = nullptr;
    delete debug_server;
    debug_server = nullptr;
}

bool debugStartListen(int port)
{
    delete debug_server; // should never be necessary
    debug_server = nullptr;
    delete debug_remote;
    debug_remote = nullptr;

    if (port <= 0 || port > 65535) {
        vbam::LogError(TR("Invalid GDB server port %1").arg(port));
        return false;
    }

    debug_server = new QTcpServer();
    remoteSendFnc = debugWriteSock;
    remoteRecvFnc = debugReadSock;
    remoteCleanUpFnc = debugCloseSock;

    if (debug_server->listen(QHostAddress::Any, static_cast<quint16>(port)))
        return true;

    vbam::LogError(TR("Error setting up GDB server socket on port %1").arg(port));
    delete debug_server;
    debug_server = nullptr;
    return false;
}

bool debugWaitSocket()
{
    if (!debug_server)
        return false;

    if (debug_remote)
        return true;

    debug_server->waitForNewConnection(100);
    debug_remote = debug_server->nextPendingConnection();
    if (debug_remote) {
        // Owned here, not by the server.
        debug_remote->setParent(nullptr);
        debug_remote->setSocketOption(QAbstractSocket::LowDelayOption, 1);
    }
    return debug_remote != nullptr;
}

#endif  // defined(VBAM_ENABLE_DEBUGGER)

void log(const char* defaultMsg, ...)
{
    va_list valist;
    char buf[2048];
    va_start(valist, defaultMsg);
    vsnprintf(buf, 2048, defaultMsg, valist);
    QString msg = QString::fromUtf8(buf);
    va_end(valist);

    if (!QCoreApplication::instance())
        return;

    // Collapse runs of identical consecutive messages. Without this, a caller
    // that logs the same line every frame blows up the log buffer and the
    // text view redraw cost until the UI locks.
    static QString last_msg;
    static unsigned long duplicate_count = 0;

    if (msg == last_msg) {
        ++duplicate_count;
        return;
    }

    if (duplicate_count > 0) {
        vbam::LogMessage(
            QStringLiteral("(previous line repeated %1 times)").arg(duplicate_count));
        duplicate_count = 0;
    }

    last_msg = msg;
    // Appends to the log store and refreshes the Logging dialog through the
    // update callback the main window installs.
    vbam::LogMessage(msg.trimmed());

    if (GuiAlive())
        systemScreenMessage(msg.trimmed());
}
