#include "cockpit.h"

#include <SDL2/SDL_image.h>
#include <SDL2/SDL_syswm.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Util/cockpit_alarm.h"

#ifdef _WIN32
#include <windows.h>
#endif

#define COCKPIT_MAX_SCALE 1.5f
#define COCKPIT_ZOOM_STEP 1.12f
#define COCKPIT_MODULE_START_TIMEOUT_MS 5000u

typedef struct
{
    const char *name;
    const char *executable;
    int x;
    int y;
    int width;
    int height;
#ifdef _WIN32
    PROCESS_INFORMATION process;
    HWND window;
#endif
    int launched;
    int visible;
} CockpitModule;

struct CockpitApp
{
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *background;
    int window_width;
    int window_height;
    int image_width;
    int image_height;
    int renderer_vsync;
    float scale;
    float offset_x;
    float offset_y;
    int dragging;
    int drag_x;
    int drag_y;
    float drag_offset_x;
    float drag_offset_y;
    int embed_modules;
    int smoke;
    int layout_dirty;
    CockpitAlarm *alarm;
#ifdef _WIN32
    HWND native_window;
#endif
    CockpitModule modules[COCKPIT_MODULE_COUNT];
};

static const CockpitModule g_module_templates[COCKPIT_MODULE_COUNT] = {
    {.name = "PFD captain", .executable = "PFD.exe", .x = 1296, .y = 946, .width = 771, .height = 721},
    {.name = "ND captain", .executable = "ND.exe", .x = 2205, .y = 946, .width = 751, .height = 721},
    {.name = "EICAS1", .executable = "EICAS1_XPlane.exe", .x = 3621, .y = 942, .width = 745, .height = 728},
    {.name = "EICAS2", .executable = "EICAS2_XPlane.exe", .x = 3550, .y = 1908, .width = 771, .height = 781},
    {.name = "ND first officer", .executable = "ND.exe", .x = 4786, .y = 946, .width = 751, .height = 721},
    {.name = "PFD first officer", .executable = "PFD.exe", .x = 5702, .y = 946, .width = 771, .height = 721},
    {.name = "FMC captain", .executable = "FMC.exe", .x = 2769, .y = 1817, .width = 636, .height = 998},
    {.name = "FMC first officer", .executable = "FMC.exe", .x = 4422, .y = 1817, .width = 636, .height = 998}
};

static float clamp_float(float value, float minimum, float maximum)
{
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}

static float minimum_scale(const CockpitApp *app)
{
    if (!app || app->window_width <= 0 || app->image_width <= 0) return 1.0f;
    return (float)app->window_width / (float)app->image_width;
}

static float maximum_scale(const CockpitApp *app)
{
    float minimum = minimum_scale(app);
    return minimum > COCKPIT_MAX_SCALE ? minimum : COCKPIT_MAX_SCALE;
}

static void update_scale_limits(CockpitApp *app)
{
    float minimum;
    float maximum;
    float image_width;
    float image_height;
    if (!app || app->window_width <= 0 || app->window_height <= 0 ||
        app->image_width <= 0 || app->image_height <= 0) return;

    minimum = minimum_scale(app);
    maximum = maximum_scale(app);
    app->scale = clamp_float(app->scale, minimum, maximum);
    image_width = app->image_width * app->scale;
    image_height = app->image_height * app->scale;

    if (image_width <= app->window_width) {
        app->offset_x = (app->window_width - image_width) * 0.5f;
    } else {
        app->offset_x = clamp_float(app->offset_x, app->window_width - image_width, 0.0f);
    }
    if (image_height <= app->window_height) {
        app->offset_y = (app->window_height - image_height) * 0.5f;
    } else {
        app->offset_y = clamp_float(app->offset_y, app->window_height - image_height, 0.0f);
    }
    app->layout_dirty = 1;
}

static void reset_view(CockpitApp *app)
{
    if (!app) return;
    app->scale = minimum_scale(app);
    app->offset_x = 0.0f;
    app->offset_y = 0.0f;
    update_scale_limits(app);
}

