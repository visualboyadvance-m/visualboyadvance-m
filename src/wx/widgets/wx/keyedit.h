#ifndef _WX_KEYTEXT_H
#define _WX_KEYTEXT_H

// wxKeyTextCtrl: a wxTextCtrl which stores/acts on key presses
// The value is the symbolic name of the key pressed
// Supports manual clearing (bs), multiple keys in widget, automatic tab on key

#include <wx/textctrl.h>
#include <wx/accel.h>
#include <vector>

typedef std::vector<wxAcceleratorEntry> wxAcceleratorEntry_v;

class wxKeyTextCtrl : public wxTextCtrl
{
public:
	// default constructor; required for use with xrc
	// FIXME: clearable and keyenter should be style flags
	wxKeyTextCtrl() : wxTextCtrl(), clearable(true), multikey(wxT(',')),
		keyenter(true), lastmod(0), lastkey(0) {};
	virtual ~wxKeyTextCtrl() {};

	void SetClearable(bool set = true) { clearable = set; }
	void SetMultikey(wxChar c = wxT(',')) { multikey = c; }
	void SetKeyEnter(bool set = true) { keyenter = set; }

	bool GetClearable() { return clearable; }
	wxChar GetMultikey() { return multikey; }
	bool GetKeyEnter() { return keyenter; }

	// convert mod+key to accel string, separated by -
	static wxString ToString(int mod, int key);
	// convert multiple keys, separated by multikey
	static wxString ToString(wxAcceleratorEntry_v keys, wxChar sep = wxT(','));
	// parses single key string into mod+key
	static bool FromString(const wxString &s, int &mod, int &key);
	// parse multi-key string into accelentry array
	// note that meta flag may be set in accelentry array item even
	// where not supported for accelerators (i.e. non-mac)
	// returns empty array on parse errors
	static wxAcceleratorEntry_v FromString(const wxString &s, wxChar sep = wxT(','));
	// parse a single key in given wxChar array up to given len
	static bool ParseString(const wxChar* s, int len, int &mod, int &key);

protected:
	void OnKeyDown(wxKeyEvent &);
	void OnKeyUp(wxKeyEvent &);

	bool clearable;
	wxChar multikey;
	bool keyenter;
	// the last keydown event received; this is processed on next keyup
	int lastmod, lastkey;

	DECLARE_DYNAMIC_CLASS();
	DECLARE_EVENT_TABLE();
};

// A simple copy-only validator
class wxKeyValidator : public wxValidator
{
public:
	wxKeyValidator(wxAcceleratorEntry_v* v) : wxValidator(), val(v) {}
	wxKeyValidator(const wxKeyValidator &v) : wxValidator(), val(v.val) {}
	wxObject* Clone() const { return new wxKeyValidator(val); }
	bool TransferToWindow();
	bool TransferFromWindow();
	bool Validate(wxWindow* p) { return true; }
protected:
	wxAcceleratorEntry_v* val;

	DECLARE_CLASS(wxKeyValidator)
};

#endif /* WX_KEYTEXT_H */
