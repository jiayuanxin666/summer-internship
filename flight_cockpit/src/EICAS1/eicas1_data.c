#include "eicas1_data.h"
#include <SDL2/SDL.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#ifdef ENABLE_XPLANE
#include "../Util/xplaneConnect.h"
#endif

static FILE *g_data_file = NULL;
static int g_file_checked = 0;
static int g_frame = 0;
static int g_parse_failures = 0;

static float finite_or(float value, float fallback)
{
    return isfinite(value) ? value : fallback;
}

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
    const char *paths[] = {"assets/eicas1.dat", "../assets/eicas1.dat"};
    char full_path[1024];
    char *base_path;

    for (int i = 0; i < 2; ++i) {
        FILE *fp = fopen(paths[i], "r");
        if (fp) {
            return fp;
        }
    }

    base_path = SDL_GetBasePath();
    if (base_path) {
        snprintf(full_path, sizeof(full_path), "%s../assets/eicas1.dat", base_path);
        FILE *fp = fopen(full_path, "r");
        if (!fp) {
            snprintf(full_path, sizeof(full_path), "%sassets/eicas1.dat", base_path);
            fp = fopen(full_path, "r");
        }
        SDL_free(base_path);
        if (fp) {
            return fp;
        }
    }

    return NULL;
}

static void normalize_data(EICAS1_Data *data)
{
    if (!data) {
        return;
    }

    data->tat = clampf_local(finite_or(data->tat, 0.0f), -80.0f, 80.0f);
    data->n1_left = clampf_local(finite_or(data->n1_left, 0.0f), 0.0f, 110.0f);
    data->n1_right = clampf_local(finite_or(data->n1_right, 0.0f), 0.0f, 110.0f);
    data->egt_left = clampf_local(finite_or(data->egt_left, 0.0f), 0.0f, 1100.0f);
    data->egt_right = clampf_local(finite_or(data->egt_right, 0.0f), 0.0f, 1100.0f);
    data->ff_left = clampf_local(finite_or(data->ff_left, 0.0f), 0.0f, 99.9f);
    data->ff_right = clampf_local(finite_or(data->ff_right, 0.0f), 0.0f, 99.9f);
    data->fuel_center = clampf_local(finite_or(data->fuel_center, 0.0f), 0.0f, 999.9f);
    data->fuel_left = clampf_local(finite_or(data->fuel_left, 0.0f), 0.0f, 999.9f);
    data->fuel_right = clampf_local(finite_or(data->fuel_right, 0.0f), 0.0f, 999.9f);
    data->alert_left = (data->n1_left > 100.0f ? EICAS1_ALERT_N1_HIGH : 0u) |
                       (data->egt_left >= 900.0f ? EICAS1_ALERT_EGT_HIGH : 0u) |
                       (data->ff_left < 0.5f ? EICAS1_ALERT_FF_LOW : 0u) |
                       (data->fuel_left < 1.0f ? EICAS1_ALERT_FUEL_LOW : 0u);
    data->alert_right = (data->n1_right > 100.0f ? EICAS1_ALERT_N1_HIGH : 0u) |
                        (data->egt_right >= 900.0f ? EICAS1_ALERT_EGT_HIGH : 0u) |
                        (data->ff_right < 0.5f ? EICAS1_ALERT_FF_LOW : 0u) |
                        (data->fuel_right < 1.0f ? EICAS1_ALERT_FUEL_LOW : 0u);
    data->valid = 1;
}

static void fill_internal_frame(EICAS1_Data *data)
{
    float t;

    if (!data) {
        return;
    }

    t = (float)g_frame / 60.0f;
    data->tat = 10.0f + sinf(t * 0.25f) * 7.0f;
    data->n1_left = 72.0f + sinf(t * 0.52f) * 14.0f;
    data->n1_right = 73.5f + sinf(t * 0.49f + 0.4f) * 14.0f;
    data->egt_left = 650.0f + sinf(t * 0.43f) * 75.0f;
    data->egt_right = 662.0f + sinf(t * 0.39f + 0.3f) * 75.0f;
    data->ff_left = 6.0f + sinf(t * 0.75f) * 3.0f;
    data->ff_right = 6.2f + sinf(t * 0.70f + 0.5f) * 3.0f;
    data->fuel_center = 5.0f - (float)g_frame * 0.0005f;
    data->fuel_left = 4.0f - (float)g_frame * 0.0004f;
    data->fuel_right = 4.0f - (float)g_frame * 0.0004f;
    data->data_source = EICAS1_DATA_SOURCE_SIM;
    normalize_data(data);
    ++g_frame;
}

void EICAS1_Data_Init(EICAS1_Data *data)
{
    if (!data) {
        return;
    }

    memset(data, 0, sizeof(*data));
    data->tat = 10.0f;
    data->n1_left = 60.0f;
    data->n1_right = 63.5f;
    data->egt_left = 650.0f;
    data->egt_right = 660.0f;
    data->ff_left = 8.8f;
    data->ff_right = 1.0f;
    data->fuel_center = 5.0f;
    data->fuel_left = 4.0f;
    data->fuel_right = 4.0f;
    data->data_source = EICAS1_DATA_SOURCE_SIM;
    normalize_data(data);
}

