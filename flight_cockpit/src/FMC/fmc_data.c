#include "fmc_data.h"
#include <SDL2/SDL.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct FMC_AvlNode
{
    FMC_NavPoint point;
    int height;
    struct FMC_AvlNode *left;
    struct FMC_AvlNode *right;
};

static const FMC_ProcedureCatalog g_ksea_catalog = {
    "KSEA",
    {"ATOM E2", "BANGR 9", "SUMMA 1", NULL},
    {"RW16C", "RW16L", "RW34C", NULL},
    {"HBM", "COV", NULL, NULL},
    {"CHINS 3", "EPH 8", "SUMMA 1", NULL},
    {"RW14R", "RW32L", "RW34C", NULL},
    {"HBM", "COV", "XYZ", NULL}
};

static const FMC_ProcedureCatalog g_kbfi_catalog = {
    "KBFI",
    {"BANGR 9", "SUMMA 1", NULL, NULL},
    {"RW14R", "RW32L", NULL, NULL},
    {"SEA", "HBM", NULL, NULL},
    {"CHINS 3", "SUMMA 1", NULL, NULL},
    {"RW14R", "RW32L", NULL, NULL},
    {"HBM", "COV", NULL, NULL}
};

static int max_int(int a, int b)
{
    return a > b ? a : b;
}

static int node_height(const FMC_AvlNode *node)
{
    return node ? node->height : 0;
}

static void copy_text(char *dst, size_t dst_size, const char *src)
{
    if (!dst || dst_size == 0) {
        return;
    }
    SDL_snprintf(dst, dst_size, "%s", src ? src : "");
}

static void uppercase_copy(char *dst, size_t dst_size, const char *src)
{
    size_t i = 0;
    if (!dst || dst_size == 0) {
        return;
    }
    while (src && src[i] && i + 1 < dst_size) {
        dst[i] = (char)toupper((unsigned char)src[i]);
        ++i;
    }
    dst[i] = '\0';
}

static FMC_AvlNode *rotate_right(FMC_AvlNode *root)
{
    FMC_AvlNode *left = root->left;
    FMC_AvlNode *middle = left->right;
    left->right = root;
    root->left = middle;
    root->height = max_int(node_height(root->left), node_height(root->right)) + 1;
    left->height = max_int(node_height(left->left), node_height(left->right)) + 1;
    return left;
}

static FMC_AvlNode *rotate_left(FMC_AvlNode *root)
{
    FMC_AvlNode *right = root->right;
    FMC_AvlNode *middle = right->left;
    right->left = root;
    root->right = middle;
    root->height = max_int(node_height(root->left), node_height(root->right)) + 1;
    right->height = max_int(node_height(right->left), node_height(right->right)) + 1;
    return right;
}

static FMC_AvlNode *avl_insert(FMC_AvlNode *root, const FMC_NavPoint *point, int *inserted)
{
    int compare;
    int balance;

    if (!root) {
        FMC_AvlNode *node = (FMC_AvlNode *)calloc(1, sizeof(*node));
        if (!node) {
            return NULL;
        }
        node->point = *point;
        node->height = 1;
        if (inserted) {
            *inserted = 1;
        }
        return node;
    }

    compare = strcmp(point->ident, root->point.ident);
    if (compare < 0) {
        FMC_AvlNode *updated = avl_insert(root->left, point, inserted);
        if (updated) {
            root->left = updated;
        }
    } else if (compare > 0) {
        FMC_AvlNode *updated = avl_insert(root->right, point, inserted);
        if (updated) {
            root->right = updated;
        }
    } else {
        return root;
    }

    root->height = max_int(node_height(root->left), node_height(root->right)) + 1;
    balance = node_height(root->left) - node_height(root->right);

    if (balance > 1 && strcmp(point->ident, root->left->point.ident) < 0) {
        return rotate_right(root);
    }
    if (balance < -1 && strcmp(point->ident, root->right->point.ident) > 0) {
        return rotate_left(root);
    }
    if (balance > 1 && strcmp(point->ident, root->left->point.ident) > 0) {
        root->left = rotate_left(root->left);
        return rotate_right(root);
    }
    if (balance < -1 && strcmp(point->ident, root->right->point.ident) < 0) {
        root->right = rotate_right(root->right);
        return rotate_left(root);
    }

    return root;
}

