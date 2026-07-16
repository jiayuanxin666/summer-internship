#include "fmc_display.h"
#include <SDL2/SDL_image.h>
#include <SDL2/SDL2_gfxPrimitives.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define FMC_TEXT_CACHE_CAPACITY 128
#define FMC_TEXT_CACHE_TEXT_LEN 96

typedef struct
{
    SDL_Renderer *renderer;
    TTF_Font *font;
    SDL_Texture *texture;
    SDL_Color color;
    int width;
    int height;
    unsigned long stamp;
    char text[FMC_TEXT_CACHE_TEXT_LEN];
} FMC_TextCacheEntry;

static FMC_TextCacheEntry g_text_cache[FMC_TEXT_CACHE_CAPACITY];
static unsigned long g_text_cache_stamp;

static int same_color(SDL_Color left, SDL_Color right)
{
    return left.r == right.r && left.g == right.g && left.b == right.b && left.a == right.a;
}

static void clear_text_cache(void)
{
    for (int i = 0; i < FMC_TEXT_CACHE_CAPACITY; ++i) {
        if (g_text_cache[i].texture) SDL_DestroyTexture(g_text_cache[i].texture);
        memset(&g_text_cache[i], 0, sizeof(g_text_cache[i]));
    }
    g_text_cache_stamp = 0;
}

static FMC_TextCacheEntry *find_text_cache(SDL_Renderer *renderer, TTF_Font *font,
                                           const char *text, SDL_Color color)
{
    FMC_TextCacheEntry *free_entry = NULL;
    FMC_TextCacheEntry *oldest = &g_text_cache[0];
    for (int i = 0; i < FMC_TEXT_CACHE_CAPACITY; ++i) {
        FMC_TextCacheEntry *entry = &g_text_cache[i];
        if (!entry->texture) {
            if (!free_entry) free_entry = entry;
            continue;
        }
        if (entry->renderer == renderer && entry->font == font &&
            same_color(entry->color, color) && strcmp(entry->text, text) == 0) {
            entry->stamp = ++g_text_cache_stamp;
            return entry;
        }
        if (entry->stamp < oldest->stamp) oldest = entry;
    }
    return free_entry ? free_entry : oldest;
}

static SDL_Color color_cyan(void)
{
    return (SDL_Color){35, 225, 220, 255};
}

static SDL_Color color_white(void)
{
    return (SDL_Color){226, 232, 228, 255};
}

static SDL_Color color_green(void)
{
    return (SDL_Color){105, 255, 135, 255};
}

static TTF_Font *open_font(int size)
{
    const char *paths[] = {
        "assets/fonts/consola.ttf",
        "../assets/fonts/consola.ttf",
        "../../assets/fonts/consola.ttf",
        "assets/ALIBABAPUHUITI-2-45-LIGHT.TTF",
        "../assets/ALIBABAPUHUITI-2-45-LIGHT.TTF",
        "../../assets/ALIBABAPUHUITI-2-45-LIGHT.TTF",
        "C:/Windows/Fonts/consola.ttf",
        "C:/Windows/Fonts/arial.ttf"
    };
    for (int i = 0; i < (int)(sizeof(paths) / sizeof(paths[0])); ++i) {
        TTF_Font *font = TTF_OpenFont(paths[i], size);
        if (font) {
            return font;
        }
    }
    return NULL;
}

static int surface_has_background(SDL_Surface *surface)
{
    Uint32 pixel_a;
    Uint32 pixel_b;
    Uint8 r1, g1, b1, a1;
    Uint8 r2, g2, b2, a2;
    if (!surface || surface->w != FMC_LOGICAL_WIDTH || surface->h != FMC_LOGICAL_HEIGHT ||
        surface->format->BytesPerPixel != 4) {
        return 0;
    }
    pixel_a = *(Uint32 *)((Uint8 *)surface->pixels + 500 * surface->pitch + 10 * 4);
    pixel_b = *(Uint32 *)((Uint8 *)surface->pixels + 900 * surface->pitch + 620 * 4);
    SDL_GetRGBA(pixel_a, surface->format, &r1, &g1, &b1, &a1);
    SDL_GetRGBA(pixel_b, surface->format, &r2, &g2, &b2, &a2);
    return a1 > 0 && a2 > 0 && (r1 + g1 + b1) > 10 && (r2 + g2 + b2) > 10;
}

