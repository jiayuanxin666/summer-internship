#include "eicas2_ui.h"
#include <SDL2/SDL2_gfxPrimitives.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define EICAS2_PI 3.14159265358979323846f

static int g_fps_value = 0;
static int g_fps_frames = 0;
static Uint32 g_fps_last_tick = 0;

static int iroundf_local(float value)
{
    return (int)(value + (value >= 0.0f ? 0.5f : -0.5f));
}

static SDL_Color color_main(void)
{
    SDL_Color color = {235, 240, 245, 255};
    return color;
}

static SDL_Color color_label(void)
{
    SDL_Color color = {120, 255, 150, 255};
    return color;
}

static SDL_Color color_aux(void)
{
    SDL_Color color = {190, 205, 215, 255};
    return color;
}

static SDL_Color color_warning(void)
{
    SDL_Color color = {255, 170, 60, 255};
    return color;
}

static SDL_Color color_danger(void)
{
    SDL_Color color = {255, 70, 70, 255};
    return color;
}

static const char *source_text(int source)
{
    switch (source) {
    case EICAS2_DATA_SOURCE_FILE:
        return "SRC FILE";
    case EICAS2_DATA_SOURCE_XPLANE:
        return "SRC XPLANE";
    case EICAS2_DATA_SOURCE_SIM:
    default:
        return "SRC SIM";
    }
}

static TTF_Font *open_consolas_font(int size)
{
    const char *paths[] = {
        "assets/fonts/consola.ttf",
        "../assets/fonts/consola.ttf",
        "assets/ALIBABAPUHUITI-2-45-LIGHT.TTF",
        "../assets/ALIBABAPUHUITI-2-45-LIGHT.TTF",
        "C:/Windows/Fonts/consola.ttf",
        "C:/Windows/Fonts/consolab.ttf",
        "C:/Windows/Fonts/arial.ttf",
        "C:/Windows/Fonts/lucon.ttf",
        "C:/Windows/Fonts/cour.ttf",
        "C:/Windows/Fonts/msyh.ttc"
    };

    for (int i = 0; i < (int)(sizeof(paths) / sizeof(paths[0])); ++i) {
        TTF_Font *font = TTF_OpenFont(paths[i], size);
        if (font) {
            return font;
        }
    }

    return NULL;
}

static int load_fonts(EICAS2_UI *ui)
{
    ui->font_small = open_consolas_font(15);
    ui->font_medium = open_consolas_font(20);
    ui->font_large = open_consolas_font(28);

    if (!ui->font_small && !ui->font_medium && !ui->font_large) {
        printf("EICAS2 font loading failed: %s\n", TTF_GetError());
        return -1;
    }

    if (!ui->font_medium) {
        ui->font_medium = open_consolas_font(20);
        if (!ui->font_medium) {
            ui->font_medium = ui->font_small ? ui->font_small : ui->font_large;
        }
    }
    if (!ui->font_small) {
        ui->font_small = ui->font_medium ? ui->font_medium : ui->font_large;
    }
    if (!ui->font_large) {
        ui->font_large = ui->font_medium ? ui->font_medium : ui->font_small;
    }

    return 0;
}

static int draw_ttf_text(SDL_Renderer *renderer, TTF_Font *font, int x, int y,
                         const char *text, SDL_Color color)
{
    SDL_Surface *surface;
    SDL_Texture *texture;
    SDL_Rect dst;

    if (!renderer || !font || !text) {
        return 0;
    }

    surface = TTF_RenderUTF8_Blended(font, text, color);
    if (!surface) {
        return 0;
    }

    texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (!texture) {
        SDL_FreeSurface(surface);
        return 0;
    }

    dst.x = x;
    dst.y = y;
    dst.w = surface->w;
    dst.h = surface->h;
    SDL_RenderCopy(renderer, texture, NULL, &dst);
    SDL_DestroyTexture(texture);
    SDL_FreeSurface(surface);
    return 1;
}

static int draw_ttf_text_center(SDL_Renderer *renderer, TTF_Font *font, int cx, int y,
                                const char *text, SDL_Color color)
{
    int w = 0;
    int h = 0;

    if (!font || !text || TTF_SizeUTF8(font, text, &w, &h) != 0) {
        return 0;
    }

    return draw_ttf_text(renderer, font, cx - w / 2, y, text, color);
}

