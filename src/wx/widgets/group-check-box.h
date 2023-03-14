#ifndef VBAM_WX_WIDGETS_GROUP_CHECK_BOX_H_
#define VBAM_WX_WIDGETS_GROUP_CHECK_BOX_H_

#include <wx/checkbox.h>
#include <wx/string.h>
#include <wx/xrc/xmlres.h>

namespace widgets {

extern const char GroupCheckBoxNameStr[];

// A custom check box that is part of a group where only one element can be
// active at any given time. The other check boxes in the group are identified
// as GroupCheckBoxes with the same name within the same top-level window.
//
// Internally, the other GroupCheckBoxes in the same group are tracked in a
// circular linked list, via the `next_` pointer. On initialization, this links
// to this object. The list is updated when the GroupCheckBox name is set or
// reset.
class GroupCheckBox : public wxCheckBox {
public:
    GroupCheckBox();
    GroupCheckBox(
        wxWindow* parent,
        wxWindowID id,
        const wxString& label,
        const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize,
        long style = 0,
        const wxValidator& validator = wxDefaultValidator,
        const wxString& name = wxString::FromAscii(GroupCheckBoxNameStr));
    ~GroupCheckBox() override;

    bool Create(
        wxWindow* parent,
        wxWindowID id,
        const wxString& label,
        const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize,
        long style = 0,
        const wxValidator& validator = wxDefaultValidator,
        const wxString& name = wxString::FromAscii(GroupCheckBoxNameStr));

    // wxCheckBox implementation.
    void SetValue(bool val) override;
    void SetName(const wxString& name) override;

    wxDECLARE_DYNAMIC_CLASS(GroupCheckBox);
    wxDECLARE_EVENT_TABLE();

private:
    void AddToGroup();
    void RemoveFromGroup();

    void OnCheck(wxCommandEvent& event);

    void UpdateGroup();

    bool initialized_ = false;
    GroupCheckBox* next_;
};

// Handler to load the resource from an XRC file as a "GroupCheckBox" object.
class GroupCheckBoxXmlHandler : public wxXmlResourceHandler {
public:
    GroupCheckBoxXmlHandler();
    ~GroupCheckBoxXmlHandler() override = default;

    wxObject* DoCreateResource() override;
    bool CanHandle(wxXmlNode* node) override;

    wxDECLARE_DYNAMIC_CLASS(wxCheckBoxXmlHandler);
};

}  // namespace widgets

#endif  // VBAM_WX_WIDGETS_GROUP_CHECK_BOX_H_
