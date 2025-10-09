#include "wx/widgets/keyboard-input-handler.h"

#include "wx/config/user-input.h"
#include "wx/widgets/user-input-event.h"

#include <wx/log.h>

namespace widgets {

namespace {

// Filters the received key code in the key event for something we can use.
wxKeyCode FilterKeyCode(const wxKeyEvent& event) {
    const wxKeyCode unicode_key = static_cast<wxKeyCode>(event.GetUnicodeKey());
    if (unicode_key == WXK_NONE) {
        // We need to filter out modifier keys here so we can differentiate
        // between a key press and a modifier press.
        const wxKeyCode keycode = static_cast<wxKeyCode>(event.GetKeyCode());
        switch (keycode) {
            case WXK_CONTROL:
            case WXK_ALT:
            case WXK_SHIFT:
#ifdef __WXMAC__
            case WXK_RAW_CONTROL:
#endif
                return WXK_NONE;
            default:
                return keycode;
        }
    }

    if (unicode_key < 32) {
        switch (unicode_key) {
            case WXK_BACK:
            case WXK_TAB:
            case WXK_RETURN:
            case WXK_ESCAPE:
                return unicode_key;
            default:
                return WXK_NONE;
        }
    }

    return unicode_key;
}

// Returns the set of modifiers for the given key event.
std::unordered_set<wxKeyModifier> GetModifiers(const wxKeyEvent& event) {
    // Standalone modifier are treated as keys and do not set the keyboard modifiers.
    switch (event.GetKeyCode()) {
        case WXK_CONTROL:
            return {wxMOD_CONTROL};
        case WXK_ALT:
            return {wxMOD_ALT};
        case WXK_SHIFT:
            return {wxMOD_SHIFT};
#ifdef __WXMAC__
        case WXK_RAW_CONTROL:
            return {wxMOD_RAW_CONTROL};
#endif
    }

    std::unordered_set<wxKeyModifier> mods;
    if (event.ControlDown()) {
        mods.insert(wxMOD_CONTROL);
    }
    if (event.AltDown()) {
        mods.insert(wxMOD_ALT);
    }
    if (event.ShiftDown()) {
        mods.insert(wxMOD_SHIFT);
    }
#ifdef __WXMAC__
    if (event.RawControlDown()) {
        mods.insert(wxMOD_RAW_CONTROL);
    }
#endif
    return mods;
}

// Builds a wxKeyModifier from a set of modifiers.
wxKeyModifier GetModifiersFromSet(const std::unordered_set<wxKeyModifier>& mods) {
    int mod = wxMOD_NONE;
    for (const wxKeyModifier m : mods) {
        mod |= m;
    }
    return static_cast<wxKeyModifier>(mod);
}

// Returns the key code for a standalone modifier.
wxKeyCode KeyFromModifier(const wxKeyModifier mod) {
    switch (mod) {
        case wxMOD_CONTROL:
            return WXK_CONTROL;
        case wxMOD_ALT:
            return WXK_ALT;
        case wxMOD_SHIFT:
            return WXK_SHIFT;
#ifdef __WXMAC__
        case wxMOD_RAW_CONTROL:
            return WXK_RAW_CONTROL;
#endif
        default:
            return WXK_NONE;
    }
}

}  // namespace

KeyboardInputHandler::KeyboardInputHandler(EventHandlerProvider* const handler_provider)
    : handler_provider_(handler_provider) {
    VBAM_CHECK(handler_provider_);
}

KeyboardInputHandler::~KeyboardInputHandler() = default;

void KeyboardInputHandler::ProcessKeyEvent(wxKeyEvent& event) {
    if (!handler_provider_->event_handler()) {
        // No event handler to send the event to.
        return;
    }

    if (event.GetEventType() == wxEVT_KEY_DOWN) {
        OnKeyDown(event);
    } else if (event.GetEventType() == wxEVT_KEY_UP) {
        OnKeyUp(event);
    }
}

void KeyboardInputHandler::Reset() {
    active_keys_.clear();
    active_mods_.clear();
    active_mod_inputs_.clear();
}

void KeyboardInputHandler::OnKeyDown(wxKeyEvent& event) {
    // Stop propagation of the event.
    event.Skip(false);

    const wxKeyCode key = FilterKeyCode(event);
    const std::unordered_set<wxKeyModifier> mods = GetModifiers(event);

    wxKeyCode key_pressed = WXK_NONE;
    if (key != WXK_NONE) {
        if (active_keys_.find(key) == active_keys_.end()) {
            // Key was not pressed before.
            key_pressed = key;
            active_keys_.insert(key);
        }
    }

    wxKeyModifier mod_pressed = wxMOD_NONE;
    for (const wxKeyModifier mod : mods) {
        if (active_mods_.find(mod) == active_mods_.end()) {
            // Mod was not pressed before.
            active_mods_.insert(mod);
            mod_pressed = mod;
            break;
        }
    }

    if (key_pressed == WXK_NONE && mod_pressed == wxMOD_NONE) {
        // No new keys or mods were pressed.
        return;
    }

    const wxKeyModifier active_mods = GetModifiersFromSet(active_mods_);
    std::vector<UserInputEvent::Data> event_data;
    if (key_pressed == WXK_NONE) {
        // A new standalone modifier was pressed, send the event.
        event_data.emplace_back(config::KeyboardInput(KeyFromModifier(mod_pressed), mod_pressed),
                                true);
    } else {
        // A new key was pressed, send the event with modifiers, first.
        event_data.emplace_back(config::KeyboardInput(key, active_mods), true);

        if (active_mods != wxMOD_NONE) {
            // Keep track of the key pressed with the active modifiers.
            active_mod_inputs_.emplace(key, active_mods);

            // Also send the key press event without modifiers.
            event_data.emplace_back(config::KeyboardInput(key, wxMOD_NONE), true);
        }
    }

    wxQueueEvent(handler_provider_->event_handler(), new UserInputEvent(std::move(event_data)));
}

void KeyboardInputHandler::OnKeyUp(wxKeyEvent& event) {
    // Stop propagation of the event.
    event.Skip(false);

    const wxKeyCode key = FilterKeyCode(event);
    const std::unordered_set<wxKeyModifier> mods = GetModifiers(event);
    const wxKeyModifier previous_mods = GetModifiersFromSet(active_mods_);

    wxKeyCode key_released = WXK_NONE;
    if (key != WXK_NONE) {
        auto iter = active_keys_.find(key);
        if (iter != active_keys_.end()) {
            // Key was pressed before.
            key_released = key;
            active_keys_.erase(iter);
        }
    }

    wxKeyModifier mod_released = wxMOD_NONE;
    if (key_released == WXK_NONE) {
        // Only look for a standalone modifier if no key was released.
        for (const wxKeyModifier mod : mods) {
            auto iter = active_mods_.find(mod);
            if (iter != active_mods_.end()) {
                // Mod was pressed before.
                mod_released = mod;
                active_mods_.erase(iter);
                break;
            }
        }
    }

    if (key_released == WXK_NONE && mod_released == wxMOD_NONE) {
        // No keys or mods were released.
        return;
    }

    std::vector<UserInputEvent::Data> event_data;
    if (key_released == WXK_NONE) {
        // A standalone modifier was released, send it.
        event_data.emplace_back(config::KeyboardInput(KeyFromModifier(mod_released), mod_released),
                                false);
    } else {
        // A key was released.
        if (previous_mods == wxMOD_NONE) {
            // The key was pressed without modifiers, just send the key release event.
            event_data.emplace_back(config::KeyboardInput(key, wxMOD_NONE), false);
        } else {
            // Check if the key was pressed with the active modifiers.
            const config::KeyboardInput input_with_modifiers(key, previous_mods);
            auto iter = active_mod_inputs_.find(input_with_modifiers);
            if (iter == active_mod_inputs_.end()) {
                // The key press event was never sent, so do it now.
                event_data.emplace_back(input_with_modifiers, true);
            } else {
                active_mod_inputs_.erase(iter);
            }

            // Send the key release event with the active modifiers.
            event_data.emplace_back(input_with_modifiers, false);

            // Also send the key release event without modifiers.
            event_data.emplace_back(config::KeyboardInput(key, wxMOD_NONE), false);
        }
    }

    // Also check for any key that were pressed with the previously active
    // modifiers and release them.
    for (const wxKeyCode active_key : active_keys_) {
        const config::KeyboardInput input(active_key, previous_mods);
        auto iter = active_mod_inputs_.find(input);
        if (iter != active_mod_inputs_.end()) {
            active_mod_inputs_.erase(iter);
            event_data.emplace_back(std::move(input), false);
        }
    }

    for (const auto& data : event_data) {
        active_mod_inputs_.erase(data.input.keyboard_input());
    }

    wxQueueEvent(handler_provider_->event_handler(), new UserInputEvent(std::move(event_data)));
}

}  // namespace widgets
