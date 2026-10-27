# WinTk API 参考

对应版本：1.0.0。公开头文件：`include/wintk.h`。

所有公开文本使用 UTF-8 编码。布局尺寸、padding、边框、圆角和字号都使用 96 DPI 逻辑单位；鼠标事件坐标和 Canvas 绘制尺寸使用设备像素。WinTk 核心只实现原始 GDI 自绘；Native/Fusion 外观由 Designer 的 `WtkStylePainter` 模块提供。

---

## 1. 基础类型

### WtkApp / WtkWidget / WtkMenu

不透明句柄：

```c
typedef struct WtkApp WtkApp;
typedef struct WtkWidget WtkWidget;
typedef struct WtkMenu WtkMenu;
```

- `WtkApp` 拥有窗口、控件、菜单、定时器。
- `WtkWidget` 可能是窗口、容器或具体控件。
- `WtkMenu` 由对应的 `wtk_menubar()` 创建并拥有。

### WtkColor

24 位 RGB：

```c
typedef uint32_t WtkColor; /* 0x00RRGGBB */

WtkColor accent = WTK_RGB(45, 91, 255);
```

注意：这不是 Win32 的 `COLORREF`（后者是 `0x00BBGGRR`）。WinTk 内部负责转换。

### WtkEvent

```c
typedef struct WtkEvent {
    WtkEventType type;
    int x, y;
    int width, height;
    int button;
    int clicks;
    int key;
    unsigned modifiers;
    int value;
    int delta;
    int cancel;
} WtkEvent;
```

| 字段 | 含义 |
|---|---|
| `type` | 事件类型 |
| `x`,`y` | 鼠标位置，设备像素，相对于控件客户区 |
| `width`,`height` | `RESIZE` 事件的新尺寸，设备像素 |
| `button` | `WTK_MOUSE_LEFT/RIGHT/MIDDLE` |
| `clicks` | 点击次数 |
| `key` | Win32 虚拟键码，例如 `VK_RETURN` |
| `modifiers` | `WTK_MOD_SHIFT / CONTROL / ALT` 位掩码 |
| `value` | 依赖事件的数值：滑块值、列表索引、复选状态、单选索引 |
| `delta` | 滚轮增量 |
| `cancel` | 仅 `CLOSE`；设为非零可取消关闭 |

### 回调类型

```c
typedef void (*WtkEventFn)(WtkWidget *widget, WtkEvent *event, void *userdata);
typedef void (*WtkDrawFn)(WtkWidget *canvas, HDC dc, int width, int height,
                          void *userdata);
typedef void (*WtkTimerFn)(WtkApp *app, void *userdata);
typedef void (*WtkMenuFn)(WtkWidget *window, void *userdata);
```

### 枚举 / 常量

| 类型 | 值 |
|---|---|
| `WtkSide` | `WTK_SIDE_TOP`, `WTK_SIDE_BOTTOM`, `WTK_SIDE_LEFT`, `WTK_SIDE_RIGHT` |
| `WtkFill` | `WTK_FILL_NONE`, `WTK_FILL_X`, `WTK_FILL_Y`, `WTK_FILL_BOTH` |
| `WtkSticky` | `WTK_STICKY_LEFT/TOP/RIGHT/BOTTOM`，`WTK_STICKY_HORIZONTAL/VERTICAL/ALL` |
| `WtkAlign` | `WTK_ALIGN_LEFT/CENTER/RIGHT` |
| `WtkButtonStyle` | `WTK_BUTTON_PRIMARY/SECONDARY/GHOST/DANGER` |
| `WtkStyle` | `WTK_STYLE_AUTO/FUSION/NATIVE/WINTK`（兼容性提示） |
| `WtkCursor` | `WTK_CURSOR_ARROW/HAND/IBEAM/CROSS/SIZE_H/SIZE_V/SIZE_ALL/SIZE_NWSE/SIZE_NESW/NO` |
| `WtkIcon` | `WTK_ICON_NONE/INFO/WARNING/ERROR/QUESTION` |
| `WtkMouseButton` | `WTK_MOUSE_LEFT=1`, `WTK_MOUSE_RIGHT=2`, `WTK_MOUSE_MIDDLE=3` |
| 字体样式 | `WTK_FONT_NORMAL/BOLD/ITALIC/UNDERLINE`（可按位或） |

### 事件类型

