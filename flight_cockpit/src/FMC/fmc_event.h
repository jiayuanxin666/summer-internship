#ifndef FMC_EVENT_H
#define FMC_EVENT_H

#include <SDL2/SDL.h>
#include "fmc_connect.h"
#include "fmc_data.h"
#include "fmc_key.h"

typedef enum
{
    FMC_PAGE_BLANK = 0,
    FMC_PAGE_INDEX,
    FMC_PAGE_STATUS,
    FMC_PAGE_ROUTE,
    FMC_PAGE_CLIMB,
    FMC_PAGE_CRUISE,
    FMC_PAGE_DESCENT,
    FMC_PAGE_DEP_ARR_INDEX,
    FMC_PAGE_DEPARTURE,
    FMC_PAGE_ARRIVAL
} FMC_Page;

typedef struct
{
    FMC_Page page;
    int route_page;
    int procedure_airport_is_origin;
    const FMC_Button *hovered_button;
    FMC_Data data;
    FMC_Connect connect;
} FMC_State;

int FMC_Event_Init(FMC_State *state, const char *xplane_host);
void FMC_Event_Destroy(FMC_State *state);
void FMC_Event_HandleKey(FMC_State *state, FMC_Key key);
void FMC_Event_HandleSDL(FMC_State *state, const SDL_Event *event,
                         int logical_x, int logical_y, int *running);
int FMC_Event_RoutePageCount(const FMC_State *state);
int FMC_Event_RunSelfTest(void);

#endif
