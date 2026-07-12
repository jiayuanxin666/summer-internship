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
} CockpitModule;

struct CockpitApp
{
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *background;
    int window_width;
    int window_height;
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
    CockpitAlarm *alarm;
#ifdef _WIN32
    HWND native_window;
#endif
    CockpitModule modules[COCKPIT_MODULE_COUNT];
};

static CockpitModule g_module_templates[COCKPIT_MODULE_COUNT] = {
    {.name = "PFD captain", .executable = "PFD.exe", .x = 1296, .y = 946, .width = 771, .height = 721},
    {.name = "ND captain", .executable = "ND.exe", .x = 2205, .y = 946, .width = 751, .height = 721},
    {.name = "EICAS1", .executable = "EICAS1.exe", .x = 3621, .y = 942, .width = 745, .height = 728},
    {.name = "EICAS2", .executable = "EICAS2.exe", .x = 3550, .y = 1908, .width = 771, .height = 781},
    {.name = "ND first officer", .executable = "ND.exe", .x = 4786, .y = 946, .width = 751, .height = 721},
    {.name = "PFD first officer", .executable = "PFD.exe", .x = 5702, .y = 946, .width = 771, .height = 721},
    {.name = "FMC captain", .executable = "FMC.exe", .x = 2769, .y = 1817, .width = 636, .height = 998},
    {.name = "FMC first officer", .executable = "FMC.exe", .x = 4422, .y = 1817, .width = 636, .height = 998},
    {.name = "Cabin MAP", .executable = "MAP.exe", .x = 80, .y = 180, .width = 1120, .height = 630}
};

static float clamp_float(float value, float minimum, float maximum)
{
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}

