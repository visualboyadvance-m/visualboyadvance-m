#ifndef VBAM_WX_WIDGETS_USER_INPUT_CTRL_H_
#define VBAM_WX_WIDGETS_USER_INPUT_CTRL_H_

#include <unordered_set>

#include <wx/longlong.h>
#include <wx/string.h>
#include <wx/textctrl.h>
#include <wx/xrc/xmlres.h>

#include "wx/config/user-input.h"
#include "wx/widgets/user-input-event.h"

namespace widgets {

extern const char UserInputCtrlNameStr[];

// A custom TextCtrl that is used for input configuration. It can be configured
// for single or multi-key input. In multi-key mode, the user can press multiple
// keys to configure a single input.
// Internally, this control stores a set of UserInput objects, which is how the
// value for the field should be modified.
class UserInputCtrl : public wxTextCtrl {
public:
    UserInputCtrl();
    UserInputCtrl(wxWindow* parent,
                  wxWindowID id,
                  const wxPoint& pos = wxDefaultPosition,
                  const wxSize& size = wxDefaultSize,
                  long style = 0,
                  const wxString& name = wxString::FromAscii(UserInputCtrlNameStr));
    ~UserInputCtrl() override;

    bool Create(wxWindow* parent,
                wxWindowID id,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = 0,
                const wxString& name = wxString::FromAscii(UserInputCtrlNameStr));

    // Sets multi-key mode on or off.
    void SetMultiKey(bool multikey);

    // Sets this control inputs.
    void SetInputs(const std::unordered_set<config::UserInput>& inputs);

    // Helper method to return the single input for no multikey UserInputCtrls.
    // Asserts if `is_multikey_` is true.
    // Returns an invalid UserInput if no input is currently set.
    config::UserInput SingleInput() const;

    // Returns the inputs set in this control.
    const std::unordered_set<config::UserInput>& inputs() const { return inputs_; }

    // Clears the inputs set in this control.
    void Clear() override;

    wxDECLARE_DYNAMIC_CLASS(UserInputCtrl);

private:
    // Event handler.
    void OnUserInput(widgets::UserInputEvent& event);

    // Updates the text in the control to reflect the current inputs.
    void UpdateText();

    bool is_multikey_ = false;

    // The last time the control was focused. This is used to ignore events sent
    // very shortly after activation.
    wxLongLong last_focus_time_ = 0;

    std::unordered_set<config::UserInput> inputs_;
};

// Handler to load the resource from an XRC file as a "UserInputCtrl" object.
class UserInputCtrlXmlHandler : public wxXmlResourceHandler {
public:
    UserInputCtrlXmlHandler();
    ~UserInputCtrlXmlHandler() override = default;

private:
    // wxXmlResourceHandler implementation.
    wxObject* DoCreateResource() override;
    bool CanHandle(wxXmlNode* node) override;

    wxDECLARE_DYNAMIC_CLASS(wxTextCtrlXmlHandler);
};

}  // namespace widgets

#endif  // VBAM_WX_WIDGETS_USER_INPUT_CTRL_H_