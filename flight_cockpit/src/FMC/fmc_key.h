#ifndef FMC_KEY_H_INCLUDED
#define FMC_KEY_H_INCLUDED

#include <SDL2/SDL.h>

typedef enum
{
    FMC_BUTTON_RECTANGLE = 0,
    FMC_BUTTON_CIRCLE = 1
} FMC_ButtonShape;

typedef enum
{
    FMC_KEY_NONE = 0,
    FMC_KEY_L1, FMC_KEY_L2, FMC_KEY_L3, FMC_KEY_L4, FMC_KEY_L5, FMC_KEY_L6,
    FMC_KEY_R1, FMC_KEY_R2, FMC_KEY_R3, FMC_KEY_R4, FMC_KEY_R5, FMC_KEY_R6,
    FMC_KEY_INIT_REF, FMC_KEY_RTE, FMC_KEY_CLB, FMC_KEY_CRZ, FMC_KEY_DES,
    FMC_KEY_DIR_INTC, FMC_KEY_LEGS, FMC_KEY_DEP_ARR, FMC_KEY_HOLD, FMC_KEY_PROG,
    FMC_KEY_FIX, FMC_KEY_NAV_RAD, FMC_KEY_PREV_PAGE, FMC_KEY_NEXT_PAGE,
    FMC_KEY_EXEC,
    FMC_KEY_0, FMC_KEY_1, FMC_KEY_2, FMC_KEY_3, FMC_KEY_4,
    FMC_KEY_5, FMC_KEY_6, FMC_KEY_7, FMC_KEY_8, FMC_KEY_9,
    FMC_KEY_PERIOD, FMC_KEY_PLUS_MINUS,
    FMC_KEY_A, FMC_KEY_B, FMC_KEY_C, FMC_KEY_D, FMC_KEY_E,
    FMC_KEY_F, FMC_KEY_G, FMC_KEY_H, FMC_KEY_I, FMC_KEY_J,
    FMC_KEY_K, FMC_KEY_L, FMC_KEY_M, FMC_KEY_N, FMC_KEY_O,
    FMC_KEY_P, FMC_KEY_Q, FMC_KEY_R, FMC_KEY_S, FMC_KEY_T,
    FMC_KEY_U, FMC_KEY_V, FMC_KEY_W, FMC_KEY_X, FMC_KEY_Y,
    FMC_KEY_Z, FMC_KEY_SPACE, FMC_KEY_DEL, FMC_KEY_SLASH, FMC_KEY_CLR,
    FMC_KEY_COUNT
} FMC_Key;

typedef struct
{
    FMC_Key key;
    FMC_ButtonShape shape;
    SDL_Rect rect;
    const char *label;
} FMC_Button;

extern const FMC_Button fmc_buttons[];
extern const int fmc_button_count;

const FMC_Button *FMC_Key_FindButton(int logical_x, int logical_y);
const FMC_Button *FMC_Key_GetButton(FMC_Key key);
int FMC_Key_HitTest(const FMC_Button *button, int logical_x, int logical_y);
void FMC_Key_DrawHover(SDL_Renderer *renderer, const FMC_Button *button);
const char *FMC_Key_InputText(FMC_Key key);
const char *FMC_Key_XPlaneCommand(FMC_Key key);

#endif
