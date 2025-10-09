#include "wx/widgets/user-input-ctrl.h"

#include <wx/time.h>

#include "core/base/check.h"
#include "wx/config/user-input.h"
#include "wx/widgets/user-input-event.h"

namespace widgets {

namespace {

// Helper callback to disable an event. This is bound dynmically to prevent
// the event from being processed by the base class.
void DisableEvent(wxEvent& event) {
    event.Skip(false);
}

}  // namespace

extern const char UserInputCtrlNameStr[] = "userinputctrl";

UserInputCtrl::UserInputCtrl() : wxTextCtrl() {}

UserInputCtrl::UserInputCtrl(wxWindow* parent,
                             wxWindowID id,
                             const wxPoint& pos,
                             const wxSize& size,
                             long style,
                             const wxString& name) {
    Create(parent, id, pos, size, style, name);
}

UserInputCtrl::~UserInputCtrl() = default;

bool UserInputCtrl::Create(wxWindow* parent,
                           wxWindowID id,
                           const wxPoint& pos,
                           const wxSize& size,
                           long style,
                           const wxString& name) {
    this->Bind(VBAM_EVT_USER_INPUT, &UserInputCtrl::OnUserInput, this);
    this->Bind(wxEVT_SET_FOCUS, [this](wxFocusEvent& event) {
        last_focus_time_ = wxGetUTCTimeMillis();
        event.Skip();
    });

    // Diable key events.
    this->Bind(wxEVT_CHAR, &DisableEvent);
    this->Bind(wxEVT_KEY_DOWN, &DisableEvent);

    return wxTextCtrl::Create(parent, id, wxEmptyString, pos, size, style, wxValidator(), name);
}

void UserInputCtrl::SetMultiKey(bool multikey) {
    is_multikey_ = multikey;
    Clear();
}

void UserInputCtrl::SetInputs(const std::unordered_set<config::UserInput>& inputs) {
    inputs_.clear();
    inputs_.insert(inputs.begin(), inputs.end());
    UpdateText();
}

config::UserInput UserInputCtrl::SingleInput() const {
    VBAM_CHECK(!is_multikey_);
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

void UserInputCtrl::OnUserInput(UserInputEvent& event) {
    // Find the first pressed input.
    nonstd::optional<config::UserInput> input = event.FirstReleasedInput();

    if (input == nonstd::nullopt) {
        // No pressed inputs.
        return;
    }

    static const wxLongLong kInterval = 100;
    if (wxGetUTCTimeMillis() - last_focus_time_ < kInterval) {
        // Ignore events sent very shortly after focus. This is used to ignore
        // some spurious joystick events like an accidental axis motion.
        event.Skip();
        return;
    }

    if (!is_multikey_) {
        inputs_.clear();
    }

    inputs_.insert(std::move(input.value()));
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
        // Remove the trailing comma.
        SetValue(value.substr(0, value.size() - 1));
    }
}

UserInputCtrlXmlHandler::UserInputCtrlXmlHandler() : wxXmlResourceHandler() {
    AddWindowStyles();
}

wxObject* UserInputCtrlXmlHandler::DoCreateResource() {
    XRC_MAKE_INSTANCE(control, UserInputCtrl)

    control->Create(m_parentAsWindow, GetID(), GetPosition(), GetSize(),
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
