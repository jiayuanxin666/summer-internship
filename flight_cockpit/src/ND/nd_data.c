#include "nd_data.h"
#include <SDL2/SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ND_PI 3.14159265358979323846
#define ND_EARTH_RADIUS_KM 6371.0
#define ND_KM_PER_NM 1.852
#define ND_VISIBLE_RANGE_NM 80.0f
#define ND_VISIBLE_ANGLE_DEG 45.0f
#define ND_GRID_LAT_CELLS 181
#define ND_GRID_LON_CELLS 360
#define ND_GRID_BUCKETS (ND_GRID_LAT_CELLS * ND_GRID_LON_CELLS)

typedef struct
{
    ND_MapPoint *items;
    int *next_indices;
    int *buckets;
    int count;
    int capacity;
    int loaded;
} ND_NavDatabase;

static FILE *g_data_file = NULL;
static int g_file_checked = 0;
static int g_frame = 0;
static ND_NavDatabase g_nav_db = {0};
static ND_MapPoint g_route[ND_MAX_ROUTE_POINTS];
static int g_route_count = 0;

static float clampf_local(float value, float min_value, float max_value)
{
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static double deg_to_rad(double deg)
{
    return deg * ND_PI / 180.0;
}

static int clampi_local(int value, int min_value, int max_value)
{
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static float smooth_linear(float current, float target, float alpha)
{
    return current + (target - current) * alpha;
}

float ND_Data_NormalizeHeading(float heading)
{
    heading = fmodf(heading, 360.0f);
    if (heading < 0.0f) {
        heading += 360.0f;
    }
    return heading;
}

float ND_Data_AngleDelta(float from_deg, float to_deg)
{
    float delta = ND_Data_NormalizeHeading(to_deg) - ND_Data_NormalizeHeading(from_deg);
    if (delta > 180.0f) {
        delta -= 360.0f;
    } else if (delta < -180.0f) {
        delta += 360.0f;
    }
    return delta;
}

float ND_Data_DistanceNm(double lat1, double lon1, double lat2, double lon2)
{
    double phi1 = deg_to_rad(lat1);
    double phi2 = deg_to_rad(lat2);
    double dphi = deg_to_rad(lat2 - lat1);
    double dlambda = deg_to_rad(lon2 - lon1);
    double s1 = sin(dphi / 2.0);
    double s2 = sin(dlambda / 2.0);
    double a = s1 * s1 + cos(phi1) * cos(phi2) * s2 * s2;
    double c = 2.0 * atan2(sqrt(a), sqrt(1.0 - a));
    return (float)((ND_EARTH_RADIUS_KM * c) / ND_KM_PER_NM);
}

float ND_Data_BearingDeg(double lat1, double lon1, double lat2, double lon2)
{
    double phi1 = deg_to_rad(lat1);
    double phi2 = deg_to_rad(lat2);
    double dlambda = deg_to_rad(lon2 - lon1);
    double y = sin(dlambda) * cos(phi2);
    double x = cos(phi1) * sin(phi2) - sin(phi1) * cos(phi2) * cos(dlambda);
    return ND_Data_NormalizeHeading((float)(atan2(y, x) * 180.0 / ND_PI));
}

static FILE *open_first_existing(const char *a, const char *b)
{
    char full_path[1024];
    char *base_path;
    FILE *fp = fopen(a, "r");
    if (fp) {
        return fp;
    }

    fp = fopen(b, "r");
    if (fp) {
        return fp;
    }

    base_path = SDL_GetBasePath();
    if (base_path) {
        snprintf(full_path, sizeof(full_path), "%s%s", base_path, a);
        fp = fopen(full_path, "r");
        if (!fp) {
            snprintf(full_path, sizeof(full_path), "%s%s", base_path, b);
            fp = fopen(full_path, "r");
        }
        SDL_free(base_path);
    }

    return fp;
}

static FILE *open_data_file(void)
{
    return open_first_existing("assets/nd.dat", "../assets/nd.dat");
}

static void copy_ident(char *dst, size_t dst_size, const char *src)
{
    if (!dst || dst_size == 0) {
        return;
    }
    if (!src || !src[0]) {
        dst[0] = '\0';
        return;
    }
    snprintf(dst, dst_size, "%s", src);
}

static int nav_grid_key_from_cell(int lat_cell, int lon_cell)
{
    lat_cell = clampi_local(lat_cell, 0, ND_GRID_LAT_CELLS - 1);

    while (lon_cell < 0) {
        lon_cell += ND_GRID_LON_CELLS;
    }
    while (lon_cell >= ND_GRID_LON_CELLS) {
        lon_cell -= ND_GRID_LON_CELLS;
    }

    return lat_cell * ND_GRID_LON_CELLS + lon_cell;
}

static int nav_grid_key(double latitude, double longitude)
{
    int lat_cell = (int)floor(latitude) + 90;
    int lon_cell = (int)floor(longitude) + 180;

    return nav_grid_key_from_cell(lat_cell, lon_cell);
}

static int nav_db_init_grid(void)
{
    if (g_nav_db.buckets) {
        return 1;
    }

    g_nav_db.buckets = (int *)malloc(sizeof(int) * ND_GRID_BUCKETS);
    if (!g_nav_db.buckets) {
        return 0;
    }

    for (int i = 0; i < ND_GRID_BUCKETS; ++i) {
        g_nav_db.buckets[i] = -1;
    }

    return 1;
}

static int nav_db_push(const ND_MapPoint *point)
{
    ND_MapPoint *items;
    int *next_indices;
    int new_capacity;
    int index;
    int key;

    if (!point) {
        return 0;
    }

    if (!nav_db_init_grid()) {
        return 0;
    }

    if (g_nav_db.count >= g_nav_db.capacity) {
        new_capacity = g_nav_db.capacity == 0 ? 4096 : g_nav_db.capacity * 2;
        items = (ND_MapPoint *)realloc(g_nav_db.items, sizeof(ND_MapPoint) * (size_t)new_capacity);
        if (!items) {
            return 0;
        }
        g_nav_db.items = items;

        next_indices = (int *)realloc(g_nav_db.next_indices, sizeof(int) * (size_t)new_capacity);
        if (!next_indices) {
            return 0;
        }
        g_nav_db.next_indices = next_indices;
        g_nav_db.capacity = new_capacity;
    }

    index = g_nav_db.count++;
    key = nav_grid_key(point->latitude, point->longitude);
    g_nav_db.items[index] = *point;
    g_nav_db.next_indices[index] = g_nav_db.buckets[key];
    g_nav_db.buckets[key] = index;
    return 1;
}

static int load_route_file(void)
{
    FILE *fp = open_first_existing("assets/KSEAKBFI.fms", "../assets/KSEAKBFI.fms");
    char line[256];
    int type;
    int unused;
    char ident[ND_IDENT_LEN];
    double lat;
    double lon;

    g_route_count = 0;
    if (!fp) {
        return 0;
    }

    while (fgets(line, sizeof(line), fp) && g_route_count < ND_MAX_ROUTE_POINTS) {
        if (sscanf(line, " %d %15s %d %lf %lf", &type, ident, &unused, &lat, &lon) == 5) {
            ND_MapPoint *point = &g_route[g_route_count++];
            memset(point, 0, sizeof(*point));
            copy_ident(point->ident, sizeof(point->ident), ident);
            copy_ident(point->name, sizeof(point->name), ident);
            point->latitude = lat;
            point->longitude = lon;
            point->type = ND_POINT_ROUTE;
        }
    }

    fclose(fp);
    return g_route_count > 0;
}

static int load_fix_file(void)
{
    FILE *fp = open_first_existing("assets/earth_fix.dat", "../assets/earth_fix.dat");
    char line[256];
    char ident[ND_IDENT_LEN];
    char type_text[16];
    char fir[16];
    char full_type[32];
    double lat;
    double lon;
    int loaded = 0;

    if (!fp) {
        return 0;
    }

    while (fgets(line, sizeof(line), fp)) {
        if (sscanf(line, " %lf %lf %15s %15s %15s %31s", &lat, &lon, ident, type_text, fir, full_type) >= 3) {
            ND_MapPoint point;
            memset(&point, 0, sizeof(point));
            copy_ident(point.ident, sizeof(point.ident), ident);
            copy_ident(point.name, sizeof(point.name), ident);
            point.latitude = lat;
            point.longitude = lon;
            point.type = ND_POINT_WAYPOINT;
            if (nav_db_push(&point)) {
                ++loaded;
            }
        }
    }

    fclose(fp);
    return loaded;
}

static int load_nav_file(void)
{
    FILE *fp = open_first_existing("assets/earth_nav.dat", "../assets/earth_nav.dat");
    char line[256];
    char ident[ND_IDENT_LEN];
    char region[16];
    char fir[16];
    int record_type;
    int elevation;
    int frequency;
    int range;
    double lat;
    double lon;
    double magnetic_variation;
    int loaded = 0;

    if (!fp) {
        return 0;
    }

    while (fgets(line, sizeof(line), fp)) {
        if (sscanf(line, " %d %lf %lf %d %d %d %lf %15s %15s %15s",
                   &record_type, &lat, &lon, &elevation, &frequency, &range,
                   &magnetic_variation, ident, region, fir) >= 8) {
            if (record_type == 2 || record_type == 3 || record_type == 12 || record_type == 13) {
                ND_MapPoint point;
                memset(&point, 0, sizeof(point));
                copy_ident(point.ident, sizeof(point.ident), ident);
                copy_ident(point.name, sizeof(point.name), ident);
                point.latitude = lat;
                point.longitude = lon;
                point.type = ND_POINT_NAVAID;
                if (nav_db_push(&point)) {
                    ++loaded;
                }
            }
        }
    }

    fclose(fp);
    return loaded;
}

static int load_airport_file(void)
{
    FILE *fp = open_first_existing("assets/apt.dat", "../assets/apt.dat");
    char line[256];
    char ident[ND_IDENT_LEN];
    double lat;
    double lon;
    int loaded = 0;

    if (!fp) {
        return 0;
    }

    while (fgets(line, sizeof(line), fp)) {
        if (sscanf(line, " %15s %lf %lf", ident, &lat, &lon) == 3) {
            ND_MapPoint point;
            memset(&point, 0, sizeof(point));
            copy_ident(point.ident, sizeof(point.ident), ident);
            copy_ident(point.name, sizeof(point.name), ident);
            point.latitude = lat;
            point.longitude = lon;
            point.type = ND_POINT_AIRPORT;
            if (nav_db_push(&point)) {
                ++loaded;
            }
        }
    }

    fclose(fp);
    return loaded;
}

int ND_Data_LoadStaticResources(void)
{
    if (g_nav_db.loaded) {
        return 1;
    }

    load_route_file();
    load_airport_file();
    load_nav_file();
    load_fix_file();
    g_nav_db.loaded = 1;
    return 1;
}

static void normalize_data(ND_Data *data)
{
    if (!data) {
        return;
    }

    data->ground_speed = clampf_local(data->ground_speed, 0.0f, 999.0f);
    data->true_air_speed = clampf_local(data->true_air_speed, 0.0f, 999.0f);
    data->heading = ND_Data_NormalizeHeading(data->heading);
    data->target_heading = ND_Data_NormalizeHeading(data->target_heading);
    data->track = ND_Data_NormalizeHeading(data->track);
    data->wind_speed = clampf_local(data->wind_speed, 0.0f, 250.0f);
    data->wind_direction = ND_Data_NormalizeHeading(data->wind_direction);
    if (data->latitude < -90.0) data->latitude = -90.0;
    if (data->latitude > 90.0) data->latitude = 90.0;
    if (data->longitude < -180.0) data->longitude = -180.0;
    if (data->longitude > 180.0) data->longitude = 180.0;
    if (data->data_source < ND_DATA_SOURCE_SIM || data->data_source > ND_DATA_SOURCE_XPLANE) {
        data->data_source = ND_DATA_SOURCE_SIM;
    }
    data->valid = 1;
}

static void copy_route_to_data(ND_Data *data)
{
    if (!data) {
        return;
    }

    data->route_count = g_route_count;
    if (data->route_count > ND_MAX_ROUTE_POINTS) {
        data->route_count = ND_MAX_ROUTE_POINTS;
    }

    for (int i = 0; i < data->route_count; ++i) {
        data->route[i] = g_route[i];
    }
}

static void fill_internal_frame(ND_Data *data)
{
    float t;

    if (!data) {
        return;
    }

    t = (float)g_frame / 60.0f;
    data->ground_speed = 260.0f + sinf(t * 0.35f) * 35.0f;
    data->true_air_speed = data->ground_speed + 6.0f + sinf(t * 0.22f) * 3.0f;
    data->heading = ND_Data_NormalizeHeading(t * 8.0f);
    data->target_heading = ND_Data_NormalizeHeading(data->heading + 25.0f);
    data->track = data->heading;
    data->latitude = 47.4489 + sin(t * 0.08) * 0.08;
    data->longitude = -122.3094 + cos(t * 0.08) * 0.14;
    data->wind_speed = 0.0f;
    data->wind_direction = 0.0f;
    data->data_source = ND_DATA_SOURCE_SIM;
    normalize_data(data);
    ++g_frame;
}

void ND_Data_Init(ND_Data *data)
{
    if (!data) {
        return;
    }

    memset(data, 0, sizeof(*data));
    data->latitude = 47.4489;
    data->longitude = -122.3094;
    data->ground_speed = 260.0f;
    data->true_air_speed = 260.0f;
    data->heading = 0.0f;
    data->target_heading = 25.0f;
    data->track = 0.0f;
    data->wind_speed = 0.0f;
    data->wind_direction = 0.0f;
    data->next_distance_nm = 1.0f;
    copy_ident(data->next_waypoint, sizeof(data->next_waypoint), "FREDY");
    data->data_source = ND_DATA_SOURCE_SIM;
    data->valid = 1;
    ND_Data_LoadStaticResources();
    copy_route_to_data(data);
    ND_Data_UpdateNavigation(data);
}

int ND_Data_LoadNextFrame(ND_Data *data)
{
    char line[256];
    float current_gs;
    float current_tas;
    float current_heading;
    float target_heading;

    if (!data) {
        return 0;
    }

    if (!g_file_checked) {
        g_data_file = open_data_file();
        g_file_checked = 1;
    }

    if (!g_data_file) {
        fill_internal_frame(data);
        ND_Data_UpdateNavigation(data);
        return 1;
    }

    if ((g_frame++ % 10) != 0) {
        return 1;
    }

    if (!fgets(line, sizeof(line), g_data_file)) {
        fseek(g_data_file, 0, SEEK_SET);
        if (!fgets(line, sizeof(line), g_data_file)) {
            fill_internal_frame(data);
            ND_Data_UpdateNavigation(data);
            return 1;
        }
    }

    if (sscanf(line, " %f , %f , %f , %f", &current_gs, &current_tas, &current_heading, &target_heading) == 4) {
        data->ground_speed = current_gs;
        data->true_air_speed = current_tas;
        data->heading = current_heading;
        data->target_heading = target_heading;
        data->track = current_heading;
        data->latitude = 47.4489 + sin((double)g_frame * 0.004) * 0.08;
        data->longitude = -122.3094 + cos((double)g_frame * 0.004) * 0.14;
        data->wind_speed = 0.0f;
        data->wind_direction = 0.0f;
        data->data_source = ND_DATA_SOURCE_FILE;
        normalize_data(data);
        ND_Data_UpdateNavigation(data);
        return 1;
    }

    fill_internal_frame(data);
    ND_Data_UpdateNavigation(data);
    return 1;
}

static void update_point_geometry(ND_Data *data, ND_MapPoint *point)
{
    if (!data || !point) {
        return;
    }
    point->distance_nm = ND_Data_DistanceNm(data->latitude, data->longitude, point->latitude, point->longitude);
    point->bearing_deg = ND_Data_BearingDeg(data->latitude, data->longitude, point->latitude, point->longitude);
    point->relative_angle_deg = ND_Data_AngleDelta(data->heading, point->bearing_deg);
}

static int point_is_better(const ND_MapPoint *candidate, const ND_MapPoint *stored)
{
    if (candidate->distance_nm < stored->distance_nm) {
        return 1;
    }
    if (fabsf(candidate->relative_angle_deg) < fabsf(stored->relative_angle_deg) - 5.0f) {
        return 1;
    }
    return 0;
}

static void add_nearby_point(ND_Data *data, const ND_MapPoint *point)
{
    int worst = 0;

    if (!data || !point) {
        return;
    }

    if (data->nearby_count < ND_MAX_NEARBY_POINTS) {
        data->nearby[data->nearby_count++] = *point;
        return;
    }

    for (int i = 1; i < data->nearby_count; ++i) {
        if (data->nearby[i].distance_nm > data->nearby[worst].distance_nm) {
            worst = i;
        }
    }

    if (point_is_better(point, &data->nearby[worst])) {
        data->nearby[worst] = *point;
    }
}

void ND_Data_UpdateNavigation(ND_Data *data)
{
    float best_next_distance = 9999.0f;
    int best_next = -1;
    int lat_center;
    int lon_center;
    int lat_span;
    int lon_span;
    double cos_lat;

    if (!data) {
        return;
    }

    ND_Data_LoadStaticResources();
    copy_route_to_data(data);

    for (int i = 0; i < data->route_count; ++i) {
        update_point_geometry(data, &data->route[i]);
        if (data->route[i].distance_nm > 0.1f && data->route[i].distance_nm < best_next_distance) {
            best_next_distance = data->route[i].distance_nm;
            best_next = i;
        }
    }

    if (best_next >= 0) {
        copy_ident(data->next_waypoint, sizeof(data->next_waypoint), data->route[best_next].ident);
        data->next_distance_nm = best_next_distance;
    } else {
        copy_ident(data->next_waypoint, sizeof(data->next_waypoint), "FREDY");
        data->next_distance_nm = 1.0f;
    }

    data->nearby_count = 0;
    if (g_nav_db.buckets && g_nav_db.next_indices) {
        lat_center = (int)floor(data->latitude) + 90;
        lon_center = (int)floor(data->longitude) + 180;
        lat_span = (int)ceil((double)ND_VISIBLE_RANGE_NM / 60.0) + 1;
        cos_lat = fabs(cos(deg_to_rad(data->latitude)));
        if (cos_lat < 0.2) {
            cos_lat = 0.2;
        }
        lon_span = (int)ceil((double)ND_VISIBLE_RANGE_NM / (60.0 * cos_lat)) + 1;

        for (int lat_cell = lat_center - lat_span; lat_cell <= lat_center + lat_span; ++lat_cell) {
            if (lat_cell < 0 || lat_cell >= ND_GRID_LAT_CELLS) {
                continue;
            }

            for (int lon_cell = lon_center - lon_span; lon_cell <= lon_center + lon_span; ++lon_cell) {
                int key = nav_grid_key_from_cell(lat_cell, lon_cell);
                for (int index = g_nav_db.buckets[key]; index >= 0; index = g_nav_db.next_indices[index]) {
                    ND_MapPoint point = g_nav_db.items[index];
                    update_point_geometry(data, &point);
                    if (point.distance_nm <= ND_VISIBLE_RANGE_NM &&
                        fabsf(point.relative_angle_deg) <= ND_VISIBLE_ANGLE_DEG) {
                        add_nearby_point(data, &point);
                    }
                }
            }
        }
    }

    normalize_data(data);
}

void ND_Data_Smooth(ND_Data *display, const ND_Data *target, float alpha)
{
    if (!display || !target) {
        return;
    }

    alpha = clampf_local(alpha, 0.0f, 1.0f);
    display->latitude += (target->latitude - display->latitude) * (double)alpha;
    display->longitude += (target->longitude - display->longitude) * (double)alpha;
    display->ground_speed = smooth_linear(display->ground_speed, target->ground_speed, alpha);
    display->true_air_speed = smooth_linear(display->true_air_speed, target->true_air_speed, alpha);
    display->heading = ND_Data_NormalizeHeading(display->heading + ND_Data_AngleDelta(display->heading, target->heading) * alpha);
    display->target_heading = ND_Data_NormalizeHeading(display->target_heading + ND_Data_AngleDelta(display->target_heading, target->target_heading) * alpha);
    display->track = ND_Data_NormalizeHeading(display->track + ND_Data_AngleDelta(display->track, target->track) * alpha);
    display->wind_speed = smooth_linear(display->wind_speed, target->wind_speed, alpha);
    display->wind_direction = ND_Data_NormalizeHeading(display->wind_direction + ND_Data_AngleDelta(display->wind_direction, target->wind_direction) * alpha);
    display->next_distance_nm = smooth_linear(display->next_distance_nm, target->next_distance_nm, alpha);
    copy_ident(display->next_waypoint, sizeof(display->next_waypoint), target->next_waypoint);
    display->route_count = target->route_count;
    display->nearby_count = target->nearby_count;
    for (int i = 0; i < target->route_count && i < ND_MAX_ROUTE_POINTS; ++i) {
        display->route[i] = target->route[i];
    }
    for (int i = 0; i < target->nearby_count && i < ND_MAX_NEARBY_POINTS; ++i) {
        display->nearby[i] = target->nearby[i];
    }
    display->valid = target->valid;
    display->data_source = target->data_source;
    normalize_data(display);
}

void ND_Data_Close(void)
{
    if (g_data_file) {
        fclose(g_data_file);
        g_data_file = NULL;
    }
    g_file_checked = 0;
    if (g_nav_db.items) {
        free(g_nav_db.items);
        g_nav_db.items = NULL;
    }
    if (g_nav_db.next_indices) {
        free(g_nav_db.next_indices);
        g_nav_db.next_indices = NULL;
    }
    if (g_nav_db.buckets) {
        free(g_nav_db.buckets);
        g_nav_db.buckets = NULL;
    }
    g_nav_db.count = 0;
    g_nav_db.capacity = 0;
    g_nav_db.loaded = 0;
    g_route_count = 0;
}
