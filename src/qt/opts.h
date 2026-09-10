#ifndef VBAM_QT_OPTS_H_
#define VBAM_QT_OPTS_H_

#include <cstdint>

#include <QSize>
#include <QString>
#include <QStringList>

// Recent file list (10 entries), persisted under [Recent] in the INI.
class RecentFiles {
public:
    static constexpr int kMaxFiles = 10;

    void AddFileToHistory(const QString& path);
    void RemoveFileFromHistory(int index);
    void Clear();
    int GetCount() const { return files_.size(); }
    QString GetHistoryFile(int index) const { return files_.value(index); }
    const QStringList& files() const { return files_; }
    void SetFiles(const QStringList& files) { files_ = files.mid(0, kMaxFiles); }

private:
    QStringList files_;
};

extern struct opts_t {
    opts_t();

    /// Display
    // Fullscreen video mode (0x0 = use current desktop mode).
    QSize fs_mode;
    int fs_bpp = 0;
    int fs_refresh = 0;

    /// GBA
    QString gba_bios;
    // quick fix for issues #48 and #445
    QString link_host = "127.0.0.1";
    QString server_ip = "*";
    uint32_t link_port = 5738;
    int link_timeout = 500;
    int gba_link_type = 0;

    /// General
    int rewind_interval = 0;

    /// Joypad
    int autofire_rate = 1;

    /// Core
    int gdb_port = 55555;
    int link_num_players = 2;
    int max_scale = 0;

    /// Sound
    int sound_en = 0x30f; // soundSetEnable()

    /// Recent
    RecentFiles recent;

    /// UI Config
    bool hide_menu_bar = false;
    bool show_onscreen_controller = false;
    bool suspend_screensaver = false;
} gopts;

// call to load config (once)
// will write defaults for options not present and delete bad opts
// will also initialize opts[] array translations
void load_opts(bool first_time_launch);
// call whenever opt vars change
// will detect changes and write config if necessary
void update_opts();
// Updates the shortcut options.
void update_shortcut_opts();
// Updates the joypad options.
void update_joypad_opts();
// returns true if option name correct; prints error if val invalid
void opt_set(const QString& name, const QString& val);

#endif // VBAM_QT_OPTS_H_
