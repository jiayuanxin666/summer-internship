#ifndef ND_DATA_H
#define ND_DATA_H

#define ND_DATA_SOURCE_SIM 0
#define ND_DATA_SOURCE_FILE 1
#define ND_DATA_SOURCE_XPLANE 2

#define ND_MAX_ROUTE_POINTS 64
#define ND_MAX_NEARBY_POINTS 96
#define ND_PREDICTION_POINTS 3
#define ND_IDENT_LEN 16
#define ND_NAME_LEN 48

typedef enum
{
    ND_POINT_WAYPOINT = 0,
    ND_POINT_AIRPORT = 1,
    ND_POINT_NAVAID = 2,
    ND_POINT_ROUTE = 3
} ND_PointType;

typedef struct
{
    char ident[ND_IDENT_LEN];
    char name[ND_NAME_LEN];
    double latitude;
    double longitude;
    float distance_nm;
    float bearing_deg;
    float relative_angle_deg;
    ND_PointType type;
} ND_MapPoint;

typedef struct
{
    double latitude;
    double longitude;
    float ground_speed;
    float true_air_speed;
    float heading;
    float target_heading;
    float track;
    float wind_speed;
    float wind_direction;

    char next_waypoint[ND_IDENT_LEN];
    float next_distance_nm;

    int route_count;
    ND_MapPoint route[ND_MAX_ROUTE_POINTS];
    int route_from_fmc;
    unsigned int route_revision;

    int nearby_count;
    ND_MapPoint nearby[ND_MAX_NEARBY_POINTS];

    int prediction_count;
    ND_MapPoint prediction[ND_PREDICTION_POINTS];
    float cross_track_error_nm;
    int cross_track_valid;
    int route_deviation_level;

    int valid;
    int data_source;
} ND_Data;

void ND_Data_Init(ND_Data *data);
int ND_Data_LoadStaticResources(void);
int ND_Data_LoadNextFrame(ND_Data *data);
void ND_Data_UpdateNavigation(ND_Data *data);
void ND_Data_Smooth(ND_Data *display, const ND_Data *target, float alpha);
void ND_Data_Close(void);

float ND_Data_NormalizeHeading(float heading);
float ND_Data_AngleDelta(float from_deg, float to_deg);
float ND_Data_DistanceNm(double lat1, double lon1, double lat2, double lon2);
float ND_Data_BearingDeg(double lat1, double lon1, double lat2, double lon2);
int ND_Data_RunSelfTest(void);

#endif