static SDL_Texture *load_texture_path(SDL_Renderer *renderer, const char *path)
{
    for (int attempt = 0; attempt < 3; ++attempt) {
        SDL_Surface *loaded = IMG_Load(path);
        if (loaded) {
            SDL_Surface *surface = SDL_ConvertSurfaceFormat(loaded, SDL_PIXELFORMAT_ARGB8888, 0);
            SDL_FreeSurface(loaded);
            if (surface) {
                SDL_Texture *texture = NULL;
                if (surface_has_background(surface)) {
                    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                                SDL_TEXTUREACCESS_STATIC,
                                                surface->w, surface->h);
                    if (texture && SDL_UpdateTexture(texture, NULL,
                                                     surface->pixels, surface->pitch) != 0) {
                        SDL_DestroyTexture(texture);
                        texture = NULL;
                    }
                }
                SDL_FreeSurface(surface);
                if (texture) {
                    return texture;
                }
            }
        }
        SDL_Delay(20);
    }
    return NULL;
}

static SDL_Texture *load_texture(SDL_Renderer *renderer, const char *name)
{
    char path[1024];
    const char *prefixes[] = {"assets/", "../assets/", "../../assets/"};
    for (int i = 0; i < (int)(sizeof(prefixes) / sizeof(prefixes[0])); ++i) {
        SDL_snprintf(path, sizeof(path), "%s%s", prefixes[i], name);
        SDL_Texture *texture = load_texture_path(renderer, path);
        if (texture) {
            return texture;
        }
    }
    {
        char *base = SDL_GetBasePath();
        if (base) {
            for (int i = 0; i < (int)(sizeof(prefixes) / sizeof(prefixes[0])); ++i) {
                SDL_snprintf(path, sizeof(path), "%s%s%s", base, prefixes[i], name);
                SDL_Texture *texture = load_texture_path(renderer, path);
                if (texture) {
                    SDL_free(base);
                    return texture;
                }
            }
            SDL_free(base);
        }
    }
    return NULL;
}

static int draw_text(SDL_Renderer *renderer, TTF_Font *font, int x, int y,
                     const char *text, SDL_Color color)
{
    FMC_TextCacheEntry *entry;
    SDL_Rect dst;
    if (!renderer || !font || !text || !text[0]) {
        return 0;
    }
    entry = find_text_cache(renderer, font, text, color);
    if (!entry->texture || entry->renderer != renderer || entry->font != font ||
        !same_color(entry->color, color) || strcmp(entry->text, text) != 0) {
        SDL_Surface *surface = TTF_RenderUTF8_Blended(font, text, color);
        if (!surface) return 0;
        if (entry->texture) SDL_DestroyTexture(entry->texture);
        entry->texture = SDL_CreateTextureFromSurface(renderer, surface);
        entry->renderer = renderer;
        entry->font = font;
        entry->color = color;
        entry->width = surface->w;
        entry->height = surface->h;
        SDL_snprintf(entry->text, sizeof(entry->text), "%s", text);
        entry->stamp = ++g_text_cache_stamp;
        SDL_FreeSurface(surface);
        if (!entry->texture) return 0;
    }
    dst = (SDL_Rect){x, y, entry->width, entry->height};
    SDL_RenderCopy(renderer, entry->texture, NULL, &dst);
    return 1;
}

static int text_size(TTF_Font *font, const char *text, int *width, int *height)
{
    if (!font || !text || TTF_SizeUTF8(font, text, width, height) != 0) {
        if (width) *width = 0;
        if (height) *height = 0;
        return 0;
    }
    return 1;
}

static void draw_text_center(SDL_Renderer *renderer, TTF_Font *font, int center_x, int y,
                             const char *text, SDL_Color color)
{
    int width = 0;
    int height = 0;
    text_size(font, text, &width, &height);
    draw_text(renderer, font, center_x - width / 2, y, text, color);
}

static void draw_text_right(SDL_Renderer *renderer, TTF_Font *font, int right_x, int y,
                            const char *text, SDL_Color color)
{
    int width = 0;
    int height = 0;
    text_size(font, text, &width, &height);
    draw_text(renderer, font, right_x - width, y, text, color);
}

static void draw_title(FMC_Display *display, const char *title)
{
    draw_text_center(display->renderer, display->font_normal, 320, 91, title, color_cyan());
}

static int row_label_y(int row)
{
    return 116 + row * 48;
}

static int row_value_y(int row)
{
    return 132 + row * 48;
}

