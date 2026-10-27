# WinTk

一个用 **C11 + Win32 + GDI** 写的轻量级 Windows 桌面 UI 框架，API 思路接近 Python 的 tkinter：创建控件、用 `pack` / `grid` 布局、绑定回调，然后进入主循环。

没有 Tcl，没有解释器，没有 WebView，也没有第三方运行时。控件都是真正的 Win32 窗口；按钮、标签、滑块、进度条和画布由 WinTk 自己绘制，输入框、文本区和列表框则嵌入原生 `EDIT` / `LISTBOX` 并套上自绘边框。

界面与文档均为中文，同时完整支持 UTF-8、中文字体回退和原生输入法。

![WinTk Gallery 中文总览](docs/gallery-dashboard.png)

更多界面：[控件页](docs/gallery-widgets.png) · [画布页](docs/gallery-canvas.png) · [关于页](docs/gallery-about.png) · [可视化设计器](docs/designer.png)

## 特性

- **Tk 风格 API**：`wtk_window`、`wtk_label`、`wtk_button`、`wtk_pack`、`wtk_grid`、`wtk_place`、`wtk_bind`。
- **12 种控件**：Frame、Label、Button、Entry、Text、Listbox、Checkbox、Radio、Slider、Progress、Separator、Canvas。
- **三种布局管理器**：
  - `pack`：支持 side、fill、expand、外部 padding。
  - `grid`：支持 row/column span、sticky、列/行权重与最小尺寸。
  - `place`：绝对坐标 + 固定尺寸，供可视化设计器和像素级表单使用。
- **核心只保留原始 GDI 自绘**：WinTk 库默认使用原来的圆角、主题色 GDI 控件；输入框、文本区和列表框仍是原生 `EDIT` / `LISTBOX`。
- **风格绘制可注入**：核心提供 `WtkStylePainter` 绘制钩子；Native/Fusion 风格由 Designer 的风格模块实现，普通 WinTk 程序不会携带这些绘制代码。
- **Designer 风格菜单**：Designer 可选择 Windows 原生、Fusion 或 WinTk 外观，并在导出代码中附带所选风格的绘制模块。
- **可视化设计器子项目**：`designer/` 可独立构建，支持拖拽控件、可拉伸分栏、上下文光标、撤销/重做、风格菜单、实时预览和 C 源码导出。
- **统一事件模型**：CLICK、CHANGE、SUBMIT、FOCUS、鼠标、滚轮、键盘、RESIZE、CLOSE。
- **键盘可用**：Tab / Shift+Tab 焦点循环、按钮 Enter/Space 激活、方向键操作滑块和单选组、Ctrl+Enter 提交多行文本。
- **原生输入控件**：编辑框、多行文本和列表框基于 Win32 原生控件，同时拥有 GDI 宿主边框、焦点状态、占位符和统一配色。
- **中文支持**：全 UTF-8 API；按 Windows 界面语言自动选择微软雅黑/微软正黑等 CJK 字体；原生 EDIT 直接使用系统中文输入法。
- **GDI 双缓冲视觉**：自绘控件、圆角卡片、主题色、按系统语言选择的 UI 字体和高 DPI 缩放。
- **UTF-8 API**：所有公开字符串均为 UTF-8，内部转换到 UTF-16；菜单、窗口标题和消息框使用 Unicode Win32 API。
- **额外能力**：原生菜单栏、消息框、确认框、一次性定时器、窗口居中/关闭事件、Canvas 鼠标事件和原生 HWND 逃生口。
- **容易构建**：MinGW-w64 Makefile、CMake、一键 `build.bat`。

## 快速开始

### 环境

- Windows 10 / 11
- MinGW-w64（已验证 GCC 16.2），需要 `windres`
- 或使用 CMake 生成 MinGW 工程

### 构建

```bat
mingw32-make
```

产物：

```text
build\libwintk.a
build\hello.exe
build\wintk_gallery.exe
designer\build\wintk_designer.exe
```

运行完整演示：

```bat
build\wintk_gallery.exe
```

运行可视化页面设计器（设计器是独立子项目，也可以单独构建）：

```bat
mingw32-make -C designer
designer\build\wintk_designer.exe
```

也可以双击 `build.bat`（内部会调用 `mingw32-make` 或 `make`）。

使用 CMake：

```bat
cmake -S . -B build-cmake -G "MinGW Makefiles"
cmake --build build-cmake -j
build-cmake\bin\wintk_gallery.exe
build-cmake\bin\wintk_designer.exe
```

