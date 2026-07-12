#include "eicas1_ui.h"
#include <SDL2/SDL2_gfxPrimitives.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define EICAS1_PI 3.14159265358979323846f

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
    case EICAS1_DATA_SOURCE_FILE:
        return "SRC FILE";
    case EICAS1_DATA_SOURCE_XPLANE:
        return "SRC XPLANE";
    case EICAS1_DATA_SOURCE_SIM:
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

static int load_fonts(EICAS1_UI *ui)
{
    ui->font_small = open_consolas_font(15);
    ui->font_medium = open_consolas_font(20);
    ui->font_large = open_consolas_font(28);

    if (!ui->font_small && !ui->font_medium && !ui->font_large) {
        printf("EICAS1 font loading failed: %s\n", TTF_GetError());
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

static void EICAS1_UI_UpdateRenderRect(EICAS1_UI *ui)
{
    float scale_x;
    float scale_y;
    float scale;
    int render_w;
    int render_h;

    if (!ui || ui->window_width <= 0 || ui->window_height <= 0) {
        return;
    }

    scale_x = (float)ui->window_width / (float)EICAS1_LOGIC_WIDTH;
    scale_y = (float)ui->window_height / (float)EICAS1_LOGIC_HEIGHT;
    scale = scale_x < scale_y ? scale_x : scale_y;
    render_w = (int)((float)EICAS1_LOGIC_WIDTH * scale);
    render_h = (int)((float)EICAS1_LOGIC_HEIGHT * scale);
    ui->render_rect.x = (ui->window_width - render_w) / 2;
    ui->render_rect.y = (ui->window_height - render_h) / 2;
    ui->render_rect.w = render_w;
    ui->render_rect.h = render_h;
}

static void draw_tat(EICAS1_UI *ui, const EICAS1_Data *data)
{
    char text[32];
    SDL_Renderer *renderer = ui->renderer;

    draw_ttf_text_center(renderer, ui->font_small, 372, 18, "TAT", color_label());
    snprintf(text, sizeof(text), "%+05.1f C", data->tat);
    draw_ttf_text_center(renderer, ui->font_medium, 372, 38, text, color_main());
}

static void draw_half_gauge(EICAS1_UI *ui, int cx, int cy, int radius,
                            float value, float max_value, const char *title,
                            const char *format)
{
    SDL_Renderer *renderer = ui->renderer;
    char text[32];
    SDL_Color value_color = color_main();
    float pct = value / max_value;
    float angle;
    int needle_x;
    int needle_y;

    if (pct < 0.0f) pct = 0.0f;
    if (pct > 1.0f) pct = 1.0f;

    arcRGBA(renderer, cx, cy, radius, 205, 335, 90, 100, 110, 255);
    arcRGBA(renderer, cx, cy, radius - 1, 205, 335, 90, 100, 110, 255);
    arcRGBA(renderer, cx, cy, radius - 2, 205, 335, 45, 55, 65, 255);

    if (strcmp(title, "N1") == 0 && value > 100.0f) {
        value_color = value > 105.0f ? color_danger() : color_warning();
    } else if (strcmp(title, "EGT C") == 0) {
        if (value > 1000.0f) {
            value_color = color_danger();
        } else if (value > 900.0f) {
            value_color = color_warning();
        }
    }

    for (int i = 0; i <= 10; ++i) {
        float a = (205.0f + (float)i * 13.0f) * EICAS1_PI / 180.0f;
        int major = (i % 5) == 0;
        int numbered = (i % 5) == 0;
        int x1 = iroundf_local((float)cx + cosf(a) * (float)(radius - (major ? 22 : 14)));
        int y1 = iroundf_local((float)cy + sinf(a) * (float)(radius - (major ? 22 : 14)));
        int x2 = iroundf_local((float)cx + cosf(a) * (float)radius);
        int y2 = iroundf_local((float)cy + sinf(a) * (float)radius);
        lineRGBA(renderer, x1, y1, x2, y2, 220, 230, 235, 255);
        if (numbered) {
            int label_value = iroundf_local(max_value * (float)i / 10.0f);
            int tx = iroundf_local((float)cx + cosf(a) * (float)(radius - 30));
            int ty = iroundf_local((float)cy + sinf(a) * (float)(radius - 30)) - 7;
            snprintf(text, sizeof(text), "%d", label_value);
            draw_ttf_text_center(renderer, ui->font_small, tx, ty, text, color_aux());
        }
    }

    angle = (205.0f + pct * 130.0f) * EICAS1_PI / 180.0f;
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

static void draw_value_box(EICAS1_UI *ui, int cx, int y, float value, const char *format)
{
    char text[32];
    SDL_Renderer *renderer = ui->renderer;

    snprintf(text, sizeof(text), format, value);
    roundedBoxRGBA(renderer, cx - 58, y, cx + 58, y + 34, 4, 2, 5, 8, 230);
    roundedRectangleRGBA(renderer, cx - 58, y, cx + 58, y + 34, 4, 90, 100, 110, 255);
    draw_ttf_text_center(renderer, ui->font_medium, cx, y + 4, text, color_main());
}

static void draw_ff(EICAS1_UI *ui, const EICAS1_Data *data)
{
    SDL_Renderer *renderer = ui->renderer;

    draw_ttf_text_center(renderer, ui->font_medium, 282, 476, "FF", color_label());
    draw_ttf_text_center(renderer, ui->font_small, 282, 498, "LB/H x1000", color_aux());
    draw_value_box(ui, 185, 520, data->ff_left, "%04.1f");
    draw_value_box(ui, 380, 520, data->ff_right, "%04.1f");
    draw_ttf_text_center(renderer, ui->font_small, 185, 558, "L", color_label());
    draw_ttf_text_center(renderer, ui->font_small, 380, 558, "R", color_label());
}

static void draw_engine_message(EICAS1_UI *ui, int cx, int y, const char *message, SDL_Color color)
{
    if (message && message[0]) {
        draw_ttf_text_center(ui->renderer, ui->font_small, cx, y, message, color);
    }
}

static void engine_status_text(unsigned int alerts, char *text, size_t size)
{
    const char *separator = "";
    text[0] = '\0';
#define APPEND_ALERT(flag, label) do { if (alerts & (flag)) { \
    snprintf(text + strlen(text), size - strlen(text), "%s%s", separator, (label)); \
    separator = "/"; } } while (0)
    APPEND_ALERT(EICAS1_ALERT_N1_HIGH, "N1");
    APPEND_ALERT(EICAS1_ALERT_EGT_HIGH, "EGT");
    APPEND_ALERT(EICAS1_ALERT_FF_LOW, "FF");
    APPEND_ALERT(EICAS1_ALERT_FUEL_LOW, "FUEL");
#undef APPEND_ALERT
    if (!text[0]) snprintf(text, size, "NORMAL");
}

static void draw_engine_status(EICAS1_UI *ui, const EICAS1_Data *data)
{
    SDL_Renderer *renderer = ui->renderer;
    char left_status[32];
    char right_status[32];

    engine_status_text(data->alert_left, left_status, sizeof(left_status));
    engine_status_text(data->alert_right, right_status, sizeof(right_status));

    roundedBoxRGBA(renderer, 552, 82, 626, 168, 5, 5, 8, 11, 220);
    roundedBoxRGBA(renderer, 638, 82, 712, 168, 5, 5, 8, 11, 220);
    roundedRectangleRGBA(renderer, 552, 82, 626, 168, 5, 90, 100, 110, 255);
    roundedRectangleRGBA(renderer, 638, 82, 712, 168, 5, 90, 100, 110, 255);
    draw_ttf_text_center(renderer, ui->font_small, 589, 98, "ENG1", color_label());
    draw_ttf_text_center(renderer, ui->font_small, 675, 98, "ENG2", color_label());
    lineRGBA(renderer, 560, 122, 618, 122, 70, 82, 94, 255);
    lineRGBA(renderer, 646, 122, 704, 122, 70, 82, 94, 255);
    draw_engine_message(ui, 589, 132, left_status,
                        data->alert_left ? color_warning() : color_main());
    draw_engine_message(ui, 675, 132, right_status,
                        data->alert_right ? color_warning() : color_main());
}

static void draw_fuel(EICAS1_UI *ui, const EICAS1_Data *data)
{
    SDL_Renderer *renderer = ui->renderer;
    char text[32];
    float total = data->fuel_left + data->fuel_center + data->fuel_right;

    roundedBoxRGBA(renderer, 456, 500, 718, 688, 5, 5, 8, 11, 230);
    roundedRectangleRGBA(renderer, 456, 500, 718, 688, 5, 90, 100, 110, 255);
    draw_ttf_text_center(renderer, ui->font_small, 587, 516, "FUEL QTY", color_label());
    draw_ttf_text_center(renderer, ui->font_small, 587, 538, "LBS x 1000", color_aux());

    draw_ttf_text_center(renderer, ui->font_small, 510, 572, "L", color_label());
    draw_ttf_text_center(renderer, ui->font_small, 587, 572, "C", color_label());
    draw_ttf_text_center(renderer, ui->font_small, 664, 572, "R", color_label());
    snprintf(text, sizeof(text), "%.1f", data->fuel_left);
    draw_ttf_text_center(renderer, ui->font_medium, 510, 596, text,
                         data->fuel_left < 1.0f ? color_warning() : color_main());
    snprintf(text, sizeof(text), "%.1f", data->fuel_center);
    draw_ttf_text_center(renderer, ui->font_medium, 587, 596, text,
                         data->fuel_center < 1.0f ? color_warning() : color_main());
    snprintf(text, sizeof(text), "%.1f", data->fuel_right);
    draw_ttf_text_center(renderer, ui->font_medium, 664, 596, text,
                         data->fuel_right < 1.0f ? color_warning() : color_main());
    lineRGBA(renderer, 486, 636, 688, 636, 90, 100, 110, 255);
    snprintf(text, sizeof(text), "TOTAL %.1f", total);
    draw_ttf_text_center(renderer, ui->font_medium, 587, 650, text,
                         total < 5.0f ? color_warning() : color_main());
}

static void draw_status(EICAS1_UI *ui, const EICAS1_Data *data)
{
    char text[48];
    snprintf(text, sizeof(text), "FPS %02d  %s", g_fps_value, source_text(data->data_source));
    draw_ttf_text(ui->renderer, ui->font_small, 18, 704, text, color_aux());
}

int EICAS1_UI_Init(EICAS1_UI *ui)
{
    if (!ui) {
        return -1;
    }

    memset(ui, 0, sizeof(*ui));
    ui->window_width = EICAS1_LOGIC_WIDTH;
    ui->window_height = EICAS1_LOGIC_HEIGHT;

    ui->window = SDL_CreateWindow("EICAS1", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                  EICAS1_LOGIC_WIDTH, EICAS1_LOGIC_HEIGHT,
                                  SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!ui->window) {
        printf("EICAS1 window creation failed: %s\n", SDL_GetError());
        return -1;
    }

    ui->renderer = SDL_CreateRenderer(ui->window, -1,
                                      SDL_RENDERER_ACCELERATED |
                                      SDL_RENDERER_PRESENTVSYNC |
                                      SDL_RENDERER_TARGETTEXTURE);
    ui->vsync_enabled = ui->renderer != NULL;
    if (!ui->renderer) {
        ui->renderer = SDL_CreateRenderer(ui->window, -1,
                                          SDL_RENDERER_ACCELERATED |
                                          SDL_RENDERER_TARGETTEXTURE);
        ui->vsync_enabled = 0;
    }
    if (!ui->renderer) {
        printf("EICAS1 renderer creation failed: %s\n", SDL_GetError());
        EICAS1_UI_Destroy(ui);
        return -1;
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
    ui->logic_texture = SDL_CreateTexture(ui->renderer, SDL_PIXELFORMAT_RGBA8888,
                                          SDL_TEXTUREACCESS_TARGET,
                                          EICAS1_LOGIC_WIDTH, EICAS1_LOGIC_HEIGHT);
    if (!ui->logic_texture) {
        printf("EICAS1 logic texture creation failed: %s\n", SDL_GetError());
        EICAS1_UI_Destroy(ui);
        return -1;
    }

    if (load_fonts(ui) != 0) {
        EICAS1_UI_Destroy(ui);
        return -1;
    }

    SDL_SetTextureBlendMode(ui->logic_texture, SDL_BLENDMODE_BLEND);
    EICAS1_UI_UpdateRenderRect(ui);
    return 0;
}

void EICAS1_UI_HandleEvent(EICAS1_UI *ui, SDL_Event *event, int *running)
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
        EICAS1_UI_UpdateRenderRect(ui);
    }
}

