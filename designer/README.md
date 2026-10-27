# WinTk Designer

一个用 WinTk 自身实现的轻量级可视化 C UI 页面编辑器，作为 WinTk 仓库的独立子项目维护。

![WinTk Designer](../docs/designer.png)

## 功能

- 11 种控件：标签、按钮、输入框、多行文本、复选框、列表框、滑块、进度条、分隔线、面板、画布。
- 从控件库拖到画布，或单击添加；支持选择、移动和 8 个方向的边角缩放。
- 拖动时根据命中区域切换光标：移动为四向箭头，边角为斜向缩放，左右/上下为对应缩放光标；属性输入框和代码区显示文本 I 型光标。
- 左侧控件库、右侧属性面板、底部代码面板均可通过分隔条调整大小。
- 窗口缩小时自动压缩标题栏按钮，代码面板收起到标题栏高度，优先保留属性面板；放大后自动恢复。
- 属性编辑：变量名、文本、X/Y/宽/高、字号、粗体、复选框选中态、主按钮样式。
- 单击控件库添加时会自动寻找空位，减少控件全部堆在页面中心；实时预览中的进度条会自动往返演示。
- 8px 网格吸附；方向键微调，`Shift+方向键` 按网格移动，`Delete` 删除，`Esc` 取消选择。
- `Ctrl+Z` / `Ctrl+Y` 撤销/重做；顶部按钮同样可用。
- 风格菜单动态加载 `styles/*.dll` 插件，列出 Windows 原生、Fusion 和 WinTk；菜单末尾可导入外部风格 DLL。
- 内置插件源码在 `styles/`，编译产物是 `build/styles/wintk_builtin_styles.dll`；Designer.exe 不包含 Native/Fusion 绘制代码。
- 风格 DLL 通过 `styles/style_plugin.h` 导出 `wintk_style_plugin_get()`；每个插件可提供多个风格，每个风格携带主题和 `WtkStylePainter`。
- 实时预览窗口使用真实 WinTk 控件；导出一个完整、可编译的 `.c` 文件，包含 `build_page()`、`WinMain()`，并保留当前选择的应用风格。
- 生成的代码使用 `wtk_place` 绝对布局，页面已设置白色背景；控件事件由开发者继续通过 `wtk_bind` 添加。

## 独立构建

单独构建 Designer（会一并编译一份框架源码，不依赖根目录已有产物）：

```bat
mingw32-make -C designer
designer\build\wintk_designer.exe
```

构建会同时生成 `designer\build\styles\wintk_builtin_styles.dll`。Designer 从 exe 同目录的 `styles\` 扫描风格插件。

或只用 CMake：

```bat
cmake -S designer -B designer\build-cmake -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build designer\build-cmake -j
designer\build-cmake\bin\wintk_designer.exe
```

CMake 产物同样包含 `designer\build-cmake\bin\styles\wintk_builtin_styles.dll`。

从仓库根目录构建时，CMake 会通过 `add_subdirectory(designer)` 构建它；根 Makefile 的 `designer` 目标会调用这个子项目的 Makefile。

## 操作

1. 从左侧拖入控件，或单击控件库在页面中央添加。
2. 在画布上拖动控件移动；拖动选中框的 8 个控制点缩放。
3. 在右侧修改属性，按 Enter 或点击“应用属性”。
4. 点击页面空白处编辑页面标题和尺寸。
5. 使用“预览”检查真实控件效果，“导出 C...”保存源码。

## 运行时风格

核心 WinTk 默认使用原始 GDI 自绘。Designer.exe 只包含插件加载器和 WinTk GDI，启动时从 `styles/` 加载风格 DLL：

- `Windows 原生`：内置 DLL 实现 UxTheme 原生外观
- `Fusion`：内置 DLL 实现中性灰 Qt Fusion 方向外观
- `WinTk`：核心 GDI，不注册 painter
- 外部业务风格：菜单“导入风格 DLL...”选择插件后立即加入列表

Designer 会扫描 exe 同目录的 `styles/*.dll`。移动 Designer 时请把 `styles` 文件夹一起复制。插件 ABI 和编写方式见 `styles/README.md`。

启动前可通过兼容提示选择初始风格：

```bat
set WINTK_STYLE=native
designer\build\wintk_designer.exe
```

可选提示为 `auto`、`native`、`fusion`（或 `qt`）、`wintk`。没有安装 painter 的普通 WinTk 程序只会使用核心 GDI。

导出 `.c` 时会调用 `designer_style_apply()`；需要把 `designer/styles/designer_style.c` 作为伴生模块一起编译：

```bat
gcc -std=c11 -DUNICODE -D_UNICODE ^
    -Iinclude -Isrc -Idesigner/styles ^
    page.c designer\styles\designer_style.c build\libwintk.a ^
    -o page.exe -mwindows -luser32 -lgdi32 -lcomctl32 -luxtheme
```

## 当前限制

- 没有独立工程文件保存/打开功能；导出的 C 文件就是交付物。
- 控件库暂不含单选组，面板不提供嵌套容器语义。
- 生成代码中的事件处理需要继续手写。
