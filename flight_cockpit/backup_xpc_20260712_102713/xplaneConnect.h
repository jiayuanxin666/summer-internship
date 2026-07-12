#ifndef XPLANE_CONNECT_H
#define XPLANE_CONNECT_H

#include <winsock2.h>
#include <ws2tcpip.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    SOCKET socket_fd;
    struct sockaddr_in xplane_addr;
    int is_open;
} XPCSocket;

typedef enum { XPC_ERROR_NONE = 0, XPC_ERROR_WSA_STARTUP, XPC_ERROR_SOCKET_CREATE,
    XPC_ERROR_BIND, XPC_ERROR_INVALID_IP, XPC_ERROR_INVALID_ARGUMENT,
    XPC_ERROR_SEND, XPC_ERROR_TIMEOUT, XPC_ERROR_RECEIVE, XPC_ERROR_SOURCE,
    XPC_ERROR_RESPONSE_HEADER, XPC_ERROR_RESPONSE_LENGTH, XPC_ERROR_DREF_COUNT } XPCError;

XPCSocket openUDP(const char *xpIP);
XPCSocket aopenUDP(const char *xpIP, unsigned short xpPort, unsigned short port);
int getDREFs(XPCSocket sock, const char **drefs, int count, float values[][8]);
void closeUDP(XPCSocket sock);
int XPCSocket_IsOpen(XPCSocket sock);
XPCError XPC_GetLastError(void);
const char *XPC_GetLastErrorString(void);

#ifdef __cplusplus
}
#endif

#endif
