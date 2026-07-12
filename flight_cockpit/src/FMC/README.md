# FMC 模块

本模块按 `FMC.doc` 的 7 个任务实现，采用 SDL2 加载 `assets/fmc.png` 作为面板，并在逻辑分辨率 `638 x 998` 上绘制页面内容。窗口缩放时保持原始宽高比，鼠标点击区域也会同步映射到逻辑坐标。

## 已实现功能

- 全部左右行选键、功能键、数字/符号键、字母/编辑键和 EXEC 键
- 矩形/圆形命中检测与鼠标悬停白框
- INDEX、STATUS、RTE、CLB、CRZ、DES、DEP/ARR、离场、进场页面
- 输入栏、DEL/CLR 编辑规则、EXEC 待执行灯
- `apt.dat` 机场 AVL 索引与 `earth_fix.dat` 航路点 AVL 索引
- 起始机场、目标机场、航班号、DIRECT 航路点输入与分页
- 爬升/巡航/下降参数输入及范围校验
- KSEA/KBFI 示例离场/进场程序、跑道、过渡点选择
- 可选 X-Plane UDP `CMND` 指令同步

## 运行与验证

VS Code 中可使用以下任务：

- `build FMC` / `run FMC`
- `test FMC`：执行数据和事件流程自检
- `build FMC XPlane` / `run FMC XPlane`

也可直接运行：

```text
build/FMC.exe --self-test
build/FMC.exe --smoke
build/FMC.exe --page route --screenshot fmc_route.bmp
build/FMC_XPlane.exe --xplane-host 127.0.0.1
```

键盘快捷键：`F1` INDEX、`F2` RTE、`F3` CLB、`F4` CRZ、`F5` DES、`F6` DEP/ARR；`PageUp/PageDown` 翻页；`Enter` 执行；`Backspace` 清除；`Delete` 进入 DELETE 模式；`Esc` 退出。
