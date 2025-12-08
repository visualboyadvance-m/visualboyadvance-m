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

// Platform-specific function to check if a specific key is currently pressed.
#ifdef __WXMSW__
#include <windows.h>
bool IsKeyPressed(int vk) {
    return (GetAsyncKeyState(vk) & 0x8000) != 0;
}
#endif

#ifdef __WXMAC__
// macOS virtual key codes for modifier keys (from Events.h / Carbon HIToolbox)
// These are returned by wxKeyEvent::GetRawKeyCode() on macOS
constexpr uint32_t kVK_Shift        = 0x38;  // 56 - Left Shift
constexpr uint32_t kVK_RightShift   = 0x3C;  // 60 - Right Shift
constexpr uint32_t kVK_Control      = 0x3B;  // 59 - Left Control
constexpr uint32_t kVK_RightControl = 0x3E;  // 62 - Right Control
constexpr uint32_t kVK_Option       = 0x3A;  // 58 - Left Option/Alt
constexpr uint32_t kVK_RightOption  = 0x3D;  // 61 - Right Option/Alt
constexpr uint32_t kVK_Command      = 0x37;  // 55 - Left Command
constexpr uint32_t kVK_RightCommand = 0x36;  // 54 - Right Command
#endif

#ifdef __WXGTK__
// GDK key symbols for left/right modifiers (same values as X11 keysyms)
// These work on both X11 and Wayland
#define GDK_KEY_Shift_L 0xffe1
#define GDK_KEY_Shift_R 0xffe2
#define GDK_KEY_Control_L 0xffe3
#define GDK_KEY_Control_R 0xffe4
#define GDK_KEY_Alt_L 0xffe9
#define GDK_KEY_Alt_R 0xffea
#define GDK_KEY_Meta_L 0xffe7
#define GDK_KEY_Meta_R 0xffe8
#define GDK_KEY_Super_L 0xffeb
#define GDK_KEY_Super_R 0xffec
#endif

