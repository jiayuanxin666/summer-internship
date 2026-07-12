#include "fmc_key.h"
#include <SDL2/SDL2_gfxPrimitives.h>
#include <stddef.h>

#define RECT_KEY(key_name, x, y, w, h, text) \
    {key_name, FMC_BUTTON_RECTANGLE, {x, y, w, h}, text}
#define CIRCLE_KEY(key_name, x, y, radius, text) \
    {key_name, FMC_BUTTON_CIRCLE, {x, y, radius, radius}, text}

const FMC_Button fmc_buttons[] = {
    RECT_KEY(FMC_KEY_L1, 6, 118, 47, 35, "L1"),
    RECT_KEY(FMC_KEY_L2, 6, 166, 47, 35, "L2"),
    RECT_KEY(FMC_KEY_L3, 6, 214, 47, 35, "L3"),
    RECT_KEY(FMC_KEY_L4, 6, 263, 47, 35, "L4"),
    RECT_KEY(FMC_KEY_L5, 6, 311, 47, 35, "L5"),
    RECT_KEY(FMC_KEY_L6, 6, 359, 47, 35, "L6"),
    RECT_KEY(FMC_KEY_R1, 587, 118, 47, 35, "R1"),
    RECT_KEY(FMC_KEY_R2, 587, 166, 47, 35, "R2"),
    RECT_KEY(FMC_KEY_R3, 587, 214, 47, 35, "R3"),
    RECT_KEY(FMC_KEY_R4, 587, 263, 47, 35, "R4"),
    RECT_KEY(FMC_KEY_R5, 587, 311, 47, 35, "R5"),
    RECT_KEY(FMC_KEY_R6, 587, 359, 47, 35, "R6"),

    RECT_KEY(FMC_KEY_INIT_REF, 70, 477, 72, 52, "INIT REF"),
    RECT_KEY(FMC_KEY_RTE, 153, 477, 72, 52, "RTE"),
    RECT_KEY(FMC_KEY_CLB, 236, 477, 72, 52, "CLB"),
    RECT_KEY(FMC_KEY_CRZ, 319, 477, 72, 52, "CRZ"),
    RECT_KEY(FMC_KEY_DES, 402, 477, 72, 52, "DES"),
    RECT_KEY(FMC_KEY_DIR_INTC, 70, 536, 72, 50, "DIR INTC"),
    RECT_KEY(FMC_KEY_LEGS, 153, 536, 72, 50, "LEGS"),
    RECT_KEY(FMC_KEY_DEP_ARR, 236, 536, 72, 50, "DEP ARR"),
    RECT_KEY(FMC_KEY_HOLD, 319, 536, 72, 50, "HOLD"),
    RECT_KEY(FMC_KEY_PROG, 402, 536, 72, 50, "PROG"),
    RECT_KEY(FMC_KEY_FIX, 70, 596, 72, 50, "FIX"),
    RECT_KEY(FMC_KEY_NAV_RAD, 153, 596, 72, 50, "NAV RAD"),
    RECT_KEY(FMC_KEY_PREV_PAGE, 70, 656, 72, 50, "PREV PAGE"),
    RECT_KEY(FMC_KEY_NEXT_PAGE, 153, 656, 72, 50, "NEXT PAGE"),
    RECT_KEY(FMC_KEY_EXEC, 499, 550, 70, 36, "EXEC"),

    CIRCLE_KEY(FMC_KEY_1, 87, 763, 25, "1"),
    CIRCLE_KEY(FMC_KEY_2, 150, 763, 25, "2"),
    CIRCLE_KEY(FMC_KEY_3, 213, 763, 25, "3"),
    CIRCLE_KEY(FMC_KEY_4, 87, 824, 25, "4"),
    CIRCLE_KEY(FMC_KEY_5, 150, 824, 25, "5"),
    CIRCLE_KEY(FMC_KEY_6, 213, 824, 25, "6"),
    CIRCLE_KEY(FMC_KEY_7, 87, 886, 25, "7"),
    CIRCLE_KEY(FMC_KEY_8, 150, 886, 25, "8"),
    CIRCLE_KEY(FMC_KEY_9, 213, 886, 25, "9"),
    CIRCLE_KEY(FMC_KEY_PERIOD, 87, 948, 25, "."),
    CIRCLE_KEY(FMC_KEY_0, 150, 948, 25, "0"),
    CIRCLE_KEY(FMC_KEY_PLUS_MINUS, 213, 948, 25, "+/-"),

    RECT_KEY(FMC_KEY_A, 263, 613, 49, 50, "A"),
    RECT_KEY(FMC_KEY_B, 332, 613, 49, 50, "B"),
    RECT_KEY(FMC_KEY_C, 401, 613, 49, 50, "C"),
    RECT_KEY(FMC_KEY_D, 470, 613, 49, 50, "D"),
    RECT_KEY(FMC_KEY_E, 539, 613, 49, 50, "E"),
    RECT_KEY(FMC_KEY_F, 263, 674, 49, 50, "F"),
    RECT_KEY(FMC_KEY_G, 332, 674, 49, 50, "G"),
    RECT_KEY(FMC_KEY_H, 401, 674, 49, 50, "H"),
    RECT_KEY(FMC_KEY_I, 470, 674, 49, 50, "I"),
    RECT_KEY(FMC_KEY_J, 539, 674, 49, 50, "J"),
    RECT_KEY(FMC_KEY_K, 263, 736, 49, 50, "K"),
    RECT_KEY(FMC_KEY_L, 332, 736, 49, 50, "L"),
    RECT_KEY(FMC_KEY_M, 401, 736, 49, 50, "M"),
    RECT_KEY(FMC_KEY_N, 470, 736, 49, 50, "N"),
    RECT_KEY(FMC_KEY_O, 539, 736, 49, 50, "O"),
    RECT_KEY(FMC_KEY_P, 263, 798, 49, 50, "P"),
    RECT_KEY(FMC_KEY_Q, 332, 798, 49, 50, "Q"),
    RECT_KEY(FMC_KEY_R, 401, 798, 49, 50, "R"),
    RECT_KEY(FMC_KEY_S, 470, 798, 49, 50, "S"),
    RECT_KEY(FMC_KEY_T, 539, 798, 49, 50, "T"),
    RECT_KEY(FMC_KEY_U, 263, 860, 49, 50, "U"),
    RECT_KEY(FMC_KEY_V, 332, 860, 49, 50, "V"),
    RECT_KEY(FMC_KEY_W, 401, 860, 49, 50, "W"),
    RECT_KEY(FMC_KEY_X, 470, 860, 49, 50, "X"),
    RECT_KEY(FMC_KEY_Y, 539, 860, 49, 50, "Y"),
    RECT_KEY(FMC_KEY_Z, 263, 922, 49, 50, "Z"),
    RECT_KEY(FMC_KEY_SPACE, 332, 922, 49, 50, "SPACE"),
    RECT_KEY(FMC_KEY_DEL, 401, 922, 49, 50, "DEL"),
    RECT_KEY(FMC_KEY_SLASH, 470, 922, 49, 50, "/"),
    RECT_KEY(FMC_KEY_CLR, 539, 922, 49, 50, "CLR")
};