### 最小程序

```c
#include "wintk.h"

static void on_click(WtkWidget *button, WtkEvent *event, void *userdata)
{
    (void)button;
    (void)event;
    wtk_label_set_text((WtkWidget *)userdata, "你好，原生 C！");
}

static int run(void)
{
    WtkApp *app = wtk_app_new();
    WtkWidget *win = wtk_window(app, "你好，WinTk", 560, 360);
    WtkWidget *title = wtk_label(win, "一个中文原生窗口");
    WtkWidget *button = wtk_button(win, "点我");

    /* NULL 使用按系统语言选择的 UI 字体（中文 Windows 下为微软雅黑 UI）。 */
    wtk_widget_set_font(title, NULL, 20, WTK_FONT_BOLD);
    wtk_pack(title, WTK_SIDE_TOP, WTK_FILL_X, 0, 28, 28);
    wtk_pack(button, WTK_SIDE_TOP, WTK_FILL_NONE, 0, 28, 10);
    wtk_bind(button, WTK_EVENT_CLICK, on_click, title);

    wtk_window_center(win);
    wtk_run(app);
    wtk_app_free(app);
    return 0;
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous,
                   LPSTR command, int show)
{
    (void)instance; (void)previous; (void)command; (void)show;
    return run();
}
```

编译时定义 `UNICODE` / `_UNICODE`，使用 GUI 子系统，并链接 `user32`、`gdi32`、`comctl32`：

```text
gcc -std=c11 -DUNICODE -D_UNICODE -Iinclude ^
    -finput-charset=UTF-8 -fexec-charset=UTF-8 ^
    app.c build\libwintk.a build\wintk-res.o ^
    -o app.exe -mwindows -luser32 -lgdi32 -lcomctl32 -luxtheme
```

如果源码中包含中文字符串，请保留 UTF-8 编译参数；MSVC 使用 `/utf-8`。

`resources/wintk.rc` 中的 manifest 会启用 Common Controls v6，并让程序在高 DPI 下使用清晰的 UI。自己的程序也建议一并链接这个资源，或提供等价的 manifest。

## 可视化页面编辑器：WinTk Designer

![WinTk Designer](docs/designer.png)

`designer/` 是一个独立子项目，也是一个用 WinTk 自身实现的轻量页面设计器：

- 左侧控件库包含标签、按钮、输入框、多行文本、复选框、列表框、滑块、进度条、分隔线、面板和画布，共 11 种。
- 将控件拖到页面，或单击控件库快速添加；在页面上拖动移动，拖动 8 个边角手柄缩放；默认 8 像素网格吸附。
- 左侧控件库、右侧属性面板和底部代码面板都带有可拖动的分隔条。
- 悬停控件、选中框和分隔条时会切换为对应的移动/缩放光标；属性输入框和代码区显示文本 I 型光标。
- 右侧属性面板可编辑变量名、文本、X/Y/宽/高、字号、粗体，以及复选框选中态和主按钮样式。
- 选中控件后按 `Delete` 删除，方向键微调位置，`Shift+方向键` 按网格移动；`Ctrl+Z` / `Ctrl+Y` 撤销/重做。
- 顶部“风格”菜单可在 Windows 原生、Fusion 和 WinTk 自定义之间即时切换。
- “预览”用真实 WinTk 控件打开独立窗口；底部实时显示生成的 C 代码，“导出 C...”写出完整单文件程序。
- 页面使用绝对布局，因此生成代码会调用 `wtk_place`；控件事件仍然由开发者在导出后通过 `wtk_bind` 添加。

生成代码的核心结构：

```c
WtkWidget *button1 = wtk_button(parent, "开始设计");
wtk_place(button1, 356, 146, 120, 38);
wtk_button_set_style(button1, WTK_BUTTON_PRIMARY);
wtk_widget_set_font(button1, NULL, 10, WTK_FONT_BOLD);
```

生成文件包含 `build_page()`、完整的 `WinMain()`，以及 `designer_style_apply()` 调用。选中风格由 Designer 的风格模块绘制；编译时要把 `designer/src/designer_style.c` 作为伴生模块一起编译，并链接 `user32`、`gdi32`、`comctl32`、`uxtheme`：

```bat
gcc -std=c11 -DUNICODE -D_UNICODE ^
    -Iinclude -Isrc -Idesigner/src ^
    page.c designer\src\designer_style.c build\libwintk.a ^
    -o page.exe -mwindows -luser32 -lgdi32 -lcomctl32 -luxtheme
```

