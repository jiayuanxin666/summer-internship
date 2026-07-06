#include "pfd_instruments.h"
#include "pfd_ui.h"
#include <SDL2/SDL2_gfxPrimitives.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define PFD_PI 3.14159265358979323846f

static float clampf(float value, float min_value, float max_value)
{
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static float deg_to_rad(float deg)
{
    return deg * PFD_PI / 180.0f;
}

static int iroundf(float value)
{
    return (int)(value + (value >= 0.0f ? 0.5f : -0.5f));
}

static float normalize_heading(float heading)
{
    heading = fmodf(heading, 360.0f);
    if (heading < 0.0f) {
        heading += 360.0f;
    }
    return heading;
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

static void local_attitude_point(float cx, float cy, float roll_deg,
                                 float lx, float ly, float pitch_px,
                                 Sint16 *out_x, Sint16 *out_y)
{
    float rad = deg_to_rad(roll_deg);
    float s = sinf(rad);
    float c = cosf(rad);
    float y = ly + pitch_px;
    *out_x = (Sint16)iroundf(cx + lx * c - y * s);
    *out_y = (Sint16)iroundf(cy + lx * s + y * c);
}

static void draw_target_bug(SDL_Renderer *renderer, int x, int y, int direction)
{
    Sint16 xs[3];
    Sint16 ys[3];
    xs[0] = (Sint16)x;
    ys[0] = (Sint16)y;
    xs[1] = (Sint16)(x + direction * 15);
    ys[1] = (Sint16)(y - 8);
    xs[2] = (Sint16)(x + direction * 15);
    ys[2] = (Sint16)(y + 8);
    filledPolygonRGBA(renderer, xs, ys, 3, 210, 70, 255, 255);
}

void PFD_DrawAirspeedIndicator(SDL_Renderer *renderer, const PFD_Data *data)
{
    if (!renderer || !data) {
        return;
    }

    const int x = 34;
    const int y = 145;
    const int w = 112;
    const int h = 370;
    const int center_y = y + h / 2;
    char text[32];
    float current = clampf(data->airspeed_current, 0.0f, 440.0f);
    float target = clampf(data->airspeed_target, 0.0f, 440.0f);
    float min_speed = clampf(current - 50.0f, 0.0f, 440.0f);
    float max_speed = clampf(current + 50.0f, 0.0f, 440.0f);
    int tick_start = ((int)(min_speed / 10.0f)) * 10;
    int tick_end = ((int)(max_speed / 10.0f) + 1) * 10;

    roundedBoxRGBA(renderer, x, y, x + w, y + h, 6, 18, 22, 26, 230);
    roundedRectangleRGBA(renderer, x, y, x + w, y + h, 6, 120, 130, 140, 255);
    stringRGBA(renderer, x + 20, y + 8, "IAS", 230, 230, 230, 255);

    for (int spd = tick_start; spd <= tick_end; spd += 10) {
        if (spd < 0 || spd > 440) {
            continue;
        }
        int ty = center_y - iroundf((float)(spd - current) * (float)h / 100.0f);
        if (ty < y + 22 || ty > y + h - 18) {
            continue;
        }
        int major = (spd % 20) == 0;
        lineRGBA(renderer, x + w - (major ? 34 : 24), ty, x + w - 7, ty,
                 220, 220, 220, 255);
        if (major) {
            snprintf(text, sizeof(text), "%d", spd);
            stringRGBA(renderer, x + 12, ty - 4, text, 230, 230, 230, 255);
        }
    }

    if (target >= min_speed && target <= max_speed) {
        int ty = center_y - iroundf((target - current) * (float)h / 100.0f);
        draw_target_bug(renderer, x + w + 1, ty, -1);
    }

    roundedBoxRGBA(renderer, x + 38, center_y - 21, x + w + 18, center_y + 21,
                   4, 0, 0, 0, 240);
    if (current < 80.0f) {
        roundedRectangleRGBA(renderer, x + 38, center_y - 21, x + w + 18, center_y + 21,
                             4, 255, 210, 40, 255);
    } else {
        roundedRectangleRGBA(renderer, x + 38, center_y - 21, x + w + 18, center_y + 21,
                             4, 255, 255, 255, 255);
    }
    snprintf(text, sizeof(text), "%03d", iroundf(current));
    draw_text_center(renderer, x + 78, center_y - 4, text, 255, 255, 255, 255);

    snprintf(text, sizeof(text), "SEL %03d", iroundf(target));
    stringRGBA(renderer, x + 14, y + h - 24, text, 210, 70, 255, 255);
}

void PFD_DrawAltitudeIndicator(SDL_Renderer *renderer, const PFD_Data *data)
{
    if (!renderer || !data) {
        return;
    }

    const int x = 604;
    const int y = 145;
    const int w = 118;
    const int h = 370;
    const int center_y = y + h / 2;
    char text[32];
    float current = clampf(data->altitude, -1000.0f, 50000.0f);
    float target = clampf(data->altitude_target, -1000.0f, 50000.0f);
    float min_alt = clampf(current - 600.0f, -1000.0f, 50000.0f);
    float max_alt = clampf(current + 600.0f, -1000.0f, 50000.0f);
    int tick_start = ((int)floorf(min_alt / 100.0f)) * 100;
    int tick_end = ((int)ceilf(max_alt / 100.0f)) * 100;

    roundedBoxRGBA(renderer, x, y, x + w, y + h, 6, 18, 22, 26, 230);
    roundedRectangleRGBA(renderer, x, y, x + w, y + h, 6, 120, 130, 140, 255);
    stringRGBA(renderer, x + 30, y + 8, "ALT", 230, 230, 230, 255);

    for (int alt = tick_start; alt <= tick_end; alt += 100) {
        if (alt < -1000 || alt > 50000) {
            continue;
        }
        int ty = center_y - iroundf(((float)alt - current) * (float)h / 1200.0f);
        if (ty < y + 22 || ty > y + h - 18) {
            continue;
        }
        int label = (alt % 200) == 0;
        lineRGBA(renderer, x + 7, ty, x + (label ? 36 : 26), ty,
                 220, 220, 220, 255);
        if (label) {
            snprintf(text, sizeof(text), "%05d", alt);
            stringRGBA(renderer, x + 45, ty - 4, text, 230, 230, 230, 255);
        }
    }

    if (target >= min_alt && target <= max_alt) {
        int ty = center_y - iroundf((target - current) * (float)h / 1200.0f);
        draw_target_bug(renderer, x - 1, ty, 1);
    }

    roundedBoxRGBA(renderer, x - 18, center_y - 21, x + 74, center_y + 21,
                   4, 0, 0, 0, 240);
    roundedRectangleRGBA(renderer, x - 18, center_y - 21, x + 74, center_y + 21,
                         4, 255, 255, 255, 255);
    snprintf(text, sizeof(text), "%05d", iroundf(current));
    draw_text_center(renderer, x + 28, center_y - 4, text, 255, 255, 255, 255);

    snprintf(text, sizeof(text), "SEL %05d", iroundf(target));
    stringRGBA(renderer, x + 18, y + h - 24, text, 210, 70, 255, 255);
}

void PFD_DrawAttitudeIndicator(SDL_Renderer *renderer, const PFD_Data *data)
{
    if (!renderer || !data) {
        return;
    }

    const int x = 166;
    const int y = 88;
    const int w = 438;
    const int h = 432;
    const float cx = (float)(x + w / 2);
    const float cy = (float)(y + h / 2);
    const float roll = 0.0f - clampf(data->roll, -90.0f, 90.0f);
    const float pitch_px = clampf(data->pitch, -45.0f, 45.0f) * 7.0f;
    SDL_Rect clip = {x + 5, y + 5, w - 10, h - 10};
    char text[32];
    Sint16 xs[4];
    Sint16 ys[4];

    roundedBoxRGBA(renderer, x, y, x + w, y + h, 8, 2, 4, 7, 255);
    SDL_RenderSetClipRect(renderer, &clip);

    local_attitude_point(cx, cy, roll, -700.0f, -700.0f, pitch_px, &xs[0], &ys[0]);
    local_attitude_point(cx, cy, roll, 700.0f, -700.0f, pitch_px, &xs[1], &ys[1]);
    local_attitude_point(cx, cy, roll, 700.0f, 0.0f, pitch_px, &xs[2], &ys[2]);
    local_attitude_point(cx, cy, roll, -700.0f, 0.0f, pitch_px, &xs[3], &ys[3]);
    filledPolygonRGBA(renderer, xs, ys, 4, 33, 112, 185, 255);

    local_attitude_point(cx, cy, roll, -700.0f, 0.0f, pitch_px, &xs[0], &ys[0]);
    local_attitude_point(cx, cy, roll, 700.0f, 0.0f, pitch_px, &xs[1], &ys[1]);
    local_attitude_point(cx, cy, roll, 700.0f, 700.0f, pitch_px, &xs[2], &ys[2]);
    local_attitude_point(cx, cy, roll, -700.0f, 700.0f, pitch_px, &xs[3], &ys[3]);
    filledPolygonRGBA(renderer, xs, ys, 4, 116, 78, 47, 255);

    local_attitude_point(cx, cy, roll, -700.0f, 0.0f, pitch_px, &xs[0], &ys[0]);
    local_attitude_point(cx, cy, roll, 700.0f, 0.0f, pitch_px, &xs[1], &ys[1]);
    thickLineRGBA(renderer, xs[0], ys[0], xs[1], ys[1], 3, 255, 255, 255, 255);

    for (float p = -30.0f; p <= 30.0f; p += 2.5f) {
        float ly = -p * 7.0f;
        int abs_tenths = (int)fabsf(p * 10.0f);
        int big = (abs_tenths % 100) == 0;
        int medium = (abs_tenths % 50) == 0;
        float len = big ? 90.0f : (medium ? 58.0f : 30.0f);
        Sint16 x1, y1, x2, y2;
        local_attitude_point(cx, cy, roll, -len, ly, pitch_px, &x1, &y1);
        local_attitude_point(cx, cy, roll, len, ly, pitch_px, &x2, &y2);
        aalineRGBA(renderer, x1, y1, x2, y2, 245, 245, 245, 230);

        if (big && fabsf(p) > 0.1f) {
            Sint16 tx1, ty1, tx2, ty2;
            snprintf(text, sizeof(text), "%d", (int)fabsf(p));
            local_attitude_point(cx, cy, roll, -len - 32.0f, ly - 4.0f, pitch_px, &tx1, &ty1);
            local_attitude_point(cx, cy, roll, len + 16.0f, ly - 4.0f, pitch_px, &tx2, &ty2);
            stringRGBA(renderer, tx1, ty1, text, 245, 245, 245, 230);
            stringRGBA(renderer, tx2, ty2, text, 245, 245, 245, 230);
        }
    }

    SDL_RenderSetClipRect(renderer, NULL);
    roundedRectangleRGBA(renderer, x, y, x + w, y + h, 8, 95, 105, 115, 255);

    thickLineRGBA(renderer, (Sint16)(cx - 80), (Sint16)cy, (Sint16)(cx - 25), (Sint16)cy,
                  4, 255, 230, 120, 255);
    thickLineRGBA(renderer, (Sint16)(cx + 25), (Sint16)cy, (Sint16)(cx + 80), (Sint16)cy,
                  4, 255, 230, 120, 255);
    thickLineRGBA(renderer, (Sint16)(cx - 25), (Sint16)cy, (Sint16)(cx - 25), (Sint16)(cy + 18),
                  4, 255, 230, 120, 255);
    thickLineRGBA(renderer, (Sint16)(cx + 25), (Sint16)cy, (Sint16)(cx + 25), (Sint16)(cy + 18),
                  4, 255, 230, 120, 255);
    filledTrigonRGBA(renderer, (Sint16)cx, (Sint16)(cy - 6),
                     (Sint16)(cx - 8), (Sint16)(cy + 8),
                     (Sint16)(cx + 8), (Sint16)(cy + 8),
                     255, 230, 120, 255);

    for (int mark = -60; mark <= 60; mark += 10) {
        float a = deg_to_rad((float)mark - 90.0f);
        int outer_x = iroundf(cx + cosf(a) * 194.0f);
        int outer_y = iroundf(y + 44 + sinf(a) * 72.0f);
        int inner_x = iroundf(cx + cosf(a) * (mark % 30 == 0 ? 176.0f : 184.0f));
        int inner_y = iroundf(y + 44 + sinf(a) * (mark % 30 == 0 ? 58.0f : 64.0f));
        lineRGBA(renderer, inner_x, inner_y, outer_x, outer_y, 235, 235, 235, 255);
    }
    filledTrigonRGBA(renderer, (Sint16)cx, (Sint16)(y + 22),
                     (Sint16)(cx - 11), (Sint16)(y + 42),
                     (Sint16)(cx + 11), (Sint16)(y + 42),
                     255, 255, 255, 255);

    if (data->agl_altitude <= 2500.0f) {
        snprintf(text, sizeof(text), "RA %04d", iroundf(data->agl_altitude));
        if (data->agl_altitude < 500.0f) {
            draw_text_center(renderer, (int)cx, y + h - 34, text, 255, 235, 80, 255);
        } else {
            draw_text_center(renderer, (int)cx, y + h - 34, text, 235, 245, 255, 255);
        }
    }
}

static int vsi_value_to_y(float vsi, int center_y)
{
    float sign = vsi >= 0.0f ? -1.0f : 1.0f;
    float abs_v = fabsf(clampf(vsi, -6000.0f, 6000.0f));
    float offset;

    if (abs_v <= 2000.0f) {
        offset = abs_v / 2000.0f * 88.0f;
    } else {
        offset = 88.0f + (abs_v - 2000.0f) / 4000.0f * 62.0f;
    }

    return center_y + iroundf(sign * offset);
}

void PFD_DrawVerticalSpeedIndicator(SDL_Renderer *renderer, const PFD_Data *data)
{
    if (!renderer || !data) {
        return;
    }

    const int x = 725;
    const int y = 184;
    const int w = 39;
    const int h = 300;
    const int center_y = y + h / 2;
    char text[32];

    roundedBoxRGBA(renderer, x, y, x + w, y + h, 5, 18, 22, 26, 225);
    roundedRectangleRGBA(renderer, x, y, x + w, y + h, 5, 110, 120, 130, 255);
    lineRGBA(renderer, x + 5, center_y, x + w - 5, center_y, 235, 235, 235, 255);

    int ticks[] = {-6000, -4000, -2000, -1500, -1000, -500, 0, 500, 1000, 1500, 2000, 4000, 6000};
    for (int i = 0; i < (int)(sizeof(ticks) / sizeof(ticks[0])); ++i) {
        int ty = vsi_value_to_y((float)ticks[i], center_y);
        int major = (ticks[i] % 2000) == 0;
        lineRGBA(renderer, x + (major ? 6 : 14), ty, x + w - 5, ty, 220, 220, 220, 255);
        if (major && ticks[i] != 0) {
            snprintf(text, sizeof(text), "%d", abs(ticks[i] / 1000));
            stringRGBA(renderer, x + 4, ty - 4, text, 230, 230, 230, 255);
        }
    }

    int pointer_y = vsi_value_to_y(data->vertical_speed, center_y);
    thickLineRGBA(renderer, x + w - 3, center_y, x + 5, pointer_y, 4, 255, 255, 255, 255);
    filledCircleRGBA(renderer, x + w - 3, center_y, 4, 255, 255, 255, 255);

    snprintf(text, sizeof(text), "%d", iroundf(data->vertical_speed));
    if (data->vertical_speed >= 0.0f) {
        if (fabsf(data->vertical_speed) > 3000.0f) {
            stringRGBA(renderer, x - 10, y - 18, text, 255, 235, 80, 255);
        } else {
            stringRGBA(renderer, x - 10, y - 18, text, 255, 255, 255, 255);
        }
    } else {
        if (fabsf(data->vertical_speed) > 3000.0f) {
            stringRGBA(renderer, x - 10, y + h + 8, text, 255, 235, 80, 255);
        } else {
            stringRGBA(renderer, x - 10, y + h + 8, text, 255, 255, 255, 255);
        }
    }
}

void PFD_DrawHeadingIndicator(SDL_Renderer *renderer, const PFD_Data *data)
{
    if (!renderer || !data) {
        return;
    }

    const int cx = PFD_LOGIC_WIDTH / 2;
    const int cy = 670;
    const int radius = 241;
    char text[32];
    float heading = normalize_heading(data->heading);
    float target = normalize_heading(data->heading_target);

    filledCircleRGBA(renderer, cx, cy, radius, 16, 20, 24, 230);
    circleRGBA(renderer, cx, cy, radius, 115, 125, 135, 255);
    circleRGBA(renderer, cx, cy, radius - 34, 60, 70, 80, 255);

    for (int deg = 0; deg < 360; deg += 5) {
        float rel = normalize_heading((float)deg - heading);
        if (rel > 180.0f) {
            rel -= 360.0f;
        }
        if (rel < -72.0f || rel > 72.0f) {
            continue;
        }

        float a = deg_to_rad(rel - 90.0f);
        int major = (deg % 30) == 0;
        int x1 = iroundf(cx + cosf(a) * (float)(radius - (major ? 36 : 22)));
        int y1 = iroundf(cy + sinf(a) * (float)(radius - (major ? 36 : 22)));
        int x2 = iroundf(cx + cosf(a) * (float)(radius - 5));
        int y2 = iroundf(cy + sinf(a) * (float)(radius - 5));
        lineRGBA(renderer, x1, y1, x2, y2, 230, 230, 230, 255);

        if (major) {
            int label = deg / 10;
            snprintf(text, sizeof(text), "%02d", label);
            draw_text_center(renderer,
                             iroundf(cx + cosf(a) * (float)(radius - 62)),
                             iroundf(cy + sinf(a) * (float)(radius - 62)) - 4,
                             text, 235, 235, 235, 255);
        }
    }

    filledTrigonRGBA(renderer, cx, 511, cx - 12, 536, cx + 12, 536,
                     255, 255, 255, 255);

    float target_rel = normalize_heading(target - heading);
    if (target_rel > 180.0f) {
        target_rel -= 360.0f;
    }
    if (target_rel >= -75.0f && target_rel <= 75.0f) {
        float a = deg_to_rad(target_rel - 90.0f);
        int tx = iroundf(cx + cosf(a) * (float)(radius - 18));
        int ty = iroundf(cy + sinf(a) * (float)(radius - 18));
        circleRGBA(renderer, tx, ty, 8, 210, 70, 255, 255);
        lineRGBA(renderer, tx - 10, ty, tx + 10, ty, 210, 70, 255, 255);
        lineRGBA(renderer, tx, ty - 10, tx, ty + 10, 210, 70, 255, 255);
    }

    roundedBoxRGBA(renderer, cx - 42, 558, cx + 42, 588, 4, 0, 0, 0, 235);
    roundedRectangleRGBA(renderer, cx - 42, 558, cx + 42, 588, 4, 255, 255, 255, 255);
    snprintf(text, sizeof(text), "%03d", iroundf(heading));
    draw_text_center(renderer, cx, 569, text, 255, 255, 255, 255);
    snprintf(text, sizeof(text), "MAG  SEL %03d", iroundf(target));
    draw_text_center(renderer, cx, 594, text, 210, 70, 255, 255);
}

void PFD_DrawThrustIndicator(SDL_Renderer *renderer, const PFD_Data *data)
{
    if (!renderer || !data) {
        return;
    }

    const int cx = 91;
    const int cy = 606;
    const int radius = 60;
    char text[32];
    float throttle = data->throttle;
    throttle = clampf(throttle, 0.0f, 100.0f);

    roundedBoxRGBA(renderer, 28, 536, 154, 682, 6, 18, 22, 26, 225);
    roundedRectangleRGBA(renderer, 28, 536, 154, 682, 6, 110, 120, 130, 255);
    stringRGBA(renderer, 56, 548, "THR", 230, 230, 230, 255);

    for (int pct = 0; pct <= 100; pct += 20) {
        float angle = -225.0f + (float)pct / 20.0f * 45.0f;
        float rad = deg_to_rad(angle);
        int x1 = iroundf(cx + cosf(rad) * (radius - 12));
        int y1 = iroundf(cy + sinf(rad) * (radius - 12));
        int x2 = iroundf(cx + cosf(rad) * radius);
        int y2 = iroundf(cy + sinf(rad) * radius);
        lineRGBA(renderer, x1, y1, x2, y2, 230, 230, 230, 255);
        snprintf(text, sizeof(text), "%d", pct);
        draw_text_center(renderer,
                         iroundf(cx + cosf(rad) * (radius - 28)),
                         iroundf(cy + sinf(rad) * (radius - 28)) - 4,
                         text, 210, 210, 210, 255);
    }

    float needle_angle = -225.0f + throttle / 20.0f * 45.0f;
    float needle_rad = deg_to_rad(needle_angle);
    thickLineRGBA(renderer, cx, cy,
                  iroundf(cx + cosf(needle_rad) * (radius - 10)),
                  iroundf(cy + sinf(needle_rad) * (radius - 10)),
                  4, 255, 255, 255, 255);
    filledCircleRGBA(renderer, cx, cy, 5, 255, 255, 255, 255);

    snprintf(text, sizeof(text), "%03d%%", iroundf(throttle));
    if (throttle > 90.0f) {
        draw_text_center(renderer, cx, 664, text, 255, 235, 80, 255);
    } else {
        draw_text_center(renderer, cx, 664, text, 255, 255, 255, 255);
    }
}

void PFD_DrawFlightModeAnnunciator(SDL_Renderer *renderer, const PFD_Data *data)
{
    if (!renderer || !data) {
        return;
    }

    const int x = 218;
    const int y = 20;
    const int w = 336;
    const int h = 42;
    int roll_active = fabsf(data->roll) > 3.0f;
    int pitch_active = fabsf(data->pitch) > 2.0f;

    roundedBoxRGBA(renderer, x, y, x + w, y + h, 5, 14, 18, 22, 245);
    roundedRectangleRGBA(renderer, x, y, x + w, y + h, 5, 110, 120, 130, 255);
    lineRGBA(renderer, x + w / 3, y, x + w / 3, y + h, 80, 90, 100, 255);
    lineRGBA(renderer, x + 2 * w / 3, y, x + 2 * w / 3, y + h, 80, 90, 100, 255);

    if (roll_active) {
        draw_text_center(renderer, x + w / 6, y + 17, "CWSR", 120, 255, 150, 255);
    } else {
        draw_text_center(renderer, x + w / 6, y + 17, "CWSR", 205, 210, 215, 255);
    }

    if (pitch_active) {
        draw_text_center(renderer, x + w / 2, y + 17, "CWSP", 120, 255, 150, 255);
    } else {
        draw_text_center(renderer, x + w / 2, y + 17, "CWSP", 205, 210, 215, 255);
    }

    draw_text_center(renderer, x + 5 * w / 6, y + 17, "MAG", 205, 210, 215, 255);

    /* TODO_XPLANE: 后期这里接入真实飞行模式、横向模式和纵向模式数据。 */
}