static void update_fps_counter(void)
{
    Uint32 now = SDL_GetTicks();
    if (g_fps_last_tick == 0) {
        g_fps_last_tick = now;
    }

    ++g_fps_frames;
    if (now - g_fps_last_tick >= 1000) {
        g_fps_value = g_fps_frames;
        g_fps_frames = 0;
        g_fps_last_tick = now;
    }
}

static void EICAS2_UI_UpdateRenderRect(EICAS2_UI *ui)
{
    float scale_x;
    float scale_y;
    float scale;
    int render_w;
    int render_h;

    if (!ui || ui->window_width <= 0 || ui->window_height <= 0) {
        return;
    }

    scale_x = (float)ui->window_width / (float)EICAS2_LOGIC_WIDTH;
    scale_y = (float)ui->window_height / (float)EICAS2_LOGIC_HEIGHT;
    scale = scale_x < scale_y ? scale_x : scale_y;
    render_w = (int)((float)EICAS2_LOGIC_WIDTH * scale);
    render_h = (int)((float)EICAS2_LOGIC_HEIGHT * scale);
    ui->render_rect.x = (ui->window_width - render_w) / 2;
    ui->render_rect.y = (ui->window_height - render_h) / 2;
    ui->render_rect.w = render_w;
    ui->render_rect.h = render_h;
}

static void draw_half_gauge(EICAS2_UI *ui, int cx, int cy, int radius,
                            float value, float max_value, const char *title,
                            const char *format)
{
    SDL_Renderer *renderer = ui->renderer;
    char text[32];
    SDL_Color value_color = value > 105.0f ? color_danger() : (value > 100.0f ? color_warning() : color_main());
    float pct = value / max_value;
    float angle;
    int needle_x;
    int needle_y;

    if (pct < 0.0f) pct = 0.0f;
    if (pct > 1.0f) pct = 1.0f;

    arcRGBA(renderer, cx, cy, radius, 205, 335, 90, 100, 110, 255);
    arcRGBA(renderer, cx, cy, radius - 1, 205, 335, 90, 100, 110, 255);
    for (int i = 0; i <= 10; ++i) {
        float a = (205.0f + (float)i * 13.0f) * EICAS2_PI / 180.0f;
        int major = (i % 5) == 0;
        int numbered = (i % 2) == 0;
        int x1 = iroundf_local((float)cx + cosf(a) * (float)(radius - (major ? 22 : 14)));
        int y1 = iroundf_local((float)cy + sinf(a) * (float)(radius - (major ? 22 : 14)));
        int x2 = iroundf_local((float)cx + cosf(a) * (float)radius);
        int y2 = iroundf_local((float)cy + sinf(a) * (float)radius);
        lineRGBA(renderer, x1, y1, x2, y2, 220, 230, 235, 255);
        if (numbered) {
            int label_value = iroundf_local(max_value * (float)i / 10.0f);
            int tx = iroundf_local((float)cx + cosf(a) * (float)(radius - 38));
            int ty = iroundf_local((float)cy + sinf(a) * (float)(radius - 38)) - 7;
            snprintf(text, sizeof(text), "%d", label_value);
            draw_ttf_text_center(renderer, ui->font_small, tx, ty, text, color_aux());
        }
    }

    angle = (205.0f + pct * 130.0f) * EICAS2_PI / 180.0f;
    needle_x = iroundf_local((float)cx + cosf(angle) * (float)(radius - 24));
    needle_y = iroundf_local((float)cy + sinf(angle) * (float)(radius - 24));
    thickLineRGBA(renderer, cx, cy, needle_x, needle_y, 4, 235, 240, 245, 255);
    filledTrigonRGBA(renderer,
                     needle_x,
                     needle_y,
                     iroundf_local((float)needle_x - cosf(angle) * 8.0f - sinf(angle) * 5.0f),
                     iroundf_local((float)needle_y - sinf(angle) * 8.0f + cosf(angle) * 5.0f),
                     iroundf_local((float)needle_x - cosf(angle) * 8.0f + sinf(angle) * 5.0f),
                     iroundf_local((float)needle_y - sinf(angle) * 8.0f - cosf(angle) * 5.0f),
                     235, 240, 245, 255);
    filledCircleRGBA(renderer, cx, cy, 5, 235, 240, 245, 255);

    draw_ttf_text_center(renderer, ui->font_medium, cx, cy - radius - 18, title, color_label());
    snprintf(text, sizeof(text), format, value);
    roundedBoxRGBA(renderer, cx - 48, cy + 22, cx + 48, cy + 54, 4, 2, 5, 8, 230);
    roundedRectangleRGBA(renderer, cx - 48, cy + 22, cx + 48, cy + 54, 4, 90, 100, 110, 255);
    draw_ttf_text_center(renderer, ui->font_medium, cx, cy + 25, text, value_color);
}