WinTk 原始 GDI 风格不依赖 `designer_style.c`；只有 Native/Fusion 需要。

设计器子项目可以单独构建：

```bat
mingw32-make -C designer
designer\build\wintk_designer.exe
```

```bat
cmake -S designer -B designer\build-cmake -G "MinGW Makefiles"
cmake --build designer\build-cmake -j
designer\build-cmake\bin\wintk_designer.exe
```

更多说明见 [designer/README.md](designer/README.md)。

设计器目前没有独立工程文件保存/打开、单选组和嵌套容器；它的定位是快速搭出页面骨架，然后让开发者继续在 C 代码里补业务逻辑和事件。

## 项目结构

```text
include/wintk.h        公开 API
src/wtk_internal.h     内部对象结构
src/wtk_core.c         应用、顶层窗口、消息循环、焦点、菜单、定时器
src/wtk_widgets.c      控件创建、通用配置、事件分发、控件逻辑
src/wtk_native.c       原生 EDIT / LISTBOX 宿主、文本与列表 API
src/wtk_layout.c       pack / grid / place 测量与布局
src/wtk_draw.c         所有自绘控件与主题绘制
examples/hello.c       小例子
examples/gallery.c     完整中文演示（分页、表单、Canvas、主题）
designer/              独立可视化设计器子项目
designer/src/designer.c       界面、画布、分隔条、属性面板与预览
designer/src/designer_model.c/.h  设计文档模型与 C 代码生成器
resources/             manifest、图标
docs/API.md            详细 API 参考
docs/designer.png      WinTk Designer 截图
```

## 核心概念

### 对象与生命周期

`WtkApp` 拥有所有窗口和控件。控件之间用父指针和子数组关联；你始终使用不透明的 `WtkWidget *`。

- 工厂函数失败时返回 `NULL`。
- 文本会被复制；`wtk_*_get_text` 返回的指针由控件持有，在下一次文本变化或控件销毁后失效。
- 回调里可以 `wtk_widget_destroy()` 当前控件。WinTk 会先销毁 HWND，把对象放入延迟回收队列，等消息分发结束后再真正 `free`。
- 关闭最后一个窗口会退出 `wtk_run`。最后统一调用 `wtk_app_free`。
- 所有 UI 操作必须在创建 `WtkApp` 的同一线程执行。

### pack 布局

```c
wtk_pack(widget, WTK_SIDE_TOP, WTK_FILL_BOTH, 1, 16, 8);
/*              方向            填充          扩展  padx pady */
```

- `WTK_SIDE_TOP/BOTTOM/LEFT/RIGHT`：贴在父容器哪一边。
- `WTK_FILL_X/Y/BOTH`：在 parcel 内是否填满交叉轴。
- `expand`：是否参与该方向的剩余空间分配。
- 同一个父容器请只使用一种布局管理器（`pack`、`grid` 或 `place`）。
- 混合方向时，WinTk 使用简化版 Tk pack 算法；常见的一行、一列和侧边栏布局与 Tk 一致。

### grid 布局

```c
wtk_grid(card, row, column, row_span, column_span,
         WTK_STICKY_ALL, padx, pady);

wtk_grid_column_configure(parent, 1, 1, 160); /* 权重 1，最小宽 160 */
wtk_grid_row_configure(parent, 0, 1, 0);
```

如果没有显式配置列权重，那么粘满水平方向的子控件会让对应列获得权重 `1`，所以常见的“一列标签 + 一列拉伸输入框”表单不需要额外配置。显式配置后，以配置为准。

### place 布局

```c
wtk_place(widget, 40, 36, 320, 44);
/*           x   y   width height，均为逻辑像素 */
```

- 坐标相对于父容器的内容区（扣除 padding 和边框）。
- 固定位置和尺寸，不参与自动重排；适合像素级表单和可视化设计器。
- 控件仍然会随系统 DPI 缩放，但父容器尺寸变化不会移动它。
- 通常由 WinTk Designer 生成，也可以手写。
- 一个父容器不要混用 `place` 与 `pack` / `grid`。

### 逻辑像素与 DPI

- 布局、padding、尺寸、字号、圆角均使用 **96 DPI 逻辑单位**，WinTk 根据系统 DPI 自动缩放。
- 鼠标事件坐标、Canvas 的绘制尺寸使用 **设备像素**。
- 字体大小是磅值（point size）。
- 进程使用系统 DPI 感知；改变显示器或动态切换 DPI 后不会重建控件。

