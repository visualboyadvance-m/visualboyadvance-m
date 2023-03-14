#include "widgets/group-check-box.h"

namespace widgets {

namespace {

wxWindow* FindTopLevelWindow(wxWindow* window) {
    while (window != nullptr && !window->IsTopLevel()) {
        window = window->GetParent();
    }
    assert(window);
    return window;
}

GroupCheckBox* FindGroupCheckBox(wxWindow* window,
                                 const wxString& name,
                                 GroupCheckBox* current) {
    if (window == current) {
        return nullptr;
    }

    if (window->IsKindOf(wxCLASSINFO(GroupCheckBox))) {
        GroupCheckBox* check_box = wxDynamicCast(window, GroupCheckBox);
        if (check_box->GetName() == name) {
            return check_box;
        }
    }

    for (wxWindow* child : window->GetChildren()) {
        GroupCheckBox* check_box = FindGroupCheckBox(child, name, current);
        if (check_box) {
            return check_box;
        }
    }

    return nullptr;
}

}  // namespace

extern const char GroupCheckBoxNameStr[] = "groupcheck";

GroupCheckBox::GroupCheckBox() : wxCheckBox(), next_(this) {}

GroupCheckBox::GroupCheckBox(wxWindow* parent,
                             wxWindowID id,
                             const wxString& label,
                             const wxPoint& pos,
                             const wxSize& size,
                             long style,
                             const wxValidator& validator,
                             const wxString& name)
    : next_(this) {
    Create(parent, id, label, pos, size, style, validator, name);
}

GroupCheckBox::~GroupCheckBox() {
    RemoveFromGroup();
}

bool GroupCheckBox::Create(wxWindow* parent,
                           wxWindowID id,
                           const wxString& label,
                           const wxPoint& pos,
                           const wxSize& size,
                           long style,
                           const wxValidator& validator,
                           const wxString& name) {
    if (!wxCheckBox::Create(parent, id, label, pos, size, style, validator,
                            name)) {
        return false;
    }

    AddToGroup();
    initialized_ = true;
    return true;
}

void GroupCheckBox::AddToGroup() {
    assert(next_ == this);

    if (GetName().IsEmpty()) {
        // No name means a singleton.
        return;
    }

    // Find another GroupCheckBox with the same name as this.
    GroupCheckBox* other_box =
        FindGroupCheckBox(FindTopLevelWindow(this), GetName(), this);
    if (!other_box) {
        return;
    }

    // Find the previous GroupCheckBox to put this in the circular linked list.
    GroupCheckBox* prev_box = other_box;
    while (prev_box->next_ != other_box) {
        prev_box = prev_box->next_;
    }

    next_ = other_box;
    prev_box->next_ = this;
}

void GroupCheckBox::RemoveFromGroup() {
    GroupCheckBox* prev_box = this;
    while (prev_box->next_ != this) {
        prev_box = prev_box->next_;
    }

    // Update the link.
    prev_box->next_ = next_;
    next_ = this;
}

void GroupCheckBox::OnCheck(wxCommandEvent& event) {
    UpdateGroup();

    // Let the event propagate.
    event.Skip();
}

void GroupCheckBox::UpdateGroup() {
    if (next_ == this) {
        // Nothing more to do if not part of a group.
        return;
    }

    if (GetValue()) {
        for (GroupCheckBox* check_box = next_; check_box != this;
             check_box = check_box->next_) {
            check_box->SetValue(false);
        }
    } else {
        // Find a checked GroupCheckBox.
        GroupCheckBox* check_box = next_;
        while (check_box != this) {
            if (check_box->GetValue()) {
                break;
            }
            check_box = check_box->next_;
        }

        // Ensure at least one GroupCheckBox is checked.
        if (check_box == this) {
            SetValue(true);
        }
    }
}

void GroupCheckBox::SetValue(bool val) {
    wxCheckBox::SetValue(val);
    if (initialized_) {
        UpdateGroup();
    }
}

void GroupCheckBox::SetName(const wxString& name) {
    wxCheckBox::SetName(name);
    if (initialized_) {
        RemoveFromGroup();
        AddToGroup();
    }
}

wxIMPLEMENT_DYNAMIC_CLASS(GroupCheckBox, wxCheckBox);

// clang-format off
wxBEGIN_EVENT_TABLE(GroupCheckBox, wxCheckBox)
    EVT_CHECKBOX(wxID_ANY, GroupCheckBox::OnCheck)
wxEND_EVENT_TABLE();
// clang-format on

GroupCheckBoxXmlHandler::GroupCheckBoxXmlHandler() : wxXmlResourceHandler() {
    AddWindowStyles();
}

wxObject* GroupCheckBoxXmlHandler::DoCreateResource() {
    XRC_MAKE_INSTANCE(control, GroupCheckBox)

    control->Create(m_parentAsWindow, GetID(), GetText("label"), GetPosition(),
                    GetSize(), GetStyle(), wxDefaultValidator, GetName());

    SetupWindow(control);

    return control;
}

bool GroupCheckBoxXmlHandler::CanHandle(wxXmlNode* node) {
    return IsOfClass(node, "GroupCheckBox");
}

wxIMPLEMENT_DYNAMIC_CLASS(GroupCheckBoxXmlHandler, wxXmlResourceHandler);

}  // namespace widgets
