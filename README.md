# 航空驾驶舱模拟系统｜暑假实习项目

本仓库用于保存暑假实习期间完成的航空驾驶舱模拟系统、需求分析报告、实习手册与答辩材料。

系统使用 **C17 + SDL2** 开发，通过模块化方式模拟民航驾驶舱的主要显示与控制设备，并支持从 **X-Plane** 获取实时飞行数据。

## 主要功能

- **Cockpit**：驾驶舱综合界面与模块启动入口
- **PFD**：主飞行显示器，显示姿态、空速、高度和航向等飞行参数
- **ND**：导航显示器，展示航路、航向及导航信息
- **EICAS 1 / EICAS 2**：发动机参数与机组告警显示
- **FMC**：飞行管理计算机界面及航路输入
- **MAP**：地图与航迹显示
- **X-Plane 联动**：通过 UDP 接收模拟器实时数据
- **模块通信**：FMC 航路可通过 Windows 共享内存同步到 ND

## 仓库结构

```text
.
├─ flight_cockpit/                 # 驾驶舱模拟系统源代码与资源
│  ├─ assets/                      # 字体、图片、导航数据与示例航路
│  ├─ src/                         # 各功能模块源代码
│  │  ├─ PFD/
│  │  ├─ ND/
│  │  ├─ EICAS1/
│  │  ├─ EICAS2/
│  │  ├─ FMC/
│  │  ├─ MAP/
│  │  └─ Util/
│  ├─ build/                       # Windows 可执行文件
│  └─ CMakeLists.txt               # CMake 构建配置
├─ 需求分析报告/                   # 项目需求分析文档
├─ 本科实习手册电子版-航空舱系统/  # 实习手册
└─ 答辩ppt/                        # 小组及个人答辩材料
```

## 环境要求

- Windows 10/11
- CMake 3.16 或更高版本
- 支持 C17 的 C 编译器（推荐 MSYS2 MinGW64）
- SDL2、SDL2_image、SDL2_ttf、SDL2_gfx
- libcurl、cJSON
- X-Plane 与 XPlaneConnect 插件（仅实时数据联动时需要）

在 MSYS2 MinGW64 中可安装依赖：

```bash
pacman -S --needed mingw-w64-x86_64-toolchain \
  mingw-w64-x86_64-cmake \
  mingw-w64-x86_64-SDL2 \
  mingw-w64-x86_64-SDL2_image \
  mingw-w64-x86_64-SDL2_ttf \
  mingw-w64-x86_64-SDL2_gfx \
  mingw-w64-x86_64-curl \
  mingw-w64-x86_64-cjson
```

## 构建与测试

在 `flight_cockpit` 目录中运行：

```powershell
$env:PATH = 'D:\MSYS2\mingw64\bin;' + $env:PATH
cmake -S . -B build-cmake -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build-cmake -j 4
ctest --test-dir build-cmake --output-on-failure
```

生成的程序位于 `flight_cockpit/build/`。运行综合入口：

```powershell
.\build\Cockpit.exe
```

也可以单独运行 `PFD.exe`、`ND.exe`、`EICAS1.exe`、`EICAS2.exe`、`FMC.exe` 或 `MAP.exe`。带 `_XPlane` 后缀的程序用于连接 X-Plane 实时数据。

## 配置说明

- X-Plane 数据连接默认使用 `127.0.0.1:49009`。
- 地图功能需要高德地图 API Key，可设置环境变量 `AMAP_API_KEY`，或使用本地配置文件 `flight_cockpit/assets/map.local.cfg`。
- API Key、个人配置、临时诊断文件及本机构建缓存不应提交到仓库。

## 项目资料

- `需求分析报告/`：系统功能、性能及接口需求
- `本科实习手册电子版-航空舱系统/`：实习过程记录与总结
- `答辩ppt/`：项目展示和个人答辩演示文稿

> 提示：仓库中的实习手册和答辩资料可能包含姓名、学号等个人信息。将仓库设为公开前，请先确认是否适合公开。

## 项目性质

本项目为本科暑假实习成果，主要用于课程实践、系统设计与学习交流。