```text
WTK_EVENT_CLICK            WTK_EVENT_DOUBLE_CLICK
WTK_EVENT_CHANGE           WTK_EVENT_SUBMIT
WTK_EVENT_FOCUS_IN         WTK_EVENT_FOCUS_OUT
WTK_EVENT_MOUSE_DOWN       WTK_EVENT_MOUSE_UP
WTK_EVENT_MOUSE_MOVE       WTK_EVENT_MOUSE_ENTER
WTK_EVENT_MOUSE_LEAVE      WTK_EVENT_MOUSE_WHEEL
WTK_EVENT_KEY_DOWN         WTK_EVENT_KEY_UP
WTK_EVENT_RESIZE           WTK_EVENT_CLOSE
```

---

## 2. 主题

```c
typedef struct WtkTheme {
    WtkColor window_bg;
    WtkColor surface;
    WtkColor surface_alt;
    WtkColor text;
    WtkColor muted;
    WtkColor border;
    WtkColor accent;
    WtkColor accent_hover;
    WtkColor accent_pressed;
    WtkColor on_accent;
    WtkColor success;
    WtkColor warning;
    WtkColor danger;

    int corner_radius;   /* logical */
    int control_height;  /* logical */
} WtkTheme;

void wtk_theme_default(WtkTheme *theme); /* 核心默认 GDI 调色板 */
void wtk_theme_fusion(WtkTheme *theme);  /* Fusion 调色板（供 painter 使用） */
void wtk_theme_native(WtkTheme *theme);  /* 系统色原生调色板 */
void wtk_app_set_theme(WtkApp *app, const WtkTheme *theme);
const WtkTheme *wtk_app_get_theme(WtkApp *app);
```

- `wtk_theme_default()` 是核心默认的原 WinTk 浅蓝/白色调色板。
- `wtk_theme_fusion()` 是 Qt Fusion 方向的中性灰调色板，供 Designer 的 painter 使用。
- `wtk_theme_native()` 从 `GetSysColor` 生成系统色原生调色板。
- `wtk_app_new()` 始终从 `wtk_theme_default()` 开始；Native/Fusion 由 Designer 的 `designer_style_apply()` 切换主题。
- 修改主题会重绘所有现有窗口，并更新原生输入控件的背景画刷。已经显式设置过的控件颜色不会被覆盖。
- `corner_radius` / `control_height` 在创建控件时确定默认值；主题切换不会自动改写已创建控件的尺寸。

---

## 3. 应用生命周期

```c
WtkApp *wtk_app_new(void);
void wtk_app_free(WtkApp *app);

int wtk_run(WtkApp *app);
void wtk_quit(WtkApp *app);

int wtk_app_dpi(WtkApp *app);
int wtk_scale(WtkApp *app, int logical_pixels);
const char *wtk_app_ui_font_family(WtkApp *app);

void wtk_app_set_style(WtkApp *app, WtkStyle style);
WtkStyle wtk_app_get_style(WtkApp *app);
WtkStyle wtk_app_resolved_style(WtkApp *app);
const char *wtk_style_name(WtkStyle style);
```

### wtk_app_new

- 设置进程 DPI 感知。
- 初始化 Common Controls。
- 注册窗口类并创建隐藏的消息窗口。
- 初始化核心的原始 GDI 主题 `wtk_theme_default()`。
- 根据 Windows 界面语言选择默认 UI 字体，并缓存其 UTF-8 名称。
- 读取可选的 `WINTK_STYLE` 环境变量作为兼容性风格提示；默认请求 `WTK_STYLE_AUTO`。
- 必须在任何 Win32 窗口创建前调用一次比较稳妥。

### wtk_run

- 显示尚未显示且 `visible` 的顶层窗口。
- 进入 Win32 消息循环，直到 `wtk_quit()` 或所有窗口关闭。
- 返回退出消息的 `wParam`；已经运行时返回 `-1`。
- 在消息循环中接管 Tab / Shift+Tab 焦点切换。

### wtk_app_free

- 销毁所有窗口和控件。
- 释放定时器、菜单和延迟回收的控件。
- 调用后不要继续使用任何 WinTk 句柄。

### 视觉风格

```c
wtk_app_set_style(app, WTK_STYLE_WINTK); /* 兼容性提示；核心始终 GDI */
wtk_app_set_style(app, WTK_STYLE_NATIVE);
wtk_app_set_style(app, WTK_STYLE_FUSION);
```

