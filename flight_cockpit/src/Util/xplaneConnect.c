#include "xplaneConnect.h"
#include <stdio.h>
#include <string.h>

#define XPC_DEFAULT_PORT 49009
#define XPC_DREF_NAME_SIZE 500
#define XPC_DREF_VALUE_COUNT 8

static int g_wsa_started = 0;

static int ensure_winsock(void)
{
    WSADATA wsa_data;

    if (g_wsa_started) {
        return 1;
    }

    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        return 0;
    }

    g_wsa_started = 1;
    return 1;
}

static void zero_socket(XPCSocket *sock)
{
    if (!sock) {
        return;
    }

    memset(sock, 0, sizeof(*sock));
    sock->socket_fd = INVALID_SOCKET;
    sock->is_open = 0;
}

XPCSocket openUDP(const char *xpIP)
{
    return aopenUDP(xpIP, XPC_DEFAULT_PORT, 0);
}

XPCSocket aopenUDP(const char *xpIP, unsigned short xpPort, unsigned short port)
{
    XPCSocket sock;
    struct sockaddr_in local_addr;
    int timeout_ms = 35;

    zero_socket(&sock);

    if (!xpIP || !ensure_winsock()) {
        return sock;
    }

    sock.socket_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock.socket_fd == INVALID_SOCKET) {
        return sock;
    }

    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    local_addr.sin_port = htons(port);

    if (bind(sock.socket_fd, (struct sockaddr *)&local_addr, sizeof(local_addr)) == SOCKET_ERROR) {
        closesocket(sock.socket_fd);
        zero_socket(&sock);
        return sock;
    }

    setsockopt(sock.socket_fd, SOL_SOCKET, SO_RCVTIMEO,
               (const char *)&timeout_ms, sizeof(timeout_ms));

    memset(&sock.xplane_addr, 0, sizeof(sock.xplane_addr));
    sock.xplane_addr.sin_family = AF_INET;
    sock.xplane_addr.sin_port = htons(xpPort);
    if (inet_pton(AF_INET, xpIP, &sock.xplane_addr.sin_addr) != 1) {
        closesocket(sock.socket_fd);
        zero_socket(&sock);
        return sock;
    }

    sock.is_open = 1;
    return sock;
}

static int get_one_dref(XPCSocket sock, const char *dref, float values[8])
{
    char request[5 + XPC_DREF_NAME_SIZE];
    char response[5 + XPC_DREF_VALUE_COUNT * sizeof(float)];
    int sent;
    int received;
    int addr_len = sizeof(sock.xplane_addr);

    if (!XPCSocket_IsOpen(sock) || !dref || !values) {
        return 0;
    }

    memset(values, 0, XPC_DREF_VALUE_COUNT * sizeof(float));
    memset(request, 0, sizeof(request));
    memcpy(request, "GETD", 4);
    strncpy(request + 5, dref, XPC_DREF_NAME_SIZE - 1);

    sent = sendto(sock.socket_fd, request, sizeof(request), 0,
                  (struct sockaddr *)&sock.xplane_addr, sizeof(sock.xplane_addr));
    if (sent == SOCKET_ERROR) {
        return 0;
    }

    received = recvfrom(sock.socket_fd, response, sizeof(response), 0,
                        (struct sockaddr *)&sock.xplane_addr, &addr_len);
    if (received < 5 + (int)sizeof(float)) {
        return 0;
    }

    memset(values, 0, XPC_DREF_VALUE_COUNT * sizeof(float));
    memcpy(values, response + 5, (size_t)(received - 5));
    return 1;
}

int getDREFs(XPCSocket sock, const char **drefs, int count, float values[][8])
{
    if (!XPCSocket_IsOpen(sock) || !drefs || !values || count <= 0) {
        return 0;
    }

    for (int i = 0; i < count; ++i) {
        if (!get_one_dref(sock, drefs[i], values[i])) {
            return 0;
        }
    }

    return 1;
}

void closeUDP(XPCSocket sock)
{
    if (sock.socket_fd != INVALID_SOCKET) {
        closesocket(sock.socket_fd);
    }
}

int XPCSocket_IsOpen(XPCSocket sock)
{
    return sock.is_open && sock.socket_fd != INVALID_SOCKET;
}
