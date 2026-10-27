# WinTk 风格插件

Designer 从 exe 同目录的 `styles/` 加载 `*.dll`。内置插件源码是 `designer_style.c`，构建产物是：

```text
designer/build/styles/wintk_builtin_styles.dll
```

`Designer.exe` 不包含 Native/Fusion 的绘制代码，只保留插件加载器和 WinTk 核心 GDI。

## ABI

插件必须导出：

```c
const WtkStylePlugin *wintk_style_plugin_get(void);
```

结构定义在 `style_plugin.h`：

```c
typedef struct WtkStylePluginStyle {
    const char *id;
    const char *name;
    WtkStyle hint;
    const WtkTheme *theme;     /* 可为 NULL，保留当前主题 */
    const WtkStylePainter *painter;
} WtkStylePluginStyle;

typedef struct WtkStylePlugin {
    unsigned abi_version;      /* 当前为 WINTK_STYLE_PLUGIN_ABI */
    const char *name;
    int style_count;
    const WtkStylePluginStyle *styles;
} WtkStylePlugin;
```

- 一个 DLL 可以注册多个风格。
- `theme`、`painter` 和字符串必须在 DLL 加载期间保持有效；Designer 不会卸载正在使用的插件。
- `painter` 使用公开的 `WtkPaintState`、`WtkWidgetKind` 和 `WtkStylePainter`，不需要 WinTk 内部头文件。
- 要使用自定义主题，插件可以链接 WinTk 静态库并调用 `wtk_theme_default()` 等函数，或直接填 `WtkTheme`。

## 最小示例

```c
#include "style_plugin.h"
#include <windows.h>

static void my_paint(WtkWidget *widget, HDC dc, const RECT *bounds,
                     const WtkPaintState *state, void *userdata)
{
    HBRUSH brush;
    (void)widget;
    (void)userdata;
    if (!bounds || !state) return;
    brush = CreateSolidBrush(RGB(32, 40, 54));
    FillRect(dc, bounds, brush);
    DeleteObject(brush);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(235, 241, 250));
    DrawTextW(dc, state->text ? state->text : L"", -1, (RECT *)bounds,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}

static const WtkStylePainter my_painter = {
    "My Style", my_paint, NULL
};

static const WtkStylePluginStyle my_styles[] = {
    { "my-style", "My Style", WTK_STYLE_WINTK, NULL, &my_painter }
};

static const WtkStylePlugin my_plugin = {
    WINTK_STYLE_PLUGIN_ABI, "My Style Plugin", 1, my_styles
};

__declspec(dllexport) const WtkStylePlugin *wintk_style_plugin_get(void)
{
    return &my_plugin;
}
```

编译：

```bat
gcc -shared -std=c11 -DUNICODE -D_UNICODE ^
    -I..\..\include -I. my_style.c ^
    -o my_style.dll -luser32 -lgdi32
```

把 `my_style.dll` 放进 `styles/`，重启 Designer，或使用风格菜单的“导入风格 DLL...”。插件会立即出现在风格菜单中。

## 注意

- 插件是原生 Windows DLL，只应加载可信文件。
- 当前 Designer 运行时和预览会使用导入的风格；生成代码的伴生模块仍是 `designer_style.c` 中的内置 Native/Fusion 实现。
- 删除 `styles/` 后 Designer 仍可运行，但只保留 WinTk 核心 GDI 风格。
