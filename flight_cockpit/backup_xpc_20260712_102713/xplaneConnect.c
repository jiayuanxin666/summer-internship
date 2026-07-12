#include "xplaneConnect.h"
#include <string.h>

#define XPC_DEFAULT_PORT 49009
#define XPC_VALUE_COUNT 8
#define XPC_MAX_DREFS 64
#define XPC_PACKET_SIZE 65536

static int g_wsa_started;
static int g_open_socket_count;
static XPCError g_last_error;

static void set_error(XPCError error) { g_last_error = error; }
static void cleanup_winsock(void) {
    if (g_wsa_started && g_open_socket_count == 0) { WSACleanup(); g_wsa_started = 0; }
}
static int ensure_winsock(void) {
    WSADATA data;
    if (g_wsa_started) return 1;
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) { set_error(XPC_ERROR_WSA_STARTUP); return 0; }
    g_wsa_started = 1;
    return 1;
}
static void zero_socket(XPCSocket *sock) {
    memset(sock, 0, sizeof(*sock)); sock->socket_fd = INVALID_SOCKET;
}

XPCSocket openUDP(const char *ip) { return aopenUDP(ip, XPC_DEFAULT_PORT, 0); }

XPCSocket aopenUDP(const char *ip, unsigned short xp_port, unsigned short local_port) {
    XPCSocket sock;
    struct sockaddr_in local;
    int timeout_ms = 35;
    zero_socket(&sock); set_error(XPC_ERROR_NONE);
    if (!ip) { set_error(XPC_ERROR_INVALID_IP); return sock; }
    if (!ensure_winsock()) return sock;
    sock.socket_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock.socket_fd == INVALID_SOCKET) { set_error(XPC_ERROR_SOCKET_CREATE); cleanup_winsock(); return sock; }
    memset(&local, 0, sizeof(local)); local.sin_family = AF_INET;
    local.sin_addr.s_addr = htonl(INADDR_ANY); local.sin_port = htons(local_port);
    if (bind(sock.socket_fd, (struct sockaddr *)&local, sizeof(local)) == SOCKET_ERROR) {
        set_error(XPC_ERROR_BIND); closesocket(sock.socket_fd); zero_socket(&sock); cleanup_winsock(); return sock;
    }
    if (setsockopt(sock.socket_fd, SOL_SOCKET, SO_RCVTIMEO,
                   (const char *)&timeout_ms, sizeof(timeout_ms)) == SOCKET_ERROR) {
        set_error(XPC_ERROR_RECEIVE); closesocket(sock.socket_fd); zero_socket(&sock); cleanup_winsock(); return sock;
    }
    memset(&sock.xplane_addr, 0, sizeof(sock.xplane_addr));
    sock.xplane_addr.sin_family = AF_INET; sock.xplane_addr.sin_port = htons(xp_port);
    if (inet_pton(AF_INET, ip, &sock.xplane_addr.sin_addr) != 1) {
        set_error(XPC_ERROR_INVALID_IP); closesocket(sock.socket_fd); zero_socket(&sock); cleanup_winsock(); return sock;
    }
    sock.is_open = 1; ++g_open_socket_count; return sock;
}

int getDREFs(XPCSocket sock, const char **drefs, int count, float values[][8]) {
    unsigned char request[XPC_PACKET_SIZE] = {0};
    unsigned char response[XPC_PACKET_SIZE] = {0};
    struct sockaddr_in source;
    int source_len = sizeof(source);
    size_t pos = 6;
    int received;
    set_error(XPC_ERROR_NONE);
    if (!XPCSocket_IsOpen(sock) || !drefs || !values || count <= 0 || count > XPC_MAX_DREFS) {
        set_error(XPC_ERROR_INVALID_ARGUMENT); return 0;
    }
    memcpy(request, "GETD", 4); request[4] = 0; request[5] = (unsigned char)count;
    for (int i = 0; i < count; ++i) {
        size_t len;
        if (!drefs[i]) { set_error(XPC_ERROR_INVALID_ARGUMENT); return 0; }
        len = strlen(drefs[i]);
        if (len == 0 || len > 255 || pos + len + 1 > sizeof(request)) {
            set_error(XPC_ERROR_INVALID_ARGUMENT); return 0;
        }
        request[pos++] = (unsigned char)len; memcpy(request + pos, drefs[i], len); pos += len;
        memset(values[i], 0, XPC_VALUE_COUNT * sizeof(float));
    }
    if (sendto(sock.socket_fd, (const char *)request, (int)pos, 0,
               (struct sockaddr *)&sock.xplane_addr, sizeof(sock.xplane_addr)) == SOCKET_ERROR) {
        set_error(XPC_ERROR_SEND); return 0;
    }
    memset(&source, 0, sizeof(source));
    received = recvfrom(sock.socket_fd, (char *)response, sizeof(response), 0,
                        (struct sockaddr *)&source, &source_len);
    if (received == SOCKET_ERROR) {
        int error = WSAGetLastError();
        set_error((error == WSAETIMEDOUT || error == WSAEWOULDBLOCK) ?
                  XPC_ERROR_TIMEOUT : XPC_ERROR_RECEIVE); return 0;
    }
    if (source.sin_addr.s_addr != sock.xplane_addr.sin_addr.s_addr ||
        source.sin_port != sock.xplane_addr.sin_port) { set_error(XPC_ERROR_SOURCE); return 0; }
    if (received < 6) { set_error(XPC_ERROR_RESPONSE_LENGTH); return 0; }
    /* NASA's protocol specification defines the GETD response header as GETD. */
    if (memcmp(response, "GETD", 4) != 0) { set_error(XPC_ERROR_RESPONSE_HEADER); return 0; }
    if (response[5] != (unsigned char)count) { set_error(XPC_ERROR_DREF_COUNT); return 0; }
    pos = 6;
    for (int i = 0; i < count; ++i) {
        unsigned int n;
        size_t bytes, copy_count;
        if (pos >= (size_t)received) { set_error(XPC_ERROR_RESPONSE_LENGTH); return 0; }
        n = response[pos++]; bytes = (size_t)n * sizeof(float);
        if (bytes > (size_t)received - pos) { set_error(XPC_ERROR_RESPONSE_LENGTH); return 0; }
        copy_count = n > XPC_VALUE_COUNT ? XPC_VALUE_COUNT : n;
        memcpy(values[i], response + pos, copy_count * sizeof(float)); pos += bytes;
    }
    return 1;
}

void closeUDP(XPCSocket sock) {
    if (sock.socket_fd != INVALID_SOCKET) { closesocket(sock.socket_fd); if (g_open_socket_count > 0) --g_open_socket_count; }
    cleanup_winsock();
}
int XPCSocket_IsOpen(XPCSocket sock) { return sock.is_open && sock.socket_fd != INVALID_SOCKET; }
XPCError XPC_GetLastError(void) { return g_last_error; }
const char *XPC_GetLastErrorString(void) {
    static const char *messages[] = { "no error", "Winsock startup failed", "socket creation failed",
        "bind failed", "IP address is invalid", "invalid argument", "sendto failed",
        "recvfrom timed out", "recvfrom failed", "response source mismatch",
        "response header is invalid", "response length is invalid", "DataRef count mismatch" };
    unsigned int error = (unsigned int)g_last_error;
    return error < sizeof(messages) / sizeof(messages[0]) ? messages[error] : "unknown error";
}