- 核心只绘制原始 WinTk GDI 外观；`WTK_STYLE_*` 和 `wtk_app_set_style()` 仅保留为兼容性提示，不会改变核心绘制。
- 要让 Native/Fusion 生效，需要注册 `WtkStylePainter`；Designer 提供的模块就在 `designer/src/designer_style.c`。
- `wtk_app_set_style_painter(app, painter, userdata)` 安装画笔；传 `NULL` 恢复核心 GDI。
- painter 的 `paint(widget, hdc, bounds, state)` 覆盖控件/窗口绘制；`field_metrics` 控制原生 EDIT/LISTBOX 的宿主内边距。
- `WtkPaintState` 包含控件类型、文本、字体、颜色、状态、值、尺寸和按钮样式；可参考 `include/wintk.h`。
- Canvas 的 `draw_callback` 始终由核心执行，painter 不覆盖 Canvas 内容。
- 原生 EDIT/LISTBOX 始终是 Windows 控件；painter 只支配其宿主边框和布局尺寸。
- `wtk_theme_default()`、`wtk_theme_fusion()`、`wtk_theme_native()` 是可选 GDI 调色板；Designer 会按风格设置。
- `WINTK_STYLE` 环境变量仍可作为兼容性提示（`auto/native/fusion/wintk`）。
- `wtk_style_name()` 返回 `"Auto"`、`"Fusion"`、`"Native"` 或 `"WinTk"`。

### 自定义风格画笔：WtkStylePainter

```c
typedef void (*WtkStylePaintFn)(WtkWidget *widget, HDC dc,
                                const RECT *bounds,
                                const WtkPaintState *state,
                                void *userdata);

typedef struct WtkStylePainter {
    const char *name;
    WtkStylePaintFn paint;
    WtkStyleFieldMetricsFn field_metrics;
} WtkStylePainter;

void wtk_app_set_style_painter(WtkApp *app,
                               const WtkStylePainter *painter,
                               void *userdata);
const WtkStylePainter *wtk_app_get_style_painter(WtkApp *app);
void *wtk_app_get_style_painter_data(WtkApp *app);
WtkWidgetKind wtk_widget_kind(WtkWidget *widget);
```

- 这是核心与外部风格模块之间的正式扩展点。核心自身始终走原始 GDI 绘制。
- `paint` 用于普通控件和窗口背景；核心会在双缓冲 HDC 上调用它。Canvas 内容仍由核心的 `draw_callback` 绘制。
- `field_metrics` 可选，用设备像素返回单行 EDIT/LISTBOX 的宿主 inset 和文本 margin。
- `WtkPaintState` 暴露：
  - 控件类型、文本、字体、背景/前景/边框和尺寸
  - hover/pressed/focused/checked/selected/enabled
  - 按钮样式、焦点环、进度、滑块值和只读/密码状态
- 注册 painter 会立即刷新现有窗口；传 `NULL` 恢复核心 GDI。
- `designer/src/designer_style.c` 是参考实现，同时提供 Native 和 Fusion 画笔。

### 尺寸与字体辅助

- `wtk_app_dpi()` 返回当前 DPI（常见为 96、120、144、192）。
- `wtk_scale(app, n)` 将 96 DPI 逻辑像素换算为设备像素。
- `wtk_app_ui_font_family()` 返回 WinTk 选定的本地化 UI 字体名（UTF-8，生命周期与 app 相同）：
  - 简体中文优先 `Microsoft YaHei UI`；
  - 繁体中文优先 `Microsoft JhengHei UI`；
  - 日文和韩文系统优先对应的 CJK UI 字体；
  - 其他语言为 `Segoe UI`。
  - 字体是否可用会通过 GDI 字体枚举检查；不可用时回退到 `Segoe UI`。
---

## 4. 窗口

```c
WtkWidget *wtk_window(WtkApp *app, const char *title,
                      int width, int height);
void wtk_window_show(WtkWidget *window);
void wtk_window_center(WtkWidget *window);
void wtk_window_set_title(WtkWidget *window, const char *title);
void wtk_window_set_min_size(WtkWidget *window, int width, int height);
void wtk_window_set_resizable(WtkWidget *window, int resizable);
void wtk_window_close(WtkWidget *window);
```

说明：

- 创建后窗口默认隐藏；`wtk_run()` 会自动显示，或手动调用 `wtk_window_show()`。
- `width/height` 是逻辑尺寸。
- `wtk_window_close()` 异步投递 `WM_CLOSE`，会先触发绑定在窗口上的 `WTK_EVENT_CLOSE`。
- 在 `WTK_EVENT_CLOSE` 回调中设置 `event->cancel = 1` 可拦截关闭。
- `wtk_window_set_resizable()` 在运行时修改窗口样式。
- 顶层窗口本身也是容器，可以直接往里 `pack` / `grid`。

窗口关闭后，对应的 `WtkWidget` 内存会在 `wtk_app_free()` 或延迟回收时释放。不要继续调用它的 API。

---

## 5. 菜单

