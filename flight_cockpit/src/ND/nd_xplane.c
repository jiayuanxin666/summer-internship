#include "nd_xplane.h"

#ifdef ENABLE_XPLANE

#include "../Util/xplaneConnect.h"

#define ND_XPLANE_DREF_COUNT 9
#define ND_MPS_TO_KT 1.94384449f

static XPCSocket g_xpc_socket;
static int g_xpc_open = 0;

static int fetch_probe(void)
{
    const char *probe[] = {
        "sim/flightmodel/position/latitude"
    };
    float values[1][8];

    return getDREFs(g_xpc_socket, probe, 1, values);
}

int ND_XPlane_Open(void)
{
    g_xpc_socket = openUDP("127.0.0.1");
    g_xpc_open = XPCSocket_IsOpen(g_xpc_socket);

    if (!g_xpc_open) {
        return 0;
    }

    if (!fetch_probe()) {
        closeUDP(g_xpc_socket);
        g_xpc_open = 0;
        return 0;
    }

    return 1;
}

int ND_XPlane_FetchData(ND_Data *data)
{
    static const char *drefs[ND_XPLANE_DREF_COUNT] = {
        "sim/flightmodel/position/latitude",
        "sim/flightmodel/position/longitude",
        "sim/flightmodel/position/mag_psi",
        "sim/flightmodel/position/hpath",
        "sim/cockpit/autopilot/heading_mag",
        "sim/flightmodel/position/true_airspeed",
        "sim/flightmodel/position/groundspeed"
        ,"sim/weather/wind_speed_kt"
        ,"sim/weather/wind_direction_degt"
    };
    float values[ND_XPLANE_DREF_COUNT][8];

    if (!data || !g_xpc_open) {
        return 0;
    }

    if (!getDREFs(g_xpc_socket, drefs, ND_XPLANE_DREF_COUNT, values)) {
        return 0;
    }

    data->latitude = values[0][0];
    data->longitude = values[1][0];
    data->heading = ND_Data_NormalizeHeading(values[2][0]);
    data->track = ND_Data_NormalizeHeading(values[3][0]);
    data->target_heading = ND_Data_NormalizeHeading(values[4][0]);
    data->true_air_speed = values[5][0] * ND_MPS_TO_KT;
    data->ground_speed = values[6][0] * ND_MPS_TO_KT;
    data->wind_speed = values[7][0];
    data->wind_direction = ND_Data_NormalizeHeading(values[8][0]);
    data->data_source = ND_DATA_SOURCE_XPLANE;
    data->valid = 1;
    ND_Data_UpdateNavigation(data);
    return 1;
}

void ND_XPlane_Close(void)
{
    if (g_xpc_open) {
        closeUDP(g_xpc_socket);
        g_xpc_open = 0;
    }
}

#endif