static void draw_left_pair(FMC_Display *display, int row, const char *label, const char *value)
{
    if (label) draw_text(display->renderer, display->font_small, 188, row_label_y(row), label, color_cyan());
    if (value) draw_text(display->renderer, display->font_normal, 188, row_value_y(row), value, color_white());
}

static void draw_right_pair(FMC_Display *display, int row, const char *label, const char *value)
{
    if (label) draw_text_right(display->renderer, display->font_small, 456, row_label_y(row), label, color_cyan());
    if (value) draw_text_right(display->renderer, display->font_normal, 456, row_value_y(row), value, color_white());
}

static void draw_page_counter(FMC_Display *display, int current, int total)
{
    char text[24];
    SDL_snprintf(text, sizeof(text), "%d/%d", current, total);
    draw_text_right(display->renderer, display->font_normal, 466, 91, text, color_cyan());
}

static void draw_scratchpad(FMC_Display *display, const FMC_Data *data)
{
    const char *text = FMC_Data_ScratchpadText(data);
    for (int x = 188; x < 457; x += 7) {
        lineRGBA(display->renderer, x, 359, x + 3, 359, 30, 220, 215, 255);
    }
    if (text && text[0]) {
        SDL_Color color = data->message[0] ? color_green() : color_white();
        draw_text(display->renderer, display->font_normal, 188, 382, text, color);
    }
}

static void draw_index(FMC_Display *display)
{
    draw_title(display, "INDEX");
    draw_text(display->renderer, display->font_normal, 188, row_value_y(0), "<STATUS", color_white());
    draw_text_right(display->renderer, display->font_normal, 456, row_value_y(0), "ROUTE MENU>", color_white());
    draw_text_right(display->renderer, display->font_normal, 456, row_value_y(1), "DATABASE>", color_white());
    draw_text_right(display->renderer, display->font_normal, 456, row_value_y(5), "ARR DATE>", color_white());
}

static void draw_status(FMC_Display *display)
{
    char utc_text[16] = "--:--";
    char date_text[16] = "-------";
    time_t now = time(NULL);
    struct tm utc_time;
#if defined(_WIN32)
    gmtime_s(&utc_time, &now);
#else
    gmtime_r(&now, &utc_time);
#endif
    strftime(utc_text, sizeof(utc_text), "%H:%M", &utc_time);
    strftime(date_text, sizeof(date_text), "%d%b%y", &utc_time);
    for (char *ch = date_text; *ch; ++ch) *ch = (char)toupper((unsigned char)*ch);

    draw_title(display, "STATUS");
    draw_left_pair(display, 0, "NAV DATA", "WORLD (XPLANE)");
    draw_left_pair(display, 1, "ACTIVE DATABASE", "01FEB18 01MAR18");
    draw_left_pair(display, 2, "SEC DATABASE", "NOT AVAIL");
    draw_left_pair(display, 3, "UTC", utc_text);
    draw_right_pair(display, 3, "DATE", date_text);
    draw_left_pair(display, 4, "PROGRAM", "X-PLANE 11.55r2");
    draw_text(display->renderer, display->font_normal, 188, row_value_y(5), "<INDEX", color_white());
    draw_text_right(display->renderer, display->font_normal, 456, row_value_y(5), "DATABASE>", color_white());
}

static void draw_route(FMC_Display *display, const FMC_State *state)
{
    const FMC_Data *data = &state->data;
    int total = FMC_Event_RoutePageCount(state);
    draw_title(display, "ACT FPLN");
    draw_page_counter(display, state->route_page, total);

    if (state->route_page == 1) {
        draw_left_pair(display, 0, "ORIGIN", data->has_origin ? data->origin.ident : "----");
        draw_right_pair(display, 0, "DESTINATION", data->has_destination ? data->destination.ident : "----");
        draw_left_pair(display, 1, "CO ROUTE", "----");
        draw_right_pair(display, 1, "FLT_NO", data->flight_number[0] ? data->flight_number : "----");
        draw_left_pair(display, 4, "VIA", data->route_count > 0 ? data->route[0].via : "----");
        draw_right_pair(display, 4, "TO", data->route_count > 0 ? data->route[0].waypoint.ident : "----");
        draw_text(display->renderer, display->font_normal, 188, row_value_y(5), "<ROUTE MENU", color_white());
        draw_text_right(display->renderer, display->font_normal, 456, row_value_y(5), "VNAV>", color_white());
    } else {
        int start = (state->route_page - 2) * 5;
        for (int row = 0; row < 5; ++row) {
            int index = start + row;
            if (index < data->route_count) {
                draw_left_pair(display, row, row == 0 ? "VIA" : NULL, data->route[index].via);
                draw_right_pair(display, row, row == 0 ? "TO" : NULL, data->route[index].waypoint.ident);
            }
        }
    }
}

