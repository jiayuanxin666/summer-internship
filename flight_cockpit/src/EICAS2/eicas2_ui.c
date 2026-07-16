#include "eicas2_ui.h"
#include <SDL2/SDL2_gfxPrimitives.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define EICAS_PI 3.14159265358979323846f

static int iroundf_local(float value)
{
    return (int)(value + (value >= 0.0f ? 0.5f : -0.5f));
}

static SDL_Color color_white(void) { SDL_Color c = {232, 232, 232, 255}; return c; }
static SDL_Color color_cyan(void) { SDL_Color c = {0, 174, 182, 255}; return c; }
static SDL_Color color_amber(void) { SDL_Color c = {255, 188, 0, 255}; return c; }
static SDL_Color color_red(void) { SDL_Color c = {255, 35, 20, 255}; return c; }

static const char *data_source_text(int source)
{
    switch (source) {
    case EICAS2_DATA_SOURCE_XPLANE: return "SRC XPLANE";
    case EICAS2_DATA_SOURCE_FILE: return "SRC FILE";
    default: return "SRC SIM";
    }
}

static TTF_Font *open_eis_font(int size)
{
    const char *paths[] = {
        "assets/fonts/consola.ttf", "../assets/fonts/consola.ttf",
        "C:/Windows/Fonts/consola.ttf", "C:/Windows/Fonts/lucon.ttf",
        "C:/Windows/Fonts/arial.ttf"
    };
    for (int i = 0; i < (int)(sizeof(paths) / sizeof(paths[0])); ++i) {
        TTF_Font *font = TTF_OpenFont(paths[i], size);
        if (font) return font;
    }
    return NULL;
}

static int load_fonts(EICAS2_UI *ui)
{
    ui->font_small = open_eis_font(15);
    ui->font_medium = open_eis_font(19);
    ui->font_large = open_eis_font(25);
    if (!ui->font_small || !ui->font_medium || !ui->font_large) {
        printf("EICAS2 font loading failed: %s\n", TTF_GetError());
        return -1;
    }
    return 0;
}

static int draw_text(SDL_Renderer *renderer, TTF_Font *font, int x, int y,
                     const char *text, SDL_Color color)
{
    SDL_Surface *surface;
    SDL_Texture *texture;
    SDL_Rect dst;
    if (!renderer || !font || !text) return 0;
    surface = TTF_RenderUTF8_Blended(font, text, color);
    if (!surface) return 0;
    texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (!texture) { SDL_FreeSurface(surface); return 0; }
    dst.x = x; dst.y = y; dst.w = surface->w; dst.h = surface->h;
    SDL_RenderCopy(renderer, texture, NULL, &dst);
    SDL_DestroyTexture(texture);
    SDL_FreeSurface(surface);
    return 1;
}

static int draw_text_center(SDL_Renderer *renderer, TTF_Font *font, int cx, int y,
                            const char *text, SDL_Color color)
{
    int w = 0, h = 0;
    if (!font || !text || TTF_SizeUTF8(font, text, &w, &h) != 0) return 0;
    return draw_text(renderer, font, cx - w / 2, y, text, color);
}

static void update_render_rect(EICAS2_UI *ui)
{
    float sx, sy, scale;
    if (!ui || ui->window_width <= 0 || ui->window_height <= 0) return;
    sx = (float)ui->window_width / EICAS2_LOGIC_WIDTH;
    sy = (float)ui->window_height / EICAS2_LOGIC_HEIGHT;
    scale = sx < sy ? sx : sy;
    ui->render_rect.w = (int)(EICAS2_LOGIC_WIDTH * scale);
    ui->render_rect.h = (int)(EICAS2_LOGIC_HEIGHT * scale);
    ui->render_rect.x = (ui->window_width - ui->render_rect.w) / 2;
    ui->render_rect.y = (ui->window_height - ui->render_rect.h) / 2;
}

static void draw_value_box(EICAS2_UI *ui, int cx, int cy, int width,
                           const char *text, SDL_Color color)
{
    rectangleRGBA(ui->renderer, cx - width / 2, cy - 12,
                  cx + width / 2, cy + 13, 225, 225, 225, 255);
    draw_text_center(ui->renderer, ui->font_medium, cx, cy - 10, text, color);
}

