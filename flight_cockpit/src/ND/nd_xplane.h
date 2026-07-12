#ifndef ND_XPLANE_H
#define ND_XPLANE_H

#include "nd_data.h"

/*
 * Optional X-Plane integration. Keep it behind ENABLE_XPLANE so the default
 * build remains runnable without the simulator.
 *
 * DREF mapping:
 * 0. sim/flightmodel/position/latitude
 * 1. sim/flightmodel/position/longitude
 * 2. sim/flightmodel/position/mag_psi
 * 3. sim/flightmodel/position/hpath
 * 4. sim/cockpit/autopilot/heading_mag
 * 5. sim/flightmodel/position/true_airspeed
 * 6. sim/flightmodel/position/groundspeed
 */

#ifdef ENABLE_XPLANE

int ND_XPlane_Open(void);
int ND_XPlane_FetchData(ND_Data *data);
void ND_XPlane_Close(void);

#endif

#endif
