#include "pfd_data.h"
#include <SDL2/SDL.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static FILE *g_data_file = NULL;
static int g_file_checked = 0;
static int g_frame = 0;

static float clampf_local(float value, float min_value, float max_value)
{
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static float normalize_heading(float heading)
{
    heading = fmodf(heading, 360.0f);
    if (heading < 0.0f) {
        heading += 360.0f;
    }
    return heading;
}

static float smooth_linear(float current, float target, float alpha)
{
    return current + (target - current) * alpha;
}

static float smooth_heading(float current, float target, float alpha)
{
    float delta;

    current = normalize_heading(current);
    target = normalize_heading(target);
    delta = target - current;

    if (delta > 180.0f) {
        delta -= 360.0f;
    } else if (delta < -180.0f) {
        delta += 360.0f;
    }

    return normalize_heading(current + delta * alpha);
}

static FILE *open_data_file(void)
{
    const char *paths[] = {
        "assets/pfd.dat",
        "../assets/pfd.dat"
    };
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
        snprintf(full_path, sizeof(full_path), "%s../assets/pfd.dat", base_path);
        FILE *fp = fopen(full_path, "r");
        if (!fp) {
            snprintf(full_path, sizeof(full_path), "%sassets/pfd.dat", base_path);
            fp = fopen(full_path, "r");
        }
        SDL_free(base_path);
        if (fp) {
            return fp;
        }
    }

    return NULL;
}

static void normalize_data(PFD_Data *data)
{
    if (!data) {
        return;
    }

    data->pitch = clampf_local(data->pitch, -45.0f, 45.0f);
    data->roll = clampf_local(data->roll, -90.0f, 90.0f);
    data->yaw = normalize_heading(data->yaw);

    data->altitude = clampf_local(data->altitude, -1000.0f, 50000.0f);
    data->agl_altitude = clampf_local(data->agl_altitude, 0.0f, 50000.0f);
    data->altitude_target = clampf_local(data->altitude_target, -1000.0f, 50000.0f);

    data->throttle = clampf_local(data->throttle, 0.0f, 100.0f);
    data->airspeed_current = clampf_local(data->airspeed_current, 0.0f, 440.0f);
    data->airspeed_target = clampf_local(data->airspeed_target, 0.0f, 440.0f);
    data->vertical_speed = clampf_local(data->vertical_speed, -6000.0f, 6000.0f);

    data->heading = normalize_heading(data->heading);
    data->heading_target = normalize_heading(data->heading_target);

    if (data->data_source < PFD_DATA_SOURCE_SIM ||
        data->data_source > PFD_DATA_SOURCE_XPLANE) {
        data->data_source = PFD_DATA_SOURCE_SIM;
    }
}

static void fill_internal_frame(PFD_Data *data)
{
    float t;

    if (!data) {
        return;
    }

    t = (float)g_frame / 60.0f;

    data->airspeed_current = 210.0f + sinf(t * 0.55f) * 38.0f;
    data->airspeed_target = 230.0f + sinf(t * 0.22f) * 55.0f;
    data->altitude = 8600.0f + sinf(t * 0.18f) * 1350.0f;
    data->altitude_target = 9000.0f + sinf(t * 0.11f) * 2200.0f;
    data->agl_altitude = 1700.0f + sinf(t * 0.31f) * 900.0f;
    data->pitch = sinf(t * 0.9f) * 10.0f;
    data->roll = sinf(t * 0.48f) * 28.0f;
    data->yaw = 0.0f;
    data->vertical_speed = cosf(t * 0.42f) * 1800.0f;
    data->heading = normalize_heading(180.0f + t * 12.0f);
    data->heading_target = normalize_heading(data->heading + 42.0f + sinf(t * 0.28f) * 20.0f);
    data->throttle = 62.0f + sinf(t * 0.36f) * 25.0f;
    data->data_source = PFD_DATA_SOURCE_SIM;

    normalize_data(data);
    ++g_frame;
}