static void draw_n2_gauge(EICAS2_UI *ui, int cx, int cy, float value)
{
    SDL_Renderer *r = ui->renderer;
    char text[32];
    const int radius = 59;
    float pct = value / 110.0f;
    float angle;
    int px, py;
    SDL_Color value_color = value > 105.0f ? color_red() :
                            (value > 100.0f ? color_amber() : color_white());
    if (pct < 0.0f) pct = 0.0f;
    if (pct > 1.0f) pct = 1.0f;
    angle = pct * 180.0f;
    filledPieRGBA(r, cx, cy, radius - 1, 0, iroundf_local(angle), 61, 61, 61, 255);
    arcRGBA(r, cx, cy, radius, 0, 180, 232, 232, 232, 255);
    for (int step = 0; step <= 10; ++step) {
        float a = ((float)step * 10.0f / 110.0f) * EICAS_PI;
        int major = (step % 2) == 0;
        int x1 = iroundf_local(cx + cosf(a) * radius);
        int y1 = iroundf_local(cy + sinf(a) * radius);
        int x2 = iroundf_local(cx + cosf(a) * (radius - (major ? 7 : 4)));
        int y2 = iroundf_local(cy + sinf(a) * (radius - (major ? 7 : 4)));
        lineRGBA(r, x1, y1, x2, y2, 232, 232, 232, 255);
        if (major) {
            int tx = iroundf_local(cx + cosf(a) * (radius - 17));
            int ty = iroundf_local(cy + sinf(a) * (radius - 17)) - 8;
            snprintf(text, sizeof(text), "%d", step);
            draw_text_center(r, ui->font_small, tx, ty, text, color_white());
        }
    }
    {
        float a = (100.0f / 110.0f) * EICAS_PI;
        lineRGBA(r,
            iroundf_local(cx + cosf(a) * (radius + 6)),
            iroundf_local(cy + sinf(a) * (radius + 6)),
            iroundf_local(cx + cosf(a) * (radius - 3)),
            iroundf_local(cy + sinf(a) * (radius - 3)), 255, 35, 20, 255);
    }
    px = iroundf_local(cx + cosf(angle * EICAS_PI / 180.0f) * radius);
    py = iroundf_local(cy + sinf(angle * EICAS_PI / 180.0f) * radius);
    lineRGBA(r, cx, cy, px, py, 245, 245, 245, 255);
    snprintf(text, sizeof(text), "%.1f", value);
    draw_value_box(ui, cx + radius, cy - 7, 77, text, value_color);
}

static void draw_vertical_tape(EICAS2_UI *ui, int x, int top, int bottom,
                               float value, float min_value, float max_value,
                               float amber_value, int pointer_from_left,
                               int caution_high)
{
    float pct = (value - min_value) / (max_value - min_value);
    float amber_pct = (amber_value - min_value) / (max_value - min_value);
    int py, ay;
    if (pct < 0.0f) pct = 0.0f;
    if (pct > 1.0f) pct = 1.0f;
    if (amber_pct < 0.0f) amber_pct = 0.0f;
    if (amber_pct > 1.0f) amber_pct = 1.0f;
    py = bottom - iroundf_local(pct * (bottom - top));
    ay = bottom - iroundf_local(amber_pct * (bottom - top));
    if (caution_high) {
        lineRGBA(ui->renderer, x, top, x, ay, 255, 188, 0, 255);
        lineRGBA(ui->renderer, x, ay, x, bottom, 232, 232, 232, 255);
        lineRGBA(ui->renderer, x - 4, top, x + 4, top, 255, 35, 20, 255);
    } else {
        lineRGBA(ui->renderer, x, top, x, ay, 232, 232, 232, 255);
        lineRGBA(ui->renderer, x, ay, x, bottom, 255, 188, 0, 255);
        lineRGBA(ui->renderer, x - 4, bottom, x + 4, bottom, 255, 35, 20, 255);
    }
    lineRGBA(ui->renderer, x - 4, ay, x + 4, ay, 255, 188, 0, 255);
    if (pointer_from_left) {
        filledTrigonRGBA(ui->renderer, x - 1, py, x - 7, py - 5, x - 7, py + 5,
                         235, 235, 235, 255);
    } else {
        filledTrigonRGBA(ui->renderer, x + 1, py, x + 7, py - 5, x + 7, py + 5,
                         235, 235, 235, 255);
    }
}

static SDL_Color oil_press_color(float value)
{
    return value <= 15.0f ? color_amber() : color_white();
}

static SDL_Color oil_temp_color(float value)
{
    return value >= 155.0f ? color_red() :
           (value >= 140.0f ? color_amber() : color_white());
}

static SDL_Color vib_color(float value)
{
    return value >= 4.0f ? color_amber() : color_white();
}

static void draw_pair(EICAS2_UI *ui, int y, const char *label1, const char *label2,
                      float left, float right, const char *format,
                      SDL_Color left_color, SDL_Color right_color)
{
    char text[32];
    snprintf(text, sizeof(text), format, left);
    draw_value_box(ui, 159, y, 77, text, left_color);
    snprintf(text, sizeof(text), format, right);
    draw_value_box(ui, 402, y, 77, text, right_color);
    draw_text_center(ui->renderer, ui->font_medium, 280, y - 29, label1, color_cyan());
    if (label2 && label2[0])
        draw_text_center(ui->renderer, ui->font_medium, 280, y - 9, label2, color_cyan());
}

