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

XPCSocket openUDP(const char *xpIP);
XPCSocket aopenUDP(const char *xpIP, unsigned short xpPort, unsigned short port);
int getDREFs(XPCSocket sock, const char **drefs, int count, float values[][8]);
void closeUDP(XPCSocket sock);
int XPCSocket_IsOpen(XPCSocket sock);

#ifdef __cplusplus
}
#endif

#endif
