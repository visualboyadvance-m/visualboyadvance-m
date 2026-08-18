#ifndef VBAM_WX_BACKGROUND_INPUT_H_
#define VBAM_WX_BACKGROUND_INPUT_H_

#include <wx/event.h>
#include <wx/log.h>
#include <wx/thread.h>
#include <wx/utils.h>
#include <wx/window.h>

void enableKeyboardBackgroundInput(wxEvtHandler* handler);

void disableKeyboardBackgroundInput();

// Makes the background-input poller treat every currently held key as a fresh
// press on its next iteration. Call when the app loses focus: the poller keeps
// its snapshot current while the app is foreground, so a key held across the
// focus change would otherwise read as "no change" and never register.
void resetBackgroundInputStateSnapshot();

#endif // VBAM_WX_BACKGROUND_INPUT_H_
