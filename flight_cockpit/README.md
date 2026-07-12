# Flight Cockpit

SDL2/C 实现的模块化飞行驾驶舱，包含 Cockpit、PFD、ND、EICAS、FMC、MAP，
支持 X-Plane Connect、FMC→ND 共享航路和客舱声光报警。

## 一键构建

在安装 MSYS2 MinGW64 依赖后运行：

```powershell
$env:PATH = 'D:\MSYS2\mingw64\bin;' + $env:PATH
cmake -S . -B build-cmake -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build-cmake -j 4
ctest --test-dir build-cmake --output-on-failure
```

可执行文件统一输出到 `build/`。运行 `build/Cockpit.exe` 启动集成界面。

## 配置

- 高德 Key：环境变量 `AMAP_API_KEY` 或本地文件 `assets/map.local.cfg`。
- X-Plane Connect：默认连接 `127.0.0.1:49009`。
- FMC 航路通过 Windows 命名共享内存自动同步给 ND。

本地密钥文件已加入 `.gitignore`，不要提交到版本库。