```c
WtkMenu *wtk_menubar(WtkWidget *window);
WtkMenu *wtk_menu_submenu(WtkMenu *parent, const char *label);
void wtk_menu_add_command(WtkMenu *menu, const char *label,
                          WtkMenuFn callback, void *userdata);
void wtk_menu_add_separator(WtkMenu *menu);
```

- `wtk_menubar` 会创建并立即挂到窗口；重复调用返回已有菜单栏。
- 标签支持 `&` 助记符，例如 `"文件(&F)"`；菜单标签可以为中文。
- 标签中的 `\t` 之后是加速键提示，例如 `"&Save\tCtrl+S"`。该提示目前只绘制，不注册快捷键。
- 回调收到的是窗口指针。
- 菜单栏会自动缩小窗口客户区，触发重排。
- 菜单由 Windows 在窗口销毁时随之销毁；WinTk 只负责释放自己的结构。

示例：

```c
WtkMenu *bar = wtk_menubar(win);
WtkMenu *file = wtk_menu_submenu(bar, "文件(&F)");
wtk_menu_add_command(file, "退出(&X)\tAlt+F4", on_exit, app);
```

---

## 6. 布局（pack / grid / place）

### pack

```c
void wtk_pack(WtkWidget *widget, WtkSide side, WtkFill fill,
              int expand, int padx, int pady);
```

参数：

| 参数 | 含义 |
|---|---|
| `side` | 贴靠父容器哪一边 |
| `fill` | 在 parcel 中填充哪些轴 |
| `expand` | 是否分配主方向剩余空间 |
| `padx`,`pady` | 外部 padding，逻辑像素 |

示例：

```c
wtk_pack(sidebar, WTK_SIDE_LEFT, WTK_FILL_Y, 0, 0, 0);
wtk_pack(content, WTK_SIDE_LEFT, WTK_FILL_BOTH, 1, 0, 0);
wtk_pack(title, WTK_SIDE_TOP, WTK_FILL_X, 0, 24, 20);
```

当前算法处理常见的一行/一列非常直观；混合方向时采用简化分配。无 anchor 配置。

### grid

```c
void wtk_grid(WtkWidget *widget, int row, int column,
              int row_span, int column_span,
              WtkSticky sticky, int padx, int pady);

void wtk_grid_column_configure(WtkWidget *master, int column,
                               int weight, int min_width);
void wtk_grid_row_configure(WtkWidget *master, int row,
                            int weight, int min_height);
```

- `row/column` 从 0 开始。
- `sticky` 决定子控件在单元格内的贴靠和拉伸；未设置时居中。
- `weight > 0` 的列/行分配多余空间。
- 若没有显式配置某轴权重，则包含“粘满该轴”的子控件的列/行会自动获得权重 1。
- `min_width/min_height` 是逻辑像素，作为基础尺寸下限。

### place

```c
void wtk_place(WtkWidget *widget, int x, int y, int width, int height);
```

- `x/y/width/height` 都是 96 DPI 逻辑像素，WinTk 在内部转换为当前 DPI 的设备像素。
- 坐标原点是对父容器的内容区（已扣除 padding 和边框）。
- 使用 place 的父容器会进入绝对布局模式，子控件不参与 pack/grid 的自动分配。
- 同一父容器不要混用 place、pack、grid。
- 尺寸不会随父容器变化而重排，适合固定表单；WinTk Designer 的生成代码使用它。
- 控件仍然参与窗口裁剪、绘制和点击命中测试。

### 通用尺寸/padding

```c
void wtk_widget_set_size(WtkWidget *widget, int width, int height);
void wtk_widget_set_padding(WtkWidget *widget,
                            int left, int top, int right, int bottom);
```

- 对容器，padding 是内容区与边框之间的间距。
- 对 Label/Button，是内部文字区域 padding。
- 传 `0` 到 `set_size` 的某一轴表示自动测量；顶层窗口不建议用 0。

---

## 7. 通用控件配置

```c
WtkApp *wtk_widget_app(WtkWidget *widget);
WtkWidget *wtk_widget_parent(WtkWidget *widget);
HWND wtk_widget_hwnd(WtkWidget *widget);

void wtk_widget_set_background(WtkWidget *widget, WtkColor color);
void wtk_widget_set_foreground(WtkWidget *widget, WtkColor color);
void wtk_widget_set_font(WtkWidget *widget, const char *family,
                         int point_size, unsigned style);
void wtk_widget_set_padding(WtkWidget *widget,
                            int left, int top, int right, int bottom);
void wtk_widget_set_size(WtkWidget *widget, int width, int height);
void wtk_widget_set_border(WtkWidget *widget, WtkColor color,
                           int width, int radius);
void wtk_widget_set_visible(WtkWidget *widget, int visible);
void wtk_widget_set_enabled(WtkWidget *widget, int enabled);
void wtk_widget_set_cursor(WtkWidget *widget, WtkCursor cursor);
void wtk_widget_set_focus_ring(WtkWidget *widget, int enabled);
void wtk_widget_focus(WtkWidget *widget);
void wtk_widget_redraw(WtkWidget *widget);
void wtk_widget_destroy(WtkWidget *widget);
```