static void draw_value_box(EICAS2_UI *ui, int cx, int y, float value,
                           const char *format, SDL_Color color)
{
    char text[32];
    SDL_Renderer *renderer = ui->renderer;

    snprintf(text, sizeof(text), format, value);
    roundedBoxRGBA(renderer, cx - 68, y, cx + 68, y + 34, 4, 2, 5, 8, 230);
    roundedRectangleRGBA(renderer, cx - 68, y, cx + 68, y + 34, 4, 90, 100, 110, 255);
    draw_ttf_text_center(renderer, ui->font_medium, cx, y + 4, text, color);
}

static void draw_threshold_marks(EICAS2_UI *ui, int cx, int y, const char *kind)
{
    SDL_Renderer *renderer = ui->renderer;
    int x1 = cx - 82;
    int x2 = cx - 58;

    if (strcmp(kind, "press") == 0) {
        lineRGBA(renderer, x1, y + 23, x2, y + 23, 255, 170, 60, 255);
        draw_ttf_text_center(renderer, ui->font_small, x1 - 14, y + 14, "15", color_warning());
        lineRGBA(renderer, x1, y + 30, x2, y + 30, 255, 70, 70, 255);
        draw_ttf_text_center(renderer, ui->font_small, x1 - 10, y + 30, "0", color_danger());
    } else if (strcmp(kind, "temp") == 0) {
        lineRGBA(renderer, x1, y + 4, x2, y + 4, 255, 70, 70, 255);
        draw_ttf_text_center(renderer, ui->font_small, x1 - 16, y + 2, "100", color_danger());
    } else if (strcmp(kind, "vib") == 0) {
        lineRGBA(renderer, x1, y + 10, x2, y + 10, 255, 170, 60, 255);
        draw_ttf_text_center(renderer, ui->font_small, x1 - 12, y + 6, "80", color_warning());
    }
}

static void draw_param_row(EICAS2_UI *ui, int y, const char *label,
                           float left, float right, const char *format,
                           const char *mark_kind)
{
    SDL_Color left_color = color_main();
    SDL_Color right_color = color_main();

    if (strcmp(label, "OIL PRESS") == 0) {
        if (left <= 0.1f) left_color = color_danger();
        else if (left <= 15.0f) left_color = color_warning();
        if (right <= 0.1f) right_color = color_danger();
        else if (right <= 15.0f) right_color = color_warning();
    } else if (strcmp(label, "OIL TEMP") == 0) {
        if (left >= 100.0f) left_color = color_danger();
        if (right >= 100.0f) right_color = color_danger();
    } else if (strcmp(label, "VIB") == 0) {
        if (left >= 8.0f) left_color = color_warning();
        if (right >= 8.0f) right_color = color_warning();
    }

    draw_value_box(ui, 205, y, left, format, left_color);
    draw_ttf_text_center(ui->renderer, ui->font_medium, 372, y + 5, label, color_label());
    draw_value_box(ui, 540, y, right, format, right_color);
    if (mark_kind && mark_kind[0]) {
        draw_threshold_marks(ui, 205, y, mark_kind);
        draw_threshold_marks(ui, 540, y, mark_kind);
    }
}

static void draw_status(EICAS2_UI *ui, const EICAS2_Data *data)
{
    char text[48];
    snprintf(text, sizeof(text), "FPS %02d  %s", g_fps_value, source_text(data->data_source));
    draw_ttf_text(ui->renderer, ui->font_small, 18, 704, text, color_aux());
}