const int fmc_button_count = (int)(sizeof(fmc_buttons) / sizeof(fmc_buttons[0]));

int FMC_Key_HitTest(const FMC_Button *button, int logical_x, int logical_y)
{
    if (!button) {
        return 0;
    }

    if (button->shape == FMC_BUTTON_CIRCLE) {
        int dx = logical_x - button->rect.x;
        int dy = logical_y - button->rect.y;
        int radius = button->rect.w;
        return dx * dx + dy * dy <= radius * radius;
    }

    return logical_x >= button->rect.x && logical_x < button->rect.x + button->rect.w &&
           logical_y >= button->rect.y && logical_y < button->rect.y + button->rect.h;
}

const FMC_Button *FMC_Key_FindButton(int logical_x, int logical_y)
{
    for (int i = 0; i < fmc_button_count; ++i) {
        if (FMC_Key_HitTest(&fmc_buttons[i], logical_x, logical_y)) {
            return &fmc_buttons[i];
        }
    }
    return NULL;
}

const FMC_Button *FMC_Key_GetButton(FMC_Key key)
{
    for (int i = 0; i < fmc_button_count; ++i) {
        if (fmc_buttons[i].key == key) {
            return &fmc_buttons[i];
        }
    }
    return NULL;
}

void FMC_Key_DrawHover(SDL_Renderer *renderer, const FMC_Button *button)
{
    if (!renderer || !button) {
        return;
    }

    if (button->shape == FMC_BUTTON_CIRCLE) {
        circleRGBA(renderer, button->rect.x, button->rect.y, button->rect.w + 3,
                   245, 245, 245, 255);
        circleRGBA(renderer, button->rect.x, button->rect.y, button->rect.w + 4,
                   245, 245, 245, 180);
    } else {
        rectangleRGBA(renderer,
                      button->rect.x - 2, button->rect.y - 2,
                      button->rect.x + button->rect.w + 1,
                      button->rect.y + button->rect.h + 1,
                      245, 245, 245, 255);
    }
}

