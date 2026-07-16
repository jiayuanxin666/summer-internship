#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <string.h>

#include "fmc_display.h"
#include "../Util/route_ipc.h"
#include "fmc_event.h"

static FMC_Page page_from_name(const char *name)
{
    if (!name) return FMC_PAGE_BLANK;
    if (strcmp(name, "index") == 0) return FMC_PAGE_INDEX;
    if (strcmp(name, "status") == 0) return FMC_PAGE_STATUS;
    if (strcmp(name, "route") == 0) return FMC_PAGE_ROUTE;
    if (strcmp(name, "climb") == 0) return FMC_PAGE_CLIMB;
    if (strcmp(name, "cruise") == 0) return FMC_PAGE_CRUISE;
    if (strcmp(name, "descent") == 0) return FMC_PAGE_DESCENT;
    if (strcmp(name, "depindex") == 0) return FMC_PAGE_DEP_ARR_INDEX;
    if (strcmp(name, "departure") == 0) return FMC_PAGE_DEPARTURE;
    if (strcmp(name, "arrival") == 0) return FMC_PAGE_ARRIVAL;
    return FMC_PAGE_BLANK;
}

static void seed_demo_state(FMC_State *state)
{
    if (!state) return;
    FMC_Data_SetOrigin(&state->data, "KSEA");
    FMC_Data_SetDestination(&state->data, "KBFI");
    FMC_Data_SetFlightNumber(&state->data, "AAA001");
    FMC_Data_AddRouteFix(&state->data, "FREDY");
    FMC_Data_AddRouteFix(&state->data, "RENTO");
    FMC_Data_AddRouteFix(&state->data, "TOTEM");
    state->data.message[0] = '\0';
    state->data.scratchpad[0] = '\0';
}

int main(int argc, char *argv[])
{
    FMC_Display display;
    FMC_State state;
    SDL_Event event;
    const char *xplane_host = "127.0.0.1";
    const char *screenshot_path = NULL;
    FMC_Page requested_page = FMC_PAGE_BLANK;
    int running = 1;
    RouteIPC *route_ipc = NULL;
    RouteIPCData last_published_route;
    int has_published_route = 0;
    int smoke = 0;
    int self_test = 0;
    int frame_count = 0;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--self-test") == 0) {
            self_test = 1;
        } else if (strcmp(argv[i], "--smoke") == 0) {
            smoke = 1;
        } else if (strcmp(argv[i], "--xplane-host") == 0 && i + 1 < argc) {
            xplane_host = argv[++i];
        } else if (strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc) {
            screenshot_path = argv[++i];
            smoke = 1;
        } else if (strcmp(argv[i], "--page") == 0 && i + 1 < argc) {
            requested_page = page_from_name(argv[++i]);
        }
    }

    SDL_SetMainReady();
    if (SDL_Init(self_test ? SDL_INIT_TIMER : SDL_INIT_VIDEO) != 0) {
        printf("SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    if (self_test) {
        int ok = FMC_Data_RunSelfTest() && FMC_Event_RunSelfTest() && RouteIPC_SelfTest();
        SDL_Quit();
        printf("FMC self-test: %s\n", ok ? "PASS" : "FAIL");
        return ok ? 0 : 1;
    }

    if (TTF_Init() == -1) {
        printf("TTF_Init failed: %s\n", TTF_GetError());
        SDL_Quit();
        return 1;
    }
    if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) {
        printf("IMG_Init failed: %s\n", IMG_GetError());
        TTF_Quit();
        SDL_Quit();
        return 1;
    }
    if (!FMC_Event_Init(&state, xplane_host)) {
        printf("FMC state initialization failed.\n");
        IMG_Quit();
        TTF_Quit();
        SDL_Quit();
        return 1;
    }
    if (FMC_Display_Init(&display) != 0) {
        FMC_Event_Destroy(&state);
        IMG_Quit();
        TTF_Quit();
        SDL_Quit();
        return 1;
    }
    route_ipc = RouteIPC_Open(1);
    memset(&last_published_route, 0, sizeof(last_published_route));

    if (requested_page != FMC_PAGE_BLANK) {
        seed_demo_state(&state);
        state.page = requested_page;
    }

    while (running) {
        while (SDL_PollEvent(&event)) {
            int logical_x = -1;
            int logical_y = -1;
            int window_x = 0;
            int window_y = 0;

            if (event.type == SDL_WINDOWEVENT &&
                event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                FMC_Display_UpdateSize(&display, event.window.data1, event.window.data2);
            }
            if (event.type == SDL_MOUSEMOTION) {
                window_x = event.motion.x;
                window_y = event.motion.y;
            } else if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) {
                window_x = event.button.x;
                window_y = event.button.y;
            } else {
                SDL_GetMouseState(&window_x, &window_y);
            }
            FMC_Display_WindowToLogical(&display, window_x, window_y, &logical_x, &logical_y);
            FMC_Event_HandleSDL(&state, &event, logical_x, logical_y, &running);
        }

        FMC_Display_Render(&display, &state);
        if (route_ipc && state.data.synchronized && FMC_Data_IsRouteReady(&state.data)) {
            RouteIPCData shared;
            memset(&shared, 0, sizeof(shared));
            if (state.data.has_origin && shared.point_count < ROUTE_IPC_CAPACITY) {
                snprintf(shared.points[shared.point_count].ident, ROUTE_IPC_IDENT_LEN, "%s", state.data.origin.ident);
                shared.points[shared.point_count].latitude = state.data.origin.latitude;
                shared.points[shared.point_count++].longitude = state.data.origin.longitude;
            }
            for (int i = 0; i < state.data.route_count && shared.point_count < ROUTE_IPC_CAPACITY; ++i) {
                snprintf(shared.points[shared.point_count].ident, ROUTE_IPC_IDENT_LEN, "%s", state.data.route[i].waypoint.ident);
                shared.points[shared.point_count].latitude = state.data.route[i].waypoint.latitude;
                shared.points[shared.point_count++].longitude = state.data.route[i].waypoint.longitude;
            }
            if (state.data.has_destination && shared.point_count < ROUTE_IPC_CAPACITY) {
                snprintf(shared.points[shared.point_count].ident, ROUTE_IPC_IDENT_LEN, "%s", state.data.destination.ident);
                shared.points[shared.point_count].latitude = state.data.destination.latitude;
                shared.points[shared.point_count++].longitude = state.data.destination.longitude;
            }
            if (!has_published_route || memcmp(&shared, &last_published_route, sizeof(shared)) != 0) {
                if (RouteIPC_Write(route_ipc, &shared)) {
                    last_published_route = shared;
                    has_published_route = 1;
                }
            }
        }
        ++frame_count;
        if (screenshot_path && frame_count == 2) {
            if (!FMC_Display_SaveScreenshot(&display, screenshot_path)) {
                printf("Failed to save screenshot: %s\n", SDL_GetError());
                running = 0;
                frame_count = -100;
            } else {
                running = 0;
            }
        } else if (smoke && frame_count >= 3) {
            running = 0;
        }
        SDL_Delay(16);
    }

    RouteIPC_Close(route_ipc);
    FMC_Display_Destroy(&display);
    FMC_Event_Destroy(&state);
    IMG_Quit();
    TTF_Quit();
    SDL_Quit();
    return frame_count < 0 ? 1 : 0;
}
