#include "nd_ui.h"
#include <SDL2/SDL2_gfxPrimitives.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define ND_PI 3.14159265358979323846f
#define ND_MAP_RANGE_NM 80.0f
#define ND_MAX_MAP_LABELS 24
#define ND_MAX_WAYPOINT_LABELS 8

typedef struct
{
    SDL_Rect rects[ND_MAX_MAP_LABELS];
    int count;
} ND_LabelLayout;

static int g_fps_value = 0;
static int g_fps_frames = 0;
static Uint32 g_fps_last_tick = 0;

static SDL_Color color_main(void)
{
    SDL_Color color = {235, 240, 245, 255};
    return color;
}

static SDL_Color color_label(void)
{
    SDL_Color color = {100, 255, 120, 255};
    return color;
}

static SDL_Color color_speed(void)
{
    SDL_Color color = {120, 255, 160, 255};
    return color;
}

static SDL_Color color_target(void)
{
    SDL_Color color = {210, 70, 255, 255};
    return color;
}

static SDL_Color color_aux(void)
{
    SDL_Color color = {190, 200, 210, 255};
    return color;
}

static float deg_to_rad(float deg)
{
    return deg * ND_PI / 180.0f;
}

static int iroundf_local(float value)
{
    return (int)(value + (value >= 0.0f ? 0.5f : -0.5f));
}

static int text_width(const char *text)
{
    int count = 0;
    while (text && text[count]) {
        ++count;
    }
    return count * 8;
}

static void draw_text_center(SDL_Renderer *renderer, int cx, int y, const char *text,
                             Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    stringRGBA(renderer, cx - text_width(text) / 2, y, text, r, g, b, a);
}

static TTF_Font *open_consolas_font(int size)
{
    const char *paths[] = {
        "assets/fonts/consola.ttf",
        "../assets/fonts/consola.ttf",
        "assets/ALIBABAPUHUITI-2-45-LIGHT.TTF",
        "../assets/ALIBABAPUHUITI-2-45-LIGHT.TTF",
        "C:/Windows/Fonts/consola.ttf",
        "C:/Windows/Fonts/consolab.ttf"
    };

    for (int i = 0; i < (int)(sizeof(paths) / sizeof(paths[0])); ++i) {
        TTF_Font *font = TTF_OpenFont(paths[i], size);
        if (font) {
            return font;
        }
    }

    return NULL;
}

