#include "cockpit.h"

#include <SDL2/SDL.h>
#include <stdio.h>
#include <string.h>

static const char *default_module_dir(void)
{
    static char path[1024];
    char *base = SDL_GetBasePath();
    if (base) {
        SDL_snprintf(path, sizeof(path), "%s", base);
        SDL_free(base);
        return path;
    }
    return "build";
}

int main(int argc, char **argv)
{
    int self_test = 0;
    int smoke = 0;
    int embed_modules = 1;
    const char *module_dir = NULL;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--self-test") == 0) self_test = 1;
        else if (strcmp(argv[i], "--smoke") == 0) smoke = 1;
        else if (strcmp(argv[i], "--no-modules") == 0) embed_modules = 0;
        else if (strcmp(argv[i], "--module-dir") == 0 && i + 1 < argc) module_dir = argv[++i];
    }
    if (self_test) {
        int ok = Cockpit_SelfTest();
        printf("Cockpit self-test: %s\n", ok ? "PASS" : "FAIL");
        return ok ? 0 : 1;
    }
    if (!module_dir) module_dir = default_module_dir();
    if (smoke) embed_modules = 0;
    return Cockpit_Run(embed_modules, smoke, module_dir) ? 0 : 1;
}
