#include "fmc_event.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

static void set_page(FMC_State *state, FMC_Page page)
{
    if (!state) {
        return;
    }
    state->page = page;
    state->data.message[0] = '\0';
}

int FMC_Event_Init(FMC_State *state, const char *xplane_host)
{
    if (!state) {
        return 0;
    }
    memset(state, 0, sizeof(*state));
    state->page = FMC_PAGE_BLANK;
    state->route_page = 1;
    if (!FMC_Data_Init(&state->data)) {
        return 0;
    }
    FMC_Connect_Init(&state->connect, xplane_host);
    return 1;
}

void FMC_Event_Destroy(FMC_State *state)
{
    if (!state) {
        return;
    }
    FMC_Connect_Close(&state->connect);
    FMC_Data_Destroy(&state->data);
}

int FMC_Event_RoutePageCount(const FMC_State *state)
{
    int count;
    if (!state) {
        return 1;
    }
    count = 1 + (state->data.route_count + 4) / 5;
    return count < 1 ? 1 : count;
}

static int scratchpad_copy(const FMC_Data *data, char *out, size_t out_size)
{
    if (!data || !out || out_size == 0 || data->delete_mode || !data->scratchpad[0]) {
        return 0;
    }
    SDL_snprintf(out, out_size, "%s", data->scratchpad);
    return 1;
}

static void delete_route_field(FMC_State *state, FMC_Key key)
{
    FMC_Data *data = &state->data;
    if (key == FMC_KEY_L1) {
        data->has_origin = 0;
        memset(&data->origin, 0, sizeof(data->origin));
    } else if (key == FMC_KEY_R1) {
        data->has_destination = 0;
        memset(&data->destination, 0, sizeof(data->destination));
    } else if (key == FMC_KEY_R2) {
        data->flight_number[0] = '\0';
    } else {
        return;
    }
    FMC_Data_ClearScratchpad(data);
    FMC_Data_MarkModified(data);
    FMC_Data_RefreshRouteExecState(data);
}

static void handle_route_lsk(FMC_State *state, FMC_Key key)
{
    FMC_Data *data = &state->data;
    char input[FMC_SCRATCHPAD_LEN];

    if (state->route_page == 1) {
        if (data->delete_mode && (key == FMC_KEY_L1 || key == FMC_KEY_R1 || key == FMC_KEY_R2)) {
            delete_route_field(state, key);
            return;
        }
        if (!scratchpad_copy(data, input, sizeof(input))) {
            return;
        }
        if (key == FMC_KEY_L1) {
            FMC_Data_SetOrigin(data, input);
        } else if (key == FMC_KEY_R1) {
            FMC_Data_SetDestination(data, input);
        } else if (key == FMC_KEY_R2) {
            FMC_Data_SetFlightNumber(data, input);
        } else if (key == FMC_KEY_R5) {
            if (FMC_Data_AddRouteFix(data, input)) {
                int pages = FMC_Event_RoutePageCount(state);
                if (pages > 1) {
                    state->route_page = pages;
                }
            }
        }
        return;
    }

    if (key >= FMC_KEY_R1 && key <= FMC_KEY_R5) {
        int row = key - FMC_KEY_R1;
        int index = (state->route_page - 2) * 5 + row;
        if (data->delete_mode && index < data->route_count) {
            for (int i = index; i + 1 < data->route_count; ++i) {
                data->route[i] = data->route[i + 1];
            }
            --data->route_count;
            FMC_Data_ClearScratchpad(data);
            FMC_Data_MarkModified(data);
            FMC_Data_RefreshRouteExecState(data);
        } else if (scratchpad_copy(data, input, sizeof(input))) {
            FMC_Data_AddRouteFix(data, input);
        }
    }
}

static void mark_parameter_success(FMC_Data *data, int result)
{
    if (result) {
        FMC_Data_ClearScratchpad(data);
        FMC_Data_SetMessage(data, "");
        FMC_Data_MarkModified(data);
    }
}

