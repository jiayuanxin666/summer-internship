#include "pfd_xplane.h"

#ifdef ENABLE_XPLANE

/* TODO_XPLANE: 启用时包含真实的 X-Plane Connect 头文件。 */
/* #include "../Util/xplaneConnect.h" */

int PFD_XPlane_Open(void)
{
    /* TODO_XPLANE: 使用 openUDP(49000) 建立连接。 */
    return 0;
}

int PFD_XPlane_FetchData(PFD_Data *data)
{
    (void)data;
    /* TODO_XPLANE: 使用 getDREFs 填充 PFD_Data，并完成米到英尺转换。 */
    return 0;
}

void PFD_XPlane_Close(void)
{
    /* TODO_XPLANE: 调用 closeUDP 释放 UDP 通信资源。 */
}

#endif