// Get the extended modifier flags for a standalone modifier key press.
// This uses the raw keycode from the event to determine L/R on Linux/GTK,
// and GetAsyncKeyState on Windows.
uint32_t GetExtendedModForModifierKey(wxKeyCode key, const wxKeyEvent& event) {
#ifdef __WXMSW__
    // On Windows, use GetAsyncKeyState to check L/R modifier keys
    (void)event;  // unused on Windows
    switch (key) {
        case WXK_SHIFT:
            if (IsKeyPressed(VK_LSHIFT))
                return config::kKeyModLeftShift;
            if (IsKeyPressed(VK_RSHIFT))
                return config::kKeyModRightShift;
            return config::kKeyModShift;
        case WXK_CONTROL:
            if (IsKeyPressed(VK_LCONTROL))
                return config::kKeyModLeftControl;
            if (IsKeyPressed(VK_RCONTROL))
                return config::kKeyModRightControl;
            return config::kKeyModControl;
        case WXK_ALT:
            if (IsKeyPressed(VK_LMENU))
                return config::kKeyModLeftAlt;
            if (IsKeyPressed(VK_RMENU))
                return config::kKeyModRightAlt;
            return config::kKeyModAlt;
        case WXK_WINDOWS_LEFT:
            return config::kKeyModLeftMeta;
        case WXK_WINDOWS_RIGHT:
            return config::kKeyModRightMeta;
        default:
            return config::kKeyModNone;
    }
#elif defined(__WXGTK__)
    // On GTK (X11 and Wayland), use the raw keycode from the event
    // GetRawKeyCode() returns the GDK keyval which matches X11 keysyms
    wxUint32 raw_key = event.GetRawKeyCode();
    switch (raw_key) {
        case GDK_KEY_Shift_L:
            return config::kKeyModLeftShift;
        case GDK_KEY_Shift_R:
            return config::kKeyModRightShift;
        case GDK_KEY_Control_L:
            return config::kKeyModLeftControl;
        case GDK_KEY_Control_R:
            return config::kKeyModRightControl;
        case GDK_KEY_Alt_L:
            return config::kKeyModLeftAlt;
        case GDK_KEY_Alt_R:
            return config::kKeyModRightAlt;
        case GDK_KEY_Meta_L:
        case GDK_KEY_Super_L:
            return config::kKeyModLeftMeta;
        case GDK_KEY_Meta_R:
        case GDK_KEY_Super_R:
            return config::kKeyModRightMeta;
        default:
            // Fallback based on wxKeyCode
            switch (key) {
                case WXK_SHIFT:
                    return config::kKeyModShift;
                case WXK_CONTROL:
                    return config::kKeyModControl;
                case WXK_ALT:
                    return config::kKeyModAlt;
                default:
                    return config::kKeyModNone;
            }
    }
#elif defined(__WXMAC__)
    // On macOS, use the raw keycode from the event (NSEvent keyCode)
    // Note: On macOS, Command (⌘) is the primary modifier and maps to wxMOD_CONTROL
    // for cross-platform consistency. The actual Ctrl key maps to wxMOD_RAW_CONTROL.
    uint32_t raw_key = event.GetRawKeyCode();
    switch (raw_key) {
        case kVK_Shift:
            return config::kKeyModLeftShift;
        case kVK_RightShift:
            return config::kKeyModRightShift;
        case kVK_Control:  // Actual Ctrl key on Mac
            return config::kKeyModLeftControl;
        case kVK_RightControl:
            return config::kKeyModRightControl;
        case kVK_Option:  // Option/Alt key
            return config::kKeyModLeftAlt;
        case kVK_RightOption:
            return config::kKeyModRightAlt;
        case kVK_Command:  // Command key
            return config::kKeyModLeftMeta;
        case kVK_RightCommand:
            return config::kKeyModRightMeta;
        default:
            // For non-modifier keys or unrecognized raw codes, fall back to key parameter
            switch (key) {
                case WXK_SHIFT:
                    return config::kKeyModShift;
                case WXK_RAW_CONTROL:
                    return config::kKeyModControl;
                case WXK_CONTROL:  // Command on Mac
                    return config::kKeyModMeta;
                case WXK_ALT:
                    return config::kKeyModAlt;
                default:
                    return config::kKeyModNone;
            }
    }
#else
    // Fallback for other platforms - no L/R distinction
    (void)event;  // unused
    switch (key) {
        case WXK_SHIFT:
            return config::kKeyModShift;
        case WXK_CONTROL:
            return config::kKeyModControl;
        case WXK_ALT:
            return config::kKeyModAlt;
        default:
            return config::kKeyModNone;
    }
#endif
}

