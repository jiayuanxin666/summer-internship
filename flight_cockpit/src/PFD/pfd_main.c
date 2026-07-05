#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include "pfd_data.h"
#include "pfd_ui.h"

int main(int argc, char *argv[])
{
    PFD_UI ui;
    PFD_Data data;
    SDL_Event event;
    int running = 1;

    (void)argc;
    (void)argv;

    SDL_SetMainReady();

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        printf("SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    if (TTF_Init() == -1) {
        printf("TTF_Init failed: %s\n", TTF_GetError());
        SDL_Quit();
        return 1;
    }

    if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) {
        printf("IMG_Init warning: PNG support unavailable: %s\n", IMG_GetError());
    }

    if (!PFD_UI_Init(&ui)) {
        IMG_Quit();
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    PFD_Data_Init(&data);

    while (running) {
        while (SDL_PollEvent(&event)) {
            PFD_UI_HandleEvent(&ui, &event, &running);
        }

        PFD_Data_LoadNextFrame(&data);
        PFD_UI_Render(&ui, &data);
        SDL_Delay(16);
    }

    PFD_Data_Close();
    PFD_UI_Destroy(&ui);
    IMG_Quit();
    TTF_Quit();
    SDL_Quit();

    return 0;
}
