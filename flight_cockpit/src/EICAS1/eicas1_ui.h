#ifndef EICAS1_UI_H
#define EICAS1_UI_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include "eicas1_data.h"

#define EICAS1_LOGIC_WIDTH 745
#define EICAS1_LOGIC_HEIGHT 728

typedef struct
{
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *logic_texture;
    TTF_Font *font_small;
    TTF_Font *font_medium;
    TTF_Font *font_large;
    int window_width;
    int window_height;
    int vsync_enabled;
    SDL_Rect render_rect;
} EICAS1_UI;

int EICAS1_UI_Init(EICAS1_UI *ui);
void EICAS1_UI_HandleEvent(EICAS1_UI *ui, SDL_Event *event, int *running);
void EICAS1_UI_Render(EICAS1_UI *ui, const EICAS1_Data *data);
void EICAS1_UI_Destroy(EICAS1_UI *ui);

#endif
