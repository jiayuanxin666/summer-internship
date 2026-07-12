#include "pfd_xplane.h"

#ifdef ENABLE_XPLANE

#include "../Util/xplaneConnect.h"
#include <math.h>
#include <stdio.h>

#define PFD_XPLANE_DREF_COUNT 12

static XPCSocket g_xpc_socket;
static int g_xpc_open = 0;

static float clampf_xplane(float value, float min_value, float max_value)
{
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static float normalize_heading_xplane(float heading)
{
    heading = fmodf(heading, 360.0f);
    if (heading < 0.0f) {
        heading += 360.0f;
    }
    return heading;
}

static void normalize_pfd_data(PFD_Data *data)
{
    data->pitch = clampf_xplane(data->pitch, -45.0f, 45.0f);
    data->roll = clampf_xplane(data->roll, -90.0f, 90.0f);
    data->yaw = normalize_heading_xplane(data->yaw);
    data->altitude = clampf_xplane(data->altitude, -1000.0f, 50000.0f);
    data->agl_altitude = clampf_xplane(data->agl_altitude, 0.0f, 50000.0f);
    data->altitude_target = clampf_xplane(data->altitude_target, -1000.0f, 50000.0f);
    data->throttle = clampf_xplane(data->throttle, 0.0f, 100.0f);
    data->airspeed_current = clampf_xplane(data->airspeed_current, 0.0f, 440.0f);
    data->airspeed_target = clampf_xplane(data->airspeed_target, 0.0f, 440.0f);
    data->vertical_speed = clampf_xplane(data->vertical_speed, -6000.0f, 6000.0f);
    data->heading = normalize_heading_xplane(data->heading);
    data->heading_target = normalize_heading_xplane(data->heading_target);
}

static int fetch_probe(void)
{
    const char *probe[] = {
        "sim/flightmodel/position/theta"
    };
    float values[1][8];

    if (!getDREFs(g_xpc_socket, probe, 1, values)) return 0;
    printf("XPC probe succeeded: sim/flightmodel/position/theta = %.2f\n", values[0][0]);
    return 1;
}

int PFD_XPlane_Open(void)
{
    printf("Connecting to X-Plane Connect at 127.0.0.1:%d...\n", PFD_XPLANE_PORT);
    g_xpc_socket = aopenUDP("127.0.0.1", PFD_XPLANE_PORT, 0);
    g_xpc_open = XPCSocket_IsOpen(g_xpc_socket);

    if (!g_xpc_open) {
        return 0;
    }
    printf("XPC socket opened.\n");

    if (!fetch_probe()) {
        closeUDP(g_xpc_socket);
        g_xpc_open = 0;
        return 0;
    }

    printf("X-Plane Connect connected.\n");
    return 1;
}

int PFD_XPlane_FetchData(PFD_Data *data)
{
    static const char *drefs[PFD_XPLANE_DREF_COUNT] = {
        "sim/flightmodel/position/theta",
        "sim/flightmodel/position/phi",
        "sim/flightmodel/position/psi",
        "sim/flightmodel/position/elevation",
        "sim/flightmodel/position/y_agl",
        "sim/flightmodel/engine/ENGN_thro",
        "sim/flightmodel/position/indicated_airspeed",
        "sim/cockpit/autopilot/airspeed",
        "sim/flightmodel/position/vh_ind_fpm",
        "sim/flightmodel/position/mag_psi",
        "sim/cockpit/autopilot/heading_mag",
        "sim/cockpit/autopilot/altitude"
    };
    float values[PFD_XPLANE_DREF_COUNT][8];
    float throttle_sum = 0.0f;
    int engine_count = PFD_ENGINE_COUNT;

    if (!data || !g_xpc_open) {
        return 0;
    }

    if (!getDREFs(g_xpc_socket, drefs, PFD_XPLANE_DREF_COUNT, values)) {
        return 0;
    }

    if (engine_count < 1) {
        engine_count = 1;
    }
    if (engine_count > 8) {
        engine_count = 8;
    }

    /* TODO_XPLANE: obtain this dynamically from aircraft configuration or an engine-count DataRef. */
    for (int i = 0; i < engine_count; ++i) {
        throttle_sum += values[5][i];
    }

    data->pitch = values[0][0];
    data->roll = values[1][0];
    data->yaw = values[2][0];
    data->altitude = values[3][0] * 3.28084f;
    data->agl_altitude = values[4][0] * 3.28084f;
    data->throttle = (throttle_sum / (float)engine_count) * 100.0f;
    data->airspeed_current = values[6][0];
    data->airspeed_target = values[7][0];
    data->vertical_speed = values[8][0];
    data->heading = values[9][0];
    data->heading_target = values[10][0];
    data->altitude_target = values[11][0];
    data->data_source = PFD_DATA_SOURCE_XPLANE;

    normalize_pfd_data(data);
    return 1;
}

void PFD_XPlane_Close(void)
{
    if (g_xpc_open) {
        closeUDP(g_xpc_socket);
        g_xpc_open = 0;
    }
}

#endif