static void update_scale_limits(CockpitApp *app)
{
    float minimum;
    float image_width;
    float image_height;
    if (!app || app->window_width <= 0 || app->window_height <= 0) return;

    minimum = (float)app->window_width / (float)COCKPIT_IMAGE_WIDTH;
    app->scale = clamp_float(app->scale, minimum, 1.5f);
    image_width = COCKPIT_IMAGE_WIDTH * app->scale;
    image_height = COCKPIT_IMAGE_HEIGHT * app->scale;

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

static HWND wait_for_process_window(DWORD process_id, DWORD timeout_ms)
{
    DWORD elapsed = 0;
    while (elapsed < timeout_ms) {
        WindowSearch search = {process_id, NULL};
        EnumWindows(find_process_window, (LPARAM)&search);
        if (search.window) return search.window;
        Sleep(20);
        elapsed += 20;
    }
    return NULL;
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
    if (strlen(executable) + length + 1 >= path_size) return 0;
    strcat(path, executable);
    return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
}

static void embed_window(HWND child, HWND parent)
{
    LONG_PTR style;
    if (!child || !parent) return;
    style = GetWindowLongPtr(child, GWL_STYLE);
    style &= ~(WS_POPUP | WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX |
               WS_MAXIMIZEBOX | WS_SYSMENU);
    style |= WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
    SetWindowLongPtr(child, GWL_STYLE, style);
    SetParent(child, parent);
    SetWindowLongPtr(child, GWL_EXSTYLE, GetWindowLongPtr(child, GWL_EXSTYLE) &
                     ~(WS_EX_APPWINDOW | WS_EX_WINDOWEDGE));
    SetWindowPos(child, NULL, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                 SWP_NOACTIVATE | SWP_FRAMECHANGED);
    ShowWindow(child, SW_SHOW);
}

static void position_modules(CockpitApp *app)
{
    if (!app || !app->native_window) return;
    for (int i = 0; i < COCKPIT_MODULE_COUNT; ++i) {
        CockpitModule *module = &app->modules[i];
        int x = (int)lroundf(app->offset_x + module->x * app->scale);
        int y = (int)lroundf(app->offset_y + module->y * app->scale);
        int width = (int)lroundf(module->width * app->scale);
        int height = (int)lroundf(module->height * app->scale);
        if (module->window && width > 0 && height > 0) {
            SetWindowPos(module->window, NULL, x, y, width, height,
                         SWP_NOACTIVATE | SWP_SHOWWINDOW);
        }
    }
}

static void close_module(CockpitModule *module)
{
    if (!module || !module->launched) return;
    if (module->window) PostMessage(module->window, WM_CLOSE, 0, 0);
    if (module->process.hProcess) {
        if (WaitForSingleObject(module->process.hProcess, 1000) == WAIT_TIMEOUT) {
            TerminateProcess(module->process.hProcess, 0);
            WaitForSingleObject(module->process.hProcess, 500);
        }
        CloseHandle(module->process.hThread);
        CloseHandle(module->process.hProcess);
    }
    module->window = NULL;
    module->launched = 0;
}

static void close_modules(CockpitApp *app)
{
    if (!app) return;
    for (int i = 0; i < COCKPIT_MODULE_COUNT; ++i) close_module(&app->modules[i]);
}

static int launch_modules(CockpitApp *app, const char *module_dir)
{
    char path[1024];
    char command_line[1200];
    char working_dir[1024];
    char *last_separator;
    if (!app || !app->native_window || !module_dir) return 0;

    if (GetFullPathNameA(module_dir, sizeof(working_dir), working_dir, NULL) == 0) {
        return 0;
    }
    last_separator = strrchr(working_dir, '\\');
    if (last_separator) *last_separator = '\0';

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
        snprintf(command_line, sizeof(command_line), "\\\"%s\\\" --embedded", path);
        if (!CreateProcessA(path, command_line, NULL, NULL, FALSE, CREATE_NO_WINDOW,
                            NULL, working_dir, &startup, &module->process)) {
            fprintf(stderr, "Cockpit: failed to start %s (error %lu)\n",
                    module->name, (unsigned long)GetLastError());
            continue;
        }
        module->launched = 1;
    }

    /* SDL creates the native window shortly after process startup.  Wait for
     * each process independently now that all eight processes are running. */
    for (int i = 0; i < COCKPIT_MODULE_COUNT; ++i) {
        CockpitModule *module = &app->modules[i];
        if (!module->launched) continue;
        module->window = wait_for_process_window(module->process.dwProcessId, 5000);
        if (!module->window) {
            fprintf(stderr, "Cockpit: no window found for %s\n", module->name);
            close_module(module);
            continue;
        }
        embed_window(module->window, app->native_window);
    }
    position_modules(app);
    return 1;
}
#else
static void position_modules(CockpitApp *app) { (void)app; }
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
        destination.w = (int)lroundf(COCKPIT_IMAGE_WIDTH * app->scale);
        destination.h = (int)lroundf(COCKPIT_IMAGE_HEIGHT * app->scale);
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
    if (!app || direction == 0) return;
    image_x = (mouse_x - app->offset_x) / app->scale;
    image_y = (mouse_y - app->offset_y) / app->scale;
    new_scale = app->scale * (direction > 0 ? 1.12f : 0.892857f);
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
            position_modules(app);
        }
        break;
    case SDL_MOUSEBUTTONDOWN:
        if (event->button.button == SDL_BUTTON_LEFT) {
            app->dragging = 1;
            app->drag_x = event->button.x;
            app->drag_y = event->button.y;
            app->drag_offset_x = app->offset_x;
            app->drag_offset_y = app->offset_y;
        }
        break;
    case SDL_MOUSEBUTTONUP:
        if (event->button.button == SDL_BUTTON_LEFT) app->dragging = 0;
        break;
    case SDL_MOUSEMOTION:
        if (app->dragging) {
            app->offset_x = app->drag_offset_x + event->motion.x - app->drag_x;
            app->offset_y = app->drag_offset_y + event->motion.y - app->drag_y;
            update_scale_limits(app);
            position_modules(app);
        }
        break;
    case SDL_MOUSEWHEEL: {
        int x, y;
        SDL_GetMouseState(&x, &y);
        zoom_at(app, x, y, event->wheel.y);
        position_modules(app);
        break;
    }
    case SDL_KEYDOWN:
        if (event->key.keysym.sym == SDLK_ESCAPE) *running = 0;
        break;
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
    if (!app->renderer) app->renderer = SDL_CreateRenderer(app->window, -1, SDL_RENDERER_SOFTWARE);
    if (!app->renderer) return 0;
    app->background = load_background(app->renderer);
    app->alarm = CockpitAlarm_Create();
    app->scale = (float)app->window_width / (float)COCKPIT_IMAGE_WIDTH;
    update_scale_limits(app);
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
    memset(&app, 0, sizeof(app));
    app.window_width = 1920;
    app.window_height = 1080;
    app.scale = (float)app.window_width / (float)COCKPIT_IMAGE_WIDTH;
    update_scale_limits(&app);
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
    app.scale *= 1.5f;
    app.offset_x = -400.0f;
    app.offset_y = -300.0f;
    update_scale_limits(&app);
    return app.offset_x <= 0.0f && app.offset_x >= app.window_width - COCKPIT_IMAGE_WIDTH * app.scale && CockpitAlarm_SelfTest();
}

int Cockpit_Run(int embed_modules, int smoke, const char *module_dir)
{
    CockpitApp app;
    SDL_Event event;
    int running = 1;
    int frames = 0;
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0) return 0;
    if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) {
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
    if (embed_modules && app.native_window && module_dir) launch_modules(&app, module_dir);
#else
    (void)module_dir;
#endif
    while (running) {
        while (SDL_PollEvent(&event)) handle_event(&app, &event, &running);
        render(&app);
        ++frames;
        if (smoke && frames >= 3) running = 0;
        SDL_Delay(16);
    }
    destroy_app(&app);
    IMG_Quit();
    SDL_Quit();
    return 1;
}