static void update_window_size(CockpitApp *app, int width, int height)
{
    if (!app || width <= 0 || height <= 0) return;
    app->window_width = width;
    app->window_height = height;
    update_scale_limits(app);
}

#ifdef _WIN32
typedef struct
{
    DWORD process_id;
    HWND window;
} WindowSearch;

static BOOL CALLBACK find_process_window(HWND window, LPARAM parameter)
{
    WindowSearch *search = (WindowSearch *)parameter;
    DWORD process_id = 0;
    GetWindowThreadProcessId(window, &process_id);
    if (process_id == search->process_id && IsWindowVisible(window) &&
        GetWindow(window, GW_OWNER) == NULL) {
        search->window = window;
        return FALSE;
    }
    return TRUE;
}

static HWND find_process_window_now(DWORD process_id)
{
    WindowSearch search = {process_id, NULL};
    EnumWindows(find_process_window, (LPARAM)&search);
    return search.window;
}

static void get_native_window(SDL_Window *window, HWND *native_window)
{
    SDL_SysWMinfo info;
    if (!window || !native_window) return;
    SDL_VERSION(&info.version);
    if (SDL_GetWindowWMInfo(window, &info) && info.subsystem == SDL_SYSWM_WINDOWS) {
        *native_window = info.info.win.window;
    }
}

static int build_module_path(const char *module_dir, const char *executable,
                             char *path, size_t path_size)
{
    DWORD length;
    if (!module_dir || !executable || !path || path_size == 0) return 0;
    length = GetFullPathNameA(module_dir, (DWORD)path_size, path, NULL);
    if (length == 0 || length >= path_size) return 0;
    if (path[length - 1] != '\\' && path[length - 1] != '/') {
        if (length + 1 >= path_size) return 0;
        path[length++] = '\\';
        path[length] = '\0';
    }
    if (strlen(executable) + length + 1 > path_size) return 0;
    if (snprintf(path + length, path_size - length, "%s", executable) < 0) return 0;
    return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
}

static void embed_window(HWND child, HWND parent)
{
    LONG_PTR style;
    if (!child || !parent) return;
    style = GetWindowLongPtr(child, GWL_STYLE);
    style &= ~(WS_POPUP | WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX |
               WS_MAXIMIZEBOX | WS_SYSMENU | WS_VISIBLE);
    style |= WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
    SetWindowLongPtr(child, GWL_STYLE, style);
    SetParent(child, parent);
    SetWindowLongPtr(child, GWL_EXSTYLE, GetWindowLongPtr(child, GWL_EXSTYLE) &
                     ~(WS_EX_APPWINDOW | WS_EX_WINDOWEDGE));
    SetWindowPos(child, NULL, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                 SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_HIDEWINDOW);
}

static void position_modules(CockpitApp *app)
{
    if (!app || !app->native_window || !app->layout_dirty) return;
    for (int i = 0; i < COCKPIT_MODULE_COUNT; ++i) {
        CockpitModule *module = &app->modules[i];
        int x = (int)lroundf(app->offset_x + module->x * app->scale);
        int y = (int)lroundf(app->offset_y + module->y * app->scale);
        int width = (int)lroundf(module->width * app->scale);
        int height = (int)lroundf(module->height * app->scale);
        if (module->window && width > 0 && height > 0) {
            int in_view = x < app->window_width && y < app->window_height &&
                          x + width > 0 && y + height > 0;
            if (!in_view) {
                if (module->visible) ShowWindow(module->window, SW_HIDE);
                module->visible = 0;
                continue;
            }
            SetWindowPos(module->window, NULL, x, y, width, height,
                         SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOOWNERZORDER |
                         SWP_NOSENDCHANGING | SWP_SHOWWINDOW);
            module->visible = 1;
        }
    }
    app->layout_dirty = 0;
}

static void close_module(CockpitModule *module)
{
    if (!module || !module->launched) return;
    if (module->window) PostMessage(module->window, WM_CLOSE, 0, 0);
    if (module->process.hProcess) {
        if (WaitForSingleObject(module->process.hProcess, 1500) == WAIT_TIMEOUT) {
            TerminateProcess(module->process.hProcess, 0);
            WaitForSingleObject(module->process.hProcess, 500);
        }
        if (module->process.hThread) CloseHandle(module->process.hThread);
        CloseHandle(module->process.hProcess);
    }
    memset(&module->process, 0, sizeof(module->process));
    module->window = NULL;
    module->launched = 0;
    module->visible = 0;
}

