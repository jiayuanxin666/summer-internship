#include <SDL2/SDL.h>

// 简易画圆函数
void drawCircle(SDL_Renderer* renderer, int x, int y, int radius) {
    for (int w = 0; w < radius * 2; w++) {
        for (int h = 0; h < radius * 2; h++) {
            int dx = radius - w; // 横轴距离圆心的距离
            int dy = radius - h; // 纵轴距离圆心的距离
            // 勾股定理判断点是否在圆内
            if (dx*dx + dy*dy <= radius*radius) {
                SDL_RenderDrawPoint(renderer, x + dx, y + dy);
            }
        }
    }
}

int main(int argc, char *argv[]) {
    // 1. 初始化SDL视频模块
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL初始化失败：%s\n", SDL_GetError());
        return 1;
    }

    // 2. 创建窗口 + 新增：创建渲染器（绘制图形必须）
    SDL_Window* window = SDL_CreateWindow(
        "SDL基础窗口+图形绘制",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        800, 600,
        SDL_WINDOW_SHOWN
    );
    if (!window) {
        printf("窗口创建失败：%s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    // 创建硬件加速渲染器
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    // 3. 事件循环（绘制+退出逻辑）
    SDL_Event event;
    int running = 1;
    while (running) {
        // 处理退出事件（关闭窗口/按ESC）
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT || (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)) {
                running = 0;
            }
        }

        // ------------------- 核心：绘制图形 -------------------
        // 清空窗口（黑色背景）
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); // RGBA：黑
        SDL_RenderClear(renderer);

        // 1. 画点（红色，坐标(100, 100)）
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255); // RGBA：红
        SDL_RenderDrawPoint(renderer, 100, 100);

        // 2. 画线（绿色，从(200, 100)到(700, 100)）
        SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255); // RGBA：绿
        SDL_RenderDrawLine(renderer, 200, 100, 700, 100);

        // 3. 画空心矩形（蓝色，左上角(200, 200)，右下角(400, 350)）
        SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255); // RGBA：蓝
        SDL_Rect rect1 = {200, 200, 200, 150}; // x,y,宽,高
        SDL_RenderDrawRect(renderer, &rect1);

        // 4. 画实心矩形（黄色，左上角(500, 200)，右下角(700, 350)）
        SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255); // RGBA：黄
        SDL_Rect rect2 = {500, 200, 200, 150};
        SDL_RenderFillRect(renderer, &rect2);

        // 5. 画圆形（紫色，圆心(400, 450)，半径80）
        SDL_SetRenderDrawColor(renderer, 128, 0, 128, 255); // RGBA：紫
        drawCircle(renderer, 400, 450, 80);

        // 更新屏幕显示（把绘制的内容渲染到窗口）
        SDL_RenderPresent(renderer);
    }

    // 4. 释放资源（新增：释放渲染器）
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}