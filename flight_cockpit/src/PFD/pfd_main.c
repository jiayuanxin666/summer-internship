#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>

#include "pfd_data.h"
#include "pfd_ui.h"

#ifdef ENABLE_XPLANE
#include "pfd_xplane.h"
#endif

int main(int argc, char *argv[])
{
    PFD_UI ui;
    PFD_Data raw_data;
    PFD_Data display_data;
    SDL_Event event;
    int running = 1;
    int xplane_available = 0;

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

    if (PFD_UI_Init(&ui) != 0) {
        IMG_Quit();
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    PFD_Data_Init(&raw_data);
    PFD_Data_Init(&display_data);

#ifdef ENABLE_XPLANE
    xplane_available = PFD_XPlane_Open();
    if (xplane_available) {
        printf("PFD X-Plane Connect enabled.\n");
    } else {
        printf("PFD X-Plane Connect unavailable; using file/simulation data.\n");
    }
#endif

    while (running) {
        int fetched = 0;

        while (SDL_PollEvent(&event)) {
            PFD_UI_HandleEvent(&ui, &event, &running);
        }

#ifdef ENABLE_XPLANE
        if (xplane_available) {
            fetched = PFD_XPlane_FetchData(&raw_data);
        }
#endif
        if (!fetched) {
            PFD_Data_LoadNextFrame(&raw_data);
        }

        PFD_Data_Smooth(&display_data, &raw_data, 0.12f);
        PFD_UI_Render(&ui, &display_data);

        SDL_Delay(16);
    }

    PFD_Data_Close();
#ifdef ENABLE_XPLANE
    PFD_XPlane_Close();
#endif
    PFD_UI_Destroy(&ui);
    IMG_Quit();
    TTF_Quit();
    SDL_Quit();

    return 0;
}
