#include "xplaneConnect.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define XPC_DEFAULT_PORT 49009
#define XPC_VALUE_COUNT 8
#define XPC_MAX_DREFS 64
#define XPC_PACKET_SIZE 65507

static int g_wsa_started;
static int g_open_socket_count;
static int g_trace_next_rx;
static XPCError g_last_error;

static void set_error(XPCError error) { g_last_error = error; }
static void cleanup_winsock(void) {
    if (g_wsa_started && g_open_socket_count == 0) {
        WSACleanup();
        g_wsa_started = 0;
    }
}
static int ensure_winsock(void) {
    WSADATA data;
    if (g_wsa_started) return 1;
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
        set_error(XPC_ERROR_WSA_STARTUP);
        return 0;
    }
    g_wsa_started = 1;
    return 1;
}
static void zero_socket(XPCSocket *sock) {
    memset(sock, 0, sizeof(*sock));
    sock->socket_fd = INVALID_SOCKET;
}
static void print_rx_trace(const unsigned char *response, int received) {
    int shown = received < 12 ? received : 12;
    printf("XPC RX: len=%d\nHEX:", received);
    for (int i = 0; i < shown; ++i) printf(" %02X", response[i]);
    printf("\nASCII header: '");
    for (int i = 0; i < 4 && i < received; ++i)
        putchar(isprint((unsigned char)response[i]) ? response[i] : '.');
    printf("'\n");
    if (received > 4) printf("padding=%u\n", (unsigned)response[4]);
    if (received > 5) printf("count=%u\n", (unsigned)response[5]);
    if (received > 6) printf("first_value_count=%u\n", (unsigned)response[6]);
}

XPCSocket openUDP(const char *ip) { return aopenUDP(ip, XPC_DEFAULT_PORT, 0); }

XPCSocket aopenUDP(const char *ip, unsigned short xp_port, unsigned short local_port) {
    XPCSocket sock;
    struct sockaddr_in local;
    int timeout_ms = 35;
    zero_socket(&sock);
    set_error(XPC_ERROR_NONE);
    if (!ip) { set_error(XPC_ERROR_INVALID_IP); return sock; }
    if (!ensure_winsock()) return sock;
    sock.socket_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock.socket_fd == INVALID_SOCKET) {
        set_error(XPC_ERROR_SOCKET_CREATE); cleanup_winsock(); return sock;
    }
    memset(&local, 0, sizeof(local));
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = htonl(INADDR_ANY);
    local.sin_port = htons(local_port);
    if (bind(sock.socket_fd, (struct sockaddr *)&local, sizeof(local)) == SOCKET_ERROR) {
        set_error(XPC_ERROR_BIND); closesocket(sock.socket_fd); zero_socket(&sock);
        cleanup_winsock(); return sock;
    }
    if (setsockopt(sock.socket_fd, SOL_SOCKET, SO_RCVTIMEO,
                   (const char *)&timeout_ms, sizeof(timeout_ms)) == SOCKET_ERROR) {
        set_error(XPC_ERROR_RECEIVE); closesocket(sock.socket_fd); zero_socket(&sock);
        cleanup_winsock(); return sock;
    }
    memset(&sock.xplane_addr, 0, sizeof(sock.xplane_addr));
    sock.xplane_addr.sin_family = AF_INET;
    sock.xplane_addr.sin_port = htons(xp_port);
    if (inet_pton(AF_INET, ip, &sock.xplane_addr.sin_addr) != 1) {
        set_error(XPC_ERROR_INVALID_IP); closesocket(sock.socket_fd); zero_socket(&sock);
        cleanup_winsock(); return sock;
    }
    sock.is_open = 1;
    ++g_open_socket_count;
    return sock;
}

