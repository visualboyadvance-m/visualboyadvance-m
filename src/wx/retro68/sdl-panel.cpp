// The SDL renderer's window handling on classic Mac OS; see sdl-panel.h.

#include "wx/retro68/sdl-panel.h"

#include <wx/nonownedwnd.h>
#include <wx/retro68/dc.h>
#include <wx/window.h>

namespace sdlmac {

void PrepareVideo() {
    SDL_SetHint(SDL_HINT_MACCLASSIC_EXTERNAL_EVENT_LOOP, "1");
}

Area AreaOf(wxWindow* win) {
    // GetPortPosition() is the window's top left corner in the port; the
    // client area starts GetClientAreaOrigin() further in.
    const wxPoint origin = win->GetPortPosition() + win->GetClientAreaOrigin();
    const wxSize size = win->GetClientSize();

    Area area;
    area.x = origin.x;
    area.y = origin.y;
    area.width = size.x > 0 ? size.x : 1;
    area.height = size.y > 0 ? size.y : 1;
    return area;
}

bool SetCreateProps(wxWindow* win, const Area& area, SDL_PropertiesID props) {
    wxNonOwnedWindow* tlw = win->GetTLW();
    GrafPort* port = tlw ? tlw->GetWindowRef() : nullptr;
    if (!port)
        return false;

    SDL_SetPointerProperty(props, SDL_PROP_WINDOW_CREATE_MACCLASSIC_WINDOW_POINTER, port);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_MACCLASSIC_ORIGIN_X_NUMBER, area.x);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_MACCLASSIC_ORIGIN_Y_NUMBER, area.y);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, area.width);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, area.height);
    return true;
}

void Sync(wxWindow* win, SDL_Window* sdl, Area* area) {
    if (!sdl)
        return;

    const Area now = AreaOf(win);
    if (now == *area)
        return;

    if (now.x != area->x || now.y != area->y) {
        // Read by the driver at the next framebuffer update.
        const SDL_PropertiesID props = SDL_GetWindowProperties(sdl);
        SDL_SetNumberProperty(props, SDL_PROP_WINDOW_MACCLASSIC_ORIGIN_X_NUMBER, now.x);
        SDL_SetNumberProperty(props, SDL_PROP_WINDOW_MACCLASSIC_ORIGIN_Y_NUMBER, now.y);
    }
    if (now.width != area->width || now.height != area->height) {
        // The driver leaves the Mac window alone and makes a framebuffer of
        // the new size.
        SDL_SetWindowSize(sdl, now.width, now.height);
    }
    *area = now;
}

void AfterPresent() {
    wxRetro68DCImpl::InvalidateCachedPortState();
}

}  // namespace sdlmac