// Convert wxKeyModifier to extended modifier flags.
// On Windows, queries the current key state. On other platforms, uses the
// stored extended modifiers from when the modifier keys were first pressed.
uint32_t WxModifierToExtended(wxKeyModifier wx_mod,
                              const std::unordered_map<wxKeyCode, uint32_t>& key_extended_mods) {
    if (wx_mod == wxMOD_NONE)
        return config::kKeyModNone;

    uint32_t result = config::kKeyModNone;

#ifdef __WXMSW__
    // On Windows, use GetAsyncKeyState to check L/R modifier keys
    (void)key_extended_mods;  // unused on Windows
    if (wx_mod & wxMOD_SHIFT) {
        if (IsKeyPressed(VK_LSHIFT))
            result |= config::kKeyModLeftShift;
        else if (IsKeyPressed(VK_RSHIFT))
            result |= config::kKeyModRightShift;
        else
            result |= config::kKeyModShift;  // Fallback
    }
    if (wx_mod & wxMOD_CONTROL) {
        if (IsKeyPressed(VK_LCONTROL))
            result |= config::kKeyModLeftControl;
        else if (IsKeyPressed(VK_RCONTROL))
            result |= config::kKeyModRightControl;
        else
            result |= config::kKeyModControl;
    }
    if (wx_mod & wxMOD_ALT) {
        if (IsKeyPressed(VK_LMENU))
            result |= config::kKeyModLeftAlt;
        else if (IsKeyPressed(VK_RMENU))
            result |= config::kKeyModRightAlt;
        else
            result |= config::kKeyModAlt;
    }
    if (wx_mod & wxMOD_META) {
        if (IsKeyPressed(VK_LWIN))
            result |= config::kKeyModLeftMeta;
        else if (IsKeyPressed(VK_RWIN))
            result |= config::kKeyModRightMeta;
        else
            result |= config::kKeyModMeta;
    }
#else
    // On macOS and other platforms, use the stored extended modifiers from when
    // the modifier keys were first pressed
    if (wx_mod & wxMOD_SHIFT) {
        auto it = key_extended_mods.find(WXK_SHIFT);
        if (it != key_extended_mods.end())
            result |= it->second;
        else
            result |= config::kKeyModShift;  // Fallback
    }
#ifdef __WXMAC__
    // On macOS: wxMOD_RAW_CONTROL = actual Ctrl key, wxMOD_CONTROL = Command key
    // Both map to Control for cross-platform compatibility
    if (wx_mod & wxMOD_RAW_CONTROL) {
        auto it = key_extended_mods.find(WXK_RAW_CONTROL);
        if (it != key_extended_mods.end())
            result |= it->second;
        else
            result |= config::kKeyModControl;
    }
    if (wx_mod & wxMOD_CONTROL) {
        // Command key on macOS
        auto it = key_extended_mods.find(WXK_CONTROL);
        if (it != key_extended_mods.end())
            result |= it->second;
        else
            result |= config::kKeyModMeta;  // Command key defaults to Meta
    }
#else
    if (wx_mod & wxMOD_CONTROL) {
        auto it = key_extended_mods.find(WXK_CONTROL);
        if (it != key_extended_mods.end())
            result |= it->second;
        else
            result |= config::kKeyModControl;
    }
#endif
    if (wx_mod & wxMOD_ALT) {
        auto it = key_extended_mods.find(WXK_ALT);
        if (it != key_extended_mods.end())
            result |= it->second;
        else
            result |= config::kKeyModAlt;
    }
#ifndef __WXMAC__
    if (wx_mod & wxMOD_META) {
        // Check both Windows keys
        auto it_left = key_extended_mods.find(WXK_WINDOWS_LEFT);
        auto it_right = key_extended_mods.find(WXK_WINDOWS_RIGHT);
        if (it_left != key_extended_mods.end())
            result |= it_left->second;
        else if (it_right != key_extended_mods.end())
            result |= it_right->second;
        else
            result |= config::kKeyModMeta;
    }
#endif
#endif

    return result;
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
    key_extended_mods_.clear();
}

