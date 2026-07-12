#include "fmc_connect.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef ENABLE_XPLANE
#include "../Util/xplaneConnect.h"

typedef struct FMC_XPlaneConnection
{
    XPCSocket socket;
} FMC_XPlaneConnection;
#endif

int FMC_Connect_Init(FMC_Connect *connect, const char *host)
{
    if (!connect) {
        return 0;
    }
    memset(connect, 0, sizeof(*connect));

#ifdef ENABLE_XPLANE
    connect->connection = (FMC_XPlaneConnection *)calloc(1, sizeof(*connect->connection));
    if (!connect->connection) {
        return 0;
    }
    connect->connection->socket = openUDP(host && host[0] ? host : "127.0.0.1");
    connect->available = XPCSocket_IsOpen(connect->connection->socket);
    if (!connect->available) {
        free(connect->connection);
        connect->connection = NULL;
    }
#else
    (void)host;
    connect->available = 0;
#endif

    return 1;
}

int FMC_Connect_SendCommand(FMC_Connect *connect, const char *command)
{
    if (!command || !command[0]) {
        return 0;
    }

#ifdef ENABLE_XPLANE
    if (connect && connect->available && connect->connection) {
        char packet[512];
        int command_length = (int)strlen(command);
        int packet_length;
        int result;

        if (command_length > (int)sizeof(packet) - 6) {
            return 0;
        }
        memset(packet, 0, sizeof(packet));
        memcpy(packet, "CMND", 4);
        packet[4] = '\0';
        memcpy(packet + 5, command, (size_t)command_length);
        /* X-Plane's CMND packet is `CMND\\0<command>\\0`.  Include the
         * trailing NUL so the receiver can treat the command as a C string. */
        packet_length = command_length + 6;
        result = sendto(connect->connection->socket.socket_fd,
                        packet, packet_length, 0,
                        (struct sockaddr *)&connect->connection->socket.xplane_addr,
                        sizeof(connect->connection->socket.xplane_addr));
        return result != SOCKET_ERROR;
    }
#else
    (void)connect;
#endif

    return 0;
}

int FMC_Connect_SendKey(FMC_Connect *connect, FMC_Key key)
{
    const char *command = FMC_Key_XPlaneCommand(key);
    return command ? FMC_Connect_SendCommand(connect, command) : 0;
}

static void send_characters(FMC_Connect *connect, const char *text)
{
    while (text && *text) {
        char ch = *text++;
        if (ch >= 'a' && ch <= 'z') {
            ch = (char)(ch - 'a' + 'A');
        }
        if (ch >= 'A' && ch <= 'Z') {
            FMC_Connect_SendKey(connect, (FMC_Key)(FMC_KEY_A + ch - 'A'));
        } else if (ch >= '0' && ch <= '9') {
            FMC_Connect_SendKey(connect, (FMC_Key)(FMC_KEY_0 + ch - '0'));
        } else if (ch == '.') {
            FMC_Connect_SendKey(connect, FMC_KEY_PERIOD);
        } else if (ch == '-') {
            FMC_Connect_SendKey(connect, FMC_KEY_PLUS_MINUS);
        } else if (ch == '/') {
            FMC_Connect_SendKey(connect, FMC_KEY_SLASH);
        }
    }
}

static void clear_xplane_scratchpad(FMC_Connect *connect)
{
    FMC_Connect_SendKey(connect, FMC_KEY_CLR);
    FMC_Connect_SendKey(connect, FMC_KEY_DEL);
    FMC_Connect_SendKey(connect, FMC_KEY_CLR);
}

int FMC_Connect_SyncRoute(FMC_Connect *connect,
                          const char *origin,
                          const char *destination,
                          const char *flight_number)
{
    if (!connect || !connect->available || !origin || !destination || !flight_number) {
        return 0;
    }

    FMC_Connect_SendKey(connect, FMC_KEY_RTE);
    clear_xplane_scratchpad(connect);
    send_characters(connect, origin);
    FMC_Connect_SendKey(connect, FMC_KEY_L1);
    clear_xplane_scratchpad(connect);
    send_characters(connect, destination);
    FMC_Connect_SendKey(connect, FMC_KEY_R1);
    clear_xplane_scratchpad(connect);
    send_characters(connect, flight_number);
    FMC_Connect_SendKey(connect, FMC_KEY_R2);
    FMC_Connect_SendKey(connect, FMC_KEY_EXEC);
    return 1;
}

void FMC_Connect_Close(FMC_Connect *connect)
{
    if (!connect) {
        return;
    }
#ifdef ENABLE_XPLANE
    if (connect->connection) {
        if (connect->available) {
            closeUDP(connect->connection->socket);
        }
        free(connect->connection);
        connect->connection = NULL;
    }
#endif
    connect->available = 0;
}