void EICAS1_UI_Render(EICAS1_UI *ui, const EICAS1_Data *data)
{
    if (!ui || !data || !ui->renderer || !ui->logic_texture) {
        return;
    }

    update_fps_counter();
    SDL_SetRenderTarget(ui->renderer, ui->logic_texture);
    SDL_SetRenderDrawColor(ui->renderer, 1, 3, 5, 255);
    SDL_RenderClear(ui->renderer);

    draw_tat(ui, data);
    draw_ttf_text_center(ui->renderer, ui->font_small, 185, 84, "L", color_label());
    draw_ttf_text_center(ui->renderer, ui->font_small, 380, 84, "R", color_label());
    draw_half_gauge(ui, 185, 170, 72, data->n1_left, 110.0f, "N1", "%04.1f");
    draw_half_gauge(ui, 380, 170, 72, data->n1_right, 110.0f, "N1", "%04.1f");
    draw_half_gauge(ui, 185, 374, 72, data->egt_left, 1000.0f, "EGT C", "%04.0f");
    draw_half_gauge(ui, 380, 374, 72, data->egt_right, 1000.0f, "EGT C", "%04.0f");
    draw_ff(ui, data);
    draw_engine_status(ui, data);
    draw_fuel(ui, data);
    draw_status(ui, data);

    rectangleRGBA(ui->renderer, 0, 0, EICAS1_LOGIC_WIDTH - 1, EICAS1_LOGIC_HEIGHT - 1,
                  70, 75, 85, 255);

    SDL_SetRenderTarget(ui->renderer, NULL);
    SDL_SetRenderDrawColor(ui->renderer, 0, 0, 0, 255);
    SDL_RenderClear(ui->renderer);
    SDL_RenderCopy(ui->renderer, ui->logic_texture, NULL, &ui->render_rect);
    SDL_RenderPresent(ui->renderer);
}

void EICAS1_UI_Destroy(EICAS1_UI *ui)
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
