#ifndef ND_UI_H
#define ND_UI_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include "nd_data.h"

#define ND_LOGIC_WIDTH 751
#define ND_LOGIC_HEIGHT 721

typedef struct
{
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *logic_texture;
    TTF_Font *font_small;
    TTF_Font *font_medium;
    TTF_Font *font_large;
    TTF_Font *font_huge;

    int window_width;
    int window_height;
    int map_range_index;

    SDL_Rect render_rect;
} ND_UI;

int ND_UI_Init(ND_UI *ui);
void ND_UI_HandleEvent(ND_UI *ui, SDL_Event *event, int *running);
void ND_UI_Render(ND_UI *ui, const ND_Data *data);
int ND_UI_SaveScreenshot(ND_UI *ui, const char *path);
void ND_UI_Destroy(ND_UI *ui);

#endif
