#include "eicas1_ui.h"
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
static SDL_Color color_green(void) { SDL_Color c = {0, 150, 35, 255}; return c; }
static SDL_Color color_amber(void) { SDL_Color c = {255, 188, 0, 255}; return c; }
static SDL_Color color_red(void) { SDL_Color c = {255, 35, 20, 255}; return c; }

static const char *data_source_text(int source)
{
    switch (source) {
    case EICAS1_DATA_SOURCE_XPLANE: return "SRC XPLANE";
    case EICAS1_DATA_SOURCE_FILE: return "SRC FILE";
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

static int load_fonts(EICAS1_UI *ui)
{
    ui->font_small = open_eis_font(15);
    ui->font_medium = open_eis_font(19);
    ui->font_large = open_eis_font(25);
    if (!ui->font_small || !ui->font_medium || !ui->font_large) {
        printf("EICAS1 font loading failed: %s\n", TTF_GetError());
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

static void update_render_rect(EICAS1_UI *ui)
{
    float sx, sy, scale;
    if (!ui || ui->window_width <= 0 || ui->window_height <= 0) return;
    sx = (float)ui->window_width / EICAS1_LOGIC_WIDTH;
    sy = (float)ui->window_height / EICAS1_LOGIC_HEIGHT;
    scale = sx < sy ? sx : sy;
    ui->render_rect.w = (int)(EICAS1_LOGIC_WIDTH * scale);
    ui->render_rect.h = (int)(EICAS1_LOGIC_HEIGHT * scale);
    ui->render_rect.x = (ui->window_width - ui->render_rect.w) / 2;
    ui->render_rect.y = (ui->window_height - ui->render_rect.h) / 2;
}

static void draw_value_box(EICAS1_UI *ui, int cx, int cy, int width,
                           const char *text, SDL_Color color)
{
    int left = cx - width / 2;
    int right = cx + width / 2;
    rectangleRGBA(ui->renderer, left, cy - 11, right, cy + 12, 225, 225, 225, 255);
    draw_text_center(ui->renderer, ui->font_small, cx, cy - 9, text, color);
}

static void draw_limit_mark(EICAS1_UI *ui, int cx, int cy, int radius,
                            float value, float maximum, SDL_Color color)
{
    float a = value / maximum * EICAS_PI;
    int x1 = iroundf_local(cx + cosf(a) * (radius + 5));
    int y1 = iroundf_local(cy + sinf(a) * (radius + 5));
    int x2 = iroundf_local(cx + cosf(a) * (radius - 5));
    int y2 = iroundf_local(cy + sinf(a) * (radius - 5));
    lineRGBA(ui->renderer, x1, y1, x2, y2, color.r, color.g, color.b, 255);
    lineRGBA(ui->renderer, x1 + 1, y1, x2 + 1, y2, color.r, color.g, color.b, 255);
}

static void draw_engine_gauge(EICAS1_UI *ui, int cx, int cy, int radius,
                              float value, float max_value, int is_n1,
                              const char *format)
{
    SDL_Renderer *r = ui->renderer;
    char text[32];
    float pct = value / max_value;
    float angle;
    int px, py;
    SDL_Color value_color = color_white();
    if (pct < 0.0f) pct = 0.0f;
    if (pct > 1.0f) pct = 1.0f;
    angle = pct * 180.0f;

    filledPieRGBA(r, cx, cy, radius - 1, 0, iroundf_local(angle), 61, 61, 61, 255);
    arcRGBA(r, cx, cy, radius, 0, 180, 232, 232, 232, 255);

    /* The 737NG dial is graduated 0, 2, 4, 6, 8, 10 (x10 percent / x100 deg C). */
    for (int step = 0; step <= 10; ++step) {
        float scale_value = is_n1 ? (float)step * 10.0f : (float)step * 100.0f;
        float a = scale_value / max_value * EICAS_PI;
        int major = (step % 2) == 0;
        if (scale_value > max_value) break;
        {
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
    }

    if (is_n1) {
        draw_limit_mark(ui, cx, cy, radius, 100.0f, max_value, color_green());
        draw_limit_mark(ui, cx, cy, radius, 105.0f, max_value, color_red());
    } else {
        draw_limit_mark(ui, cx, cy, radius, 950.0f, max_value, color_red());
    }

    px = iroundf_local(cx + cosf(angle * EICAS_PI / 180.0f) * radius);
    py = iroundf_local(cy + sinf(angle * EICAS_PI / 180.0f) * radius);
    lineRGBA(r, cx, cy, px, py, 245, 245, 245, 255);

    if ((is_n1 && value > 105.0f) || (!is_n1 && value >= 950.0f))
        value_color = color_red();
    else if ((is_n1 && value > 100.0f) || (!is_n1 && value >= 925.0f))
        value_color = color_amber();
    snprintf(text, sizeof(text), format, value);
    draw_value_box(ui, cx + radius, cy - 7, is_n1 ? 67 : 58, text, value_color);
}

static void draw_tat(EICAS1_UI *ui, const EICAS1_Data *data)
{
    char text[32];
    draw_text(ui->renderer, ui->font_small, 50, 14, "TAT", color_cyan());
    snprintf(text, sizeof(text), "%+.1f C", data->tat);
    draw_text(ui->renderer, ui->font_medium, 145, 10, text, color_white());
}

static void draw_engine_messages(EICAS1_UI *ui, const EICAS1_Data *data)
{
    const int lefts[2] = {430, 577};
    const unsigned int alerts[2] = {data->alert_left, data->alert_right};
    const char *names[2] = {"ENG1", "ENG2"};
    for (int i = 0; i < 2; ++i) {
        int x = lefts[i];
        rectangleRGBA(ui->renderer, x, 44, x + 128, 118, 48, 39, 48, 255);
        lineRGBA(ui->renderer, x, 68, x + 128, 68, 48, 39, 48, 255);
        lineRGBA(ui->renderer, x, 93, x + 128, 93, 48, 39, 48, 255);
        draw_text_center(ui->renderer, ui->font_small, x + 64, 27, names[i], color_cyan());
        /* These are the three annunciations fitted to the 737NG primary engine display. */
        if (!((i == 0) ? data->engine_running_left : data->engine_running_right) &&
            ((i == 0) ? data->n1_left : data->n1_right) > 2.0f)
            draw_text_center(ui->renderer, ui->font_small, x + 64, 47, "START VALVE OPEN", color_amber());
        (void)alerts;
        if (!((i == 0) ? data->engine_running_left : data->engine_running_right))
            draw_text_center(ui->renderer, ui->font_small, x + 64, 96, "LOW OIL PRESSURE", color_amber());
    }
}

static void draw_fuel(EICAS1_UI *ui, const EICAS1_Data *data)
{
    char text[32];
    float total = data->fuel_left + data->fuel_center + data->fuel_right;
    SDL_Renderer *r = ui->renderer;
    lineRGBA(r, 439, 580, 468, 580, 0, 160, 170, 255);
    lineRGBA(r, 439, 580, 439, 649, 0, 160, 170, 255);
    lineRGBA(r, 439, 649, 694, 649, 0, 160, 170, 255);
    lineRGBA(r, 694, 580, 694, 649, 0, 160, 170, 255);
    lineRGBA(r, 665, 580, 694, 580, 0, 160, 170, 255);
    draw_text_center(r, ui->font_small, 566, 575, "FUEL QTY-LBS X 1000", color_cyan());
    snprintf(text, sizeof(text), "%.1f", data->fuel_left);
    draw_text_center(r, ui->font_medium, 510, 599, text, color_white());
    snprintf(text, sizeof(text), "%.1f", data->fuel_center);
    draw_text_center(r, ui->font_medium, 566, 599, text, color_white());
    snprintf(text, sizeof(text), "%.1f", data->fuel_right);
    draw_text_center(r, ui->font_medium, 622, 599, text, color_white());
    draw_text(r, ui->font_small, 477, 661, "TOTAL", color_cyan());
    snprintf(text, sizeof(text), "%.1f", total);
    draw_text(r, ui->font_medium, 527, 657, text, color_white());
}

int EICAS1_UI_Init(EICAS1_UI *ui)
{
    if (!ui) return -1;
    memset(ui, 0, sizeof(*ui));
    ui->window_width = EICAS1_LOGIC_WIDTH;
    ui->window_height = EICAS1_LOGIC_HEIGHT;
    ui->window = SDL_CreateWindow("B737-800 UPPER ENGINE DISPLAY",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        EICAS1_LOGIC_WIDTH, EICAS1_LOGIC_HEIGHT, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!ui->window) return -1;
    ui->renderer = SDL_CreateRenderer(ui->window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE);
    ui->vsync_enabled = ui->renderer != NULL;
    if (!ui->renderer) {
        ui->renderer = SDL_CreateRenderer(ui->window, -1,
            SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE);
        ui->vsync_enabled = 0;
    }
    if (!ui->renderer) { EICAS1_UI_Destroy(ui); return -1; }
    ui->logic_texture = SDL_CreateTexture(ui->renderer, SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET, EICAS1_LOGIC_WIDTH, EICAS1_LOGIC_HEIGHT);
    if (!ui->logic_texture || load_fonts(ui) != 0) { EICAS1_UI_Destroy(ui); return -1; }
    SDL_SetTextureBlendMode(ui->logic_texture, SDL_BLENDMODE_BLEND);
    update_render_rect(ui);
    return 0;
}

void EICAS1_UI_HandleEvent(EICAS1_UI *ui, SDL_Event *event, int *running)
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

void EICAS1_UI_Render(EICAS1_UI *ui, const EICAS1_Data *data)
{
    char text[32];
    if (!ui || !data || !ui->renderer || !ui->logic_texture) return;
    SDL_SetRenderTarget(ui->renderer, ui->logic_texture);
    SDL_SetRenderDrawColor(ui->renderer, 0, 0, 0, 255);
    SDL_RenderClear(ui->renderer);
    draw_tat(ui, data);
    draw_engine_gauge(ui, 87, 115, 59, data->n1_left, 110.0f, 1, "%.1f");
    draw_engine_gauge(ui, 306, 115, 59, data->n1_right, 110.0f, 1, "%.1f");
    draw_text_center(ui->renderer, ui->font_medium, 197, 203, "N1", color_cyan());
    draw_engine_gauge(ui, 87, 285, 59, data->egt_left, 1000.0f, 0, "%.0f");
    draw_engine_gauge(ui, 306, 285, 59, data->egt_right, 1000.0f, 0, "%.0f");
    draw_text_center(ui->renderer, ui->font_medium, 197, 361, "EGT", color_cyan());
    snprintf(text, sizeof(text), "%.2f", data->ff_left);
    draw_value_box(ui, 87, 475, 65, text, color_white());
    snprintf(text, sizeof(text), "%.2f", data->ff_right);
    draw_value_box(ui, 306, 475, 65, text, color_white());
    draw_text_center(ui->renderer, ui->font_medium, 197, 489, "FF", color_cyan());
    draw_engine_messages(ui, data);
    draw_fuel(ui, data);
    draw_text(ui->renderer, ui->font_small, 8, EICAS1_LOGIC_HEIGHT - 22,
              data_source_text(data->data_source), color_cyan());
    SDL_SetRenderTarget(ui->renderer, NULL);
    SDL_SetRenderDrawColor(ui->renderer, 0, 0, 0, 255);
    SDL_RenderClear(ui->renderer);
    SDL_RenderCopy(ui->renderer, ui->logic_texture, NULL, &ui->render_rect);
    SDL_RenderPresent(ui->renderer);
}

void EICAS1_UI_Destroy(EICAS1_UI *ui)
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
