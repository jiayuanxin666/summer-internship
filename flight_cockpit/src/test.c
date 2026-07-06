#include <stdio.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL2_gfxPrimitives.h>

#include "Util/SDL_Util.h"

int main(int argc, char *argv[])
{
    // 1. 初始化 SDL2、SDL2_image、SDL2_ttf
    if (initSDL() == -1)
    {
        printf("SDL load failed\n");
        return -1;
    }
    else
    {
        printf("SDL load success\n");
    }

    // 2. 创建窗口
    SDL_Window *window = SDL_CreateWindow(
        "TEST",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        800,
        800,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );

    if (window == NULL)
    {
        printf("create window failed: %s\n", SDL_GetError());
        SDL_Quit();
        return -1;
    }

    // 3. 创建渲染器
    SDL_Renderer *renderer = SDL_CreateRenderer(
        window,
        -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );

    if (renderer == NULL)
    {
        printf("create renderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    // 4. 加载图片，注意路径是相对于项目根目录
    const char image_path[] = "assets/fmc.png";

    SDL_Texture *texture = IMG_LoadTexture(renderer, image_path);
    if (texture == NULL)
    {
        printf("image load failed: %s\n", IMG_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        IMG_Quit();
        TTF_Quit();
        SDL_Quit();
        return -1;
    }

    // 图片显示位置和大小
    SDL_Rect dstrect = {400, 400, 200, 400};

    // 5. 主循环
    int quit = 0;
    SDL_Event event;

    while (!quit)
    {
        // 处理事件
        while (SDL_PollEvent(&event))
        {
            // 点击窗口关闭按钮退出
            if (event.type == SDL_QUIT)
            {
                quit = 1;
            }

            // 按 ESC 退出
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)
            {
                quit = 1;
            }
        }

        // 6. 清屏，设置背景为黑色
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        // ==================== SDL2 基础图形绘制 ====================

        // 红色点
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
        SDL_RenderDrawPoint(renderer, 50, 50);

        // 蓝色线段
        SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255);
        SDL_RenderDrawLine(renderer, 100, 100, 200, 200);

        // 绿色空心矩形
        SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
        SDL_Rect rect = {200, 200, 100, 100};
        SDL_RenderDrawRect(renderer, &rect);

        // ==================== SDL2_gfx 进阶图形绘制 ====================

        // 紫色圆弧
        arcRGBA(renderer, 150, 250, 70, 0, 270, 128, 0, 128, 255);

        // 橙色扇形
        pieRGBA(renderer, 350, 250, 70, 0, 120, 255, 165, 0, 255);

        // 青色实心三角形
        const Sint16 polyX[] = {500, 580, 540};
        const Sint16 polyY[] = {220, 220, 140};
        int polyCount = 3;

        filledPolygonRGBA(
            renderer,
            polyX,
            polyY,
            polyCount,
            0,
            255,
            255,
            255
        );

        // ==================== 图片绘制 ====================

        SDL_RenderCopy(renderer, texture, NULL, &dstrect);

        // 7. 更新窗口显示
        SDL_RenderPresent(renderer);

        // 控制帧率约 60 FPS
        SDL_Delay(16);
    }

    // 8. 释放资源
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    IMG_Quit();
    TTF_Quit();
    SDL_Quit();

    return 0;
}