static void handle_vnav_lsk(FMC_State *state, FMC_Key key)
{
    FMC_Data *data = &state->data;
    char input[FMC_SCRATCHPAD_LEN];
    int result = 0;
    if (!scratchpad_copy(data, input, sizeof(input))) {
        return;
    }

    if (state->page == FMC_PAGE_CLIMB) {
        if (key == FMC_KEY_L1) {
            FMC_Data_SetClimbSpeed(data, input);
            return;
        }
        if (key == FMC_KEY_L2) {
            FMC_Data_SetSpeedAltitudeLimit(data, input);
            return;
        }
        if (key == FMC_KEY_R1) {
            result = FMC_Data_SetAltitude(data->climb_transition_altitude,
                                          sizeof(data->climb_transition_altitude),
                                          input, 1000, 60000, 0);
        }
    } else if (state->page == FMC_PAGE_CRUISE) {
        if (key == FMC_KEY_L1) {
            FMC_Data_SetCruiseSpeed(data, input);
            return;
        }
        if (key == FMC_KEY_R1) {
            result = FMC_Data_SetAltitude(data->cruise_altitude,
                                          sizeof(data->cruise_altitude),
                                          input, 1000, 60000, 1);
        }
    } else if (state->page == FMC_PAGE_DESCENT) {
        if (key == FMC_KEY_L1) {
            FMC_Data_SetDescentSpeed(data, input);
            return;
        }
        if (key == FMC_KEY_L2) {
            FMC_Data_SetSpeedAltitudeLimit(data, input);
            return;
        }
        if (key == FMC_KEY_R1) {
            result = FMC_Data_SetAltitude(data->descent_transition_level,
                                          sizeof(data->descent_transition_level),
                                          input, 1000, 60000, 1);
        } else if (key == FMC_KEY_R3) {
            FMC_Data_SetVpa(data, input);
            return;
        }
    }

    if (!result) {
        FMC_Data_SetMessage(data, "INVALID ENTRY");
    } else {
        mark_parameter_success(data, result);
    }
}

static void select_procedure(FMC_State *state, FMC_Key key, int arrival)
{
    FMC_Data *data = &state->data;
    const char *airport = arrival
        ? (state->procedure_airport_is_origin
               ? (data->has_origin ? data->origin.ident : "KSEA")
               : (data->has_destination ? data->destination.ident : "KBFI"))
        : (data->has_origin ? data->origin.ident : "KSEA");
    const FMC_ProcedureCatalog *catalog = FMC_Data_GetProcedureCatalogForAirport(airport);
    const char *const *procedures = arrival ? catalog->arrival_procedures
                                            : catalog->departure_procedures;
    const char *const *runways = arrival ? catalog->arrival_runways
                                         : catalog->departure_runways;
    const char *const *transitions = arrival ? catalog->arrival_transitions
                                              : catalog->departure_transitions;
    int index;
    if (key >= FMC_KEY_L1 && key <= FMC_KEY_L3) {
        index = key - FMC_KEY_L1;
        if (!procedures[index]) {
            FMC_Data_SetMessage(data, "NOT AVAILABLE");
            return;
        }
        if (arrival) {
            data->selected_arrival_procedure = index;
            if (data->selected_arrival_runway >= 0 &&
                !FMC_Data_ProcedureCompatible(catalog, 1, index,
                                              data->selected_arrival_runway)) {
                data->selected_arrival_runway = -1;
            }
            data->selected_arrival_transition = -1;
        } else {
            data->selected_departure_procedure = index;
            if (data->selected_departure_runway >= 0 &&
                !FMC_Data_ProcedureCompatible(catalog, 0, index,
                                              data->selected_departure_runway)) {
                data->selected_departure_runway = -1;
            }
            data->selected_departure_transition = -1;
        }
        FMC_Data_MarkModified(data);
    } else if (key >= FMC_KEY_R1 && key <= FMC_KEY_R3) {
        index = key - FMC_KEY_R1;
        if (!runways[index]) {
            FMC_Data_SetMessage(data, "NOT AVAILABLE");
            return;
        }
        if (arrival) {
            data->selected_arrival_runway = index;
            if (data->selected_arrival_procedure >= 0 &&
                !FMC_Data_ProcedureCompatible(catalog, 1,
                                              data->selected_arrival_procedure, index)) {
                data->selected_arrival_procedure = -1;
            }
            data->selected_arrival_transition = -1;
        } else {
            data->selected_departure_runway = index;
            if (data->selected_departure_procedure >= 0 &&
                !FMC_Data_ProcedureCompatible(catalog, 0,
                                              data->selected_departure_procedure, index)) {
                data->selected_departure_procedure = -1;
            }
            data->selected_departure_transition = -1;
        }
        FMC_Data_MarkModified(data);
    } else if (key == FMC_KEY_L4 || key == FMC_KEY_R4) {
        index = key == FMC_KEY_L4 ? 0 : 1;
        if (!transitions[index]) {
            FMC_Data_SetMessage(data, "NOT AVAILABLE");
            return;
        }
        if (arrival) {
            data->selected_arrival_transition = index;
        } else {
            data->selected_departure_transition = index;
        }
        FMC_Data_MarkModified(data);
    }
    FMC_Data_SetMessage(data, "");
}

