#include "route_ipc.h"
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#define ROUTE_IPC_MAGIC 0x46524F55u
struct RouteIPC { HANDLE mapping; HANDLE mutex; RouteIPCData *view; };

RouteIPC *RouteIPC_Open(int create)
{
    RouteIPC *ipc = calloc(1, sizeof(*ipc));
    if (!ipc) return NULL;
    ipc->mapping = create ? CreateFileMappingA(INVALID_HANDLE_VALUE,NULL,PAGE_READWRITE,0,sizeof(RouteIPCData),"Local\\FlightCockpitRouteV1") : OpenFileMappingA(FILE_MAP_ALL_ACCESS,FALSE,"Local\\FlightCockpitRouteV1");
    ipc->mutex = CreateMutexA(NULL,FALSE,"Local\\FlightCockpitRouteMutexV1");
    if (ipc->mapping) ipc->view = MapViewOfFile(ipc->mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(RouteIPCData));
    if (!ipc->view || !ipc->mutex) { RouteIPC_Close(ipc); return NULL; }
    if (create && ipc->view->magic != ROUTE_IPC_MAGIC) { memset(ipc->view,0,sizeof(*ipc->view)); ipc->view->magic=ROUTE_IPC_MAGIC; ipc->view->version=1; }
    return ipc;
}

int RouteIPC_Write(RouteIPC *ipc, const RouteIPCData *data)
{
    unsigned int next;
    if(!ipc||!data||WaitForSingleObject(ipc->mutex,100)!=WAIT_OBJECT_0)return 0;
    next=ipc->view->sequence+1;*ipc->view=*data;ipc->view->magic=ROUTE_IPC_MAGIC;ipc->view->version=1;ipc->view->sequence=next;ReleaseMutex(ipc->mutex);return 1;
}

int RouteIPC_Read(RouteIPC *ipc, RouteIPCData *data, unsigned int *last_sequence)
{
    int changed=0;if(!ipc||!data||WaitForSingleObject(ipc->mutex,10)!=WAIT_OBJECT_0)return 0;
    if(ipc->view->magic==ROUTE_IPC_MAGIC&&ipc->view->version==1&&ipc->view->point_count>=0&&ipc->view->point_count<=ROUTE_IPC_CAPACITY&&(!last_sequence||ipc->view->sequence!=*last_sequence)){*data=*ipc->view;if(last_sequence)*last_sequence=data->sequence;changed=1;}ReleaseMutex(ipc->mutex);return changed;
}

void RouteIPC_Close(RouteIPC *ipc){if(!ipc)return;if(ipc->view)UnmapViewOfFile(ipc->view);if(ipc->mapping)CloseHandle(ipc->mapping);if(ipc->mutex)CloseHandle(ipc->mutex);free(ipc);}
#else
struct RouteIPC { RouteIPCData data; };
RouteIPC *RouteIPC_Open(int create){(void)create;return calloc(1,sizeof(RouteIPC));}
int RouteIPC_Write(RouteIPC *ipc,const RouteIPCData *data){if(!ipc||!data)return 0;ipc->data=*data;ipc->data.sequence++;return 1;}
int RouteIPC_Read(RouteIPC *ipc,RouteIPCData *data,unsigned int *seq){if(!ipc||!data||!ipc->data.sequence||seq&&*seq==ipc->data.sequence)return 0;*data=ipc->data;if(seq)*seq=data->sequence;return 1;}
void RouteIPC_Close(RouteIPC *ipc){free(ipc);}
#endif

int RouteIPC_SelfTest(void)
{
    RouteIPC *writer=RouteIPC_Open(1),*reader=RouteIPC_Open(0);RouteIPCData out,in;unsigned int seq=0;int ok=0;
    memset(&out,0,sizeof(out));out.point_count=1;strcpy(out.points[0].ident,"TEST");out.points[0].latitude=47.5;out.points[0].longitude=-122.3;
    if(writer&&reader&&RouteIPC_Write(writer,&out)&&RouteIPC_Read(reader,&in,&seq))ok=in.point_count==1&&!strcmp(in.points[0].ident,"TEST")&&seq>0;
    RouteIPC_Close(reader);RouteIPC_Close(writer);return ok;
}
