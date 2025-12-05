#ifndef WX_WIDGETS_SDL_POLLER_H_
#define WX_WIDGETS_SDL_POLLER_H_

#include <cstdint>
#include <map>

#include <wx/timer.h>

#ifndef ENABLE_SDL3
#include <SDL.h>
#else
#include <SDL3/SDL.h>
#endif

#include "wx/config/option-observer.h"
#include "wx/widgets/event-handler-provider.h"

// Forward declarations.
class wxEvtHandler;

namespace widgets {

// Forward declaration.
class JoyState;

// The SDL worker is responsible for handling SDL events and firing the
// appropriate `UserInputEvent` for joysticks.
// It is used to fire `UserInputEvent` for joysticks. This class should be kept
// as a singleton owned by the application object.
class SdlPoller final : public wxTimer {
public:
    explicit SdlPoller(EventHandlerProvider* const handler_provider);
    ~SdlPoller() final;

    // Disable copy and copy assignment.
    SdlPoller(const SdlPoller&) = delete;
    SdlPoller& operator=(const SdlPoller&) = delete;

    // Sets or unsets the controller rumble.
    void SetRumble(bool rumble);

    // Get the current keyboard modifier state from SDL with L/R distinction.
    // This can be called from wxWidgets keyboard handlers to get extended
    // modifier information.
    static uint32_t GetCurrentSdlMods();

private:
    // Helper method to find a joystick state from a joystick ID.
    // Returns NULL if not present.
    JoyState* FindJoyState(const SDL_JoystickID& joy_id);

    // Remap all controllers.
    void RemapControllers();

    // Reconnects all controllers.
    void ReconnectControllers(bool enable_game_controller);

    // wxTimer implementation.
    void Notify() final;

    // Map of SDL joystick ID to joystick state. Only contains active joysticks.
    // Only accessed from the SDL worker thread.
    std::map<SDL_JoystickID, JoyState> joystick_states_;

    // Set to true to enable the game controller API.
    bool enable_game_controller_ = false;

    // The provider of event handlers to send the events to.
    EventHandlerProvider* const handler_provider_;

    // Observer for the game controller enabled option.
    const config::OptionsObserver game_controller_enabled_observer_;
};

}  // namespace widgets

#endif  // WX_WIDGETS_SDL_POLLER_H_