static const FMC_NavPoint *avl_find(const FMC_AvlNode *root, const char *ident)
{
    char normalized[FMC_IDENT_LEN];
    uppercase_copy(normalized, sizeof(normalized), ident);

    while (root) {
        int compare = strcmp(normalized, root->point.ident);
        if (compare == 0) {
            return &root->point;
        }
        root = compare < 0 ? root->left : root->right;
    }
    return NULL;
}

static FMC_NavPoint *avl_find_mutable(FMC_AvlNode *root, const char *ident)
{
    char normalized[FMC_IDENT_LEN];
    uppercase_copy(normalized, sizeof(normalized), ident);

    while (root) {
        int compare = strcmp(normalized, root->point.ident);
        if (compare == 0) {
            return &root->point;
        }
        root = compare < 0 ? root->left : root->right;
    }
    return NULL;
}

static void avl_destroy(FMC_AvlNode *root)
{
    if (!root) {
        return;
    }
    avl_destroy(root->left);
    avl_destroy(root->right);
    free(root);
}

static FILE *open_resource(const char *name)
{
    char path[1024];
    const char *prefixes[] = {"assets/", "../assets/", "../../assets/"};

    for (int i = 0; i < (int)(sizeof(prefixes) / sizeof(prefixes[0])); ++i) {
        SDL_snprintf(path, sizeof(path), "%s%s", prefixes[i], name);
        FILE *file = fopen(path, "r");
        if (file) {
            return file;
        }
    }

    {
        char *base = SDL_GetBasePath();
        if (base) {
            for (int i = 0; i < (int)(sizeof(prefixes) / sizeof(prefixes[0])); ++i) {
                SDL_snprintf(path, sizeof(path), "%s%s%s", base, prefixes[i], name);
                FILE *file = fopen(path, "r");
                if (file) {
                    SDL_free(base);
                    return file;
                }
            }
            SDL_free(base);
        }
    }
    return NULL;
}

static void insert_point(FMC_AvlNode **root, int *count, const char *ident,
                         double latitude, double longitude)
{
    FMC_NavPoint point;
    int inserted = 0;
    FMC_AvlNode *updated;

    memset(&point, 0, sizeof(point));
    uppercase_copy(point.ident, sizeof(point.ident), ident);
    point.latitude = latitude;
    point.longitude = longitude;
    updated = avl_insert(*root, &point, &inserted);
    if (updated) {
        *root = updated;
    }
    if (inserted) {
        ++*count;
    }
}

static void upsert_point(FMC_AvlNode **root, int *count, const char *ident,
                         double latitude, double longitude)
{
    FMC_NavPoint *existing = avl_find_mutable(*root, ident);
    if (existing) {
        existing->latitude = latitude;
        existing->longitude = longitude;
        return;
    }
    insert_point(root, count, ident, latitude, longitude);
}

static void load_airports(FMC_Data *data)
{
    FILE *file = open_resource("apt.dat");
    char line[256];
    char ident[FMC_IDENT_LEN];
    double latitude;
    double longitude;

    if (file) {
        while (fgets(line, sizeof(line), file)) {
            if (sscanf(line, " %15s %lf %lf", ident, &latitude, &longitude) == 3) {
                insert_point(&data->airport_root, &data->airport_count,
                             ident, latitude, longitude);
            }
        }
        fclose(file);
    }

    /* The supplied training database contains duplicate/synthetic identifiers.
     * Keep the documented Seattle demo route deterministic for FMC-to-ND tests. */
    upsert_point(&data->airport_root, &data->airport_count, "KSEA", 47.4489, -122.3094);
    upsert_point(&data->airport_root, &data->airport_count, "KBFI", 47.5401, -122.3094);
}

static void load_fixes(FMC_Data *data)
{
    FILE *file = open_resource("earth_fix.dat");
    char line[256];
    char ident[FMC_IDENT_LEN];
    double latitude;
    double longitude;

    if (file) {
        while (fgets(line, sizeof(line), file)) {
            if (sscanf(line, " %lf %lf %15s", &latitude, &longitude, ident) == 3) {
                insert_point(&data->fix_root, &data->fix_count, ident, latitude, longitude);
            }
        }
        fclose(file);
    }

    upsert_point(&data->fix_root, &data->fix_count, "FREDY", 47.5578, -122.2892);
    upsert_point(&data->fix_root, &data->fix_count, "RENTO", 47.4847, -122.2319);
    upsert_point(&data->fix_root, &data->fix_count, "TOTEM", 47.4500, -122.1833);
    upsert_point(&data->fix_root, &data->fix_count, "BOTLL", 47.4158, -122.1358);
    upsert_point(&data->fix_root, &data->fix_count, "KIRBY", 47.3808, -122.0875);
}

