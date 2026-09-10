#ifndef VBAM_QT_GAME_AREA_H_
#define VBAM_QT_GAME_AREA_H_

#include <chrono>
#include <cstdint>
#include <memory>
#include <set>

#include <QPoint>
#include <QString>
#include <QTimer>
#include <QWidget>

#include "core/base/system.h"
#include "qt/config/option-observer.h"
#include "qt/config/option.h"
#include "qt/widgets/input-dispatcher.h"

#ifndef NO_FFMPEG
#include "components/av_recording/av_recording.h"
#endif

class MainWindow;
class DrawingPanelBase;
class QVBoxLayout;

// Set on first launch (no config file yet) to arm the one-time runtime
// display-filter probe; cleared once GameArea has settled on a filter.
extern bool g_default_filter_probe_pending;

// The central widget of the main window: owns the loaded game, the emulation
// loop and the drawing panel that presents frames. Equivalent of GameArea in
// the wx port.
//
// Emulation is driven by `idle_timer_` (0 ms QTimer): each tick runs one chunk
// of emulation (emusys->emuMain), which produces at most one frame; pacing
// comes from the sound driver (blocking write) or, with sound off, from the
// null driver's timer. The event loop runs between ticks so the GUI stays
// responsive.
class GameArea final : public QWidget {
    Q_OBJECT

public:
    explicit GameArea(QWidget* parent = nullptr);
    ~GameArea() override;

    void SetMainFrame(MainWindow* parent) { main_frame = parent; }

    // set to game title + link info
    void SetFrameTitle();

    void LoadGame(const QString& name);
    void UnloadGame(bool destruct = false);

    IMAGE_TYPE game_type() { return loaded; }
    uint32_t game_size() { return rom_size; }
    QString game_dir();   // directory of the loaded ROM
    QString game_name();  // file name (with extension) of the loaded ROM
    QString game_base_name();  // file name without extension
    QString bat_dir() { return batdir; }
    QString state_dir() { return statedir; }
    void recompute_dirs();

    bool LoadState();
    bool LoadState(int slot);
    bool LoadState(const QString& fname);

    bool SaveState();
    bool SaveState(int slot);
    bool SaveState(const QString& fname);

    // save to default location
    void SaveBattery();

    // true if file at default location may not match memory
    bool cheats_dirty = false;

    static const int GBWidth = 160, GBHeight = 144, SGBWidth = 256, SGBHeight = 224,
                     GBAWidth = 240, GBAHeight = 160;
    void AddBorder();
    void DelBorder();

    // Delete() & set to NULL to force reinit
    DrawingPanelBase* panel = nullptr;
    struct EmulatedSystem* emusys = nullptr;

    // pause game or signal a long operation, similar to pausing
    void Pause();
    void Resume();

    // true if paused since last reset of flag
    bool was_paused = false;

    // osdstat is always displayed at top-left of screen
    QString osdstat;

    // osdtext is displayed for 3 seconds after osdtime, and then cleared
    QString osdtext;
    uint32_t osdtime = 0;

    // Rewind: count down to 0 and rewind
    uint32_t rewind_time = 0;
    // Rewind: flag to the idle loop to do a rewind
    bool do_rewind = false;
    // Rewind: rewind states
    char* rewind_mem = nullptr;
    int num_rewind_states = 0;
    int next_rewind_state = 0;

    // Loaded rom information
    IMAGE_TYPE loaded = IMAGE_UNKNOWN;
    QString loaded_game;  // full path
    uint32_t rom_crc32 = 0;
    QString rom_name;
    QString rom_scene_rls;
    QString rom_scene_rls_name;
    uint32_t rom_size = 0;

// FIXME: size this properly
#define NUM_REWINDS 8
#define REWIND_SIZE 1024 * 512 * NUM_REWINDS

    // Resets the panel; it will be re-created on the next idle tick.
    void ResetPanel();
    // Schedules a panel reset at the start of the next idle tick. Used by
    // option observers so the panel is never destroyed mid-frame.
    void SchedulePanelReset();

    void ShowFullScreen(bool full);
    bool IsFullScreen() { return fullscreen; }
    // set size of frame & panel to scaled screen size
    void AdjustSize(bool force);
#ifndef NO_FFMPEG
    void StartSoundRecording(const QString& fname);
    void StopSoundRecording();
    void StartVidRecording(const QString& fname);
    void StopVidRecording();
    void AddFrame(const uint8_t* data); // video
    void AddFrame(const uint16_t* data, int length); // audio
    bool IsRecording() { return snd_rec.IsRecording() || vid_rec.IsRecording(); }
#endif
    void StartGameRecording(const QString& fname);
    void StopGameRecording();
    void StartGamePlayback(const QString& fname);
    void StopGamePlayback();

