#include "wx/widgets/user-input-ctrl.h"

#include <wx/time.h>

#include "wx/config/user-input.h"
#include "wx/opts.h"
#include "wx/widgets/user-input-event.h"

namespace widgets {

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
    this->SetClientObject(new UserInputEventSender(this));
    this->Bind(VBAM_EVT_USER_INPUT_UP, &UserInputCtrl::OnUserInputUp, this);
    this->Bind(wxEVT_SET_FOCUS, [this](wxFocusEvent& event) {
        is_navigating_away_ = false;
        last_focus_time_ = wxGetUTCTimeMillis();
        event.Skip();
    });
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

config::UserInput UserInputCtrl::SingleInput() const {
    assert(!is_multikey_);
    if (inputs_.empty()) {
        return config::UserInput();
    }
    return *inputs_.begin();
}

void UserInputCtrl::Clear() {
    inputs_.clear();
    UpdateText();
}

wxIMPLEMENT_DYNAMIC_CLASS(UserInputCtrl, wxTextCtrl);

void UserInputCtrl::OnUserInputUp(UserInputEvent& event) {
    static const wxLongLong kInterval = 100;
    if (wxGetUTCTimeMillis() - last_focus_time_ < kInterval) {
        // Ignore events sent very shortly after focus. This is used to ignore
        // some spurious joystick events like an accidental axis motion.
        event.Skip();
        return;
    }

    if (is_navigating_away_) {
        // Ignore events sent after the control has been navigated away from.
        event.Skip();
        return;
    }

    if (!is_multikey_) {
        inputs_.clear();
    }

    inputs_.insert(event.input());
    UpdateText();
    Navigate();
    is_navigating_away_ = true;
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
        // Remove the trailing comma.
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