static void close_modules(CockpitApp *app)
{
    DWORD deadline;
    int processes_running;
    if (!app) return;

    /* Ask every child to stop first, then share one grace period across all
     * modules.  A per-module wait can make shutdown take several seconds. */
    for (int i = 0; i < COCKPIT_MODULE_COUNT; ++i) {
        CockpitModule *module = &app->modules[i];
        if (module->launched && module->window) {
            PostMessage(module->window, WM_CLOSE, 0, 0);
            module->window = NULL;
        }
    }

    deadline = GetTickCount() + 1500u;
    do {
        processes_running = 0;
        for (int i = 0; i < COCKPIT_MODULE_COUNT; ++i) {
            CockpitModule *module = &app->modules[i];
            if (module->launched && module->process.hProcess &&
                WaitForSingleObject(module->process.hProcess, 0) == WAIT_TIMEOUT) {
                processes_running = 1;
            }
        }
        if (processes_running && (LONG)(deadline - GetTickCount()) > 0) Sleep(20);
    } while (processes_running && (LONG)(deadline - GetTickCount()) > 0);

    for (int i = 0; i < COCKPIT_MODULE_COUNT; ++i) {
        CockpitModule *module = &app->modules[i];
        if (module->process.hProcess) {
            if (WaitForSingleObject(module->process.hProcess, 0) == WAIT_TIMEOUT) {
                TerminateProcess(module->process.hProcess, 0);
                WaitForSingleObject(module->process.hProcess, 500);
            }
            if (module->process.hThread) CloseHandle(module->process.hThread);
            CloseHandle(module->process.hProcess);
        }
        memset(&module->process, 0, sizeof(module->process));
        module->window = NULL;
        module->launched = 0;
        module->visible = 0;
    }
}

static int launch_modules(CockpitApp *app, const char *module_dir)
{
    char path[1024];
    char command_line[1200];
    char working_dir[1024];
    DWORD working_dir_length;
    DWORD start_time;
    int launched_count = 0;
    int attached_count = 0;
    if (!app || !app->native_window || !module_dir) return 0;

    working_dir_length = GetFullPathNameA(module_dir, (DWORD)sizeof(working_dir),
                                          working_dir, NULL);
    if (working_dir_length == 0 || working_dir_length >= sizeof(working_dir)) {
        return 0;
    }

    /* Start every child first.  Waiting after each CreateProcess call makes
     * a missing/slow module block all modules behind it. */
    for (int i = 0; i < COCKPIT_MODULE_COUNT; ++i) {
        CockpitModule *module = &app->modules[i];
        STARTUPINFOA startup = {0};
        startup.cb = sizeof(startup);
        memset(&module->process, 0, sizeof(module->process));
        if (!build_module_path(module_dir, module->executable, path, sizeof(path))) {
            fprintf(stderr, "Cockpit: module not found: %s\\%s\n", module_dir, module->executable);
            continue;
        }
        snprintf(command_line, sizeof(command_line), "\"%s\" --embedded", path);
        if (!CreateProcessA(path, command_line, NULL, NULL, FALSE, CREATE_NO_WINDOW,
                            NULL, working_dir, &startup, &module->process)) {
            fprintf(stderr, "Cockpit: failed to start %s (error %lu)\n",
                    module->name, (unsigned long)GetLastError());
            continue;
        }
        module->launched = 1;
        module->visible = 0;
        ++launched_count;
    }

    /* Poll all processes together so one failed module cannot add another
     * full timeout to startup. */
    start_time = GetTickCount();
    while (attached_count < launched_count &&
           GetTickCount() - start_time < COCKPIT_MODULE_START_TIMEOUT_MS) {
        for (int i = 0; i < COCKPIT_MODULE_COUNT; ++i) {
            CockpitModule *module = &app->modules[i];
            if (!module->launched || module->window) continue;
            if (WaitForSingleObject(module->process.hProcess, 0) == WAIT_OBJECT_0) {
                fprintf(stderr, "Cockpit: %s exited before creating a window\n", module->name);
                close_module(module);
                --launched_count;
                continue;
            }
            module->window = find_process_window_now(module->process.dwProcessId);
            if (module->window) {
                embed_window(module->window, app->native_window);
                ++attached_count;
            }
        }
        if (attached_count < launched_count) Sleep(20);
    }

    for (int i = 0; i < COCKPIT_MODULE_COUNT; ++i) {
        CockpitModule *module = &app->modules[i];
        if (!module->launched || module->window) continue;
        fprintf(stderr, "Cockpit: no window found for %s\n", module->name);
        close_module(module);
    }
    app->layout_dirty = 1;
    position_modules(app);
    return attached_count > 0;
}
#else
static void position_modules(CockpitApp *app)
{
    if (app) app->layout_dirty = 0;
}
static int launch_modules(CockpitApp *app, const char *module_dir)
{
    (void)app;
    (void)module_dir;
    fprintf(stderr, "Cockpit: module embedding is only available on Windows.\n");
    return 0;
}
static void close_modules(CockpitApp *app) { (void)app; }
#endif