int EICAS2_UI_Init(EICAS2_UI *ui)
{
    if (!ui) return -1;
    memset(ui, 0, sizeof(*ui));
    ui->window_width = EICAS2_LOGIC_WIDTH;
    ui->window_height = EICAS2_LOGIC_HEIGHT;
    ui->window = SDL_CreateWindow("B737-800 LOWER ENGINE DISPLAY",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        EICAS2_LOGIC_WIDTH, EICAS2_LOGIC_HEIGHT, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!ui->window) return -1;
    ui->renderer = SDL_CreateRenderer(ui->window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE);
    ui->vsync_enabled = ui->renderer != NULL;
    if (!ui->renderer) {
        ui->renderer = SDL_CreateRenderer(ui->window, -1,
            SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE);
        ui->vsync_enabled = 0;
    }
    if (!ui->renderer) { EICAS2_UI_Destroy(ui); return -1; }
    ui->logic_texture = SDL_CreateTexture(ui->renderer, SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET, EICAS2_LOGIC_WIDTH, EICAS2_LOGIC_HEIGHT);
    if (!ui->logic_texture || load_fonts(ui) != 0) { EICAS2_UI_Destroy(ui); return -1; }
    SDL_SetTextureBlendMode(ui->logic_texture, SDL_BLENDMODE_BLEND);
    update_render_rect(ui);
    return 0;
}

void EICAS2_UI_HandleEvent(EICAS2_UI *ui, SDL_Event *event, int *running)
{
    if (!ui || !event || !running) return;
    if (event->type == SDL_QUIT ||
        (event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_ESCAPE)) *running = 0;
    else if (event->type == SDL_WINDOWEVENT && event->window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
        ui->window_width = event->window.data1;
        ui->window_height = event->window.data2;
        update_render_rect(ui);
    }
}

void EICAS2_UI_Render(EICAS2_UI *ui, const EICAS2_Data *data)
{
    char text[32];
    if (!ui || !data || !ui->renderer || !ui->logic_texture) return;
    SDL_SetRenderTarget(ui->renderer, ui->logic_texture);
    SDL_SetRenderDrawColor(ui->renderer, 0, 0, 0, 255);
    SDL_RenderClear(ui->renderer);

    draw_n2_gauge(ui, 166, 90, data->n2_left);
    draw_n2_gauge(ui, 392, 90, data->n2_right);
    draw_text_center(ui->renderer, ui->font_medium, 280, 157, "N2", color_cyan());

    snprintf(text, sizeof(text), "%.2f", data->ff_left);
    draw_value_box(ui, 166, 238, 58, text, color_white());
    snprintf(text, sizeof(text), "%.2f", data->ff_right);
    draw_value_box(ui, 392, 238, 58, text, color_white());
    draw_text_center(ui->renderer, ui->font_medium, 280, 230, "FF", color_cyan());

    draw_pair(ui, 337, "OIL", "PRESS", data->oil_press_left, data->oil_press_right,
              "%.0f", oil_press_color(data->oil_press_left), oil_press_color(data->oil_press_right));
    draw_vertical_tape(ui, 212, 295, 335, data->oil_press_left, 0.0f, 100.0f, 15.0f, 0, 0);
    draw_vertical_tape(ui, 339, 295, 335, data->oil_press_right, 0.0f, 100.0f, 15.0f, 1, 0);

    draw_pair(ui, 465, "OIL", "TEMP", data->oil_temp_left, data->oil_temp_right,
              "%.0f", oil_temp_color(data->oil_temp_left), oil_temp_color(data->oil_temp_right));
    draw_vertical_tape(ui, 212, 423, 464, data->oil_temp_left, 0.0f, 200.0f, 155.0f, 0, 1);
    draw_vertical_tape(ui, 339, 423, 464, data->oil_temp_right, 0.0f, 200.0f, 155.0f, 1, 1);

    draw_pair(ui, 563, "OIL QTY", "", data->oil_qty_left, data->oil_qty_right,
              "%.0f", color_white(), color_white());

    draw_pair(ui, 647, "VIB", "", data->vib_left, data->vib_right,
              "%.1f", vib_color(data->vib_left), vib_color(data->vib_right));
    draw_vertical_tape(ui, 212, 626, 669, data->vib_left, 0.0f, 5.0f, 4.0f, 0, 1);
    draw_vertical_tape(ui, 339, 626, 669, data->vib_right, 0.0f, 5.0f, 4.0f, 1, 1);

    draw_text(ui->renderer, ui->font_small, 8, EICAS2_LOGIC_HEIGHT - 22,
              data_source_text(data->data_source), color_cyan());

    SDL_SetRenderTarget(ui->renderer, NULL);
    SDL_SetRenderDrawColor(ui->renderer, 0, 0, 0, 255);
    SDL_RenderClear(ui->renderer);
    SDL_RenderCopy(ui->renderer, ui->logic_texture, NULL, &ui->render_rect);
    SDL_RenderPresent(ui->renderer);
}

void EICAS2_UI_Destroy(EICAS2_UI *ui)
{
    if (!ui) return;
    if (ui->font_small) TTF_CloseFont(ui->font_small);
    if (ui->font_medium) TTF_CloseFont(ui->font_medium);
    if (ui->font_large) TTF_CloseFont(ui->font_large);
    if (ui->logic_texture) SDL_DestroyTexture(ui->logic_texture);
    if (ui->renderer) SDL_DestroyRenderer(ui->renderer);
    if (ui->window) SDL_DestroyWindow(ui->window);
    memset(ui, 0, sizeof(*ui));
}