### 视觉风格

WinTk 核心的默认（也是唯一内置）外观是原来的 GDI 自绘。风格绘制通过公开的 `WtkStylePainter` 注入：

```c
WtkApp *app = wtk_app_new();          /* 核心 GDI 外观 */
wtk_app_set_style_painter(app, NULL, NULL); /* 恢复核心 GDI */
```

- 核心不包含 Native/Fusion 的控件绘制代码。
- Designer 内置 `designer/src/designer_style.c`，实现 Windows 原生和 Fusion 外观；WinTk 风格不注册 painter，直接使用核心 GDI。
- `WTK_STYLE_*` 和 `wtk_app_set_style()` 保留为兼容性“风格提示”，本身不会改变核心绘制；必须注册 painter 才有 Native/Fusion 效果。
- `WtkStylePainter` 提供 `paint(widget, hdc, bounds, state)` 和可选的 `field_metrics(...)`；`state` 暴露控件类型、文本、字体、颜色、状态、值和尺寸。
- `wtk_theme_default()` / `wtk_theme_fusion()` / `wtk_theme_native()` 提供 GDI 调色板；Designer 会切换到对应主题。
- `WINTK_STYLE` 环境变量仍可作为提示使用，但没有 painter 时不会改变核心绘制。
- 原生 EDIT / LISTBOX 始终由 Windows 绘制；painter 通过 `field_metrics` 控制其宿主内边距。

Designer 的“风格”菜单提供 Windows 原生、Fusion 和 WinTk。窗口整体截图仍由 Gallery 展示原始 GDI 外观：[Gallery](docs/gallery-dashboard.png)；Designer 的原生外观：[Designer](docs/designer.png)。

### 中文、Unicode 与输入法

公开字符串统一使用 UTF-8，窗口标题、菜单、消息框和所有自绘文本在内部转为 UTF-16，因此源码中可以直接写中文：

```c
WtkWidget *title = wtk_label(win, "数据查看器");
WtkWidget *button = wtk_button(win, "确定");
wtk_window_set_title(win, "统计面板");
```

WinTk 会根据 Windows 界面语言自动选择默认 UI 字体：

- 简体中文：`Microsoft YaHei UI`（微软雅黑 UI）
- 繁体中文：`Microsoft JhengHei UI`（微软正黑 UI）
- 其他语言：`Segoe UI`，日文/韩文系统会优先使用对应 CJK 字体

`wtk_app_ui_font_family(app)` 返回当前选中的字体名（UTF-8）。调用 `wtk_widget_set_font(widget, NULL, size, style)` 会使用该字体；显式传入 `"Microsoft YaHei UI"` 等字体名则可以覆盖。即使系统 UI 语言不是中文，只要安装了中文字体，GDI 通常仍会做字体链接；需要严格控制字形时请显式指定中文字体。

**输入法：** Entry 和 Text 直接封装原生 `EDIT` 控件，中文输入法的组合、候选窗口、复制粘贴都由 Windows 原生处理，WinTk 不会拦截 `WM_IME_*` 消息。需要用户输入中文时，请使用 Entry 或 Text；自绘的 Label、Button、Canvas 只负责显示和事件。

**源码编码：** 构建脚本已经传入 `-finput-charset=UTF-8 -fexec-charset=UTF-8`（MinGW）或 `/utf-8`（MSVC/CMake），确保源码中的中文字符串按 UTF-8 编译。自行编译时也请带上对应参数。

需要注意 `strlen()` 返回的是 UTF-8 字节数，不是汉字个数；Gallery 的草稿统计使用了一个简单的 UTF-8 码点计数器。Canvas 回调里绘制中文时，需要自行选入包含中文字形的 HFONT，例如使用 `wtk_app_ui_font_family(app)` 创建字体。

### 事件

```c
wtk_bind(widget, WTK_EVENT_CLICK, callback, userdata);
wtk_bind(entry,  WTK_EVENT_SUBMIT, callback, userdata);
wtk_unbind(widget, WTK_EVENT_CLICK, callback);
```

回调签名：

```c
void callback(WtkWidget *widget, WtkEvent *event, void *userdata);
```

常用事件按控件：