int EICAS2_UI_Init(EICAS2_UI *ui)
{
    if (!ui) {
        return -1;
    }

    memset(ui, 0, sizeof(*ui));
    ui->window_width = EICAS2_LOGIC_WIDTH;
    ui->window_height = EICAS2_LOGIC_HEIGHT;

    ui->window = SDL_CreateWindow("EICAS2", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                  EICAS2_LOGIC_WIDTH, EICAS2_LOGIC_HEIGHT,
                                  SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!ui->window) {
        printf("EICAS2 window creation failed: %s\n", SDL_GetError());
        return -1;
    }

    ui->renderer = SDL_CreateRenderer(ui->window, -1,
                                      SDL_RENDERER_ACCELERATED |
                                      SDL_RENDERER_PRESENTVSYNC |
                                      SDL_RENDERER_TARGETTEXTURE);
    if (!ui->renderer) {
        printf("EICAS2 renderer creation failed: %s\n", SDL_GetError());
        EICAS2_UI_Destroy(ui);
        return -1;
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
    ui->logic_texture = SDL_CreateTexture(ui->renderer, SDL_PIXELFORMAT_RGBA8888,
                                          SDL_TEXTUREACCESS_TARGET,
                                          EICAS2_LOGIC_WIDTH, EICAS2_LOGIC_HEIGHT);
    if (!ui->logic_texture) {
        printf("EICAS2 logic texture creation failed: %s\n", SDL_GetError());
        EICAS2_UI_Destroy(ui);
        return -1;
    }

    if (load_fonts(ui) != 0) {
        EICAS2_UI_Destroy(ui);
        return -1;
    }

    SDL_SetTextureBlendMode(ui->logic_texture, SDL_BLENDMODE_BLEND);
    EICAS2_UI_UpdateRenderRect(ui);
    return 0;
}

void EICAS2_UI_HandleEvent(EICAS2_UI *ui, SDL_Event *event, int *running)
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
        EICAS2_UI_UpdateRenderRect(ui);
    }
}

void EICAS2_UI_Render(EICAS2_UI *ui, const EICAS2_Data *data)
{
    if (!ui || !data || !ui->renderer || !ui->logic_texture) {
        return;
    }

    update_fps_counter();
    SDL_SetRenderTarget(ui->renderer, ui->logic_texture);
    SDL_SetRenderDrawColor(ui->renderer, 1, 3, 5, 255);
    SDL_RenderClear(ui->renderer);

    draw_ttf_text_center(ui->renderer, ui->font_medium, 372, 22, "EICAS2 SECONDARY ENGINE", color_label());
    draw_ttf_text_center(ui->renderer, ui->font_small, 205, 72, "L", color_label());
    draw_ttf_text_center(ui->renderer, ui->font_small, 540, 72, "R", color_label());
    draw_half_gauge(ui, 205, 160, 72, data->n2_left, 110.0f, "N2", "%04.1f");
    draw_half_gauge(ui, 540, 160, 72, data->n2_right, 110.0f, "N2", "%04.1f");

    draw_param_row(ui, 292, "FF", data->ff_left, data->ff_right, "%04.1f", "");
    draw_param_row(ui, 362, "OIL PRESS", data->oil_press_left, data->oil_press_right, "%04.0f", "press");
    draw_param_row(ui, 432, "OIL TEMP", data->oil_temp_left, data->oil_temp_right, "%04.0f", "temp");
    draw_param_row(ui, 502, "OIL QTY", data->oil_qty_left, data->oil_qty_right, "%04.1f", "");
    draw_param_row(ui, 572, "VIB", data->vib_left, data->vib_right, "%04.2f", "vib");

    draw_ttf_text_center(ui->renderer, ui->font_small, 205, 642, "LEFT ENGINE", color_aux());
    draw_ttf_text_center(ui->renderer, ui->font_small, 540, 642, "RIGHT ENGINE", color_aux());
    draw_status(ui, data);

    rectangleRGBA(ui->renderer, 0, 0, EICAS2_LOGIC_WIDTH - 1, EICAS2_LOGIC_HEIGHT - 1,
                  70, 75, 85, 255);

    SDL_SetRenderTarget(ui->renderer, NULL);
    SDL_SetRenderDrawColor(ui->renderer, 0, 0, 0, 255);
    SDL_RenderClear(ui->renderer);
    SDL_RenderCopy(ui->renderer, ui->logic_texture, NULL, &ui->render_rect);
    SDL_RenderPresent(ui->renderer);
}

void EICAS2_UI_Destroy(EICAS2_UI *ui)
{
    TTF_Font *font_small;
    TTF_Font *font_medium;
    TTF_Font *font_large;

    if (!ui) {
        return;
    }

    font_small = ui->font_small;
    font_medium = ui->font_medium;
    font_large = ui->font_large;

    if (font_small) {
        TTF_CloseFont(font_small);
    }
    if (font_medium && font_medium != font_small) {
        TTF_CloseFont(font_medium);
    }
    if (font_large && font_large != font_small && font_large != font_medium) {
        TTF_CloseFont(font_large);
    }
    ui->font_small = NULL;
    ui->font_medium = NULL;
    ui->font_large = NULL;

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