- `wtk_widget_hwnd` 返回宿主 HWND；Entry/Text/Listbox 的真正原生 child 需用 `GetWindow(hwnd, GW_CHILD)`。
- `wtk_widget_set_font(..., family, ...)` 的 `family` 为 `NULL` 或空字符串时，使用 `wtk_app_ui_font_family()` 返回的本地化字体。中文应用推荐传 `NULL` 或显式传 `"Microsoft YaHei UI"`。
- `set_cursor` 支持箭头、手型、I 型、十字、水平/垂直/全向缩放、斜向缩放和禁止光标。
- `wtk_widget_set_focus_ring(widget, 0)` 可关闭 Canvas 等自定义控件的强调色焦点框（分隔条使用），不影响键盘焦点。
- `set_border` 的 `width/radius` 为逻辑像素；主要用于 Frame、Button、原生输入宿主。
- 隐藏控件会从布局中排除；显示后重新参与布局。
- 禁用控件不参与 Tab 顺序，不接收鼠标。
- 销毁当前控件是安全的，对象会延迟释放。
- 顶层窗口使用 `wtk_widget_destroy` 会直接销毁 HWND，不触发 `WTK_EVENT_CLOSE`；需要拦截关闭流程请用 `wtk_window_close`。

---

## 8. 事件绑定

```c
void wtk_bind(WtkWidget *widget, WtkEventType type,
              WtkEventFn callback, void *userdata);
void wtk_unbind(WtkWidget *widget, WtkEventType type,
                WtkEventFn callback);
```

- 同一个事件可绑定多个回调，按绑定顺序调用。
- 删除回调使用函数指针精确匹配（无 userdata 参数）。
- 回调修改 `event->cancel` 只对 `WTK_EVENT_CLOSE` 有意义。
- 回调期间可以销毁控件；WinTk 会延迟回收。
- 不要从非 UI 线程调用 WinTk。

---

## 9. Label

```c
WtkWidget *wtk_label(WtkWidget *parent, const char *text);
void wtk_label_set_text(WtkWidget *label, const char *text);
const char *wtk_label_get_text(WtkWidget *label);
void wtk_label_set_align(WtkWidget *label, WtkAlign align);
void wtk_label_set_wrap_width(WtkWidget *label, int logical_width);
```

- 支持 `\n` 换行，也支持中文自动换行。
- `wtk_label_set_wrap_width` 设置自动换行宽度；0 表示关闭。
- 返回的文本指针在下次修改或销毁前有效。
- 默认字体为 10pt 的本地化 UI 字体（中文 Windows 下为微软雅黑 UI）；可用 `wtk_widget_set_font` 覆盖。

---

## 10. Button

```c
WtkWidget *wtk_button(WtkWidget *parent, const char *text);
void wtk_button_set_text(WtkWidget *button, const char *text);
const char *wtk_button_get_text(WtkWidget *button);
void wtk_button_set_style(WtkWidget *button, WtkButtonStyle style);
void wtk_button_set_align(WtkWidget *button, WtkAlign align);
```

- 鼠标点击、Space、Enter 会触发 `WTK_EVENT_CLICK`。
- 默认样式 `WTK_BUTTON_SECONDARY`，默认居中。
- Primary 使用主题 accent；Ghost 适合侧边导航；Danger 使用 danger 色。

---

## 11. Entry

```c
WtkWidget *wtk_entry(WtkWidget *parent);
void wtk_entry_set_text(WtkWidget *entry, const char *text);
const char *wtk_entry_get_text(WtkWidget *entry);
void wtk_entry_set_placeholder(WtkWidget *entry, const char *text);
void wtk_entry_set_password(WtkWidget *entry, int password);
void wtk_entry_set_readonly(WtkWidget *entry, int readonly);
void wtk_entry_select_all(WtkWidget *entry);
```

