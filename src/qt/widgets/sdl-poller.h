#ifndef VBAM_QT_WIDGETS_SDL_POLLER_H_
#define VBAM_QT_WIDGETS_SDL_POLLER_H_

#include <cstdint>
#include <map>
#include <memory>

#include <QObject>
#include <QTimer>

#ifndef ENABLE_SDL3
#include <SDL.h>
#else
#include <SDL3/SDL.h>
#endif

#include "qt/config/option-observer.h"

namespace widgets {

class InputDispatcher;
class JoyState;

// Polls SDL joystick/game controller events on a QTimer and delivers the
// resulting UserInputBatch through the InputDispatcher. Also owns the SDL
// joystick subsystem for the process. Singleton owned by the application.
class SdlPoller final : public QObject {
    Q_OBJECT

public:
    explicit SdlPoller(InputDispatcher* dispatcher);
    ~SdlPoller() override;

    SdlPoller(const SdlPoller&) = delete;
    SdlPoller& operator=(const SdlPoller&) = delete;

    // Sets or unsets the controller rumble.
    void SetRumble(bool rumble);

    // Current keyboard modifier state from SDL with L/R distinction, as
    // KeyModFlag bits (0 when SDL does not track the keyboard).
    static uint32_t GetCurrentSdlMods();

    // Polls SDL immediately (also called by the timer). Public so the emulation
    // loop can drain joystick events right before reading the joypad.
    void Poll();

private:
    JoyState* FindJoyState(const SDL_JoystickID& joy_id);
    void RemapControllers();
    void ReconnectControllers(bool enable_game_controller);

    std::map<SDL_JoystickID, std::unique_ptr<JoyState>> joystick_states_;
    bool enable_game_controller_ = false;
    InputDispatcher* const dispatcher_;
    QTimer timer_;
    const config::OptionsObserver game_controller_enabled_observer_;
};

}  // namespace widgets

#endif  // VBAM_QT_WIDGETS_SDL_POLLER_H_