int getDREFsSized(XPCSocket sock, const char **drefs, const int *capacities,
                  int count, float **values) {
    unsigned char request[XPC_PACKET_SIZE] = {0};
    unsigned char response[XPC_PACKET_SIZE];
    struct sockaddr_in response_addr;
    int response_addr_len = sizeof(response_addr);
    size_t cursor = 6;
    int received;

    set_error(XPC_ERROR_NONE);
    if (!XPCSocket_IsOpen(sock) || !drefs || !capacities || !values ||
        count <= 0 || count > XPC_MAX_DREFS) {
        set_error(XPC_ERROR_INVALID_ARGUMENT); return 0;
    }
    memcpy(request, "GETD", 4);
    request[4] = 0;
    request[5] = (unsigned char)count;
    for (int i = 0; i < count; ++i) {
        size_t len;
        if (!drefs[i] || !values[i] || capacities[i] <= 0) {
            set_error(XPC_ERROR_INVALID_ARGUMENT); return 0;
        }
        len = strlen(drefs[i]);
        if (len == 0 || len > 255 || len + 1 > sizeof(request) - cursor) {
            set_error(XPC_ERROR_INVALID_ARGUMENT); return 0;
        }
        request[cursor++] = (unsigned char)len;
        memcpy(request + cursor, drefs[i], len);
        cursor += len;
        memset(values[i], 0, (size_t)capacities[i] * sizeof(float));
    }
    if (sendto(sock.socket_fd, (const char *)request, (int)cursor, 0,
               (const struct sockaddr *)&sock.xplane_addr, sizeof(sock.xplane_addr)) == SOCKET_ERROR) {
        set_error(XPC_ERROR_SEND); return 0;
    }
    memset(&response_addr, 0, sizeof(response_addr));
    received = recvfrom(sock.socket_fd, (char *)response, sizeof(response), 0,
                        (struct sockaddr *)&response_addr, &response_addr_len);
    if (received == SOCKET_ERROR) {
        int error = WSAGetLastError();
        if (error == WSAETIMEDOUT || error == WSAEWOULDBLOCK) {
            set_error(XPC_ERROR_TIMEOUT);
        } else if (error == WSAECONNRESET) {
            set_error(XPC_ERROR_RECEIVE);
        } else {
            fprintf(stderr, "XPC recvfrom failed, WSA error: %d.\n", error);
            set_error(XPC_ERROR_RECEIVE);
        }
        return 0;
    }
    if (g_trace_next_rx) { print_rx_trace(response, received); g_trace_next_rx = 0; }
    if (response_addr.sin_addr.s_addr != sock.xplane_addr.sin_addr.s_addr ||
        response_addr.sin_port != sock.xplane_addr.sin_port) {
        fprintf(stderr, "XPC response source mismatch: %s:%u.\n", inet_ntoa(response_addr.sin_addr),
                (unsigned)ntohs(response_addr.sin_port));
        set_error(XPC_ERROR_SOURCE); return 0;
    }
    if (received < 6) {
        fprintf(stderr, "XPC response too short: received=%d, required=6.\n", received);
        set_error(XPC_ERROR_RESPONSE_LENGTH); return 0;
    }
    if (memcmp(response, "GETD", 4) != 0 && memcmp(response, "RESP", 4) != 0) {
        fprintf(stderr, "XPC invalid response header: hex=%02X %02X %02X %02X, ascii='%c%c%c%c'.\n",
                response[0], response[1], response[2], response[3],
                isprint(response[0]) ? response[0] : '.', isprint(response[1]) ? response[1] : '.',
                isprint(response[2]) ? response[2] : '.', isprint(response[3]) ? response[3] : '.');
        set_error(XPC_ERROR_RESPONSE_HEADER); return 0;
    }
    if (response[5] != (unsigned char)count) {
        fprintf(stderr, "XPC result count mismatch: expected=%d, received=%u.\n",
                count, (unsigned)response[5]);
        set_error(XPC_ERROR_DREF_COUNT); return 0;
    }
    cursor = 6;
    for (int i = 0; i < count; ++i) {
        unsigned int value_count;
        size_t value_bytes, copy_count;
        if (cursor >= (size_t)received) {
            fprintf(stderr, "XPC response too short: received=%d, required=%zu.\n", received, cursor + 1);
            set_error(XPC_ERROR_RESPONSE_LENGTH); return 0;
        }
        value_count = response[cursor++];
        value_bytes = (size_t)value_count * sizeof(float);
        if (value_bytes > (size_t)received - cursor) {
            fprintf(stderr, "XPC response too short: received=%d, required=%zu.\n",
                    received, cursor + value_bytes);
            set_error(XPC_ERROR_RESPONSE_LENGTH); return 0;
        }
        copy_count = value_count > (unsigned int)capacities[i]
                         ? (size_t)capacities[i] : (size_t)value_count;
        memcpy(values[i], response + cursor, copy_count * sizeof(float));
        cursor += value_bytes;
    }
    return 1;
}

int getDREFs(XPCSocket sock, const char **drefs, int count, float values[][8]) {
    int capacities[XPC_MAX_DREFS];
    float *outputs[XPC_MAX_DREFS];
    if (!values || count <= 0 || count > XPC_MAX_DREFS) {
        set_error(XPC_ERROR_INVALID_ARGUMENT);
        return 0;
    }
    for (int i = 0; i < count; ++i) {
        capacities[i] = XPC_VALUE_COUNT;
        outputs[i] = values[i];
    }
    return getDREFsSized(sock, drefs, capacities, count, outputs);
}

void closeUDP(XPCSocket sock) {
    if (sock.socket_fd != INVALID_SOCKET) {
        closesocket(sock.socket_fd);
        if (g_open_socket_count > 0) --g_open_socket_count;
    }
    cleanup_winsock();
}
int XPCSocket_IsOpen(XPCSocket sock) { return sock.is_open && sock.socket_fd != INVALID_SOCKET; }
XPCError XPC_GetLastError(void) { return g_last_error; }
void XPC_EnableRxTraceOnce(void) { g_trace_next_rx = 1; }
const char *XPC_GetLastErrorString(void) {
    static const char *messages[] = { "no error", "Winsock startup failed", "socket creation failed",
        "bind failed", "IP address is invalid", "invalid argument", "sendto failed",
        "recvfrom timed out", "recvfrom failed", "response source mismatch",
        "response header is invalid", "response length is invalid", "DataRef count mismatch" };
    unsigned int error = (unsigned int)g_last_error;
    return error < sizeof(messages) / sizeof(messages[0]) ? messages[error] : "unknown error";
}