- 基于原生单行 `EDIT`，带自绘圆角宿主边框。
- Enter 触发 `WTK_EVENT_SUBMIT`。
- 用户输入触发 `WTK_EVENT_CHANGE`。
- 占位符由 WinTk 在原生 EDIT 上绘制；获得焦点时隐藏。
- `wtk_entry_set_text` 不会触发用户回调（内部抑制 EN_CHANGE）。
- 返回值指针在文本变化或控件销毁前有效。

---

## 12. Text

```c
WtkWidget *wtk_text(WtkWidget *parent);
void wtk_text_set_text(WtkWidget *text, const char *value);
const char *wtk_text_get_text(WtkWidget *text);
void wtk_text_set_placeholder(WtkWidget *text, const char *placeholder);
void wtk_text_set_readonly(WtkWidget *text, int readonly);
```

- 基于原生多行 `EDIT`，带垂直滚动条。
- 输入触发 `WTK_EVENT_CHANGE`；Ctrl+Enter 触发 `WTK_EVENT_SUBMIT`。
- 默认请求尺寸约 360×180 逻辑像素；用 `set_size` 调整高宽。
- 公开文本统一使用 LF (`\n`) 分隔行；WinTk 内部转换为原生 EDIT 所需的 CRLF，并在读取时转换回 LF。设置和读取都支持多行文本。

---

## 13. Listbox

```c
WtkWidget *wtk_listbox(WtkWidget *parent);
int wtk_listbox_add(WtkWidget *listbox, const char *text);
void wtk_listbox_clear(WtkWidget *listbox);
int wtk_listbox_count(WtkWidget *listbox);
int wtk_listbox_get_selection(WtkWidget *listbox);
void wtk_listbox_select(WtkWidget *listbox, int index);
void wtk_listbox_remove(WtkWidget *listbox, int index);
const char *wtk_listbox_get_item(WtkWidget *listbox, int index);
```

- 基于原生 `LISTBOX`，自带滚动条。
- 选择变化触发 `WTK_EVENT_CHANGE`，`event->value` 是索引，-1 表示无选中。
- 双击触发 `WTK_EVENT_DOUBLE_CLICK`。
- `wtk_listbox_select(-1)` 清除选择；非负索引会把对应项滚动到可见位置。
- `get_item` 返回的指针在下一次调用 `get_item` 或控件销毁前有效。

---

## 14. Checkbox / Radio

```c
WtkWidget *wtk_checkbox(WtkWidget *parent, const char *text, int checked);
int wtk_checkbox_get(WtkWidget *checkbox);
void wtk_checkbox_set(WtkWidget *checkbox, int checked);

WtkWidget *wtk_radiogroup(WtkWidget *parent);
WtkWidget *wtk_radio(WtkWidget *group, const char *text);
int wtk_radiogroup_get(WtkWidget *group);
void wtk_radiogroup_set(WtkWidget *group, int index);
```

- Checkbox 在用户切换时触发自身 `WTK_EVENT_CHANGE`，`event->value` 为 0/1。
- Radio 必须创建在 `wtk_radiogroup` 下；第一个 radio 默认被选中。
- 单选变化触发 **group** 的 `WTK_EVENT_CHANGE`，`event->value` 是索引。
- 方向键在组内切换并移动焦点；Tab 只进入当前选中的 radio。
- 程序化 `set` 不会触发回调。

---

## 15. Slider / Progress

```c
WtkWidget *wtk_slider(WtkWidget *parent, int minimum, int maximum, int value);
int wtk_slider_get(WtkWidget *slider);
void wtk_slider_set(WtkWidget *slider, int value);
void wtk_slider_set_step(WtkWidget *slider, int step);

WtkWidget *wtk_progress(WtkWidget *parent);
void wtk_progress_set(WtkWidget *progress, double fraction); /* 0..1 */
double wtk_progress_get(WtkWidget *progress);
void wtk_progress_set_show_percent(WtkWidget *progress, int show);
```

- Slider 鼠标拖动、点击、方向键、Home/End、PageUp/PageDown 都会改变值。
- 值变化触发 `WTK_EVENT_CHANGE`，`event->value` 是新整数。
- 程序化 `wtk_slider_set` 不触发回调。
- Progress 是确定进度条；`show_percent` 会在条下方绘制百分比。

---

## 16. Separator / Canvas

```c
WtkWidget *wtk_separator(WtkWidget *parent);

WtkWidget *wtk_canvas(WtkWidget *parent, int width, int height);
void wtk_canvas_set_draw(WtkWidget *canvas, WtkDrawFn callback, void *userdata);
void wtk_canvas_redraw(WtkWidget *canvas);
```

Canvas 说明：