static void execute_pending(FMC_State *state)
{
    FMC_Data *data = &state->data;
    if (state->page == FMC_PAGE_ROUTE && !FMC_Data_IsRouteReady(data)) {
        data->exec_pending = 0;
        FMC_Data_SetMessage(data, "ROUTE INCOMPLETE");
        return;
    }
    if (!data->exec_pending) {
        FMC_Data_SetMessage(data, "NO MODIFICATION");
        return;
    }

    if (state->page == FMC_PAGE_ROUTE) {
        if (state->connect.available &&
            !FMC_Connect_SyncRoute(&state->connect,
                                   data->origin.ident,
                                   data->destination.ident,
                                   data->flight_number)) {
            FMC_Data_SetMessage(data, "XPLANE SYNC FAILED");
            return;
        }
    } else {
        if (state->connect.available &&
            !FMC_Connect_SendKey(&state->connect, FMC_KEY_EXEC)) {
            FMC_Data_SetMessage(data, "XPLANE SYNC FAILED");
            return;
        }
    }
    FMC_Data_MarkSynchronized(data);
}

static void handle_line_select(FMC_State *state, FMC_Key key)
{
    switch (state->page) {
    case FMC_PAGE_INDEX:
        if (key == FMC_KEY_L1) set_page(state, FMC_PAGE_STATUS);
        else if (key == FMC_KEY_R1) set_page(state, FMC_PAGE_ROUTE);
        else FMC_Data_SetMessage(&state->data, "FUNCTION NOT MODELED");
        break;
    case FMC_PAGE_STATUS:
        if (key == FMC_KEY_L6) set_page(state, FMC_PAGE_INDEX);
        else FMC_Data_SetMessage(&state->data, "FUNCTION NOT MODELED");
        break;
    case FMC_PAGE_ROUTE:
        handle_route_lsk(state, key);
        break;
    case FMC_PAGE_CLIMB:
    case FMC_PAGE_CRUISE:
    case FMC_PAGE_DESCENT:
        handle_vnav_lsk(state, key);
        break;
    case FMC_PAGE_DEP_ARR_INDEX:
        if (key == FMC_KEY_L1) {
            state->procedure_airport_is_origin = 1;
            set_page(state, FMC_PAGE_DEPARTURE);
        } else if (key == FMC_KEY_R1 || key == FMC_KEY_R2) {
            state->procedure_airport_is_origin = key == FMC_KEY_R1;
            state->data.selected_arrival_procedure = -1;
            state->data.selected_arrival_runway = -1;
            state->data.selected_arrival_transition = -1;
            set_page(state, FMC_PAGE_ARRIVAL);
        }
        break;
    case FMC_PAGE_DEPARTURE:
        select_procedure(state, key, 0);
        break;
    case FMC_PAGE_ARRIVAL:
        select_procedure(state, key, 1);
        break;
    default:
        break;
    }
}

static void change_page(FMC_State *state, int direction)
{
    if (state->page == FMC_PAGE_ROUTE) {
        int total = FMC_Event_RoutePageCount(state);
        int next = state->route_page + direction;
        if (next >= 1 && next <= total) {
            state->route_page = next;
            FMC_Data_SetMessage(&state->data, "");
        } else {
            FMC_Data_SetMessage(&state->data, direction > 0 ? "LAST PAGE" : "FIRST PAGE");
        }
    } else if (state->page == FMC_PAGE_CLIMB ||
               state->page == FMC_PAGE_CRUISE ||
               state->page == FMC_PAGE_DESCENT) {
        int page = (int)state->page + direction;
        if (page < FMC_PAGE_CLIMB) page = FMC_PAGE_CLIMB;
        if (page > FMC_PAGE_DESCENT) page = FMC_PAGE_DESCENT;
        if (page == (int)state->page) {
            FMC_Data_SetMessage(&state->data, direction > 0 ? "LAST PAGE" : "FIRST PAGE");
        } else {
            set_page(state, (FMC_Page)page);
        }
    }
}

