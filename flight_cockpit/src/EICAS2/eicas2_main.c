#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>

#include "eicas2_data.h"
#include "eicas2_ui.h"

/* EICAS2 is a standalone executable; build it separately as build/eicas2.exe. */

#ifdef ENABLE_XPLANE
#include "../Util/xplaneConnect.h"
#include <windows.h>

static int g_data_ready = 0;
static EICAS2_Data g_shared_data;
static CRITICAL_SECTION g_data_lock;
static HANDLE g_exit_event = NULL;

static DWORD WINAPI EICAS2_DataThread(LPVOID param)
{
    EICAS2_Data local;
    int connected;
    int failures = 0;
    Uint32 last_retry;
    (void)param;

    EICAS2_Data_Init(&local);
    connected = EICAS2_XPlane_Open();
    last_retry = SDL_GetTicks();
    if (connected) {
        printf("EICAS2 X-Plane connected.\n");
    } else {
        printf("EICAS2 X-Plane unavailable (%s); using file/simulation data.\n",
               XPC_GetLastErrorString());
    }
    while (WaitForSingleObject(g_exit_event, 30) == WAIT_TIMEOUT) {
        if (connected) {
            if (EICAS2_XPlane_FetchData(&local)) {
                failures = 0;
            } else if (++failures >= 3) {
                EICAS2_XPlane_Close();
                connected = 0;
                last_retry = SDL_GetTicks();
                printf("EICAS2 X-Plane lost (%s); switching to file/simulation data.\n",
                       XPC_GetLastErrorString());
                EICAS2_Data_LoadNextFrame(&local);
            }
        } else {
            EICAS2_Data_LoadNextFrame(&local);
            if (SDL_GetTicks() - last_retry >= 2000) {
                last_retry = SDL_GetTicks();
                if (EICAS2_XPlane_Open()) {
                    connected = 1;
                    failures = 0;
                    printf("EICAS2 X-Plane reconnected.\n");
                }
            }
        }
        EnterCriticalSection(&g_data_lock);
        g_shared_data = local;
        g_data_ready = 1;
        LeaveCriticalSection(&g_data_lock);
    }
    EICAS2_XPlane_Close();
    return 0;
}
#endif

int main(int argc, char *argv[])
{
    EICAS2_UI ui;
    EICAS2_Data raw_data;
    EICAS2_Data display_data;
    SDL_Event event;
    int running = 1;
#ifdef ENABLE_XPLANE
    HANDLE data_thread = NULL;
#endif

    (void)argc;
    (void)argv;
    SDL_SetMainReady();

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        printf("SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    if (TTF_Init() != 0) {
        printf("TTF_Init failed: %s\n", TTF_GetError());
        SDL_Quit();
        return 1;
    }
    if ((IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG) == 0) {
        printf("IMG_Init warning: PNG support unavailable: %s\n", IMG_GetError());
    }

    if (EICAS2_UI_Init(&ui) != 0) {
        IMG_Quit();
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    EICAS2_Data_Init(&raw_data);
    EICAS2_Data_Init(&display_data);

#ifdef ENABLE_XPLANE
    InitializeCriticalSection(&g_data_lock);
    g_data_ready = 0;
    g_shared_data = raw_data;
    g_exit_event = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (g_exit_event) {
        data_thread = CreateThread(NULL, 0, EICAS2_DataThread, NULL, 0, NULL);
    }
#endif

    while (running) {
        while (SDL_PollEvent(&event)) {
            EICAS2_UI_HandleEvent(&ui, &event, &running);
        }

#ifdef ENABLE_XPLANE
        if (data_thread) {
            EnterCriticalSection(&g_data_lock);
            if (g_data_ready) {
                raw_data = g_shared_data;
                g_data_ready = 0;
            }
            LeaveCriticalSection(&g_data_lock);
        } else {
            EICAS2_Data_LoadNextFrame(&raw_data);
        }
#else
        EICAS2_Data_LoadNextFrame(&raw_data);
#endif

        EICAS2_Data_Smooth(&display_data, &raw_data, 0.16f);
        EICAS2_UI_Render(&ui, &display_data);
        if (!ui.vsync_enabled) SDL_Delay(16);
    }

#ifdef ENABLE_XPLANE
    if (data_thread) {
        SetEvent(g_exit_event);
        WaitForSingleObject(data_thread, INFINITE);
        CloseHandle(data_thread);
    }
    if (g_exit_event) CloseHandle(g_exit_event);
    DeleteCriticalSection(&g_data_lock);
#endif

    EICAS2_Data_Close();
    EICAS2_UI_Destroy(&ui);
    IMG_Quit();
    TTF_Quit();
    SDL_Quit();
    return 0;
}
