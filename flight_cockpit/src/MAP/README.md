# 客舱公共信息 MAP 模块

运行时按以下优先级读取高德 Web 服务 Key：`--amap-key KEY`、环境变量
`AMAP_API_KEY`、`assets/map.local.cfg`。本地配置已被 `.gitignore` 排除。

```powershell
$flags = pkg-config --cflags --libs sdl2 SDL2_image SDL2_ttf libcurl libcjson
gcc -std=c11 -DENABLE_XPLANE src/MAP/map_main.c src/MAP/map_ui.c src/MAP/map_data.c src/Util/xplaneConnect.c -o build/MAP.exe ($flags -split ' ') -lws2_32 -lm
build\MAP.exe
```

支持参数：`--embedded`、`--smoke`、`--self-test`、`--api-test`。
