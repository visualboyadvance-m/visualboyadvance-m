#ifndef VBAM_WX_BACKGROUND_INPUT_H_
#define VBAM_WX_BACKGROUND_INPUT_H_

#include <wx/event.h>
#include <wx/log.h>
#include <wx/thread.h>
#include <wx/utils.h>
#include <wx/window.h>

void enableKeyboardBackgroundInput(wxEvtHandler* handler);

void disableKeyboardBackgroundInput();

#endif // VBAM_WX_BACKGROUND_INPUT_H_
