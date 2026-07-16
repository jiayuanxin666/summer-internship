#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>

#include "pfd_data.h"
#include "pfd_ui.h"

static const char *PFD_SourceName(int source)
{
    switch (source) {
        case PFD_DATA_SOURCE_FILE: return "FILE";
        case PFD_DATA_SOURCE_XPLANE: return "XPLANE";
        default: return "SIM";
    }
}

#ifdef ENABLE_XPLANE
#include "pfd_xplane.h"
#include <windows.h>

static volatile LONG g_thread_exit = 0;
static int g_data_ready = 0;
static int g_xplane_online = 0;
static PFD_Data g_shared_data;
static CRITICAL_SECTION g_data_lock;

static DWORD WINAPI PFD_DataThread(LPVOID param)
{
    PFD_Data local;
    int connected = 0;
    int failure_count = 0;
    int connected_once = 0;
    int connection_announcement_pending = 0;
    (void)param;

    PFD_Data_Init(&local);
    printf("Connecting to X-Plane Connect at 127.0.0.1:%d...\n", PFD_XPLANE_PORT);
    while (InterlockedCompareExchange(&g_thread_exit, 0, 0) == 0) {
        if (!connected) {
            if (PFD_XPlane_Open()) {
                connected = 1;
                failure_count = 0;
                connection_announcement_pending = 1;
            } else {
                for (int waited = 0; waited < 20 &&
                     InterlockedCompareExchange(&g_thread_exit, 0, 0) == 0; ++waited) {
                    Sleep(100);
                }
            }
            continue;
        }

        if (PFD_XPlane_FetchData(&local)) {
            failure_count = 0;
            EnterCriticalSection(&g_data_lock);
            g_xplane_online = 1;
            g_shared_data = local;
            g_data_ready = 1;
            LeaveCriticalSection(&g_data_lock);
            if (connection_announcement_pending) {
                printf(connected_once ? "X-Plane Connect restored.\n"
                                      : "X-Plane Connect connected.\n");
                connected_once = 1;
                connection_announcement_pending = 0;
            }
        } else {
            ++failure_count;
            if (failure_count >= 10) {
                EnterCriticalSection(&g_data_lock);
                g_xplane_online = 0;
                g_data_ready = 0;
                LeaveCriticalSection(&g_data_lock);
                PFD_XPlane_Close();
                connected = 0;
                failure_count = 0;
                printf("X-Plane Connect lost; switching to file/simulation data.\n");
                for (int waited = 0; waited < 20 &&
                     InterlockedCompareExchange(&g_thread_exit, 0, 0) == 0; ++waited) {
                    Sleep(100);
                }
                continue;
            }
        }
        Sleep(30);
    }

    PFD_XPlane_Close();
    printf("PFD X-Plane data thread stopped.\n");
    return 0;
}
#endif

int main(int argc, char *argv[])
{
    PFD_UI ui;
    PFD_Data raw_data;
    PFD_Data display_data;
    SDL_Event event;
    int running = 1;
    int previous_source = -1;
#ifdef ENABLE_XPLANE
    HANDLE data_thread = NULL;
    Uint32 last_xplane_frame_tick = 0;
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

    if (PFD_UI_Init(&ui) != 0) {
        IMG_Quit();
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    PFD_Data_Init(&raw_data);
    PFD_Data_Init(&display_data);

#ifdef ENABLE_XPLANE
    InitializeCriticalSection(&g_data_lock);
    g_shared_data = raw_data;
    InterlockedExchange(&g_thread_exit, 0);
    g_data_ready = 0;
    g_xplane_online = 0;
    data_thread = CreateThread(NULL, 0, PFD_DataThread, NULL, 0, NULL);
    if (data_thread) {
        printf("PFD X-Plane data thread enabled.\n");
    } else {
        DeleteCriticalSection(&g_data_lock);
        printf("PFD X-Plane thread creation failed; using file/simulation data.\n");
    }
#endif

    while (running) {
        while (SDL_PollEvent(&event)) {
            PFD_UI_HandleEvent(&ui, &event, &running);
        }

#ifdef ENABLE_XPLANE
        if (data_thread) {
            int online;
            EnterCriticalSection(&g_data_lock);
            online = g_xplane_online;
            if (online && g_data_ready) {
                raw_data = g_shared_data;
                g_data_ready = 0;
                last_xplane_frame_tick = SDL_GetTicks();
            }
            LeaveCriticalSection(&g_data_lock);
            if (!online) {
                PFD_Data_LoadNextFrame(&raw_data);
            } else if (last_xplane_frame_tick == 0 ||
                       SDL_GetTicks() - last_xplane_frame_tick > 250) {
                PFD_Data_LoadNextFrame(&raw_data);
            }
        } else {
            PFD_Data_LoadNextFrame(&raw_data);
        }
#else
        PFD_Data_LoadNextFrame(&raw_data);
#endif

        if ((int)raw_data.data_source != previous_source) {
            if (previous_source < 0) {
                printf("PFD source changed: NONE -> %s\n",
                       PFD_SourceName(raw_data.data_source));
            } else {
                printf("PFD source changed: %s -> %s\n",
                       PFD_SourceName(previous_source),
                       PFD_SourceName(raw_data.data_source));
            }
            previous_source = raw_data.data_source;
        }

        PFD_Data_Smooth(&display_data, &raw_data, 0.12f);
        PFD_UI_Render(&ui, &display_data);

        SDL_Delay(16);
    }

    PFD_Data_Close();
#ifdef ENABLE_XPLANE
    if (data_thread) {
        printf("PFD X-Plane thread exit requested by main window.\n");
        InterlockedExchange(&g_thread_exit, 1);
        WaitForSingleObject(data_thread, INFINITE);
        CloseHandle(data_thread);
        DeleteCriticalSection(&g_data_lock);
    }
#endif
    PFD_UI_Destroy(&ui);
    IMG_Quit();
    TTF_Quit();
    SDL_Quit();

    return 0;
}