static void draw_vnav(FMC_Display *display, const FMC_State *state)
{
    const FMC_Data *data = &state->data;
    char speed[24];
    char limit[24];
    if (state->page == FMC_PAGE_CLIMB) {
        SDL_snprintf(speed, sizeof(speed), "%s/%s", data->climb_speed_low, data->climb_speed_mach);
        SDL_snprintf(limit, sizeof(limit), "%s/%s", data->speed_limit, data->speed_limit_altitude);
        draw_title(display, "ACT VNAV CLIMB");
        draw_page_counter(display, 1, 3);
        draw_left_pair(display, 0, "TGT SPEED", speed);
        draw_right_pair(display, 0, "TRANS ALT", data->climb_transition_altitude);
        draw_left_pair(display, 1, "SPD/ALT LIMIT", limit);
        draw_left_pair(display, 2, NULL, "--/-----");
    } else if (state->page == FMC_PAGE_CRUISE) {
        draw_title(display, "VNAV CRUISE");
        draw_page_counter(display, 2, 3);
        draw_left_pair(display, 0, "TGT SPEED", data->cruise_speed);
        draw_right_pair(display, 0, "CRZ ALT", data->cruise_altitude);
    } else {
        SDL_snprintf(limit, sizeof(limit), "%s/%s", data->speed_limit, data->speed_limit_altitude);
        draw_title(display, "VNAV DESCENT");
        draw_page_counter(display, 3, 3);
        draw_left_pair(display, 0, "TGT SPEED", data->descent_speed);
        draw_right_pair(display, 0, "TRANS FL", data->descent_transition_level);
        draw_left_pair(display, 1, "SPD/ALT LIMIT", limit);
        draw_left_pair(display, 2, NULL, "--/-----");
        draw_right_pair(display, 2, "VPA", data->descent_vpa);
    }
}

static void draw_dep_arr_index(FMC_Display *display, const FMC_Data *data)
{
    const char *origin = data->has_origin ? data->origin.ident : "KSEA";
    const char *destination = data->has_destination ? data->destination.ident : "KBFI";
    draw_title(display, "DEP/ARR INDEX");
    draw_text_center(display->renderer, display->font_small, 320, 111, "ACT FPLN", color_cyan());
    draw_text(display->renderer, display->font_normal, 188, row_value_y(0), "<DEP", color_white());
    draw_text_center(display->renderer, display->font_normal, 320, row_value_y(0), origin, color_white());
    draw_text_right(display->renderer, display->font_normal, 456, row_value_y(0), "ARR>", color_white());
    draw_text_center(display->renderer, display->font_normal, 320, row_value_y(1), destination, color_white());
    draw_text_right(display->renderer, display->font_normal, 456, row_value_y(1), "ARR>", color_white());
}

static void draw_option_left(FMC_Display *display, int row, const char *label,
                             const char *option, int selected)
{
    char text[40];
    if (!option) {
        draw_left_pair(display, row, label, NULL);
        return;
    }
    SDL_snprintf(text, sizeof(text), "%s%s", option, selected ? " <SEL>" : "");
    draw_left_pair(display, row, label, text);
}

static void draw_option_right(FMC_Display *display, int row, const char *label,
                              const char *option, int selected)
{
    char text[40];
    if (!option) {
        draw_right_pair(display, row, label, NULL);
        return;
    }
    SDL_snprintf(text, sizeof(text), "%s%s", option, selected ? " <SEL>" : "");
    draw_right_pair(display, row, label, text);
}