void PFD_Data_Init(PFD_Data *data)
{
    if (!data) {
        return;
    }

    memset(data, 0, sizeof(*data));
    data->pitch = 0.0f;
    data->roll = 0.0f;
    data->yaw = 0.0f;
    data->altitude = 2000.0f;
    data->agl_altitude = 1500.0f;
    data->altitude_target = 3000.0f;
    data->throttle = 50.0f;
    data->airspeed_current = 160.0f;
    data->airspeed_target = 180.0f;
    data->vertical_speed = 0.0f;
    data->heading = 180.0f;
    data->heading_target = 200.0f;
    data->data_source = PFD_DATA_SOURCE_SIM;
}

int PFD_Data_LoadNextFrame(PFD_Data *data)
{
    char line[256];
    float current_airspeed;
    float target_airspeed;
    float current_height;
    float target_height;
    float agl_altitude;
    float current_pitch;
    float current_roll;
    float current_vertical_speed;
    float current_heading;
    float target_heading;
    float current_thrust;

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

    for (int attempt = 0; attempt < 200; ++attempt) {
        if (!fgets(line, sizeof(line), g_data_file)) {
            fseek(g_data_file, 0, SEEK_SET);
            if (!fgets(line, sizeof(line), g_data_file)) {
                fill_internal_frame(data);
                return 1;
            }
        }

        if (sscanf(line,
                   " %f , %f , %f , %f , %f , %f , %f , %f , %f , %f , %f",
                   &current_airspeed,
                   &target_airspeed,
                   &current_height,
                   &target_height,
                   &agl_altitude,
                   &current_pitch,
                   &current_roll,
                   &current_vertical_speed,
                   &current_heading,
                   &target_heading,
                   &current_thrust) == 11) {
            data->airspeed_current = current_airspeed;
            data->airspeed_target = target_airspeed;
            data->altitude = current_height;
            data->altitude_target = target_height;
            data->agl_altitude = agl_altitude;
            data->pitch = current_pitch;
            data->roll = current_roll;
            data->vertical_speed = current_vertical_speed;
            data->heading = current_heading;
            data->heading_target = target_heading;
            data->throttle = current_thrust;
            data->yaw = current_heading;
            data->data_source = PFD_DATA_SOURCE_FILE;
            normalize_data(data);
            return 1;
        }
    }

    fill_internal_frame(data);
    return 1;
}

void PFD_Data_Smooth(PFD_Data *display, const PFD_Data *target, float alpha)
{
    if (!display || !target) {
        return;
    }

    alpha = clampf_local(alpha, 0.0f, 1.0f);

    display->pitch = smooth_linear(display->pitch, target->pitch, alpha);
    display->roll = smooth_linear(display->roll, target->roll, alpha);
    display->yaw = smooth_heading(display->yaw, target->yaw, alpha);
    display->altitude = smooth_linear(display->altitude, target->altitude, alpha);
    display->agl_altitude = smooth_linear(display->agl_altitude, target->agl_altitude, alpha);
    display->altitude_target = smooth_linear(display->altitude_target, target->altitude_target, alpha);
    display->throttle = smooth_linear(display->throttle, target->throttle, alpha);
    display->airspeed_current = smooth_linear(display->airspeed_current, target->airspeed_current, alpha);
    display->airspeed_target = smooth_linear(display->airspeed_target, target->airspeed_target, alpha);
    display->vertical_speed = smooth_linear(display->vertical_speed, target->vertical_speed, alpha);
    display->heading = smooth_heading(display->heading, target->heading, alpha);
    display->heading_target = smooth_heading(display->heading_target, target->heading_target, alpha);
    display->data_source = target->data_source;

    normalize_data(display);
}

void PFD_Data_Close(void)
{
    if (g_data_file) {
        fclose(g_data_file);
        g_data_file = NULL;
    }
    g_file_checked = 0;
}