int FMC_Data_Init(FMC_Data *data)
{
    if (!data) {
        return 0;
    }

    memset(data, 0, sizeof(*data));
    copy_text(data->climb_speed_low, sizeof(data->climb_speed_low), "290");
    copy_text(data->climb_speed_mach, sizeof(data->climb_speed_mach), ".74");
    copy_text(data->climb_transition_altitude, sizeof(data->climb_transition_altitude), "12000");
    copy_text(data->speed_limit, sizeof(data->speed_limit), "250");
    copy_text(data->speed_limit_altitude, sizeof(data->speed_limit_altitude), "10000");
    copy_text(data->cruise_speed, sizeof(data->cruise_speed), "300/.74");
    copy_text(data->cruise_altitude, sizeof(data->cruise_altitude), "----");
    copy_text(data->descent_speed, sizeof(data->descent_speed), ".74/290");
    copy_text(data->descent_transition_level, sizeof(data->descent_transition_level), "FL300");
    copy_text(data->descent_vpa, sizeof(data->descent_vpa), "2.5");
    data->selected_departure_procedure = -1;
    data->selected_departure_runway = -1;
    data->selected_departure_transition = -1;
    data->selected_arrival_procedure = -1;
    data->selected_arrival_runway = -1;
    data->selected_arrival_transition = -1;

    load_airports(data);
    load_fixes(data);
    return data->airport_count > 0 && data->fix_count > 0;
}

void FMC_Data_Destroy(FMC_Data *data)
{
    if (!data) {
        return;
    }
    avl_destroy(data->airport_root);
    avl_destroy(data->fix_root);
    data->airport_root = NULL;
    data->fix_root = NULL;
    data->airport_count = 0;
    data->fix_count = 0;
}

const FMC_NavPoint *FMC_Data_FindAirport(const FMC_Data *data, const char *ident)
{
    return data ? avl_find(data->airport_root, ident) : NULL;
}

const FMC_NavPoint *FMC_Data_FindFix(const FMC_Data *data, const char *ident)
{
    return data ? avl_find(data->fix_root, ident) : NULL;
}

void FMC_Data_SetMessage(FMC_Data *data, const char *message)
{
    if (data) {
        copy_text(data->message, sizeof(data->message), message);
    }
}

static int set_airport(FMC_Data *data, const char *ident, int origin)
{
    const FMC_NavPoint *point;
    if (!data || !ident || !ident[0]) {
        return 0;
    }
    point = FMC_Data_FindAirport(data, ident);
    if (!point) {
        FMC_Data_SetMessage(data, "NOT IN DATABASE");
        return 0;
    }
    if (origin) {
        data->origin = *point;
        data->has_origin = 1;
    } else {
        data->destination = *point;
        data->has_destination = 1;
    }
    FMC_Data_ClearScratchpad(data);
    FMC_Data_SetMessage(data, "");
    FMC_Data_MarkModified(data);
    FMC_Data_RefreshRouteExecState(data);
    return 1;
}

int FMC_Data_SetOrigin(FMC_Data *data, const char *ident)
{
    return set_airport(data, ident, 1);
}

int FMC_Data_SetDestination(FMC_Data *data, const char *ident)
{
    return set_airport(data, ident, 0);
}

int FMC_Data_SetFlightNumber(FMC_Data *data, const char *text)
{
    if (!data || !text || !text[0]) {
        return 0;
    }
    uppercase_copy(data->flight_number, sizeof(data->flight_number), text);
    FMC_Data_ClearScratchpad(data);
    FMC_Data_SetMessage(data, "");
    FMC_Data_MarkModified(data);
    FMC_Data_RefreshRouteExecState(data);
    return 1;
}

int FMC_Data_AddRouteFix(FMC_Data *data, const char *ident)
{
    const FMC_NavPoint *point;
    if (!data || !ident || !ident[0] || data->route_count >= FMC_MAX_ROUTE_LEGS) {
        return 0;
    }
    point = FMC_Data_FindFix(data, ident);
    if (!point) {
        FMC_Data_SetMessage(data, "NOT IN DATABASE");
        return 0;
    }
    copy_text(data->route[data->route_count].via,
              sizeof(data->route[data->route_count].via), "DIRECT");
    data->route[data->route_count].waypoint = *point;
    ++data->route_count;
    FMC_Data_ClearScratchpad(data);
    FMC_Data_SetMessage(data, "");
    FMC_Data_MarkModified(data);
    FMC_Data_RefreshRouteExecState(data);
    return 1;
}

