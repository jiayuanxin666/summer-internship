#include "SDL_Util.h"

int initSDL(void)
{
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        printf("SDL初始化失败: %s\n", SDL_GetError());
        return -1;
    }

    if (TTF_Init() == -1)
    {
        printf("SDL字体初始化失败: %s\n", TTF_GetError());
        SDL_Quit();
        return -1;
    }

    if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG))
    {
        printf("SDL图片初始化失败: %s\n", IMG_GetError());
        TTF_Quit();
        SDL_Quit();
        return -1;
    }

    return 0;
}

void closeSDL(void)
{
    IMG_Quit();
    TTF_Quit();
    SDL_Quit();
}