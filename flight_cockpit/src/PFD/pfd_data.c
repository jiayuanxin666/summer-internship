#include "pfd_data.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static FILE *g_data_file = NULL;
static int g_file_checked = 0;
static int g_frame = 0;

static float clampf(float value, float min_value, float max_value)
{
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static FILE *open_data_file(void)
{
    const char *paths[] = {
        "assets/pfd.dat",
        "../assets/pfd.dat"
    };

    for (int i = 0; i < 2; ++i) {
        FILE *fp = fopen(paths[i], "r");
        if (fp) {
            return fp;
        }
    }
    return NULL;
}

static void normalize_data(PFD_Data *data)
{
    data->pitch = clampf(data->pitch, -45.0f, 45.0f);
    data->roll = clampf(data->roll, -90.0f, 90.0f);
    data->yaw = clampf(data->yaw, 0.0f, 359.9f);
    data->altitude = clampf(data->altitude, -1000.0f, 50000.0f);
    data->agl_altitude = clampf(data->agl_altitude, 0.0f, 50000.0f);
    data->altitude_target = clampf(data->altitude_target, -1000.0f, 50000.0f);
    data->airspeed_current = clampf(data->airspeed_current, 0.0f, 440.0f);
    data->airspeed_target = clampf(data->airspeed_target, 0.0f, 440.0f);
    data->vertical_speed = clampf(data->vertical_speed, -6000.0f, 6000.0f);
    data->heading = fmodf(data->heading, 360.0f);
    if (data->heading < 0.0f) data->heading += 360.0f;
    data->heading_target = fmodf(data->heading_target, 360.0f);
    if (data->heading_target < 0.0f) data->heading_target += 360.0f;

    /* pfd.dat and internal frames use 0..100. X-Plane 0..1 throttle is converted in pfd_xplane.c later. */
    data->throttle = clampf(data->throttle, 0.0f, 100.0f);
}

static void fill_internal_frame(PFD_Data *data)
{
    float t = (float)g_frame / 60.0f;

    data->airspeed_current = 210.0f + sinf(t * 0.55f) * 38.0f;
    data->airspeed_target = 230.0f + sinf(t * 0.22f) * 55.0f;
    data->altitude = 8600.0f + sinf(t * 0.18f) * 1350.0f;
    data->altitude_target = 9000.0f + sinf(t * 0.11f) * 2200.0f;
    data->agl_altitude = 1700.0f + sinf(t * 0.31f) * 900.0f;
    data->pitch = sinf(t * 0.9f) * 10.0f;
    data->roll = sinf(t * 0.48f) * 28.0f;
    data->yaw = 0.0f;
    data->vertical_speed = cosf(t * 0.42f) * 1800.0f;
    data->heading = fmodf(180.0f + t * 12.0f, 360.0f);
    data->heading_target = fmodf(data->heading + 42.0f + sinf(t * 0.28f) * 20.0f, 360.0f);
    data->throttle = 62.0f + sinf(t * 0.36f) * 25.0f;

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
            normalize_data(data);
            return 1;
        }
    }

    /* 文件存在但连续多行格式错误时，退回内部模拟数据，避免主循环卡死。 */
    fill_internal_frame(data);
    return 1;
}

void PFD_Data_Close(void)
{
    if (g_data_file) {
        fclose(g_data_file);
        g_data_file = NULL;
    }
    g_file_checked = 0;
}
