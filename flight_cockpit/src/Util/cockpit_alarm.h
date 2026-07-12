#ifndef COCKPIT_ALARM_H
#define COCKPIT_ALARM_H
#include <SDL2/SDL.h>
typedef struct CockpitAlarm CockpitAlarm;
CockpitAlarm *CockpitAlarm_Create(void);
void CockpitAlarm_Update(CockpitAlarm *alarm);
void CockpitAlarm_Render(CockpitAlarm *alarm, SDL_Renderer *renderer, int width);
void CockpitAlarm_Destroy(CockpitAlarm *alarm);
int CockpitAlarm_SelfTest(void);
#endif