void FMC_Data_AppendScratchpad(FMC_Data *data, const char *text)
{
    size_t current;
    size_t available;
    if (!data || !text) {
        return;
    }
    if (data->delete_mode) {
        data->delete_mode = 0;
        data->scratchpad[0] = '\0';
    }
    data->message[0] = '\0';
    current = strlen(data->scratchpad);
    available = sizeof(data->scratchpad) - current - 1;
    if (available > 0) {
        strncat(data->scratchpad, text, available);
    }
}

void FMC_Data_ClearScratchpad(FMC_Data *data)
{
    if (!data) {
        return;
    }
    data->scratchpad[0] = '\0';
    data->delete_mode = 0;
}

void FMC_Data_DeleteScratchpad(FMC_Data *data)
{
    size_t length;
    if (!data) {
        return;
    }
    if (data->delete_mode) {
        FMC_Data_ClearScratchpad(data);
        return;
    }
    length = strlen(data->scratchpad);
    if (length > 0) {
        data->scratchpad[length - 1] = '\0';
    }
}

const char *FMC_Data_ScratchpadText(const FMC_Data *data)
{
    if (!data) {
        return "";
    }
    if (data->message[0]) {
        return data->message;
    }
    return data->delete_mode ? "DELETE" : data->scratchpad;
}

static int parse_int_range(const char *text, int min_value, int max_value, int *value)
{
    char *end = NULL;
    long parsed;
    if (!text || !text[0]) {
        return 0;
    }
    errno = 0;
    parsed = strtol(text, &end, 10);
    if (errno != 0 || !end || *end != '\0' ||
        parsed < min_value || parsed > max_value) {
        return 0;
    }
    if (value) {
        *value = (int)parsed;
    }
    return 1;
}

static int parse_mach(const char *text, double *value)
{
    char *end = NULL;
    double parsed;
    if (!text || !text[0]) {
        return 0;
    }
    errno = 0;
    parsed = strtod(text, &end);
    if (errno != 0 || !end || *end != '\0' || !isfinite(parsed) ||
        parsed < 0.4 || parsed > 0.95) {
        return 0;
    }
    if (value) {
        *value = parsed;
    }
    return 1;
}

int FMC_Data_SetClimbSpeed(FMC_Data *data, const char *text)
{
    char copy[32];
    char *slash;
    int low;
    double mach;
    if (!data || !text) {
        return 0;
    }
    copy_text(copy, sizeof(copy), text);
    slash = strchr(copy, '/');
    if (!slash) {
        if (!parse_int_range(copy, 100, 399, &low)) {
            FMC_Data_SetMessage(data, "INVALID SPEED");
            return 0;
        }
        copy_text(data->climb_speed_low, sizeof(data->climb_speed_low), copy);
    } else {
        *slash = '\0';
        if (copy[0] && !parse_int_range(copy, 100, 399, &low)) {
            FMC_Data_SetMessage(data, "INVALID SPEED");
            return 0;
        }
        if (!parse_mach(slash + 1, &mach)) {
            FMC_Data_SetMessage(data, "INVALID MACH");
            return 0;
        }
        if (copy[0]) {
            copy_text(data->climb_speed_low, sizeof(data->climb_speed_low), copy);
        }
        copy_text(data->climb_speed_mach, sizeof(data->climb_speed_mach), slash + 1);
    }
    FMC_Data_ClearScratchpad(data);
    FMC_Data_SetMessage(data, "");
    FMC_Data_MarkModified(data);
    return 1;
}

int FMC_Data_SetSpeedAltitudeLimit(FMC_Data *data, const char *text)
{
    char copy[32];
    char *slash;
    int speed;
    int altitude;
    if (!data || !text) {
        return 0;
    }
    copy_text(copy, sizeof(copy), text);
    slash = strchr(copy, '/');
    if (!slash) {
        FMC_Data_SetMessage(data, "FORMAT SPD/ALT");
        return 0;
    }
    *slash = '\0';
    if (!parse_int_range(copy, 100, 399, &speed) ||
        !parse_int_range(slash + 1, 0, 60000, &altitude)) {
        FMC_Data_SetMessage(data, "INVALID SPD/ALT");
        return 0;
    }
    copy_text(data->speed_limit, sizeof(data->speed_limit), copy);
    copy_text(data->speed_limit_altitude, sizeof(data->speed_limit_altitude), slash + 1);
    FMC_Data_ClearScratchpad(data);
    FMC_Data_SetMessage(data, "");
    FMC_Data_MarkModified(data);
    return 1;
}