void FMC_Event_HandleKey(FMC_State *state, FMC_Key key)
{
    const char *input;
    if (!state || key <= FMC_KEY_NONE || key >= FMC_KEY_COUNT) {
        return;
    }

    input = FMC_Key_InputText(key);
    if (input) {
        FMC_Data_AppendScratchpad(&state->data, input);
        return;
    }

    if (key >= FMC_KEY_L1 && key <= FMC_KEY_R6) {
        handle_line_select(state, key);
        return;
    }

    switch (key) {
    case FMC_KEY_INIT_REF:
        set_page(state, FMC_PAGE_INDEX);
        break;
    case FMC_KEY_RTE:
        state->route_page = 1;
        set_page(state, FMC_PAGE_ROUTE);
        break;
    case FMC_KEY_CLB:
        set_page(state, FMC_PAGE_CLIMB);
        break;
    case FMC_KEY_CRZ:
        set_page(state, FMC_PAGE_CRUISE);
        break;
    case FMC_KEY_DES:
        set_page(state, FMC_PAGE_DESCENT);
        break;
    case FMC_KEY_DEP_ARR:
        set_page(state, FMC_PAGE_DEP_ARR_INDEX);
        break;
    case FMC_KEY_PREV_PAGE:
        change_page(state, -1);
        break;
    case FMC_KEY_NEXT_PAGE:
        change_page(state, 1);
        break;
    case FMC_KEY_DEL:
        state->data.scratchpad[0] = '\0';
        state->data.message[0] = '\0';
        state->data.delete_mode = 1;
        break;
    case FMC_KEY_CLR:
        FMC_Data_DeleteScratchpad(&state->data);
        break;
    case FMC_KEY_EXEC:
        execute_pending(state);
        break;
    default:
        FMC_Data_SetMessage(&state->data, "FUNCTION NOT MODELED");
        break;
    }

    if (key != FMC_KEY_EXEC) {
        FMC_Connect_SendKey(&state->connect, key);
    }
}

static FMC_Key keyboard_to_key(SDL_Keycode keycode)
{
    if (keycode >= SDLK_a && keycode <= SDLK_z) {
        return (FMC_Key)(FMC_KEY_A + keycode - SDLK_a);
    }
    if (keycode >= SDLK_0 && keycode <= SDLK_9) {
        return (FMC_Key)(FMC_KEY_0 + keycode - SDLK_0);
    }
    switch (keycode) {
    case SDLK_PERIOD: return FMC_KEY_PERIOD;
    case SDLK_MINUS: return FMC_KEY_PLUS_MINUS;
    case SDLK_SLASH: return FMC_KEY_SLASH;
    case SDLK_SPACE: return FMC_KEY_SPACE;
    case SDLK_BACKSPACE: return FMC_KEY_CLR;
    case SDLK_DELETE: return FMC_KEY_DEL;
    case SDLK_RETURN: case SDLK_KP_ENTER: return FMC_KEY_EXEC;
    case SDLK_PAGEUP: return FMC_KEY_PREV_PAGE;
    case SDLK_PAGEDOWN: return FMC_KEY_NEXT_PAGE;
    case SDLK_F1: return FMC_KEY_INIT_REF;
    case SDLK_F2: return FMC_KEY_RTE;
    case SDLK_F3: return FMC_KEY_CLB;
    case SDLK_F4: return FMC_KEY_CRZ;
    case SDLK_F5: return FMC_KEY_DES;
    case SDLK_F6: return FMC_KEY_DEP_ARR;
    default: return FMC_KEY_NONE;
    }
}

