#include "eicas2_data.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#ifdef ENABLE_XPLANE
#include "../Util/xplaneConnect.h"
#endif

static FILE *g_data_file = NULL;
static int g_file_checked = 0;
static int g_frame = 0;

static float clampf_local(float value, float min_value, float max_value)
{
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static float smooth_linear(float current, float target, float alpha)
{
    return current + (target - current) * alpha;
}

static FILE *open_data_file(void)
{
    const char *paths[] = {"assets/eicas2.dat", "../assets/eicas2.dat"};

    for (int i = 0; i < 2; ++i) {
        FILE *fp = fopen(paths[i], "r");
        if (fp) {
            return fp;
        }
    }

    return NULL;
}

static void normalize_data(EICAS2_Data *data)
{
    if (!data) {
        return;
    }

    data->n2_left = clampf_local(data->n2_left, 0.0f, 110.0f);
    data->n2_right = clampf_local(data->n2_right, 0.0f, 110.0f);
    data->ff_left = clampf_local(data->ff_left, 0.0f, 99.9f);
    data->ff_right = clampf_local(data->ff_right, 0.0f, 99.9f);
    data->oil_press_left = clampf_local(data->oil_press_left, 0.0f, 120.0f);
    data->oil_press_right = clampf_local(data->oil_press_right, 0.0f, 120.0f);
    data->oil_temp_left = clampf_local(data->oil_temp_left, 0.0f, 180.0f);
    data->oil_temp_right = clampf_local(data->oil_temp_right, 0.0f, 180.0f);
    data->oil_qty_left = clampf_local(data->oil_qty_left, 0.0f, 25.0f);
    data->oil_qty_right = clampf_local(data->oil_qty_right, 0.0f, 25.0f);
    data->vib_left = clampf_local(data->vib_left, 0.0f, 10.0f);
    data->vib_right = clampf_local(data->vib_right, 0.0f, 10.0f);
    data->valid = 1;
}

static void fill_internal_frame(EICAS2_Data *data)
{
    float t;

    if (!data) {
        return;
    }

    t = (float)g_frame / 60.0f;
    data->n2_left = 70.0f + sinf(t * 0.45f) * 12.0f;
    data->n2_right = 72.0f + sinf(t * 0.42f + 0.4f) * 12.0f;
    data->ff_left = 5.0f + sinf(t * 0.74f) * 4.0f;
    data->ff_right = 5.0f + sinf(t * 0.68f + 0.3f) * 4.0f;
    data->oil_press_left = 42.0f + sinf(t * 0.3f) * 5.0f;
    data->oil_press_right = 42.5f + sinf(t * 0.28f) * 5.0f;
    data->oil_temp_left = 92.0f + sinf(t * 0.21f) * 7.0f;
    data->oil_temp_right = 93.0f + sinf(t * 0.23f) * 7.0f;
    data->oil_qty_left = 12.0f + sinf(t * 0.18f) * 1.0f;
    data->oil_qty_right = 12.0f + sinf(t * 0.17f + 0.2f) * 1.0f;
    data->vib_left = 1.2f + sinf(t * 0.6f) * 0.5f;
    data->vib_right = 1.3f + sinf(t * 0.58f) * 0.5f;
    data->data_source = EICAS2_DATA_SOURCE_SIM;
    normalize_data(data);
    ++g_frame;
}

void EICAS2_Data_Init(EICAS2_Data *data)
{
    if (!data) {
        return;
    }

    memset(data, 0, sizeof(*data));
    data->n2_left = 70.0f;
    data->n2_right = 72.5f;
    data->ff_left = 3.5f;
    data->ff_right = 4.8f;
    data->oil_press_left = 40.0f;
    data->oil_press_right = 40.7f;
    data->oil_temp_left = 90.0f;
    data->oil_temp_right = 91.2f;
    data->oil_qty_left = 12.0f;
    data->oil_qty_right = 12.0f;
    data->vib_left = 1.0f;
    data->vib_right = 1.0f;
    data->data_source = EICAS2_DATA_SOURCE_SIM;
    normalize_data(data);
}

int EICAS2_Data_LoadNextFrame(EICAS2_Data *data)
{
    char line[256];
    float values[12];

    if (!data) {
        return 0;
    }

    if (!g_file_checked) {
        g_data_file = open_data_file();
        g_file_checked = 1;
    }

    if (!g_data_file) {
        fill_internal_frame(data);
        return 1;
    }

    if ((g_frame++ % 10) != 0) {
        return 1;
    }

    if (!fgets(line, sizeof(line), g_data_file)) {
        fseek(g_data_file, 0, SEEK_SET);
        if (!fgets(line, sizeof(line), g_data_file)) {
            fill_internal_frame(data);
            return 1;
        }
    }

    if (sscanf(line, " %f , %f , %f , %f , %f , %f , %f , %f , %f , %f , %f , %f",
               &values[0], &values[1], &values[2], &values[3], &values[4], &values[5],
               &values[6], &values[7], &values[8], &values[9], &values[10], &values[11]) == 12) {
        data->n2_left = values[0];
        data->n2_right = values[1];
        data->ff_left = values[2];
        data->ff_right = values[3];
        data->oil_press_left = values[4];
        data->oil_press_right = values[5];
        data->oil_temp_left = values[6];
        data->oil_temp_right = values[7];
        data->oil_qty_left = values[8];
        data->oil_qty_right = values[9];
        data->vib_left = values[10];
        data->vib_right = values[11];
        data->data_source = EICAS2_DATA_SOURCE_FILE;
        normalize_data(data);
        return 1;
    }

    fill_internal_frame(data);
    return 1;
}

void EICAS2_Data_Smooth(EICAS2_Data *display, const EICAS2_Data *target, float alpha)
{
    if (!display || !target) {
        return;
    }

    alpha = clampf_local(alpha, 0.0f, 1.0f);
    display->n2_left = smooth_linear(display->n2_left, target->n2_left, alpha);
    display->n2_right = smooth_linear(display->n2_right, target->n2_right, alpha);
    display->ff_left = smooth_linear(display->ff_left, target->ff_left, alpha);
    display->ff_right = smooth_linear(display->ff_right, target->ff_right, alpha);
    display->oil_press_left = smooth_linear(display->oil_press_left, target->oil_press_left, alpha);
    display->oil_press_right = smooth_linear(display->oil_press_right, target->oil_press_right, alpha);
    display->oil_temp_left = smooth_linear(display->oil_temp_left, target->oil_temp_left, alpha);
    display->oil_temp_right = smooth_linear(display->oil_temp_right, target->oil_temp_right, alpha);
    display->oil_qty_left = smooth_linear(display->oil_qty_left, target->oil_qty_left, alpha);
    display->oil_qty_right = smooth_linear(display->oil_qty_right, target->oil_qty_right, alpha);
    display->vib_left = smooth_linear(display->vib_left, target->vib_left, alpha);
    display->vib_right = smooth_linear(display->vib_right, target->vib_right, alpha);
    display->valid = target->valid;
    display->data_source = target->data_source;
    normalize_data(display);
}

void EICAS2_Data_Close(void)
{
    if (g_data_file) {
        fclose(g_data_file);
        g_data_file = NULL;
    }
    g_file_checked = 0;
}

#ifdef ENABLE_XPLANE

#define EICAS2_XPLANE_DREF_COUNT 5
#define EICAS2_KGSEC_TO_KLBH 7.936632f
#define EICAS2_OIL_QTY_FACTOR 18.889f

static XPCSocket g_xpc_socket;
static int g_xpc_open = 0;

int EICAS2_XPlane_Open(void)
{
    g_xpc_socket = openUDP("127.0.0.1");
    g_xpc_open = XPCSocket_IsOpen(g_xpc_socket);
    return g_xpc_open;
}

int EICAS2_XPlane_FetchData(EICAS2_Data *data)
{
    static const char *drefs[EICAS2_XPLANE_DREF_COUNT] = {
        "sim/flightmodel/engine/ENGN_N2_",
        "sim/cockpit2/engine/indicators/fuel_flow_kg_sec",
        "sim/cockpit2/engine/indicators/oil_pressure_psi",
        "sim/cockpit2/engine/indicators/oil_temperature_deg_C",
        "sim/flightmodel/engine/ENGN_oil_quan"
    };
    float values[EICAS2_XPLANE_DREF_COUNT][8];

    if (!data || !g_xpc_open) {
        return 0;
    }

    if (!getDREFs(g_xpc_socket, drefs, EICAS2_XPLANE_DREF_COUNT, values)) {
        return 0;
    }

    data->n2_left = values[0][0];
    data->n2_right = values[0][1];
    data->ff_left = values[1][0] * EICAS2_KGSEC_TO_KLBH;
    data->ff_right = values[1][1] * EICAS2_KGSEC_TO_KLBH;
    data->oil_press_left = values[2][0];
    data->oil_press_right = values[2][1];
    data->oil_temp_left = values[3][0];
    data->oil_temp_right = values[3][1];
    data->oil_qty_left = values[4][0] * EICAS2_OIL_QTY_FACTOR;
    data->oil_qty_right = values[4][1] * EICAS2_OIL_QTY_FACTOR;
    data->vib_left = 1.5f;
    data->vib_right = 1.5f;
    data->data_source = EICAS2_DATA_SOURCE_XPLANE;
    normalize_data(data);
    return 1;
}

void EICAS2_XPlane_Close(void)
{
    if (g_xpc_open) {
        closeUDP(g_xpc_socket);
        g_xpc_open = 0;
    }
}

#endif