static SDL_Texture *load_background(SDL_Renderer *renderer)
{
    const char *paths[] = {"assets/main.png", "../assets/main.png", "../../assets/main.png"};
    for (int i = 0; i < (int)(sizeof(paths) / sizeof(paths[0])); ++i) {
        SDL_Texture *texture = IMG_LoadTexture(renderer, paths[i]);
        if (texture) return texture;
    }
    {
        char *base = SDL_GetBasePath();
        if (base) {
            char path[1024];
            SDL_snprintf(path, sizeof(path), "%s../assets/main.png", base);
            SDL_Texture *texture = IMG_LoadTexture(renderer, path);
            SDL_free(base);
            if (texture) return texture;
        }
    }
    return NULL;
}

static void render(CockpitApp *app)
{
    SDL_Rect destination;
    if (!app || !app->renderer) return;
    SDL_SetRenderDrawColor(app->renderer, 0, 0, 0, 255);
    SDL_RenderClear(app->renderer);
    if (app->background) {
        destination.x = (int)lroundf(app->offset_x);
        destination.y = (int)lroundf(app->offset_y);
        destination.w = (int)lroundf(app->image_width * app->scale);
        destination.h = (int)lroundf(app->image_height * app->scale);
        SDL_RenderCopy(app->renderer, app->background, NULL, &destination);
    }
    CockpitAlarm_Update(app->alarm);
    CockpitAlarm_Render(app->alarm, app->renderer, app->window_width);
    SDL_RenderPresent(app->renderer);
    position_modules(app);
}

static void zoom_at(CockpitApp *app, int mouse_x, int mouse_y, int direction)
{
    float image_x;
    float image_y;
    float new_scale;
    if (!app || direction == 0 || app->scale <= 0.0f) return;
    if (direction > 8) direction = 8;
    if (direction < -8) direction = -8;
    image_x = (mouse_x - app->offset_x) / app->scale;
    image_y = (mouse_y - app->offset_y) / app->scale;
    new_scale = clamp_float(app->scale * powf(COCKPIT_ZOOM_STEP, (float)direction),
                            minimum_scale(app), maximum_scale(app));
    if (fabsf(new_scale - app->scale) < 0.000001f) return;
    app->scale = new_scale;
    app->offset_x = mouse_x - image_x * new_scale;
    app->offset_y = mouse_y - image_y * new_scale;
    update_scale_limits(app);
}

