#ifndef EICAS2_UI_H
#define EICAS2_UI_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include "eicas2_data.h"

#define EICAS2_LOGIC_WIDTH 745
#define EICAS2_LOGIC_HEIGHT 728

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
} EICAS2_UI;

int EICAS2_UI_Init(EICAS2_UI *ui);
void EICAS2_UI_HandleEvent(EICAS2_UI *ui, SDL_Event *event, int *running);
void EICAS2_UI_Render(EICAS2_UI *ui, const EICAS2_Data *data);
void EICAS2_UI_Destroy(EICAS2_UI *ui);

#endif
