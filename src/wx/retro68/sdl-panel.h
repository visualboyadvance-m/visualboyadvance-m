#ifndef VBAM_WX_RETRO68_SDL_PANEL_H_
#define VBAM_WX_RETRO68_SDL_PANEL_H_

// The SDL renderer on classic Mac OS (Retro68). wxRetro68 child windows are
// not Mac windows of their own but areas of their top-level window's port, so
// SDL draws into the top-level WindowPtr, at the panel's client area: the
// Retro68 SDL3 adopts it through SDL_PROP_WINDOW_CREATE_MACCLASSIC_WINDOW_POINTER
// and takes the area from the MACCLASSIC_ORIGIN properties. wx runs the
// Toolbox event loop, so SDL is told to keep out of it
// (SDL_HINT_MACCLASSIC_EXTERNAL_EVENT_LOOP).

#include <SDL3/SDL.h>

class wxWindow;

namespace sdlmac {

// Where SDL draws in the port of `win`'s top-level window: the client area of
// `win`, in the port's local coordinates.
struct Area {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;

    bool operator==(const Area& other) const {
        return x == other.x && y == other.y && width == other.width &&
               height == other.height;
    }
    bool operator!=(const Area& other) const { return !(*this == other); }
};

// Call before SDL_InitSubSystem(SDL_INIT_VIDEO).
void PrepareVideo();

// The client area of `win` in its top-level window's port.
Area AreaOf(wxWindow* win);

// Sets the properties that make SDL_CreateWindowWithProperties() draw into
// `area` of `win`'s top-level window; false when there is no Mac window.
bool SetCreateProps(wxWindow* win, const Area& area, SDL_PropertiesID props);

// Moves and resizes `sdl` to the client area of `win`, when `*area` (what it
// was set to last) is out of date.
void Sync(wxWindow* win, SDL_Window* sdl, Area* area);

// SDL changed the port's colours behind wx's back; call after it drew.
void AfterPresent();

}  // namespace sdlmac

#endif  // VBAM_WX_RETRO68_SDL_PANEL_H_
