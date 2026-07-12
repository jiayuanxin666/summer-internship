#ifndef EICAS1_DATA_H
#define EICAS1_DATA_H

#define EICAS1_DATA_SOURCE_SIM 0
#define EICAS1_DATA_SOURCE_FILE 1
#define EICAS1_DATA_SOURCE_XPLANE 2

#define EICAS1_ALERT_N1_HIGH  0x01u
#define EICAS1_ALERT_EGT_HIGH 0x02u
#define EICAS1_ALERT_FF_LOW   0x04u
#define EICAS1_ALERT_FUEL_LOW 0x08u

typedef struct
{
    float tat;
    float n1_left;
    float n1_right;
    float egt_left;
    float egt_right;
    float ff_left;
    float ff_right;
    float fuel_center;
    float fuel_left;
    float fuel_right;
    int valid;
    int data_source;
    unsigned int alert_left;
    unsigned int alert_right;
} EICAS1_Data;

void EICAS1_Data_Init(EICAS1_Data *data);
int EICAS1_Data_LoadNextFrame(EICAS1_Data *data);
void EICAS1_Data_Smooth(EICAS1_Data *display, const EICAS1_Data *target, float alpha);
void EICAS1_Data_Close(void);

#ifdef ENABLE_XPLANE
int EICAS1_XPlane_Open(void);
int EICAS1_XPlane_FetchData(EICAS1_Data *data);
void EICAS1_XPlane_Close(void);
#endif

#endif