static void draw_procedure(FMC_Display *display, const FMC_State *state, int arrival)
{
    const FMC_Data *data = &state->data;
    const char *airport = arrival
        ? (state->procedure_airport_is_origin
               ? (data->has_origin ? data->origin.ident : "KSEA")
               : (data->has_destination ? data->destination.ident : "KBFI"))
        : (data->has_origin ? data->origin.ident : "KSEA");
    const FMC_ProcedureCatalog *catalog = FMC_Data_GetProcedureCatalogForAirport(airport);
    char title[48];
    int selected_procedure = arrival ? data->selected_arrival_procedure : data->selected_departure_procedure;
    int selected_runway = arrival ? data->selected_arrival_runway : data->selected_departure_runway;
    int selected_transition = arrival ? data->selected_arrival_transition : data->selected_departure_transition;
    const char *const *procedures = arrival ? catalog->arrival_procedures : catalog->departure_procedures;
    const char *const *runways = arrival ? catalog->arrival_runways : catalog->departure_runways;
    const char *const *transitions = arrival ? catalog->arrival_transitions : catalog->departure_transitions;

    SDL_snprintf(title, sizeof(title), "%s %s", airport, arrival ? "ARRIVAL" : "DEPART");
    draw_title(display, title);
    draw_text_center(display->renderer, display->font_small, 320, 111, "MOD FPLN", color_cyan());
    draw_page_counter(display, 1, 1);

    for (int i = 0; i < 3; ++i) {
        const char *procedure = procedures[i];
        const char *runway = runways[i];
        if (selected_runway >= 0 &&
            !FMC_Data_ProcedureCompatible(catalog, arrival, i, selected_runway)) {
            procedure = NULL;
        }
        if (selected_procedure >= 0 &&
            !FMC_Data_ProcedureCompatible(catalog, arrival, selected_procedure, i)) {
            runway = NULL;
        }
        draw_option_left(display, i, i == 0 ? (arrival ? "STARS" : "SIDS") : NULL,
                         procedure, selected_procedure == i);
        draw_option_right(display, i, i == 0 ? (arrival ? "APPR" : "RWYS") : NULL,
                          runway, selected_runway == i);
    }
    if (selected_procedure >= 0 || selected_runway >= 0) {
        draw_option_left(display, 3, "TRANS", transitions[0], selected_transition == 0);
        draw_option_right(display, 3, "TRANS", transitions[1], selected_transition == 1);
    }
}

int FMC_Display_Init(FMC_Display *display)
{
    if (!display) {
        return -1;
    }
    memset(display, 0, sizeof(*display));
    display->window_width = FMC_LOGICAL_WIDTH;
    display->window_height = FMC_LOGICAL_HEIGHT;

    display->window = SDL_CreateWindow("FMC",
                                       SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                       FMC_LOGICAL_WIDTH, FMC_LOGICAL_HEIGHT,
                                       SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!display->window) {
        printf("FMC window creation failed: %s\n", SDL_GetError());
        return -1;
    }
    display->renderer = SDL_CreateRenderer(display->window, -1,
                                            SDL_RENDERER_ACCELERATED |
                                            SDL_RENDERER_PRESENTVSYNC |
                                            SDL_RENDERER_TARGETTEXTURE);
    if (!display->renderer) {
        display->renderer = SDL_CreateRenderer(display->window, -1,
                                                SDL_RENDERER_SOFTWARE |
                                                SDL_RENDERER_TARGETTEXTURE);
    }
    if (!display->renderer) {
        printf("FMC renderer creation failed: %s\n", SDL_GetError());
        FMC_Display_Destroy(display);
        return -1;
    }
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
    display->logic_texture = SDL_CreateTexture(display->renderer,
                                                SDL_PIXELFORMAT_ARGB8888,
                                                SDL_TEXTUREACCESS_TARGET,
                                                FMC_LOGICAL_WIDTH, FMC_LOGICAL_HEIGHT);
    display->background_texture = load_texture(display->renderer, "fmc.png");
    display->font_small = open_font(13);
    display->font_normal = open_font(17);
    display->font_large = open_font(20);
    if (!display->logic_texture || !display->background_texture ||
        !display->font_small || !display->font_normal || !display->font_large) {
        printf("FMC resource loading failed: %s / %s\n", SDL_GetError(), TTF_GetError());
        FMC_Display_Destroy(display);
        return -1;
    }
    FMC_Display_UpdateSize(display, display->window_width, display->window_height);
    return 0;
}

void FMC_Display_UpdateSize(FMC_Display *display, int width, int height)
{
    float scale_x;
    float scale_y;
    float scale;
    if (!display || width <= 0 || height <= 0) {
        return;
    }
    display->window_width = width;
    display->window_height = height;
    scale_x = (float)width / (float)FMC_LOGICAL_WIDTH;
    scale_y = (float)height / (float)FMC_LOGICAL_HEIGHT;
    scale = scale_x < scale_y ? scale_x : scale_y;
    display->render_rect.w = (int)(FMC_LOGICAL_WIDTH * scale);
    display->render_rect.h = (int)(FMC_LOGICAL_HEIGHT * scale);
    display->render_rect.x = (width - display->render_rect.w) / 2;
    display->render_rect.y = (height - display->render_rect.h) / 2;
}

