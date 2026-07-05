#ifndef PFD_UI_H
#define PFD_UI_H

#include <SDL2/SDL.h>
#include "pfd_data.h"

#define PFD_LOGIC_WIDTH 772
#define PFD_LOGIC_HEIGHT 721

typedef struct
{
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *logic_texture;

    int window_width;
    int window_height;

    SDL_Rect render_rect;
} PFD_UI;

int PFD_UI_Init(PFD_UI *ui);
void PFD_UI_HandleEvent(PFD_UI *ui, SDL_Event *event, int *running);
void PFD_UI_Render(PFD_UI *ui, const PFD_Data *data);
void PFD_UI_Destroy(PFD_UI *ui);

#endif