static int load_fonts(ND_UI *ui)
{
    if (!ui) {
        return -1;
    }

    ui->font_small = open_consolas_font(16);
    ui->font_medium = open_consolas_font(20);
    ui->font_large = open_consolas_font(30);
    ui->font_huge = open_consolas_font(36);

    if (!ui->font_small || !ui->font_medium || !ui->font_large || !ui->font_huge) {
        printf("ND font loading failed: %s\n", TTF_GetError());
        return -1;
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

static int draw_ttf_text_right(SDL_Renderer *renderer, TTF_Font *font, int right, int y,
                               const char *text, SDL_Color color)
{
    int w = 0;
    int h = 0;

    if (!font || !text || TTF_SizeUTF8(font, text, &w, &h) != 0) {
        return 0;
    }

    return draw_ttf_text(renderer, font, right - w, y, text, color);
}

static const char *source_text(int source)
{
    switch (source) {
    case ND_DATA_SOURCE_FILE:
        return "SRC FILE";
    case ND_DATA_SOURCE_XPLANE:
        return "SRC XPLANE";
    case ND_DATA_SOURCE_SIM:
    default:
        return "SRC SIM";
    }
}

static const char *point_type_text(ND_PointType type)
{
    switch (type) {
    case ND_POINT_AIRPORT:
        return "APT";
    case ND_POINT_NAVAID:
        return "NAV";
    case ND_POINT_ROUTE:
        return "RTE";
    case ND_POINT_WAYPOINT:
    default:
        return "WPT";
    }
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

static void ND_UI_UpdateRenderRect(ND_UI *ui)
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

    scale_x = (float)ui->window_width / (float)ND_LOGIC_WIDTH;
    scale_y = (float)ui->window_height / (float)ND_LOGIC_HEIGHT;
    scale = scale_x < scale_y ? scale_x : scale_y;

    render_w = (int)((float)ND_LOGIC_WIDTH * scale);
    render_h = (int)((float)ND_LOGIC_HEIGHT * scale);

    ui->render_rect.x = (ui->window_width - render_w) / 2;
    ui->render_rect.y = (ui->window_height - render_h) / 2;
    ui->render_rect.w = render_w;
    ui->render_rect.h = render_h;
}

static void map_project(float cx, float cy, float radius, float distance_nm, float rel_angle_deg,
                        int *out_x, int *out_y)
{
    float distance = distance_nm / ND_MAP_RANGE_NM;
    float angle = deg_to_rad(rel_angle_deg);
    if (distance > 1.0f) {
        distance = 1.0f;
    }
    *out_x = iroundf_local(cx + sinf(angle) * radius * distance);
    *out_y = iroundf_local(cy - cosf(angle) * radius * distance);
}

static int reserve_map_label(ND_LabelLayout *layout, int x, int y, const char *text)
{
    SDL_Rect rect;

    if (!layout || !text || layout->count >= ND_MAX_MAP_LABELS) {
        return 0;
    }

    rect.x = x - 3;
    rect.y = y - 2;
    rect.w = text_width(text) + 6;
    rect.h = 14;
    for (int i = 0; i < layout->count; ++i) {
        if (SDL_HasIntersection(&rect, &layout->rects[i])) {
            return 0;
        }
    }

    layout->rects[layout->count++] = rect;
    return 1;
}

static void draw_top_data(ND_UI *ui, const ND_Data *data)
{
    SDL_Renderer *renderer;
    char text[64];

    if (!ui || !data) {
        return;
    }

    renderer = ui->renderer;

    roundedBoxRGBA(renderer, 10, 10, 174, 86, 5, 5, 8, 11, 210);
    roundedRectangleRGBA(renderer, 10, 10, 174, 86, 5, 70, 82, 94, 255);

    draw_ttf_text(renderer, ui->font_small, 22, 19, "GS", color_label());
    snprintf(text, sizeof(text), "%03d", iroundf_local(data->ground_speed));
    draw_ttf_text_right(renderer, ui->font_medium, 154, 16, text, color_speed());
    draw_ttf_text(renderer, ui->font_small, 22, 43, "TAS", color_label());
    snprintf(text, sizeof(text), "%03d", iroundf_local(data->true_air_speed));
    draw_ttf_text_right(renderer, ui->font_medium, 154, 40, text, color_speed());
    draw_ttf_text(renderer, ui->font_small, 22, 66, "WIND", color_aux());
    snprintf(text, sizeof(text), "---\xC2\xB0/---");
    draw_ttf_text_right(renderer, ui->font_small, 154, 66, text, color_aux());

    roundedBoxRGBA(renderer, 276, 12, 475, 86, 5, 5, 8, 11, 220);
    roundedRectangleRGBA(renderer, 276, 12, 475, 86, 5, 70, 82, 94, 255);
    snprintf(text, sizeof(text), "%03d", iroundf_local(data->heading));
    draw_ttf_text_center(renderer, ui->font_huge, 375, 9, text, color_main());
    draw_ttf_text(renderer, ui->font_small, 306, 57, "TRK", color_aux());
    draw_ttf_text_center(renderer, ui->font_medium, 375, 51, "HDG", color_label());
    draw_ttf_text_right(renderer, ui->font_small, 445, 57, "MAG", color_aux());
    snprintf(text, sizeof(text), "%03d", iroundf_local(data->track));
    draw_ttf_text(renderer, ui->font_small, 306, 72, text, color_speed());
    snprintf(text, sizeof(text), "%03d", iroundf_local(data->heading));
    draw_ttf_text_right(renderer, ui->font_small, 445, 72, text, color_speed());

    roundedBoxRGBA(renderer, 560, 10, 741, 86, 5, 5, 8, 11, 210);
    roundedRectangleRGBA(renderer, 560, 10, 741, 86, 5, 70, 82, 94, 255);
    draw_ttf_text(renderer, ui->font_small, 574, 20, "NEXT WPT", color_aux());
    snprintf(text, sizeof(text), "%s", data->next_waypoint[0] ? data->next_waypoint : "FREDY");
    draw_ttf_text(renderer, ui->font_medium, 574, 40, text, color_target());
    snprintf(text, sizeof(text), "%.1f NM", data->next_distance_nm > 0.1f ? data->next_distance_nm : 1.0f);
    draw_ttf_text(renderer, ui->font_small, 574, 66, "MIN", color_label());
    draw_ttf_text_right(renderer, ui->font_small, 724, 66, text, color_speed());
}

static void draw_plane_symbol(SDL_Renderer *renderer, int cx, int cy)
{
    Sint16 xs[3] = {(Sint16)cx, (Sint16)(cx - 16), (Sint16)(cx + 16)};
    Sint16 ys[3] = {(Sint16)(cy - 28), (Sint16)(cy + 17), (Sint16)(cy + 17)};
    polygonRGBA(renderer, xs, ys, 3, 255, 255, 255, 255);
    lineRGBA(renderer, cx, cy - 28, cx, cy + 32, 255, 255, 255, 255);
    circleRGBA(renderer, cx, cy, 4, 255, 255, 255, 255);
}

static void draw_heading_scale(SDL_Renderer *renderer, const ND_Data *data,
                               int cx, int cy, int radius)
{
    char text[32];

    for (int rel = -60; rel <= 60; rel += 5) {
        float angle = deg_to_rad((float)rel - 90.0f);
        int major = (rel % 10) == 0;
        int x1 = iroundf_local((float)cx + cosf(angle) * (float)(radius - (major ? 22 : 13)));
        int y1 = iroundf_local((float)cy + sinf(angle) * (float)(radius - (major ? 22 : 13)));
        int x2 = iroundf_local((float)cx + cosf(angle) * (float)radius);
        int y2 = iroundf_local((float)cy + sinf(angle) * (float)radius);
        lineRGBA(renderer, x1, y1, x2, y2, 220, 230, 235, 255);

        if (rel % 30 == 0) {
            int heading = (int)(ND_Data_NormalizeHeading(data->heading + (float)rel) / 10.0f + 0.5f);
            if (heading == 36) {
                heading = 0;
            }
            snprintf(text, sizeof(text), "%02d", heading);
            draw_text_center(renderer,
                             iroundf_local((float)cx + cosf(angle) * (float)(radius - 42)),
                             iroundf_local((float)cy + sinf(angle) * (float)(radius - 42)) - 4,
                             text, 235, 240, 245, 255);
        }
    }

    filledTrigonRGBA(renderer, cx, cy - radius + 2, cx - 12, cy - radius + 26,
                     cx + 12, cy - radius + 26, 255, 255, 255, 255);
}

static void draw_target_heading_bug(SDL_Renderer *renderer, const ND_Data *data,
                                    int cx, int cy, int radius)
{
    float rel = ND_Data_AngleDelta(data->heading, data->target_heading);
    float angle;
    int x;
    int y;

    if (rel < -62.0f || rel > 62.0f) {
        return;
    }

    angle = deg_to_rad(rel);
    x = iroundf_local((float)cx + sinf(angle) * (float)(radius - 12));
    y = iroundf_local((float)cy - cosf(angle) * (float)(radius - 12));
    rectangleRGBA(renderer, x - 9, y - 9, x + 9, y + 9, 210, 70, 255, 255);
    lineRGBA(renderer, x - 13, y, x + 13, y, 210, 70, 255, 255);
    lineRGBA(renderer, x, y - 13, x, y + 13, 210, 70, 255, 255);
}

static void draw_range_rings(SDL_Renderer *renderer, int cx, int cy, int radius)
{
    circleRGBA(renderer, cx, cy, radius / 4, 56, 76, 86, 210);
    circleRGBA(renderer, cx, cy, radius / 2, 56, 76, 86, 210);
    circleRGBA(renderer, cx, cy, (radius * 3) / 4, 56, 76, 86, 210);
    arcRGBA(renderer, cx, cy, radius, 210, 330, 115, 130, 140, 255);
    lineRGBA(renderer, cx, cy - radius, cx, cy - 44, 56, 76, 86, 220);
    lineRGBA(renderer, cx - radius, cy, cx + radius, cy, 36, 52, 62, 180);
    draw_text_center(renderer, cx + radius - 28, cy - 12, "80", 185, 198, 205, 255);
    draw_text_center(renderer, cx + radius / 2 - 22, cy - 12, "40", 185, 198, 205, 255);
}

static void draw_route(SDL_Renderer *renderer, const ND_Data *data,
                       int cx, int cy, int radius, ND_LabelLayout *labels)
{
    int prev_valid = 0;
    int prev_x = 0;
    int prev_y = 0;

    for (int i = 0; i < data->route_count; ++i) {
        const ND_MapPoint *point = &data->route[i];
        int x;
        int y;

        if (point->distance_nm > ND_MAP_RANGE_NM || fabsf(point->relative_angle_deg) > 70.0f) {
            continue;
        }

        map_project((float)cx, (float)cy, (float)radius,
                    point->distance_nm, point->relative_angle_deg, &x, &y);

        if (prev_valid) {
            thickLineRGBA(renderer, prev_x, prev_y, x, y, 3, 210, 70, 255, 255);
        }

        filledCircleRGBA(renderer, x, y, 5, 210, 70, 255, 255);
        circleRGBA(renderer, x, y, 9, 210, 70, 255, 255);
        if (reserve_map_label(labels, x + 10, y - 4, point->ident)) {
            stringRGBA(renderer, x + 10, y - 4, point->ident, 235, 220, 255, 255);
        }
        prev_x = x;
        prev_y = y;
        prev_valid = 1;
    }
}

static void draw_nearby_points(SDL_Renderer *renderer, const ND_Data *data,
                               int cx, int cy, int radius, ND_LabelLayout *labels)
{
    char label[64];
    int waypoint_labels = 0;

    for (int i = 0; i < data->nearby_count; ++i) {
        const ND_MapPoint *point = &data->nearby[i];
        int x;
        int y;

        if (point->distance_nm > ND_MAP_RANGE_NM || fabsf(point->relative_angle_deg) > 50.0f) {
            continue;
        }

        map_project((float)cx, (float)cy, (float)radius,
                    point->distance_nm, point->relative_angle_deg, &x, &y);

        if (point->type == ND_POINT_AIRPORT) {
            rectangleRGBA(renderer, x - 6, y - 6, x + 6, y + 6, 120, 220, 255, 255);
        } else if (point->type == ND_POINT_NAVAID) {
            circleRGBA(renderer, x, y, 6, 255, 205, 90, 255);
        } else {
            lineRGBA(renderer, x - 4, y, x + 4, y, 185, 198, 205, 230);
            lineRGBA(renderer, x, y - 4, x, y + 4, 185, 198, 205, 230);
        }
    }

    for (int pass = 0; pass < 3; ++pass) {
        for (int i = 0; i < data->nearby_count; ++i) {
            const ND_MapPoint *point = &data->nearby[i];
            int x;
            int y;
            int label_x;

            if (point->distance_nm > ND_MAP_RANGE_NM || fabsf(point->relative_angle_deg) > 50.0f) {
                continue;
            }
            if ((pass == 0 && point->type != ND_POINT_AIRPORT) ||
                (pass == 1 && point->type != ND_POINT_NAVAID) ||
                (pass == 2 && point->type != ND_POINT_WAYPOINT)) {
                continue;
            }
            if (pass == 2 && waypoint_labels >= ND_MAX_WAYPOINT_LABELS) {
                continue;
            }

            map_project((float)cx, (float)cy, (float)radius,
                        point->distance_nm, point->relative_angle_deg, &x, &y);
            snprintf(label, sizeof(label), "%s", point->ident);
            label_x = x + (pass == 2 ? 7 : 9);
            if (!reserve_map_label(labels, label_x, y - 4, label)) {
                continue;
            }

            if (pass == 0) {
                stringRGBA(renderer, label_x, y - 4, label, 120, 220, 255, 255);
            } else if (pass == 1) {
                stringRGBA(renderer, label_x, y - 4, label, 255, 205, 90, 255);
            } else {
                stringRGBA(renderer, label_x, y - 4, label, 185, 198, 205, 230);
                ++waypoint_labels;
            }
        }
    }
}

static void draw_legend(ND_UI *ui, const ND_Data *data)
{
    SDL_Renderer *renderer;
    char text[64];

    if (!ui || !data) {
        return;
    }

    renderer = ui->renderer;
    roundedBoxRGBA(renderer, 14, 672, 210, 711, 4, 0, 0, 0, 175);
    snprintf(text, sizeof(text), "FPS %02d  %s", g_fps_value, source_text(data->data_source));
    draw_ttf_text(renderer, ui->font_small, 24, 678, text, color_aux());
    snprintf(text, sizeof(text), "LAT %.3f  LON %.3f", data->latitude, data->longitude);
    draw_ttf_text(renderer, ui->font_small, 24, 696, text, color_aux());

    roundedBoxRGBA(renderer, 554, 672, 737, 711, 4, 0, 0, 0, 175);
    draw_ttf_text(renderer, ui->font_small, 566, 678, "APT NAV WPT RTE", color_aux());
    draw_ttf_text(renderer, ui->font_small, 566, 696, point_type_text(ND_POINT_AIRPORT),
                  (SDL_Color){120, 220, 255, 255});
    draw_ttf_text(renderer, ui->font_small, 620, 696, point_type_text(ND_POINT_NAVAID),
                  (SDL_Color){255, 205, 90, 255});
    draw_ttf_text(renderer, ui->font_small, 674, 696, point_type_text(ND_POINT_ROUTE),
                  color_target());
}

static void draw_map(SDL_Renderer *renderer, const ND_Data *data)
{
    const int cx = ND_LOGIC_WIDTH / 2;
    const int cy = 424;
    const int radius = 292;
    SDL_Rect clip = {18, 96, ND_LOGIC_WIDTH - 36, 570};
    ND_LabelLayout labels = {{{0}}, 0};

    roundedBoxRGBA(renderer, 18, 96, ND_LOGIC_WIDTH - 18, 666, 8, 3, 6, 9, 255);
    SDL_RenderSetClipRect(renderer, &clip);

    draw_range_rings(renderer, cx, cy, radius);
    draw_heading_scale(renderer, data, cx, cy, radius);
    draw_target_heading_bug(renderer, data, cx, cy, radius);
    draw_route(renderer, data, cx, cy, radius, &labels);
    draw_nearby_points(renderer, data, cx, cy, radius, &labels);

    thickLineRGBA(renderer, cx, cy - radius + 32, cx, cy - 42, 2, 255, 255, 255, 240);
    draw_plane_symbol(renderer, cx, cy);

    SDL_RenderSetClipRect(renderer, NULL);
    roundedRectangleRGBA(renderer, 18, 96, ND_LOGIC_WIDTH - 18, 666, 8, 70, 82, 94, 255);
}

int ND_UI_Init(ND_UI *ui)
{
    if (!ui) {
        return -1;
    }

    memset(ui, 0, sizeof(*ui));
    ui->window_width = ND_LOGIC_WIDTH;
    ui->window_height = ND_LOGIC_HEIGHT;

    ui->window = SDL_CreateWindow(
        "ND",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        ND_LOGIC_WIDTH,
        ND_LOGIC_HEIGHT,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

    if (!ui->window) {
        printf("ND window creation failed: %s\n", SDL_GetError());
        return -1;
    }

    ui->renderer = SDL_CreateRenderer(
        ui->window,
        -1,
        SDL_RENDERER_ACCELERATED |
        SDL_RENDERER_PRESENTVSYNC |
        SDL_RENDERER_TARGETTEXTURE);

    if (!ui->renderer) {
        printf("ND renderer creation failed: %s\n", SDL_GetError());
        ND_UI_Destroy(ui);
        return -1;
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");

    ui->logic_texture = SDL_CreateTexture(
        ui->renderer,
        SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET,
        ND_LOGIC_WIDTH,
        ND_LOGIC_HEIGHT);

    if (!ui->logic_texture) {
        printf("ND logic texture creation failed: %s\n", SDL_GetError());
        ND_UI_Destroy(ui);
        return -1;
    }

    if (load_fonts(ui) != 0) {
        ND_UI_Destroy(ui);
        return -1;
    }

    SDL_SetTextureBlendMode(ui->logic_texture, SDL_BLENDMODE_BLEND);
    ND_UI_UpdateRenderRect(ui);
    return 0;
}

void ND_UI_HandleEvent(ND_UI *ui, SDL_Event *event, int *running)
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
        ND_UI_UpdateRenderRect(ui);
    }
}

void ND_UI_Render(ND_UI *ui, const ND_Data *data)
{
    if (!ui || !data || !ui->renderer || !ui->logic_texture) {
        return;
    }

    update_fps_counter();

    SDL_SetRenderTarget(ui->renderer, ui->logic_texture);
    SDL_SetRenderDrawColor(ui->renderer, 1, 3, 5, 255);
    SDL_RenderClear(ui->renderer);

    draw_top_data(ui, data);
    draw_map(ui->renderer, data);
    draw_legend(ui, data);

    rectangleRGBA(ui->renderer, 0, 0, ND_LOGIC_WIDTH - 1, ND_LOGIC_HEIGHT - 1,
                  70, 75, 85, 255);

    SDL_SetRenderTarget(ui->renderer, NULL);
    SDL_SetRenderDrawColor(ui->renderer, 0, 0, 0, 255);
    SDL_RenderClear(ui->renderer);

    if (ui->render_rect.w > 0 && ui->render_rect.h > 0) {
        SDL_RenderCopy(ui->renderer, ui->logic_texture, NULL, &ui->render_rect);
    }

    SDL_RenderPresent(ui->renderer);
}

void ND_UI_Destroy(ND_UI *ui)
{
    if (!ui) {
        return;
    }

    if (ui->font_small) {
        TTF_CloseFont(ui->font_small);
        ui->font_small = NULL;
    }

    if (ui->font_medium) {
        TTF_CloseFont(ui->font_medium);
        ui->font_medium = NULL;
    }

    if (ui->font_large) {
        TTF_CloseFont(ui->font_large);
        ui->font_large = NULL;
    }

    if (ui->font_huge) {
        TTF_CloseFont(ui->font_huge);
        ui->font_huge = NULL;
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