void KeyboardInputHandler::OnKeyDown(wxKeyEvent& event) {
    // Stop propagation of the event.
    event.Skip(false);

    const wxKeyCode key = FilterKeyCode(event);
    const std::unordered_set<wxKeyModifier> mods = GetModifiers(event);

    // Sync active_mods_ with what wxWidgets reports in this event.
    // This handles cases where modifier key releases were missed (e.g., focus loss).
    // Check which modifiers the event says are currently held
    std::unordered_set<wxKeyModifier> event_mods;
    if (event.ControlDown()) event_mods.insert(wxMOD_CONTROL);
    if (event.AltDown()) event_mods.insert(wxMOD_ALT);
    if (event.ShiftDown()) event_mods.insert(wxMOD_SHIFT);
#ifdef __WXMAC__
    if (event.RawControlDown()) event_mods.insert(wxMOD_RAW_CONTROL);
#endif

    // Remove any modifiers we think are held but the event says aren't
    std::vector<wxKeyModifier> to_remove;
    for (const wxKeyModifier mod : active_mods_) {
        if (event_mods.find(mod) == event_mods.end()) {
            to_remove.push_back(mod);
        }
    }

    // If any modifiers were released while we weren't getting events (e.g., during
    // a modal dialog), also clear active_keys_ since those key releases were likely
    // missed too.
    if (!to_remove.empty()) {
        active_keys_.clear();
        active_mod_inputs_.clear();
    }

    for (const wxKeyModifier mod : to_remove) {
        active_mods_.erase(mod);
        // Also clean up the extended mod tracking for this modifier key
        key_extended_mods_.erase(KeyFromModifier(mod));
    }

    // Handle standalone modifier key press.
    // If a modifier key is pressed and we think it's already active, this means
    // the release was missed (e.g., during a modal dialog). Clear all state and
    // treat this as a fresh press.
    if (key == WXK_NONE && !mods.empty()) {
        const wxKeyModifier mod = *mods.begin();
        if (active_mods_.find(mod) != active_mods_.end()) {
            // We think this modifier was already held, but we're getting a fresh
            // key down event for it. This means the release was missed.
            // Clear all state to recover.
            active_keys_.clear();
            active_mods_.clear();
            active_mod_inputs_.clear();
            key_extended_mods_.clear();
        }
    }

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
    // Get extended modifiers with L/R distinction from SDL
    const uint32_t extended_mods = WxModifierToExtended(active_mods, key_extended_mods_);

    std::vector<UserInputEvent::Data> event_data;
    if (key_pressed == WXK_NONE) {
        // A new standalone modifier was pressed, send the event.
        // Get extended modifier flag for this specific modifier key
        const wxKeyCode mod_key = KeyFromModifier(mod_pressed);
        const uint32_t ext_mod = GetExtendedModForModifierKey(mod_key, event);
        // Store the extended modifier for this modifier key so we can use it on release
        key_extended_mods_[mod_key] = ext_mod;
        event_data.emplace_back(config::KeyboardInput(mod_key, ext_mod), true);
    } else {
        // A new key was pressed, send the event with modifiers, first.
        event_data.emplace_back(config::KeyboardInput(key, extended_mods), true);

        if (active_mods != wxMOD_NONE) {
            // Keep track of the key pressed with the active modifiers.
            active_mod_inputs_.emplace(key, extended_mods);
            // Store the extended modifiers for this key so we can use them on release
            key_extended_mods_[key] = extended_mods;

            // Also send the key press event without modifiers.
            event_data.emplace_back(config::KeyboardInput(key, config::kKeyModNone), true);
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
        const wxKeyCode mod_key = KeyFromModifier(mod_released);
        // Use the extended modifier that was stored when the key was pressed
        auto ext_iter = key_extended_mods_.find(mod_key);
        uint32_t ext_mod = (ext_iter != key_extended_mods_.end())
                               ? ext_iter->second
                               : static_cast<uint32_t>(mod_released);
        if (ext_iter != key_extended_mods_.end()) {
            key_extended_mods_.erase(ext_iter);
        }
        event_data.emplace_back(config::KeyboardInput(mod_key, ext_mod), false);
    } else {
        // A key was released.
        if (previous_mods == wxMOD_NONE) {
            // The key was pressed without modifiers, just send the key release event.
            event_data.emplace_back(config::KeyboardInput(key, config::kKeyModNone), false);
        } else {
            // Get the extended modifiers that were stored when the key was pressed
            auto ext_iter = key_extended_mods_.find(key);
            uint32_t extended_mods = (ext_iter != key_extended_mods_.end())
                                         ? ext_iter->second
                                         : WxModifierToExtended(previous_mods, key_extended_mods_);
            if (ext_iter != key_extended_mods_.end()) {
                key_extended_mods_.erase(ext_iter);
            }

            // Check if the key was pressed with the active modifiers.
            const config::KeyboardInput input_with_modifiers(key, extended_mods);
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
            event_data.emplace_back(config::KeyboardInput(key, config::kKeyModNone), false);
        }
    }

    // Also check for any key that were pressed with the previously active
    // modifiers and release them.
    for (const wxKeyCode active_key : active_keys_) {
        auto ext_iter = key_extended_mods_.find(active_key);
        uint32_t extended_mods = (ext_iter != key_extended_mods_.end())
                                     ? ext_iter->second
                                     : WxModifierToExtended(previous_mods, key_extended_mods_);
        const config::KeyboardInput input(active_key, extended_mods);
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