- 回调收到双缓冲 HDC、客户区宽高（设备像素）和 userdata。
- WinTk 会先填充画布背景，再调用绘制回调。
- 支持鼠标、滚轮、键盘、焦点和双击事件。
- 需要键盘事件时先点击画布使其获得焦点。
- 绘制中创建 GDI 对象后请自行 `DeleteObject`；保存/恢复 DC 状态也由用户负责。

---

## 17. 定时器 / 对话框

```c
unsigned wtk_after(WtkApp *app, unsigned delay_ms,
                   WtkTimerFn callback, void *userdata);
void wtk_cancel_timer(WtkApp *app, unsigned timer_id);

void wtk_message_box(WtkWidget *owner, const char *title,
                     const char *message, WtkIcon icon);
int wtk_ask_yes_no(WtkWidget *owner, const char *title,
                   const char *message);
```

- `wtk_after` 是一次性定时器，返回 id；0 表示失败。
- 定时器回调在 UI 线程执行。
- 定时器触发前调用 `wtk_cancel_timer` 可取消。
- 消息框 owner 可以是任意控件，WinTk 会自动找顶层窗口；也可以传 `NULL`。
- `wtk_ask_yes_no` 返回 1 表示 Yes，0 表示 No。

---

## 18. 字体与颜色细节

### 字体

```c
/* 使用本地化默认字体（中文 Windows 下为微软雅黑 UI） */
wtk_widget_set_font(widget, NULL, 12, WTK_FONT_BOLD);

/* 或显式指定字体族 */
wtk_widget_set_font(widget, "Microsoft YaHei UI", 12, WTK_FONT_BOLD);
```

- family 为 UTF-8 字体名；`NULL` / 空串表示使用 `wtk_app_ui_font_family()`。
- point size 为磅值，WinTk 会用 DPI 转换成 GDI 像素高度。
- style 可按位组合：`WTK_FONT_BOLD | WTK_FONT_ITALIC`。
- 每个控件独立持有字体对象；改变字体会重新测量并触发重排。
- 所有 WinTk 自绘文本和原生 EDIT/LISTBOX 使用同一字体。Canvas 是由用户直接绘制，绘制中文时需要自行选入一个支持中文的 `HFONT`。

### 主题色和显式颜色

- `wtk_widget_set_background` / `set_foreground` 会写入控件级显式覆盖。
- 主题变化不会修改显式覆盖的颜色。
- 卡片实现：

```c
wtk_widget_set_background(card, theme->surface);
wtk_widget_set_border(card, theme->border, 1, theme->corner_radius + 1);
wtk_widget_set_padding(card, 20, 20, 20, 20);
```

---

## 19. 内建事件支持表

| 控件 | 支持事件 |
|---|---|
| Window | CLICK 之外的鼠标/键盘事件无默认处理；CLOSE、RESIZE 可用 |
| Frame / 容器 | 可用 `wtk_bind` 绑定鼠标事件 |
| Label | 鼠标事件（不接收焦点） |
| Button | CLICK、鼠标、键盘、焦点 |
| Entry/Text | CHANGE、SUBMIT、FOCUS_IN/OUT、键盘、鼠标宿主边框事件 |
| Listbox | CHANGE、DOUBLE_CLICK、FOCUS_IN/OUT、原生滚动 |
| Checkbox | CHANGE、鼠标、键盘、焦点 |
| Radio | 通过 group 的 CHANGE；鼠标、键盘、焦点 |
| Slider | CHANGE、鼠标、键盘、焦点 |
| Progress | 鼠标事件（无焦点） |
| Canvas | 鼠标、滚轮、键盘、FOCUS、RESIZE |
| Menu | 命令回调 |

不是每个事件都对所有控件有意义；例如输入控件的 `CHANGE` 由原生控件通知驱动。

---

## 20. 集成与构建

WinTk 需要：

- 链接库：`user32`, `gdi32`, `comctl32`；使用 Designer 的 Native/Fusion 风格时还需 `uxtheme`
- 编译定义：`UNICODE`, `_UNICODE`
- Windows GUI 子系统：MinGW 的 `-mwindows`
- 公共控件 v6 manifest（`resources/wintk.rc` 已包含）
- DPI 由 WinTk 在 `wtk_app_new()` 中设置；不需要自己的 manifest DPI 设置
- 源码中的中文字符串：GCC/Clang 使用 `-finput-charset=UTF-8 -fexec-charset=UTF-8`，MSVC 使用 `/utf-8`。WinTk 的 Makefile 和 CMake 工程已经设置好；自行编译时请不要遗漏。

可作为静态库：

```text
build/libwintk.a
```