| 控件 | 主要事件 | 说明 |
|---|---|---|
| Window | `CLOSE`, `RESIZE` | `CLOSE` 中设置 `event->cancel = 1` 可阻止关闭 |
| Button | `CLICK` | 鼠标或 Enter/Space |
| Entry | `CHANGE`, `SUBMIT`, `FOCUS_IN/OUT` | Enter 触发 `SUBMIT` |
| Text | `CHANGE`, `SUBMIT` | Ctrl+Enter 触发 `SUBMIT` |
| Listbox | `CHANGE`, `DOUBLE_CLICK` | `event->value` 是索引 |
| Checkbox | `CHANGE` | `event->value` 为 0/1 |
| Radio group | `CHANGE` | 绑定在 `wtk_radiogroup` 上，`event->value` 是选项索引 |
| Slider | `CHANGE`, `FOCUS_*` | `event->value` 是新值 |
| Canvas | 鼠标、滚轮、键盘、`DOUBLE_CLICK` | 需要焦点时先点击画布 |

`WtkEvent` 还有 `x/y`（设备像素）、`button`、`key`（Win32 VK 码）、`modifiers`、`width/height`、`delta` 等字段。

### 控件速查

| 工厂 | 获取/设置 | 备注 |
|---|---|---|
| `wtk_frame(parent)` | 通用样式 | 容器；可设背景、边框、padding |
| `wtk_label(parent, text)` | `wtk_label_set_text` | 支持对齐和自动换行宽度 |
| `wtk_button(parent, text)` | `wtk_button_set_text/style/align` | Primary/Secondary/Ghost/Danger |
| `wtk_entry(parent)` | `wtk_entry_get_text`, `set_text` | 原生 EDIT，占位符、密码、只读 |
| `wtk_text(parent)` | `wtk_text_get_text`, `set_text` | 原生多行 EDIT，纵向滚动 |
| `wtk_listbox(parent)` | `add/clear/select/get_item` | 原生 LISTBOX |
| `wtk_checkbox(parent, text, checked)` | `get/set` | |
| `wtk_radiogroup(parent)` | `get/set` | 把 `wtk_radio(group, text)` 加到组里 |
| `wtk_slider(parent,min,max,value)` | `get/set/set_step` | 自绘 |
| `wtk_progress(parent)` | `set(fraction)`, `set_show_percent` | 自绘 |
| `wtk_separator(parent)` | `wtk_widget_set_border` | 细分隔线 |
| `wtk_canvas(parent,w,h)` | `wtk_canvas_set_draw` | GDI 双缓冲画布 |

通用配置：

```c
wtk_widget_set_background(w, WTK_RGB(245, 247, 251));
wtk_widget_set_foreground(w, theme->muted);
wtk_widget_set_font(w, NULL, 14, WTK_FONT_BOLD); /* 使用本地化 UI 字体 */
/* 也可以指定字体：wtk_widget_set_font(w, "Microsoft YaHei UI", 14, 0); */
wtk_widget_set_padding(w, 16, 12, 16, 12);
wtk_widget_set_border(w, theme->border, 1, 12);
wtk_widget_set_size(w, 320, 44);
wtk_widget_set_cursor(w, WTK_CURSOR_HAND);
wtk_widget_set_visible(w, 1);
wtk_widget_set_enabled(w, 1);
```

颜色是 24 位 `0xRRGGBB`，用 `WTK_RGB(r, g, b)` 生成。`wtk_widget_set_border` 主要作用于 Frame、Button 和原生输入宿主。圆角和宽度使用逻辑单位。

### 主题

```c
WtkTheme theme;
wtk_theme_default(&theme);
theme.accent = WTK_RGB(0, 150, 136);
wtk_app_set_theme(app, &theme);
```

主题颜色在绘制时动态解析；已经显式设置过颜色的控件会保留显式颜色。修改主题会重绘所有窗口。

### Canvas

```c
static void draw(WtkWidget *canvas, HDC dc, int width, int height, void *data)
{
    (void)canvas;
    SetBkMode(dc, TRANSPARENT);
    Rectangle(dc, 20, 20, width - 20, height - 20);
    TextOutW(dc, 30, 30, L"GDI drawing", 11);
}

WtkWidget *canvas = wtk_canvas(parent, 640, 360);
wtk_canvas_set_draw(canvas, draw, NULL);
wtk_bind(canvas, WTK_EVENT_MOUSE_MOVE, on_mouse, NULL);
wtk_pack(canvas, WTK_SIDE_TOP, WTK_FILL_BOTH, 1, 0, 0);
```

绘制回调收到的是已经填充背景的双缓冲 HDC，可以直接调用 Win32 GDI。鼠标坐标是画布内的设备像素。

### 菜单、对话框与定时器

