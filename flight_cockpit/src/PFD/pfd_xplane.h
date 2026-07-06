#ifndef PFD_XPLANE_H
#define PFD_XPLANE_H

#include "pfd_data.h"

/*
 * TODO_XPLANE:
 * Keep X-Plane support disabled in the default build.
 * Later, after adding src/Util/xplaneConnect.h and src/Util/xplaneConnect.c:
 * - add src/Util/xplaneConnect.c and src/PFD/pfd_xplane.c to the build task
 * - add -DENABLE_XPLANE
 * - use openUDP(49000) to create the XPCSocket
 * - use getDREFs to fetch all PFD values in one batch
 * - call closeUDP on program exit
 *
 * DREF mapping for the future integration:
 * 0. sim/flightmodel/position/theta
 *    pitch, degrees
 * 1. sim/flightmodel/position/phi
 *    roll, degrees
 * 2. sim/flightmodel/position/psi
 *    yaw, degrees
 * 3. sim/flightmodel/position/elevation
 *    pressure altitude, meters; convert with feet = meter * 3.28084f
 * 4. sim/flightmodel/position/y_agl
 *    AGL altitude, meters; convert with feet = meter * 3.28084f
 * 5. sim/flightmodel/engine/ENGN_thro
 *    throttle array float[8], range 0.0 to 1.0; convert to 0..100 here
 * 6. sim/flightmodel/position/indicated_airspeed
 *    indicated airspeed, knots
 * 7. sim/cockpit/autopilot/airspeed
 *    autopilot target airspeed, knots
 * 8. sim/flightmodel/position/vh_ind_fpm
 *    vertical speed, feet per minute
 * 9. sim/flightmodel/position/mag_psi
 *    magnetic heading, degrees
 * 10. sim/cockpit/autopilot/heading_mag
 *     target magnetic heading, degrees
 * 11. sim/cockpit/autopilot/altitude
 *     target altitude, feet
 *
 * TODO_XPLANE_THREAD:
 * When X-Plane is integrated, use a Windows CreateThread data worker.
 * The main thread handles SDL rendering and events.
 * The data thread loops over PFD_XPlane_FetchData.
 * Use shared PFD_Data plus int flags such as data_ready and thread_exit.
 * The worker writes to a local PFD_Data first, then copies it to shared data
 * and sets data_ready = 1.
 * The main thread copies shared data when data_ready == 1, then clears it.
 * On exit, set thread_exit = 1 and WaitForSingleObject for the worker.
 */

#ifdef ENABLE_XPLANE

int PFD_XPlane_Open(void);
int PFD_XPlane_FetchData(PFD_Data *data);
void PFD_XPlane_Close(void);

#endif

#endif