const char *FMC_Key_InputText(FMC_Key key)
{
    static const char *letters[] = {
        "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M",
        "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z"
    };
    static const char *digits[] = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9"};

    if (key >= FMC_KEY_A && key <= FMC_KEY_Z) {
        return letters[key - FMC_KEY_A];
    }
    if (key >= FMC_KEY_0 && key <= FMC_KEY_9) {
        return digits[key - FMC_KEY_0];
    }
    switch (key) {
    case FMC_KEY_PERIOD: return ".";
    case FMC_KEY_PLUS_MINUS: return "-";
    case FMC_KEY_SLASH: return "/";
    case FMC_KEY_SPACE: return " ";
    default: return NULL;
    }
}

const char *FMC_Key_XPlaneCommand(FMC_Key key)
{
    static char letter_command[32];
    static char digit_command[32];

    if (key >= FMC_KEY_A && key <= FMC_KEY_Z) {
        letter_command[0] = '\0';
        SDL_snprintf(letter_command, sizeof(letter_command), "sim/FMS/key_%c", 'A' + key - FMC_KEY_A);
        return letter_command;
    }
    if (key >= FMC_KEY_0 && key <= FMC_KEY_9) {
        digit_command[0] = '\0';
        SDL_snprintf(digit_command, sizeof(digit_command), "sim/FMS/key_%d", key - FMC_KEY_0);
        return digit_command;
    }

    switch (key) {
    case FMC_KEY_L1: return "sim/FMS/ls_1l";
    case FMC_KEY_L2: return "sim/FMS/ls_2l";
    case FMC_KEY_L3: return "sim/FMS/ls_3l";
    case FMC_KEY_L4: return "sim/FMS/ls_4l";
    case FMC_KEY_L5: return "sim/FMS/ls_5l";
    case FMC_KEY_L6: return "sim/FMS/ls_6l";
    case FMC_KEY_R1: return "sim/FMS/ls_1r";
    case FMC_KEY_R2: return "sim/FMS/ls_2r";
    case FMC_KEY_R3: return "sim/FMS/ls_3r";
    case FMC_KEY_R4: return "sim/FMS/ls_4r";
    case FMC_KEY_R5: return "sim/FMS/ls_5r";
    case FMC_KEY_R6: return "sim/FMS/ls_6r";
    case FMC_KEY_INIT_REF: return "sim/FMS/init";
    case FMC_KEY_RTE: return "sim/FMS/fpln";
    case FMC_KEY_CLB: return "sim/FMS/clb";
    case FMC_KEY_CRZ: return "sim/FMS/crz";
    case FMC_KEY_DES: return "sim/FMS/des";
    case FMC_KEY_LEGS: return "sim/FMS/legs";
    case FMC_KEY_DEP_ARR: return "sim/FMS/dep_arr";
    case FMC_KEY_PROG: return "sim/FMS/prog";
    case FMC_KEY_EXEC: return "sim/FMS/exec";
    case FMC_KEY_CLR: return "sim/FMS/clear";
    case FMC_KEY_PERIOD: return "sim/FMS/key_period";
    case FMC_KEY_PLUS_MINUS: return "sim/FMS/key_minus";
    case FMC_KEY_SLASH: return "sim/FMS/key_slash";
    default: return NULL;
    }
}
