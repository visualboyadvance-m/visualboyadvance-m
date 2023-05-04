#include "widgets/user-input-ctrl.h"

#include "config/user-input.h"

#include "opts.h"

namespace widgets {

namespace {

int FilterKeyCode(const wxKeyEvent& event) {
    const wxChar keycode = event.GetUnicodeKey();
    if (keycode == WXK_NONE) {
        return event.GetKeyCode();
    }

    if (keycode < 32) {
        switch (keycode) {
            case WXK_BACK:
            case WXK_TAB:
            case WXK_RETURN:
            case WXK_ESCAPE:
                return keycode;
            default:
                return WXK_NONE;
        }
    }

    return keycode;
}

}  // namespace

extern const char UserInputCtrlNameStr[] = "userinputctrl";

UserInputCtrl::UserInputCtrl() : wxTextCtrl() {}

UserInputCtrl::UserInputCtrl(wxWindow* parent,
                             wxWindowID id,
                             const wxString& value,
                             const wxPoint& pos,
                             const wxSize& size,
                             long style,
                             const wxString& name) {
    Create(parent, id, value, pos, size, style, name);
}

UserInputCtrl::~UserInputCtrl() = default;

bool UserInputCtrl::Create(wxWindow* parent,
                           wxWindowID id,
                           const wxString& value,
                           const wxPoint& pos,
                           const wxSize& size,
                           long style,
                           const wxString& name) {
    return wxTextCtrl::Create(parent, id, value, pos, size, style, wxValidator(), name);
}

void UserInputCtrl::SetMultiKey(bool multikey) {
    is_multikey_ = multikey;
    Clear();
}

void UserInputCtrl::SetInputs(const std::set<config::UserInput>& inputs) {
    inputs_ = inputs;
    UpdateText();
}

void UserInputCtrl::Clear() {
    inputs_.clear();
    UpdateText();
}

wxIMPLEMENT_DYNAMIC_CLASS(UserInputCtrl, wxTextCtrl);

// clang-format off
wxBEGIN_EVENT_TABLE(UserInputCtrl, wxTextCtrl)
    EVT_SDLJOY(UserInputCtrl::OnJoy)
    EVT_KEY_DOWN(UserInputCtrl::OnKeyDown)
    EVT_KEY_UP(UserInputCtrl::OnKeyUp)
wxEND_EVENT_TABLE();
// clang-format on

void UserInputCtrl::OnJoy(wxJoyEvent& event) {
    static wxLongLong last_event = 0;

    // Filter consecutive axis motions within 300ms, as this adds two bindings
    // +1/-1 instead of the one intended.
    if ((event.control() == wxJoyControl::AxisPlus || event.control() == wxJoyControl::AxisMinus) &&
        wxGetUTCTimeMillis() - last_event < 300) {
        return;
    }

    last_event = wxGetUTCTimeMillis();

    // Control was unpressed, ignore.
    if (!event.pressed()) {
        return;
    }

    if (!is_multikey_) {
        inputs_.clear();
    }

    inputs_.emplace(event);
    UpdateText();
    Navigate();
}

void UserInputCtrl::OnKeyDown(wxKeyEvent& event) {
    last_mod_ = event.GetModifiers();
    last_key_ = FilterKeyCode(event);
}

void UserInputCtrl::OnKeyUp(wxKeyEvent&) {
    const int mod = last_mod_;
    const int key = last_key_;
    last_mod_ = last_key_ = 0;

    // key is only 0 if we missed the keydown event
    // or if we are being shipped pseudo keypress events
    // either way, just ignore.
    if (!key) {
        return;
    }

    if (!is_multikey_) {
        inputs_.clear();
    }

    inputs_.emplace(key, mod);
    UpdateText();
    Navigate();
}

void UserInputCtrl::UpdateText() {
    static const wxChar kSeparator = L',';

    // Build the display string.
    wxString value;
    for (const auto& input : inputs_) {
        value += input.ToLocalizedString() + kSeparator;
    }

    if (value.empty()) {
        SetValue(wxEmptyString);
    } else {
        SetValue(value.substr(0, value.size() - 1));
    }
}

UserInputCtrlValidator::UserInputCtrlValidator(const config::GameControl game_control) : wxValidator(), game_control_(game_control) {}

wxObject* UserInputCtrlValidator::Clone() const {
    return new UserInputCtrlValidator(game_control_);
}

bool UserInputCtrlValidator::TransferToWindow() {
    UserInputCtrl* control = wxDynamicCast(GetWindow(), UserInputCtrl);
    assert(control);

    control->SetInputs(gopts.game_control_bindings[game_control_]);
    return true;
}

bool UserInputCtrlValidator::TransferFromWindow() {
    UserInputCtrl* control = wxDynamicCast(GetWindow(), UserInputCtrl);
    assert(control);

    gopts.game_control_bindings[game_control_] = control->inputs();
    return true;
}


UserInputCtrlXmlHandler::UserInputCtrlXmlHandler() : wxXmlResourceHandler() {
    AddWindowStyles();
}

wxObject* UserInputCtrlXmlHandler::DoCreateResource() {
    XRC_MAKE_INSTANCE(control, UserInputCtrl)

    control->Create(m_parentAsWindow, GetID(), GetText("value"), GetPosition(), GetSize(),
                    GetStyle() | wxTE_PROCESS_ENTER | wxTE_PROCESS_TAB, GetName());
    SetupWindow(control);

    if (GetBool("multikey", false)) {
        control->SetMultiKey(true);
    }

    return control;
}

bool UserInputCtrlXmlHandler::CanHandle(wxXmlNode* node) {
    return IsOfClass(node, "UserInputCtrl");
}

wxIMPLEMENT_DYNAMIC_CLASS(UserInputCtrlXmlHandler, wxXmlResourceHandler);

}  // namespace widgets
