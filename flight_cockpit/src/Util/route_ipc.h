#ifndef ROUTE_IPC_H
#define ROUTE_IPC_H

#define ROUTE_IPC_CAPACITY 66
#define ROUTE_IPC_IDENT_LEN 16

typedef struct {
    char ident[ROUTE_IPC_IDENT_LEN];
    double latitude;
    double longitude;
} RouteIPCPoint;

typedef struct {
    unsigned int magic;
    unsigned int version;
    unsigned int sequence;
    int point_count;
    RouteIPCPoint points[ROUTE_IPC_CAPACITY];
} RouteIPCData;

typedef struct RouteIPC RouteIPC;
RouteIPC *RouteIPC_Open(int create);
int RouteIPC_Write(RouteIPC *ipc, const RouteIPCData *data);
int RouteIPC_Read(RouteIPC *ipc, RouteIPCData *data, unsigned int *last_sequence);
void RouteIPC_Close(RouteIPC *ipc);
int RouteIPC_SelfTest(void);

#endif
