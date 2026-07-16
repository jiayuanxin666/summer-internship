#ifndef COCKPIT_H
#define COCKPIT_H

#include <SDL2/SDL.h>

#define COCKPIT_IMAGE_WIDTH 8026
#define COCKPIT_IMAGE_HEIGHT 3136
#define COCKPIT_MODULE_COUNT 8

typedef struct CockpitApp CockpitApp;

int Cockpit_SelfTest(void);
int Cockpit_Run(int embed_modules, int smoke, const char *module_dir);

#endif
