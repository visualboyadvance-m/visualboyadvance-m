#include "qt/widgets/keyboard-input-handler.h"

#include <vector>

#include <QKeyEvent>

#include "core/base/check.h"
#include "qt/config/user-input.h"
#include "qt/widgets/input-dispatcher.h"
#include "qt/widgets/sdl-poller.h"

#if defined(_WIN32)
#include <windows.h>
#endif

namespace widgets {

namespace {

// Qt key codes of the standalone modifier keys. On macOS Qt swaps Control and
// Meta: Qt::Key_Control is the Command key and Qt::Key_Meta the Control key,
// matching Qt::ControlModifier / Qt::MetaModifier. The KeyModFlag values follow
// the Qt semantics (kKeyModControl is the Qt::ControlModifier key), so a
// "CTRL+O" binding is Command+O on macOS, like every other Qt application.
bool IsModifierKey(int key) {
    switch (key) {
        case Qt::Key_Shift:
        case Qt::Key_Control:
        case Qt::Key_Alt:
        case Qt::Key_AltGr:
        case Qt::Key_Meta:
        case Qt::Key_Super_L:
        case Qt::Key_Super_R:
        case Qt::Key_Hyper_L:
        case Qt::Key_Hyper_R:
            return true;
        default:
            return false;
    }
}

// Filters the received key code in the key event for something we can use.
// Returns Qt::Key_unknown for modifier keys so we can differentiate between a
// key press and a modifier press.
int FilterKeyCode(const QKeyEvent* event) {
    const int key = event->key();
    if (key == 0 || key == Qt::Key_unknown || IsModifierKey(key)) {
        return Qt::Key_unknown;
    }
    return key;
}

// The generic modifier flag a standalone modifier key stands for.
uint32_t GenericModForKey(int key) {
    switch (key) {
        case Qt::Key_Shift:
            return config::kKeyModShift;
        case Qt::Key_Control:
            return config::kKeyModControl;
        case Qt::Key_Alt:
        case Qt::Key_AltGr:
            return config::kKeyModAlt;
        case Qt::Key_Meta:
        case Qt::Key_Super_L:
        case Qt::Key_Super_R:
        case Qt::Key_Hyper_L:
        case Qt::Key_Hyper_R:
            return config::kKeyModMeta;
        default:
            return config::kKeyModNone;
    }
}

// Returns the canonical key code for a standalone modifier.
int KeyFromModifier(uint32_t mod) {
    switch (mod) {
        case config::kKeyModControl:
            return Qt::Key_Control;
        case config::kKeyModAlt:
            return Qt::Key_Alt;
        case config::kKeyModShift:
            return Qt::Key_Shift;
        case config::kKeyModMeta:
            return Qt::Key_Meta;
        default:
            return Qt::Key_unknown;
    }
}

// Returns the set of modifiers for the given key event.
std::unordered_set<uint32_t> GetModifiers(const QKeyEvent* event) {
    // Standalone modifiers are treated as keys and do not set the keyboard
    // modifiers.
    const uint32_t self = GenericModForKey(event->key());
    if (self != config::kKeyModNone) {
        return {self};
    }

    std::unordered_set<uint32_t> mods;
    const Qt::KeyboardModifiers qmods = event->modifiers();
    if (qmods & Qt::ControlModifier) {
        mods.insert(config::kKeyModControl);
    }
    if (qmods & Qt::AltModifier) {
        mods.insert(config::kKeyModAlt);
    }
    if (qmods & Qt::ShiftModifier) {
        mods.insert(config::kKeyModShift);
    }
    if (qmods & Qt::MetaModifier) {
        mods.insert(config::kKeyModMeta);
    }
    return mods;
}

// Modifiers the event reports as currently held (not counting the key itself).
std::unordered_set<uint32_t> GetEventModifiers(const QKeyEvent* event) {
    std::unordered_set<uint32_t> mods;
    const Qt::KeyboardModifiers qmods = event->modifiers();
    if (qmods & Qt::ControlModifier) {
        mods.insert(config::kKeyModControl);
    }
    if (qmods & Qt::AltModifier) {
        mods.insert(config::kKeyModAlt);
    }
    if (qmods & Qt::ShiftModifier) {
        mods.insert(config::kKeyModShift);
    }
    if (qmods & Qt::MetaModifier) {
        mods.insert(config::kKeyModMeta);
    }
    return mods;
}

// Builds a modifier bit mask from a set of modifiers.
uint32_t GetModifiersFromSet(const std::unordered_set<uint32_t>& mods) {
    uint32_t mod = config::kKeyModNone;
    for (const uint32_t m : mods) {
        mod |= m;
    }
    return mod;
}

#if defined(_WIN32)
bool IsKeyPressed(int vk) {
    return (GetAsyncKeyState(vk) & 0x8000) != 0;
}
#endif

#if defined(__APPLE__)
// macOS virtual key codes for modifier keys (Carbon HIToolbox Events.h), as
// returned by QKeyEvent::nativeVirtualKey().
constexpr quint32 kVK_Shift = 0x38;
constexpr quint32 kVK_RightShift = 0x3C;
constexpr quint32 kVK_Control = 0x3B;
constexpr quint32 kVK_RightControl = 0x3E;
constexpr quint32 kVK_Option = 0x3A;
constexpr quint32 kVK_RightOption = 0x3D;
constexpr quint32 kVK_Command = 0x37;
constexpr quint32 kVK_RightCommand = 0x36;
#endif

#if !defined(_WIN32) && !defined(__APPLE__)
// X11 keysyms for left/right modifiers, as returned by
// QKeyEvent::nativeVirtualKey() on both xcb and wayland.
constexpr quint32 kXK_Shift_L = 0xffe1;
constexpr quint32 kXK_Shift_R = 0xffe2;
constexpr quint32 kXK_Control_L = 0xffe3;
constexpr quint32 kXK_Control_R = 0xffe4;
constexpr quint32 kXK_Meta_L = 0xffe7;
constexpr quint32 kXK_Meta_R = 0xffe8;
constexpr quint32 kXK_Alt_L = 0xffe9;
constexpr quint32 kXK_Alt_R = 0xffea;
constexpr quint32 kXK_Super_L = 0xffeb;
constexpr quint32 kXK_Super_R = 0xffec;
#endif

// Picks the L/R flag for `generic` out of `sdl_mods`, or kKeyModNone.
uint32_t SdlSideForModifier(uint32_t generic, uint32_t sdl_mods) {
    switch (generic) {
        case config::kKeyModShift:
            return sdl_mods & (config::kKeyModLeftShift | config::kKeyModRightShift);
        case config::kKeyModControl:
            return sdl_mods & (config::kKeyModLeftControl | config::kKeyModRightControl);
        case config::kKeyModAlt:
            return sdl_mods & (config::kKeyModLeftAlt | config::kKeyModRightAlt);
        case config::kKeyModMeta:
            return sdl_mods & (config::kKeyModLeftMeta | config::kKeyModRightMeta);
        default:
            return config::kKeyModNone;
    }
}

// Get the extended modifier flags for a standalone modifier key press. Uses
// the native key code of the event to determine L/R (keysym on X11/Wayland,
// virtual key on macOS), GetAsyncKeyState on Windows, then SDL's modifier
// state, then falls back to the generic flag.
uint32_t GetExtendedModForModifierKey(int key, const QKeyEvent* event) {
    const uint32_t generic = GenericModForKey(key);
    if (generic == config::kKeyModNone) {
        return config::kKeyModNone;
    }

#if defined(_WIN32)
    (void)event;
    switch (generic) {
        case config::kKeyModShift:
            if (IsKeyPressed(VK_LSHIFT))
                return config::kKeyModLeftShift;
            if (IsKeyPressed(VK_RSHIFT))
                return config::kKeyModRightShift;
            break;
        case config::kKeyModControl:
            if (IsKeyPressed(VK_LCONTROL))
                return config::kKeyModLeftControl;
            if (IsKeyPressed(VK_RCONTROL))
                return config::kKeyModRightControl;
            break;
        case config::kKeyModAlt:
            if (IsKeyPressed(VK_LMENU))
                return config::kKeyModLeftAlt;
            if (IsKeyPressed(VK_RMENU))
                return config::kKeyModRightAlt;
            break;
        case config::kKeyModMeta:
            if (key == Qt::Key_Super_L || IsKeyPressed(VK_LWIN))
                return config::kKeyModLeftMeta;
            if (key == Qt::Key_Super_R || IsKeyPressed(VK_RWIN))
                return config::kKeyModRightMeta;
            break;
    }
#elif defined(__APPLE__)
    // Qt::Key_Control is Command and Qt::Key_Meta is Control on macOS.
    switch (event->nativeVirtualKey()) {
        case kVK_Shift:
            return config::kKeyModLeftShift;
        case kVK_RightShift:
            return config::kKeyModRightShift;
        case kVK_Command:
            return config::kKeyModLeftControl;
        case kVK_RightCommand:
            return config::kKeyModRightControl;
        case kVK_Option:
            return config::kKeyModLeftAlt;
        case kVK_RightOption:
            return config::kKeyModRightAlt;
        case kVK_Control:
            return config::kKeyModLeftMeta;
        case kVK_RightControl:
            return config::kKeyModRightMeta;
        default:
            break;
    }
#else
    switch (event->nativeVirtualKey()) {
        case kXK_Shift_L:
            return config::kKeyModLeftShift;
        case kXK_Shift_R:
            return config::kKeyModRightShift;
        case kXK_Control_L:
            return config::kKeyModLeftControl;
        case kXK_Control_R:
            return config::kKeyModRightControl;
        case kXK_Alt_L:
            return config::kKeyModLeftAlt;
        case kXK_Alt_R:
            return config::kKeyModRightAlt;
        case kXK_Meta_L:
        case kXK_Super_L:
            return config::kKeyModLeftMeta;
        case kXK_Meta_R:
        case kXK_Super_R:
            return config::kKeyModRightMeta;
        default:
            break;
    }
    if (key == Qt::Key_Super_L || key == Qt::Key_Hyper_L)
        return config::kKeyModLeftMeta;
    if (key == Qt::Key_Super_R || key == Qt::Key_Hyper_R)
        return config::kKeyModRightMeta;
#endif

    // SDL tracks the physical keyboard when it owns a window; use its L/R
    // state when it has one.
    const uint32_t sdl_side = SdlSideForModifier(generic, SdlPoller::GetCurrentSdlMods());
    if (sdl_side != config::kKeyModNone) {
        // Only one side can be reported for a single key press.
        return sdl_side & (sdl_side - 1) ? generic : sdl_side;
    }

    return generic;
}

// Convert generic modifier flags to extended modifier flags. On Windows,
// queries the current key state. On other platforms, uses the stored extended
// modifiers from when the modifier keys were first pressed, then SDL's state.
uint32_t GenericModifiersToExtended(uint32_t generic_mods,
                                    const std::unordered_map<int, uint32_t>& key_extended_mods) {
    if (generic_mods == config::kKeyModNone)
        return config::kKeyModNone;

    uint32_t result = config::kKeyModNone;

#if defined(_WIN32)
    (void)key_extended_mods;
    if (generic_mods & config::kKeyModShift) {
        if (IsKeyPressed(VK_LSHIFT))
            result |= config::kKeyModLeftShift;
        else if (IsKeyPressed(VK_RSHIFT))
            result |= config::kKeyModRightShift;
        else
            result |= config::kKeyModShift;
    }
    if (generic_mods & config::kKeyModControl) {
        if (IsKeyPressed(VK_LCONTROL))
            result |= config::kKeyModLeftControl;
        else if (IsKeyPressed(VK_RCONTROL))
            result |= config::kKeyModRightControl;
        else
            result |= config::kKeyModControl;
    }
    if (generic_mods & config::kKeyModAlt) {
        if (IsKeyPressed(VK_LMENU))
            result |= config::kKeyModLeftAlt;
        else if (IsKeyPressed(VK_RMENU))
            result |= config::kKeyModRightAlt;
        else
            result |= config::kKeyModAlt;
    }
    if (generic_mods & config::kKeyModMeta) {
        if (IsKeyPressed(VK_LWIN))
            result |= config::kKeyModLeftMeta;
        else if (IsKeyPressed(VK_RWIN))
            result |= config::kKeyModRightMeta;
        else
            result |= config::kKeyModMeta;
    }
#else
    const uint32_t sdl_mods = SdlPoller::GetCurrentSdlMods();
    static constexpr uint32_t kGenerics[] = {config::kKeyModShift, config::kKeyModControl,
                                             config::kKeyModAlt, config::kKeyModMeta};
    for (const uint32_t generic : kGenerics) {
        if (!(generic_mods & generic))
            continue;

        const auto it = key_extended_mods.find(KeyFromModifier(generic));
        if (it != key_extended_mods.end()) {
            result |= it->second;
            continue;
        }

        const uint32_t sdl_side = SdlSideForModifier(generic, sdl_mods);
        if (sdl_side != config::kKeyModNone && !(sdl_side & (sdl_side - 1))) {
            result |= sdl_side;
            continue;
        }

        result |= generic;
    }
#endif

    return result;
}

}  // namespace

KeyboardInputHandler::KeyboardInputHandler(InputDispatcher* dispatcher, InputSink sync_sink)
    : dispatcher_(dispatcher), sync_sink_(std::move(sync_sink)) {
    VBAM_CHECK(dispatcher_);
    VBAM_CHECK(sync_sink_);
}

KeyboardInputHandler::~KeyboardInputHandler() = default;

uint32_t KeyboardInputHandler::CurrentModFlags(const QKeyEvent* event) {
    return GetModifiersFromSet(GetEventModifiers(event));
}

uint32_t KeyboardInputHandler::ModFlagForKey(int qt_key, uint32_t extended_hint) {
    const uint32_t generic = GenericModForKey(qt_key);
    if (generic == config::kKeyModNone) {
        return config::kKeyModNone;
    }
    const uint32_t side = SdlSideForModifier(generic, extended_hint);
    if (side != config::kKeyModNone && !(side & (side - 1))) {
        return side;
    }
    return generic;
}

bool KeyboardInputHandler::ProcessKeyEvent(QKeyEvent* event) {
    if (!event) {
        return false;
    }

    // Auto-repeat presses carry no new information; the "already active" test
    // below would drop them anyway, but skipping them here avoids the
    // reconciliation logic misreading them.
    if (event->type() == QEvent::KeyPress) {
        if (event->isAutoRepeat()) {
            return false;
        }
        OnKeyDown(event);
    } else if (event->type() == QEvent::KeyRelease) {
        if (event->isAutoRepeat()) {
            return false;
        }
        OnKeyUp(event);
    } else {
        return false;
    }

    return last_event_produced_inputs_;
}

// Releases every input this handler currently believes is held, through the
// synchronous sink, and forgets them.
//
// The tracking sets are cleared in several places to recover from missed key
// events -- a modal dialog, a focus change, a modifier whose release never
// arrived. Clearing them alone is not enough: the joypad bits those inputs set
// are owned by the sink, and once the handler has forgotten a key its eventual
// release finds nothing to match and returns early, so the bit is never cleared
// and the button stays down for good. Release first, then forget.
void KeyboardInputHandler::ReleaseAllTracked() {
    for (const config::KeyboardInput& input : active_mod_inputs_) {
        sync_sink_(input, false);
    }
    for (const int key : active_keys_) {
        sync_sink_(config::KeyboardInput(key, config::kKeyModNone), false);
    }
    for (const uint32_t mod : active_mods_) {
        const int mod_key = KeyFromModifier(mod);
        if (mod_key == Qt::Key_unknown) {
            continue;
        }
        const auto iter = key_extended_mods_.find(mod_key);
        const uint32_t ext_mod = (iter != key_extended_mods_.end()) ? iter->second : mod;
        sync_sink_(config::KeyboardInput(mod_key, ext_mod), false);
    }

    active_keys_.clear();
    active_mods_.clear();
    active_mod_inputs_.clear();
    key_extended_mods_.clear();
}

void KeyboardInputHandler::Reset() {
    ReleaseAllTracked();
}

void KeyboardInputHandler::OnKeyDown(QKeyEvent* event) {
    last_event_produced_inputs_ = false;

    const int key = FilterKeyCode(event);
    const std::unordered_set<uint32_t> mods = GetModifiers(event);

    // Sync active_mods_ with what Qt reports in this event. This handles cases
    // where modifier key releases were missed (e.g., focus loss).
    const std::unordered_set<uint32_t> event_mods = GetEventModifiers(event);

    // A modifier's own key-down does not necessarily report that modifier as
    // held -- the flags describe the state the key event is modifying, not the
    // key itself -- so its absence there says nothing about whether it is down.
    // Exempt it, or every press of a held modifier reads as a release and takes
    // everything else held down with it.
    std::unordered_set<uint32_t> self_mods;
    if (key == Qt::Key_unknown) {
        for (const uint32_t mod : mods) {
            self_mods.insert(mod);
        }
    }

    // Remove any modifiers we think are held but the event says aren't.
    std::vector<uint32_t> to_remove;
    for (const uint32_t mod : active_mods_) {
        if (event_mods.find(mod) == event_mods.end() && self_mods.find(mod) == self_mods.end()) {
            to_remove.push_back(mod);
        }
    }

    // If any modifiers were released while we weren't getting events (e.g.,
    // during a modal dialog), also clear active_keys_ since those key releases
    // were likely missed too. Release everything before forgetting it -- see
    // ReleaseAllTracked().
    if (!to_remove.empty()) {
        ReleaseAllTracked();
    }

    int key_pressed = Qt::Key_unknown;
    if (key != Qt::Key_unknown) {
        if (active_keys_.find(key) == active_keys_.end()) {
            // Key was not pressed before.
            key_pressed = key;
            active_keys_.insert(key);
        }
    }

    uint32_t mod_pressed = config::kKeyModNone;
    for (const uint32_t mod : mods) {
        if (active_mods_.find(mod) == active_mods_.end()) {
            // Mod was not pressed before.
            active_mods_.insert(mod);
            mod_pressed = mod;
            break;
        }
    }

    if (key_pressed == Qt::Key_unknown && mod_pressed == config::kKeyModNone) {
        // No new keys or mods were pressed.
        return;
    }

    const uint32_t active_mods = GetModifiersFromSet(active_mods_);
    // Get extended modifiers with L/R distinction.
    const uint32_t extended_mods = GenericModifiersToExtended(active_mods, key_extended_mods_);

    std::vector<UserInputBatch::Data> event_data;
    if (key_pressed == Qt::Key_unknown) {
        // A new standalone modifier was pressed, send the event.
        const int mod_key = KeyFromModifier(mod_pressed);
        const uint32_t ext_mod = GetExtendedModForModifierKey(event->key(), event);
        // Store the extended modifier for this modifier key so we can use it
        // on release.
        key_extended_mods_[mod_key] = ext_mod;
        event_data.emplace_back(config::KeyboardInput(mod_key, ext_mod), true);
    } else {
        // A new key was pressed, send the event with modifiers, first.
        event_data.emplace_back(config::KeyboardInput(key, extended_mods), true);

        if (active_mods != config::kKeyModNone) {
            // Keep track of the key pressed with the active modifiers.
            active_mod_inputs_.emplace(key, extended_mods);
            // Store the extended modifiers for this key so we can use them on
            // release.
            key_extended_mods_[key] = extended_mods;

            // Also send the key press event without modifiers.
            event_data.emplace_back(config::KeyboardInput(key, config::kKeyModNone), true);
        }
    }

    // Synchronous joypad-state update (always). The sink filters non-game
    // commands internally. This path is focus-independent and is what makes
    // joypad input work both in foreground and (with the background-input
    // option enabled) in background.
    for (const auto& data : event_data) {
        sync_sink_(data.input, data.pressed);
    }

    // Dispatch to the rest of the consumer chain (GameArea, MemView, the
    // hotkey-config dialog and the shortcut matcher in MainWindow). The focus
    // gate for "no hotkeys when unfocused" lives at the shortcut-matching
    // site, not here.
    UserInputBatch batch;
    batch.data = std::move(event_data);
    last_event_produced_inputs_ = true;
    dispatcher_->Dispatch(batch);
}

void KeyboardInputHandler::OnKeyUp(QKeyEvent* event) {
    last_event_produced_inputs_ = false;

    const int key = FilterKeyCode(event);
    const std::unordered_set<uint32_t> mods = GetModifiers(event);
    const uint32_t previous_mods = GetModifiersFromSet(active_mods_);

    int key_released = Qt::Key_unknown;
    if (key != Qt::Key_unknown) {
        auto iter = active_keys_.find(key);
        if (iter != active_keys_.end()) {
            // Key was pressed before.
            key_released = key;
            active_keys_.erase(iter);
        }
    }

    uint32_t mod_released = config::kKeyModNone;
    if (key == Qt::Key_unknown) {
        // Only look for a standalone modifier if the event carries no key at
        // all. Testing key_released instead also catches a key that was
        // released but was not in active_keys_ -- a duplicate release -- which
        // would then read the modifier still held in the event as the modifier
        // having just gone up.
        for (const uint32_t mod : mods) {
            auto iter = active_mods_.find(mod);
            if (iter != active_mods_.end()) {
                // Mod was pressed before.
                mod_released = mod;
                active_mods_.erase(iter);
                break;
            }
        }
    }

    if (key_released == Qt::Key_unknown && mod_released == config::kKeyModNone) {
        // No keys or mods were released.
        return;
    }

    std::vector<UserInputBatch::Data> event_data;
    if (key_released == Qt::Key_unknown) {
        // A standalone modifier was released, send it.
        const int mod_key = KeyFromModifier(mod_released);
        // Use the extended modifier that was stored when the key was pressed.
        auto ext_iter = key_extended_mods_.find(mod_key);
        const uint32_t ext_mod =
            (ext_iter != key_extended_mods_.end()) ? ext_iter->second : mod_released;
        if (ext_iter != key_extended_mods_.end()) {
            key_extended_mods_.erase(ext_iter);
        }
        event_data.emplace_back(config::KeyboardInput(mod_key, ext_mod), false);

        // A key pressed while this modifier was held was reported as the
        // combination, and that combination is no longer held once the
        // modifier goes up. Release it here rather than waiting for the key
        // itself: by then the modifier is gone from active_mods_, so the key's
        // own release reports the plain form and the combination is never
        // matched by anything, leaving it stuck down.
        for (auto iter = active_mod_inputs_.begin(); iter != active_mod_inputs_.end();) {
            // Compare through the generic modifier: an entry recorded with the
            // left/right-specific flag does not share bits with the generic
            // one, so a raw mask misses it.
            if (config::ToGenericModifiers(iter->mod_extended()) & mod_released) {
                event_data.emplace_back(*iter, false);
                iter = active_mod_inputs_.erase(iter);
            } else {
                ++iter;
            }
        }
    } else {
        // A key was released.
        if (previous_mods == config::kKeyModNone) {
            // The key was pressed without modifiers, just send the key release
            // event.
            event_data.emplace_back(config::KeyboardInput(key, config::kKeyModNone), false);
        } else {
            // Get the extended modifiers that were stored when the key was
            // pressed.
            auto ext_iter = key_extended_mods_.find(key);
            const uint32_t extended_mods =
                (ext_iter != key_extended_mods_.end())
                    ? ext_iter->second
                    : GenericModifiersToExtended(previous_mods, key_extended_mods_);
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
    for (const int active_key : active_keys_) {
        auto ext_iter = key_extended_mods_.find(active_key);
        const uint32_t extended_mods =
            (ext_iter != key_extended_mods_.end())
                ? ext_iter->second
                : GenericModifiersToExtended(previous_mods, key_extended_mods_);
        const config::KeyboardInput input(active_key, extended_mods);
        auto iter = active_mod_inputs_.find(input);
        if (iter != active_mod_inputs_.end()) {
            active_mod_inputs_.erase(iter);
            event_data.emplace_back(input, false);
        }
    }

    for (const auto& data : event_data) {
        active_mod_inputs_.erase(data.input.keyboard_input());
    }

    // Synchronous joypad-state update (always). See OnKeyDown for the
    // rationale on why this happens unconditionally.
    for (const auto& data : event_data) {
        sync_sink_(data.input, data.pressed);
    }

    UserInputBatch batch;
    batch.data = std::move(event_data);
    last_event_produced_inputs_ = true;
    dispatcher_->Dispatch(batch);
}

}  // namespace widgets
