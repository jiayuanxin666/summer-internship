#ifndef PFD_INSTRUMENTS_H
#define PFD_INSTRUMENTS_H

#include <SDL2/SDL.h>
#include "pfd_data.h"

void PFD_DrawAirspeedIndicator(SDL_Renderer *renderer, const PFD_Data *data);
void PFD_DrawAltitudeIndicator(SDL_Renderer *renderer, const PFD_Data *data);
void PFD_DrawAttitudeIndicator(SDL_Renderer *renderer, const PFD_Data *data);
void PFD_DrawVerticalSpeedIndicator(SDL_Renderer *renderer, const PFD_Data *data);
void PFD_DrawHeadingIndicator(SDL_Renderer *renderer, const PFD_Data *data);
void PFD_DrawThrustIndicator(SDL_Renderer *renderer, const PFD_Data *data);
void PFD_DrawFlightModeAnnunciator(SDL_Renderer *renderer, const PFD_Data *data);

#endif
