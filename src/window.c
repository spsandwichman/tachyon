#include "meteor.h"
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_video.h>

Window window_create(const char* title, u32 w, u32 h, SDL_WindowFlags flags) {
    Window win = {};

    SDL_CreateWindowAndRenderer(
        title, 
        w, h, 
        flags, 
        &win.window, 
        &win.renderer
    );

    return win;
}
