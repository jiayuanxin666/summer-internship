#include "PFD/pfd_xplane.h"
#include "Util/xplaneConnect.h"
#include <stdio.h>

int main(void) {
    const char *drefs[] = {"sim/flightmodel/position/theta"};
    float values[1][8] = {{0}};
    XPCSocket sock = aopenUDP("127.0.0.1", PFD_XPLANE_PORT, 0);
    if (!XPCSocket_IsOpen(sock)) {
        fprintf(stderr, "XPC socket open failed: %s.\n", XPC_GetLastErrorString());
        return 1;
    }
    printf("XPC socket opened.\n");
    if (!getDREFs(sock, drefs, 1, values)) {
        fprintf(stderr, "XPC theta fetch failed: %s.\n", XPC_GetLastErrorString());
        closeUDP(sock);
        return 2;
    }
    printf("XPC theta fetch succeeded.\nPitch = %.2f degrees.\n", values[0][0]);
    closeUDP(sock);
    return 0;
}
