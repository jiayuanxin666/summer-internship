#ifndef PFD_DATA_H
#define PFD_DATA_H

#define PFD_DATA_SOURCE_SIM 0
#define PFD_DATA_SOURCE_FILE 1
#define PFD_DATA_SOURCE_XPLANE 2

typedef struct
{
    float pitch;
    float roll;
    float yaw;

    float altitude;
    float agl_altitude;
    float altitude_target;

    float throttle;

    float airspeed_current;
    float airspeed_target;

    float vertical_speed;

    float heading;
    float heading_target;

    int data_source;
} PFD_Data;

void PFD_Data_Init(PFD_Data *data);
int PFD_Data_LoadNextFrame(PFD_Data *data);
void PFD_Data_Smooth(PFD_Data *display, const PFD_Data *target, float alpha);
void PFD_Data_Close(void);

#endif
