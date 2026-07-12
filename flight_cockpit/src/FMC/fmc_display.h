#ifndef FMC_DISPLAY_H
#define FMC_DISPLAY_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include "fmc_event.h"

#define FMC_LOGICAL_WIDTH 638
#define FMC_LOGICAL_HEIGHT 998

typedef struct
{
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *logic_texture;
    SDL_Texture *background_texture;
    TTF_Font *font_small;
    TTF_Font *font_normal;
    TTF_Font *font_large;
    int window_width;
    int window_height;
    SDL_Rect render_rect;
} FMC_Display;

int FMC_Display_Init(FMC_Display *display);
void FMC_Display_UpdateSize(FMC_Display *display, int width, int height);
int FMC_Display_WindowToLogical(const FMC_Display *display, int window_x, int window_y,
                                int *logical_x, int *logical_y);
void FMC_Display_Render(FMC_Display *display, const FMC_State *state);
int FMC_Display_SaveScreenshot(FMC_Display *display, const char *path);
void FMC_Display_Destroy(FMC_Display *display);

#endif