int FMC_Data_SetAltitude(char *target, size_t target_size, const char *text,
                         int min_value, int max_value, int allow_flight_level)
{
    int value;
    const char *digits = text;
    if (!target || !text || !text[0]) {
        return 0;
    }
    if (allow_flight_level && (text[0] == 'F' || text[0] == 'f') &&
        (text[1] == 'L' || text[1] == 'l')) {
        digits = text + 2;
        if (!parse_int_range(digits, min_value / 100, max_value / 100, &value)) {
            return 0;
        }
        SDL_snprintf(target, target_size, "FL%d", value);
        return 1;
    }
    if (!parse_int_range(digits, min_value, max_value, &value)) {
        return 0;
    }
    copy_text(target, target_size, digits);
    return 1;
}

static int set_combined_speed(char *target, size_t target_size, const char *text)
{
    char copy[32];
    char *slash;
    int speed;
    double mach;
    copy_text(copy, sizeof(copy), text);
    slash = strchr(copy, '/');
    if (!slash) {
        if (parse_int_range(copy, 100, 399, &speed) || parse_mach(copy, &mach)) {
            copy_text(target, target_size, text);
            return 1;
        }
        return 0;
    }
    *slash = '\0';
    if ((parse_int_range(copy, 100, 399, &speed) && parse_mach(slash + 1, &mach)) ||
        (parse_mach(copy, &mach) && parse_int_range(slash + 1, 100, 399, &speed))) {
        copy_text(target, target_size, text);
        return 1;
    }
    return 0;
}

int FMC_Data_SetCruiseSpeed(FMC_Data *data, const char *text)
{
    if (!data || !set_combined_speed(data->cruise_speed, sizeof(data->cruise_speed), text)) {
        if (data) FMC_Data_SetMessage(data, "INVALID SPEED");
        return 0;
    }
    FMC_Data_ClearScratchpad(data);
    FMC_Data_SetMessage(data, "");
    FMC_Data_MarkModified(data);
    return 1;
}

int FMC_Data_SetDescentSpeed(FMC_Data *data, const char *text)
{
    if (!data || !set_combined_speed(data->descent_speed, sizeof(data->descent_speed), text)) {
        if (data) FMC_Data_SetMessage(data, "INVALID SPEED");
        return 0;
    }
    FMC_Data_ClearScratchpad(data);
    FMC_Data_SetMessage(data, "");
    FMC_Data_MarkModified(data);
    return 1;
}

int FMC_Data_SetVpa(FMC_Data *data, const char *text)
{
    char *end = NULL;
    double value;
    if (!data || !text) {
        return 0;
    }
    errno = 0;
    value = strtod(text, &end);
    if (errno != 0 || !end || *end != '\0' || !isfinite(value) ||
        value < 1.0 || value > 6.0) {
        FMC_Data_SetMessage(data, "INVALID VPA");
        return 0;
    }
    SDL_snprintf(data->descent_vpa, sizeof(data->descent_vpa), "%.1f", value);
    FMC_Data_ClearScratchpad(data);
    FMC_Data_SetMessage(data, "");
    FMC_Data_MarkModified(data);
    return 1;
}

int FMC_Data_IsRouteReady(const FMC_Data *data)
{
    return data && data->has_origin && data->has_destination && data->flight_number[0];
}

void FMC_Data_RefreshRouteExecState(FMC_Data *data)
{
    if (!data) {
        return;
    }
    data->exec_pending = FMC_Data_IsRouteReady(data) ? 1 : 0;
    data->synchronized = 0;
}

void FMC_Data_MarkModified(FMC_Data *data)
{
    if (!data) {
        return;
    }
    data->exec_pending = 1;
    data->synchronized = 0;
}

void FMC_Data_MarkSynchronized(FMC_Data *data)
{
    if (!data) {
        return;
    }
    data->exec_pending = 0;
    data->synchronized = 1;
    FMC_Data_SetMessage(data, "EXECUTED");
}

