#ifndef MAP_DATA_H
#define MAP_DATA_H

#include <stddef.h>

#define MAP_TRACK_CAPACITY 40

typedef struct { double longitude, latitude; } MAP_Point;

typedef struct {
    char city[64], district[64], township[96], adcode[16];
    char weather[32], temperature[16], humidity[16], windpower[16], winddirection[24];
    MAP_Point track[MAP_TRACK_CAPACITY];
    int track_count;
    double longitude, latitude, bearing;
    int zoom;
    unsigned long revision;
    char status[128];
} MAP_Data;

typedef struct { unsigned char *bytes; size_t size; long http_status; } MAP_HTTP_Response;

void MAP_Data_Init(MAP_Data *data);
void MAP_Data_AddPoint(MAP_Data *data, double longitude, double latitude);
double MAP_Data_Bearing(const MAP_Point *from, const MAP_Point *to);
int MAP_HTTP_Get(const char *url, MAP_HTTP_Response *response);
void MAP_HTTP_Free(MAP_HTTP_Response *response);
int MAP_Data_FetchLocationWeather(MAP_Data *data, const char *api_key);
int MAP_Data_FetchStaticMap(const MAP_Data *data, const char *api_key,
                            int width, int height, MAP_HTTP_Response *response);
int MAP_Data_FetchXPlane(MAP_Data *data);
void MAP_Data_CloseXPlane(void);
int MAP_Data_SelfTest(void);

#endif