int EICAS1_Data_LoadNextFrame(EICAS1_Data *data)
{
    char line[256];
    float tat;
    float n1_left;
    float n1_right;
    float egt_left;
    float egt_right;
    float ff_left;
    float ff_right;
    float fuel_left;
    float fuel_center;
    float fuel_right;

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

    for (int attempt = 0; attempt < 8; ++attempt) {
        if (!fgets(line, sizeof(line), g_data_file)) {
            clearerr(g_data_file);
            fseek(g_data_file, 0, SEEK_SET);
            continue;
        }
        if (sscanf(line, " %f , %f , %f , %f , %f , %f , %f , %f , %f , %f",
               &tat, &n1_left, &n1_right, &egt_left, &egt_right,
               &ff_left, &ff_right, &fuel_left, &fuel_center, &fuel_right) == 10) {
        data->tat = tat;
        data->n1_left = n1_left;
        data->n1_right = n1_right;
        data->egt_left = egt_left;
        data->egt_right = egt_right;
        data->ff_left = ff_left;
        data->ff_right = ff_right;
        /* The last three CSV columns are L/C/R tanks in lb; display uses LBS x 1000. */
        data->fuel_left = fuel_left / 1000.0f;
        data->fuel_center = fuel_center / 1000.0f;
        data->fuel_right = fuel_right / 1000.0f;
        data->data_source = EICAS1_DATA_SOURCE_FILE;
        normalize_data(data);
        g_parse_failures = 0;
        return 1;
        }
    }
    ++g_parse_failures;
    fill_internal_frame(data);
    return 1;
}

void EICAS1_Data_Smooth(EICAS1_Data *display, const EICAS1_Data *target, float alpha)
{
    if (!display || !target) {
        return;
    }

    alpha = clampf_local(alpha, 0.0f, 1.0f);
    display->tat = smooth_linear(display->tat, target->tat, alpha);
    display->n1_left = smooth_linear(display->n1_left, target->n1_left, alpha);
    display->n1_right = smooth_linear(display->n1_right, target->n1_right, alpha);
    display->egt_left = smooth_linear(display->egt_left, target->egt_left, alpha);
    display->egt_right = smooth_linear(display->egt_right, target->egt_right, alpha);
    display->ff_left = smooth_linear(display->ff_left, target->ff_left, alpha);
    display->ff_right = smooth_linear(display->ff_right, target->ff_right, alpha);
    display->fuel_center = smooth_linear(display->fuel_center, target->fuel_center, alpha);
    display->fuel_left = smooth_linear(display->fuel_left, target->fuel_left, alpha);
    display->fuel_right = smooth_linear(display->fuel_right, target->fuel_right, alpha);
    display->valid = target->valid;
    display->data_source = target->data_source;
    normalize_data(display);
}

void EICAS1_Data_Close(void)
{
    if (g_data_file) {
        fclose(g_data_file);
        g_data_file = NULL;
    }
    g_file_checked = 0;
    g_frame = 0;
    g_parse_failures = 0;
}

#ifdef ENABLE_XPLANE

#define EICAS1_XPLANE_DREF_COUNT 5
#define EICAS1_KGSEC_TO_KLBH 7.936632f
#define EICAS1_KG_TO_KLB 0.00220462262185f

static XPCSocket g_xpc_socket;
static int g_xpc_open = 0;

static int fetch_probe(void)
{
    const char *probe[] = {
        "sim/weather/temperature_ambient_c"
    };
    float values[1][8];

    return getDREFs(g_xpc_socket, probe, 1, values);
}

int EICAS1_XPlane_Open(void)
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

    return g_xpc_open;
}

int EICAS1_XPlane_FetchData(EICAS1_Data *data)
{
    static const char *drefs[EICAS1_XPLANE_DREF_COUNT] = {
        "sim/weather/temperature_ambient_c",
        "sim/flightmodel/engine/ENGN_N1_",
        "sim/flightmodel/engine/ENGN_EGT_c",
        "sim/cockpit2/engine/indicators/fuel_flow_kg_sec",
        "sim/flightmodel/weight/m_fuel"
    };
    float tat[1] = {0};
    float n1[8] = {0};
    float egt[8] = {0};
    float ff[8] = {0};
    float fuel[9] = {0};
    float *values[] = {tat, n1, egt, ff, fuel};
    const int capacities[] = {1, 8, 8, 8, 9};

    if (!data || !g_xpc_open) {
        return 0;
    }

    if (!getDREFsSized(g_xpc_socket, drefs, capacities,
                       EICAS1_XPLANE_DREF_COUNT, values)) {
        return 0;
    }

    data->tat = tat[0];
    data->n1_left = n1[0]; data->n1_right = n1[1];
    data->egt_left = egt[0]; data->egt_right = egt[1];
    data->ff_left = ff[0] * EICAS1_KGSEC_TO_KLBH;
    data->ff_right = ff[1] * EICAS1_KGSEC_TO_KLBH;
    data->fuel_left = fuel[0] * EICAS1_KG_TO_KLB;
    data->fuel_center = fuel[1] * EICAS1_KG_TO_KLB;
    data->fuel_right = fuel[2] * EICAS1_KG_TO_KLB;
    data->data_source = EICAS1_DATA_SOURCE_XPLANE;
    normalize_data(data);
    return 1;
}

void EICAS1_XPlane_Close(void)
{
    if (g_xpc_open) {
        closeUDP(g_xpc_socket);
        g_xpc_open = 0;
    }
}

#endif
