#include "pfd_ui.h"
#include "pfd_instruments.h"
#include <SDL2/SDL2_gfxPrimitives.h>
#include <stdio.h>
#include <string.h>

static void PFD_UI_UpdateRenderRect(PFD_UI *ui)
{
    float scale_x;
    float scale_y;
    float scale;
    int render_w;
    int render_h;

    if (!ui) {
        return;
    }

    if (ui->window_width <= 0 || ui->window_height <= 0) {
        ui->render_rect.x = 0;
        ui->render_rect.y = 0;
        ui->render_rect.w = 0;
        ui->render_rect.h = 0;
        return;
    }

    scale_x = (float)ui->window_width / (float)PFD_LOGIC_WIDTH;
    scale_y = (float)ui->window_height / (float)PFD_LOGIC_HEIGHT;
    scale = scale_x < scale_y ? scale_x : scale_y;

    render_w = (int)((float)PFD_LOGIC_WIDTH * scale);
    render_h = (int)((float)PFD_LOGIC_HEIGHT * scale);

    ui->render_rect.x = (ui->window_width - render_w) / 2;
    ui->render_rect.y = (ui->window_height - render_h) / 2;
    ui->render_rect.w = render_w;
    ui->render_rect.h = render_h;
}

int PFD_UI_Init(PFD_UI *ui)
{
    if (!ui) {
        return 0;
    }

    memset(ui, 0, sizeof(*ui));
    ui->window_width = PFD_LOGIC_WIDTH;
    ui->window_height = PFD_LOGIC_HEIGHT;

    ui->window = SDL_CreateWindow(
        "PFD",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        PFD_LOGIC_WIDTH,
        PFD_LOGIC_HEIGHT,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

    if (!ui->window) {
        printf("SDL_CreateWindow failed: %s\n", SDL_GetError());
        return 0;
    }

    ui->renderer = SDL_CreateRenderer(
        ui->window,
        -1,
        SDL_RENDERER_ACCELERATED |
        SDL_RENDERER_PRESENTVSYNC |
        SDL_RENDERER_TARGETTEXTURE);

    if (!ui->renderer) {
        printf("SDL_CreateRenderer failed: %s\n", SDL_GetError());
        PFD_UI_Destroy(ui);
        return 0;
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");

    ui->logic_texture = SDL_CreateTexture(
        ui->renderer,
        SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET,
        PFD_LOGIC_WIDTH,
        PFD_LOGIC_HEIGHT);

    if (!ui->logic_texture) {
        printf("SDL_CreateTexture failed: %s\n", SDL_GetError());
        PFD_UI_Destroy(ui);
        return 0;
    }

    SDL_SetTextureBlendMode(ui->logic_texture, SDL_BLENDMODE_BLEND);
    PFD_UI_UpdateRenderRect(ui);
    return 1;
}

void PFD_UI_HandleEvent(PFD_UI *ui, SDL_Event *event, int *running)
{
    if (!ui || !event || !running) {
        return;
    }

    if (event->type == SDL_QUIT) {
        *running = 0;
    } else if (event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_ESCAPE) {
        *running = 0;
    } else if (event->type == SDL_WINDOWEVENT &&
               event->window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
        ui->window_width = event->window.data1;
        ui->window_height = event->window.data2;
        PFD_UI_UpdateRenderRect(ui);
    }
}

void PFD_UI_Render(PFD_UI *ui, const PFD_Data *data)
{
    if (!ui || !data || !ui->renderer || !ui->logic_texture) {
        return;
    }

    SDL_SetRenderTarget(ui->renderer, ui->logic_texture);
    SDL_SetRenderDrawColor(ui->renderer, 2, 3, 5, 255);
    SDL_RenderClear(ui->renderer);

    PFD_DrawAttitudeIndicator(ui->renderer, data);
    PFD_DrawAirspeedIndicator(ui->renderer, data);
    PFD_DrawAltitudeIndicator(ui->renderer, data);
    PFD_DrawVerticalSpeedIndicator(ui->renderer, data);
    PFD_DrawHeadingIndicator(ui->renderer, data);
    PFD_DrawThrustIndicator(ui->renderer, data);
    PFD_DrawFlightModeAnnunciator(ui->renderer, data);

    rectangleRGBA(ui->renderer, 0, 0, PFD_LOGIC_WIDTH - 1, PFD_LOGIC_HEIGHT - 1,
                  70, 75, 85, 255);
    stringRGBA(ui->renderer, 12, PFD_LOGIC_HEIGHT - 18, "PFD SDL2 DEMO  TODO_XPLANE READY",
               145, 150, 160, 255);

    SDL_SetRenderTarget(ui->renderer, NULL);
    SDL_SetRenderDrawColor(ui->renderer, 0, 0, 0, 255);
    SDL_RenderClear(ui->renderer);
    SDL_RenderCopy(ui->renderer, ui->logic_texture, NULL, &ui->render_rect);
    SDL_RenderPresent(ui->renderer);
}

void PFD_UI_Destroy(PFD_UI *ui)
{
    if (!ui) {
        return;
    }

    if (ui->logic_texture) {
        SDL_DestroyTexture(ui->logic_texture);
        ui->logic_texture = NULL;
    }
    if (ui->renderer) {
        SDL_DestroyRenderer(ui->renderer);
        ui->renderer = NULL;
    }
    if (ui->window) {
        SDL_DestroyWindow(ui->window);
        ui->window = NULL;
    }
}