static void handle_event(CockpitApp *app, const SDL_Event *event, int *running)
{
    if (!app || !event || !running) return;
    switch (event->type) {
    case SDL_QUIT:
        *running = 0;
        break;
    case SDL_WINDOWEVENT:
        if (event->window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
            update_window_size(app, event->window.data1, event->window.data2);
        } else if (event->window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
            app->dragging = 0;
            SDL_CaptureMouse(SDL_FALSE);
        }
        break;
    case SDL_MOUSEBUTTONDOWN:
        if (event->button.button == SDL_BUTTON_LEFT) {
            app->dragging = 1;
            app->drag_x = event->button.x;
            app->drag_y = event->button.y;
            app->drag_offset_x = app->offset_x;
            app->drag_offset_y = app->offset_y;
            SDL_CaptureMouse(SDL_TRUE);
        }
        break;
    case SDL_MOUSEBUTTONUP:
        if (event->button.button == SDL_BUTTON_LEFT) {
            app->dragging = 0;
            SDL_CaptureMouse(SDL_FALSE);
        }
        break;
    case SDL_MOUSEMOTION:
        if (app->dragging && (event->motion.state & SDL_BUTTON_LMASK)) {
            app->offset_x = app->drag_offset_x + event->motion.x - app->drag_x;
            app->offset_y = app->drag_offset_y + event->motion.y - app->drag_y;
            update_scale_limits(app);
        } else if (app->dragging) {
            app->dragging = 0;
            SDL_CaptureMouse(SDL_FALSE);
        }
        break;
    case SDL_MOUSEWHEEL: {
        int x, y;
        int direction = event->wheel.y;
        if (event->wheel.direction == SDL_MOUSEWHEEL_FLIPPED) direction = -direction;
        SDL_GetMouseState(&x, &y);
        zoom_at(app, x, y, direction);
        break;
    }
    case SDL_KEYDOWN: {
        SDL_Keycode key = event->key.keysym.sym;
        if (key == SDLK_ESCAPE) {
            *running = 0;
        } else if (key == SDLK_HOME || key == SDLK_r || key == SDLK_0 || key == SDLK_KP_0) {
            reset_view(app);
        } else if (key == SDLK_EQUALS || key == SDLK_KP_PLUS) {
            zoom_at(app, app->window_width / 2, app->window_height / 2, 1);
        } else if (key == SDLK_MINUS || key == SDLK_KP_MINUS) {
            zoom_at(app, app->window_width / 2, app->window_height / 2, -1);
        }
        break;
    }
    default:
        break;
    }
}

