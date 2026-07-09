#ifndef EICAS2_DATA_H
#define EICAS2_DATA_H

#define EICAS2_DATA_SOURCE_SIM 0
#define EICAS2_DATA_SOURCE_FILE 1
#define EICAS2_DATA_SOURCE_XPLANE 2

typedef struct
{
    float n2_left;
    float n2_right;
    float ff_left;
    float ff_right;
    float oil_press_left;
    float oil_press_right;
    float oil_temp_left;
    float oil_temp_right;
    float oil_qty_left;
    float oil_qty_right;
    float vib_left;
    float vib_right;
    int valid;
    int data_source;
} EICAS2_Data;

void EICAS2_Data_Init(EICAS2_Data *data);
int EICAS2_Data_LoadNextFrame(EICAS2_Data *data);
void EICAS2_Data_Smooth(EICAS2_Data *display, const EICAS2_Data *target, float alpha);
void EICAS2_Data_Close(void);

#ifdef ENABLE_XPLANE
int EICAS2_XPlane_Open(void);
int EICAS2_XPlane_FetchData(EICAS2_Data *data);
void EICAS2_XPlane_Close(void);
#endif

#endif
