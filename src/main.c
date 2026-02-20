#include <SDL3/SDL_error.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_oldnames.h>
#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_rect.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_surface.h>
#include <SDL3/SDL_timer.h>
#include <SDL3/SDL_video.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <stdio.h>
#include <stdlib.h>

#include "common/portability.h"
#include "common/str.h"
#include "meteor.h"

#include "microui/microui.h"

////////////////////////////////////////////////////////////////
// microui callbacks
////////////////////////////////////////////////////////////////

TTF_Font* global_font;

int ui_get_text_width(mu_Font font, const char *str, int len) {
    int width;
    int height;
    if (len < 0) {
        len = strlen(str);
    }
    if (!TTF_GetStringSize(global_font, str, len, &width, &height)) {
        mtr_panic("TTF_GetStringSize failed on string '"str_fmt"' (len %d) - sdl says '%s'", len, str, len, SDL_GetError());
    }

    return width;
}

int ui_get_text_height(mu_Font font) {
    return (int)TTF_GetFontHeight(global_font);
}

////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////

int main() {
    
    // init SDL

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        mtr_panic("failed to initialize sdl3");
    }

    // init SDL_ttf

    if (!TTF_Init()) {
        mtr_panic("failed to initialize sdl3_ttf");
    }

    global_font = TTF_OpenFont("IBMPlexMono-Medium.ttf", 10);
    if (!global_font) {
        mtr_panic("failed to load ui font");
    }

    // init microui

    mu_Context *ctx = malloc(sizeof(mu_Context));
    mu_init(ctx);

    ctx->text_width = ui_get_text_width;
    ctx->text_height = ui_get_text_height;

    // init sdl windows
    Window win = window_create("window 1", 1000, 1000, SDL_WINDOW_RESIZABLE);
    
    // run event loop
    while (true) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_EVENT_QUIT:
                mtr_info("quit event recieved, exiting");
                mtr_quit(0);
            }
        }

        // render UI

        mu_begin(ctx);
        
        if (mu_begin_window(ctx, "My Window", mu_rect(10, 10, 300, 400))) {
            /* process ui here... */
            if (mu_button(ctx, "My Button")) {
                printf("'My Button' was pressed\n");
            }
            mu_end_window(ctx);
        }

        mu_end(ctx);

        mu_Command *cmd = NULL;
        while (mu_next_command(ctx, &cmd)) {
            switch (cmd->type) {
            case MU_COMMAND_RECT: {
                SDL_FRect rect = {
                    .x = cmd->rect.rect.x,
                    .y = cmd->rect.rect.y,
                    .w = cmd->rect.rect.w,
                    .h = cmd->rect.rect.h,
                };
                
                SDL_SetRenderDrawColor(win.renderer, 
                    cmd->rect.color.r,
                    cmd->rect.color.g,
                    cmd->rect.color.b,
                    cmd->rect.color.a
                );

                SDL_RenderFillRect(win.renderer, &rect);

                break;
            }
            case MU_COMMAND_TEXT: {
                
                SDL_Color color = {
                    cmd->text.color.r,
                    cmd->text.color.g,
                    cmd->text.color.b,
                    cmd->text.color.a
                };

                // SDL_RenderFillRect(win.renderer, &rect);

                SDL_Surface* rendered_text = TTF_RenderText_Blended(
                    global_font, 
                    cmd->text.str, 
                    strlen(cmd->text.str), 
                    color
                );

                SDL_Texture* texture = SDL_CreateTextureFromSurface(win.renderer, rendered_text);

                SDL_FRect src_rect = {
                    .x = 0,
                    .y = 0,
                    .w = texture->w,
                    .h = texture->h,
                };
                
                SDL_FRect dest_rect = {
                    .x = cmd->text.pos.x,
                    .y = cmd->text.pos.y,
                    .w = texture->w,
                    .h = texture->h,
                };

                SDL_RenderTexture(win.renderer, texture, &src_rect, &dest_rect);
               
                SDL_DestroySurface(rendered_text);
                SDL_DestroyTexture(texture);

                break;
            }
            }
        }

        SDL_RenderPresent(win.renderer);
    }

    ////////////////////////////////////////////////////////////////

    mtr_quit(0);
}



NORETURN void mtr_quit(usize status) {
    SDL_Quit();
    TTF_Quit();
    exit(status);
}
