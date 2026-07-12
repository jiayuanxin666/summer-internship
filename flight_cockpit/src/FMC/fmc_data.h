#ifndef FMC_DATA_H
#define FMC_DATA_H

#include <stddef.h>

#define FMC_IDENT_LEN 16
#define FMC_TEXT_LEN 32
#define FMC_SCRATCHPAD_LEN 48
#define FMC_MAX_ROUTE_LEGS 64

typedef struct FMC_AvlNode FMC_AvlNode;

typedef struct
{
    char ident[FMC_IDENT_LEN];
    double latitude;
    double longitude;
} FMC_NavPoint;

typedef struct
{
    char via[FMC_IDENT_LEN];
    FMC_NavPoint waypoint;
} FMC_RouteLeg;

typedef struct
{
    const char *airport;
    const char *departure_procedures[4];
    const char *departure_runways[4];
    const char *departure_transitions[4];
    const char *arrival_procedures[4];
    const char *arrival_runways[4];
    const char *arrival_transitions[4];
} FMC_ProcedureCatalog;

typedef struct
{
    FMC_AvlNode *airport_root;
    FMC_AvlNode *fix_root;
    int airport_count;
    int fix_count;

    char scratchpad[FMC_SCRATCHPAD_LEN];
    char message[FMC_SCRATCHPAD_LEN];
    int delete_mode;

    FMC_NavPoint origin;
    FMC_NavPoint destination;
    int has_origin;
    int has_destination;
    char flight_number[FMC_TEXT_LEN];

    FMC_RouteLeg route[FMC_MAX_ROUTE_LEGS];
    int route_count;

    char climb_speed_low[8];
    char climb_speed_mach[8];
    char climb_transition_altitude[12];
    char speed_limit[8];
    char speed_limit_altitude[12];
    char cruise_speed[16];
    char cruise_altitude[12];
    char descent_speed[16];
    char descent_transition_level[12];
    char descent_vpa[8];

    int selected_departure_procedure;
    int selected_departure_runway;
    int selected_departure_transition;
    int selected_arrival_procedure;
    int selected_arrival_runway;
    int selected_arrival_transition;

    int exec_pending;
    int synchronized;
} FMC_Data;

int FMC_Data_Init(FMC_Data *data);
void FMC_Data_Destroy(FMC_Data *data);

const FMC_NavPoint *FMC_Data_FindAirport(const FMC_Data *data, const char *ident);
const FMC_NavPoint *FMC_Data_FindFix(const FMC_Data *data, const char *ident);
int FMC_Data_SetOrigin(FMC_Data *data, const char *ident);
int FMC_Data_SetDestination(FMC_Data *data, const char *ident);
int FMC_Data_SetFlightNumber(FMC_Data *data, const char *text);
int FMC_Data_AddRouteFix(FMC_Data *data, const char *ident);

void FMC_Data_AppendScratchpad(FMC_Data *data, const char *text);
void FMC_Data_ClearScratchpad(FMC_Data *data);
void FMC_Data_DeleteScratchpad(FMC_Data *data);
void FMC_Data_SetMessage(FMC_Data *data, const char *message);
const char *FMC_Data_ScratchpadText(const FMC_Data *data);

int FMC_Data_SetClimbSpeed(FMC_Data *data, const char *text);
int FMC_Data_SetSpeedAltitudeLimit(FMC_Data *data, const char *text);
int FMC_Data_SetAltitude(char *target, size_t target_size, const char *text,
                         int min_value, int max_value, int allow_flight_level);
int FMC_Data_SetCruiseSpeed(FMC_Data *data, const char *text);
int FMC_Data_SetDescentSpeed(FMC_Data *data, const char *text);
int FMC_Data_SetVpa(FMC_Data *data, const char *text);

int FMC_Data_IsRouteReady(const FMC_Data *data);
void FMC_Data_MarkModified(FMC_Data *data);
void FMC_Data_MarkSynchronized(FMC_Data *data);

const FMC_ProcedureCatalog *FMC_Data_GetProcedureCatalog(const FMC_Data *data, int arrival);
int FMC_Data_RunSelfTest(void);

#endif