或直接把 `src/*.c` 加入你的工程，并添加 `include` 和 `src` 到头文件搜索路径。

使用 Designer 导出的 Native/Fusion 页面时，需要额外编译伴生风格模块：

```text
designer/src/designer_style.c
```

并添加 `-Idesigner/src -Isrc`。生成的 `page.c` 会调用 `designer_style_apply()`；纯 WinTk GDI 页面不需要这个模块。

---

## 21. 线程和内存约定

- **单 UI 线程**：所有 WinTk 调用都应在同一个线程，且该线程负责 `wtk_run` 的消息循环。
- **对象所有权**：应用拥有所有控件；不要手动 `free`。
- **字符串所有权**：输入字符串会被复制；输出字符串指针的内部生命周期见各控件说明。
- **销毁**：优先销毁子控件而不是在回调中直接释放父容器。WinTk 支持延迟回收销毁的子树，但销毁后句柄不应再使用。
- **自定义 Win32 操作**：通过 `wtk_widget_hwnd()` 可以取得宿主 HWND；对 Entry/Text/Listbox 的原生子窗口请用 `GetWindow(hwnd, GW_CHILD)`，例如设置 `EM_*` / `LB_*` 消息。WinTk 不负责恢复你改动的内部状态。

---

## 22. 中文、UTF-8 与输入法

### 文本与字体

- 所有公开 API 接收 UTF-8；WinTk 内部转换到 UTF-16 后再调用 Win32。
- 窗口标题、菜单、消息框使用 Unicode Win32 API，因此可以直接使用中文。
- 默认字体由 Windows 界面语言决定，并通过 `wtk_app_ui_font_family()` 暴露：
  - 简体中文优先 `Microsoft YaHei UI`；
  - 繁体中文优先 `Microsoft JhengHei UI`；
  - 日文、韩文系统选择对应的 CJK UI 字体；
  - 其他系统使用 `Segoe UI`。
- 若要覆盖字体，调用 `wtk_widget_set_font(widget, "Microsoft YaHei UI", 12, 0)`；传 `NULL` 则使用应用默认字体。
- Label 的中文测量、换行和省略由 GDI 完成；中英文混排使用同一字体。Canvas 绘制中文时需要自行创建并选入 CJK 字体。

### 输入法

- Entry 和 Text 内部是原生 Windows `EDIT`，完整使用系统 IME（微软拼音、搜狗等）。
- 输入法组合、候选窗、取消组合、复制粘贴、撤销、选择等由 EDIT 控件处理。
- WinTk 的输入子类只拦截 Enter（提交）和 Tab（由框架做焦点切换），其余消息（包括 `WM_IME_*`）转发给原生控件。
- 不要用自绘 Label/Canvas 接收文本输入；需要中文输入时使用 Entry 或 Text。

### 编码与字符计数

- C 源码中的中文字符串依赖编译器的源码/执行字符集。官方构建已设置：
  - GCC/Clang：`-finput-charset=UTF-8 -fexec-charset=UTF-8`
  - MSVC：`/utf-8`
- `strlen()` 返回 UTF-8 字节数，不是汉字数量。需要按字符统计时请自行解码 UTF-8（Gallery 的草稿统计是一个例子）。

---

## 23. 已知取舍

- 默认 UI 字体随 Windows 界面语言变化；如果需要所有语言下完全一致的字形，必须显式指定字体。
- 中文输入依赖原生 EDIT；自绘控件不是文本输入控件。
- 菜单加速键只显示提示，不注册全局加速键。
- 不支持滚动容器和内容虚拟化。
- `pack` 的混合方向布局与 Tk 的完整算法不完全相同。
- `place` 只做固定坐标放置，不支持 anchor、比例尺寸或随父容器重排。
- 核心只包含原始 WinTk GDI 绘制；Native/Fusion 画笔由 Designer 的伴生模块提供，不在 `libwintk.a` 内。
- 不注册 `WtkStylePainter` 时，`WTK_STYLE_*` 只是兼容性提示，不会改变外观。
- 菜单、系统消息框和文件对话框始终使用 Windows 原生实现，不随 painter 重绘。
- painter 模块可访问 `WtkPaintState`；如果直接使用 Designer 的 `designer_style.c`，需要同时提供 `include` 与 `src` 头文件路径。
- 没有 per-monitor DPI 的实时重建；系统 DPI 在不同显示上可能看起来略有缩放差异。
- 自绘控件使用 GDI，不支持 alpha 或硬件加速混合。
- 这是一套小框架，目标是把 tkinter 式的编程模型带到纯 C/Win32 小工具中，而不是替代 Qt/GTK/WinUI。