```c
WtkMenu *bar = wtk_menubar(window);
WtkMenu *file = wtk_menu_submenu(bar, "文件(&F)");
wtk_menu_add_command(file, "退出(&X)\tAlt+F4", on_exit, app);

wtk_message_box(window, "提示", "这是一个中文消息框。", WTK_ICON_INFO);
wtk_ask_yes_no(window, "确认", "确定要继续吗？");

wtk_after(app, 1000, on_timer, data); /* 一次性定时器 */
```

菜单支持子菜单、分隔符和命令回调。`\t` 后面的文本会作为加速键提示绘制出来，但 WinTk v1 不安装全局键盘加速键；需要快捷键时请用控件的键盘事件或 Win32 的 `RegisterHotKey`。

### 设计自己的窗口

- 用 `wtk_widget_set_background()` 设置卡片/侧边栏背景。
- 用 `wtk_widget_set_border(w, color, 1, radius)` 获得圆角卡片。
- Frame 的 padding 决定内容与边框的内边距。
- 多个页面可以做成同级的 Frame，再切换 `wtk_widget_set_visible()`。
- 自定义布局时，`wtk_widget_hwnd()` 可以拿到宿主 HWND，与原生 Win32 代码互操作。

## 与 tkinter 的对照

| tkinter | WinTk |
|---|---|
| `Tk()` + `mainloop()` | `wtk_app_new()` + `wtk_run()` |
| `Label(root, text=...)` | `wtk_label(root, "...")` |
| `.pack(side=..., fill=...)` | `wtk_pack(w, side, fill, expand, padx, pady)` |
| `.grid(row=..., column=...)` | `wtk_grid(w, row, col, rs, cs, sticky, px, py)` |
| `.place(x=..., y=..., width=..., height=...)` | `wtk_place(w, x, y, width, height)` |
| `.bind("<Button-1>", cb)` | `wtk_bind(w, WTK_EVENT_MOUSE_DOWN, cb, data)` |
| `command=callback` | `wtk_bind(button, WTK_EVENT_CLICK, callback, data)` |
| Canvas | `wtk_canvas` + GDI 回调 |
| Tcl variables / traces | 暂未实现，需要自己保存状态 |

WinTk 不是 Tk 的绑定，也没有实现完整的 Tk 几何算法、anchor、滚动容器或 Tcl 变量系统。它是“如果你喜欢 tkinter 的模型，但想用 C 和原生 Win32 写一个小工具”时的起点。

## 当前限制

- v1 不含滚动容器、PanedWindow、Treeview、Table、ComboBox、Tooltip、图片控件。
- 没有 anchor 配置；pack 交叉轴默认靠左/靠上，grid 未设置 sticky 时默认居中。
- `place` 不会随父容器重排；缩放窗口时固定坐标表单不会自适应。
- 没有自动滚动条，也没有控件虚拟化；内容超出窗口时会被裁剪。
- WinTk Designer 是轻量版：没有独立工程文件、单选组和嵌套容器；生成代码后需要继续手写事件。
- 核心只包含 WinTk 原始 GDI 自绘。Native/Fusion 是 Designer 提供的画笔模块，不在 `libwintk.a` 内；不注册 painter 时 `WTK_STYLE_*` 只是提示。
- Native/Fusion 通过宿主 GDI 绘制，不是把每个按钮替换成独立的原生 BUTTON 子窗口；原生 EDIT 和 LISTBOX 仍是真实系统控件。
- 菜单、系统消息框和文件对话框始终使用 Windows 原生实现，不随 painter 重绘。
- 系统 DPI 感知，不支持跨显示器动态重建字体和布局。
- 默认 UI 字体会随 Windows 界面语言变化；如果需要跨语言完全一致的字形，请显式设置字体。
- 中文输入依赖原生 EDIT；自绘控件本身不是文本输入控件。
- 菜单加速键提示是展示性的，不自动注册快捷键。
- 自定义控件依赖 GDI，不支持半透明、动画或 Direct2D。
- 单 UI 线程；控件和事件都不做加锁。
- 以 MinGW-w64 + Windows 10/11 为主要构建/测试环境；CMake 工程已提供，但 MSVC 未被 CI 覆盖。

## 文档

- [API 参考](docs/API.md)
- [Hello 示例](examples/hello.c)
- [完整 Gallery](examples/gallery.c)
- [WinTk Designer 子项目](designer/README.md)
- [设计器界面](designer/src/designer.c)
- [设计文档模型与 C 代码生成器](designer/src/designer_model.c)

## License

MIT，见 [LICENSE](LICENSE)。