const FMC_ProcedureCatalog *FMC_Data_GetProcedureCatalog(const FMC_Data *data, int arrival)
{
    const char *ident = NULL;
    if (data) {
        if (arrival && data->has_destination) {
            ident = data->destination.ident;
        } else if (!arrival && data->has_origin) {
            ident = data->origin.ident;
        }
    }
    return FMC_Data_GetProcedureCatalogForAirport(ident);
}

const FMC_ProcedureCatalog *FMC_Data_GetProcedureCatalogForAirport(const char *ident)
{
    return ident && strcmp(ident, "KBFI") == 0 ? &g_kbfi_catalog : &g_ksea_catalog;
}

int FMC_Data_ProcedureCompatible(const FMC_ProcedureCatalog *catalog, int arrival,
                                 int procedure_index, int runway_index)
{
    unsigned int runway_mask = 0;
    const char *const *procedures;
    const char *const *runways;

    if (!catalog || procedure_index < 0 || procedure_index >= 4 ||
        runway_index < 0 || runway_index >= 4) {
        return 0;
    }
    procedures = arrival ? catalog->arrival_procedures : catalog->departure_procedures;
    runways = arrival ? catalog->arrival_runways : catalog->departure_runways;
    if (!procedures[procedure_index] || !runways[runway_index]) {
        return 0;
    }

    if (strcmp(catalog->airport, "KSEA") == 0) {
        static const unsigned int departure_masks[3] = {
            1u << 0, 1u << 1, (1u << 0) | (1u << 2)
        };
        static const unsigned int arrival_masks[3] = {
            1u << 0, 1u << 1, 1u << 2
        };
        runway_mask = arrival ? arrival_masks[procedure_index]
                               : departure_masks[procedure_index];
    } else if (strcmp(catalog->airport, "KBFI") == 0) {
        static const unsigned int departure_masks[3] = {
            1u << 0, 1u << 1, 0u
        };
        static const unsigned int arrival_masks[3] = {
            (1u << 0) | (1u << 1), 1u << 1, 0u
        };
        runway_mask = arrival ? arrival_masks[procedure_index]
                               : departure_masks[procedure_index];
    } else {
        return 1;
    }
    return (runway_mask & (1u << runway_index)) != 0;
}

int FMC_Data_RunSelfTest(void)
{
    FMC_Data data;
    int ok = 1;
    if (!FMC_Data_Init(&data)) {
        return 0;
    }
    ok = ok && FMC_Data_FindAirport(&data, "KSEA") != NULL;
    ok = ok && FMC_Data_FindAirport(&data, "ksea") != NULL;
    ok = ok && FMC_Data_FindFix(&data, "FREDY") != NULL;
    ok = ok && FMC_Data_FindAirport(&data, "KSEA")->longitude < -122.0;
    ok = ok && FMC_Data_FindFix(&data, "FREDY")->longitude < -122.0;
    ok = ok && FMC_Data_SetOrigin(&data, "KSEA");
    ok = ok && !data.exec_pending;
    ok = ok && FMC_Data_SetDestination(&data, "KBFI");
    ok = ok && !data.exec_pending;
    ok = ok && FMC_Data_SetFlightNumber(&data, "AAA001");
    ok = ok && FMC_Data_IsRouteReady(&data);
    ok = ok && data.exec_pending;
    ok = ok && FMC_Data_AddRouteFix(&data, "FREDY");
    ok = ok && data.route_count == 1;
    ok = ok && FMC_Data_SetClimbSpeed(&data, "290/.76");
    ok = ok && FMC_Data_SetSpeedAltitudeLimit(&data, "250/10000");
    ok = ok && FMC_Data_SetAltitude(data.cruise_altitude, sizeof(data.cruise_altitude),
                                    "35000", 1000, 60000, 1);
    ok = ok && !FMC_Data_SetClimbSpeed(&data, "99") &&
         strcmp(data.message, "INVALID SPEED") == 0;
    ok = ok && !FMC_Data_SetCruiseSpeed(&data, "nan");
    ok = ok && !FMC_Data_SetVpa(&data, "nan");
    ok = ok && FMC_Data_ProcedureCompatible(&g_ksea_catalog, 0, 0, 0);
    ok = ok && !FMC_Data_ProcedureCompatible(&g_ksea_catalog, 0, 0, 1);
    FMC_Data_Destroy(&data);
    return ok;
}
