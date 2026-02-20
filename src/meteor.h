#ifndef METEOR_H
#define METEOR_H

#include <SDL3/SDL.h>
#include <SDL3/SDL_video.h>
#include "common/type.h"
#include "common/portability.h"

#define mtr_panic(msg, ...) do {\
    printf("mtr panic: " msg "\n" __VA_OPT__(,) __VA_ARGS__);\
    exit(1); \
} while (0)

#define mtr_info(msg, ...) \
    printf("mtr info:  " msg "\n" __VA_OPT__(,) __VA_ARGS__)

NORETURN void mtr_quit(usize status);

typedef struct Window {
    SDL_Window* window;
    SDL_Renderer* renderer;
} Window;

Window window_create(const char* title, u32 w, u32 h, SDL_WindowFlags flags);

void mtr_run_ui();

#endif // METEOR_H
