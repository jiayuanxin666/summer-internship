#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>

#include "nd_data.h"
#include "nd_ui.h"

#ifdef ENABLE_XPLANE
#include "nd_xplane.h"
#include <windows.h>

static volatile int g_thread_exit = 0;
static int g_data_ready = 0;
static ND_Data g_shared_data;
static CRITICAL_SECTION g_data_lock;

static DWORD WINAPI ND_DataThread(LPVOID param)
{
    ND_Data local;
    (void)param;

    ND_Data_Init(&local);
    while (!g_thread_exit) {
        if (ND_XPlane_FetchData(&local)) {
            EnterCriticalSection(&g_data_lock);
            g_shared_data = local;
            g_data_ready = 1;
            LeaveCriticalSection(&g_data_lock);
        }
        Sleep(30);
    }

    return 0;
}
#endif

int main(int argc, char *argv[])
{
    ND_UI ui;
    ND_Data raw_data;
    ND_Data display_data;
    SDL_Event event;
    int running = 1;
#ifdef ENABLE_XPLANE
    int xplane_available = 0;
    HANDLE data_thread = NULL;
#endif

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

    if (ND_UI_Init(&ui) != 0) {
        IMG_Quit();
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    ND_Data_Init(&raw_data);
    ND_Data_Init(&display_data);

#ifdef ENABLE_XPLANE
    xplane_available = ND_XPlane_Open();
    if (xplane_available) {
        InitializeCriticalSection(&g_data_lock);
        g_shared_data = raw_data;
        g_thread_exit = 0;
        g_data_ready = 0;
        data_thread = CreateThread(NULL, 0, ND_DataThread, NULL, 0, NULL);
        if (data_thread) {
            printf("ND X-Plane Connect enabled.\n");
        } else {
            xplane_available = 0;
            DeleteCriticalSection(&g_data_lock);
            printf("ND X-Plane thread creation failed; using file/simulation data.\n");
        }
    } else {
        printf("ND X-Plane Connect unavailable; using file/simulation data.\n");
    }
#endif

    while (running) {
        while (SDL_PollEvent(&event)) {
            ND_UI_HandleEvent(&ui, &event, &running);
        }

#ifdef ENABLE_XPLANE
        if (xplane_available) {
            EnterCriticalSection(&g_data_lock);
            if (g_data_ready) {
                raw_data = g_shared_data;
                g_data_ready = 0;
            }
            LeaveCriticalSection(&g_data_lock);
        } else {
            ND_Data_LoadNextFrame(&raw_data);
        }
#else
        ND_Data_LoadNextFrame(&raw_data);
#endif

        ND_Data_Smooth(&display_data, &raw_data, 0.16f);
        ND_UI_Render(&ui, &display_data);

        SDL_Delay(16);
    }

#ifdef ENABLE_XPLANE
    if (data_thread) {
        g_thread_exit = 1;
        WaitForSingleObject(data_thread, 1000);
        CloseHandle(data_thread);
        DeleteCriticalSection(&g_data_lock);
    }
    ND_XPlane_Close();
#endif

    ND_Data_Close();
    ND_UI_Destroy(&ui);
    IMG_Quit();
    TTF_Quit();
    SDL_Quit();

    return 0;
}
