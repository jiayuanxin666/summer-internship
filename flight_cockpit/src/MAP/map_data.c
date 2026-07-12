#include "map_data.h"

#include <curl/curl.h>
#include <cjson/cJSON.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef ENABLE_XPLANE
#include "../Util/xplaneConnect.h"
static XPCSocket g_xplane;
static int g_xplane_open;
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static void copy_text(char *dst, size_t size, const char *src)
{
    if (!dst || !size) return;
    snprintf(dst, size, "%s", src ? src : "--");
}

void MAP_Data_Init(MAP_Data *data)
{
    memset(data, 0, sizeof(*data));
    data->longitude = 120.38;
    data->latitude = 36.07;
    data->zoom = 8;
    copy_text(data->city, sizeof(data->city), "青岛市");
    copy_text(data->district, sizeof(data->district), "黄岛区");
    copy_text(data->township, sizeof(data->township), "薛家岛街道");
    copy_text(data->weather, sizeof(data->weather), "正在获取");
    copy_text(data->temperature, sizeof(data->temperature), "--");
    copy_text(data->humidity, sizeof(data->humidity), "--");
    copy_text(data->windpower, sizeof(data->windpower), "--");
    copy_text(data->winddirection, sizeof(data->winddirection), "--");
    copy_text(data->status, sizeof(data->status), "准备连接高德地图");
    MAP_Data_AddPoint(data, data->longitude, data->latitude);
}

double MAP_Data_Bearing(const MAP_Point *from, const MAP_Point *to)
{
    double p1 = from->latitude * M_PI / 180.0, p2 = to->latitude * M_PI / 180.0;
    double dl = (to->longitude - from->longitude) * M_PI / 180.0;
    double y = sin(dl) * cos(p2);
    double x = cos(p1) * sin(p2) - sin(p1) * cos(p2) * cos(dl);
    double result = atan2(y, x) * 180.0 / M_PI;
    return fmod(result + 360.0, 360.0);
}

void MAP_Data_AddPoint(MAP_Data *data, double longitude, double latitude)
{
    MAP_Point point = {longitude, latitude};
    if (data->track_count && fabs(data->track[data->track_count - 1].longitude - longitude) < 1e-7 &&
        fabs(data->track[data->track_count - 1].latitude - latitude) < 1e-7) return;
    if (data->track_count == MAP_TRACK_CAPACITY) {
        memmove(data->track, data->track + 1, sizeof(data->track[0]) * (MAP_TRACK_CAPACITY - 1));
        data->track_count--;
    }
    if (data->track_count) data->bearing = MAP_Data_Bearing(&data->track[data->track_count - 1], &point);
    data->track[data->track_count++] = point;
    data->longitude = longitude;
    data->latitude = latitude;
    data->revision++;
}

static size_t write_callback(void *contents, size_t size, size_t count, void *user)
{
    size_t add = size * count;
    MAP_HTTP_Response *r = user;
    unsigned char *next = realloc(r->bytes, r->size + add + 1);
    if (!next) return 0;
    r->bytes = next;
    memcpy(r->bytes + r->size, contents, add);
    r->size += add;
    r->bytes[r->size] = 0;
    return add;
}

static int http_get_with_handle(CURL *curl, const char *url, MAP_HTTP_Response *response)
{
    CURLcode result;
    memset(response, 0, sizeof(*response));
    if (!curl) return 0;
    curl_easy_reset(curl);
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "FlightCockpit-MAP/1.0");
    result = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response->http_status);
    return result == CURLE_OK && response->http_status == 200 && response->size > 0;
}

int MAP_HTTP_Get(const char *url, MAP_HTTP_Response *response)
{
    CURL *curl = curl_easy_init();
    int ok = http_get_with_handle(curl, url, response);
    if (curl) curl_easy_cleanup(curl);
    return ok;
}

void MAP_HTTP_Free(MAP_HTTP_Response *response)
{
    free(response->bytes);
    memset(response, 0, sizeof(*response));
}

static const char *json_string(cJSON *object, const char *name)
{
    cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
    return cJSON_IsString(item) ? item->valuestring : NULL;
}

static int fetch_json(CURL *curl, const char *url, cJSON **root)
{
    MAP_HTTP_Response response;
    if (!http_get_with_handle(curl, url, &response)) return 0;
    *root = cJSON_ParseWithLength((char *)response.bytes, response.size);
    MAP_HTTP_Free(&response);
    return *root != NULL;
}

