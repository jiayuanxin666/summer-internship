#ifndef MAP_UI_H
#define MAP_UI_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include "map_data.h"

typedef struct {
    SDL_Window *window; SDL_Renderer *renderer; TTF_Font *font, *font_title;
    SDL_Texture *map_texture, *plane_texture, *add_texture, *sub_texture;
    SDL_Texture *fullscreen_texture, *window_texture;
    int width, height, fullscreen, dragging_map, drag_x, drag_y;
    SDL_Rect fullscreen_button, zoom_in_button, zoom_out_button;
} MAP_UI;

int MAP_UI_Init(MAP_UI *ui, int embedded);
void MAP_UI_Destroy(MAP_UI *ui);
void MAP_UI_HandleEvent(MAP_UI *ui, const SDL_Event *event, int *running, MAP_Data *data, int *refresh);
void MAP_UI_SetMapBytes(MAP_UI *ui, const unsigned char *bytes, size_t size);
void MAP_UI_Render(MAP_UI *ui, const MAP_Data *data);

#endif
