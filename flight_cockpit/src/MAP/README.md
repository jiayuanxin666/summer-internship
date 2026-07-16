# 客舱公共信息 MAP 模块

运行时按以下优先级读取高德 Web 服务 Key：`--amap-key KEY`、环境变量
`AMAP_API_KEY`、`assets/map.local.cfg`。本地配置已被 `.gitignore` 排除。

```powershell
$flags = pkg-config --cflags --libs sdl2 SDL2_image SDL2_ttf libcurl libcjson
gcc -std=c11 -DENABLE_XPLANE src/MAP/map_main.c src/MAP/map_ui.c src/MAP/map_data.c src/Util/xplaneConnect.c -o build/MAP.exe ($flags -split ' ') -lws2_32 -lm
build\MAP.exe
```

支持参数：`--embedded`、`--smoke`、`--self-test`、`--api-test`。

界面按需求将右侧公共信息区域设置为窗口宽度约 13%，地点、天气面板和控制按钮会随窗口尺寸动态调整。网络请求带超时和一次自动重试；地图请求只在启动、交互刷新或 30 秒定时刷新时执行，不进入逐帧渲染循环。