int MAP_Data_FetchLocationWeather(MAP_Data *data, const char *api_key)
{
    char url[1024];
    CURL *curl = NULL;
    cJSON *root = NULL, *regeocode, *component, *lives, *live;
    const char *status;
    if (!api_key || !*api_key) { copy_text(data->status, sizeof(data->status), "未配置高德 API Key"); return 0; }
    curl = curl_easy_init();
    if (!curl) { copy_text(data->status, sizeof(data->status), "网络组件初始化失败"); return 0; }
    snprintf(url, sizeof(url), "https://restapi.amap.com/v3/geocode/regeo?location=%.6f,%.6f&extensions=base&key=%s",
             data->longitude, data->latitude, api_key);
    if (!fetch_json(curl, url, &root)) { curl_easy_cleanup(curl); copy_text(data->status, sizeof(data->status), "逆地理编码请求失败"); return 0; }
    status = json_string(root, "status");
    regeocode = cJSON_GetObjectItemCaseSensitive(root, "regeocode");
    component = regeocode ? cJSON_GetObjectItemCaseSensitive(regeocode, "addressComponent") : NULL;
    if (!status || strcmp(status, "1") || !component) { cJSON_Delete(root); curl_easy_cleanup(curl); copy_text(data->status, sizeof(data->status), "逆地理编码响应异常"); return 0; }
    copy_text(data->city, sizeof(data->city), json_string(component, "city"));
    copy_text(data->district, sizeof(data->district), json_string(component, "district"));
    copy_text(data->township, sizeof(data->township), json_string(component, "township"));
    copy_text(data->adcode, sizeof(data->adcode), json_string(component, "adcode"));
    cJSON_Delete(root); root = NULL;
    snprintf(url, sizeof(url), "https://restapi.amap.com/v3/weather/weatherInfo?city=%s&extensions=base&key=%s", data->adcode, api_key);
    if (!fetch_json(curl, url, &root)) { curl_easy_cleanup(curl); copy_text(data->status, sizeof(data->status), "天气请求失败"); return 0; }
    lives = cJSON_GetObjectItemCaseSensitive(root, "lives");
    live = cJSON_IsArray(lives) ? cJSON_GetArrayItem(lives, 0) : NULL;
    if (!live) { cJSON_Delete(root); curl_easy_cleanup(curl); copy_text(data->status, sizeof(data->status), "天气响应异常"); return 0; }
    copy_text(data->weather, sizeof(data->weather), json_string(live, "weather"));
    copy_text(data->temperature, sizeof(data->temperature), json_string(live, "temperature"));
    copy_text(data->humidity, sizeof(data->humidity), json_string(live, "humidity"));
    copy_text(data->windpower, sizeof(data->windpower), json_string(live, "windpower"));
    copy_text(data->winddirection, sizeof(data->winddirection), json_string(live, "winddirection"));
    cJSON_Delete(root);
    curl_easy_cleanup(curl);
    copy_text(data->status, sizeof(data->status), "数据已更新");
    return 1;
}

int MAP_Data_FetchStaticMap(const MAP_Data *data, const char *api_key, int width, int height, MAP_HTTP_Response *response)
{
    char url[8192], paths[6000] = "";
    size_t used = 0;
    if (!api_key || !*api_key) return 0;
    for (int i = 0; i < data->track_count; ++i) {
        int n = snprintf(paths + used, sizeof(paths) - used, "%s%.6f,%.6f", i ? ";" : "", data->track[i].longitude, data->track[i].latitude);
        if (n < 0 || (size_t)n >= sizeof(paths) - used) break;
        used += (size_t)n;
    }
    if (width > 1024) width = 1024;
    if (height > 1024) height = 1024;
    snprintf(url, sizeof(url), "https://restapi.amap.com/v3/staticmap?location=%.6f,%.6f&zoom=%d&size=%d*%d&scale=2%s%s&key=%s",
             data->longitude, data->latitude, data->zoom, width, height,
             used > 0 ? "&paths=5,0x3699ff,1.0,,:" : "", paths, api_key);
    return MAP_HTTP_Get(url, response);
}

int MAP_Data_FetchXPlane(MAP_Data *data)
{
#ifdef ENABLE_XPLANE
    const char *drefs[] = {"sim/flightmodel/position/longitude", "sim/flightmodel/position/latitude"};
    float values[2][8];
    if (!g_xplane_open) { g_xplane = openUDP("127.0.0.1"); g_xplane_open = XPCSocket_IsOpen(g_xplane); }
    if (g_xplane_open && getDREFs(g_xplane, drefs, 2, values)) { MAP_Data_AddPoint(data, values[0][0], values[1][0]); return 1; }
#else
    (void)data;
#endif
    return 0;
}

void MAP_Data_CloseXPlane(void)
{
#ifdef ENABLE_XPLANE
    if (g_xplane_open) closeUDP(g_xplane);
    g_xplane_open = 0;
#endif
}

int MAP_Data_SelfTest(void)
{
    MAP_Data data; MAP_Point a = {0, 0}, b = {1, 0};
    MAP_Data_Init(&data);
    if (fabs(MAP_Data_Bearing(&a, &b) - 90.0) > 0.01) return 0;
    for (int i = 0; i < 50; ++i) MAP_Data_AddPoint(&data, 120.0 + i * .01, 36.0);
    return data.track_count == MAP_TRACK_CAPACITY && fabs(data.track[0].longitude - 120.10) < .001;
}