static int init_app(CockpitApp *app, int embed_modules, int smoke)
{
    SDL_DisplayMode mode;
    memset(app, 0, sizeof(*app));
    app->embed_modules = embed_modules;
    app->smoke = smoke;
    memcpy(app->modules, g_module_templates, sizeof(g_module_templates));
    app->window = SDL_CreateWindow("Flight Cockpit", SDL_WINDOWPOS_UNDEFINED,
                                   SDL_WINDOWPOS_UNDEFINED, 0, 0,
                                   SDL_WINDOW_SHOWN | SDL_WINDOW_FULLSCREEN_DESKTOP |
                                   SDL_WINDOW_BORDERLESS);
    if (!app->window) {
        fprintf(stderr, "Cockpit window creation failed: %s\n", SDL_GetError());
        return 0;
    }
    SDL_GetWindowSize(app->window, &app->window_width, &app->window_height);
    if (SDL_GetCurrentDisplayMode(0, &mode) == 0 && app->window_width <= 0) {
        app->window_width = mode.w;
        app->window_height = mode.h;
    }
    app->renderer = SDL_CreateRenderer(app->window, -1,
                                       SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (app->renderer) {
        app->renderer_vsync = 1;
    } else {
        app->renderer = SDL_CreateRenderer(app->window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!app->renderer) {
        fprintf(stderr, "Cockpit renderer creation failed: %s\n", SDL_GetError());
        return 0;
    }
    app->background = load_background(app->renderer);
    if (!app->background) {
        fprintf(stderr, "Cockpit background load failed: %s\n", IMG_GetError());
        return 0;
    }
    app->image_width = COCKPIT_IMAGE_WIDTH;
    app->image_height = COCKPIT_IMAGE_HEIGHT;
    if (SDL_QueryTexture(app->background, NULL, NULL,
                         &app->image_width, &app->image_height) != 0 ||
        app->image_width <= 0 || app->image_height <= 0) {
        app->image_width = COCKPIT_IMAGE_WIDTH;
        app->image_height = COCKPIT_IMAGE_HEIGHT;
    }
    app->alarm = CockpitAlarm_Create();
    reset_view(app);
#ifdef _WIN32
    get_native_window(app->window, &app->native_window);
#endif
    return app->background != NULL;
}

static void destroy_app(CockpitApp *app)
{
    if (!app) return;
    close_modules(app);
    CockpitAlarm_Destroy(app->alarm);
    if (app->background) SDL_DestroyTexture(app->background);
    if (app->renderer) SDL_DestroyRenderer(app->renderer);
    if (app->window) SDL_DestroyWindow(app->window);
    memset(app, 0, sizeof(*app));
}

int Cockpit_SelfTest(void)
{
    CockpitApp app;
    float anchor_x;
    float minimum_offset_x;
    memset(&app, 0, sizeof(app));
    app.window_width = 1920;
    app.window_height = 1080;
    app.image_width = COCKPIT_IMAGE_WIDTH;
    app.image_height = COCKPIT_IMAGE_HEIGHT;
    reset_view(&app);
    if (fabsf(app.scale - 0.2392f) > 0.01f) return 0;
    if (fabsf(app.offset_y - 165.0f) > 5.0f) return 0;
    for (int i = 0; i < COCKPIT_MODULE_COUNT; ++i) {
        const CockpitModule *module = &g_module_templates[i];
        if (module->x < 0 || module->y < 0 ||
            module->x + module->width > COCKPIT_IMAGE_WIDTH ||
            module->y + module->height > COCKPIT_IMAGE_HEIGHT) {
            return 0;
        }
    }

    /* Zooming past the width-fit limit must not shift the image. */
    minimum_offset_x = app.offset_x;
    zoom_at(&app, 500, 500, -1);
    if (fabsf(app.offset_x - minimum_offset_x) > 0.01f) return 0;

    /* A valid zoom keeps the same image coordinate below the mouse on X. */
    anchor_x = (1200.0f - app.offset_x) / app.scale;
    zoom_at(&app, 1200, 500, 1);
    if (fabsf((1200.0f - app.offset_x) / app.scale - anchor_x) > 0.01f) return 0;

    app.scale *= 1.5f;
    app.offset_x = -400.0f;
    app.offset_y = -300.0f;
    update_scale_limits(&app);
    return app.offset_x <= 0.0f &&
           app.offset_x >= app.window_width - app.image_width * app.scale &&
           CockpitAlarm_SelfTest();
}

int Cockpit_Run(int embed_modules, int smoke, const char *module_dir)
{
    CockpitApp app;
    SDL_Event event;
    int running = 1;
    int frames = 0;
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    SDL_SetHint(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, "0");
#ifdef SDL_HINT_WINDOWS_DPI_AWARENESS
    SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2");
#endif
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0) {
        fprintf(stderr, "Cockpit SDL initialization failed: %s\n", SDL_GetError());
        return 0;
    }
    if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) {
        fprintf(stderr, "Cockpit SDL_image initialization failed: %s\n", IMG_GetError());
        SDL_Quit();
        return 0;
    }
    if (!init_app(&app, embed_modules, smoke)) {
        destroy_app(&app);
        IMG_Quit();
        SDL_Quit();
        return 0;
    }
#ifdef _WIN32
    (void)GetAsyncKeyState(VK_ESCAPE); /* Clear any stale transition state. */
    if (embed_modules && app.native_window && module_dir) launch_modules(&app, module_dir);
#else
    (void)module_dir;
#endif
    while (running) {
#ifdef _WIN32
        if (GetAsyncKeyState(VK_ESCAPE) & 0x8001) running = 0;
#endif
        while (SDL_PollEvent(&event)) handle_event(&app, &event, &running);
        if (!running) break;
        render(&app);
        ++frames;
        if (smoke && frames >= 3) running = 0;
        if (!app.renderer_vsync) SDL_Delay(16);
    }
    destroy_app(&app);
    IMG_Quit();
    SDL_Quit();
    return 1;
}
