// This file is for marking some strings for translations that we are not
// pulling upstream translations for, such as the core wxWidgets translations.
//
// Please sort.

#include "wxhead.h"

[[maybe_unused]] void f()
{
	wxString s;

	s = _("&Apply");
	s = _("Artists");
	s = _("Cancel");
	s = _("Close");
	s = _("Developers");
	s = _("License");
	s = _("OK");
}