int FMC_Display_WindowToLogical(const FMC_Display *display, int window_x, int window_y,
                                int *logical_x, int *logical_y)
{
    if (!display || display->render_rect.w <= 0 || display->render_rect.h <= 0 ||
        window_x < display->render_rect.x || window_y < display->render_rect.y ||
        window_x >= display->render_rect.x + display->render_rect.w ||
        window_y >= display->render_rect.y + display->render_rect.h) {
        return 0;
    }
    if (logical_x) {
        *logical_x = (window_x - display->render_rect.x) * FMC_LOGICAL_WIDTH / display->render_rect.w;
    }
    if (logical_y) {
        *logical_y = (window_y - display->render_rect.y) * FMC_LOGICAL_HEIGHT / display->render_rect.h;
    }
    return 1;
}

void FMC_Display_Render(FMC_Display *display, const FMC_State *state)
{
    if (!display || !state || !display->renderer || !display->logic_texture) {
        return;
    }
    SDL_SetRenderTarget(display->renderer, display->logic_texture);
    SDL_SetRenderDrawColor(display->renderer, 0, 0, 0, 255);
    SDL_RenderClear(display->renderer);
    SDL_RenderCopy(display->renderer, display->background_texture, NULL, NULL);

    switch (state->page) {
    case FMC_PAGE_INDEX: draw_index(display); break;
    case FMC_PAGE_STATUS: draw_status(display); break;
    case FMC_PAGE_ROUTE: draw_route(display, state); break;
    case FMC_PAGE_CLIMB:
    case FMC_PAGE_CRUISE:
    case FMC_PAGE_DESCENT: draw_vnav(display, state); break;
    case FMC_PAGE_DEP_ARR_INDEX: draw_dep_arr_index(display, &state->data); break;
    case FMC_PAGE_DEPARTURE: draw_procedure(display, state, 0); break;
    case FMC_PAGE_ARRIVAL: draw_procedure(display, state, 1); break;
    case FMC_PAGE_BLANK:
    default: break;
    }

    if (state->page != FMC_PAGE_BLANK) {
        draw_scratchpad(display, &state->data);
    }
    if (state->data.exec_pending) {
        boxRGBA(display->renderer, 514, 515, 554, 521, 30, 225, 230, 255);
    }
    if (state->hovered_button) {
        FMC_Key_DrawHover(display->renderer, state->hovered_button);
    }

    SDL_SetRenderTarget(display->renderer, NULL);
    SDL_SetRenderDrawColor(display->renderer, 0, 0, 0, 255);
    SDL_RenderClear(display->renderer);
    SDL_RenderCopy(display->renderer, display->logic_texture, NULL, &display->render_rect);
    SDL_RenderPresent(display->renderer);
}

int FMC_Display_SaveScreenshot(FMC_Display *display, const char *path)
{
    SDL_Surface *surface;
    int result;
    if (!display || !path || !display->renderer || !display->logic_texture) {
        return 0;
    }
    surface = SDL_CreateRGBSurfaceWithFormat(0, FMC_LOGICAL_WIDTH, FMC_LOGICAL_HEIGHT,
                                             32, SDL_PIXELFORMAT_ARGB8888);
    if (!surface) {
        return 0;
    }
    /* Read from the logical render target so screenshots remain 638x998
     * regardless of the current window size and letterboxing. */
    SDL_SetRenderTarget(display->renderer, display->logic_texture);
    result = SDL_RenderReadPixels(display->renderer, NULL, SDL_PIXELFORMAT_ARGB8888,
                                  surface->pixels, surface->pitch);
    SDL_SetRenderTarget(display->renderer, NULL);
    if (result == 0) {
        result = SDL_SaveBMP(surface, path);
    }
    SDL_FreeSurface(surface);
    return result == 0;
}

void FMC_Display_Destroy(FMC_Display *display)
{
    if (!display) {
        return;
    }
    if (display->font_small) TTF_CloseFont(display->font_small);
    if (display->font_normal) TTF_CloseFont(display->font_normal);
    if (display->font_large) TTF_CloseFont(display->font_large);
    clear_text_cache();
    if (display->background_texture) SDL_DestroyTexture(display->background_texture);
    if (display->logic_texture) SDL_DestroyTexture(display->logic_texture);
    if (display->renderer) SDL_DestroyRenderer(display->renderer);
    if (display->window) SDL_DestroyWindow(display->window);
    memset(display, 0, sizeof(*display));
}
