#ifndef BACKGROUND_INPUT_H
#define BACKGROUND_INPUT_H

#include <wx/event.h>
#include <wx/log.h>
#include <wx/thread.h>
#include <wx/utils.h>
#include <wx/window.h>

#include <unordered_map>

#if defined(__WXMSW__)
extern std::unordered_map<int, wxKeyCode> gs_specialKeys;
#elif defined(__WXMAC__)
#else // defined(__WXGTK__)
extern std::unordered_map<unsigned, int> x11KeySym;
#endif

void enableKeyboardBackgroundInput(wxEvtHandler* handler);

void disableKeyboardBackgroundInput();

#endif // BACKGROUND_INPUT_H
