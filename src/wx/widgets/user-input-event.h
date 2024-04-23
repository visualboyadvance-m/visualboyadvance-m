#ifndef WX_WIDGETS_USER_INPUT_EVENT_H_
#define WX_WIDGETS_USER_INPUT_EVENT_H_

#include <unordered_set>

#include <wx/clntdata.h>
#include <wx/event.h>

#include "wx/config/user-input.h"

namespace widgets {

// Event fired when a user input is pressed or released. The event contains the
// user input that was pressed or released.
class UserInputEvent final : public wxEvent {
public:
    UserInputEvent(const config::UserInput& input, bool pressed);
    virtual ~UserInputEvent() override = default;

    // wxEvent implementation.
    wxEvent* Clone() const override;

    const config::UserInput& input() const { return input_; }

private:
    const config::UserInput input_;
};

// Object that is used to fire user input events when a key is pressed or
// released. To use this object, attach it to a wxWindow and listen for
// the VBAM_EVT_USER_INPUT_DOWN and VBAM_EVT_USER_INPUT_UP events.
class UserInputEventSender final : public wxClientData {
public:
    // `window` must not be nullptr. Will assert otherwise.
    // `window` must outlive this object.
    explicit UserInputEventSender(wxWindow* const window);
    ~UserInputEventSender() override;

    // Disable copy and copy assignment.
    UserInputEventSender(const UserInputEventSender&) = delete;
    UserInputEventSender& operator=(const UserInputEventSender&) = delete;

private:
    // Keyboard event handlers.
    void OnKeyDown(wxKeyEvent& event);
    void OnKeyUp(wxKeyEvent& event);

    // Resets the internal state. Called on focus in/out.
    void Reset(wxFocusEvent& event);

    std::unordered_set<wxKeyCode> active_keys_;
    std::unordered_set<wxKeyModifier> active_mods_;
    std::unordered_set<config::KeyboardInput> active_mod_inputs_;

    // The wxWindow this object is attached to.
    // Must outlive this object.
    wxWindow* const window_;
};

}  // namespace widgets

// Fired when a user input is pressed.
wxDECLARE_EVENT(VBAM_EVT_USER_INPUT_DOWN, widgets::UserInputEvent);
// Fired when a user input is released.
wxDECLARE_EVENT(VBAM_EVT_USER_INPUT_UP, widgets::UserInputEvent);

#endif  // WX_WIDGETS_USER_INPUT_EVENT_H_