    void ShowPointer();
    void HidePointer();
    void HideMenuBar();
    void ShowMenuBar();
    void OnGBBorderChanged(config::Option* option);
    void UpdateLcdFilter();
    void SuspendScreenSaver();
    void UnsuspendScreenSaver();

    // Runs the emulation loop tick (public so MainWindow can force a frame,
    // e.g. NextFrame while paused).
    void OnIdle();

    // Requests the emulation loop to keep running (equivalent of
    // wxIdleEvent::RequestMore()).
    void RequestMore();

    // The widget currently presenting frames (the drawing panel), or nullptr.
    QWidget* PanelWidget() const;

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private Q_SLOTS:
    void OnInputBatch(const widgets::UserInputBatch& batch);

private:
    MainWindow* main_frame = nullptr;

    // set minsize of frame & panel to unscaled screen size
    void LowerMinSize();
    // set minsize of frame & panel to scaled screen size
    void AdjustMinSize();

    QString batdir, statedir;

    int basic_width = GBAWidth, basic_height = GBAHeight;
    bool fullscreen = false;

    bool paused = false;
    QTimer idle_timer_;
    QVBoxLayout* layout_ = nullptr;

#ifndef NO_FFMPEG
    recording::MediaRecorder snd_rec, vid_rec;
#endif

    void MouseActivity();
    bool pointer_blanked = false, menu_bar_hidden = false, screensaver_suspended = false;
    uint32_t mouse_active_time = 0;
    QPoint mouse_last_pos;

    void OnAudioRateChanged();
    void OnVolumeChanged(config::Option* option);
    void OnDispFilterChanged();

    bool schedule_audio_restart_ = false;
    bool pending_resume_after_panel_ = false;
    bool pending_panel_reset_ = false;

    // Construct (but do not lay out) the DrawingPanel for `method`.
    DrawingPanelBase* NewPanelForRenderMethod(config::RenderMethod method);

    // Renderer-init fallback: when a panel reports DrawingInitFailed(), record
    // the method and switch to the next renderer in the platform priority list
    // (Windows: DX12, Vulkan, SDL, OpenGL, Direct3D 9, Simple; macOS: Metal,
    // Vulkan, SDL, OpenGL, Simple; Linux: Vulkan, SDL, OpenGL, Simple).
    // Renderers that failed to init are remembered for the session so the
    // search converges instead of looping.
    std::set<config::RenderMethod> render_init_failed_;
    void EvaluateRenderer();

    // Runtime display-filter auto-probe (first launch only). Once a ROM is
    // running, this cycles candidate filters from highest to lowest quality,
    // measures each one's real frame rate, and settles on the highest that
    // sustains ~60fps, persisting the choice.
    enum class FilterProbeState { kInactive, kStartupDelay, kStabilize, kMeasure };
    FilterProbeState filter_probe_state_ = FilterProbeState::kInactive;
    int filter_probe_index_ = 0;
    int filter_probe_frames_ = 0;
    std::chrono::steady_clock::time_point filter_probe_phase_start_;
    std::chrono::steady_clock::time_point filter_probe_last_frame_;
    double filter_probe_min_interval_ms_ = 0.0;
    int filter_probe_stable_count_ = 0;
    std::vector<double> filter_probe_intervals_;
    void StepFilterProbe();
    void BeginFilterProbeCandidate();
    void AdvanceFilterProbe(double measured_fps);
    void FinishFilterProbe();

    std::unique_ptr<config::OptionsObserver> render_observer_;
    std::unique_ptr<config::OptionsObserver> disp_filter_observer_;
    std::unique_ptr<config::OptionsObserver> scale_observer_;
    std::unique_ptr<config::OptionsObserver> gb_border_observer_;
    std::unique_ptr<config::OptionsObserver> gb_palette_observer_;
    std::unique_ptr<config::OptionsObserver> gb_declick_observer_;
    std::unique_ptr<config::OptionsObserver> lcd_filters_observer_;
    std::unique_ptr<config::OptionsObserver> audio_rate_observer_;
    std::unique_ptr<config::OptionsObserver> audio_volume_observer_;
    std::unique_ptr<config::OptionsObserver> audio_observer_;
    std::unique_ptr<config::OptionsObserver> menu_bar_observer_;
};

// QString version of OSD message
void systemScreenMessage(const QString& msg);

// Takes the status bar copy of the last screen message down.
void systemClearStatusMessage();

#endif  // VBAM_QT_GAME_AREA_H_
