#ifndef FMC_CONNECT_H
#define FMC_CONNECT_H

#include "fmc_key.h"

typedef struct
{
    int available;
#ifdef ENABLE_XPLANE
    struct FMC_XPlaneConnection *connection;
#endif
} FMC_Connect;

int FMC_Connect_Init(FMC_Connect *connect, const char *host);
int FMC_Connect_SendCommand(FMC_Connect *connect, const char *command);
int FMC_Connect_SendKey(FMC_Connect *connect, FMC_Key key);
int FMC_Connect_SyncRoute(FMC_Connect *connect,
                          const char *origin,
                          const char *destination,
                          const char *flight_number);
void FMC_Connect_Close(FMC_Connect *connect);

#endif
