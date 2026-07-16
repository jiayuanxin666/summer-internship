# Flight Cockpit

基于 SDL2 和 C17 的模块化飞行驾驶舱，包含 Cockpit、PFD、ND、EICAS、FMC 和 MAP。项目支持 X-Plane Connect、FMC 到 ND 的共享航路，以及驾驶舱声光告警。

## 构建与测试

安装 MSYS2 MinGW64 依赖后，在项目根目录运行：

```powershell
$env:PATH = 'D:\MSYS2\mingw64\bin;' + $env:PATH
cmake -S . -B build-cmake -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build-cmake -j 4
ctest --test-dir build-cmake --output-on-failure
```

可执行文件统一输出到 `build/`。运行 `build/Cockpit.exe` 启动集成界面。

## 配置

- 高德地图 Key：使用环境变量 `AMAP_API_KEY`，或本地文件 `assets/map.local.cfg`。
- X-Plane Connect：默认连接 `127.0.0.1:49009`。
- FMC 航路：通过 Windows 命名共享内存自动同步到 ND。

本地密钥、构建产物、截图和诊断文件已加入 `.gitignore`，请勿提交到版本库。
