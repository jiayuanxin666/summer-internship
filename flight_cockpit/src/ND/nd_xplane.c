#include "nd_xplane.h"

#ifdef ENABLE_XPLANE

#include "../Util/xplaneConnect.h"
#include <math.h>

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
    ND_Data next;

    if (!data || !g_xpc_open) {
        return 0;
    }

    if (!getDREFs(g_xpc_socket, drefs, ND_XPLANE_DREF_COUNT, values)) {
        return 0;
    }

    for (int i = 0; i < ND_XPLANE_DREF_COUNT; ++i) {
        if (!isfinite(values[i][0])) {
            return 0;
        }
    }
    if (values[0][0] < -90.0f || values[0][0] > 90.0f ||
        values[1][0] < -180.0f || values[1][0] > 180.0f ||
        values[5][0] < 0.0f || values[6][0] < 0.0f || values[7][0] < 0.0f) {
        return 0;
    }

    next = *data;
    next.latitude = values[0][0];
    next.longitude = values[1][0];
    next.heading = ND_Data_NormalizeHeading(values[2][0]);
    next.track = ND_Data_NormalizeHeading(values[3][0]);
    next.target_heading = ND_Data_NormalizeHeading(values[4][0]);
    next.true_air_speed = values[5][0] * ND_MPS_TO_KT;
    next.ground_speed = values[6][0] * ND_MPS_TO_KT;
    next.wind_speed = values[7][0];
    next.wind_direction = ND_Data_NormalizeHeading(values[8][0]);
    next.data_source = ND_DATA_SOURCE_XPLANE;
    next.valid = 1;
    ND_Data_UpdateNavigation(&next);
    *data = next;
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
