#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <string.h>

#include "nd_data.h"
#include "nd_ui.h"
#include "../Util/route_ipc.h"

#ifdef ENABLE_XPLANE
#include "nd_xplane.h"
#include <windows.h>

static volatile LONG g_thread_exit = 0;
static volatile LONG g_xplane_connected = 0;
static int g_data_ready = 0;
static ND_Data g_shared_data;
static CRITICAL_SECTION g_data_lock;

static DWORD WINAPI ND_DataThread(LPVOID param)
{
    ND_Data local;
    DWORD last_reconnect = 0;
    int failed_reads = 0;
    (void)param;

    ND_Data_Init(&local);
    while (!InterlockedCompareExchange(&g_thread_exit, 0, 0)) {
        if (!InterlockedCompareExchange(&g_xplane_connected, 0, 0)) {
            DWORD now = GetTickCount();
            if (last_reconnect == 0 || now - last_reconnect >= 2000) {
                last_reconnect = now;
                ND_XPlane_Close();
                if (ND_XPlane_Open()) {
                    InterlockedExchange(&g_xplane_connected, 1);
                    failed_reads = 0;
                    printf("ND X-Plane Connect enabled.\n");
                }
            }
            Sleep(100);
            continue;
        }

        if (ND_XPlane_FetchData(&local)) {
            failed_reads = 0;
            EnterCriticalSection(&g_data_lock);
            g_shared_data = local;
            g_data_ready = 1;
            LeaveCriticalSection(&g_data_lock);
        } else if (++failed_reads >= 10) {
            ND_XPlane_Close();
            InterlockedExchange(&g_xplane_connected, 0);
            failed_reads = 0;
            last_reconnect = GetTickCount();
            printf("ND X-Plane connection lost; retrying.\n");
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
    RouteIPC *route_ipc = NULL;
    RouteIPCData shared_route;
    unsigned int route_sequence = 0;
    int shared_route_valid = 0;
    int self_test = 0;
    const char *screenshot_path = NULL;
    int frame_count = 0;
#ifdef ENABLE_XPLANE
    HANDLE data_thread = NULL;
#endif

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--self-test") == 0) {
            self_test = 1;
        } else if (strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc) {
            screenshot_path = argv[++i];
        }
    }
    if (self_test) {
        int ok = ND_Data_RunSelfTest();
        printf("ND self-test: %s\n", ok ? "PASS" : "FAIL");
        return ok ? 0 : 1;
    }

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
    route_ipc = RouteIPC_Open(0);

#ifdef ENABLE_XPLANE
    InitializeCriticalSection(&g_data_lock);
    g_shared_data = raw_data;
    InterlockedExchange(&g_thread_exit, 0);
    InterlockedExchange(&g_xplane_connected, 0);
    g_data_ready = 0;
    data_thread = CreateThread(NULL, 0, ND_DataThread, NULL, 0, NULL);
    if (!data_thread) {
        DeleteCriticalSection(&g_data_lock);
        printf("ND X-Plane thread creation failed; using file/simulation data.\n");
    }
#endif

    while (running) {
        while (SDL_PollEvent(&event)) {
            ND_UI_HandleEvent(&ui, &event, &running);
        }

#ifdef ENABLE_XPLANE
        if (data_thread && InterlockedCompareExchange(&g_xplane_connected, 0, 0)) {
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

        if (!route_ipc) route_ipc = RouteIPC_Open(0);
        if (route_ipc) {
            RouteIPCData shared;
            if (RouteIPC_Read(route_ipc, &shared, &route_sequence)) {
                shared_route = shared;
                shared_route_valid = 1;
            }
        }

        if (shared_route_valid) {
                raw_data.route_count = shared_route.point_count > ND_MAX_ROUTE_POINTS ? ND_MAX_ROUTE_POINTS : shared_route.point_count;
                for (int i = 0; i < raw_data.route_count; ++i) {
                    snprintf(raw_data.route[i].ident, sizeof(raw_data.route[i].ident), "%s", shared_route.points[i].ident);
                    raw_data.route[i].latitude = shared_route.points[i].latitude;
                    raw_data.route[i].longitude = shared_route.points[i].longitude;
                    raw_data.route[i].type = ND_POINT_ROUTE;
                }
                raw_data.route_from_fmc = 1;
                raw_data.route_revision = route_sequence;
                ND_Data_UpdateNavigation(&raw_data);
        }

        ND_Data_Smooth(&display_data, &raw_data, 0.16f);
        ND_UI_Render(&ui, &display_data);

        ++frame_count;
        if (screenshot_path && frame_count == 2) {
            if (!ND_UI_SaveScreenshot(&ui, screenshot_path)) {
                printf("ND screenshot failed: %s\n", SDL_GetError());
            }
            running = 0;
        }

        SDL_Delay(16);
    }

#ifdef ENABLE_XPLANE
    if (data_thread) {
        InterlockedExchange(&g_thread_exit, 1);
        WaitForSingleObject(data_thread, INFINITE);
        CloseHandle(data_thread);
    }
    if (data_thread) DeleteCriticalSection(&g_data_lock);
    ND_XPlane_Close();
#endif

    RouteIPC_Close(route_ipc);
    ND_Data_Close();
    ND_UI_Destroy(&ui);
    IMG_Quit();
    TTF_Quit();
    SDL_Quit();

    return 0;
}
