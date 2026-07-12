#ifndef PFD_XPLANE_H
#define PFD_XPLANE_H

#include "pfd_data.h"

#ifndef PFD_ENGINE_COUNT
#define PFD_ENGINE_COUNT 2
#endif

#ifndef PFD_XPLANE_PORT
#define PFD_XPLANE_PORT 49009
#endif

/*
 * DREF mapping:
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
 */

#ifdef ENABLE_XPLANE

int PFD_XPlane_Open(void);
int PFD_XPlane_FetchData(PFD_Data *data);
void PFD_XPlane_Close(void);

#endif

#endif
