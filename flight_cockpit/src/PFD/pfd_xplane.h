#ifndef PFD_XPLANE_H
#define PFD_XPLANE_H

#include "pfd_data.h"

/*
 * TODO_XPLANE:
 * 后期导入 src/Util/xplaneConnect.h 和 src/Util/xplaneConnect.c 后启用。
 * 在 tasks.json 中加入 src/Util/xplaneConnect.c 和 src/PFD/pfd_xplane.c。
 * 编译时添加 -DENABLE_XPLANE。
 * 使用 openUDP(49000) 创建 XPCSocket。
 * 使用 getDREFs 一次性获取所有 PFD 所需数据。
 * 程序退出时调用 closeUDP 释放 UDP 通信。
 *
 * DREF 对应关系：
 * 0. sim/flightmodel/position/theta             pitch，单位：度
 * 1. sim/flightmodel/position/phi               roll，单位：度
 * 2. sim/flightmodel/position/psi               yaw，单位：度
 * 3. sim/flightmodel/position/elevation         气压高度，单位：米，feet = meter * 3.28084f
 * 4. sim/flightmodel/position/y_agl             离地高度，单位：米，feet = meter * 3.28084f
 * 5. sim/flightmodel/engine/ENGN_thro           油门数组 float[8]，范围 0.0 到 1.0
 * 6. sim/flightmodel/position/indicated_airspeed 当前指示空速，单位：节
 * 7. sim/cockpit/autopilot/airspeed             自动驾驶目标空速，单位：节
 * 8. sim/flightmodel/position/vh_ind_fpm        垂直速度，单位：英尺/分钟
 * 9. sim/flightmodel/position/mag_psi           当前磁航向，单位：度
 * 10. sim/cockpit/autopilot/heading_mag         目标磁航向，单位：度
 * 11. sim/cockpit/autopilot/altitude            目标高度，单位：英尺
 *
 * TODO_XPLANE_THREAD:
 * 后期接入 X-Plane 后，建议使用 Windows CreateThread 创建数据线程。
 * 主线程负责 SDL 渲染和事件处理。
 * 数据线程负责循环调用 PFD_XPlane_FetchData。
 * 使用共享 PFD_Data 和 int 类型标志位 data_ready / thread_exit 实现轻量同步。
 * 子线程先读取到局部 PFD_Data，再一次性复制到共享数据，最后设置 data_ready = 1。
 * 主线程发现 data_ready == 1 后复制共享数据用于渲染，再将 data_ready = 0。
 * 程序退出时设置 thread_exit = 1，并 WaitForSingleObject 等待子线程退出。
 */

#ifdef ENABLE_XPLANE

int PFD_XPlane_Open(void);
int PFD_XPlane_FetchData(PFD_Data *data);
void PFD_XPlane_Close(void);

#endif

#endif