void FMC_Event_HandleSDL(FMC_State *state, const SDL_Event *event,
                         int logical_x, int logical_y, int *running)
{
    if (!state || !event || !running) {
        return;
    }
    if (event->type == SDL_QUIT) {
        *running = 0;
    } else if (event->type == SDL_KEYDOWN) {
        if (event->key.keysym.sym == SDLK_ESCAPE) {
            *running = 0;
        } else {
            FMC_Key key = keyboard_to_key(event->key.keysym.sym);
            if (key != FMC_KEY_NONE) {
                FMC_Event_HandleKey(state, key);
            }
        }
    } else if (event->type == SDL_MOUSEMOTION) {
        state->hovered_button = FMC_Key_FindButton(logical_x, logical_y);
    } else if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT) {
        const FMC_Button *button = FMC_Key_FindButton(logical_x, logical_y);
        if (button) {
            state->hovered_button = button;
            FMC_Event_HandleKey(state, button->key);
        }
    }
}

int FMC_Event_RunSelfTest(void)
{
    FMC_State state;
    int ok;
    if (!FMC_Event_Init(&state, NULL)) {
        return 0;
    }
    ok = fmc_button_count == FMC_KEY_COUNT - 1;
    for (int key = FMC_KEY_L1; key < FMC_KEY_COUNT; ++key) {
        ok = ok && FMC_Key_GetButton((FMC_Key)key) != NULL;
    }
    ok = ok && strcmp(FMC_Key_XPlaneCommand(FMC_KEY_RTE), "sim/FMS/fpln") == 0;
    ok = ok && strcmp(FMC_Key_XPlaneCommand(FMC_KEY_EXEC), "sim/FMS/exec") == 0;
    FMC_Event_HandleKey(&state, FMC_KEY_RTE);
    FMC_Data_AppendScratchpad(&state.data, "KSEA");
    FMC_Event_HandleKey(&state, FMC_KEY_L1);
    ok = state.data.has_origin && !state.data.exec_pending;
    FMC_Event_HandleKey(&state, FMC_KEY_EXEC);
    ok = ok && strcmp(state.data.message, "ROUTE INCOMPLETE") == 0;
    FMC_Data_AppendScratchpad(&state.data, "KBFI");
    FMC_Event_HandleKey(&state, FMC_KEY_R1);
    FMC_Data_AppendScratchpad(&state.data, "AAA001");
    FMC_Event_HandleKey(&state, FMC_KEY_R2);
    FMC_Data_AppendScratchpad(&state.data, "FREDY");
    FMC_Event_HandleKey(&state, FMC_KEY_R5);
    ok = ok && state.page == FMC_PAGE_ROUTE &&
         state.data.has_origin && state.data.has_destination &&
         strcmp(state.data.flight_number, "AAA001") == 0 &&
         state.data.route_count == 1 && state.data.exec_pending;
    FMC_Event_HandleKey(&state, FMC_KEY_EXEC);
    ok = ok && !state.data.exec_pending && state.data.synchronized;
    FMC_Event_HandleKey(&state, FMC_KEY_CLB);
    FMC_Data_AppendScratchpad(&state.data, "310/.78");
    FMC_Event_HandleKey(&state, FMC_KEY_L1);
    ok = ok && strcmp(state.data.climb_speed_low, "310") == 0 &&
         strcmp(state.data.climb_speed_mach, ".78") == 0;
    FMC_Event_HandleKey(&state, FMC_KEY_DEP_ARR);
    FMC_Event_HandleKey(&state, FMC_KEY_L1);
    FMC_Event_HandleKey(&state, FMC_KEY_L1);
    ok = ok && state.page == FMC_PAGE_DEPARTURE &&
         state.data.selected_departure_procedure == 0;
    FMC_Event_HandleKey(&state, FMC_KEY_R2);
    ok = ok && state.data.selected_departure_runway == 1 &&
         state.data.selected_departure_procedure == -1;
    FMC_Event_HandleKey(&state, FMC_KEY_DEP_ARR);
    FMC_Event_HandleKey(&state, FMC_KEY_R1);
    ok = ok && state.page == FMC_PAGE_ARRIVAL && state.procedure_airport_is_origin;
    FMC_Event_HandleKey(&state, FMC_KEY_DEP_ARR);
    FMC_Event_HandleKey(&state, FMC_KEY_R2);
    ok = ok && state.page == FMC_PAGE_ARRIVAL && !state.procedure_airport_is_origin;
    FMC_Event_Destroy(&state);
    return ok;
}
